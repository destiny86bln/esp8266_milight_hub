#include <Arduino.h>
#include <StringStream.h>
#include <ArduinoJson.h>
#include <Size.h>

#ifndef _SENSORS_H_INCLUDED
#define _SENSORS_H_INCLUDED


#define SENSORS_FILE  "/sensors.json"
#define SENSORS_TERMINATOR '\0'

#define WEB_INDEX_FILENAME "/web/index.html"


class Sensors {
public:
  Sensors() :
    sensorSI7021(false),
    sensorBME280(false),
    sensorBH1750(false),
    sensorSI7021hum(""),
    sensorSI7021temp(""),
    sensorBME280pres(""),
    sensorBME280hum(""),
    sensorBME280temp(""),
    sensorBH1750lux(""),
    sensorPinStates(""),
    sensorPins(0)
  { }


  static void deserialize(Sensors& sensors, String json);
  static void load(Sensors& sensors);
  void save();
  String toJson(const bool prettyPrint = true);
  void serialize(Stream& stream, const bool prettyPrint = false);
  void patch(JsonObject& obj);

  bool sensorSI7021;
  bool sensorBME280;
  bool sensorBH1750;

  String sensorSI7021hum;
  String sensorSI7021temp;
  String sensorBME280pres;
  String sensorBME280hum;
  String sensorBME280temp;
  String sensorBH1750lux;
  String sensorPinStates;
  uint8_t sensorPins;

  template <typename T>
  void setIfPresent(JsonObject& obj, const char* key, T& var) {
    if (obj.containsKey(key)) {
      var = obj.get<T>(key);
    }
  }
};

#endif
