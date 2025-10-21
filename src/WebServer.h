#pragma once
#include <ESP8266WebServer.h>
#include "EEEProm.h"
#include "FSHandler.h"

extern ESP8266WebServer server;

extern bool debugEnabled;
extern void addDebugMessage(const char* topic, const char* message);

void setupWebServer();
void handleRoot();
void handleSaveWifi();
void handleGetWifi();
void handleStaticFiles();
void handleSlavesPage();
void handleSaveSlaves();
void handleGetSlaves();
void handleSavePollInterval();
void handleGetPollInterval();
void handleSaveTimeout();   
void handleGetTimeout();   
void handleGetStatistics();
void handleRemoveSlaveStats();
void handleGetSlaveConfig();
void handleUpdateSlaveConfig();
void handleGetIPInfo();

void handleToggleDebug();
void handleGetDebugState();
void handleGetDebugMessages();

String getContentType(String filename);