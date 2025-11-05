#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ModbusMaster.h>

#include "FSHandler.h"
#include "MQTTHandler.h"
#include "WebServer.h"

// ==================== DEVICE TYPE DEFINITIONS ====================

/**
 * @brief Supported device type identifiers
 */
struct DeviceTypes {
    static constexpr const char* G01S = "G01S";
    static constexpr const char* HeylaParam = "HeylaParam";
    static constexpr const char* HeylaVoltage = "HeylaVoltage";
    static constexpr const char* HeylaEnergy = "HeylaEnergy";
};

// ==================== CONSTANTS ====================

constexpr uint8_t kMaxStatisticsSlaves = 20;
constexpr uint8_t kRs485DePin = 5;
constexpr unsigned long kDefaultQueryInterval = 200; // ms

constexpr unsigned long kDefaultPollInterval = 10000;    // 10 seconds
constexpr unsigned long kDefaultTimeout = 1000;          // 1 second
constexpr unsigned long kQueryInterval = 200;            // 0.2 seconds between slaves

// ==================== STRUCTURES ====================

/**
 * @brief Meter parameter configuration
 */
struct MeterParameter {
    float ct;
    float pt;
    float divider;
};

/**
 * @brief Voltage parameter configuration  
 */
struct VoltageParameter {
    float pt;
    float divider;
};

/**
 * @brief Energy parameter configuration
 */
struct EnergyParameter {
    float divider;
};

/**
 * @brief Complete slave device configuration
 */
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

/**
 * @brief Query statistics for each slave
 */
struct SlaveStatistics {
    uint8_t slaveId;
    char slaveName[32];
    uint32_t totalQueries;
    uint32_t successCount;
    uint32_t timeoutCount;
    uint32_t failedCount;
    char statusHistory[3]; // Stores last 3 statuses as chars: 'S','F','T'
};

// ==================== MODBUS INITIALIZATION ====================

bool initModbus();
bool modbusReloadSlaves();

// ==================== QUERY MANAGEMENT ====================

void updateNonBlockingQuery();
void updatePollInterval(int intervalSeconds);
void updateTimeout(int timeoutSeconds);

// ==================== DATA PROCESSING ====================

float convertRegisterToTemperature(uint16_t registerValue, float divider = 1.0f);
float convertRegisterToHumidity(uint16_t registerValue, float divider = 1.0f);
float readEnergyValue(uint16_t registerIndex, float divider);

// ==================== STATISTICS MANAGEMENT ====================

void updateSlaveStatistic(uint8_t slaveId, const char* slaveName, bool success, bool timeout);
String getStatisticsJson();
void removeSlaveStatistic(uint8_t slaveId, const char* slaveName);

// ==================== UTILITY FUNCTIONS ====================

uint32_t readUint32FromRegisters(uint16_t highWord, uint16_t lowWord);

// ==================== CONFIGURATION HELPERS ====================

void loadDeviceParameters(SensorSlave& slave, JsonObject slaveObj);
void loadMeterParameters(SensorSlave& slave, JsonObject slaveObj);
void loadVoltageParameters(SensorSlave& slave, JsonObject slaveObj);
void loadEnergyParameters(SensorSlave& slave, JsonObject slaveObj);

// ==================== DATA PROCESSING HELPERS ====================

void processSensorData(JsonObject& root, const SensorSlave& slave);
void processMeterData(JsonObject& root, const SensorSlave& slave);
void processVoltageData(JsonObject& root, const SensorSlave& slave);
void processEnergyData(JsonObject& root, const SensorSlave& slave);
void publishData(const SensorSlave& slave, const JsonDocument& doc);

// ==================== ERROR HANDLING ====================

void handleQueryStartFailure();
void handleQueryTimeout();
void checkCycleCompletion();

// ==================== TIMING FUNCTIONS ====================

unsigned long calculateTimeDelta(uint8_t slaveId, const char* slaveName);
String calculateSameDeviceDelta(uint8_t slaveId, const char* slaveName);
String getSameDeviceDelta(uint8_t slaveId, const char* slaveName, bool resetTimer);

// ==================== DEBUG FUNCTIONS ====================

void addBatchSeparatorMessage();

// ==================== EXTERNAL VARIABLES ====================

extern int slaveCount;
extern unsigned long timeoutDuration;