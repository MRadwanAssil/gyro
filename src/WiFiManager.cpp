#include <WiFi.h>
#include "WiFiManager.h"

bool WiFiManager::connect(
    const char* ssid, const char* password, int timeout, void (*onSuccess)(), void (*onError)()
) {
    WiFi.begin(ssid, password);

    uint32_t startAttemptTime = millis();

    while (WiFi.status() != WL_CONNECTED && (millis() - startAttemptTime < timeout)) {
        vTaskDelay(pdMS_TO_TICKS(300));
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (onSuccess != nullptr) {
            onSuccess();
            return true;
        }
    }
    else {
        if (onError != nullptr) {
            onError();
            return false;
        }
    }
}