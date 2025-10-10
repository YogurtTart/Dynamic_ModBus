#pragma once
#include <PubSubClient.h>
#include <WiFiClient.h>

extern const char* mqttServer;
extern const uint16_t mqttPort;
extern const char* mqttTopicPub;
extern unsigned long previousMQTTReconnect;
extern const unsigned long mqttReconnectInterval;

extern WiFiClient espClient;
extern PubSubClient mqttClient;

void reconnectMQTT();
void publishMessage(const char* topic, const char* payload);
