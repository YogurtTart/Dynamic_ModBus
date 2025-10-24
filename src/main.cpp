#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "EEEProm.h"
#include "WiFiHandler.h"
#include "WebServer.h"
#include "FSHandler.h"
#include "MQTTHandler.h"
#include "ModBusHandler.h"

// void forceResetEEPROM() {
//     Serial.println("🔄 FORCING EEPROM RESET...");
//     EEPROM.begin(EEPROM_SIZE);
    
//     // Create default WiFi params
//     WifiParams defaultParams;
//     defaultParams.magic = 0xDEADBEEF;
//     strcpy(defaultParams.STAWifiID, "Tanand_Hardware");
//     strcpy(defaultParams.STApassword, "202040406060808010102020");
//     strcpy(defaultParams.APWifiID, "ESP8266_AP");
//     strcpy(defaultParams.APpassword, "12345678");
    
//     // Write to EEPROM
//     EEPROM.put(WIFI_PARAMS_ADDR, defaultParams);
//     EEPROM.commit();
//     EEPROM.end();
    
//     Serial.println("✅ EEPROM reset to defaults");
//     delay(1000);
// }


void setup() {
    Serial.begin(9600);
    Serial.println("\n🔌 ESP8266 Starting...");
    delay(1000);

    //forceResetEEPROM();  // ⬅️ UNCOMMENT THIS LINE FOR FIRST RUN

    // Initialize EEPROM and load WiFi settings
    Serial.println("📝 Initializing EEPROM...");
    initEEEPROM();
    loadWifi();
    Serial.println("✅ EEPROM initialized");

    // Initialize File System
    Serial.println("📁 Initializing LittleFS...");
    if (!initFileSystem()) {
        Serial.println("❌ LittleFS initialization failed!");
        return;
    }
    Serial.println("✅ LittleFS initialized");
        
    // Setup WiFi and Web Server
    Serial.println("📡 Setting up WiFi...");
    setupWiFi();

    mqttClient.setServer(mqttServer, mqttPort);
    
    Serial.println("🌐 Starting Web Server...");
    setupWebServer();

    initModbus();
    modbus_reloadSlaves();
    
    Serial.println("🎉 System fully initialized and ready!");
    Serial.println("======================================");
}

void loop() {
    server.handleClient();    // Handle web requests
    checkWiFi();              // Check WiFi connection

    // ----------------- Handle MQTT only if STA is connected -----------------
    if (WiFi.status() == WL_CONNECTED) {
        checkMQTT();          // Check and maintain MQTT connection
    } 

    if(slaveCount > 0){
        updateNonBlockingQuery();
    }
    
    // ----------------- Handle Web Server -----------------
    server.handleClient();

    ArduinoOTA.handle();      // Handle OTA updates
}