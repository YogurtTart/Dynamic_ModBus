#include "WebServer.h"

// Constants
constexpr int kMaxDebugMessages = 30;
constexpr int kWebServerPort = 80;

// Global Variables
ESP8266WebServer server(kWebServerPort);
bool debugEnabled = false;

String debugMessages[kMaxDebugMessages];
int debugMessageCount = 0;
int debugMessageIndex = 0;

void setupWebServer() {
    Serial.println("🌐 Initializing Web Server...");
    
    // Serve static files
    server.onNotFound(handleStaticFiles);
    
    // API endpoints - organized by functionality
    server.on("/", HTTP_GET, handleRoot);
    server.on("/slaves.html", HTTP_GET, handleSlavesPage);
    
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
    server.on("/savepollinterval", HTTP_POST, handleSavePollInterval);
    server.on("/getpollinterval", HTTP_GET, handleGetPollInterval);
    server.on("/savetimeout", HTTP_POST, handleSaveTimeout);
    server.on("/gettimeout", HTTP_GET, handleGetTimeout);
    
    // Statistics endpoints
    server.on("/getstatistics", HTTP_GET, handleGetStatistics);
    server.on("/removeslavestats", HTTP_POST, handleRemoveSlaveStats);
    
    // Debug endpoints
    server.on("/toggledebug", HTTP_POST, handleToggleDebug);
    server.on("/getdebugstate", HTTP_GET, handleGetDebugState);
    server.on("/getdebugmessages", HTTP_GET, handleGetDebugMessages);
    
    server.begin();
    Serial.println("✅ HTTP server started on port 80");
}

// Helper Functions
void serveHtmlFile(const String& filename) {
    if (!fileExists(filename)) {
        Serial.printf("❌ File not found: %s\n", filename.c_str());
        server.send(500, "text/plain", "File not found: " + filename);
        return;
    }
    
    String html = readFile(filename);
    server.send(200, "text/html", html);
    Serial.printf("✅ Served: %s (%d bytes)\n", filename.c_str(), html.length());
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

// Request Handlers
void handleRoot() {
    Serial.println("🌐 Handling root request");
    serveHtmlFile("/index.html");
}

void handleSlavesPage() {
    Serial.println("🌐 Handling slaves page request");
    serveHtmlFile("/slaves.html");
}

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
    
    WifiParams newParams;
    memset(&newParams, 0, sizeof(newParams));
    
    strncpy(newParams.STAWifiID, staSsid.c_str(), sizeof(newParams.STAWifiID) - 1);
    strncpy(newParams.STApassword, staPassword.c_str(), sizeof(newParams.STApassword) - 1);
    strncpy(newParams.APWifiID, apSsid.c_str(), sizeof(newParams.APWifiID) - 1);
    strncpy(newParams.APpassword, apPassword.c_str(), sizeof(newParams.APpassword) - 1);
    strncpy(newParams.mqttServer, mqttServer.c_str(), sizeof(newParams.mqttServer) - 1);
    
    // Ensure null termination
    newParams.STAWifiID[sizeof(newParams.STAWifiID) - 1] = '\0';
    newParams.STApassword[sizeof(newParams.STApassword) - 1] = '\0';
    newParams.APWifiID[sizeof(newParams.APWifiID) - 1] = '\0';
    newParams.APpassword[sizeof(newParams.APpassword) - 1] = '\0';
    newParams.mqttServer[sizeof(newParams.mqttServer) - 1] = '\0';
    
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
    
    sendJsonResponse(doc);
}

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
    
    String content = readFile(path);
    server.send(200, contentType, content);
    Serial.printf("✅ Served file: %s (%d bytes)\n", path.c_str(), content.length());
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

