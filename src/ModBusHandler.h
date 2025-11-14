#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ModbusMaster.h>

#include "FSHandler.h"
#include "MQTTHandler.h"
#include "WebServer.h"
#include "TemplateManager.h"

/********************************TO ADD NEW DEVICE**********************************************/
struct DeviceTypes {
    static constexpr const char* G01S = "G01S";
    static constexpr const char* HeylaParam = "HeylaParam";
    static constexpr const char* HeylaVoltage = "HeylaVoltage";
    static constexpr const char* HeylaEnergy = "HeylaEnergy";
};

// ==================== CONSTANTS ====================
constexpr uint8_t kMaxStatisticsSlaves = 12;
constexpr uint8_t kRs485DePin = 5;
constexpr unsigned long kDefaultQueryInterval = 200; // ms
constexpr unsigned long kDefaultPollInterval = 10000;    // 10 seconds
constexpr unsigned long kDefaultTimeout = 1000;          // 1 second
constexpr unsigned long kQueryInterval = 200;            // 0.2 seconds between slaves

// ==================== REGISTER SIZE ENUM ====================
enum RegisterSize {
    SIZE_16BIT = 1,  // Single 16-bit register
    SIZE_32BIT = 2,  // Two 16-bit registers = 32-bit value  
    SIZE_48BIT = 3,  // Three 16-bit registers = 48-bit value
    SIZE_64BIT = 4   // Four 16-bit registers = 64-bit value
};

// ==================== DEVICE TYPE ENUM ====================
enum DeviceType {
    DEVICE_G01S = 0,
    DEVICE_HEYLA_PARAM = 1,
    DEVICE_HEYLA_VOLTAGE = 2,
    DEVICE_HEYLA_ENERGY = 3
};

// ==================== PARAMETER STRUCTURES ====================

struct MeterParameter {
    float divider;  
};

struct VoltageParameter {
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
    MeterParameter Current, zeroPhaseCurrent;
    MeterParameter ActivePower, totalActivePower;
    MeterParameter ReactivePower, totalReactivePower;
    MeterParameter ApparentPower, totalApparentPower;
    MeterParameter PowerFactor, totalPowerFactor;
};

struct VoltageConfig {
    VoltageParameter Voltage, phaseVoltageMean, zeroSequenceVoltage;
};

struct EnergyConfig {
    EnergyParameter totalActiveEnergy, importActiveEnergy, exportActiveEnergy;
};

// ==================== MAIN SLAVE STRUCTURE ====================
// Add these fields to the SensorSlave struct:
struct SensorSlave {
    // Common fields for ALL devices
    uint8_t id;
    uint16_t startRegister;
    uint16_t registerCount;
    String name;
    String mqttTopic;
    
    float ct;       
    float pt; 
    
    DeviceType deviceType;
    
    RegisterSize registerSize;
    
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
float readEnergyValue(uint64_t rawValue, float divider);

// ==================== STATISTICS MANAGEMENT ====================
void updateSlaveStatistic(uint8_t slaveId, const char* slaveName, bool success, bool timeout);
String getStatisticsJson();
void removeSlaveStatistic(uint8_t slaveId, const char* slaveName);

// ==================== UTILITY FUNCTIONS ====================
uint32_t readUint32FromRegisters(uint16_t highWord, uint16_t lowWord);

// ==================== CONFIGURATION HELPERS ====================
DeviceType determineDeviceTypeFromString(const String& deviceTypeStr);
void loadDeviceParameters(SensorSlave& slave, JsonObject slaveObj);
void loadG01SParameters(SensorConfig& sensorConfig, JsonObject paramsObj);
void loadMeterParameters(MeterConfig& meterConfig, JsonObject paramsObj);
void loadVoltageParameters(VoltageConfig& voltageConfig, JsonObject paramsObj);
void loadEnergyParameters(EnergyConfig& energyConfig, JsonObject paramsObj);

// ==================== REGISTER PROCESSING FUNCTIONS ====================
void readAllRegistersIntoArray(uint16_t* registerArray, uint16_t numRegisters);
uint64_t combineRegisters(uint16_t* registers, RegisterSize size, uint16_t startIndex);
uint64_t* combineRegistersBySize(uint16_t* rawRegisters, uint16_t numRawRegisters, RegisterSize regSize, uint16_t& combinedCount);
int64_t convertToSigned(uint64_t value, RegisterSize regSize);

// ==================== DATA PROCESSING HELPERS ====================
void processSensorData(JsonObject& root, const SensorConfig& sensorConfig, uint64_t* combinedValues, RegisterSize regSize);
void processMeterData(JsonObject& root, const MeterConfig& meterConfig, uint64_t* combinedValues, RegisterSize regSize, float ct, float pt);
void processVoltageData(JsonObject& root, const VoltageConfig& voltageConfig, uint64_t* combinedValues, RegisterSize regSize, float pt);
void processEnergyData(JsonObject& root, const EnergyConfig& energyConfig, uint64_t* combinedValues, RegisterSize regSize);
void publishData(const SensorSlave& slave, const JsonDocument& doc);

// ==================== CALCULATION FUNCTIONS ====================
float calculateCurrent(uint64_t registerValue, float divider);
float calculateSinglePhasePower(uint64_t registerValue, float divider);
float calculateThreePhasePower(uint64_t registerValue, float divider);
float calculatePowerFactor(uint64_t registerValue, float divider);
float calculateVoltage(uint64_t registerValue, float divider);

// ==================== ERROR HANDLING ====================
void handleQueryStartFailure();
void handleQueryTimeout();
void checkCycleCompletion();
void publishSlaveError(uint8_t slaveId, const char* slaveName, const char* errorMsg);

// ==================== DEBUG FUNCTIONS ====================
void addBatchSeparatorMessage();

// ==================== EXTERNAL VARIABLES ====================
extern int slaveCount;
extern unsigned long timeoutDuration;