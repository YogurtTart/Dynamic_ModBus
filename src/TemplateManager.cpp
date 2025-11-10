#include "TemplateManager.h"

// ==================== TEMPLATE CACHE ====================
JsonDocument templatesCache;
bool cacheLoaded = false;

// ==================== SAFETY CONSTANTS ====================
constexpr int MAX_RECURSION_DEPTH = 10;

bool loadDeviceTemplate(const String& deviceType, JsonObject& templateConfig) {

    // Load cache if not already loaded
    if (!cacheLoaded) {
        if (!loadTemplates(templatesCache)) {
            return false;
        }
        cacheLoaded = true;
    }
    
    if (templatesCache[deviceType].is<JsonObject>()) {
        templateConfig.set(templatesCache[deviceType]);
        return true;
    }
    
    return false;
}

bool mergeWithOverride(JsonObject& slaveConfig, const JsonObject& templateConfig, JsonObject& output) {
    output.set(templateConfig);
    
    if (slaveConfig["override"].is<JsonObject>()) {
        JsonObject overrideObj = slaveConfig["override"];
        deepMerge(overrideObj, output);
    }
    
    return true;
}

// ✅ CLEAN: Single function, no useless wrapper
void detectOverrides(const JsonObject& currentConfig, const JsonObject& templateConfig, JsonObject& overrideOutput, int depth) {
    // ✅ Safety check built in
    if (depth > MAX_RECURSION_DEPTH) {
        Serial.println("⚠️  Max recursion depth reached in detectOverrides!");
        return;
    }
    
    for (JsonPair kv : currentConfig) {
        const char* key = kv.key().c_str();
        JsonVariant currentValue = kv.value();
        
        if (templateConfig[key].isNull() == false) {
            JsonVariant templateValue = templateConfig[key];
            
            if (currentValue.is<JsonObject>() && templateValue.is<JsonObject>()) {
                JsonObject currentObj = currentValue.as<JsonObject>();
                JsonObject templateObj = templateValue.as<JsonObject>();
                JsonObject overrideObj = overrideOutput[key].to<JsonObject>();
                
                // ✅ Direct recursive call with depth+1
                detectOverrides(currentObj, templateObj, overrideObj, depth + 1);
                
                if (overrideObj.size() == 0) {
                    overrideOutput.remove(key);
                }
                
            } else {
                if (!deepCompare(currentValue, templateValue)) {
                    overrideOutput[key].set(currentValue);
                }
            }
        } else {
            overrideOutput[key].set(currentValue);
        }
    }
}

// ✅ CLEAN: Single function, no useless wrapper
bool deepCompare(const JsonVariant& a, const JsonVariant& b, int depth) {
    // ✅ Safety check built in
    if (depth > MAX_RECURSION_DEPTH) {
        Serial.println("⚠️  Max recursion depth reached in deepCompare!");
        return false;
    }
    
    if (a.is<JsonObject>() && b.is<JsonObject>()) {
        JsonObject objA = a.as<JsonObject>();
        JsonObject objB = b.as<JsonObject>();
        
        if (objA.size() != objB.size()) return false;
        
        for (JsonPair kv : objA) {
            const char* key = kv.key().c_str();
            if (objB[key].isNull() || !deepCompare(kv.value(), objB[key], depth + 1)) {
                return false;
            }
        }
        return true;
    }
    
    if (a.is<JsonArray>() && b.is<JsonArray>()) {
        JsonArray arrA = a.as<JsonArray>();
        JsonArray arrB = b.as<JsonArray>();
        
        if (arrA.size() != arrB.size()) return false;
        
        for (size_t i = 0; i < arrA.size(); i++) {
            if (!deepCompare(arrA[i], arrB[i], depth + 1)) {
                return false;
            }
        }
        return true;
    }
    
    return a == b;
}

// ✅ CLEAN: Single function, no useless wrapper
void deepMerge(const JsonObject& source, JsonObject& dest, int depth) {
    // ✅ Safety check built in
    if (depth > MAX_RECURSION_DEPTH) {
        Serial.println("⚠️  Max recursion depth reached in deepMerge!");
        return;
    }
    
    for (JsonPair kv : source) {
        const char* key = kv.key().c_str();
        
        if (kv.value().is<JsonObject>() && dest[key].is<JsonObject>()) {
            JsonObject nestedSource = kv.value().as<JsonObject>();
            JsonObject nestedDest = dest[key].as<JsonObject>();
            deepMerge(nestedSource, nestedDest, depth + 1);
        } else {
            dest[key].set(kv.value());
        }
    }
}

bool saveTemplates(const JsonDocument& templates) {
    String jsonString;
    serializeJson(templates, jsonString);
    
    clearTemplateCache();
    
    return writeFile("/templates.json", jsonString);
}

bool loadTemplates(JsonDocument& templates) {
    if (!fileExists("/templates.json")) {
        return false;
    }
    
    // Use streaming for large template files
    File file = LittleFS.open("/templates.json", "r");
    if (!file) {
        return false;
    }
    
    DeserializationError error = deserializeJson(templates, file);
    file.close();
    return !error;
}

void clearTemplateCache() {
    cacheLoaded = false;
    templatesCache.clear();
    Serial.println("✅ Template cache cleared");
}