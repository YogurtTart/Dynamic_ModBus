#include "ModBusHandler.h"

// ==================== GLOBAL VARIABLES ====================

ModbusMaster node;
SensorSlave* slaves = nullptr;
int slaveCount = 0;

// Non-blocking query state
unsigned long lastQueryTime = 0;
uint8_t currentSlaveIndex = 0;
unsigned long pollInterval = kDefaultPollInterval;
unsigned long timeoutDuration = kDefaultTimeout;

// Non-blocking state variables
enum QueryState { 
    STATE_IDLE, 
    STATE_START_QUERY, 
    STATE_WAIT_RESPONSE, 
    STATE_PROCESS_DATA, 
    STATE_WAITING 
};

QueryState currentState = STATE_IDLE;
unsigned long lastActionTime = 0;
unsigned long queryStartTime = 0;
bool waitingForResponse = false;

// Statistics
SlaveStatistics slaveStats[kMaxStatisticsSlaves];
uint8_t slaveStatsCount = 0;

// ==================== MEMORY SAFETY MACRO ====================
#define CLEANUP(ptr) do { if(ptr) { delete[] ptr; ptr = nullptr; } } while(0)

// ==================== RS485 CONTROL FUNCTIONS ====================

void preTransmission() { 
    digitalWrite(kRs485DePin, HIGH); 
}

void postTransmission() { 
    digitalWrite(kRs485DePin, LOW); 
}

// ==================== DATA CONVERSION FUNCTIONS ====================

float convertRegisterToTemperature(uint16_t registerValue, float divider) {
    int16_t tempInt;
    if (registerValue & 0x8000) {
        tempInt = -((0xFFFF - registerValue) + 1);
    } else {
        tempInt = registerValue;
    }
    return (tempInt * 0.1f) / divider;
}

float convertRegisterToHumidity(uint16_t registerValue, float divider) {
    return (registerValue * 0.1f) / divider;
}

float calculateCurrent(uint64_t registerValue, float divider) {
    return (registerValue * slaves[currentSlaveIndex].ct / 10000.0f) / divider;
}

float calculateSinglePhasePower(int64_t registerValue, float divider, float ct, float pt) {
    return (registerValue * pt * ct / 100.0f) / divider;
}

float calculateThreePhasePower(int64_t registerValue, float divider, float ct, float pt) {
    return (registerValue * pt * ct / 10.0f) / divider;
}

float calculatePowerFactor(int64_t registerValue, float divider) {
    return (registerValue / 1000.0f) / divider;
}

float calculateVoltage(uint64_t registerValue, float divider, float pt) {
    return (registerValue * pt / 100.0f) / divider;
}

float readEnergyValue(uint64_t rawValue, float divider) {
    return (rawValue / 100.0f) / divider;
}

// ==================== MODBUS INITIALIZATION ====================

