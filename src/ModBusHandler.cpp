#include "ModBusHandler.h"

#define MAX485_DE 5

ModbusMaster node;
SensorSlave* slaves = nullptr;
int slaveCount = 0;

// Non-blocking query state
unsigned long lastQueryTime = 0;
uint8_t currentSlaveIndex = 0;
bool queryInProgress = false;

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
    
    if (!queryInProgress) {
        // Start new query cycle
        queryInProgress = true;
        currentSlaveIndex = 0;
        lastQueryTime = currentTime;
        Serial.println("🚀 Starting non-blocking query cycle");
    }
    
    // Check if it's time to query next slave
    if (currentTime - lastQueryTime >= QUERY_INTERVAL) {
        lastQueryTime = currentTime;
        
        // Query current slave & publish
        modbus_readAllDataJSON();
        
        // Move to next slave
        currentSlaveIndex++;
        
        // Check if cycle complete
        if (currentSlaveIndex >= slaveCount) {
            queryInProgress = false;
            Serial.println("🎉 Query cycle completed");
        }
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
                
                uint16_t rawTemp = node.getResponseBuffer(0);
                Obj["temperature"] = convertRegisterToTemperature(rawTemp);
                uint16_t rawHum = node.getResponseBuffer(1);
                Obj["humidity"] = convertRegisterToHumidity(rawHum);
                
                Obj["mqtt_topic"] = slave.mqttTopic;
            } else {
                Serial.println("Reading Sensor Failed");
            }  

            } else if (slave.name.indexOf("Meter") >= 0 || slave.name.indexOf("meter") >= 0) {

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
                    
                    
                    Obj["mqtt_topic"] = slave.mqttTopic;
                } else {
                    Serial.println("Reading Meter Failed");
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