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
    Serial.println("📡 Phase 3: Setting up WiFi...");
    setupWiFi();
    
    Serial.println("🌐 Phase 4: Starting Web Server...");
    setupWebServer();
    
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
    
    Serial.println("✅ System fully initialized and ready!");
}

void handleSystemOperations() {
    server.handleClient();    // Handle web requests
    checkWiFi();              // Maintain WiFi connection
    
    // ✅ USE HELPER: Only check MQTT if WiFi is up
    if (isWiFiConnected()) {  
        checkMQTT();          // Maintain MQTT connection
    }
    
    // ✅ EFFICIENT: Only process ModBus if slaves are configured
    if (slaveCount > 0) {
        updateNonBlockingQuery(); // Process ModBus queries
    }
    
    // OTA handled inside checkWiFi() when appropriate
}


void factoryReset() {
    Serial.println("🔄 FACTORY RESET: Restoring all defaults...");
    
    // Reset EEPROM to defaults
    resetToDefaults();
    
    // Reinitialize everything
    initializeSystem();
    
    Serial.println("✅ Factory reset complete!");
}

// 🆕 ADDED: System status function
void printSystemStatus() {
    Serial.println("\n📊 SYSTEM STATUS:");
    Serial.printf("   WiFi: %s", isWiFiConnected() ? "Connected" : "Disconnected");
    if (isWiFiConnected()) {
        Serial.printf(" (%s)", getSTAIP().c_str());
    }
    Serial.println();
    
    Serial.printf("   MQTT: %s", isMQTTConnected() ? "Connected" : "Disconnected");
    if (isMQTTConnected()) {
        Serial.printf(" (%s)", getMQTTServer().c_str());
    }
    Serial.println();
    
    Serial.printf("   Slaves: %d configured\n", slaveCount);
    Serial.printf("   Templates: %d available\n", getTemplateCount());
    Serial.printf("   Free Heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("   AP Clients: %d connected\n", getAPClientCount());
}

// ==================== ARDUINO MAIN FUNCTIONS ====================

void setup() {
    Serial.begin(9600);
    Serial.println("\n🔌 ESP8266 ModBus Gateway Starting...");
    Serial.printf("📊 Free Heap: %d bytes\n", ESP.getFreeHeap());

    //factoryReset();
    
    initializeSystem();
    
    Serial.printf("📊 Free Heap after init: %d bytes\n", ESP.getFreeHeap());
    printSystemStatus();
    Serial.println("🎉 System fully initialized and ready!");
}

void loop() {
    handleSystemOperations();
    delay(10); // Small delay for stability
    
    // 🆕 ADDED: Periodic status reporting (every 5 minutes)
    static unsigned long lastStatusReport = 0;
    if (millis() - lastStatusReport > 300000) { // 5 minutes
        lastStatusReport = millis();
        printSystemStatus();
    }
}