bool initModbus() {
    pinMode(kRs485DePin, OUTPUT);
    digitalWrite(kRs485DePin, LOW);
    
    node.begin(1, Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    
    Serial.println("✅ Modbus initialized");
    return true;
}

// ==================== CONFIGURATION LOADING HELPERS ====================

DeviceType determineDeviceTypeFromString(const String& deviceTypeStr) {
    if (deviceTypeStr == "G01S") return DEVICE_G01S;
    if (deviceTypeStr == "HeylaParam") return DEVICE_HEYLA_PARAM;
    if (deviceTypeStr == "HeylaVoltage") return DEVICE_HEYLA_VOLTAGE;
    if (deviceTypeStr == "HeylaEnergy") return DEVICE_HEYLA_ENERGY;
    return DEVICE_G01S;
}

void loadDeviceParameters(SensorSlave& slave, JsonObject slaveObj) {
    switch(slave.deviceType) {
        case DEVICE_G01S:
            loadG01SParameters(slave.config.sensor, slaveObj);
            break;
        case DEVICE_HEYLA_PARAM:
            loadMeterParameters(slave.config.meter, slaveObj);
            break;
        case DEVICE_HEYLA_VOLTAGE:
            loadVoltageParameters(slave.config.voltage, slaveObj);
            break;
        case DEVICE_HEYLA_ENERGY:
            loadEnergyParameters(slave.config.energy, slaveObj);
            break;
    }
}

void loadG01SParameters(SensorConfig& sensorConfig, JsonObject paramsObj) {
    JsonObject sensorObj = paramsObj["sensor"].as<JsonObject>();
    sensorConfig.tempDivider = sensorObj["tempdivider"] | 1.0f;
    sensorConfig.humidDivider = sensorObj["humiddivider"] | 1.0f;
}

void loadMeterParameters(MeterConfig& meterConfig, JsonObject paramsObj) {
    JsonObject meterObj = paramsObj["meter"].as<JsonObject>();
    
    // Load grouped dividers
    #define LOAD_GROUPED_PARAM(group) \
        if (meterObj[#group].is<JsonObject>()) { \
            JsonObject paramObj = meterObj[#group].as<JsonObject>(); \
            meterConfig.group.divider = paramObj["divider"] | 1.0f; \
        }
    
    LOAD_GROUPED_PARAM(Current);
    LOAD_GROUPED_PARAM(zeroPhaseCurrent);
    LOAD_GROUPED_PARAM(ActivePower);
    LOAD_GROUPED_PARAM(totalActivePower);
    LOAD_GROUPED_PARAM(ReactivePower);
    LOAD_GROUPED_PARAM(totalReactivePower);
    LOAD_GROUPED_PARAM(ApparentPower);
    LOAD_GROUPED_PARAM(totalApparentPower);
    LOAD_GROUPED_PARAM(PowerFactor);
    LOAD_GROUPED_PARAM(totalPowerFactor);
    
    #undef LOAD_GROUPED_PARAM
}

void loadVoltageParameters(VoltageConfig& voltageConfig, JsonObject paramsObj) {
    JsonObject voltageObj = paramsObj["voltage"].as<JsonObject>();
    
    #define LOAD_VOLTAGE_PARAM(field) \
        if (voltageObj[#field].is<JsonObject>()) { \
            JsonObject paramObj = voltageObj[#field].as<JsonObject>(); \
            voltageConfig.field.divider = paramObj["divider"] | 1.0f; \
        }
    
    LOAD_VOLTAGE_PARAM(Voltage);
    LOAD_VOLTAGE_PARAM(phaseVoltageMean);
    LOAD_VOLTAGE_PARAM(zeroSequenceVoltage);
    
    #undef LOAD_VOLTAGE_PARAM
}

void loadEnergyParameters(EnergyConfig& energyConfig, JsonObject paramsObj) {
    JsonObject energyObj = paramsObj["energy"].as<JsonObject>();
    
    #define LOAD_ENERGY_PARAM(field) \
        if (energyObj[#field].is<JsonObject>()) { \
            JsonObject paramObj = energyObj[#field].as<JsonObject>(); \
            energyConfig.field.divider = paramObj["divider"] | 1.0f; \
        }
    
    LOAD_ENERGY_PARAM(totalActiveEnergy);
    LOAD_ENERGY_PARAM(importActiveEnergy);
    LOAD_ENERGY_PARAM(exportActiveEnergy);
    
    #undef LOAD_ENERGY_PARAM
}

// ==================== SLAVE CONFIGURATION MANAGEMENT ====================

bool modbusReloadSlaves() {
    Serial.println("🔄 Reloading slaves with template system...");
    
    JsonDocument config;
    if (!loadSlaveConfig(config)) {
        Serial.println("❌ Failed to load slave configuration");
        return false;
    }
    
    int newIntervalSeconds, newTimeoutSeconds;
    loadPollingConfig(newIntervalSeconds, newTimeoutSeconds);
    
    updatePollInterval(newIntervalSeconds);
    updateTimeout(newTimeoutSeconds);
    
    currentState = STATE_IDLE;
    currentSlaveIndex = 0;
    waitingForResponse = false;

    JsonArray slavesArray = config["slaves"];
    int newSlaveCount = slavesArray.size();
    
    if (slaves != nullptr) {
        delete[] slaves;
        slaves = nullptr;
    }
    
    slaves = new SensorSlave[newSlaveCount]();
    slaveCount = newSlaveCount;
    
    for (int i = 0; i < slaveCount; i++) {
        JsonObject slaveObj = slavesArray[i];
        
        const char* deviceType = slaveObj["deviceType"];
        
        JsonDocument templateDoc;
        JsonObject templateConfig = templateDoc.to<JsonObject>();
        JsonDocument mergedDoc;
        JsonObject mergedConfig = mergedDoc.to<JsonObject>();
        
        if (!loadDeviceTemplate(deviceType, templateConfig)) {
            continue;
        }
        
        mergeWithOverride(slaveObj, templateConfig, mergedConfig);
        
        slaves[i].id = mergedConfig["id"] | slaveObj["id"];
        slaves[i].startRegister = mergedConfig["startReg"] | slaveObj["startReg"];
        slaves[i].registerCount = mergedConfig["numReg"] | slaveObj["numReg"];
        slaves[i].name = mergedConfig["name"] | slaveObj["name"].as<String>();
        slaves[i].mqttTopic = mergedConfig["mqttTopic"] | slaveObj["mqttTopic"].as<String>();
        slaves[i].deviceType = determineDeviceTypeFromString(deviceType);
        slaves[i].ct = mergedConfig["ct"] |  slaveObj["ct"];
        slaves[i].pt = mergedConfig["pt"] |  slaveObj["pt"];
        
        if (slaveObj["registerSize"].is<int>()) {
            int size = slaveObj["registerSize"];
            if (size >= 1 && size <= 4) {
                slaves[i].registerSize = static_cast<RegisterSize>(size);
            } else {
                slaves[i].registerSize = SIZE_16BIT;
            }
        } else {
            slaves[i].registerSize = SIZE_16BIT;
        }
        
        loadDeviceParameters(slaves[i], mergedConfig);
    }
    
    Serial.printf("✅ Reloaded %d slaves with template system\n", slaveCount);
    return true;
}

// ==================== DATA PROCESSING HELPERS ====================

void processSensorData(JsonObject& root, const SensorConfig& sensorConfig, uint64_t* combinedValues, RegisterSize regSize) {
    uint16_t tempRaw = combinedValues[0] & 0xFFFF;
    uint16_t humidRaw = combinedValues[1] & 0xFFFF;
    
    root["temperature_(C)"] = convertRegisterToTemperature(tempRaw, sensorConfig.tempDivider);

    root["temperature_(F)"] = ((convertRegisterToTemperature(tempRaw, sensorConfig.tempDivider)) * 9/5) + 32;
    
    root["humidity"] = convertRegisterToHumidity(humidRaw, sensorConfig.humidDivider);
}

void processMeterData(JsonObject& root, const MeterConfig& meterConfig, uint64_t* combinedValues, RegisterSize regSize, float ct, float pt) {
    int valueIndex = 0;
    
    // Current measurements - all use the same Current divider
    root["A_Current_(A)"] = calculateCurrent(combinedValues[valueIndex++], meterConfig.Current.divider);
    root["B_Current_(A)"] = calculateCurrent(combinedValues[valueIndex++], meterConfig.Current.divider);
    root["C_Current_(A)"] = calculateCurrent(combinedValues[valueIndex++], meterConfig.Current.divider);
    root["Zero_Phase_Current_(A)"] = calculateCurrent(combinedValues[valueIndex++], meterConfig.zeroPhaseCurrent.divider);
    
    // Active Power - A/B/C use ActivePower divider, Total uses totalActivePower divider
    root["A_Active_Power_(kW)"] = calculateSinglePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.ActivePower.divider, ct, pt);
    root["B_Active_Power_(kW)"] = calculateSinglePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.ActivePower.divider, ct, pt);
    root["C_Active_Power_(kW)"] = calculateSinglePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.ActivePower.divider, ct, pt);
    root["Total_Active_Power_(kW)"] = calculateThreePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.totalActivePower.divider, ct, pt);
    
    // Reactive Power - A/B/C use ReactivePower divider, Total uses totalReactivePower divider
    root["A_Reactive_Power_(kVAr)"] = calculateSinglePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.ReactivePower.divider, ct, pt);
    root["B_Reactive_Power_(kVAr)"] = calculateSinglePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.ReactivePower.divider, ct, pt);
    root["C_Reactive_Power_(kVAr)"] = calculateSinglePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.ReactivePower.divider, ct, pt);
    root["Total_Reactive_Power_(kVAr)"] = calculateThreePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.totalReactivePower.divider, ct, pt);
    
    // Apparent Power - A/B/C use ApparentPower divider, Total uses totalApparentPower divider
    root["A_Apparent_Power_(kVA)"] = calculateSinglePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.ApparentPower.divider, ct, pt);
    root["B_Apparent_Power_(kVA)"] = calculateSinglePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.ApparentPower.divider, ct, pt);
    root["C_Apparent_Power_(kVA)"] = calculateSinglePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.ApparentPower.divider, ct, pt);
    root["Total_Apparent_Power_(kVA)"] = calculateThreePhasePower(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.totalApparentPower.divider, ct, pt);
    
    // Power Factor - A/B/C use PowerFactor divider, Total uses totalPowerFactor divider
    root["A_Power_Factor"] = calculatePowerFactor(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.PowerFactor.divider);
    root["B_Power_Factor"] = calculatePowerFactor(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.PowerFactor.divider);
    root["C_Power_Factor"] = calculatePowerFactor(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.PowerFactor.divider);
    root["Total_Power_Factor"] = calculatePowerFactor(convertToSigned(combinedValues[valueIndex++], regSize), meterConfig.totalPowerFactor.divider);
}

