#include <Arduino.h>
#include <StringStream.h>
#include <ArduinoJson.h>
#include <Size.h>

#ifndef _DEVICENAMES_H_INCLUDED
#define _DEVICENAMES_H_INCLUDED


#define NAMES_FILE  "/names.json"
#define NAMES_TERMINATOR '\0'

class Names {
public:
  Names() :
    deviceNames("")
{}

  static void deserialize(Names& names, String json);
  static void load(Names& names);
  void save();
  String toJson(const bool prettyPrint = true);
  void serialize(Stream& stream, const bool prettyPrint = false);
  void patch(JsonObject& obj);

  String deviceNames;

  template <typename T>
  void setIfPresent(JsonObject& obj, const char* key, T& var) {
    if (obj.containsKey(key)) {
      var = obj.get<T>(key);
    }
  }
};
#endif
