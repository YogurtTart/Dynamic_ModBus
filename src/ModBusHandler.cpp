#include "ModBusHandler.h"

#define MAX485_DE 5

ModbusMaster node;

SensorSlave* slaves = nullptr;
int slaveCount = 0;

// Non-blocking query state
unsigned long lastQueryTime = 0;
uint8_t currentSlaveIndex = 0;
unsigned long pollInterval = 10000;
unsigned long timeoutDuration = 1000;

// Non-blocking state variables
enum QueryState { 
    STATE_IDLE, 
    STATE_START_QUERY,      // Start query state
    STATE_WAIT_RESPONSE,    // Wait for response state
    STATE_PROCESS_DATA,     // Process received data state
    STATE_WAITING 
};
QueryState currentState = STATE_IDLE;

unsigned long lastActionTime = 0;
unsigned long queryStartTime = 0;
bool waitingForResponse = false;

#define QUERY_INTERVAL 200  // .2 seconds between slaves

SlaveStatistics slaveStats[MAX_STATISTICS_SLAVES];
uint8_t slaveStatsCount = 0;


void preTransmission() { 
    digitalWrite(MAX485_DE, HIGH); 
}


void postTransmission() { 
    digitalWrite(MAX485_DE, LOW); 
}

float convertRegisterToTemperature(uint16_t regVal, float divider = 1.0) {
    int16_t tempInt;
    if (regVal & 0x8000) 
        tempInt = -((0xFFFF - regVal) + 1);
    else 
        tempInt = regVal;
    return (tempInt * 0.1) / divider;
}

float convertRegisterToHumidity(uint16_t regVal, float divider = 1.0) {
    return (regVal * 0.1) / divider;
}

float convertRegisterToVoltage(uint16_t regVal, float pt, float divider = 1.0) {
    return (regVal * pt /100) / divider;
}

// Formula calculation functions
float calculateCurrent(uint16_t registerValue, float ct, float divider = 1.0) {
    return (registerValue * ct / 10000.0) / divider;
}

float calculateSinglePhasePower(uint16_t registerValue, float pt, float ct, float divider = 1.0) {
    return (registerValue * pt * ct / 100.0) / divider;
}

float calculateThreePhasePower(uint16_t registerValue, float pt, float ct, float divider = 1.0) {
    return (registerValue * pt * ct / 10.0) / divider;
}

float calculatePowerFactor(uint16_t registerValue, float divider = 1.0) {
    return (registerValue / 10000.0) / divider;
}

float calculateVoltage(uint16_t registerValue, float pt, float divider = 1.0) {
    return (registerValue * pt / 100.0) / divider;
}

