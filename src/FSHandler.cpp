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