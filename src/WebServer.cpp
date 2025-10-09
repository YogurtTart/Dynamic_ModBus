#include "WebServer.h"
#include "EEEProm.h"
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>

ESP8266WebServer server(80);

void setupWebServer() {
    Serial.println("🌐 Initializing Web Server on port 80...");
    
    // Serve static files
    server.onNotFound(handleStaticFiles);
    
    // API endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/savewifi", HTTP_POST, handleSaveWifi);
    server.on("/getwifi", HTTP_GET, handleGetWifi);
    
    server.begin();
    Serial.println("✅ HTTP server started successfully");
    Serial.println("📍 Available endpoints:");
    Serial.println("   GET  /         - Configuration page");
    Serial.println("   GET  /getwifi  - Get current WiFi settings");
    Serial.println("   POST /savewifi - Save new WiFi settings");
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

void handleGetWifi() {
    Serial.println("📡 GET /getwifi - Returning current WiFi settings");
    StaticJsonDocument<512> doc;
    doc["sta_ssid"] = currentParams.STAWifiID;
    doc["sta_password"] = currentParams.STApassword;
    doc["ap_ssid"] = currentParams.APWifiID;
    doc["ap_password"] = currentParams.APpassword;
    
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
    
    Serial.printf("📥 Received data:\n");
    Serial.printf("   STA SSID: %s\n", sta_ssid.c_str());
    Serial.printf("   STA Pass: %s\n", sta_password.c_str());
    Serial.printf("   AP SSID: %s\n", ap_ssid.c_str());
    Serial.printf("   AP Pass: %s\n", ap_password.c_str());
    
    WifiParams newParams;
    
    // Initialize the struct
    memset(&newParams, 0, sizeof(newParams));
    
    // Copy ALL values from the web form
    strncpy(newParams.STAWifiID, sta_ssid.c_str(), sizeof(newParams.STAWifiID) - 1);
    strncpy(newParams.STApassword, sta_password.c_str(), sizeof(newParams.STApassword) - 1);
    strncpy(newParams.APWifiID, ap_ssid.c_str(), sizeof(newParams.APWifiID) - 1);
    strncpy(newParams.APpassword, ap_password.c_str(), sizeof(newParams.APpassword) - 1);
    
    // Ensure null termination
    newParams.STAWifiID[sizeof(newParams.STAWifiID) - 1] = '\0';
    newParams.STApassword[sizeof(newParams.STApassword) - 1] = '\0';
    newParams.APWifiID[sizeof(newParams.APWifiID) - 1] = '\0';
    newParams.APpassword[sizeof(newParams.APpassword) - 1] = '\0';
    
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