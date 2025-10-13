#pragma once
#include <ESP8266WebServer.h>
#include "EEEProm.h"
#include "FSHandler.h"

extern ESP8266WebServer server;

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

String getContentType(String filename);