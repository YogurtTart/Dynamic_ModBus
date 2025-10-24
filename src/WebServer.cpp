#include "WebServer.h"
#include "EEEProm.h"
#include "FSHandler.h" 
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

ESP8266WebServer server(80);
bool debugEnabled = false;

const int MAX_DEBUG_MESSAGES = 30;
String debugMessages[MAX_DEBUG_MESSAGES];
int debugMessageCount = 0;
int debugMessageIndex = 0;

void setupWebServer() {
    Serial.println("🌐 Initializing Web Server on port 80...");
    
    // Serve static files
    server.onNotFound(handleStaticFiles);
    
    // API endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/savewifi", HTTP_POST, handleSaveWifi);
    server.on("/getwifi", HTTP_GET, handleGetWifi);
    server.on("/getipinfo", HTTP_GET, handleGetIPInfo);
    server.on("/slaves.html", HTTP_GET, handleSlavesPage);
    server.on("/saveslaves", HTTP_POST, handleSaveSlaves);
    server.on("/getslaves", HTTP_GET, handleGetSlaves);
    server.on("/savepollinterval", HTTP_POST, handleSavePollInterval);
    server.on("/getpollinterval", HTTP_GET, handleGetPollInterval);
    server.on("/savetimeout", HTTP_POST, handleSaveTimeout);    
    server.on("/gettimeout", HTTP_GET, handleGetTimeout);      
    server.on("/getstatistics", HTTP_GET, handleGetStatistics);   
    server.on("/removeslavestats", HTTP_POST, handleRemoveSlaveStats);
    server.on("/getslaveconfig", HTTP_POST, handleGetSlaveConfig);
    server.on("/updateslaveconfig", HTTP_POST, handleUpdateSlaveConfig);
    server.on("/toggledebug", HTTP_POST, handleToggleDebug);
    server.on("/getdebugstate", HTTP_GET, handleGetDebugState);
    server.on("/getdebugmessages", HTTP_GET, handleGetDebugMessages);
    
    server.begin();
    Serial.println("✅ HTTP server started successfully");
    Serial.println("📍 Available endpoints:");
    Serial.println("   GET  /         - Configuration page");
    Serial.println("   GET  /getwifi  - Get current WiFi settings");
    Serial.println("   POST /savewifi - Save new WiFi settings");
    Serial.println("   GET  /getslaves - Get slave configuration");     
    Serial.println("   POST /saveslaves - Save slave configuration");  
    Serial.println("   GET  /getpollinterval - Get poll interval");     
    Serial.println("   POST /savepollinterval - Save poll interval");    
    Serial.println("   GET  /gettimeout - Get current timeout");         
    Serial.println("   POST /savetimeout - Save new timeout");  
    Serial.println("   GET  /getstatistics - Get query statistics");          
}

void handleRoot() {
    Serial.println("🌐 Handling root request (/)");
    if (!fileExists("/index.html")) {
        Serial.println("❌ index.html not found in LittleFS");
        server.send(500, "text/plain", "Index file not found in LittleFS");
        return;
    }
    
    String html = readFile("/index.html");
    server.send(200, "text/html", html);
    Serial.println("✅ Served index.html");
}


void handleSlavesPage() {
    Serial.println("🌐 Handling slaves page request (/slaves.html)");
    if (!fileExists("/slaves.html")) {
        Serial.println("❌ slaves.html not found in LittleFS");
        server.send(500, "text/plain", "Slaves page not found in LittleFS");
        return;
    }
    
    String html = readFile("/slaves.html");
    server.send(200, "text/html", html);
    Serial.println("✅ Served slaves.html");
}

void handleSaveSlaves() {
    Serial.println("💾 POST /saveslaves - Saving slave configuration");
    
    String body = server.arg("plain");
    Serial.printf("📥 Received slave config: %s\n", body.c_str());
    
    // Parse the incoming JSON
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        Serial.printf("❌ JSON parsing failed: %s\n", error.c_str());
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // Save using centralized FSHandler
    if (saveSlaveConfig(doc)) {
        server.send(200, "application/json", "{\"status\":\"success\"}");
        Serial.println("✅ Slave configuration saved successfully");
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
        Serial.println("❌ Failed to save slave configuration");
    }
}

