#include "ModBusHandler.h"

#define MAX485_DE 5

ModbusMaster node;
SensorSlave* slaves = nullptr;
int slaveCount = 0;

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

bool modbus_begin() {
    // Initialize RS485 control pin
    pinMode(MAX485_DE, OUTPUT);
    digitalWrite(MAX485_DE, LOW);
    
    // Initialize LittleFS
    if (!LittleFS.begin()) {
        Serial.println("❌ LittleFS initialization failed!");
        return false;
    }
    
    // Load slaves from file
    File file = LittleFS.open("/slaves.json", "r");
    if (!file) {
        Serial.println("❌ Failed to open slaves.json");
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.print("❌ JSON deserialization failed: ");
        Serial.println(error.c_str());
        return false;
    }
    
    JsonArray slavesArray = doc.as<JsonArray>();
    slaveCount = slavesArray.size();
    slaves = new SensorSlave[slaveCount];
    
    for (int i = 0; i < slaveCount; i++) {
        JsonObject slaveObj = slavesArray[i];
        slaves[i].id = slaveObj["id"];
        slaves[i].startReg = slaveObj["startReg"]; 
        slaves[i].numRegs = slaveObj["numReg"];
        slaves[i].name = slaveObj["name"].as<String>();
        slaves[i].mqttTopic = slaveObj["mqttTopic"].as<String>();
        
        Serial.printf("✅ Loaded slave: ID=%d, Name=%s, StartReg=%d, NumRegs=%d, Topic=%s\n", 
                     slaves[i].id, slaves[i].name.c_str(), slaves[i].startReg, slaves[i].numRegs, slaves[i].mqttTopic.c_str());
    }
    
    // Initialize ModbusMaster
    node.begin(1, Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    
    Serial.println("✅ Modbus initialized");
    return true;
}

String modbus_readAllDataJSON() {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    
    for (int i = 0; i < slaveCount; i++) {
        SensorSlave slave = slaves[i];
        
        if (slave.name.indexOf("Sensor") >= 0) {
            // Read sensor data
            node.begin(slave.id, Serial);
            
            if (node.readHoldingRegisters(slave.startReg, slave.numRegs) == node.ku8MBSuccess) {
                JsonObject sensorObj = root[slave.name].to<JsonObject>();
                
                // Add device info
                sensorObj["id"] = slave.id;
                sensorObj["name"] = slave.name;
                sensorObj["type"] = "sensor";
                
                // Convert sensor data based on number of registers
                if (slave.numRegs >= 1) {
                    uint16_t rawTemp = node.getResponseBuffer(0);
                    sensorObj["temperature"] = convertRegisterToTemperature(rawTemp);
                }
                if (slave.numRegs >= 2) {
                    uint16_t rawHum = node.getResponseBuffer(1);
                    sensorObj["humidity"] = convertRegisterToHumidity(rawHum);
                }
                
                sensorObj["mqtt_topic"] = slave.mqttTopic;
            }
            
        } else if (slave.name.indexOf("Meter") >= 0 || slave.name.indexOf("meter") >= 0) {
            node.begin(slave.id, Serial);
            
            // Read BasicParams (20 registers)
            if (node.readHoldingRegisters(0, 20) == node.ku8MBSuccess) {
                // Read BasicVoltage (5 registers)
                if (node.readHoldingRegisters(20, 5) == node.ku8MBSuccess) {
                    JsonObject meterObj = root[slave.name].to<JsonObject>();
                    
                    // Add device info
                    meterObj["id"] = slave.id;
                    meterObj["name"] = slave.name;
                    meterObj["type"] = "meter";
                    
                    // Add BasicParams to JSON directly from registers
                    JsonObject basicParams = meterObj["BasicParams"].to<JsonObject>();
                    basicParams["Current1"] = node.getResponseBuffer(0);
                    basicParams["Current2"] = node.getResponseBuffer(1);
                    basicParams["Current3"] = node.getResponseBuffer(2);
                    basicParams["ZeroPhaseCurrent"] = node.getResponseBuffer(3);
                    basicParams["ActivePower1"] = (int16_t)node.getResponseBuffer(4);
                    basicParams["ActivePower2"] = (int16_t)node.getResponseBuffer(5);
                    basicParams["ActivePower3"] = (int16_t)node.getResponseBuffer(6);
                    basicParams["TTPActivePower"] = (int16_t)node.getResponseBuffer(7);
                    basicParams["ReactivePower1"] = (int16_t)node.getResponseBuffer(8);
                    basicParams["ReactivePower2"] = (int16_t)node.getResponseBuffer(9);
                    basicParams["ReactivePower3"] = (int16_t)node.getResponseBuffer(10);
                    basicParams["TTPReactivePower"] = (int16_t)node.getResponseBuffer(11);
                    basicParams["ApparentPower1"] = (int16_t)node.getResponseBuffer(12);
                    basicParams["ApparentPower2"] = (int16_t)node.getResponseBuffer(13);
                    basicParams["ApparentPower3"] = (int16_t)node.getResponseBuffer(14);
                    basicParams["TTPApparentPower"] = (int16_t)node.getResponseBuffer(15);
                    basicParams["PowerFactor1"] = (int16_t)node.getResponseBuffer(16);
                    basicParams["PowerFactor2"] = (int16_t)node.getResponseBuffer(17);
                    basicParams["PowerFactor3"] = (int16_t)node.getResponseBuffer(18);
                    basicParams["TTPPowerPhase"] = (int16_t)node.getResponseBuffer(19);
                    
                    // Add BasicVoltage to JSON directly from registers
                    JsonObject basicVoltage = meterObj["BasicVoltage"].to<JsonObject>();
                    basicVoltage["AVoltage"] = node.getResponseBuffer(0);
                    basicVoltage["BVoltage"] = node.getResponseBuffer(1);
                    basicVoltage["CVoltage"] = node.getResponseBuffer(2);
                    basicVoltage["VoltageMeanValue"] = node.getResponseBuffer(3);
                    basicVoltage["ZeroSeqVoltage"] = node.getResponseBuffer(4);
                    
                    meterObj["mqtt_topic"] = slave.mqttTopic;
                }
            }
            
        } else {
            // Handle other devices (apple, qew123, app, etc.)
            node.begin(slave.id, Serial);
            
            if (node.readHoldingRegisters(slave.startReg, slave.numRegs) == node.ku8MBSuccess) {
                JsonObject otherObj = root[slave.name].to<JsonObject>();
                
                // Add device info
                otherObj["id"] = slave.id;
                otherObj["name"] = slave.name;
                otherObj["type"] = "generic";
                otherObj["mqtt_topic"] = slave.mqttTopic;
                
                // Add raw register data for generic devices
                JsonArray rawData = otherObj.createNestedArray("raw_data");
                for (int j = 0; j < slave.numRegs; j++) {
                    rawData.add(node.getResponseBuffer(j));
                }
            }
        }
    }
    
    String output;
    serializeJsonPretty(doc, output);
    return output;
}

bool modbus_reloadSlaves() {
    Serial.println("🔄 Reloading slaves...");
    
    JsonDocument config;
    if (!loadSlaveConfig(config)) {
        Serial.println("❌ Failed to load config");
        return false;
    }
    
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
        slaves[i].numRegs = slaveObj["numReg"];
        slaves[i].name = slaveObj["name"].as<String>();
        slaves[i].mqttTopic = slaveObj["mqttTopic"].as<String>();
        
        Serial.printf("✅ Slave: ID=%d, Name=%s\n", slaves[i].id, slaves[i].name.c_str());
    }
    
    Serial.printf("✅ Reloaded %d slaves\n", slaveCount);
    return true;
}