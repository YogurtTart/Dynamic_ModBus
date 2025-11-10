#include "WebServer.h"

// ==================== CONSTANTS ====================

constexpr int kMaxDebugMessages = 30;
constexpr int kWebServerPort = 80;
constexpr int kMaxDevices = 20;

// ==================== GLOBAL VARIABLES ====================

ESP8266WebServer server(kWebServerPort);
bool debugEnabled = false;

// Enhanced dynamic timing variables
unsigned long lastSequenceTime = 0;
unsigned long systemStartTime = 0;
DeviceTiming deviceTiming[kMaxDevices];
uint8_t deviceTimeCount = 0;

String debugMessages[kMaxDebugMessages];
int debugMessageCount = 0;
int debugMessageIndex = 0;

// ==================== WEB SERVER INITIALIZATION ====================


void setupWebServer() {
    Serial.println("🌐 Initializing Web Server...");

    // Initialize device timing array
    for (int i = 0; i < kMaxDevices; i++) {
        deviceTiming[i].slaveId = 0;
        deviceTiming[i].lastSeenTime = 0;
        deviceTiming[i].lastSequenceTime = 0;
        deviceTiming[i].isFirstMessage = true;
        deviceTiming[i].messageCount = 0;
    }
    
    // Set system start time
    systemStartTime = millis();
    
    // Serve static files
    server.onNotFound(handleStaticFiles);

    // SPA endpoint
    server.on("/", []() { serveHtmlFile("/index.html"); });
    
    // WiFi endpoints
    server.on("/savewifi", HTTP_POST, handleSaveWifi);
    server.on("/getwifi", HTTP_GET, handleGetWifi);
    server.on("/getipinfo", HTTP_GET, handleGetIpInfo);
    
    // Slave endpoints
    server.on("/saveslaves", HTTP_POST, handleSaveSlaves);
    server.on("/getslaves", HTTP_GET, handleGetSlaves);
    server.on("/getslaveconfig", HTTP_POST, handleGetSlaveConfig);
    server.on("/updateslaveconfig", HTTP_POST, handleUpdateSlaveConfig);
    
    // Configuration endpoints
    server.on("/savepollingconfig", HTTP_POST, handleSavePollingConfig);
    server.on("/getpollingconfig", HTTP_GET, handleGetPollingConfig);
    
    // Statistics endpoints
    server.on("/getstatistics", HTTP_GET, handleGetStatistics);
    server.on("/removeslavestats", HTTP_POST, handleRemoveSlaveStats);
    
    // Debug endpoints
    server.on("/toggledebug", HTTP_POST, handleToggleDebug);
    server.on("/getdebugstate", HTTP_GET, handleGetDebugState);
    server.on("/getdebugmessages", HTTP_GET, handleGetDebugMessages);
    server.on("/cleartable", HTTP_POST, handleClearTable);
    
    server.begin();
    Serial.println("✅ HTTP server started on port 80");
}

// ==================== HELPER FUNCTIONS ====================

void serveHtmlFile(const String& filename) {
    if (!fileExists(filename)) {
        Serial.printf("❌ File not found: %s\n", filename.c_str());
        server.send(500, "text/plain", "File not found: " + filename);
        return;
    }
    
    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.printf("❌ Failed to open: %s\n", filename.c_str());
        server.send(500, "text/plain", "Failed to open file");
        return;
    }
    
    server.streamFile(file, "text/html");
    file.close();
    Serial.printf("✅ Streamed: %s\n", filename.c_str());
}

void sendJsonResponse(const JsonDocument& doc) {
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

bool parseJsonBody(JsonDocument& doc) {
    String body = server.arg("plain");
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        Serial.printf("❌ JSON parsing failed: %s\n", error.c_str());
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return false;
    }
    return true;
}

// ==================== WIFI CONFIGURATION HANDLERS ====================


