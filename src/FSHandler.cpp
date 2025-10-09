#include "FSHandler.h"

bool initFileSystem() {
    if (!LittleFS.begin()) {
        Serial.println("ERROR: Failed to mount LittleFS");
        return false;
    }
    Serial.println("LittleFS mounted successfully");
    
    // List files for debugging
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
        Serial.printf("File: %s, Size: %d\n", dir.fileName().c_str(), dir.fileSize());
    }
    
    return true;
}

bool fileExists(const String& path) {
    return LittleFS.exists(path);
}

String readFile(const String& path) {
    File file = LittleFS.open(path, "r");
    if (!file) {
        return String();
    }
    String content = file.readString();
    file.close();
    return content;
}

bool writeFile(const String& path, const String& content) {
    File file = LittleFS.open(path, "w");
    if (!file) {
        return false;
    }
    size_t bytesWritten = file.print(content);
    file.close();
    return (bytesWritten > 0);
}