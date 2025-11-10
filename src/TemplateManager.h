#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "FSHandler.h"

// Template management functions
bool loadDeviceTemplate(const String& deviceType, JsonObject& templateConfig);
bool mergeWithOverride(JsonObject& slaveConfig, const JsonObject& templateConfig, JsonObject& output);
void detectOverrides(const JsonObject& currentConfig, const JsonObject& templateConfig, JsonObject& overrideOutput, int depth = 0);
bool saveTemplates(const JsonDocument& templates);
bool loadTemplates(JsonDocument& templates);

// Internal helper functions  
bool deepCompare(const JsonVariant& a, const JsonVariant& b, int depth = 0);
void deepMerge(const JsonObject& source, JsonObject& dest, int depth = 0); 

void clearTemplateCache();