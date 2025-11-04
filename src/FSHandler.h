#pragma once
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ModBusHandler.h"

bool initFileSystem();
bool fileExists(const String& path);
String readFile(const String& path);
bool writeFile(const String& path, const String& content);

bool saveSlaveConfig(const JsonDocument& config);
bool loadSlaveConfig(JsonDocument& config);

// ✅ COMBINED: Only the new combined functions
bool savePollingConfig(int interval, int timeoutSeconds);
bool loadPollingConfig(int& interval, int& timeoutSeconds);