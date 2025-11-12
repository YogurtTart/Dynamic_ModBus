#include "FSHandler.h"

// ==================== FILE SYSTEM OPERATIONS ====================

bool initFileSystem() {
    Serial.println("🔧 Attempting to mount LittleFS...");
    if (!LittleFS.begin()) {
        Serial.println("❌ ERROR: Failed to mount LittleFS");
        return false;
    }
    Serial.println("✅ LittleFS mounted successfully");
    
    Serial.println("📁 Listing files in LittleFS:");
    Dir dir = LittleFS.openDir("/");
    int fileCount = 0;
    
    while (dir.next()) {
        Serial.printf("   📄 %s (%d bytes)\n", dir.fileName().c_str(), dir.fileSize());
        fileCount++;
    }
    
    if (fileCount == 0) {
        Serial.println("⚠️  No files found in LittleFS - did you upload filesystem?");
    } else {
        Serial.printf("✅ Found %d files in LittleFS\n", fileCount);
    }
    
    return true;
}

bool fileExists(const String& path) {
    bool exists = LittleFS.exists(path);
    Serial.printf("🔍 File check: %s - %s\n", path.c_str(), exists ? "EXISTS" : "NOT FOUND");
    return exists;
}

String readFile(const String& path) {
    Serial.printf("📖 Reading file: %s\n", path.c_str());
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.println("❌ Failed to open file for reading");
        return String();
    }
    String content = file.readString();
    file.close();
    Serial.printf("✅ Read %d bytes from %s\n", content.length(), path.c_str());
    return content;
}

bool writeFile(const String& path, const String& content) {
    Serial.printf("📝 Writing %d bytes to: %s\n", content.length(), path.c_str());
    File file = LittleFS.open(path, "w");
    if (!file) {
        Serial.println("❌ Failed to open file for writing");
        return false;
    }
    size_t bytesWritten = file.print(content);
    file.close();
    Serial.printf("✅ Wrote %d bytes to %s\n", bytesWritten, path.c_str());
    return (bytesWritten > 0);
}

// ==================== SLAVE CONFIGURATION FUNCTIONS ====================

bool saveSlaveConfig(const JsonDocument& config) {
    Serial.println("💾 Saving slave configuration to LittleFS (STREAMING MODE)...");
    
    // uint32_t freeHeap = ESP.getFreeHeap();
    // Serial.printf("📊 Free heap before save: %d bytes\n", freeHeap);
    
    // if (freeHeap < 10000) {
    //     Serial.println("🆘 CRITICAL: Not enough memory for JSON serialization - ABORTING");
    //     return false;
    // }
    
    File file = LittleFS.open("/slaves.json", "w");
    if (!file) {
        Serial.println("❌ Failed to open slaves.json for writing");
        return false;
    }
    
    size_t bytesWritten = serializeJson(config, file);
    file.close();
    
    if (bytesWritten == 0) {
        Serial.println("❌ Failed to write slave configuration");
        return false;
    }
    
    Serial.printf("✅ Wrote %d bytes to slaves.json. Free heap after: %d bytes\n", 
                  bytesWritten, ESP.getFreeHeap());
    return true;
}

bool loadSlaveConfig(JsonDocument& config) {
    Serial.println("📖 Loading slave configuration from LittleFS (STREAMING MODE)...");
    
    // uint32_t freeHeap = ESP.getFreeHeap();
    // Serial.printf("📊 Free heap before load: %d bytes\n", freeHeap);
    
    // if (freeHeap < 15000) {
    //     Serial.println("🆘 CRITICAL: Not enough memory for JSON parsing - ABORTING");
    //     return false;
    // }
    
    if (!fileExists("/slaves.json")) {
        Serial.println("⚠️  No slave configuration found, using defaults");
        return false;
    }
    
    File file = LittleFS.open("/slaves.json", "r");
    if (!file) {
        Serial.println("❌ Failed to open slaves.json for reading");
        return false;
    }
    
    size_t fileSize = file.size();
    Serial.printf("📁 File size: %d bytes\n", fileSize);
    
    if (fileSize > 5000) {
        Serial.println("⚠️  WARNING: Large JSON file - memory may be tight");
    }
    
    DeserializationError error = deserializeJson(config, file);
    file.close();
    
    if (error) {
        Serial.printf("❌ Failed to parse slave config: %s\n", error.c_str());
        return false;
    }

    Serial.printf("✅ Slave configuration loaded successfully. Free heap after: %d bytes\n", ESP.getFreeHeap());
    return true;
}

// ==================== POLLING CONFIGURATION FUNCTIONS ====================

bool savePollingConfig(int interval, int timeoutSeconds) {
    Serial.printf("💾 Saving polling config (interval: %ds, timeout: %ds) to LittleFS...\n", interval, timeoutSeconds);
    
    JsonDocument doc;
    doc["pollInterval"] = interval;
    doc["timeout"] = timeoutSeconds;
    
    File file = LittleFS.open("/polling.json", "w");
    if (!file) {
        Serial.println("❌ Failed to open polling.json for writing");
        return false;
    }
    
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    bool success = (bytesWritten > 0);
    if (success) {
        Serial.println("✅ Polling config saved successfully");
        modbusReloadSlaves();
    } else {
        Serial.println("❌ Failed to save polling config");
    }

    return success;
}

bool loadPollingConfig(int& interval, int& timeoutSeconds) {
    Serial.println("📖 Loading polling config from LittleFS...");
    
    if (!fileExists("/polling.json")) {
        Serial.println("⚠️  No polling config found, using defaults (interval: 10s, timeout: 1s)");
        interval = 10;
        timeoutSeconds = 1;
        return false;
    }
    
    File file = LittleFS.open("/polling.json", "r");
    if (!file) {
        Serial.println("❌ Failed to open polling.json for reading");
        interval = 10;
        timeoutSeconds = 1;
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.printf("❌ Failed to parse polling config: %s, using defaults\n", error.c_str());
        interval = 10;
        timeoutSeconds = 1;
        return false;
    }
    
    interval = doc["pollInterval"] | 10;
    timeoutSeconds = doc["timeout"] | 1;
    Serial.printf("✅ Polling config loaded: interval=%ds, timeout=%ds\n", interval, timeoutSeconds);
    return true;
}