#pragma once

#include <EEPROM.h>
#include <ArduinoJson.h>

// EEPROM addresses
#define EEPROM_SIZE 512
#define WIFI_PARAMS_ADDR  0

// ---------- Wifi Parameter Struct ----------
struct WifiParams {
  uint32_t magic;
  char STAWifiID[32];
  char STApassword[32];
  char APWifiID[32];
  char APpassword[32];
  char mqttServer[64]; 
};

// ---------- Global Parameter ----------
extern WifiParams currentParams;

// Function declarations
void initEEEPROM();
void saveWifi(WifiParams &newParams);
void loadWifi();