void handleGetSlaves() {
    Serial.println("📡 GET /getslaves - Returning slave configuration");
    
    StaticJsonDocument<2048> doc;
    
    if (loadSlaveConfig(doc)) {
        String response;
        serializeJson(doc, response);
        server.send(200, "application/json", response);
        Serial.printf("✅ Sent slave configuration (%d bytes)\n", response.length());
    } else {
        // Return empty config
        server.send(200, "application/json", "{\"slaves\":[]}");
        Serial.println("✅ Sent empty slave configuration");
    }
}

void handleSavePollInterval() {
    Serial.println("💾 POST /savepollinterval - Saving poll interval");
    
    String body = server.arg("plain");
    Serial.printf("📥 Received poll interval: %s\n", body.c_str());
    
    // Parse the incoming JSON
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        Serial.printf("❌ JSON parsing failed: %s\n", error.c_str());
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    int interval = doc["pollInterval"] | 10;
    
    // Save using centralized FSHandler
    if (savePollInterval(interval)) {
        server.send(200, "application/json", "{\"status\":\"success\"}");
        Serial.printf("✅ Poll interval saved: %d seconds\n", interval);
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
        Serial.println("❌ Failed to save poll interval");
    }
}

void handleGetPollInterval() {
    Serial.println("📡 GET /getpollinterval - Returning poll interval");
    
    // Load using centralized FSHandler
    int interval = loadPollInterval();
    
    StaticJsonDocument<128> doc;
    doc["pollInterval"] = interval;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
    Serial.printf("✅ Sent poll interval: %d seconds\n", interval);
}

void handleGetWifi() {
    Serial.println("📡 GET /getwifi - Returning current WiFi settings");
    StaticJsonDocument<512> doc;
    doc["sta_ssid"] = currentParams.STAWifiID;
    doc["sta_password"] = currentParams.STApassword;
    doc["ap_ssid"] = currentParams.APWifiID;
    doc["ap_password"] = currentParams.APpassword;
    doc["mqtt_server"] = currentParams.mqttServer;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
    Serial.printf("✅ Sent settings: STA=%s, AP=%s\n", currentParams.STAWifiID, currentParams.APWifiID);
}