void processVoltageData(JsonObject& root, const VoltageConfig& voltageConfig, uint64_t* combinedValues, RegisterSize regSize, float pt) {
    int valueIndex = 0;
    
    // A/B/C voltages use the same Voltage divider
    root["A_Voltage_(V)"] = calculateVoltage(combinedValues[valueIndex++], voltageConfig.Voltage.divider, pt);
    root["B_Voltage_(V)"] = calculateVoltage(combinedValues[valueIndex++], voltageConfig.Voltage.divider, pt);
    root["C_Voltage_(V)"] = calculateVoltage(combinedValues[valueIndex++], voltageConfig.Voltage.divider, pt);
    root["Phase_Voltage_Mean"] = calculateVoltage(combinedValues[valueIndex++], voltageConfig.phaseVoltageMean.divider, pt);
    root["Zero_Sequence_Voltage"] = calculateVoltage(combinedValues[valueIndex++], voltageConfig.zeroSequenceVoltage.divider, pt);
}

void processEnergyData(JsonObject& root, const EnergyConfig& energyConfig, uint64_t* combinedValues, RegisterSize regSize) {
    int valueIndex = 0;
    
    root["Total_Active_Energy_(kwH)"] = readEnergyValue(combinedValues[valueIndex++], energyConfig.totalActiveEnergy.divider);
    root["Import_Active_Energy_(kwH)"] = readEnergyValue(combinedValues[valueIndex++], energyConfig.importActiveEnergy.divider);
    root["Export_Active_Energy_(kwH)"] = readEnergyValue(combinedValues[valueIndex++], energyConfig.exportActiveEnergy.divider);
}

