#include <Sensors.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <IntParsing.h>
#include <algorithm>



void Sensors::deserialize(Sensors& sensors, String json) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& parsedSensors = jsonBuffer.parseObject(json);
  sensors.patch(parsedSensors);
  Serial.println(F("deserialized sensors file successfully"));
  Serial.println(json);

}


void Sensors::patch(JsonObject& parsedSensors) {
  if (parsedSensors.success()) {
    this->setIfPresent(parsedSensors, "sensor_si7021", sensorSI7021);
    this->setIfPresent(parsedSensors, "sensor_bme280", sensorBME280);
    this->setIfPresent(parsedSensors, "sensor_bh1750", sensorBH1750);
    this->setIfPresent(parsedSensors, "sensor_pins", sensorPins);
    this->setIfPresent(parsedSensors, "sensor_si7021_hum", sensorSI7021hum);
    this->setIfPresent(parsedSensors, "sensor_si7021_temp", sensorSI7021temp);
    this->setIfPresent(parsedSensors, "sensor_bme280_pres", sensorBME280pres);
    this->setIfPresent(parsedSensors, "sensor_bme280_hum", sensorBME280hum);
    this->setIfPresent(parsedSensors, "sensor_bme280_temp", sensorBME280temp);
    this->setIfPresent(parsedSensors, "sensor_bh1750_lux", sensorBH1750lux);
    this->setIfPresent(parsedSensors, "sensor_pin_states",  sensorPinStates);
  }
}

void Sensors::load(Sensors& sensors) {
  if (SPIFFS.exists(SENSORS_FILE)) {
    File f = SPIFFS.open(SENSORS_FILE, "r");
    String sensorsContents = f.readStringUntil(SENSORS_TERMINATOR);
    f.close();
    Serial.println(F("Loaded sensors file successfully"));
    deserialize(sensors, sensorsContents);
  } else {
    Serial.println(F("No sensors file in FS found!"));
    sensors.save();
  }
}

String Sensors::toJson(const bool prettyPrint) {
  String buffer = "";
  StringStream s(buffer);
  serialize(s, prettyPrint);
  return buffer;
}

void Sensors::save() {
  File f = SPIFFS.open(SENSORS_FILE, "w");
  if (!f) {
    Serial.println(F("Opening sensors file failed"));
  } else {
    Serial.println(F("Saved sensors file successfully"));
    serialize(f);
    f.close();
  }
}

void Sensors::serialize(Stream& stream, const bool prettyPrint) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();

  root["sensor_si7021"] = this->sensorSI7021;
  root["sensor_bme280"] = this->sensorBME280;
  root["sensor_bh1750"] = this->sensorBH1750;
  root["sensor_pins"] = this->sensorPins;

  root["sensor_si7021_hum"] = this->sensorSI7021hum;
  root["sensor_si7021_temp"] = this->sensorSI7021temp;
  root["sensor_bme280_pres"] = this->sensorBME280pres;
  root["sensor_bme280_hum"] = this->sensorBME280hum;
  root["sensor_bme280_temp"] = this->sensorBME280temp;
  root["sensor_bh1750_lux"] = this->sensorBH1750lux;
  root["sensor_pin_states"] = this->sensorPinStates;


    if (prettyPrint) {
      root.prettyPrintTo(stream);
    } else {
      root.printTo(stream);
    }
  Serial.println(F("serialized sensors file successfully"));
  }
