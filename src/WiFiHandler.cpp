#include "WiFiHandler.h"
#include "EEEProm.h"
#include <ArduinoOTA.h>

// ==================== GLOBAL VARIABLES ====================
bool otaInitialized = false;
unsigned long previousWiFiCheck = 0;
unsigned long staConnectionStart = 0;
bool staConnecting = false;
const unsigned long wifiCheckInterval = 20000;
const unsigned long wifiConnectionTimeout = 10000;

// ==================== OTA INITIALIZATION ====================

void initializeOTA() {
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
    Serial.println("✅ OTA ready");
}

// ==================== WIFI SETUP ====================

void setupWiFi() {
    Serial.println("📡 Setting up WiFi in AP+STA mode (STA disconnected)...");
    
    // Start in AP_STA mode but don't auto-connect STA
    WiFi.mode(WIFI_AP_STA);
    
    Serial.printf("📶 Starting AP: %s\n", currentParams.APWifiID);
    WiFi.softAP(currentParams.APWifiID, currentParams.APpassword);

    Serial.print("📍 AP IP: "); 
    Serial.println(WiFi.softAPIP());
    
    // STA interface is available but not connected
    Serial.println("🔌 STA interface ready - use manual connection to connect");
    
    // OTA not initialized until STA is connected
    otaInitialized = false;
    staConnecting = false;
}

// ==================== MANUAL STA CONNECTION ====================

void connectSTA() {
    // Only start connection if not already connected or connecting
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("⚠️ STA already connected");
        return;
    }
    
    if (staConnecting) {
        Serial.println("⚠️ STA connection already in progress");
        return;
    }
    
    Serial.println("🔄 Starting manual STA connection (non-blocking)...");
    Serial.printf("🔌 Connecting to: %s\n", currentParams.STAWifiID);
    
    WiFi.begin(currentParams.STAWifiID, currentParams.STApassword);
    staConnecting = true;
    staConnectionStart = millis();
    
    Serial.println("⏳ Connection attempt started - checking status in background");
}

void disconnectSTA() {
    Serial.println("🔌 Disconnecting STA...");
    WiFi.disconnect();
    staConnecting = false;
    
    // Properly shut down OTA
    if (otaInitialized) {
        // Note: ArduinoOTA doesn't have a formal end() method, but we can mark it as unavailable
        Serial.println("📦 OTA updates disabled (STA disconnected)");
        otaInitialized = false;
    }
    
    Serial.println("✅ STA disconnected");
}

bool isSTAConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

bool isSTAConnecting() {
    return staConnecting;
}

// ==================== WIFI MAINTENANCE ====================

void checkWiFi() {
    unsigned long now = millis();
    
    // Handle STA connection timeout (non-blocking)
    if (staConnecting) {
        if (WiFi.status() == WL_CONNECTED) {
            // Connection successful
            Serial.println("\n✅ STA connected successfully!");
            Serial.printf("📍 STA IP: %s\n", WiFi.localIP().toString().c_str());
            staConnecting = false;
            
            // Initialize OTA only when STA connection is successful
            if (!otaInitialized) {
                initializeOTA();
            }
        } 
        else if (now - staConnectionStart >= wifiConnectionTimeout) {
            // Connection timeout
            Serial.println("\n❌ STA connection timeout - giving up");
            staConnecting = false;
            WiFi.disconnect();
        }
        else {
            // Still connecting - show progress every 2 seconds
            static unsigned long lastProgress = 0;
            if (now - lastProgress >= 2000) {
                Serial.print(".");
                lastProgress = now;
            }
        }
    }
    
    // Handle STA disconnection
    if (otaInitialized && WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️ STA disconnected, OTA unavailable");
        otaInitialized = false;
    }
}

// ==================== OTA HANDLING ====================

void handleOTA() {

    if (WiFi.status() != WL_CONNECTED) return;
        
        // Only handle OTA if it's initialized
        if (otaInitialized) {
            ArduinoOTA.handle();
        }

}

// ==================== CONNECTION STATUS ====================

bool isWiFiConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

String getSTAIP() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "Not connected";
}

String getSTAStatus() {
    if (staConnecting) {
        return "Connecting...";
    } else if (WiFi.status() == WL_CONNECTED) {
        return "Connected";
    } else {
        return "Disconnected";
    }
}

String getAPIP() {
    return WiFi.softAPIP().toString();
}

int getAPClientCount() {
    return WiFi.softAPgetStationNum();
}