void publishData(const SensorSlave& slave, const JsonDocument& doc) {
    String sameDeviceDelta = getSameDeviceDelta(slave.id, slave.name.c_str(), false);
    getSameDeviceDelta(slave.id, slave.name.c_str(), true);
    
    unsigned long timeDelta = calculateTimeDelta(slave.id, slave.name.c_str());
    String formattedDelta = formatTimeDelta(timeDelta);
    
    String output;
    serializeJson(doc, output);
    publishMessage(slave.mqttTopic.c_str(), output.c_str());
    
    if (debugEnabled) {
        addDebugMessage(slave.mqttTopic.c_str(), output.c_str(), formattedDelta.c_str(), sameDeviceDelta.c_str());
    }
}

// ==================== COMMON ERROR HANDLER ====================

void publishSlaveError(uint8_t slaveId, const char* slaveName, const char* errorMsg) {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["id"] = slaveId;
    root["name"] = slaveName;
    root["error"] = errorMsg;
    
    for (int i = 0; i < slaveCount; i++) {
        if (slaves[i].id == slaveId && slaves[i].name == slaveName) {
            root["mqtt_topic"] = slaves[i].mqttTopic;
            publishData(slaves[i], doc);
            return;
        }
    }
}

// ==================== NON-BLOCKING QUERY STATE MACHINE ====================

