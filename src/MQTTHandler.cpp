#include "MQTTHandler.h"
#include <Arduino.h>

// ==================== GLOBAL VARIABLES ====================
// ✅ REMOVED: Hard-coded mqttServer - now uses currentParams.mqttServer
const uint16_t mqttPort = 1883;
const char* mqttTopicPub = "Lora/receive";
unsigned long previousMQTTReconnect = 0;
const unsigned long mqttReconnectInterval = 20000;  // 20 seconds

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ==================== MQTT CONNECTION MANAGEMENT ====================

void checkMQTT() {
    unsigned long now = millis();
    
    // Only check MQTT connection every 20 seconds when not connected
    if (!mqttClient.connected()) {
        if (now - previousMQTTReconnect >= mqttReconnectInterval) {
            previousMQTTReconnect = now;
            reconnectMQTT();
        }
    } else {
        // Process MQTT messages when connected
        mqttClient.loop();
    }
}

void reconnectMQTT() {
    // ✅ USE CONFIG: Get server from EEPROM settings
    const char* server = currentParams.mqttServer;
    uint16_t port = atoi(currentParams.mqttPort); // Convert port string to int
    
    mqttClient.setServer(server, port);

    Serial.print("🔌 Attempting MQTT connection to ");
    Serial.print(server);
    Serial.print(":");
    Serial.print(port);
    Serial.println("...");
    
    if (mqttClient.connect("ESP8266_LoRa_Client")) {
        Serial.println("✅ MQTT connected");
    } else {
        Serial.print("❌ MQTT failed, rc=");
        Serial.print(mqttClient.state());
        Serial.println(" try again in 20 seconds");
    }
}

// ==================== MESSAGE PUBLISHING ====================

// ✅ Centralized publish function
void publishMessage(const char* topic, const char* payload) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic, payload);
        Serial.print("📤 MQTT Published → ");
        Serial.print(topic);
        Serial.print(": ");
        Serial.println(payload);
    } else {
        Serial.println("⚠️ MQTT not connected, message not sent");
    }
}

// 🆕 ADDED: Connection status helper
bool isMQTTConnected() {
    return mqttClient.connected();
}

// 🆕 ADDED: Get MQTT server info
String getMQTTServer() {
    return String(currentParams.mqttServer) + ":" + String(currentParams.mqttPort);
}