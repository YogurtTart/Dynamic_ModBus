#include "ModBusHandler.h"

#define MAX485_DE 5

ModbusMaster node;
VoltageData currentVoltageData;

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

#define QUERY_INTERVAL 500  // .5 seconds between slaves


void preTransmission() { 
    digitalWrite(MAX485_DE, HIGH); 
}


void postTransmission() { 
    digitalWrite(MAX485_DE, LOW); 
}

float convertRegisterToTemperature(uint16_t regVal) {
        int16_t tempInt;
    if (regVal & 0x8000) 
        tempInt = -((0xFFFF - regVal) + 1);
    else 
        tempInt = regVal;
    return tempInt * 0.1;
}

float convertRegisterToHumidity(uint16_t regVal) {
    return regVal * 0.1;
}

float convertRegisterToVoltage(uint16_t regVal) {
    return regVal * 0.1;
}

bool hasVoltageData() {
    return currentVoltageData.hasData;
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
    JsonObject root = doc.to<JsonObject>();
    bool success = false;
    
    // Check if the query was successful by examining response code

            if (slave.name.indexOf("Sensor") >= 0) {
                JsonObject Obj = root[slave.name].to<JsonObject>();
                Obj["id"] = slave.id;
                Obj["name"] = slave.name;
                Obj["temperature"] = convertRegisterToTemperature(node.getResponseBuffer(0));
                Obj["humidity"] = convertRegisterToHumidity(node.getResponseBuffer(1));
                Obj["mqtt_topic"] = slave.mqttTopic;
                success = true;
                Serial.printf("✅ Sensor %d: %.1f°C, %.1f%%\n", slave.id, 
                             convertRegisterToTemperature(node.getResponseBuffer(0)),
                             convertRegisterToHumidity(node.getResponseBuffer(1)));
            } 
            else if (slave.name.indexOf("Meter") >= 0) {
                JsonObject Obj = root[slave.name].to<JsonObject>();
                Obj["id"] = slave.id;
                Obj["name"] = slave.name;
                
                JsonObject basicParams = Obj.createNestedObject("BasicParams");
                basicParams["Current1"] = node.getResponseBuffer(0);
                basicParams["Current2"] = node.getResponseBuffer(1);
                basicParams["Current3"] = node.getResponseBuffer(2);
                basicParams["ZeroPhaseCurrent"] = node.getResponseBuffer(3);
                basicParams["ActiveP1"] = node.getResponseBuffer(4);
                basicParams["ActiveP2"] = node.getResponseBuffer(5);
                basicParams["ActiveP3"] = node.getResponseBuffer(6);
                basicParams["3PhaseActiveP"] = node.getResponseBuffer(7);
                basicParams["ReactiveP1"] = node.getResponseBuffer(8);
                basicParams["ReactiveP2"] = node.getResponseBuffer(9);
                basicParams["ReactiveP3"] = node.getResponseBuffer(10);
                basicParams["3PhaseReactiveP"] = node.getResponseBuffer(11);
                basicParams["ApparentP1"] = node.getResponseBuffer(12);
                basicParams["ApparentP2"] = node.getResponseBuffer(13);
                basicParams["ApparentP3"] = node.getResponseBuffer(14);
                basicParams["3PhaseApparentP"] = node.getResponseBuffer(15);
                basicParams["PowerF1"] = node.getResponseBuffer(16);
                basicParams["PowerF2"] = node.getResponseBuffer(17);
                basicParams["PowerF3"] = node.getResponseBuffer(18);
                basicParams["3PhasePowerF"] = node.getResponseBuffer(19);
                
                if (hasVoltageData()) {
                    JsonObject voltageParams = Obj.createNestedObject("Voltage");
                    voltageParams["PhaseA"] = currentVoltageData.phaseA;
                    voltageParams["PhaseB"] = currentVoltageData.phaseB;
                    voltageParams["PhaseC"] = currentVoltageData.phaseC;
                    voltageParams["PhaseVoltageMean"] = currentVoltageData.phaseVoltageMean;
                    voltageParams["ZeroSequenceVoltage"] = currentVoltageData.zeroSequenceVoltage;
                    
                    Serial.printf("✅ Meter %s with Voltage: I1=%d, V=%.1f/%.1f/%.1f\n", 
                                slave.name.c_str(),
                                node.getResponseBuffer(0),
                                currentVoltageData.phaseA, currentVoltageData.phaseB, currentVoltageData.phaseC);
                } else {
                    Serial.printf("✅ Meter %s: I1=%d (No voltage data yet)\n", 
                                slave.name.c_str(), node.getResponseBuffer(0));
                }
                
                Obj["mqtt_topic"] = slave.mqttTopic;
                success = true;
            }
            else if (slave.name.indexOf("Voltage") >= 0) {
                // Store voltage data globally
                currentVoltageData.phaseA = convertRegisterToVoltage(node.getResponseBuffer(0));
                currentVoltageData.phaseB = convertRegisterToVoltage(node.getResponseBuffer(1));
                currentVoltageData.phaseC = convertRegisterToVoltage(node.getResponseBuffer(2));
                currentVoltageData.phaseVoltageMean = convertRegisterToVoltage(node.getResponseBuffer(3));
                currentVoltageData.zeroSequenceVoltage = convertRegisterToVoltage(node.getResponseBuffer(4));
                currentVoltageData.hasData = true;
                
                JsonObject Obj = root[slave.name].to<JsonObject>();
                Obj["id"] = slave.id;
                Obj["name"] = slave.name;
                
                JsonObject basicParams = Obj.createNestedObject("BasicParams");
                basicParams["PhaseA"] = currentVoltageData.phaseA;
                basicParams["PhaseB"] = currentVoltageData.phaseB;
                basicParams["PhaseC"] = currentVoltageData.phaseC;
                basicParams["PhaseVoltageMean"] = currentVoltageData.phaseVoltageMean;
                basicParams["ZeroSequenceVoltage"] = currentVoltageData.zeroSequenceVoltage;
                
                Obj["mqtt_topic"] = slave.mqttTopic;
                success = true;
                Serial.printf("✅ Voltage stored: A=%.1fV, B=%.1fV, C=%.1fV\n", 
                            currentVoltageData.phaseA, currentVoltageData.phaseB, currentVoltageData.phaseC);
            }
            else {
                // Unknown devices
                JsonObject Obj = root[slave.name].to<JsonObject>();
                Obj["id"] = slave.id;
                Obj["name"] = slave.name;
                Obj["type"] = "unknown";
                Obj["mqtt_topic"] = "Lora/error";

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
    
    // Allocate new memory
    slaves = new SensorSlave[newSlaveCount];
    slaveCount = newSlaveCount;
    
    for (int i = 0; i < slaveCount; i++) {
        JsonObject slaveObj = slavesArray[i];
        slaves[i].id = slaveObj["id"];
        slaves[i].startReg = slaveObj["startReg"]; 
        slaves[i].numReg = slaveObj["numReg"];
        slaves[i].name = slaveObj["name"].as<String>();
        slaves[i].mqttTopic = slaveObj["mqttTopic"].as<String>();
        
        Serial.printf("✅ Slave: ID=%d, Name=%s\n", slaves[i].id, slaves[i].name.c_str());
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