bool startNonBlockingQuery() {
    if (currentSlaveIndex >= slaveCount) {
        return false;
    }
    
    SensorSlave& slave = slaves[currentSlaveIndex];
    
    node.begin(slave.id, Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    node.clearResponseBuffer();
    node.clearTransmitBuffer();

    delay(300);

    uint8_t result = node.readHoldingRegisters(slave.startRegister, slave.registerCount);
    queryStartTime = millis();
    waitingForResponse = true;

    Serial.printf("➡️ Querying slave %d: %s\n", slave.id, slave.name.c_str());
    return (result == node.ku8MBSuccess);
}

void processNonBlockingData() {
    SensorSlave& slave = slaves[currentSlaveIndex];
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    root["id"] = slave.id;
    root["name"] = slave.name;
    root["mqtt_topic"] = slave.mqttTopic;
    root["start_reg"] = slave.startRegister;
    root["num_reg"] = slave.registerCount;
    root["register_size"] = slave.registerSize;
    root["ct"] = slave.ct;
    root["pt"] = slave.pt;
    

    uint16_t* rawRegisters = new uint16_t[slave.registerCount];
    readAllRegistersIntoArray(rawRegisters, slave.registerCount);
    
    uint16_t combinedCount = 0;
    uint64_t* combinedValues = combineRegistersBySize(rawRegisters, slave.registerCount, slave.registerSize, combinedCount);
    
    switch(slave.deviceType) {
        case DEVICE_G01S:
            processSensorData(root, slave.config.sensor, combinedValues, slave.registerSize);
            break;
        case DEVICE_HEYLA_PARAM:
            processMeterData(root, slave.config.meter, combinedValues, slave.registerSize, slave.ct, slave.pt);
            break;
        case DEVICE_HEYLA_VOLTAGE:
            processVoltageData(root, slave.config.voltage, combinedValues, slave.registerSize, slave.pt);
            break;
        case DEVICE_HEYLA_ENERGY:
            processEnergyData(root, slave.config.energy, combinedValues, slave.registerSize);
            break;
    }
    
    CLEANUP(rawRegisters);
    CLEANUP(combinedValues);
    
    publishData(slave, doc);
    waitingForResponse = false;
}

void checkCycleCompletion() {
    if (currentSlaveIndex >= slaveCount) {
        unsigned long currentTime = millis();
        
        lastSequenceTime = currentTime;
        currentState = STATE_WAITING;
        lastActionTime = currentTime;
        
        addBatchSeparatorMessage();
        
        Serial.printf("🎉 Cycle complete - sequence time reset to: %lu\n", currentTime);
    } else {
        currentState = STATE_START_QUERY;
    }
}

void handleQueryStartFailure() {
    Serial.printf("❌ Failed to start query for slave %d\n", slaves[currentSlaveIndex].id);
    updateSlaveStatistic(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), false, false);
    publishSlaveError(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), "Failed to start Modbus query");
}

void handleQueryTimeout() {
    Serial.printf("⏰ TIMEOUT on slave %d after %lu ms - SKIPPING TO NEXT!\n", slaves[currentSlaveIndex].id, timeoutDuration);
    updateSlaveStatistic(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), false, true);
    publishSlaveError(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), "Modbus timeout - no response from device");
    waitingForResponse = false;
}

