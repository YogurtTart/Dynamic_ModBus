#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ModbusMaster.h>

#include "FSHandler.h"
#include "MQTTHandler.h"
#include "WebServer.h"

/********************************TO ADD NEW DEVICE**********************************************/
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

// ==================== DEVICE TYPE ENUM ====================
enum DeviceType {
    DEVICE_G01S = 0,
    DEVICE_HEYLA_PARAM = 1,
    DEVICE_HEYLA_VOLTAGE = 2,
    DEVICE_HEYLA_ENERGY = 3
};

// ==================== PARAMETER STRUCTURES ====================

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

// ==================== DEVICE-SPECIFIC CONFIGS ====================

struct SensorConfig {
    float tempDivider;
    float humidDivider;
};

struct MeterConfig {
    MeterParameter aCurrent, bCurrent, cCurrent, zeroPhaseCurrent;
    MeterParameter aActivePower, bActivePower, cActivePower, totalActivePower;
    MeterParameter aReactivePower, bReactivePower, cReactivePower, totalReactivePower;
    MeterParameter aApparentPower, bApparentPower, cApparentPower, totalApparentPower;
    MeterParameter aPowerFactor, bPowerFactor, cPowerFactor, totalPowerFactor;
};

struct VoltageConfig {
    VoltageParameter aVoltage, bVoltage, cVoltage, phaseVoltageMean, zeroSequenceVoltage;
};

struct EnergyConfig {
    EnergyParameter totalActiveEnergy, importActiveEnergy, exportActiveEnergy;
};

// ==================== MAIN SLAVE STRUCTURE ====================
struct SensorSlave {
    // Common fields for ALL devices
    uint8_t id;
    uint16_t startRegister;
    uint16_t registerCount;
    String name;
    String mqttTopic;
    
    // Device type - tells us which config to use
    DeviceType deviceType;
    
    // Union - only ONE of these is active at a time
    union {
        SensorConfig sensor;
        MeterConfig meter;
        VoltageConfig voltage;
        EnergyConfig energy;
    } config;
};

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
DeviceType determineDeviceType(const String& name);
void loadDeviceParameters(SensorSlave& slave, JsonObject slaveObj);
void loadMeterParameters(MeterConfig& meterConfig, JsonObject slaveObj);
void loadVoltageParameters(VoltageConfig& voltageConfig, JsonObject slaveObj);
void loadEnergyParameters(EnergyConfig& energyConfig, JsonObject slaveObj);

// ==================== DATA PROCESSING HELPERS ====================
void processSensorData(JsonObject& root, const SensorConfig& sensorConfig);
void processMeterData(JsonObject& root, const MeterConfig& meterConfig);
void processVoltageData(JsonObject& root, const VoltageConfig& voltageConfig);
void processEnergyData(JsonObject& root, const EnergyConfig& energyConfig);
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