void handleGetIpInfo() {
    Serial.println("📡 Returning IP information");
    
    JsonDocument doc;
    
    // STA IP info
    if (WiFi.status() == WL_CONNECTED) {
        doc["sta_ip"] = WiFi.localIP().toString();
        doc["sta_subnet"] = WiFi.subnetMask().toString();
        doc["sta_gateway"] = WiFi.gatewayIP().toString();
        doc["sta_connected"] = true;
    } else {
        doc["sta_ip"] = "Not connected";
        doc["sta_subnet"] = "N/A";
        doc["sta_gateway"] = "N/A";
        doc["sta_connected"] = false;
    }
    
    // AP IP info
    doc["ap_ip"] = WiFi.softAPIP().toString();
    doc["ap_connected_clients"] = WiFi.softAPgetStationNum();
    
    sendJsonResponse(doc);
    Serial.printf("✅ Sent IP info - STA: %s, AP: %s\n", 
                  doc["sta_ip"].as<const char*>(), 
                  doc["ap_ip"].as<const char*>());
}

void handleSaveWifi() {
    Serial.println("💾 Saving WiFi settings");
    
    String staSsid = server.arg("sta_ssid");
    String staPassword = server.arg("sta_password");
    String apSsid = server.arg("ap_ssid");
    String apPassword = server.arg("ap_password");
    String mqttServer = server.arg("mqtt_server");
    String mqttPort = server.arg("mqtt_port");
    
    WifiParams newParams;
    memset(&newParams, 0, sizeof(newParams));
    
    strncpy(newParams.STAWifiID, staSsid.c_str(), sizeof(newParams.STAWifiID) - 1);
    strncpy(newParams.STApassword, staPassword.c_str(), sizeof(newParams.STApassword) - 1);
    strncpy(newParams.APWifiID, apSsid.c_str(), sizeof(newParams.APWifiID) - 1);
    strncpy(newParams.APpassword, apPassword.c_str(), sizeof(newParams.APpassword) - 1);
    strncpy(newParams.mqttServer, mqttServer.c_str(), sizeof(newParams.mqttServer) - 1);
    strncpy(newParams.mqttPort, mqttPort.c_str(), sizeof(newParams.mqttPort) - 1);
    
    // Ensure null termination
    newParams.STAWifiID[sizeof(newParams.STAWifiID) - 1] = '\0';
    newParams.STApassword[sizeof(newParams.STApassword) - 1] = '\0';
    newParams.APWifiID[sizeof(newParams.APWifiID) - 1] = '\0';
    newParams.APpassword[sizeof(newParams.APpassword) - 1] = '\0';
    newParams.mqttServer[sizeof(newParams.mqttServer) - 1] = '\0';
    newParams.mqttPort[sizeof(newParams.mqttPort) - 1] = '\0';
    
    saveWifi(newParams);
    server.send(200, "application/json", "{\"status\":\"success\"}");
    Serial.println("✅ WiFi settings saved");
}

void handleGetWifi() {
    Serial.println("📡 Returning WiFi settings");
    
    JsonDocument doc;
    
    doc["sta_ssid"] = currentParams.STAWifiID;
    doc["sta_password"] = currentParams.STApassword;
    doc["ap_ssid"] = currentParams.APWifiID;
    doc["ap_password"] = currentParams.APpassword;
    doc["mqtt_server"] = currentParams.mqttServer;
    doc["mqtt_port"] = currentParams.mqttPort;
    
    sendJsonResponse(doc);
}

// ==================== STATIC FILE HANDLING ====================

void handleStaticFiles() {
    String path = server.uri();
    Serial.printf("📁 Static file request: %s\n", path.c_str());
    
    if (path.endsWith("/")) {
        path += "index.html";
        Serial.printf("🔀 Redirected to: %s\n", path.c_str());
    }
    
    String contentType = getContentType(path);
    Serial.printf("📄 Content type: %s\n", contentType.c_str());
    
    if (!fileExists(path)) {
        Serial.printf("❌ File not found: %s\n", path.c_str());
        server.send(404, "text/plain", "File not found: " + path);
        return;
    }
    
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("❌ Failed to open: %s\n", path.c_str());
        server.send(500, "text/plain", "Failed to open file");
        return;
    }
    
    server.streamFile(file, contentType);
    file.close();
    Serial.printf("✅ Streamed file: %s\n", path.c_str());
}

