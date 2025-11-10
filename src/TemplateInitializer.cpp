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
    
    // All parameters
    const char* meterConfigs[] = {
        "aCurrent", "bCurrent", "cCurrent", "zeroPhaseCurrent",
        "aActivePower", "bActivePower", "cActivePower", "totalActivePower",
        "aReactivePower", "bReactivePower", "cReactivePower", "totalReactivePower",
        "aApparentPower", "bApparentPower", "cApparentPower", "totalApparentPower",
        "aPowerFactor", "bPowerFactor", "cPowerFactor", "totalPowerFactor"
    };
    
    // ✅ ONLY these get 1000 divider
    const char* power1000Configs[] = {
        "aActivePower", "bActivePower", "cActivePower",
        "aReactivePower", "bReactivePower", "cReactivePower", 
        "aApparentPower", "bApparentPower", "cApparentPower"
    };
    
    for (const char* config : meterConfigs) {
        JsonObject param = meterParams[config].to<JsonObject>();
        param["ct"] = 1.0;
        param["pt"] = 1.0;

        // ✅ SIMPLE: Check if this config is in the 1000-divider list
        bool is1000Divider = false;
        for (const char* powerConfig : power1000Configs) {
            if (strcmp(config, powerConfig) == 0) {
                is1000Divider = true;
                break;
            }
        }
        
        param["divider"] = is1000Divider ? 1000.0 : 1.0;
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