bool initModbus() {
    // Initialize RS485 control pin
    pinMode(MAX485_DE, OUTPUT);
    digitalWrite(MAX485_DE, LOW);
    
    // Initialize ModbusMaster
    node.begin(1, Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    
    Serial.println("✅ Modbus initialized");
    return true;
}

// Start a non-blocking query
bool startNonBlockingQuery() {
    if (currentSlaveIndex >= slaveCount) return false;
    
    SensorSlave slave = slaves[currentSlaveIndex];
    
    node.begin(slave.id, Serial);

    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    node.clearResponseBuffer();
    node.clearTransmitBuffer();

    delay(300);

    uint8_t result = node.readHoldingRegisters(slave.startReg, slave.numReg);
    
    
    queryStartTime = millis();
    waitingForResponse = true;

    Serial.printf("➡️ Querying slave %d: %s\n", slave.id, slave.name.c_str());

    return (result == node.ku8MBSuccess);
}

// Process the received data
void processNonBlockingData() {
    SensorSlave slave = slaves[currentSlaveIndex];
    
    JsonDocument doc;
    doc.clear();

    JsonObject root = doc.to<JsonObject>();
    bool success = false;

    if (slave.name.indexOf("Sensor") >= 0) {
    JsonObject Obj = root[slave.name].to<JsonObject>();
    Obj["id"] = slave.id;
    Obj["name"] = slave.name;
    
    Obj["temperature"] = convertRegisterToTemperature(node.getResponseBuffer(0), slave.tempdivider);
    Obj["humidity"] = convertRegisterToHumidity(node.getResponseBuffer(1), slave.humiddivider);
    Obj["mqtt_topic"] = slave.mqttTopic;
    
    // Include divider in output for reference
    Obj["tempdivider"] = slave.tempdivider;
    Obj["humiddivider"] = slave.humiddivider;
    
    success = true;

} else if (slave.name.indexOf("Meter") >= 0) {
        JsonObject Obj = root[slave.name].to<JsonObject>();
        Obj["id"] = slave.id;
        Obj["name"] = slave.name;
        
        // Apply formulas using helper functions
        JsonObject trueValues = Obj.createNestedObject("TrueValues");
        
        // Current values
        trueValues["ACurrent"] = calculateCurrent(node.getResponseBuffer(0), slave.ACurrent.ct, slave.ACurrent.divider);
        trueValues["BCurrent"] = calculateCurrent(node.getResponseBuffer(1), slave.BCurrent.ct, slave.BCurrent.divider);
        trueValues["CCurrent"] = calculateCurrent(node.getResponseBuffer(2), slave.CCurrent.ct, slave.CCurrent.divider);
        trueValues["ZeroPhaseCurrent"] = calculateCurrent(node.getResponseBuffer(3), slave.ZeroPhaseCurrent.ct, slave.ZeroPhaseCurrent.divider);
        
        // Single phase active power
        trueValues["AActiveP"] = calculateSinglePhasePower(node.getResponseBuffer(4), slave.AActiveP.pt, slave.AActiveP.ct, slave.AActiveP.divider);
        trueValues["BActiveP"] = calculateSinglePhasePower(node.getResponseBuffer(5), slave.BActiveP.pt, slave.BActiveP.ct, slave.BActiveP.divider);
        trueValues["CActiveP"] = calculateSinglePhasePower(node.getResponseBuffer(6), slave.CActiveP.pt, slave.CActiveP.ct, slave.CActiveP.divider);
        
        // Three phase active power
        trueValues["Total3PActiveP"] = calculateThreePhasePower(node.getResponseBuffer(7), slave.Total3PActiveP.pt, slave.Total3PActiveP.ct, slave.Total3PActiveP.divider);
        
        // Single phase reactive power
        trueValues["AReactiveP"] = calculateSinglePhasePower(node.getResponseBuffer(8), slave.AReactiveP.pt, slave.AReactiveP.ct, slave.AReactiveP.divider);
        trueValues["BReactiveP"] = calculateSinglePhasePower(node.getResponseBuffer(9), slave.BReactiveP.pt, slave.BReactiveP.ct, slave.BReactiveP.divider);
        trueValues["CReactiveP"] = calculateSinglePhasePower(node.getResponseBuffer(10), slave.CReactiveP.pt, slave.CReactiveP.ct, slave.CReactiveP.divider);
        
        // Three phase reactive power
        trueValues["Total3PReactiveP"] = calculateThreePhasePower(node.getResponseBuffer(11), slave.Total3PReactiveP.pt, slave.Total3PReactiveP.ct, slave.Total3PReactiveP.divider);
        
        // Single phase apparent power
        trueValues["AApparentP"] = calculateSinglePhasePower(node.getResponseBuffer(12), slave.AApparentP.pt, slave.AApparentP.ct, slave.AApparentP.divider);
        trueValues["BApparentP"] = calculateSinglePhasePower(node.getResponseBuffer(13), slave.BApparentP.pt, slave.BApparentP.ct, slave.BApparentP.divider);
        trueValues["CApparentP"] = calculateSinglePhasePower(node.getResponseBuffer(14), slave.CApparentP.pt, slave.CApparentP.ct, slave.CApparentP.divider);
        
        // Three phase apparent power
        trueValues["Total3PApparentP"] = calculateThreePhasePower(node.getResponseBuffer(15), slave.Total3PApparentP.pt, slave.Total3PApparentP.ct, slave.Total3PApparentP.divider);
        
        // Power factor
        trueValues["APowerF"] = calculatePowerFactor(node.getResponseBuffer(16), slave.APowerF.divider);
        trueValues["BPowerF"] = calculatePowerFactor(node.getResponseBuffer(17), slave.BPowerF.divider);
        trueValues["CPowerF"] = calculatePowerFactor(node.getResponseBuffer(18), slave.CPowerF.divider);
        trueValues["Total3PPowerF"] = calculatePowerFactor(node.getResponseBuffer(19), slave.Total3PPowerF.divider);
        
        Obj["mqtt_topic"] = slave.mqttTopic;
        success = true;
    }
    else if (slave.name.indexOf("Voltage") >= 0) {
        
        // ✅ NEW: Voltage device processing
        JsonObject Obj = root[slave.name].to<JsonObject>();
        Obj["id"] = slave.id;
        Obj["name"] = slave.name;
        
        // Apply voltage formulas
        JsonObject trueValues = Obj.createNestedObject("TrueValues");
        
        // Voltage values (starting from register 0 for voltage devices)
        trueValues["AVoltage"] = calculateVoltage(node.getResponseBuffer(0), slave.AVoltage.pt, slave.AVoltage.divider);
        trueValues["BVoltage"] = calculateVoltage(node.getResponseBuffer(1), slave.BVoltage.pt, slave.BVoltage.divider);
        trueValues["CVoltage"] = calculateVoltage(node.getResponseBuffer(2), slave.CVoltage.pt, slave.CVoltage.divider);
        trueValues["PhaseVoltageMean"] = calculateVoltage(node.getResponseBuffer(3), slave.PhaseVoltageMean.pt, slave.PhaseVoltageMean.divider);
        trueValues["ZeroSequenceVoltage"] = calculateVoltage(node.getResponseBuffer(4), slave.ZeroSequenceVoltage.pt, slave.ZeroSequenceVoltage.divider);
        
        Obj["mqtt_topic"] = slave.mqttTopic;
        success = true;
    
    } else {
        // Unknown devices
        JsonObject Obj = root[slave.name].to<JsonObject>();
        Obj["id"] = slave.id;
        Obj["name"] = slave.name;
        Obj["type"] = "unknown";
        Obj["mqtt_topic"] = "Lora/NameError";
        slave.mqttTopic = "Lora/NameError";

        // Add raw register data since we don't know the format
        JsonArray rawData = Obj.createNestedArray("raw_data");
        for (int j = 0; j < slave.numReg; j++) {
            rawData.add(node.getResponseBuffer(j));
        }
        
        success = true;
        Serial.printf("✅ Unknown device %d: %d registers read\n", slave.id, slave.numReg);
    }
    
    // Publish if successful
    if (success) {
        String output;
        serializeJson(doc, output);    
        publishMessage(slave.mqttTopic.c_str(), output.c_str());
    } else {
        Serial.printf("❌ No valid data from slave %d\n", slave.id);
        String output = "Failed";
        String topic = "Lora/error";
        publishMessage(topic.c_str(), output.c_str());
    }
    
    waitingForResponse = false;
}

// Check if we should move to next cycle
void checkCycleCompletion() {
    if (currentSlaveIndex >= slaveCount) {
        currentState = STATE_WAITING;
        Serial.printf("🎉 Query cycle completed, waiting %lu ms\n", pollInterval);
    } else {
        currentState = STATE_START_QUERY;
    }
}

// Non-blocking state machine
void updateNonBlockingQuery() {
    unsigned long currentTime = millis();
    
    switch (currentState) {
        case STATE_IDLE:
            // Start first query cycle
            currentState = STATE_START_QUERY;
            currentSlaveIndex = 0;
            lastActionTime = currentTime;
            waitingForResponse = false;
            Serial.println("🚀 Starting NON-BLOCKING query cycle");
            break;
            
        case STATE_START_QUERY:
            if (currentTime - lastActionTime >= QUERY_INTERVAL) {
                lastActionTime = currentTime;
                
                // Start querying current slave
                if (startNonBlockingQuery()) {
                    currentState = STATE_WAIT_RESPONSE;
                    Serial.printf("⏳ Waiting for slave %d response...\n", slaves[currentSlaveIndex].id);
                } else {
                    // Failed to start query, move to next slave
                    Serial.printf("❌ Failed to start query for slave %d\n", slaves[currentSlaveIndex].id);

                    updateSlaveStatistic(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), false, false);

                    JsonDocument doc;
                    doc["id"] = slaves[currentSlaveIndex].id;
                    doc["name"] = slaves[currentSlaveIndex].name;
                    doc["error"] = "Failed to start Modbus query";
                    
                    String output;
                    serializeJson(doc, output);
                    publishMessage("Lora/error", output.c_str()); //!!!!!!!!!!!!!!!!!!!!!!!!!! Change topic here for failed query starts
                    

                    currentSlaveIndex++;
                    checkCycleCompletion();
                }
            }
            break;
            
        case STATE_WAIT_RESPONSE:
            // Check for timeout OR response
            if (currentTime - queryStartTime > timeoutDuration) {
                // ✅ ACTUAL TIMEOUT - SKIP THIS SLAVE!
                Serial.printf("⏰ TIMEOUT on slave %d after %lu ms - SKIPPING TO NEXT!\n", 
                             slaves[currentSlaveIndex].id, timeoutDuration);

                updateSlaveStatistic(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), false, true);
        
                waitingForResponse = false;
                currentSlaveIndex++;
                checkCycleCompletion();
            }
            else if (node.getResponseBuffer(0) != 0xFFFF) {  // Check if Modbus transaction is complete by checking response buffer , If we have data in the first buffer, the transaction is complete
                // Response received successfully
                currentState = STATE_PROCESS_DATA;
            }
            break;
            
        case STATE_PROCESS_DATA:
            // Process the received data
            processNonBlockingData();
            updateSlaveStatistic(slaves[currentSlaveIndex].id, slaves[currentSlaveIndex].name.c_str(), true, false);

            currentSlaveIndex++;
            checkCycleCompletion();
            break;
            
        case STATE_WAITING:
            if (currentTime - lastActionTime >= pollInterval) {
                // Poll interval elapsed, start new cycle
                currentState = STATE_START_QUERY;
                currentSlaveIndex = 0;
                lastActionTime = currentTime;
                waitingForResponse = false;
                Serial.println("🔄 Starting new NON-BLOCKING query cycle");
            }
            break;
    }
}

