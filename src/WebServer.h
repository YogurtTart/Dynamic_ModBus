#pragma once

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

#include "EEEProm.h"
#include "FSHandler.h"

// ==================== EXTERNAL DECLARATIONS ====================

extern ESP8266WebServer server;
extern bool debugEnabled;

// ==================== TIMING MANAGEMENT STRUCTURES ====================

/**
 * @brief Device timing tracking structure
 */
struct DeviceTiming {
    uint8_t slaveId;
    char slaveName[32];
    unsigned long lastSeenTime;
    unsigned long lastSequenceTime;
    bool isFirstMessage;
    unsigned long messageCount;
};

// ==================== GLOBAL TIMING VARIABLES ====================

extern DeviceTiming deviceTiming[20]; // Max 20 devices
extern uint8_t deviceTimeCount;
extern unsigned long lastSequenceTime; // For "Since Prev" across all devices
extern unsigned long systemStartTime; // For "Real Time" calculation

// ==================== TIMING FUNCTIONS ====================

unsigned long calculateTimeDelta(uint8_t slaveId, const char* slaveName);
String getSameDeviceDelta(uint8_t slaveId, const char* slaveName, bool resetTimer = false);
void updateDeviceTiming(uint8_t slaveId, const char* slaveName, unsigned long currentTime);
String formatTimeDelta(unsigned long deltaMs);
String getCurrentTimeString();
void resetAllTiming();

// ==================== WEB SERVER MANAGEMENT ====================

void setupWebServer();

// ==================== REQUEST HANDLERS ====================

void handleRoot();
void handleStaticFiles();
void handleGetIpInfo();

// ==================== WIFI CONFIGURATION HANDLERS ====================

void handleSaveWifi();
void handleGetWifi();

// ==================== SLAVE CONFIGURATION HANDLERS ====================

void handleSaveSlaves();
void handleGetSlaves();
void handleGetSlaveConfig();
void handleUpdateSlaveConfig();

// ==================== POLLING CONFIGURATION HANDLERS ====================

void handleSavePollingConfig();
void handleGetPollingConfig();

// ==================== STATISTICS HANDLERS ====================

void handleGetStatistics();
void handleRemoveSlaveStats();

// ==================== DEBUG MANAGEMENT HANDLERS ====================

void handleToggleDebug();
void handleGetDebugState();
void handleGetDebugMessages();
void handleClearTable();
void addDebugMessage(const char* topic, const char* message, const char* timeDelta, const char* sameDeviceDelta);

// ==================== UTILITY FUNCTIONS ====================

String getContentType(const String& filename);

// ==================== HELPER FUNCTIONS ====================

void serveHtmlFile(const String& filename);
void sendJsonResponse(const JsonDocument& doc);
bool parseJsonBody(JsonDocument& doc);