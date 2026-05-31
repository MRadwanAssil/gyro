#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

class WiFiManager {
private:
public:
    bool connect(
        const char* ssid,
        const char* password,
        int timeout,
        void (*onSuccess)(),
        void (*onError)()
    );
};

#endif