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
void connectSTA();        // Manual STA connection (non-blocking)
void disconnectSTA();     // Manual STA disconnection
bool isSTAConnected();    // Check if STA is connected
bool isSTAConnecting();   // NEW: Check if connection is in progress
String getSTAStatus();    // NEW: Get detailed status text

bool isWiFiConnected();
String getSTAIP();
String getAPIP();
int getAPClientCount();

void initializeOTA();
void handleOTA();