#pragma once

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

#include "EEEProm.h"
#include "FSHandler.h"

extern ESP8266WebServer server;
extern bool debugEnabled;

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
void handleSavePollInterval();
void handleGetPollInterval();

// Timeout Configuration
void handleSaveTimeout();
void handleGetTimeout();

// Statistics
void handleGetStatistics();
void handleRemoveSlaveStats();

// Debug Management
void handleToggleDebug();
void handleGetDebugState();
void handleGetDebugMessages();
void addDebugMessage(const char* topic, const char* message);

// Utilities
String getContentType(const String& filename);

// Helper Functions
void serveHtmlFile(const String& filename);
void sendJsonResponse(const JsonDocument& doc);
bool parseJsonBody(JsonDocument& doc);