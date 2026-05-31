#ifndef MQTTMANAGER_H
#define MQTTMANAGER_H

#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

// FreeRTOS'a tek seferde göndereceğimiz paket
struct MqttTaskParams {
    class MQTTManager* instance; // Sınıfın kendisine (this) ulaşmak için
    SemaphoreHandle_t mpuMutex;
    float* roll;                 // Referans yerine pointer kullanıyoruz
    float* pitch;
    float* yaw;
};

class MQTTManager {
private:
    WiFiClientSecure espClient;
    PubSubClient client;
    TaskHandle_t MqttTaskHandle = NULL;

    void mqttReconnect(const char* mqtt_user, const char* mqtt_pass);
public:
    MQTTManager();

    static void mqttTaskWrapper(void* parameter);
    void mqttTask(SemaphoreHandle_t mpuMutex, float* roll, float* pitch, float* yaw);
        void startTask(
            SemaphoreHandle_t mutex, float& r, float& p, float& y, const char* server, int port
        );
};

#endif