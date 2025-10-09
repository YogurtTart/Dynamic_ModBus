#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include "EEEProm.h"
#include "WiFiHandler.h"
#include "WebServer.h"
#include "FSHandler.h"

void setup() {
    Serial.begin(9600);

    // Initialize EEPROM and load WiFi settings
    initEEEPROM();
    loadWifi();

    // Initialize File System
    initFileSystem();
        
    // Setup WiFi and Web Server
    setupWiFi();
    setupWebServer();  // ✅ WEB SERVER ACTIVATED!

    Serial.println("System ready!");
}

void loop() {
    server.handleClient();    // Handle web requests

    checkWiFi();              // Check WiFi connection
    
    ArduinoOTA.handle();      // Handle OTA updates
}