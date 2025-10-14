#include "ModBusHandler.h"

#define MAX485_DE 5

ModbusMaster node;
VoltageData currentVoltageData;

SensorSlave* slaves = nullptr;
int slaveCount = 0;

// Non-blocking query state
unsigned long lastQueryTime = 0;
uint8_t currentSlaveIndex = 0;
bool queryInProgress = false;
unsigned long pollInterval = 10000;

enum QueryState { 
    STATE_IDLE, 
    STATE_QUERYING, 
    STATE_WAITING 
};
QueryState currentState = STATE_IDLE;

unsigned long lastActionTime = 0;

#define QUERY_INTERVAL 500  // .5 seconds between slaves

// RS485 control
void preTransmission() { 
    digitalWrite(MAX485_DE, HIGH); 
}

void postTransmission() { 
    digitalWrite(MAX485_DE, LOW); 
}

// Convert register to temperature
float convertRegisterToTemperature(uint16_t regVal) {
    int16_t tempInt;
    if (regVal & 0x8000) 
        tempInt = -((0xFFFF - regVal) + 1);
    else 
        tempInt = regVal;
    return tempInt * 0.1;
}

// Convert register to humidity
float convertRegisterToHumidity(uint16_t regVal) {
    return regVal * 0.1;
}

// Convert register to Voltage
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

// Update non-blocking query (call this in loop())
void updateNonBlockingQuery() {
    unsigned long currentTime = millis();
    
     switch (currentState) {
        case STATE_IDLE:
            // Start first query cycle
            currentState = STATE_QUERYING;
            currentSlaveIndex = 0;
            lastActionTime = currentTime;
            Serial.println("🚀 Starting query cycle");
            break;
            
        case STATE_QUERYING:
            if (currentTime - lastActionTime >= QUERY_INTERVAL) {
                lastActionTime = currentTime;
                
                // Query current slave
                modbus_readAllDataJSON();
                currentSlaveIndex++;
                
                if (currentSlaveIndex >= slaveCount) {
                    // All slaves queried, wait for poll interval
                    currentState = STATE_WAITING;
                    Serial.printf("🎉 Query cycle completed, waiting %lu ms\n", pollInterval);
                }
            }
            break;
            
        case STATE_WAITING:
            if (currentTime - lastActionTime >= pollInterval) {
                // Poll interval elapsed, start new cycle
                currentState = STATE_QUERYING;
                currentSlaveIndex = 0;
                lastActionTime = currentTime;
                Serial.println("🔄 Starting new query cycle");
            }
            break;
    }
}

String modbus_readAllDataJSON() {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    
    SensorSlave slave = slaves[currentSlaveIndex];
    
    if (slave.name.indexOf("Sensor") >= 0) {
        // Read sensor data
        node.begin(slave.id, Serial);
        
        if (node.readHoldingRegisters(slave.startReg, slave.numReg) == node.ku8MBSuccess) {
            JsonObject Obj = root[slave.name].to<JsonObject>();
            
            Obj["id"] = slave.id;
            Obj["name"] = slave.name;
            
            Obj["temperature"] = convertRegisterToTemperature(node.getResponseBuffer(0));
            Obj["humidity"] = convertRegisterToHumidity(node.getResponseBuffer(1));
            
            Obj["mqtt_topic"] = slave.mqttTopic;
        } else {
            Serial.println("Reading Sensor Failed");
        }  

    } else if (slave.name.indexOf("Meter") >= 0) {
        node.begin(slave.id, Serial);
        
        if (node.readHoldingRegisters(slave.startReg, slave.numReg) == node.ku8MBSuccess) {
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
            
            // ✅ ADD: Include voltage data if available
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
            
        } else {
            Serial.println("❌ Reading Meter Failed");
        }  
        
    } else if (slave.name.indexOf("Voltage") >= 0) {
        node.begin(slave.id, Serial);
        
        if (node.readHoldingRegisters(slave.startReg, slave.numReg) == node.ku8MBSuccess) {
            // ✅ STORE voltage data globally
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
            
            Serial.printf("✅ Voltage stored: A=%.1fV, B=%.1fV, C=%.1fV\n", 
                        currentVoltageData.phaseA, currentVoltageData.phaseB, currentVoltageData.phaseC);
                        
        } else {
            Serial.println("❌ Reading Voltage Failed");
        }  
        
    } else {
        // Handle unknown/other devices
        node.begin(slave.id, Serial);
        
        if (node.readHoldingRegisters(slave.startReg, slave.numReg) == node.ku8MBSuccess) {
            JsonObject Obj = root[slave.name].to<JsonObject>();
            
            Obj["id"] = slave.id;
            Obj["name"] = slave.name;
            Obj["type"] = "unknown";  // ✅ Mark as unknown type
            Obj["mqtt_topic"] = slave.mqttTopic;

            slave.mqttTopic = "Lora/Error";
            
            // Add raw register data since we don't know the format
            JsonArray rawData = Obj.createNestedArray("raw_data");
            for (int j = 0; j < slave.numReg; j++) {
                rawData.add(node.getResponseBuffer(j));
            }
            
            Serial.printf("⚠️ Unknown device type: %s, returning raw data\n", slave.name.c_str());
            
        } else {
            Serial.println("Reading Unknown Device Failed");
        }
    }
    
    String output;
    serializeJson(doc, output);    

    publishMessage(slave.mqttTopic.c_str(), output.c_str());

    return output;
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
    
    // Reset query state when reloading slaves
    currentState = STATE_IDLE;
    currentSlaveIndex = 0;

    // Get the slaves array
    JsonArray slavesArray = config["slaves"];
    int newSlaveCount = slavesArray.size();
    
    // ✅ CRITICAL FIX: Free old memory before allocating new
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

void updatePollInterval(int newIntervalSeconds) {
    pollInterval = newIntervalSeconds * 1000; // Convert to milliseconds
    
    // If we're currently waiting, reset the timer
    if (currentState == STATE_WAITING) {
        lastActionTime = millis();
    }
    
    Serial.printf("🔄 Poll interval updated to: %d seconds (%lu ms)\n", newIntervalSeconds, pollInterval);
}