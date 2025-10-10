#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "EEEProm.h"
#include "WiFiHandler.h"
#include "WebServer.h"
#include "FSHandler.h"
#include "MQTTHandler.h"


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
    
    Serial.println("🎉 System fully initialized and ready!");
    Serial.println("======================================");
}

void loop() {
    server.handleClient();    // Handle web requests
    checkWiFi();              // Check WiFi connection

    // ----------------- Keep MQTT Alive -----------------
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop();

    ArduinoOTA.handle();      // Handle OTA updates
}

// struct BasicParams{
//     unsigned Current1
//     unsigned Current2
//     unsigned Current3
//     unsigned ZeroPhaseCurrent
//     signed ActivePower1
//     signed ActivePower2
//     signed ActivePower3
//     signed TTPActivePower
//     signed ReactivePower1
//     signed ReactivePower2
//     signed ReactivePower3
//     signed TTPReactivePower
//     signed ApparentPower1
//     signed ApparentPower2
//     signed ApparentPower3
//     signed TTPApparentPower
//     signed PowerFactor1
//     signed PowerFactor2
//     signed PowerFactor3
//     signed TTPPowerPhase
// }

// Struct BasicVoltage{
//     unsigned AVoltage
//     unsigned BVoltage
//     unsigned CVoltage
//     unsigned VoltageMeanValue
//     unsigned ZeroSeqVoltage
// }