String getContentType(const String& filename) {
    if (filename.endsWith(".html")) return "text/html";
    if (filename.endsWith(".css")) return "text/css";
    if (filename.endsWith(".js")) return "application/javascript";
    if (filename.endsWith(".png")) return "image/png";
    if (filename.endsWith(".jpg")) return "image/jpeg";
    if (filename.endsWith(".gif")) return "image/gif";
    if (filename.endsWith(".ico")) return "image/x-icon";
    if (filename.endsWith(".json")) return "application/json";
    return "text/plain";
}

// ==================== SLAVE CONFIGURATION HANDLERS ====================

void handleSaveSlaves() {
    Serial.println("💾 Saving slave configuration");
    
    JsonDocument newDoc;
    if (!parseJsonBody(newDoc)) return;
    
    Serial.printf("📥 Received slave config: %d bytes\n", server.arg("plain").length());
    
    // 🆕 FIX: Load existing config first to preserve overrides
    JsonDocument existingDoc;
    if (loadSlaveConfig(existingDoc)) {
        JsonArray existingSlaves = existingDoc["slaves"];
        JsonArray newSlaves = newDoc["slaves"];
        
        // 🆕 Merge overrides from existing slaves into new slaves
        for (int i = 0; i < newSlaves.size(); i++) {
            JsonObject newSlave = newSlaves[i];
            uint8_t newId = newSlave["id"];
            const char* newName = newSlave["name"];
            
            // Find matching slave in existing config
            for (JsonObject existingSlave : existingSlaves) {
                if (existingSlave["id"] == newId && 
                    strcmp(existingSlave["name"], newName) == 0) {
                    
                    // 🆕 Preserve override if it exists
                    if (existingSlave["override"].is<JsonObject>()) {
                        newSlave["override"] = existingSlave["override"];
                        Serial.printf("✅ Preserved overrides for slave %d: %s\n", newId, newName);
                    }
                    break;
                }
            }
        }
    }
    
    // Now save the merged config
    if (saveSlaveConfig(newDoc)) {
        modbusReloadSlaves();
        server.send(200, "application/json", "{\"status\":\"success\"}");
        Serial.println("✅ Slave configuration saved successfully with preserved overrides");
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
        Serial.println("❌ Failed to save slave configuration");
    }
}

void handleGetSlaves() {
    Serial.println("📡 Returning slave configuration");
    
    JsonDocument doc;
    if (loadSlaveConfig(doc)) {
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
        Serial.printf("✅ Sent slave configuration (%d bytes)\n", response.length());
    } else {
        server.send(200, "application/json", "{\"slaves\":[]}");
        Serial.println("✅ Sent empty slave configuration");
    }
}

