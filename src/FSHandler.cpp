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

// ✅ NEW: Save slave configuration
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

    modbus_reloadSlaves();

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

bool savePollInterval(int interval) {
    Serial.printf("💾 Saving poll interval (%d seconds) to LittleFS...\n", interval);
    
    StaticJsonDocument<128> doc;
    doc["pollInterval"] = interval;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    bool success = writeFile("/pollinterval.json", jsonString);
    if (success) {
        Serial.println("✅ Poll interval saved successfully");
    } else {
        Serial.println("❌ Failed to save poll interval");
    }

    modbus_reloadSlaves();
    return success;
}

// ✅ NEW: Load poll interval
int loadPollInterval() {
    Serial.println("📖 Loading poll interval from LittleFS...");
    
    if (!fileExists("/pollinterval.json")) {
        Serial.println("⚠️  No poll interval found, using default (10 seconds)");
        return 10;
    }
    
    String jsonString = readFile("/pollinterval.json");
    if (jsonString.length() == 0) {
        Serial.println("❌ Empty poll interval file, using default");
        return 10;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.printf("❌ Failed to parse poll interval: %s, using default\n", error.c_str());
        return 10;
    }
    
    int interval = doc["pollInterval"] | 10; // Default to 10 if not found
    Serial.printf("✅ Poll interval loaded: %d seconds\n", interval);
    return interval;
}


bool saveTimeout(int timeoutSeconds) {
    Serial.printf("💾 Saving timeout (%d seconds) to LittleFS...\n", timeoutSeconds);
    
    StaticJsonDocument<128> doc;
    doc["timeout"] = timeoutSeconds;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    bool success = writeFile("/timeout.json", jsonString);
    if (success) {
        Serial.println("✅ Timeout saved successfully");
    } else {
        Serial.println("❌ Failed to save timeout");
    }
    return success;
}

int loadTimeout() {
    Serial.println("📖 Loading timeout from LittleFS...");
    
    if (!fileExists("/timeout.json")) {
        Serial.println("⚠️  No timeout found, using default (5 seconds)");
        return 5;
    }
    
    String jsonString = readFile("/timeout.json");
    if (jsonString.length() == 0) {
        Serial.println("❌ Empty timeout file, using default");
        return 5;
    }
    
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.printf("❌ Failed to parse timeout: %s, using default\n", error.c_str());
        return 5;
    }
    
    int timeout = doc["timeout"] | 1; // Default to 1 seconds if not found
    Serial.printf("✅ Timeout loaded: %d seconds\n", timeout);
    return timeout;
}