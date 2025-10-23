#include "MQTTHandler.h"
#include <Arduino.h>

const char* mqttServer = "192.168.31.66";
const uint16_t mqttPort = 1883;
const char* mqttTopicPub ="Lora/receive";
unsigned long previousMQTTReconnect = 0;
const unsigned long mqttReconnectInterval = 5000;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void reconnectMQTT() {
    unsigned long now = millis();
    if (!mqttClient.connected() && now - previousMQTTReconnect > mqttReconnectInterval) {
        previousMQTTReconnect = now;
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect("ESP8266_LoRa_Client")) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again later");
        }
    }
}

// ✅ Centralized publish function
void publishMessage(const char* topic, const char* payload) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic, payload);
        Serial.print("MQTT Published → ");
        Serial.print(topic);
        Serial.print(": ");
        Serial.println(payload);
    } else {
        Serial.println("⚠️ MQTT not connected, message not sent");
    }
}