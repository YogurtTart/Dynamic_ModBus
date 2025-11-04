#pragma once

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

#include "EEEProm.h"
#include "FSHandler.h"

extern ESP8266WebServer server;
extern bool debugEnabled;

// ==================== ENHANCED DYNAMIC TIMING MANAGEMENT ====================
struct DeviceTiming {
    uint8_t slaveId;
    char slaveName[32];
    unsigned long lastSeenTime;
    unsigned long lastSequenceTime;
    bool isFirstMessage;
    unsigned long messageCount;
};

// Global timing variables
extern DeviceTiming deviceTiming[20]; // Max 20 devices
extern uint8_t deviceTimeCount;
extern unsigned long lastSequenceTime; // For "Since Prev" across all devices
extern unsigned long systemStartTime; // For "Real Time" calculation

// Timing functions
unsigned long calculateTimeDelta(uint8_t slaveId, const char* slaveName);
String getSameDeviceDelta(uint8_t slaveId, const char* slaveName, bool resetTimer = false);
void updateDeviceTiming(uint8_t slaveId, const char* slaveName, unsigned long currentTime);
String formatTimeDelta(unsigned long deltaMs);
String getCurrentTimeString();
void resetAllTiming();

// Web Server Management
void setupWebServer();

// Request Handlers
void handleRoot();
void handleSlavesPage();
void handleStaticFiles();
void handleGetIpInfo();

// WiFi Configuration
void handleSaveWifi();
void handleGetWifi();

// Slave Configuration
void handleSaveSlaves();
void handleGetSlaves();
void handleGetSlaveConfig();
void handleUpdateSlaveConfig();

// Polling Configuration
void handleSavePollingConfig();
void handleGetPollingConfig();

// Statistics
void handleGetStatistics();
void handleRemoveSlaveStats();

// Debug Management
void handleToggleDebug();
void handleGetDebugState();
void handleGetDebugMessages();
void handleClearTable();
void addDebugMessage(const char* topic, const char* message, const char* timeDelta, const char* sameDeviceDelta);

// Utilities
String getContentType(const String& filename);

// Helper Functions
void serveHtmlFile(const String& filename);
void sendJsonResponse(const JsonDocument& doc);
bool parseJsonBody(JsonDocument& doc);