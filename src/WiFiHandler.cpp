#include "WiFiHandler.h"
#include "EEEProm.h"
#include <ArduinoOTA.h>

// ==================== GLOBAL VARIABLES ====================
bool otaInitialized = false;
unsigned long previousWiFiCheck = 0;
const unsigned long wifiCheckInterval = 20000;
const unsigned long wifiConnectionTimeout = 10000;

// ==================== WIFI SETUP ====================

void setupWiFi() {
    Serial.println("📡 Setting up WiFi in AP+STA mode...");
    WiFi.mode(WIFI_AP_STA);
    
    Serial.printf("🔌 Connecting to STA: %s\n", currentParams.STAWifiID);
    WiFi.begin(currentParams.STAWifiID, currentParams.STApassword);
    
    Serial.printf("📶 Starting AP: %s\n", currentParams.APWifiID);
    WiFi.softAP(currentParams.APWifiID, currentParams.APpassword);
    
    Serial.print("📍 AP IP: "); 
    Serial.println(WiFi.softAPIP());
    
    // ✅ NON-BLOCKING: Start connection but don't wait
    Serial.println("⏳ STA connection attempt started (non-blocking)...");
    
    // Set initial OTA state
    otaInitialized = false;
}

// ==================== WIFI MAINTENANCE ====================

void checkWiFi() {
    unsigned long now = millis();
    
    // ✅ EFFICIENT: Only check every 20 seconds
    if (now - previousWiFiCheck >= wifiCheckInterval) {
        previousWiFiCheck = now;
        
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("🔌 STA disconnected, reconnecting...");
            WiFi.disconnect();
            WiFi.begin(currentParams.STAWifiID, currentParams.STApassword);
            otaInitialized = false;  // Reset OTA until stable connection
        } else if (!otaInitialized) {
            // ✅ INITIALIZE OTA ONLY ONCE when connection becomes stable
            Serial.println("🚀 Initializing OTA updates...");
            
            ArduinoOTA.onStart([]() {
                Serial.println("📦 OTA update started");
            });
            
            ArduinoOTA.onEnd([]() {
                Serial.println("✅ OTA update finished");
            });
            
            ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
                Serial.printf("📥 OTA progress: %u%%\r", (progress / (total / 100)));
            });
            
            ArduinoOTA.onError([](ota_error_t error) {
                Serial.printf("❌ OTA error[%u]: ", error);
            });
            
            ArduinoOTA.begin();
            otaInitialized = true;
            Serial.println("✅ OTA ready - STA connected and stable");
        }
    }
    
    // ✅ Handle OTA only when initialized and connected
    if (otaInitialized && WiFi.status() == WL_CONNECTED) {
        ArduinoOTA.handle();
    }
}

// ==================== CONNECTION STATUS ====================

bool isWiFiConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

String getSTAIP() {
    return WiFi.localIP().toString();
}

String getAPIP() {
    return WiFi.softAPIP().toString();
}

int getAPClientCount() {
    return WiFi.softAPgetStationNum();
}