void updateNonBlockingQuery() {
    unsigned long currentTime = millis();
    
    switch (currentState) {
        case STATE_IDLE:
            currentState = STATE_START_QUERY;
            currentSlaveIndex = 0;
            lastActionTime = currentTime;
            waitingForResponse = false;
            Serial.println("🚀 Starting NON-BLOCKING query cycle");
            break;
            
        case STATE_START_QUERY:
            if (currentTime - lastActionTime >= kQueryInterval) {
                lastActionTime = currentTime;
                
                if (startNonBlockingQuery()) {
                    currentState = STATE_WAIT_RESPONSE;
                    Serial.printf("⏳ Waiting for slave %d response...\n", slaves[currentSlaveIndex].id);
                } else {
                    handleQueryStartFailure();
                    currentSlaveIndex++;
                    checkCycleCompletion();
                }
            }
            break;
            
        case STATE_WAIT_RESPONSE:
            if (currentTime - queryStartTime > timeoutDuration) {
                handleQueryTimeout();
                currentSlaveIndex++;
                checkCycleCompletion();
            } else if (node.getResponseBuffer(0) != 0xFFFF) {
                currentState = STATE_PROCESS_DATA;
            }
            break;
            
        case STATE_PROCESS_DATA:
            processNonBlockingData();
            updateSlaveStatistic(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), true, false);
            currentSlaveIndex++;
            checkCycleCompletion();
            break;
            
        case STATE_WAITING:
            if (currentTime - lastActionTime >= pollInterval) {
                Serial.printf("🔄 NEW CYCLE | Waited: %lums | Expected: %lums | Diff: %lums\n", currentTime - lastActionTime, pollInterval, (currentTime - lastActionTime) - pollInterval);
                currentState = STATE_START_QUERY;
                currentSlaveIndex = 0;
                lastActionTime = currentTime;
                waitingForResponse = false;
            }
            break;
    }
}

// ==================== CONFIGURATION MANAGEMENT ====================

void updateTimeout(int newTimeoutSeconds) {
    timeoutDuration = newTimeoutSeconds * 1000;
    Serial.printf("⏱️  Timeout updated to: %d seconds (%lu ms)\n", newTimeoutSeconds, timeoutDuration);
}

void updatePollInterval(int newIntervalSeconds) {
    pollInterval = newIntervalSeconds * 1000;
    
    if (currentState == STATE_WAITING) {
        lastActionTime = millis();
    }
    
    Serial.printf("🔄 Poll interval updated to: %d seconds (%lu ms)\n", newIntervalSeconds, pollInterval);
}

// ==================== STATISTICS MANAGEMENT ====================

void updateSlaveStatistic(uint8_t slaveId, const char* slaveName, bool success, bool timeout) {
    if (slaveId == 0 || slaveName == nullptr) return;
    
    for (int i = 0; i < slaveStatsCount; i++) {
        if (slaveStats[i].slaveId == slaveId && strcmp(slaveStats[i].slaveName, slaveName) == 0) {
            slaveStats[i].totalQueries++;
            if (success) {
                slaveStats[i].successCount++;
            } else if (timeout) {
                slaveStats[i].timeoutCount++;
            } else {
                slaveStats[i].failedCount++;
            }
            
            slaveStats[i].statusHistory[2] = slaveStats[i].statusHistory[1];
            slaveStats[i].statusHistory[1] = slaveStats[i].statusHistory[0];
            
            if (success) slaveStats[i].statusHistory[0] = 'S';
            else if (timeout) slaveStats[i].statusHistory[0] = 'T';
            else slaveStats[i].statusHistory[0] = 'F';
            
            return;
        }
    }
    
    if (slaveStatsCount < kMaxStatisticsSlaves) {
        SlaveStatistics* newStat = &slaveStats[slaveStatsCount];
        newStat->slaveId = slaveId;
        strncpy(newStat->slaveName, slaveName, sizeof(newStat->slaveName) - 1);
        newStat->slaveName[sizeof(newStat->slaveName) - 1] = '\0';
        newStat->totalQueries = 1;
        newStat->successCount = success ? 1 : 0;
        newStat->timeoutCount = timeout ? 1 : 0;
        newStat->failedCount = (!success && !timeout) ? 1 : 0;
        
        strcpy(newStat->statusHistory, "   ");
        slaveStatsCount++;
    }
}

