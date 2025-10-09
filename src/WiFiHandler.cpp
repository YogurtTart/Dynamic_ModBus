#include "WiFiHandler.h"
#include "EEEProm.h"
#include <ArduinoOTA.h>

bool otaInitialized = false;
unsigned long previousWiFiCheck = 0;
const unsigned long wifiCheckInterval = 10000;

void setupWiFi() {
    Serial.println("📡 Setting up WiFi in AP+STA mode...");
    WiFi.mode(WIFI_AP_STA);
    
    Serial.printf("🔌 Connecting to STA: %s\n", currentParams.STAWifiID);
    WiFi.begin(currentParams.STAWifiID, currentParams.STApassword);
    
    Serial.printf("📶 Starting AP: %s\n", currentParams.APWifiID);
    WiFi.softAP(currentParams.APWifiID, currentParams.APpassword);
    
    Serial.print("📍 AP IP: "); Serial.println(WiFi.softAPIP());
    
    // Wait a bit for STA connection
    Serial.println("⏳ Waiting for STA connection...");
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("\n✅ STA connected, IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n❌ STA connection failed, will retry...");
    }
}

void checkWiFi() {
    unsigned long now = millis();
    if (now - previousWiFiCheck >= wifiCheckInterval) {
        previousWiFiCheck = now;
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("🔌 STA disconnected, reconnecting...");
            WiFi.begin(currentParams.STAWifiID, currentParams.STApassword);
            otaInitialized = false;
        } else if (!otaInitialized) {
            Serial.println("🚀 Initializing OTA updates...");
            ArduinoOTA.begin();
            otaInitialized = true;
            Serial.println("✅ OTA ready - STA connected and stable");
        }
    }
}