void handleSaveWifi() {
    Serial.println("💾 POST /savewifi - Saving new WiFi settings");
    
    String sta_ssid = server.arg("sta_ssid");
    String sta_password = server.arg("sta_password");
    String ap_ssid = server.arg("ap_ssid");
    String ap_password = server.arg("ap_password");
    String mqtt_server = server.arg("mqtt_server");
    
    Serial.printf("📥 Received data:\n");
    Serial.printf("   STA SSID: %s\n", sta_ssid.c_str());
    Serial.printf("   STA Pass: %s\n", sta_password.c_str());
    Serial.printf("   AP SSID: %s\n", ap_ssid.c_str());
    Serial.printf("   AP Pass: %s\n", ap_password.c_str());
    Serial.printf("   MQTT Server: %s\n", mqtt_server.c_str());
    
    WifiParams newParams;
    
    // Initialize the struct
    memset(&newParams, 0, sizeof(newParams));
    
    // Copy ALL values from the web form
    strncpy(newParams.STAWifiID, sta_ssid.c_str(), sizeof(newParams.STAWifiID) - 1);
    strncpy(newParams.STApassword, sta_password.c_str(), sizeof(newParams.STApassword) - 1);
    strncpy(newParams.APWifiID, ap_ssid.c_str(), sizeof(newParams.APWifiID) - 1);
    strncpy(newParams.APpassword, ap_password.c_str(), sizeof(newParams.APpassword) - 1);
    strncpy(newParams.mqttServer, mqtt_server.c_str(), sizeof(newParams.mqttServer) - 1);
    
    // Ensure null termination
    newParams.STAWifiID[sizeof(newParams.STAWifiID) - 1] = '\0';
    newParams.STApassword[sizeof(newParams.STApassword) - 1] = '\0';
    newParams.APWifiID[sizeof(newParams.APWifiID) - 1] = '\0';
    newParams.APpassword[sizeof(newParams.APpassword) - 1] = '\0';
    newParams.mqttServer[sizeof(newParams.mqttServer) - 1] = '\0';
    
    // Pass to saveWifi
    saveWifi(newParams);
    
    server.send(200, "application/json", "{\"status\":\"success\"}");
    Serial.println("✅ WiFi settings saved successfully");
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

String getContentType(String filename) {
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

// Update in WebServer.cpp
void handleSaveTimeout() {
    Serial.println("💾 POST /savetimeout - Saving timeout");
    
    String body = server.arg("plain");
    Serial.printf("📥 Received timeout: %s\n", body.c_str());
    
    // Parse the incoming JSON
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        Serial.printf("❌ JSON parsing failed: %s\n", error.c_str());
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    int timeout = doc["timeout"] | 1;
    
    // Save using centralized FSHandler
    if (saveTimeout(timeout)) {
        server.send(200, "application/json", "{\"status\":\"success\"}");
        Serial.printf("✅ Timeout saved: %d seconds\n", timeout);
    } else {
        server.send(500, "application/json", "{\"status\":\"error\"}");
        Serial.println("❌ Failed to save timeout");
    }
}

void handleGetTimeout() {
    Serial.println("📡 GET /gettimeout - Returning timeout");
    
    // Load using centralized FSHandler
    int timeout = loadTimeout();
    
    StaticJsonDocument<128> doc;
    doc["timeout"] = timeout;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
    Serial.printf("✅ Sent timeout: %d seconds\n", timeout);
}

void handleGetStatistics() {
    Serial.println("📊 GET /getstatistics - Returning query statistics");
    
    String statsJSON = getStatisticsJSON();
    server.send(200, "application/json", statsJSON);
    Serial.printf("✅ Sent statistics (%d bytes)\n", statsJSON.length());
}

void handleRemoveSlaveStats() {
    Serial.println("🗑️ POST /removeslavestats - Removing slave statistics");
    
    String body = server.arg("plain");
    Serial.printf("📥 Received remove request: %s\n", body.c_str());
    
    // Parse the incoming JSON
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        Serial.printf("❌ JSON parsing failed: %s\n", error.c_str());
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    uint8_t slaveId = doc["slaveId"];
    const char* slaveName = doc["slaveName"];
    
    removeSlaveStatistic(slaveId, slaveName);
    server.send(200, "application/json", "{\"status\":\"success\"}");
    Serial.printf("✅ Removed statistics for slave %d: %s\n", slaveId, slaveName);
}

// Add these function declarations in WebServer.h and implementations in WebServer.cpp

void handleGetSlaveConfig() {
    Serial.println("🔍 POST /getslaveconfig - Searching for specific slave");
    
    String body = server.arg("plain");
    if (body.length() == 0) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty request body\"}");
        return;
    }
    
    Serial.printf("📥 Received search request: %s\n", body.c_str());
    
    // Parse the incoming JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        Serial.printf("❌ JSON parsing failed: %s\n", error.c_str());
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    uint8_t slaveId = doc["slaveId"];
    const char* slaveName = doc["slaveName"];
    
    Serial.printf("🔎 Searching for slave ID: %d, Name: %s\n", slaveId, slaveName);
    
    // Load slave configuration
    JsonDocument configDoc;
    if (!loadSlaveConfig(configDoc)) {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"No slave configuration found\"}");
        return;
    }
    
    JsonArray slavesArray = configDoc["slaves"];
    bool slaveFound = false;
    JsonObject foundSlave;
    
    // Search for the specific slave
    for (JsonObject slave : slavesArray) {
        if (slave["id"] == slaveId && strcmp(slave["name"], slaveName) == 0) {
            foundSlave = slave;
            slaveFound = true;
            break;
        }
    }
    
    if (!slaveFound) {
        Serial.printf("❌ Slave not found: ID=%d, Name=%s\n", slaveId, slaveName);
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Slave not found\"}");
        return;
    }
    
    // Return the found slave configuration
    String response;
    serializeJson(foundSlave, response);
    server.send(200, "application/json", response);
    Serial.printf("✅ Found slave configuration for ID %d: %s\n", slaveId, slaveName);
}

