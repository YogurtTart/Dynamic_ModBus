#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "EEEProm.h"

// Forward declarations
extern WifiParams currentParams;

// Global variables
extern const uint16_t mqttPort;
extern const char* mqttTopicPub;
extern unsigned long previousMQTTReconnect;
extern const unsigned long mqttReconnectInterval;

extern WiFiClient espClient;
extern PubSubClient mqttClient;

// Function declarations
void reconnectMQTT();
void publishMessage(const char* topic, const char* payload);
void checkMQTT();

// 🆕 ADDED: Connection status helpers
bool isMQTTConnected();
String getMQTTServer();