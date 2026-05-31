#include <WiFi.h>  // Network kütüphanesi eklendi
#include <PubSubClient.h>
#include "MQTTManager.h"

MQTTManager::MQTTManager() : client(espClient) {
    espClient.setInsecure(); // SSL Handshake hatasını çözen altın dokunuş
}

void MQTTManager::startTask(
    SemaphoreHandle_t mutex, float& r, float& p, float& y, const char* server, int port
) {
    client.setServer(server, port);

    MqttTaskParams* params = new MqttTaskParams();
    params->instance = this;
    params->mpuMutex = mutex;
    params->roll = &r;   // Global değişkenin adresi paketleniyor
    params->pitch = &p;
    params->yaw = &y;

    xTaskCreatePinnedToCore(
        MQTTManager::mqttTaskWrapper, "MQTT_Task", 8192, params, 1, &MqttTaskHandle, 0
    );
}

void MQTTManager::mqttTaskWrapper(void* parameter) {
    MqttTaskParams* params = (MqttTaskParams*)parameter;

    // DÜZELTİLDİ: Baştaki '*' işaretleri kaldırıldı, doğrudan adresler gönderiliyor
    params->instance->mqttTask(
        params->mpuMutex,
        params->roll,
        params->pitch,
        params->yaw
    );

    delete params;
    vTaskDelete(NULL);
}

void MQTTManager::mqttReconnect(const char* mqtt_user, const char* mqtt_pass) {
    while (!client.connected()) {
        Serial.print("MQTT connecting...");
        String clientId = "esp32-imu-" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println(" connected");
        }
        else {
            Serial.print(" failed, state=");
            Serial.println(client.state());
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// DÜZELTİLDİ: Parametreler referans (&) yerine pointer (*) yapıldı
void MQTTManager::mqttTask(SemaphoreHandle_t mpuMutex, float* roll, float* pitch, float* yaw) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100);

    for (;;) {
        if (!client.connected()) {
            mqttReconnect("esp32", "Ra123@Ra123");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        client.loop();

        float r = 0, p = 0, y = 0;

        if (xSemaphoreTake(mpuMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            // DÜZELTİLDİ: roll, pitch ve yaw artık pointer olduğu için 
            // başlarına '*' koyarak adresteki float değerleri çekiyoruz.
            r = *roll;
            p = *pitch;
            y = *yaw;
            xSemaphoreGive(mpuMutex);
        }

        // JSON Mesajı Oluşturma ve Yayınlama
        String msg = "{\"roll\":" + String(r, 2) + ",\"pitch\":" + String(p, 2) + ",\"yaw\":" + String(y, 2) + "}";
        client.publish("/imu/data", msg.c_str());

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}