void handleUpdateSlaveConfig() {
    Serial.println("💾 POST /updateslaveconfig - Updating specific slave");
    
    String body = server.arg("plain");
    if (body.length() == 0) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Empty request body\"}");
        return;
    }
    
    Serial.printf("📥 Received update request: %s\n", body.c_str());
    
    // Parse the incoming JSON
    JsonDocument updateDoc;
    DeserializationError error = deserializeJson(updateDoc, body);
    
    if (error) {
        Serial.printf("❌ JSON parsing failed: %s\n", error.c_str());
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    // Validate required fields using new method
    if (!updateDoc["id"].is<int>() || !updateDoc["name"].is<const char*>()) {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing required fields: id and name\"}");
        return;
    }
    
    uint8_t slaveId = updateDoc["id"];
    const char* slaveName = updateDoc["name"];
    
    Serial.printf("🔄 Updating slave ID: %d, Name: %s\n", slaveId, slaveName);
    
    // Load current slave configuration
    JsonDocument configDoc;
    if (!loadSlaveConfig(configDoc)) {
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"No slave configuration found\"}");
        return;
    }
    
    JsonArray slavesArray = configDoc["slaves"];
    bool slaveFound = false;
    int slaveIndex = -1;
    
    // Find the specific slave
    for (int i = 0; i < slavesArray.size(); i++) {
        JsonObject slave = slavesArray[i];
        if (slave["id"] == slaveId && strcmp(slave["name"], slaveName) == 0) {
            slaveIndex = i;
            slaveFound = true;
            break;
        }
    }
    
    if (!slaveFound) {
        Serial.printf("❌ Slave not found for update: ID=%d, Name=%s\n", slaveId, slaveName);
        server.send(404, "application/json", "{\"status\":\"error\",\"message\":\"Slave not found\"}");
        return;
    }
    
    // Replace the slave object with the updated one
    slavesArray[slaveIndex].clear();
    for (JsonPair kv : updateDoc.as<JsonObject>()) {
        slavesArray[slaveIndex][kv.key()] = kv.value();
    }
    
    // Save the updated configuration
    if (saveSlaveConfig(configDoc)) {
        server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Slave configuration updated successfully\"}");
        Serial.printf("✅ Successfully updated slave ID %d: %s\n", slaveId, slaveName);
        
        // Reload slaves in Modbus handler
        modbus_reloadSlaves();
    } else {
        server.send(500, "application/json", "{\"status\":\"error\",\"message\":\"Failed to save configuration\"}");
        Serial.println("❌ Failed to save updated slave configuration");
    }
}

void handleToggleDebug() {
    String body = server.arg("plain");
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.send(400, "application/json", "{\"status\":\"error\"}");
        return;
    }
    
    debugEnabled = doc["enabled"] | false;
    server.send(200, "application/json", "{\"status\":\"success\"}");
    
    Serial.printf("🔧 Debug mode %s\n", debugEnabled ? "ENABLED" : "DISABLED");
}

void handleGetDebugState() {
    StaticJsonDocument<128> doc;
    doc["enabled"] = debugEnabled;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void handleGetDebugMessages() {
    // Return stored debug messages as JSON array
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
    
    // Create JSON message for web interface
    StaticJsonDocument<512> doc;
    doc["topic"] = topic;
    doc["message"] = message;
    doc["timestamp"] = millis();
    
    String jsonMessage;
    serializeJson(doc, jsonMessage);
    
    // Store in circular buffer
    debugMessages[debugMessageIndex] = jsonMessage;
    debugMessageIndex = (debugMessageIndex + 1) % MAX_DEBUG_MESSAGES;
    
    // Track actual count
    if (debugMessageCount < MAX_DEBUG_MESSAGES) {
        debugMessageCount++;
    }
    
    Serial.printf("📢 DEBUG [%s]: %s\n", topic, message);
}


void handleGetIPInfo() {
    Serial.println("📡 GET /getipinfo - Returning IP information");
    
    StaticJsonDocument<512> doc;
    
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
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
    Serial.printf("✅ Sent IP info - STA: %s, AP: %s\n", 
                  doc["sta_ip"].as<const char*>(), 
                  doc["ap_ip"].as<const char*>());
}
