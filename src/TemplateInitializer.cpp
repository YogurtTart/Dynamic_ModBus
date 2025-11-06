#include "TemplateInitializer.h"
#include "FSHandler.h"
#include "TemplateManager.h"

// Helper function to add G01S config to template
void addG01SConfig(JsonObject& templateObj) {
    JsonObject sensorParams = templateObj["sensor"].to<JsonObject>();
    sensorParams["tempdivider"] = 1.0;
    sensorParams["humiddivider"] = 1.0;
}

// Helper function to add meter config to template
void addMeterConfig(JsonObject& templateObj) {
    const char* meterConfigs[] = {
        "ACurrent", "BCurrent", "CCurrent", "ZeroPhaseCurrent",
        "AActivePower", "BActivePower", "CActivePower", "TotalActivePower",
        "AReactivePower", "BReactivePower", "CReactivePower", "TotalReactivePower",
        "AApparentPower", "BApparentPower", "CApparentPower", "TotalApparentPower",
        "APowerFactor", "BPowerFactor", "CPowerFactor", "TotalPowerFactor"
    };
    
    JsonObject meterParams = templateObj["meter"].to<JsonObject>(); // 🆕 Nested under "meter"
    
    for (const char* config : meterConfigs) {
        JsonObject param = meterParams[config].to<JsonObject>();
        param["ct"] = 1.0;
        param["pt"] = 1.0;
        param["divider"] = 1.0;
    }
}

// Helper function to add voltage config to template
void addVoltageConfig(JsonObject& templateObj) {
    const char* voltageConfigs[] = {
        "AVoltage", "BVoltage", "CVoltage", 
        "PhaseVoltageMean", "ZeroSequenceVoltage"
    };
    
    JsonObject voltageParams = templateObj["voltage"].to<JsonObject>(); // 🆕 Nested under "voltage"
    
    for (const char* config : voltageConfigs) {
        JsonObject param = voltageParams[config].to<JsonObject>();
        param["pt"] = 1.0;
        param["divider"] = 1.0;
    }
}

// Helper function to add energy config to template
void addEnergyConfig(JsonObject& templateObj) {
    JsonObject energyParams = templateObj["energy"].to<JsonObject>(); // 🆕 Nested under "energy"
    
    JsonObject totalActiveEnergy = energyParams["totalActiveEnergy"].to<JsonObject>();
    totalActiveEnergy["divider"] = 1.0;
    
    JsonObject importActiveEnergy = energyParams["importActiveEnergy"].to<JsonObject>();
    importActiveEnergy["divider"] = 1.0;
    
    JsonObject exportActiveEnergy = energyParams["exportActiveEnergy"].to<JsonObject>();
    exportActiveEnergy["divider"] = 1.0;
}

// Temporary: Move createDefaultTemplates here
bool createDefaultTemplates() {
    if (fileExists("/templates.json")) {
        return true;
    }

    JsonDocument templatesDoc;

    // G01S Template
    JsonObject g01sTemplate = templatesDoc["G01S"].to<JsonObject>();
    JsonObject sensorParams = g01sTemplate["sensor"].to<JsonObject>();
    sensorParams["tempdivider"] = 1.0;
    sensorParams["humiddivider"] = 1.0;

    // HeylaParam Template
    JsonObject heylaParamTemplate = templatesDoc["HeylaParam"].to<JsonObject>();
    JsonObject meterParams = heylaParamTemplate["meter"].to<JsonObject>();
    
    const char* meterConfigs[] = {
        "ACurrent", "BCurrent", "CCurrent", "ZeroPhaseCurrent",
        "AActivePower", "BActivePower", "CActivePower", "TotalActivePower",
        "AReactivePower", "BReactivePower", "CReactivePower", "TotalReactivePower",
        "AApparentPower", "BApparentPower", "CApparentPower", "TotalApparentPower",
        "APowerFactor", "BPowerFactor", "CPowerFactor", "TotalPowerFactor"
    };
    
    for (const char* config : meterConfigs) {
        JsonObject param = meterParams[config].to<JsonObject>();
        param["ct"] = 1.0;
        param["pt"] = 1.0;
        param["divider"] = 1.0;
    }

    // HeylaVoltage Template
    JsonObject heylaVoltageTemplate = templatesDoc["HeylaVoltage"].to<JsonObject>();
    JsonObject voltageParams = heylaVoltageTemplate["voltage"].to<JsonObject>();
    
    const char* voltageConfigs[] = {
        "AVoltage", "BVoltage", "CVoltage", 
        "PhaseVoltageMean", "ZeroSequenceVoltage"
    };
    
    for (const char* config : voltageConfigs) {
        JsonObject param = voltageParams[config].to<JsonObject>();
        param["pt"] = 1.0;
        param["divider"] = 1.0;
    }

    // HeylaEnergy Template
    JsonObject heylaEnergyTemplate = templatesDoc["HeylaEnergy"].to<JsonObject>();
    JsonObject energyParams = heylaEnergyTemplate["energy"].to<JsonObject>();
    
    JsonObject totalActiveEnergy = energyParams["totalActiveEnergy"].to<JsonObject>();
    totalActiveEnergy["divider"] = 1.0;
    
    JsonObject importActiveEnergy = energyParams["importActiveEnergy"].to<JsonObject>();
    importActiveEnergy["divider"] = 1.0;
    
    JsonObject exportActiveEnergy = energyParams["exportActiveEnergy"].to<JsonObject>();
    exportActiveEnergy["divider"] = 1.0;

    // Save to file
    bool success = saveTemplates(templatesDoc);
    if (success) {
        Serial.println("✅ Default templates created successfully");
    } else {
        Serial.println("❌ Failed to create default templates");
    }
    return success;
}