void handleGetSlaveConfig() {
    Serial.println("🔍 Getting specific slave with template merge");
    
    // 1. Get the request data (which slave to load)
    String body = server.arg("plain");
    if (body.length() == 0) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty request body\"}");
        return;
    }
    
    // 2. Parse the JSON request
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // 3. Extract which slave we're looking for
    uint8_t slaveId = doc["slaveId"];
    const char* slaveName = doc["slaveName"];
    Serial.printf("🔎 Loading slave ID: %d, Name: %s\n", slaveId, slaveName);
    
    // 4. Load ALL slaves from file
    JsonDocument configDoc;
    if (!loadSlaveConfig(configDoc)) {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"No slave configuration found\"}");
        return;
    }
    
    // 5. Find the specific slave we want
    JsonArray slavesArray = configDoc["slaves"];
    bool slaveFound = false;
    JsonObject foundSlave;
    
    for (JsonObject slave : slavesArray) {
        if (slave["id"] == slaveId && strcmp(slave["name"], slaveName) == 0) {
            foundSlave = slave;
            slaveFound = true;
            break;
        }
    }
    
    if (!slaveFound) {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Slave not found\"}");
        return;
    }
    
    // 6. 🆕 TEMPLATE SYSTEM: Load the device template
    JsonDocument templateDoc;
    JsonObject templateConfig = templateDoc.to<JsonObject>();
    JsonDocument mergedDoc;
    JsonObject mergedConfig = mergedDoc.to<JsonObject>();
    
    const char* deviceType = foundSlave["deviceType"];
    if (loadDeviceTemplate(deviceType, templateConfig)) {
        // 7. 🆕 Merge template defaults + slave overrides
        mergeWithOverride(foundSlave, templateConfig, mergedConfig);
        
        // 8. Add the basic slave info to the merged config
        mergedConfig["id"] = foundSlave["id"];
        mergedConfig["name"] = foundSlave["name"];
        mergedConfig["deviceType"] = foundSlave["deviceType"];
        mergedConfig["startReg"] = foundSlave["startReg"];
        mergedConfig["numReg"] = foundSlave["numReg"];
        mergedConfig["mqttTopic"] = foundSlave["mqttTopic"];
        mergedConfig["registerSize"] = foundSlave["registerSize"];
        
        // 9. Send the COMPLETE config to UI
        String response;
        serializeJson(mergedConfig, response);
        server.send(200, "application/json", response);
        Serial.printf("✅ Sent merged config for slave %d: %s (template: %s)\n", slaveId, slaveName, deviceType);
    } else {
        // Fallback if no template found
        String response;
        serializeJson(foundSlave, response);
        server.send(200, "application/json", response);
        Serial.printf("⚠️  No template found for slave %d, sent raw config\n", slaveId);
    }
}

