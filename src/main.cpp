#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

#include "EEEProm.h"
#include "WiFiHandler.h"
#include "WebServer.h"
#include "FSHandler.h"
#include "MQTTHandler.h"
#include "ModBusHandler.h"
#include "TemplateInitializer.h"

// ==================== SYSTEM INITIALIZATION ====================

void forceResetEEPROM() {
    Serial.println("🔄 FORCING EEPROM RESET...");
    EEPROM.begin(EEPROM_SIZE);
    
    // Create default WiFi parameters
    WifiParams defaultParams;
    defaultParams.magic = 0xDEADBEEF;
    strcpy(defaultParams.STAWifiID, "Tanand_Hardware");
    strcpy(defaultParams.STApassword, "202040406060808010102020");
    strcpy(defaultParams.APWifiID, "ESP8266_AP");
    strcpy(defaultParams.APpassword, "12345678");
    strcpy(defaultParams.mqttServer, "192.168.31.66");
    strcpy(defaultParams.mqttPort, "1883");
    
    // Write to EEPROM
    EEPROM.put(WIFI_PARAMS_ADDR, defaultParams);
    EEPROM.commit();
    EEPROM.end();
    
    Serial.println("✅ EEPROM reset to defaults");
    delay(1000);
}

void initializeSystem() {
    Serial.println("🎯 Starting ESP8266 System Initialization...");
    
    // Phase 1: Core Storage & Filesystem
    Serial.println("📝 Phase 1: Initializing EEPROM...");
    initEEEPROM();
    loadWifi();
    
    Serial.println("📁 Phase 2: Initializing File System...");
    if (!initFileSystem()) {
        Serial.println("❌ CRITICAL: File system initialization failed!");
        return;
    }
    
    // Phase 2: Network Services  
    Serial.println("🌐 Phase 3: Starting Web Server...");
    setupWebServer();
    
    Serial.println("📡 Phase 4: Setting up WiFi (AP+STA mode, STA disconnected)...");
    setupWiFi();  // Now starts in AP_STA mode but STA is disconnected
    
    // Phase 3: Application Logic
    Serial.println("🔧 Phase 5: Initializing ModBus...");
    if (!initModbus()) {
        Serial.println("❌ ModBus initialization failed!");
    }
    
    // ✅ OPTIMIZED: Only create templates if they don't exist
    if (templatesNeedCreation()) {
        Serial.println("📋 Phase 6: Creating default templates...");
        if (!createDefaultTemplates()) {
            Serial.println("❌ Template creation failed!");
        }
    } else {
        Serial.printf("📋 Phase 6: Templates already exist (%d templates)\n", getTemplateCount());
    }
    
    Serial.println("🔄 Phase 7: Loading slave configurations...");
    if (!modbusReloadSlaves()) {
        Serial.println("⚠️  No slave configurations loaded");
    }
    
    Serial.println("✅ System fully initialized!");
    Serial.println("📍 AP Mode: Active - Connect to configure device");
    Serial.println("🔌 STA Mode: Ready - Use web interface to connect manually");
}

// ==================== ARDUINO MAIN FUNCTIONS ====================

void setup() {
    Serial.begin(9600);
    Serial.println("\n🔌 ESP8266 ModBus Gateway Starting...");
    Serial.printf("📊 Free Heap: %d bytes\n", ESP.getFreeHeap());
    
    initializeSystem();

    //forceResetEEPROM();  // ⬅️ UNCOMMENT THIS LINE FOR FIRST RUN
    
    Serial.println("🎉 System fully initialized and ready!");
}

void loop() {
    server.handleClient();    // Handle web requests
    checkWiFi();              // Maintain WiFi connection (non-blocking STA checks)
    handleOTA(); 

    // ✅ USE HELPER: Only check MQTT if WiFi is up
    if (isWiFiConnected()) {  
        checkMQTT();          // Maintain MQTT connection
    }
    
    // ✅ EFFICIENT: Only process ModBus if slaves are configured
    if (slaveCount > 0) {
        updateNonBlockingQuery(); // Process ModBus queries
    }
    
    // OTA handled inside checkWiFi() when STA is connected

    delay(10); // Small delay for stability
}