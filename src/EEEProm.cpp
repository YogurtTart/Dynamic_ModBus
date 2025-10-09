
#include "EEEProm.h"

WifiParams currentParams;

void initEEEPROM(){
    EEPROM.begin(EEPROM_SIZE);
}

void saveWifi(WifiParams &newParams) {
    // Load current config first
    EEPROM.get(WIFI_PARAMS_ADDR, currentParams);
    
    // Only update STA if new values are not empty
    if (strlen(newParams.STAWifiID) > 0) {
        strncpy(currentParams.STAWifiID, newParams.STAWifiID, sizeof(currentParams.STAWifiID) - 1);
        currentParams.STAWifiID[sizeof(currentParams.STAWifiID) - 1] = '\0';
    }
    
    if (strlen(newParams.STApassword) > 0) {
        strncpy(currentParams.STApassword, newParams.STApassword, sizeof(currentParams.STApassword) - 1);
        currentParams.STApassword[sizeof(currentParams.STApassword) - 1] = '\0';
    }
    
    // Only update AP if new values are not empty  
    if (strlen(newParams.APWifiID) > 0) {
        strncpy(currentParams.APWifiID, newParams.APWifiID, sizeof(currentParams.APWifiID) - 1);
        currentParams.APWifiID[sizeof(currentParams.APWifiID) - 1] = '\0';
    }
    
    if (strlen(newParams.APpassword) > 0) {
        strncpy(currentParams.APpassword, newParams.APpassword, sizeof(currentParams.APpassword) - 1);
        currentParams.APpassword[sizeof(currentParams.APpassword) - 1] = '\0';
    }
    
    // Ensure magic number is preserved
    currentParams.magic = 0xDEADBEEF;
    
    EEPROM.put(WIFI_PARAMS_ADDR, currentParams);
    
    if (!EEPROM.commit()) {
        Serial.println("ERROR: EEPROM commit failed!");
    } else {
        Serial.println("WiFi settings saved successfully");
    }
}


// Load interval from EEPROM
void loadWifi() {
  
EEPROM.get(WIFI_PARAMS_ADDR , currentParams);
    
//If EEEPROM data corrupt, reset to default
 if (currentParams.magic != 0xDEADBEEF) {
        currentParams.magic = 0xDEADBEEF;
        strcpy(currentParams.STAWifiID, "Tanand_Hardware");
        strcpy(currentParams.STApassword, "202040406060808010102020");
        strcpy(currentParams.APWifiID, "ESP8266_AP");
        strcpy(currentParams.APpassword, "12345678"); 
        
       saveWifi(currentParams);
    }
}