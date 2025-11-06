#include "TemplateManager.h"


bool loadDeviceTemplate(const String& deviceType, JsonObject& templateConfig) {
    JsonDocument templatesDoc;
    if (!loadTemplates(templatesDoc)) {
        return false;
    }
    
    // 🆕 FIX: Use new ArduinoJson API
    if (templatesDoc[deviceType].is<JsonObject>()) {
        templateConfig.set(templatesDoc[deviceType]);
        return true;
    }
    
    return false;
}

bool mergeWithOverride(JsonObject& slaveConfig, const JsonObject& templateConfig, JsonObject& output) {
    // First, copy the template as base
    output.set(templateConfig);
    
    // 🆕 FIX: Use new ArduinoJson API
    if (slaveConfig["override"].is<JsonObject>()) {
        JsonObject overrideObj = slaveConfig["override"];
        deepMerge(overrideObj, output);
    }
    
    return true;
}

void detectOverrides(const JsonObject& currentConfig, const JsonObject& templateConfig, JsonObject& overrideOutput) {
    for (JsonPair kv : currentConfig) {
        const char* key = kv.key().c_str();
        JsonVariant currentValue = kv.value();
        
        // Check if this key exists in template
        if (templateConfig[key].isNull() == false) {
            JsonVariant templateValue = templateConfig[key];
            
            // If both are objects, recursively compare their contents
            if (currentValue.is<JsonObject>() && templateValue.is<JsonObject>()) {
                JsonObject currentObj = currentValue.as<JsonObject>();
                JsonObject templateObj = templateValue.as<JsonObject>();
                JsonObject overrideObj = overrideOutput[key].to<JsonObject>();
                
                // 🆕 RECURSIVE CALL: Compare nested objects
                detectOverrides(currentObj, templateObj, overrideObj);
                
                // Remove empty nested objects from override
                if (overrideObj.size() == 0) {
                    overrideOutput.remove(key);
                }
                
            } else {
                // Primitive values - compare directly
                if (!deepCompare(currentValue, templateValue)) {
                    overrideOutput[key].set(currentValue);
                }
            }
        } else {
            // Key doesn't exist in template - always store
            overrideOutput[key].set(currentValue);
        }
    }
}

bool deepCompare(const JsonVariant& a, const JsonVariant& b) {
    // Handle nested objects recursively
    if (a.is<JsonObject>() && b.is<JsonObject>()) {
        JsonObject objA = a.as<JsonObject>();
        JsonObject objB = b.as<JsonObject>();
        
        if (objA.size() != objB.size()) return false;
        
        // 🆕 Use temporary override to check if objects are identical
        JsonDocument tempDoc;
        JsonObject tempOverride = tempDoc.to<JsonObject>();
        detectOverrides(objA, objB, tempOverride);
        
        return (tempOverride.size() == 0); // Objects are identical if no overrides
    }
    
    // Handle arrays
    if (a.is<JsonArray>() && b.is<JsonArray>()) {
        JsonArray arrA = a.as<JsonArray>();
        JsonArray arrB = b.as<JsonArray>();
        
        if (arrA.size() != arrB.size()) return false;
        
        for (size_t i = 0; i < arrA.size(); i++) {
            if (!deepCompare(arrA[i], arrB[i])) {
                return false;
            }
        }
        return true;
    }
    
    // Primitive types
    return a == b;
}

void deepMerge(const JsonObject& source, JsonObject& dest) {
    for (JsonPair kv : source) {
        const char* key = kv.key().c_str();
        
        // 🆕 FIX: Use new ArduinoJson API
        if (kv.value().is<JsonObject>() && dest[key].is<JsonObject>()) {
            // Recursively merge nested objects
            JsonObject nestedSource = kv.value().as<JsonObject>();
            JsonObject nestedDest = dest[key].as<JsonObject>();
            deepMerge(nestedSource, nestedDest);
        } else {
            // Replace or add the value
            dest[key].set(kv.value());
        }
    }
}

bool saveTemplates(const JsonDocument& templates) {
    String jsonString;
    serializeJson(templates, jsonString);
    return writeFile("/templates.json", jsonString);
}

bool loadTemplates(JsonDocument& templates) {
    if (!fileExists("/templates.json")) {
        return false;
    }
    
    String jsonString = readFile("/templates.json");
    if (jsonString.length() == 0) {
        return false;
    }
    
    DeserializationError error = deserializeJson(templates, jsonString);
    return !error;
}