#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ModbusMaster.h>
#include <LittleFS.h>
#include "FSHandler.h"
#include "MQTTHandler.h"

extern int slaveCount;
extern unsigned long timeoutDuration; 

struct SensorSlave {
    uint8_t id;
    uint16_t startReg;
    uint16_t numReg;
    String name;
    String mqttTopic;
};

struct VoltageData {
    float phaseA;
    float phaseB;
    float phaseC;
    float phaseVoltageMean;
    float zeroSequenceVoltage;
    bool hasData = false;
};

bool hasVoltageData();

// Function declarations
bool initModbus();
float convertRegisterToTemperature(uint16_t regVal);
float convertRegisterToHumidity(uint16_t regVal);
bool modbus_reloadSlaves();

void updateNonBlockingQuery();
float convertRegisterToVoltage(uint16_t regVal);
void updatePollInterval(int newIntervalSeconds);
void updateTimeout(int newTimeoutSeconds);

// ✅ NEW: Non-blocking functions
bool startNonBlockingQuery();
void processNonBlockingData();