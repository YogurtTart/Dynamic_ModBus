#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ModbusMaster.h>
#include <LittleFS.h>
#include "FSHandler.h"

struct SensorSlave {
    uint8_t id;
    uint16_t startReg;
    uint16_t numRegs;
    String name;
    String mqttTopic;
};

// Function declarations
bool modbus_begin();
String modbus_readAllDataJSON();
float convertRegisterToTemperature(uint16_t regVal);
float convertRegisterToHumidity(uint16_t regVal);
bool modbus_reloadSlaves();