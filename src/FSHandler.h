#pragma once
#include <LittleFS.h>
#include <ArduinoJson.h>

bool initFileSystem();
bool fileExists(const String& path);
String readFile(const String& path);
bool writeFile(const String& path, const String& content);

bool saveSlaveConfig(const JsonDocument& config);
bool loadSlaveConfig(JsonDocument& config);
bool savePollInterval(int interval);
int loadPollInterval();