#include "MQTTHandler.h"
#include <Arduino.h>

// Global variables
const char* mqttServer = "192.168.31.66";
const uint16_t mqttPort = 1883;
const char* mqttTopicPub = "Lora/receive";
unsigned long previousMQTTReconnect = 0;
const unsigned long mqttReconnectInterval = 20000;  // 20 seconds

WiFiClient espClient;
PubSubClient mqttClient(espClient);

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
    mqttClient.setServer(currentParams.mqttServer, mqttPort);

    Serial.print("🔌 Attempting MQTT connection to ");
    Serial.print(currentParams.mqttServer);
    Serial.print(":");
    Serial.print(mqttPort);
    Serial.println("...");
    
    if (mqttClient.connect("ESP8266_LoRa_Client")) {
        Serial.println("✅ connected");
    } else {
        Serial.print("❌ failed, rc=");
        Serial.print(mqttClient.state());
        Serial.println(" try again in 20 seconds");
    }
}

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