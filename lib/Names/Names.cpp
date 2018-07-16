#include <Names.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <IntParsing.h>
#include <algorithm>



void Names::deserialize(Names& names, String json) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& parsedNames = jsonBuffer.parseObject(json);
  names.patch(parsedNames);
  Serial.println(F("deserialized names file successfully"));
  Serial.println(json);
}

void Names::patch(JsonObject& parsedNames) {
  if (parsedNames.success()) {
    this->setIfPresent<String>(parsedNames, "device_names", deviceNames);

    if (parsedNames.containsKey("device_names")) {
      this->deviceNames = parsedNames["device_names"].as<String>();
    }
  }
}

void Names::load(Names& names) {
  if (SPIFFS.exists(NAMES_FILE)) {
    File f = SPIFFS.open(NAMES_FILE, "r");
    String namesContents = f.readStringUntil(NAMES_TERMINATOR);
    f.close();
    Serial.println(F("Loaded names file successfully"));
    deserialize(names, namesContents);
    Serial.println(namesContents);
  } else {
    Serial.println(F("No names file in FS found!"));
    names.save();
  }
}

String Names::toJson(const bool prettyPrint) {
  String buffer = "";
  StringStream s(buffer);
  serialize(s, prettyPrint);
  return buffer;
}

void Names::save() {
  File f = SPIFFS.open(NAMES_FILE, "w");
  if (!f) {
    Serial.println(F("Opening names file failed"));
  } else {
    Serial.println(F("Saved names file successfully"));
    serialize(f);
    f.close();
  }
}

void Names::serialize(Stream& stream, const bool prettyPrint) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["device_names"] = this->deviceNames;
  if (prettyPrint) {
    root.prettyPrintTo(stream);
  } else {
    root.printTo(stream);
  }
}
