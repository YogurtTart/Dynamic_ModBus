#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ModbusMaster.h>

#include "FSHandler.h"
#include "MQTTHandler.h"
#include "WebServer.h"

// Constants
constexpr uint8_t kMaxStatisticsSlaves = 20;
constexpr uint8_t kRs485DePin = 5;
constexpr unsigned long kDefaultQueryInterval = 200; // ms

// Forward declarations
extern int slaveCount;
extern unsigned long timeoutDuration;

// Structures
struct MeterParameter {
    float ct;
    float pt;
    float divider;
};

struct VoltageParameter {
    float pt;
    float divider;
};

struct EnergyParameter {
    float divider;
};

struct SensorSlave {
    uint8_t id;
    uint16_t startRegister;
    uint16_t registerCount;
    String name;
    String mqttTopic;
    
    // Sensor parameters
    float tempDivider;
    float humidDivider;
    
    // Meter parameters
    MeterParameter aCurrent, bCurrent, cCurrent, zeroPhaseCurrent;
    MeterParameter aActivePower, bActivePower, cActivePower, totalActivePower;
    MeterParameter aReactivePower, bReactivePower, cReactivePower, totalReactivePower;
    MeterParameter aApparentPower, bApparentPower, cApparentPower, totalApparentPower;
    MeterParameter aPowerFactor, bPowerFactor, cPowerFactor, totalPowerFactor;
    
    // Voltage parameters
    VoltageParameter aVoltage, bVoltage, cVoltage, phaseVoltageMean, zeroSequenceVoltage;
    
    // Energy parameters
    EnergyParameter totalActiveEnergy, importActiveEnergy, exportActiveEnergy;
};

struct SlaveStatistics {
    uint8_t slaveId;
    char slaveName[32];
    uint32_t totalQueries;
    uint32_t successCount;
    uint32_t timeoutCount;
    uint32_t failedCount;
};

// ModBus Initialization
bool initModbus();
bool modbusReloadSlaves();

// Query Management
void updateNonBlockingQuery();
void updatePollInterval(int intervalSeconds);
void updateTimeout(int timeoutSeconds);

// Data Processing
float convertRegisterToTemperature(uint16_t registerValue, float divider = 1.0f);
float convertRegisterToHumidity(uint16_t registerValue, float divider = 1.0f);
float readEnergyValue(uint16_t registerIndex, float divider);

// Statistics
void updateSlaveStatistic(uint8_t slaveId, const char* slaveName, bool success, bool timeout);
String getStatisticsJson();
void removeSlaveStatistic(uint8_t slaveId, const char* slaveName);

// Utility Functions
uint32_t readUint32FromRegisters(uint16_t highWord, uint16_t lowWord);

// Configuration loading helpers
void loadDeviceParameters(SensorSlave& slave, JsonObject slaveObj);
void loadMeterParameters(SensorSlave& slave, JsonObject slaveObj);
void loadVoltageParameters(SensorSlave& slave, JsonObject slaveObj);
void loadEnergyParameters(SensorSlave& slave, JsonObject slaveObj);

// Data processing helpers
void processSensorData(JsonObject& root, const SensorSlave& slave);
void processMeterData(JsonObject& root, const SensorSlave& slave);
void processVoltageData(JsonObject& root, const SensorSlave& slave);
void processEnergyData(JsonObject& root, const SensorSlave& slave);
void publishData(const SensorSlave& slave, const JsonDocument& doc, bool success);

// Error handling helpers
void handleQueryStartFailure();
void handleQueryTimeout();
void checkCycleCompletion();