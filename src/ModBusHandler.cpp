#include "ModBusHandler.h"

// Constants
constexpr unsigned long kDefaultPollInterval = 10000;    // 10 seconds
constexpr unsigned long kDefaultTimeout = 1000;          // 1 second
constexpr unsigned long kQueryInterval = 200;            // 0.2 seconds between slaves

// Global Variables
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

float calculateCurrent(uint16_t registerValue, float ct, float divider) {
    return (registerValue * ct / 10000.0f) / divider;
}

float calculateSinglePhasePower(int16_t registerValue, float pt, float ct, float divider) {
    return (registerValue * pt * ct / 100.0f) / divider;
}

float calculateThreePhasePower(int16_t registerValue, float pt, float ct, float divider) {
    return (registerValue * pt * ct / 10.0f) / divider;
}

float calculatePowerFactor(int16_t registerValue, float divider) {
    return (registerValue / 10000.0f) / divider;
}

float calculateVoltage(uint16_t registerValue, float pt, float divider) {
    return (registerValue * pt / 100.0f) / divider;
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

void loadDeviceParameters(SensorSlave& slave, JsonObject slaveObj) {
    if (slave.name.indexOf("Sensor") >= 0) {
        slave.tempDivider = slaveObj["tempdivider"] | 1.0f;
        slave.humidDivider = slaveObj["humiddivider"] | 1.0f;
    } else if (slave.name.indexOf("Meter") >= 0) {
        loadMeterParameters(slave, slaveObj);
    } else if (slave.name.indexOf("Voltage") >= 0) {
        loadVoltageParameters(slave, slaveObj);
    } else if (slave.name.indexOf("Energy") >= 0) {
        loadEnergyParameters(slave, slaveObj);
    }
}

void loadMeterParameters(SensorSlave& slave, JsonObject slaveObj) {
    #define LOAD_METER_PARAM(field) \
        slave.field.ct = slaveObj[#field]["ct"] | 1.0f; \
        slave.field.pt = slaveObj[#field]["pt"] | 1.0f; \
        slave.field.divider = slaveObj[#field]["divider"] | 1.0f;
    
    LOAD_METER_PARAM(aCurrent);
    LOAD_METER_PARAM(bCurrent);
    LOAD_METER_PARAM(cCurrent);
    LOAD_METER_PARAM(zeroPhaseCurrent);
    LOAD_METER_PARAM(aActivePower);
    LOAD_METER_PARAM(bActivePower);
    LOAD_METER_PARAM(cActivePower);
    LOAD_METER_PARAM(totalActivePower);
    LOAD_METER_PARAM(aReactivePower);
    LOAD_METER_PARAM(bReactivePower);
    LOAD_METER_PARAM(cReactivePower);
    LOAD_METER_PARAM(totalReactivePower);
    LOAD_METER_PARAM(aApparentPower);
    LOAD_METER_PARAM(bApparentPower);
    LOAD_METER_PARAM(cApparentPower);
    LOAD_METER_PARAM(totalApparentPower);
    LOAD_METER_PARAM(aPowerFactor);
    LOAD_METER_PARAM(bPowerFactor);
    LOAD_METER_PARAM(cPowerFactor);
    LOAD_METER_PARAM(totalPowerFactor);
    
    #undef LOAD_METER_PARAM
}

void loadVoltageParameters(SensorSlave& slave, JsonObject slaveObj) {
    #define LOAD_VOLTAGE_PARAM(field) \
        if (slaveObj[#field].is<JsonObject>()) { \
            slave.field.pt = slaveObj[#field]["pt"] | 1.0f; \
            slave.field.divider = slaveObj[#field]["divider"] | 1.0f; \
        } else { \
            slave.field.pt = 1.0f; \
            slave.field.divider = 1.0f; \
        }
    
    LOAD_VOLTAGE_PARAM(aVoltage);
    LOAD_VOLTAGE_PARAM(bVoltage);
    LOAD_VOLTAGE_PARAM(cVoltage);
    LOAD_VOLTAGE_PARAM(phaseVoltageMean);
    LOAD_VOLTAGE_PARAM(zeroSequenceVoltage);
    
    #undef LOAD_VOLTAGE_PARAM
}

void loadEnergyParameters(SensorSlave& slave, JsonObject slaveObj) {
    slave.totalActiveEnergy.divider = slaveObj["totalActiveEnergy"]["divider"] | 1.0f;
    slave.importActiveEnergy.divider = slaveObj["importActiveEnergy"]["divider"] | 1.0f;
    slave.exportActiveEnergy.divider = slaveObj["exportActiveEnergy"]["divider"] | 1.0f;
}

// ==================== SLAVE CONFIGURATION MANAGEMENT ====================

bool modbusReloadSlaves() {
    Serial.println("🔄 Reloading slaves...");
    
    JsonDocument config;
    if (!loadSlaveConfig(config)) {
        Serial.println("❌ Failed to load slave configuration");
        return false;
    }
    
    // Load configuration settings
    int newIntervalSeconds = loadPollInterval();
    updatePollInterval(newIntervalSeconds);

    int newTimeoutSeconds = loadTimeout();
    updateTimeout(newTimeoutSeconds);
    
    // Reset query state when reloading slaves
    currentState = STATE_IDLE;
    currentSlaveIndex = 0;
    waitingForResponse = false;

    // Get the slaves array
    JsonArray slavesArray = config["slaves"];
    int newSlaveCount = slavesArray.size();
    
    // Free old memory before allocating new
    if (slaves != nullptr) {
        delete[] slaves;
        slaves = nullptr;
    }
    
    // Allocate new memory and initialize to zero
    slaves = new SensorSlave[newSlaveCount]();
    slaveCount = newSlaveCount;
    
    // Load each slave configuration
    for (int i = 0; i < slaveCount; i++) {
        JsonObject slaveObj = slavesArray[i];
        slaves[i] = SensorSlave{};
        
        // Load basic fields
        slaves[i].id = slaveObj["id"];
        slaves[i].startRegister = slaveObj["startReg"];
        slaves[i].registerCount = slaveObj["numReg"];
        slaves[i].name = slaveObj["name"].as<String>();
        slaves[i].mqttTopic = slaveObj["mqttTopic"].as<String>();
        
        // Load device-specific parameters
        loadDeviceParameters(slaves[i], slaveObj);
        
        Serial.printf("✅ Slave: ID=%d, Name=%s\n", slaves[i].id, slaves[i].name.c_str());
    }
    
    Serial.printf("✅ Reloaded %d slaves\n", slaveCount);
    return true;
}

// ==================== DATA PROCESSING HELPERS ====================

void processSensorData(JsonObject& root, const SensorSlave& slave) {
    root["temperature"] = convertRegisterToTemperature(node.getResponseBuffer(0), slave.tempDivider);
    root["humidity"] = convertRegisterToHumidity(node.getResponseBuffer(1), slave.humidDivider);
}

void processMeterData(JsonObject& root, const SensorSlave& slave) {
    // Current values
    root["A_Current"] = calculateCurrent(node.getResponseBuffer(0), slave.aCurrent.ct, slave.aCurrent.divider);
    root["B_Current"] = calculateCurrent(node.getResponseBuffer(1), slave.bCurrent.ct, slave.bCurrent.divider);
    root["C_Current"] = calculateCurrent(node.getResponseBuffer(2), slave.cCurrent.ct, slave.cCurrent.divider);
    root["Zero_Phase_Current"] = calculateCurrent(node.getResponseBuffer(3), slave.zeroPhaseCurrent.ct, slave.zeroPhaseCurrent.divider);
    
    // Active Power
    root["A_Active_Power"] = calculateSinglePhasePower(node.getResponseBuffer(4), slave.aActivePower.pt, slave.aActivePower.ct, slave.aActivePower.divider);
    root["B_Active_Power"] = calculateSinglePhasePower(node.getResponseBuffer(5), slave.bActivePower.pt, slave.bActivePower.ct, slave.bActivePower.divider);
    root["C_Active_Power"] = calculateSinglePhasePower(node.getResponseBuffer(6), slave.cActivePower.pt, slave.cActivePower.ct, slave.cActivePower.divider);
    root["Total_Active_Power"] = calculateThreePhasePower(node.getResponseBuffer(7), slave.totalActivePower.pt, slave.totalActivePower.ct, slave.totalActivePower.divider);
    
    // Reactive Power
    root["A_Reactive_Power"] = calculateSinglePhasePower(node.getResponseBuffer(8), slave.aReactivePower.pt, slave.aReactivePower.ct, slave.aReactivePower.divider);
    root["B_Reactive_Power"] = calculateSinglePhasePower(node.getResponseBuffer(9), slave.bReactivePower.pt, slave.bReactivePower.ct, slave.bReactivePower.divider);
    root["C_Reactive_Power"] = calculateSinglePhasePower(node.getResponseBuffer(10), slave.cReactivePower.pt, slave.cReactivePower.ct, slave.cReactivePower.divider);
    root["Total_Reactive_Power"] = calculateThreePhasePower(node.getResponseBuffer(11), slave.totalReactivePower.pt, slave.totalReactivePower.ct, slave.totalReactivePower.divider);
    
    // Apparent Power
    root["A_Apparent_Power"] = calculateSinglePhasePower(node.getResponseBuffer(12), slave.aApparentPower.pt, slave.aApparentPower.ct, slave.aApparentPower.divider);
    root["B_Apparent_Power"] = calculateSinglePhasePower(node.getResponseBuffer(13), slave.bApparentPower.pt, slave.bApparentPower.ct, slave.bApparentPower.divider);
    root["C_Apparent_Power"] = calculateSinglePhasePower(node.getResponseBuffer(14), slave.cApparentPower.pt, slave.cApparentPower.ct, slave.cApparentPower.divider);
    root["Total_Apparent_Power"] = calculateThreePhasePower(node.getResponseBuffer(15), slave.totalApparentPower.pt, slave.totalApparentPower.ct, slave.totalApparentPower.divider);
    
    // Power Factor
    root["A_Power_Factor"] = calculatePowerFactor(node.getResponseBuffer(16), slave.aPowerFactor.divider);
    root["B_Power_Factor"] = calculatePowerFactor(node.getResponseBuffer(17), slave.bPowerFactor.divider);
    root["C_Power_Factor"] = calculatePowerFactor(node.getResponseBuffer(18), slave.cPowerFactor.divider);
    root["Total_Power_Factor"] = calculatePowerFactor(node.getResponseBuffer(19), slave.totalPowerFactor.divider);
}

void processVoltageData(JsonObject& root, const SensorSlave& slave) {
    root["A_Voltage"] = calculateVoltage(node.getResponseBuffer(0), slave.aVoltage.pt, slave.aVoltage.divider);
    root["B_Voltage"] = calculateVoltage(node.getResponseBuffer(1), slave.bVoltage.pt, slave.bVoltage.divider);
    root["C_Voltage"] = calculateVoltage(node.getResponseBuffer(2), slave.cVoltage.pt, slave.cVoltage.divider);
    root["Phase_Voltage_Mean"] = calculateVoltage(node.getResponseBuffer(3), slave.phaseVoltageMean.pt, slave.phaseVoltageMean.divider);
    root["Zero_Sequence_Voltage"] = calculateVoltage(node.getResponseBuffer(4), slave.zeroSequenceVoltage.pt, slave.zeroSequenceVoltage.divider);
}

void processEnergyData(JsonObject& root, const SensorSlave& slave) {
    root["Total_Active_Energy"] = readEnergyValue(0, slave.totalActiveEnergy.divider);
    root["Import_Active_Energy"] = readEnergyValue(2, slave.importActiveEnergy.divider);
    root["Export_Active_Energy"] = readEnergyValue(4, slave.exportActiveEnergy.divider);
}

void publishData(const SensorSlave& slave, const JsonDocument& doc, bool success) {
    
    // Get timing data
    String sameDeviceDelta = getSameDeviceDelta(slave.id, slave.name.c_str(), false);
    getSameDeviceDelta(slave.id, slave.name.c_str(), true); // Reset for next
    
    unsigned long timeDelta = calculateTimeDelta(slave.id, slave.name.c_str());
    String formattedDelta = formatTimeDelta(timeDelta);
    
    // Publish message
    String output;
    serializeJson(doc, output);
    publishMessage(slave.mqttTopic.c_str(), output.c_str());
    
    if (debugEnabled) {
        addDebugMessage(slave.mqttTopic.c_str(), output.c_str(), formattedDelta.c_str(), sameDeviceDelta.c_str());
    }
    
}

// ==================== NON-BLOCKING QUERY STATE MACHINE ====================

bool startNonBlockingQuery() {
    if (currentSlaveIndex >= slaveCount) {
        return false;
    }
    
    SensorSlave& slave = slaves[currentSlaveIndex];
    
    // Re-initialize Modbus for this slave
    node.begin(slave.id, Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    node.clearResponseBuffer();
    node.clearTransmitBuffer();

    delay(300); // Stabilization delay

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
    
    bool success = false;

    // Common fields for all devices
    root["id"] = slave.id;
    root["name"] = slave.name;
    root["mqtt_topic"] = slave.mqttTopic;

    // Device-specific data processing
    if (slave.name.indexOf("Sensor") >= 0) {
        processSensorData(root, slave);
        success = true;
    } else if (slave.name.indexOf("Meter") >= 0) {
        processMeterData(root, slave);
        success = true;
    } else if (slave.name.indexOf("Voltage") >= 0) {
        processVoltageData(root, slave);
        success = true;
    } else if (slave.name.indexOf("Energy") >= 0) {
        processEnergyData(root, slave);
        success = true;
    } else {
        root["error"] = "Unknown device type";
    }
    
    // Publish results
    publishData(slave, doc, success);
    waitingForResponse = false;
}

void checkCycleCompletion() {
    if (currentSlaveIndex >= slaveCount) {
        unsigned long currentTime = millis();
        
        lastSequenceTime = currentTime;
        currentState = STATE_WAITING;
        lastActionTime = currentTime;
        
        // 🎯 ADD BATCH SEPARATOR
        addBatchSeparatorMessage();
        
        Serial.printf("🎉 Cycle complete - sequence time reset to: %lu\n", currentTime);
    } else {
        currentState = STATE_START_QUERY;
    }
}

void handleQueryStartFailure() {
    Serial.printf("❌ Failed to start query for slave %d\n", slaves[currentSlaveIndex].id);
    updateSlaveStatistic(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), false, false);

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["id"] = slaves[currentSlaveIndex].id;
    root["name"] = slaves[currentSlaveIndex].name;
    root["error"] = "Failed to start Modbus query";
    root["mqtt_topic"] = slaves[currentSlaveIndex].mqttTopic;

    publishData(slaves[currentSlaveIndex], doc, false);
}

void handleQueryTimeout() {
    Serial.printf("⏰ TIMEOUT on slave %d after %lu ms - SKIPPING TO NEXT!\n", 
                  slaves[currentSlaveIndex].id, timeoutDuration);

    updateSlaveStatistic(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), false, true);

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["id"] = slaves[currentSlaveIndex].id;
    root["name"] = slaves[currentSlaveIndex].name;
    root["error"] = "Modbus timeout - no response from device";
    root["mqtt_topic"] = slaves[currentSlaveIndex].mqttTopic;
    
    publishData(slaves[currentSlaveIndex], doc, false);
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
                Serial.printf("🔄 NEW CYCLE | Waited: %lums | Expected: %lums | Diff: %lums\n", 
                            currentTime - lastActionTime, pollInterval, 
                            (currentTime - lastActionTime) - pollInterval);
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
    timeoutDuration = newTimeoutSeconds * 1000; // Convert to milliseconds
    Serial.printf("⏱️  Timeout updated to: %d seconds (%lu ms)\n", newTimeoutSeconds, timeoutDuration);
}

void updatePollInterval(int newIntervalSeconds) {
    pollInterval = newIntervalSeconds * 1000; // Convert to milliseconds
    
    // If we're currently waiting, reset the timer
    if (currentState == STATE_WAITING) {
        lastActionTime = millis();
    }
    
    Serial.printf("🔄 Poll interval updated to: %d seconds (%lu ms)\n", newIntervalSeconds, pollInterval);
}

// ==================== STATISTICS MANAGEMENT ====================

void updateSlaveStatistic(uint8_t slaveId, const char* slaveName, bool success, bool timeout) {
    // Quick validation
    if (slaveId == 0 || slaveName == nullptr) return;
    
    // Find existing stats for this slave
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
            
            // Shift history right (oldest drops off)
            slaveStats[i].statusHistory[2] = slaveStats[i].statusHistory[1];
            slaveStats[i].statusHistory[1] = slaveStats[i].statusHistory[0];
            
            // Add new status at beginning (newest on left)
            if (success) slaveStats[i].statusHistory[0] = 'S';
            else if (timeout) slaveStats[i].statusHistory[0] = 'T';
            else slaveStats[i].statusHistory[0] = 'F';
            
            return;

        }
    }
    
    // Create new stats entry if not found and we have space
    if (slaveStatsCount < kMaxStatisticsSlaves) {
            SlaveStatistics* newStat = &slaveStats[slaveStatsCount];
            newStat->slaveId = slaveId;
            strncpy(newStat->slaveName, slaveName, sizeof(newStat->slaveName) - 1);
            newStat->slaveName[sizeof(newStat->slaveName) - 1] = '\0';
            newStat->totalQueries = 1;
            newStat->successCount = success ? 1 : 0;
            newStat->timeoutCount = timeout ? 1 : 0;
            newStat->failedCount = (!success && !timeout) ? 1 : 0;
            
            // ADD THIS: INITIALIZE STATUS HISTORY FOR NEW SLAVE
            strcpy(newStat->statusHistory, "   "); // Start with 3 spaces

            
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
            // Shift all subsequent elements left
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

// ==================== Debug Batch Seperator ====================

void addBatchSeparatorMessage() {
    if (!debugEnabled) return;
    
    JsonDocument doc;
    doc["type"] = "batch_separator";
    doc["message"] = "Query Loop Completed";
    
    String output;
    serializeJson(doc, output);
    
    addDebugMessage("BATCH", output.c_str(), "0", "0");
}