void handleUpdateSlaveConfig() {
    Serial.println("💾 Updating specific slave with template system");
    
    String body = server.arg("plain");
    if (body.length() == 0) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty request body\"}");
        return;
    }
    
    JsonDocument updateDoc;
    DeserializationError error = deserializeJson(updateDoc, body);
    
    if (error) {
        Serial.printf("❌ JSON parse error: %s\n", error.c_str());
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // 🆕 DEBUG: Print what we received
    Serial.printf("📥 Received update: %s\n", body.c_str());
    
    // 🆕 FIX: Check for basic required fields
    if (!updateDoc["id"].is<int>() || !updateDoc["name"].is<const char*>()) {
        Serial.println("❌ Missing id or name fields");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing required fields: id or name\"}");
        return;
    }
    
    uint8_t slaveId = updateDoc["id"];
    const char* slaveName = updateDoc["name"];
    
    // 🆕 FIX: Get deviceType from slave config, not from updateDoc
    JsonDocument configDoc;
    if (!loadSlaveConfig(configDoc)) {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"No slave configuration found\"}");
        return;
    }
    
    JsonArray slavesArray = configDoc["slaves"];
    bool slaveFound = false;
    int slaveIndex = -1;
    const char* deviceType = nullptr;
    
    for (size_t i = 0; i < slavesArray.size(); i++) {
        JsonObject slave = slavesArray[i];
        if (slave["id"] == slaveId && strcmp(slave["name"], slaveName) == 0) {
            slaveIndex = i;
            slaveFound = true;
            deviceType = slave["deviceType"]; // 🆕 Get deviceType from existing slave
            break;
        }
    }
    
    if (!slaveFound || deviceType == nullptr) {
        Serial.printf("❌ Slave not found or no deviceType: ID=%d, Name=%s\n", slaveId, slaveName);
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Slave not found or no deviceType\"}");
        return;
    }
    
    Serial.printf("🔄 Updating slave ID: %d, Name: %s, Type: %s\n", slaveId, slaveName, deviceType);
    
    // Load template for this device type
    JsonDocument templateDoc;
    JsonObject templateConfig = templateDoc.to<JsonObject>();
    
    if (!loadDeviceTemplate(deviceType, templateConfig)) {
        Serial.printf("❌ Template not found for: %s\n", deviceType);
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Template not found for device type\"}");
        return;
    }
    
    // 🆕 FIX: Create a copy with ONLY parameter fields (no basic slave info)
    JsonDocument paramsOnlyDoc;
    JsonObject paramsOnly = paramsOnlyDoc.to<JsonObject>();
    
    // Copy only parameter fields (exclude basic slave info)
    for (JsonPair kv : updateDoc.as<JsonObject>()) {
        const char* key = kv.key().c_str();
        // 🆕 Only include fields that are NOT basic slave info
        if (strcmp(key, "id") != 0 && 
            strcmp(key, "name") != 0 &&
            strcmp(key, "deviceType") != 0 &&
            strcmp(key, "startReg") != 0 &&
            strcmp(key, "numReg") != 0 &&
            strcmp(key, "mqttTopic") != 0 &&
            strcmp(key, "registerSize") != 0) {
            paramsOnly[key].set(kv.value());
        }
    }
    
    Serial.printf("📊 Comparing %d parameter fields against template\n", paramsOnly.size());
    
    // Detect overrides by comparing submitted PARAMETERS vs template
    JsonDocument overrideDoc;
    JsonObject overrideOutput = overrideDoc.to<JsonObject>();
    detectOverrides(paramsOnly, templateConfig, overrideOutput);
    
    Serial.printf("📊 Detected %d parameter overrides\n", overrideOutput.size());
    
    // Update the slave - keep only basic info + override
    slavesArray[slaveIndex].clear();
    
    // 🆕 Store basic fields DIRECTLY (not in override)
    slavesArray[slaveIndex]["id"] = slaveId;
    slavesArray[slaveIndex]["name"] = slaveName;
    slavesArray[slaveIndex]["deviceType"] = deviceType;
    slavesArray[slaveIndex]["startReg"] = updateDoc["startReg"];  // 🆕 Updated if changed
    slavesArray[slaveIndex]["numReg"] = updateDoc["numReg"];      // 🆕 Updated if changed
    slavesArray[slaveIndex]["mqttTopic"] = updateDoc["mqttTopic"]; // 🆕 Updated if changed
    slavesArray[slaveIndex]["registerSize"] = updateDoc["registerSize"]; // 🆕 Updated if changed
    
    // Only add override if there are any parameter overrides
    if (overrideOutput.size() > 0) {
        slavesArray[slaveIndex]["override"] = overrideOutput;
        Serial.printf("💾 Storing parameter overrides: %d parameters\n", overrideOutput.size());
    } else {
        Serial.println("💾 No parameter overrides to store");
    }

    // Now save the  config
    if (saveSlaveConfig(configDoc)) {
        modbusReloadSlaves();
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Slave configuration updated successfully\"}");
        Serial.println("✅ Slave configuration saved successfully with preserved overrides");
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
        Serial.println("❌ Failed to save slave configuration");
    }
}

// ==================== POLLING CONFIGURATION HANDLERS ====================

void handleSavePollingConfig() {
    Serial.println("💾 Saving polling configuration");
    
    JsonDocument doc;
    if (!parseJsonBody(doc)) return;
    
    int interval = doc["pollInterval"] | 10;
    int timeout = doc["timeout"] | 1;
    
    if (savePollingConfig(interval, timeout)) {
        server.send(200, "application/json", "{\"status\":\"success\"}");
        Serial.printf("✅ Polling config saved: interval=%ds, timeout=%ds\n", interval, timeout);
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
        Serial.println("❌ Failed to save polling config");
    }
}

void handleGetPollingConfig() {
    Serial.println("📡 Returning polling configuration");
    
    int interval, timeout;
    loadPollingConfig(interval, timeout);
    
    JsonDocument doc;
    doc["pollInterval"] = interval;
    doc["timeout"] = timeout;
    
    sendJsonResponse(doc);
    Serial.printf("✅ Sent polling config: interval=%ds, timeout=%ds\n", interval, timeout);
}

