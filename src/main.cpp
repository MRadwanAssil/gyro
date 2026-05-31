#include <WiFi.h>
#include <Wire.h>
#include <math.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "MPUData.h"
#include "WiFiManager.h"
#include "MPUManager.h"
#include "MQTTManager.h"

// ================= DONANIM PİNLERİ =================
const int INTERNAL_LED = 2;    // ESP32 Dahili Mavi LED (Çoğu kartta GPIO 2)
const int RESET_BUTTON = 0;    // ESP32 Dahili BOOT Butonu (GPIO 0)

// ================= SABİT PARAMETRELER VE AYARLAR =================
const char* ssid = "Tulipsoft";
const char* password = "Ra21436587";
const char* mqtt_server = "f1816010c8504a99a9798e2d37f7cc41.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "esp32";
const char* mqtt_pass = "Ra123@Ra123";

const int MPU = 0x68;
const float SMOOTH = 0.08;
const float GYRO_SENS = 131.0;

const long WIRE_CLOCK = 400000;
const int WIFI_CONNECT_TIMEOUT = 10000;
const int WIFI_RECONNECT_DELAY = 300;
const int MQTT_RECONNECT_DELAY = 1000;

const uint32_t IMU_SAMPLE_PERIOD_US = 20000; // 50Hz için 20000 mikrosaniye
const TickType_t MQTT_TASK_PERIOD_MS = pdMS_TO_TICKS(100); // 100ms MQTT periyodu
const int MQTT_STRING_PRECISION = 2; // Ondalık hassasiyet derecesi

const uint8_t MPU_REG_ACCEL_XOUT_H = 0x3B;
const uint8_t MPU_READ_BYTE_COUNT = 14;
const float RAD_TO_DEG_CONSTANT = 57.2958;

// ================= NESNELER VE DEĞİŞKENLER =================
MPUManager* mpuManager;
MQTTManager* mqttManager;
WiFiManager* wifiManager;

WiFiClientSecure espClient;
PubSubClient client(espClient);

int16_t AcX, AcY, AcZ, GyX, GyY, GyZ;
MPUData mpuData;

// Paylaşılan verileri korumak için Mutex
SemaphoreHandle_t mpuMutex;

// FreeRTOS Task Tanımlaması (Core 0 için)
TaskHandle_t MqttTaskHandle;

// Global Durum Bayrakları
bool isSystemReady = false;
bool hasSystemError = false;
uint32_t ledTurnOffTime = 0;

// Fonksiyon Protokolleri
void readMPU();
void ledIndicatorTask(void* parameter);

void setup() {
    Serial.begin(115200);

    // Pin Modları Tanımlanıyor
    pinMode(INTERNAL_LED, OUTPUT);
    pinMode(RESET_BUTTON, INPUT_PULLUP); // Boot butonu dahili pull-up dirençlidir

    // LED gösterge görevini (Blink) arka planda Core 0 üzerinde başlatıyoruz
    xTaskCreatePinnedToCore(
        ledIndicatorTask,    // Görev fonksiyonu
        "LED_Task",         // Görev adı
        2048,               // Stack boyutu
        NULL,               // Parametre
        2,                  // Öncelik (Yüksek)
        NULL,               // Task handle
        0                   // Core 0
    );

    mpuManager = new MPUManager();
    wifiManager = new WiFiManager();
    mqttManager = new MQTTManager();

    Wire.begin();
    Wire.setClock(WIRE_CLOCK);

    // MPU WAKEUP
    mpuManager->begin();

    // Wi-Fi Bağlantısı Başlatılıyor
    bool wifiSuccess = wifiManager->connect(ssid, password, WIFI_CONNECT_TIMEOUT,
        []() { Serial.println("wifi connected."); },
        []() {
            Serial.println("error while connecting to the wifi.");
            hasSystemError = true; // Hata bayrağını kaldır (LED sürekli yanacak)
        }
    );

    // Eğer Wi-Fi bağlantısı başarısız olduysa setup'ı burada kilitle
    if (!wifiSuccess) {
        hasSystemError = true;
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    randomSeed(micros());

    // Mutex oluşturuluyor
    mpuMutex = xSemaphoreCreateMutex();

    // MQTT görevini Core 0 üzerinde başlatıyoruz
    mqttManager->startTask(
        mpuMutex, mpuData.roll, mpuData.pitch, mpuData.yaw, mqtt_server, mqtt_port
    );

    // Sistem hatasız bir şekilde başarıyla açıldı!
    isSystemReady = true;
}

// LOOP fonksiyonu otomatik olarak CORE 1 üzerinde çalışır.
void loop() {
    static uint32_t lastMicros = micros();
    static bool buttonPressed = false;

    // ----- FİZİKSEL RESET BUTONU KONTROLÜ -----
    if (digitalRead(RESET_BUTTON) == LOW) {
        if (!buttonPressed) { // Debounce koruması
            Serial.println("reseting mpu...");

            // LED'i 500ms sonra sönecek şekilde zamanla ve manuel yak
            ledTurnOffTime = millis() + 500;
            digitalWrite(INTERNAL_LED, HIGH);

            // Diğer çekirdek veriyi yazarken çakışmasın diye kilit altına alıyoruz
            if (xSemaphoreTake(mpuMutex, portMAX_DELAY)) {
                mpuManager->reset();
                xSemaphoreGive(mpuMutex);
            }
            buttonPressed = true;
        }
    }
    else {
        buttonPressed = false;
    }

    // ----- IMU VERİ OKUMA VE SİNYAL SÜRECİ -----
    uint32_t now = micros();
    float dt = (now - lastMicros) * 0.000001;
    lastMicros = now;

    // Güncel açı verisini mpuManager'dan lokal bir değişkene çekiyoruz
    MPUData freshData = mpuManager->readMPU(GYRO_SENS, SMOOTH, dt);

    // Çekirdekler arası veri güvenliği için kilitleme
    if (xSemaphoreTake(mpuMutex, portMAX_DELAY)) {
        mpuData = freshData;
        xSemaphoreGive(mpuMutex);
    }

    // Hassas zamanlama için non-blocking yaklaşım (50Hz)
    while (micros() - now < IMU_SAMPLE_PERIOD_US) {
        portYIELD();
    }
}

// ================= LED DURUM GÖSTERGE GÖREVİ (CORE 0) =================
void ledIndicatorTask(void* parameter) {
    for (;;) {
        if (hasSystemError) {
            digitalWrite(INTERNAL_LED, HIGH);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        else if (!isSystemReady) {
            digitalWrite(INTERNAL_LED, HIGH);
            vTaskDelay(pdMS_TO_TICKS(50));
            digitalWrite(INTERNAL_LED, LOW);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        else {
            // --- SİSTEM NORMAL ÇALIŞIRKEN BUTON VE LED KONTROLÜ ---
            if (millis() < ledTurnOffTime) {
                // Eğer buton tetiklendiyse ve henüz 500ms geçmediyse LED'i açık tut
                digitalWrite(INTERNAL_LED, HIGH);
                vTaskDelay(pdMS_TO_TICKS(20)); // Hızlı kontrol için kısa bekleme
            }
            else {
                // Zaman dolduysa veya butona basılmadıysa LED kapalı kalır
                digitalWrite(INTERNAL_LED, LOW);
                vTaskDelay(pdMS_TO_TICKS(200)); // İşlemciyi yormamak için ideal süre
            }
        }
    }
}