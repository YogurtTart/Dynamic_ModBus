#include "TemplateInitializer.h"
#include "FSHandler.h"
#include "TemplateManager.h"

// ==================== DEVICE TEMPLATE BUILDERS ====================

void addG01SConfig(JsonObject& templateObj) {
    JsonObject sensorParams = templateObj["sensor"].to<JsonObject>();
    sensorParams["tempdivider"] = 1.0;
    sensorParams["humiddivider"] = 1.0;
}

void addMeterConfig(JsonObject& templateObj) {
    JsonObject meterParams = templateObj["meter"].to<JsonObject>();
    
    struct MeterParam {
        const char* name;
        float divider;
    };
    
    MeterParam meterConfigs[] = {
        {"aCurrent", 1.0}, {"bCurrent", 1.0}, {"cCurrent", 1.0}, {"zeroPhaseCurrent", 1.0}, 
        {"aActivePower", 1000.0}, {"bActivePower", 1000.0}, {"cActivePower", 1000.0}, {"totalActivePower", 10000.0},
        {"aReactivePower", 1000.0}, {"bReactivePower", 1000.0}, {"cReactivePower", 1000.0}, {"totalReactivePower", 10000.0},
        {"aApparentPower", 1000.0}, {"bApparentPower", 1000.0}, {"cApparentPower", 1000.0}, {"totalApparentPower", 10000.0},
        {"aPowerFactor", 1.0}, {"bPowerFactor", 1.0}, {"cPowerFactor", 1.0}, {"totalPowerFactor", 1.0}
    };
    
    for (const MeterParam& config : meterConfigs) {
        JsonObject param = meterParams[config.name].to<JsonObject>();
        param["ct"] = 1.0;
        param["pt"] = 1.0;
        param["divider"] = config.divider;
    }

}

void addVoltageConfig(JsonObject& templateObj) {
    JsonObject voltageParams = templateObj["voltage"].to<JsonObject>();
    
    // ✅ OPTIMIZED: Single array definition
    const char* voltageConfigs[] = {
        "aVoltage", "bVoltage", "cVoltage", 
        "phaseVoltageMean", "zeroSequenceVoltage"
    };
    
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

// ==================== TEMPLATE CREATION ====================

bool createDefaultTemplates() {
    // ✅ OPTIMIZED: Single file existence check
    if (fileExists("/templates.json")) {
        Serial.println("✅ Templates already exist, skipping creation");
        return true;
    }

    Serial.println("📝 Creating default templates...");
    JsonDocument templatesDoc;

    // ✅ OPTIMIZED: Template definitions in array
    struct TemplateDef {
        const char* name;
        void (*builder)(JsonObject&);
    };
    
    TemplateDef templates[] = {
        {"G01S", addG01SConfig},
        {"HeylaParam", addMeterConfig},
        {"HeylaVoltage", addVoltageConfig},
        {"HeylaEnergy", addEnergyConfig}
    };

    // Create all templates
    for (const auto& templateDef : templates) {
        JsonObject templateObj = templatesDoc[templateDef.name].to<JsonObject>();
        templateDef.builder(templateObj);
        Serial.printf("✅ Created template: %s\n", templateDef.name);
    }

    // Save to file
    bool success = saveTemplates(templatesDoc);
    if (success) {
        Serial.println("✅ Default templates created successfully");
        
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

// 🆕 ADDED: Check if templates need creation
bool templatesNeedCreation() {
    return !fileExists("/templates.json");
}

// 🆕 ADDED: Get template count
int getTemplateCount() {
    if (!fileExists("/templates.json")) {
        return 0;
    }
    
    JsonDocument templatesDoc;
    if (loadTemplates(templatesDoc)) {
        return templatesDoc.size();
    }
    return 0;
}