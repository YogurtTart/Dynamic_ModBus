#include "FSHandler.h"

bool initFileSystem() {
    Serial.println("🔧 Attempting to mount LittleFS...");
    if (!LittleFS.begin()) {
        Serial.println("❌ ERROR: Failed to mount LittleFS");
        return false;
    }
    Serial.println("✅ LittleFS mounted successfully");
    
    // List files for debugging
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

//Save slave configuration
bool saveSlaveConfig(const JsonDocument& config) {
    Serial.println("💾 Saving slave configuration to LittleFS...");
    
    String jsonString;
    serializeJson(config, jsonString);
    
    bool success = writeFile("/slaves.json", jsonString);
    if (success) {
        Serial.println("✅ Slave configuration saved successfully");
    } else {
        Serial.println("❌ Failed to save slave configuration");
    }

    modbusReloadSlaves();

    return success;
}

bool loadSlaveConfig(JsonDocument& config) {
    Serial.println("📖 Loading slave configuration from LittleFS...");
    
    if (!fileExists("/slaves.json")) {
        Serial.println("⚠️  No slave configuration found, using defaults");
        return false;
    }
    
    String jsonString = readFile("/slaves.json");
    if (jsonString.length() == 0) {
        Serial.println("❌ Empty slave configuration file");
        return false;
    }
    
    DeserializationError error = deserializeJson(config, jsonString);
    if (error) {
        Serial.printf("❌ Failed to parse slave config: %s\n", error.c_str());
        return false;
    }

    Serial.println("✅ Slave configuration loaded successfully");
    return true;
}


//Save Time Polling configuration
bool savePollingConfig(int interval, int timeoutSeconds) {
    Serial.printf("💾 Saving polling config (interval: %ds, timeout: %ds) to LittleFS...\n", interval, timeoutSeconds);
    
    JsonDocument doc;
    doc["pollInterval"] = interval;
    doc["timeout"] = timeoutSeconds;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    bool success = writeFile("/polling.json", jsonString);
    if (success) {
        Serial.println("✅ Polling config saved successfully");
    } else {
        Serial.println("❌ Failed to save polling config");
    }

    modbusReloadSlaves();
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
    
    String jsonString = readFile("/polling.json");
    if (jsonString.length() == 0) {
        Serial.println("❌ Empty polling config file, using defaults");
        interval = 10;
        timeoutSeconds = 1;
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
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