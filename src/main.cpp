#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

#include "EEEProm.h"
#include "WiFiHandler.h"
#include "WebServer.h"
#include "FSHandler.h"
#include "MQTTHandler.h"
#include "ModBusHandler.h"

void forceResetEEPROM() {
    Serial.println("🔄 FORCING EEPROM RESET...");
    EEPROM.begin(EEPROM_SIZE);
    
    // Create default WiFi params
    WifiParams defaultParams;
    defaultParams.magic = 0xDEADBEEF;
    strcpy(defaultParams.STAWifiID, "Tanand_Hardware");
    strcpy(defaultParams.STApassword, "202040406060808010102020");
    strcpy(defaultParams.APWifiID, "ESP8266_AP");
    strcpy(defaultParams.APpassword, "12345678");
    strcpy(defaultParams.mqttServer, "192.168.31.66");
    
    // Write to EEPROM
    EEPROM.put(WIFI_PARAMS_ADDR, defaultParams);
    EEPROM.commit();
    EEPROM.end();
    
    Serial.println("✅ EEPROM reset to defaults");
    delay(1000);
}

// System initialization
void initializeSystem();
void handleSystemOperations();

void setup() {
    Serial.begin(9600);
    Serial.println("\n🔌 ESP8266 ModBus Gateway Starting...");
    delay(1000);

    //forceResetEEPROM();  // ⬅️ UNCOMMENT THIS LINE FOR FIRST RUN

    initializeSystem();
    Serial.println("🎉 System fully initialized and ready!");
}

void loop() {
    handleSystemOperations();
    delay(10); // Small delay for stability
}

void initializeSystem() {
    // Initialize components in logical order
    Serial.println("📝 Initializing EEPROM...");
    initEEEPROM();
    loadWifi();
    
    Serial.println("📁 Initializing File System...");
    if (!initFileSystem()) {
        Serial.println("❌ File system initialization failed!");
        return;
    }
    
    Serial.println("📡 Setting up WiFi...");
    setupWiFi();
    
    Serial.println("🌐 Starting Web Server...");
    setupWebServer();
    
    Serial.println("🔌 Initializing ModBus...");
    initModbus();
    modbusReloadSlaves();
}

void handleSystemOperations() {
    server.handleClient();    // Handle web requests
    checkWiFi();              // Maintain WiFi connection
    
    if (WiFi.status() == WL_CONNECTED) {
        checkMQTT();          // Maintain MQTT connection
    }
    
    if (slaveCount > 0) {
        updateNonBlockingQuery(); // Process ModBus queries
    }
    
    ArduinoOTA.handle();      // Handle OTA updates
}