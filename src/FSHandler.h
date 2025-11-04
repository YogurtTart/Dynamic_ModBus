#pragma once

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ModBusHandler.h"

// ==================== FILE SYSTEM FUNCTIONS ====================

bool initFileSystem();

bool fileExists(const String& path);

String readFile(const String& path);

bool writeFile(const String& path, const String& content);

// ==================== SLAVE CONFIGURATION FUNCTIONS ====================

bool saveSlaveConfig(const JsonDocument& config);

bool loadSlaveConfig(JsonDocument& config);

// ==================== POLLING CONFIGURATION FUNCTIONS ====================

bool savePollingConfig(int interval, int timeoutSeconds);

bool loadPollingConfig(int& interval, int& timeoutSeconds);