String getStatisticsJson() {
    JsonDocument doc;
    JsonArray statsArray = doc.to<JsonArray>();
    
    for (int i = 0; i < slaveStatsCount; i++) {
        JsonObject statObj = statsArray.add<JsonObject>();
        statObj["slaveId"] = slaveStats[i].slaveId;
        statObj["slaveName"] = slaveStats[i].slaveName;
        statObj["totalQueries"] = slaveStats[i].totalQueries;
        statObj["success"] = slaveStats[i].successCount;
        statObj["timeout"] = slaveStats[i].timeoutCount;
        statObj["failed"] = slaveStats[i].failedCount;
        statObj["statusHistory"] = slaveStats[i].statusHistory;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

void removeSlaveStatistic(uint8_t slaveId, const char* slaveName) {
    if (slaveId == 0 || slaveName == nullptr) return;
    
    for (int i = 0; i < slaveStatsCount; i++) {
        if (slaveStats[i].slaveId == slaveId && strcmp(slaveStats[i].slaveName, slaveName) == 0) {
            for (int j = i; j < slaveStatsCount - 1; j++) {
                slaveStats[j] = slaveStats[j + 1];
            }
            slaveStatsCount--;
            Serial.printf("📊 Removed statistics for slave %d: %s\n", slaveId, slaveName);
            return;
        }
    }
}

// ==================== UTILITY FUNCTIONS ====================

uint32_t readUint32FromRegisters(uint16_t highWord, uint16_t lowWord) {
    return ((uint32_t)highWord << 16) | lowWord;
}

float readEnergyValue(uint16_t registerIndex, float divider) {
    uint16_t highWord = node.getResponseBuffer(registerIndex);
    uint16_t lowWord = node.getResponseBuffer(registerIndex + 1);
    
    uint32_t value = readUint32FromRegisters(highWord, lowWord);
    return (value / 100.0f) / divider;
}

void addBatchSeparatorMessage() {
    if (!debugEnabled) return;
    
    JsonDocument doc;
    doc["type"] = "batch_separator";
    doc["message"] = "Query Loop Completed";
    
    String output;
    serializeJson(doc, output);
    
    addDebugMessage("BATCH", output.c_str(), "0", "0");
}

// ==================== REGISTER PROCESSING FUNCTIONS ====================

void readAllRegistersIntoArray(uint16_t* registerArray, uint16_t numRegisters) {
    for (int i = 0; i < numRegisters; i++) {
        registerArray[i] = node.getResponseBuffer(i);
    }
}

uint64_t combineRegisters(uint16_t* registers, RegisterSize size, uint16_t startIndex) {
    uint64_t result = 0;
    
    for (int i = 0; i < size; i++) {
        result = (result << 16) | registers[startIndex + i];
    }
    
    return result;
}

uint64_t* combineRegistersBySize(uint16_t* rawRegisters, uint16_t numRawRegisters, RegisterSize regSize, uint16_t& combinedCount) {
    combinedCount = numRawRegisters / regSize;
    uint64_t* combinedArray = new uint64_t[combinedCount];
    
    for (int i = 0; i < combinedCount; i++) {
        uint16_t startIndex = i * regSize;
        combinedArray[i] = combineRegisters(rawRegisters, regSize, startIndex);
    }
    
    return combinedArray;
}

int64_t convertToSigned(uint64_t value, RegisterSize regSize) {
    switch(regSize) {
        case SIZE_16BIT:
            return (int16_t)(value & 0xFFFF);
        case SIZE_32BIT:
            return (int32_t)(value & 0xFFFFFFFF);
        case SIZE_48BIT:
            if (value & 0x800000000000) {
                return (int64_t)(value | 0xFFFF000000000000);
            } else {
                return value;
            }
        case SIZE_64BIT:
            return (int64_t)value;
        default:
            return (int64_t)value;
    }
}