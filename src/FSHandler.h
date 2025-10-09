#pragma once
#include <LittleFS.h>

bool initFileSystem();
bool fileExists(const String& path);
String readFile(const String& path);
bool writeFile(const String& path, const String& content);