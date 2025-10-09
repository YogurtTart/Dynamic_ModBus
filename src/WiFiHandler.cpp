#include "WiFiHandler.h"
#include "EEEProm.h"
#include <ArduinoOTA.h>

bool otaInitialized = false;
unsigned long previousWiFiCheck = 0;
const unsigned long wifiCheckInterval = 10000;

//==========================================

void setupWiFi() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(currentParams.STAWifiID, currentParams.STApassword);
    WiFi.softAP(currentParams.APWifiID, currentParams.APpassword);
    Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
    Serial.print("STA IP: "); Serial.println(WiFi.localIP());
}

void checkWiFi() {
    unsigned long now = millis();
    if (now - previousWiFiCheck >= wifiCheckInterval) {
        previousWiFiCheck = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("Reconnecting STA...");
            WiFi.begin(currentParams.STAWifiID, currentParams.STApassword);
            otaInitialized = false;
        } else if (!otaInitialized) {
            ArduinoOTA.begin();
            otaInitialized = true;
            Serial.print("STA connected, IP: ");
            Serial.println(WiFi.localIP());
        }
    }
}