void handleSaveSlaves() {
    Serial.println("💾 Saving slave configuration");
    
    JsonDocument doc;
    if (!parseJsonBody(doc)) return;
    
    Serial.printf("📥 Received slave config: %d bytes\n", server.arg("plain").length());
    
    if (saveSlaveConfig(doc)) {
        server.send(200, "application/json", "{\"status\":\"success\"}");
        Serial.println("✅ Slave configuration saved successfully");
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

void handleSavePollInterval() {
    Serial.println("💾 Saving poll interval");
    
    JsonDocument doc;
    if (!parseJsonBody(doc)) return;
    
    int interval = doc["pollInterval"] | 10;
    
    if (savePollInterval(interval)) {
        server.send(200, "application/json", "{\"status\":\"success\"}");
        Serial.printf("✅ Poll interval saved: %d seconds\n", interval);
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
        Serial.println("❌ Failed to save poll interval");
    }
}

void handleGetPollInterval() {
    Serial.println("📡 Returning poll interval");
    
    int interval = loadPollInterval();
    
    JsonDocument doc;
    doc["pollInterval"] = interval;
    
    sendJsonResponse(doc);
    Serial.printf("✅ Sent poll interval: %d seconds\n", interval);
}

void handleSaveTimeout() {
    Serial.println("💾 Saving timeout");
    
    JsonDocument doc;
    if (!parseJsonBody(doc)) return;
    
    int timeout = doc["timeout"] | 1;
    
    if (saveTimeout(timeout)) {
        server.send(200, "application/json", "{\"status\":\"success\"}");
        Serial.printf("✅ Timeout saved: %d seconds\n", timeout);
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
        Serial.println("❌ Failed to save timeout");
    }
}

void handleGetTimeout() {
    Serial.println("📡 Returning timeout");
    
    int timeout = loadTimeout();
    
    JsonDocument doc;
    doc["timeout"] = timeout;
    
    sendJsonResponse(doc);
    Serial.printf("✅ Sent timeout: %d seconds\n", timeout);
}

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

void handleGetSlaveConfig() {
    Serial.println("🔍 Searching for specific slave");
    
    String body = server.arg("plain");
    if (body.length() == 0) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty request body\"}");
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    uint8_t slaveId = doc["slaveId"];
    const char* slaveName = doc["slaveName"];
    
    Serial.printf("🔎 Searching for slave ID: %d, Name: %s\n", slaveId, slaveName);
    
    JsonDocument configDoc;
    if (!loadSlaveConfig(configDoc)) {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"No slave configuration found\"}");
        return;
    }
    
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
    
    String response;
    serializeJson(foundSlave, response);
    server.send(200, "application/json", response);
    Serial.printf("✅ Found slave configuration for ID %d: %s\n", slaveId, slaveName);
}

void handleUpdateSlaveConfig() {
    Serial.println("💾 Updating specific slave");
    
    String body = server.arg("plain");
    if (body.length() == 0) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty request body\"}");
        return;
    }
    
    JsonDocument updateDoc;
    DeserializationError error = deserializeJson(updateDoc, body);
    
    if (error) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    if (!updateDoc["id"].is<int>() || !updateDoc["name"].is<const char*>()) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing required fields: id and name\"}");
        return;
    }
    
    uint8_t slaveId = updateDoc["id"];
    const char* slaveName = updateDoc["name"];
    
    Serial.printf("🔄 Updating slave ID: %d, Name: %s\n", slaveId, slaveName);
    
    JsonDocument configDoc;
    if (!loadSlaveConfig(configDoc)) {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"No slave configuration found\"}");
        return;
    }
    
    JsonArray slavesArray = configDoc["slaves"];
    bool slaveFound = false;
    int slaveIndex = -1;
    
    for (int i = 0; i < slavesArray.size(); i++) {
        JsonObject slave = slavesArray[i];
        if (slave["id"] == slaveId && strcmp(slave["name"], slaveName) == 0) {
            slaveIndex = i;
            slaveFound = true;
            break;
        }
    }
    
    if (!slaveFound) {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Slave not found\"}");
        return;
    }
    
    // Replace the slave object with the updated one
    slavesArray[slaveIndex].clear();
    for (JsonPair kv : updateDoc.as<JsonObject>()) {
        slavesArray[slaveIndex][kv.key()] = kv.value();
    }
    
    if (saveSlaveConfig(configDoc)) {
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Slave configuration updated successfully\"}");
        Serial.printf("✅ Successfully updated slave ID %d: %s\n", slaveId, slaveName);
        modbusReloadSlaves();
    } else {
        server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save configuration\"}");
        Serial.println("❌ Failed to save updated slave configuration");
    }
}

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

void addDebugMessage(const char* topic, const char* message) {
    if (!debugEnabled) return;
    
    JsonDocument doc;
    doc["topic"] = topic;
    doc["message"] = message;
    doc["timestamp"] = millis();
    
    String jsonMessage;
    serializeJson(doc, jsonMessage);
    
    // Store in circular buffer
    debugMessages[debugMessageIndex] = jsonMessage;
    debugMessageIndex = (debugMessageIndex + 1) % kMaxDebugMessages;
    
    if (debugMessageCount < kMaxDebugMessages) {
        debugMessageCount++;
    }
    
    Serial.printf("📢 DEBUG [%s]: %s\n", topic, message);
}