// ==================== STATISTICS HANDLERS ====================

void handleGetStatistics() {
    Serial.println("📊 Returning query statistics");
    
    String statsJson = getStatisticsJson();
    server.send(200, "application/json", statsJson);
    Serial.printf("✅ Sent statistics (%d bytes)\n", statsJson.length());
}

void handleRemoveSlaveStats() {
    Serial.println("🗑️ Removing slave statistics");
    
    JsonDocument doc;
    if (!parseJsonBody(doc)) return;
    
    uint8_t slaveId = doc["slaveId"];
    const char* slaveName = doc["slaveName"];
    
    removeSlaveStatistic(slaveId, slaveName);
    server.send(200, "application/json", "{\"status\":\"success\"}");
    Serial.printf("✅ Removed statistics for slave %d: %s\n", slaveId, slaveName);
}

// ==================== DEBUG MANAGEMENT HANDLERS ====================

void handleToggleDebug() {
    JsonDocument doc;
    if (!parseJsonBody(doc)) return;
    
    debugEnabled = doc["enabled"] | false;
    server.send(200, "application/json", "{\"status\":\"success\"}");
    
    Serial.printf("🔧 Debug mode %s\n", debugEnabled ? "ENABLED" : "DISABLED");
}

void handleGetDebugState() {
    JsonDocument doc;
    doc["enabled"] = debugEnabled;
    sendJsonResponse(doc);
}

void handleGetDebugMessages() {
    String response = "[";
    for (int i = 0; i < debugMessageCount; i++) {
        response += debugMessages[i];
        if (i < debugMessageCount - 1) response += ",";
    }
    response += "]";
    
    // Clear messages after reading
    debugMessageCount = 0;
    debugMessageIndex = 0;
    
    server.send(200, "application/json", response);
}

void addDebugMessage(const char* topic, const char* message, const char* timeDelta, const char* sameDeviceDelta) {
    if (!debugEnabled) return;
    
    JsonDocument doc;
    doc["topic"] = topic;
    doc["message"] = message;
    doc["timestamp"] = millis();
    doc["timeDelta"] = timeDelta;
    doc["sameDeviceDelta"] = sameDeviceDelta;
    doc["realTime"] = getCurrentTimeString();
    
    String jsonMessage;
    serializeJson(doc, jsonMessage);
    
    // Store in circular buffer
    debugMessages[debugMessageIndex] = jsonMessage;
    debugMessageIndex = (debugMessageIndex + 1) % kMaxDebugMessages;
    
    if (debugMessageCount < kMaxDebugMessages) {
        debugMessageCount++;
    }
    
    Serial.printf("📢 DEBUG [%s]: %s (Δ%s, sameΔ%s)\n", topic, message, timeDelta, sameDeviceDelta);
}

void handleClearTable() {
    Serial.println("🗑️ Clearing table and resetting timing data");
    resetAllTiming();
    
    // Also clear debug messages
    debugMessageCount = 0;
    debugMessageIndex = 0;
    
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Table cleared and timing reset\"}");
    Serial.println("✅ Table cleared and all timing data reset");
}

// ==================== ENHANCED TIMING FUNCTIONS ====================

unsigned long calculateTimeDelta(uint8_t slaveId, const char* slaveName) {
    unsigned long currentTime = millis();
    unsigned long delta = 0;
    
    if (lastSequenceTime > 0) {
        delta = currentTime - lastSequenceTime;
    }
    
    lastSequenceTime = currentTime;
    updateDeviceTiming(slaveId, slaveName, currentTime);
    
    return delta;
}

String formatTimeDelta(unsigned long deltaMs) {
    if (deltaMs == 0) return "+0ms";
    if (deltaMs < 1000) return "+" + String(deltaMs) + "ms";
    return "+" + String(deltaMs / 1000.0, 1) + "s";
}

