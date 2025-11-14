#pragma once

#include <ArduinoJson.h>

bool createDefaultTemplates();
void addG01SConfig(JsonObject& templateObj); 
void addMeterConfig(JsonObject& templateObj);
void addVoltageConfig(JsonObject& templateObj); 
void addEnergyConfig9(JsonObject& templateObj);
void addEnergyConfig27(JsonObject& templateObj);


bool templatesNeedCreation();
int getTemplateCount();