#include "EEEProm.h"

// ==================== GLOBAL VARIABLES ====================
WifiParams currentParams;

// ==================== CONSTANTS ====================
const uint32_t EEPROM_MAGIC = 0xDEADBEEF;

// ==================== EEPROM INITIALIZATION ====================

void initEEEPROM(){
    Serial.println("🔧 EEPROM.begin() called");
    EEPROM.begin(EEPROM_SIZE);
}

// ==================== STRING HELPER ====================

// 🆕 ADDED: Safe string copy helper
void safeStringCopy(char* dest, const char* src, size_t maxLen) {
    strncpy(dest, src, maxLen - 1);
    dest[maxLen - 1] = '\0';
}

// ==================== WIFI PARAMETER MANAGEMENT ====================

void saveWifi(WifiParams &newParams) {
    Serial.println("💾 saveWifi() called");
    
    // Load current config first
    EEPROM.get(WIFI_PARAMS_ADDR, currentParams);
    Serial.println("📥 Loaded current config from EEPROM");
    
    // Debug current state
    Serial.printf("🔍 Current STA: %s, AP: %s, MQTT: %s:%s\n", 
                 currentParams.STAWifiID, currentParams.APWifiID, 
                 currentParams.mqttServer, currentParams.mqttPort);
    Serial.printf("🔍 New STA: %s, AP: %s, MQTT: %s:%s\n", 
                 newParams.STAWifiID, newParams.APWifiID, 
                 newParams.mqttServer, newParams.mqttPort);
    
    // ✅ OPTIMIZED: Use helper for string copying
    if (strlen(newParams.STAWifiID) > 0) {
        safeStringCopy(currentParams.STAWifiID, newParams.STAWifiID, sizeof(currentParams.STAWifiID));
        Serial.println("✅ Updated STA SSID");
    } else {
        Serial.println("⏭️  STA SSID empty, keeping current");
    }
    
    if (strlen(newParams.STApassword) > 0) {
        safeStringCopy(currentParams.STApassword, newParams.STApassword, sizeof(currentParams.STApassword));
        Serial.println("✅ Updated STA Password");
    } else {
        Serial.println("⏭️  STA Password empty, keeping current");
    }
    
    if (strlen(newParams.APWifiID) > 0) {
        safeStringCopy(currentParams.APWifiID, newParams.APWifiID, sizeof(currentParams.APWifiID));
        Serial.println("✅ Updated AP SSID");
    } else {
        Serial.println("⏭️  AP SSID empty, keeping current");
    }
    
    if (strlen(newParams.APpassword) > 0) {
        safeStringCopy(currentParams.APpassword, newParams.APpassword, sizeof(currentParams.APpassword));
        Serial.println("✅ Updated AP Password");
    } else {
        Serial.println("⏭️  AP Password empty, keeping current");
    }
    
    if (strlen(newParams.mqttServer) > 0) {
        safeStringCopy(currentParams.mqttServer, newParams.mqttServer, sizeof(currentParams.mqttServer));
        Serial.println("✅ Updated MQTT Server");
    } else {
        Serial.println("⏭️  MQTT Server empty, keeping current");
    }

    if (strlen(newParams.mqttPort) > 0) {
        safeStringCopy(currentParams.mqttPort, newParams.mqttPort, sizeof(currentParams.mqttPort));
        Serial.println("✅ Updated MQTT Port");
    } else {
        Serial.println("⏭️  MQTT Port empty, keeping current");
    }
    
    // Ensure magic number is preserved
    currentParams.magic = EEPROM_MAGIC;
    
    EEPROM.put(WIFI_PARAMS_ADDR, currentParams);
    Serial.println("📤 Writing to EEPROM...");
    
    if (!EEPROM.commit()) {
        Serial.println("❌ ERROR: EEPROM commit failed!");
    } else {
        Serial.println("✅ WiFi & MQTT settings saved successfully to EEPROM");
        Serial.printf("📊 Final settings - STA: %s, AP: %s, MQTT: %s:%s\n", 
                     currentParams.STAWifiID, currentParams.APWifiID, 
                     currentParams.mqttServer, currentParams.mqttPort);
    }
}

void loadWifi() {
    Serial.println("📖 loadWifi() called");
    
    EEPROM.get(WIFI_PARAMS_ADDR, currentParams);
    Serial.printf("🔍 Read from EEPROM - Magic: 0x%08X\n", currentParams.magic);
    
    // If EEPROM data corrupt, reset to default
    if (currentParams.magic != EEPROM_MAGIC) {
        Serial.println("⚠️  EEPROM corrupted or first boot, loading defaults");
        currentParams.magic = EEPROM_MAGIC;
        
        // ✅ OPTIMIZED: Use helper for default strings
        safeStringCopy(currentParams.STAWifiID, "Tanand_Hardware", sizeof(currentParams.STAWifiID));
        safeStringCopy(currentParams.STApassword, "202040406060808010102020", sizeof(currentParams.STApassword));
        safeStringCopy(currentParams.APWifiID, "ESP8266_AP", sizeof(currentParams.APWifiID));
        safeStringCopy(currentParams.APpassword, "12345678", sizeof(currentParams.APpassword));
        safeStringCopy(currentParams.mqttServer, "192.168.31.66", sizeof(currentParams.mqttServer));
        safeStringCopy(currentParams.mqttPort, "1883", sizeof(currentParams.mqttPort));
        
        Serial.println("📝 Default settings loaded:");
        Serial.printf("   STA: %s\n", currentParams.STAWifiID);
        Serial.printf("   AP: %s\n", currentParams.APWifiID);
        Serial.printf("   MQTT: %s:%s\n", currentParams.mqttServer, currentParams.mqttPort);
        
        saveWifi(currentParams);
    } else {
        Serial.println("✅ EEPROM data valid");
        Serial.printf("📋 Loaded settings - STA: %s, AP: %s, MQTT: %s:%s\n", 
                     currentParams.STAWifiID, currentParams.APWifiID, 
                     currentParams.mqttServer, currentParams.mqttPort);
    }
}