String getCurrentTimeString() {
    // Calculate time since system start (real time)
    unsigned long elapsedMs = millis() - systemStartTime;
    unsigned long seconds = elapsedMs / 1000;
    unsigned long hours = (seconds % 86400) / 3600;
    unsigned long minutes = (seconds % 3600) / 60;
    unsigned long secs = seconds % 60;
    
    char timeBuffer[9];
    snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu", hours, minutes, secs);
    return String(timeBuffer);
}

String calculateSameDeviceDelta(uint8_t slaveId, const char* slaveName) {
    return getSameDeviceDelta(slaveId, slaveName, false);
}

String getSameDeviceDelta(uint8_t slaveId, const char* slaveName, bool resetTimer) {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < deviceTimeCount; i++) {
        if (deviceTiming[i].slaveId == slaveId && 
            strcmp(deviceTiming[i].slaveName, slaveName) == 0) {
            
            if (deviceTiming[i].isFirstMessage) {
                if (resetTimer) {
                    deviceTiming[i].isFirstMessage = false;
                    deviceTiming[i].lastSeenTime = currentTime;
                }
                return "First";
            }
            
            unsigned long delta = currentTime - deviceTiming[i].lastSeenTime;
            if (resetTimer) {
                deviceTiming[i].lastSeenTime = currentTime;
            }
            return formatTimeDelta(delta);
        }
    }
    
    // Device not found - initialize
    if (deviceTimeCount < 20) {
        deviceTiming[deviceTimeCount].slaveId = slaveId;
        strncpy(deviceTiming[deviceTimeCount].slaveName, slaveName, 31);
        deviceTiming[deviceTimeCount].slaveName[31] = '\0';
        deviceTiming[deviceTimeCount].lastSeenTime = currentTime;
        deviceTiming[deviceTimeCount].lastSequenceTime = currentTime;
        deviceTiming[deviceTimeCount].isFirstMessage = true;
        deviceTiming[deviceTimeCount].messageCount = 1;
        deviceTimeCount++;
        return "First";
    }
    
    return "+0ms";
}

void updateDeviceTiming(uint8_t slaveId, const char* slaveName, unsigned long currentTime) {
    int deviceIndex = -1;
    
    // Find existing device
    for (int i = 0; i < deviceTimeCount; i++) {
        if (deviceTiming[i].slaveId == slaveId && 
            strcmp(deviceTiming[i].slaveName, slaveName) == 0) {
            deviceIndex = i;
            break;
        }
    }
    
    if (deviceIndex == -1 && deviceTimeCount < 20) {
        // Create new device entry
        deviceIndex = deviceTimeCount;
        deviceTiming[deviceIndex].slaveId = slaveId;
        strncpy(deviceTiming[deviceIndex].slaveName, slaveName, 31);
        deviceTiming[deviceIndex].slaveName[31] = '\0';
        deviceTiming[deviceIndex].lastSeenTime = currentTime;
        deviceTiming[deviceIndex].lastSequenceTime = currentTime;
        deviceTiming[deviceIndex].isFirstMessage = true;
        deviceTiming[deviceIndex].messageCount = 0;
        deviceTimeCount++;
    }
    
    if (deviceIndex != -1) {
        deviceTiming[deviceIndex].lastSequenceTime = currentTime;
        deviceTiming[deviceIndex].messageCount++;
    }
}

void resetAllTiming() {
    // Reset all timing variables
    for (int i = 0; i < kMaxDevices; i++) {
        deviceTiming[i].slaveId = 0;
        deviceTiming[i].lastSeenTime = 0;
        deviceTiming[i].lastSequenceTime = 0;
        deviceTiming[i].isFirstMessage = true;
        deviceTiming[i].messageCount = 0;
    }
    
    deviceTimeCount = 0;
    lastSequenceTime = 0;
    systemStartTime = millis(); // Reset real time counter
    
    Serial.println("✅ All timing data reset - Real Time, Since Prev, and Since Same cleared");
}