bool modbus_reloadSlaves() {
    Serial.println("🔄 Reloading slaves...");
    
    JsonDocument config;
    if (!loadSlaveConfig(config)) {
        Serial.println("❌ Failed to load config");
        return false;
    }
    
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
    
    // Allocate new memory and INITIALIZE TO ZERO
    slaves = new SensorSlave[newSlaveCount](); // <-- ADD PARENTHESES TO INITIALIZE TO ZERO
    slaveCount = newSlaveCount;
    
    for (int i = 0; i < slaveCount; i++) {
        JsonObject slaveObj = slavesArray[i];
        
        // Initialize the slave struct to zero first
        memset(&slaves[i], 0, sizeof(SensorSlave)); // <-- ADD THIS LINE
        
        // Load basic fields
        slaves[i].id = slaveObj["id"];
        slaves[i].startReg = slaveObj["startReg"]; 
        slaves[i].numReg = slaveObj["numReg"];
        slaves[i].name = slaveObj["name"].as<String>();
        slaves[i].mqttTopic = slaveObj["mqttTopic"].as<String>();
        
        // ✅ CRITICAL FIX: Load divider values for Sensor devices
        if (slaves[i].name.indexOf("Sensor") >= 0) {
            slaves[i].tempdivider = slaveObj["tempdivider"] | 1.0f;  // Default to 1.0 if not found
            slaves[i].humiddivider = slaveObj["humiddivider"] | 1.0f; // Default to 1.0 if not found
        }
        
        // ✅ CRITICAL FIX: Load meter parameters for Meter devices
        else if (slaves[i].name.indexOf("Meter") >= 0) {
            // Load CT/PT/divider values for each meter parameter
            #define LOAD_METER_PARAM(field) \
                slaves[i].field.ct = slaveObj[#field]["ct"] | 1.0f; \
                slaves[i].field.pt = slaveObj[#field]["pt"] | 1.0f; \
                slaves[i].field.divider = slaveObj[#field]["divider"] | 1.0f;
            
            LOAD_METER_PARAM(ACurrent);
            LOAD_METER_PARAM(BCurrent);
            LOAD_METER_PARAM(CCurrent);
            LOAD_METER_PARAM(ZeroPhaseCurrent);
            LOAD_METER_PARAM(AActiveP);
            LOAD_METER_PARAM(BActiveP);
            LOAD_METER_PARAM(CActiveP);
            LOAD_METER_PARAM(Total3PActiveP);
            LOAD_METER_PARAM(AReactiveP);
            LOAD_METER_PARAM(BReactiveP);
            LOAD_METER_PARAM(CReactiveP);
            LOAD_METER_PARAM(Total3PReactiveP);
            LOAD_METER_PARAM(AApparentP);
            LOAD_METER_PARAM(BApparentP);
            LOAD_METER_PARAM(CApparentP);
            LOAD_METER_PARAM(Total3PApparentP);
            LOAD_METER_PARAM(APowerF);
            LOAD_METER_PARAM(BPowerF);
            LOAD_METER_PARAM(CPowerF);
            LOAD_METER_PARAM(Total3PPowerF);
            
            #undef LOAD_METER_PARAM
        } else if (slaves[i].name.indexOf("Voltage") >= 0) {
            // Load PT/divider values for each voltage parameter
            #define LOAD_VOLTAGE_PARAM(field) \
                if (slaveObj.containsKey(#field)) { \
                    slaves[i].field.pt = slaveObj[#field]["pt"] | 1.0f; \
                    slaves[i].field.divider = slaveObj[#field]["divider"] | 1.0f; \
                } else { \
                    slaves[i].field.pt = 1.0f; \
                    slaves[i].field.divider = 1.0f; \
                }
            
            LOAD_VOLTAGE_PARAM(AVoltage);
            LOAD_VOLTAGE_PARAM(BVoltage);
            LOAD_VOLTAGE_PARAM(CVoltage);
            LOAD_VOLTAGE_PARAM(PhaseVoltageMean);
            LOAD_VOLTAGE_PARAM(ZeroSequenceVoltage);
            
            #undef LOAD_VOLTAGE_PARAM
            
            Serial.printf("✅ Voltage Slave: ID=%d, Name=%s\n", 
                         slaves[i].id, slaves[i].name.c_str());
        }
        
        Serial.printf("✅ Slave: ID=%d, Name=%s, TempDiv=%.1f, HumidDiv=%.1f\n", 
                     slaves[i].id, slaves[i].name.c_str(), 
                     slaves[i].tempdivider, slaves[i].humiddivider);
    }
    
    Serial.printf("✅ Reloaded %d slaves\n", slaveCount);
    return true;
}

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

void updateSlaveStatistic(uint8_t slaveId, const char* slaveName, bool success, bool timeout) {
    // Quick validation
    if (slaveId == 0 || slaveName == NULL) return;
    
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
            return;
        }
    }
    
    // Create new stats entry if not found and we have space
    if (slaveStatsCount < MAX_STATISTICS_SLAVES) {
        SlaveStatistics* newStat = &slaveStats[slaveStatsCount];
        newStat->slaveId = slaveId;
        strncpy(newStat->slaveName, slaveName, sizeof(newStat->slaveName) - 1);
        newStat->slaveName[sizeof(newStat->slaveName) - 1] = '\0';
        newStat->totalQueries = 1;
        newStat->successCount = success ? 1 : 0;
        newStat->timeoutCount = timeout ? 1 : 0;
        newStat->failedCount = (!success && !timeout) ? 1 : 0;
        slaveStatsCount++;
    }
}

String getStatisticsJSON() {
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
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

void removeSlaveStatistic(uint8_t slaveId, const char* slaveName) {
    // Quick validation
    if (slaveId == 0 || slaveName == NULL) return;
    
    // Find and remove the statistic
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

void clearJsonDocument(JsonDocument& doc) {
    doc.clear();
}