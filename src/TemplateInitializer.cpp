#include "TemplateInitializer.h"
#include "FSHandler.h"
#include "TemplateManager.h"


void addG01SConfig(JsonObject& templateObj) {
    JsonObject sensorParams = templateObj["sensor"].to<JsonObject>();
    sensorParams["tempdivider"] = 1.0;
    sensorParams["humiddivider"] = 1.0;
}

void addMeterConfig(JsonObject& templateObj) {
    // ✅ FIXED: Use camelCase to match C++ struct
    const char* meterConfigs[] = {
        "aCurrent", "bCurrent", "cCurrent", "zeroPhaseCurrent",
        "aActivePower", "bActivePower", "cActivePower", "totalActivePower",
        "aReactivePower", "bReactivePower", "cReactivePower", "totalReactivePower",
        "aApparentPower", "bApparentPower", "cApparentPower", "totalApparentPower",
        "aPowerFactor", "bPowerFactor", "cPowerFactor", "totalPowerFactor"
    };
    
    JsonObject meterParams = templateObj["meter"].to<JsonObject>();
    
    for (const char* config : meterConfigs) {
        JsonObject param = meterParams[config].to<JsonObject>();
        param["ct"] = 1.0;
        param["pt"] = 1.0;

        // 🆕 SET 1000 DIVIDER FOR POWER VALUES, 1.0 FOR OTHERS
        if (strstr(config, "ActivePower") != nullptr) { // Contains "Power"
            param["divider"] = 1000.0;
        } else {
            param["divider"] = 1.0;
        }
    }
}

void addVoltageConfig(JsonObject& templateObj) {
    // ✅ FIXED: Use camelCase to match C++ struct
    const char* voltageConfigs[] = {
        "aVoltage", "bVoltage", "cVoltage", 
        "phaseVoltageMean", "zeroSequenceVoltage"
    };
    
    JsonObject voltageParams = templateObj["voltage"].to<JsonObject>();
    
    for (const char* config : voltageConfigs) {
        JsonObject param = voltageParams[config].to<JsonObject>();
        param["pt"] = 1.0;
        param["divider"] = 1.0;
    }
}

void addEnergyConfig(JsonObject& templateObj) {
    JsonObject energyParams = templateObj["energy"].to<JsonObject>();
    
    JsonObject totalActiveEnergy = energyParams["totalActiveEnergy"].to<JsonObject>();
    totalActiveEnergy["divider"] = 1.0;
    
    JsonObject importActiveEnergy = energyParams["importActiveEnergy"].to<JsonObject>();
    importActiveEnergy["divider"] = 1.0;
    
    JsonObject exportActiveEnergy = energyParams["exportActiveEnergy"].to<JsonObject>();
    exportActiveEnergy["divider"] = 1.0;
}

bool createDefaultTemplates() {
    if (fileExists("/templates.json")) {
        Serial.println("✅ Templates already exist, skipping creation");
        return true;
    }

    JsonDocument templatesDoc;

    // G01S Template
    JsonObject g01sTemplate = templatesDoc["G01S"].to<JsonObject>();
    addG01SConfig(g01sTemplate);

    // HeylaParam Template - FIXED: camelCase
    JsonObject heylaParamTemplate = templatesDoc["HeylaParam"].to<JsonObject>();
    addMeterConfig(heylaParamTemplate);

    // HeylaVoltage Template - FIXED: camelCase  
    JsonObject heylaVoltageTemplate = templatesDoc["HeylaVoltage"].to<JsonObject>();
    addVoltageConfig(heylaVoltageTemplate);

    // HeylaEnergy Template - Already correct
    JsonObject heylaEnergyTemplate = templatesDoc["HeylaEnergy"].to<JsonObject>();
    addEnergyConfig(heylaEnergyTemplate);

    // Save to file
    bool success = saveTemplates(templatesDoc);
    if (success) {
        Serial.println("✅ Default templates created successfully with camelCase naming");
        
        // Debug: Print the created templates
        String jsonString;
        serializeJson(templatesDoc, jsonString);
        Serial.println("📋 Created templates:");
        Serial.println(jsonString);
    } else {
        Serial.println("❌ Failed to create default templates");
    }
    return success;
}
