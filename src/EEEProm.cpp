#include "EEEProm.h"

WifiParams currentParams;

void initEEEPROM(){
    Serial.println("🔧 EEPROM.begin() called");
    EEPROM.begin(EEPROM_SIZE);
}

void saveWifi(WifiParams &newParams) {
    Serial.println("💾 saveWifi() called");
    
    // Load current config first
    EEPROM.get(WIFI_PARAMS_ADDR, currentParams);
    Serial.println("📥 Loaded current config from EEPROM");
    
    // Debug current state
    Serial.printf("🔍 Current STA: %s, AP: %s, MQTT: %s\n", 
                 currentParams.STAWifiID, currentParams.APWifiID, currentParams.mqttServer, currentParams.mqttPort);
    Serial.printf("🔍 New STA: %s, AP: %s, MQTT: %s\n", 
                 newParams.STAWifiID, newParams.APWifiID, newParams.mqttServer, currentParams.mqttPort);
    
    // Only update STA if new values are not empty
    if (strlen(newParams.STAWifiID) > 0) {
        strncpy(currentParams.STAWifiID, newParams.STAWifiID, sizeof(currentParams.STAWifiID) - 1);
        currentParams.STAWifiID[sizeof(currentParams.STAWifiID) - 1] = '\0';
        Serial.println("✅ Updated STA SSID");
    } else {
        Serial.println("⏭️  STA SSID empty, keeping current");
    }
    
    if (strlen(newParams.STApassword) > 0) {
        strncpy(currentParams.STApassword, newParams.STApassword, sizeof(currentParams.STApassword) - 1);
        currentParams.STApassword[sizeof(currentParams.STApassword) - 1] = '\0';
        Serial.println("✅ Updated STA Password");
    } else {
        Serial.println("⏭️  STA Password empty, keeping current");
    }
    
    // Only update AP if new values are not empty  
    if (strlen(newParams.APWifiID) > 0) {
        strncpy(currentParams.APWifiID, newParams.APWifiID, sizeof(currentParams.APWifiID) - 1);
        currentParams.APWifiID[sizeof(currentParams.APWifiID) - 1] = '\0';
        Serial.println("✅ Updated AP SSID");
    } else {
        Serial.println("⏭️  AP SSID empty, keeping current");
    }
    
    if (strlen(newParams.APpassword) > 0) {
        strncpy(currentParams.APpassword, newParams.APpassword, sizeof(currentParams.APpassword) - 1);
        currentParams.APpassword[sizeof(currentParams.APpassword) - 1] = '\0';
        Serial.println("✅ Updated AP Password");
    } else {
        Serial.println("⏭️  AP Password empty, keeping current");
    }
    
    // ✅ NEW: Update MQTT Server if not empty
    if (strlen(newParams.mqttServer) > 0) {
        strncpy(currentParams.mqttServer, newParams.mqttServer, sizeof(currentParams.mqttServer) - 1);
        currentParams.mqttServer[sizeof(currentParams.mqttServer) - 1] = '\0';
        Serial.println("✅ Updated MQTT Server");
    } else {
        Serial.println("⏭️  MQTT Server empty, keeping current");
    }

    if (strlen(newParams.mqttPort) > 0) {
        strncpy(currentParams.mqttPort, newParams.mqttPort, sizeof(currentParams.mqttPort) - 1);
        currentParams.mqttPort[sizeof(currentParams.mqttPort) - 1] = '\0';
        Serial.println("✅ Updated MQTT Port");
    } else {
        Serial.println("⏭️  MQTT Port empty, keeping current");
    }
    
    // Ensure magic number is preserved
    currentParams.magic = 0xDEADBEEF;
    
    EEPROM.put(WIFI_PARAMS_ADDR, currentParams);
    Serial.println("📤 Writing to EEPROM...");
    
    if (!EEPROM.commit()) {
        Serial.println("❌ ERROR: EEPROM commit failed!");
    } else {
        Serial.println("✅ WiFi & MQTT settings saved successfully to EEPROM");
        Serial.printf("📊 Final settings - STA: %s, AP: %s, MQTT: %s, MQTT_Port: %s\n", 
                     currentParams.STAWifiID, currentParams.APWifiID, currentParams.mqttServer, currentParams.mqttPort);
    }
}

void loadWifi() {
    Serial.println("📖 loadWifi() called");
    
    EEPROM.get(WIFI_PARAMS_ADDR, currentParams);
    Serial.printf("🔍 Read from EEPROM - Magic: 0x%08X\n", currentParams.magic);
    
    // If EEPROM data corrupt, reset to default
    if (currentParams.magic != 0xDEADBEEF) {
        Serial.println("⚠️  EEPROM corrupted or first boot, loading defaults");
        currentParams.magic = 0xDEADBEEF;
        strcpy(currentParams.STAWifiID, "Tanand_Hardware");
        strcpy(currentParams.STApassword, "202040406060808010102020");
        strcpy(currentParams.APWifiID, "ESP8266_AP");
        strcpy(currentParams.APpassword, "12345678");
        strcpy(currentParams.mqttServer, "192.168.31.66");
        strcpy(currentParams.mqttPort, "1883");
        
        Serial.println("📝 Default settings loaded:");
        Serial.printf("   STA: %s\n", currentParams.STAWifiID);
        Serial.printf("   AP: %s\n", currentParams.APWifiID);
        Serial.printf("   MQTT: %s\n", currentParams.mqttServer);
        Serial.printf("   MQTTPort: %s\n", currentParams.mqttPort);
        
        saveWifi(currentParams);
    } else {
        Serial.println("✅ EEPROM data valid");
        Serial.printf("📋 Loaded settings - STA: %s, AP: %s, MQTT: %s\n", 
                     currentParams.STAWifiID, currentParams.APWifiID, currentParams.mqttServer, currentParams.mqttPort);
    }
}