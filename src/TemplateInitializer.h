#pragma once

#include <ArduinoJson.h>

bool createDefaultTemplates();
void addG01SConfig(JsonObject& templateObj); 
void addMeterConfig(JsonObject& templateObj);
void addVoltageConfig(JsonObject& templateObj); 
void addEnergyConfig(JsonObject& templateObj);


bool templatesNeedCreation();
int getTemplateCount();