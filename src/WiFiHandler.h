#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "EEEProm.h"

// Forward declarations
extern WifiParams currentParams;

// Global variables
extern bool otaInitialized;

// Function declarations
void setupWiFi();
void checkWiFi();

bool isWiFiConnected();
String getSTAIP();
String getAPIP();
int getAPClientCount();