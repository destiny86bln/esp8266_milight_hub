#ifndef UNIT_TEST

#include <SPI.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <stdlib.h>
#include <FS.h>
#include <IntParsing.h>
#include <Size.h>
#include <LinkedList.h>
#include <GroupStateStore.h>
#include <MiLightRadioConfig.h>
#include <MiLightRemoteConfig.h>
#include <MiLightHttpServer.h>
#include <Settings.h>
#include <Sensors.h>
#include <Names.h>

#include <MiLightUdpServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266SSDP.h>
#include <MqttClient.h>
#include <RGBConverter.h>
#include <MiLightDiscoveryServer.h>
#include <MiLightClient.h>
#include <BulbStateUpdater.h>
#include <LEDStatus.h>
//Some More includes for the aditional features
#include <ArduinoOTA.h>
#include <EEPROM.h>

#include <BME280I2C.h>
#include <AS_BH1750.h>
#include <si7021.h>
#include <Wire.h>
#include <Timer.h>


//Some aditional inits
BME280I2C::Settings bme_settings(
BME280::OSR_X1,
BME280::OSR_X1,
BME280::OSR_X1,
BME280::Mode_Forced,
BME280::StandbyTime_1000ms,
BME280::Filter_Off,
BME280::SpiEnable_False,
0x76);
BME280I2C bme280(bme_settings);
AS_BH1750 bh1750;
float temp(NAN), hum(NAN), pres(NAN);
int read_i2c , timerI2cId = 0 ;
int pinState[5] = { -3 , -3 , -3 , -3 , -3 };
uint8_t aktPin[5] ;
SI7021 si7021;
Timer tmr;


WiFiManager wifiManager;

static LEDStatus *ledStatus;

Settings settings;
Sensors sensors;
Names names;

MiLightClient* milightClient = NULL;
MiLightRadioFactory* radioFactory = NULL;
MiLightHttpServer *httpServer = NULL;
MqttClient* mqttClient = NULL;
MiLightDiscoveryServer* discoveryServer = NULL;
uint8_t currentRadioType = 0;

// For tracking and managing group state
GroupStateStore* stateStore = NULL;
BulbStateUpdater* bulbStateUpdater = NULL;

int numUdpServers = 0;
MiLightUdpServer** udpServers = NULL;
WiFiUDP udpSeder;

//setting pins for read use
void pinInit() {
  if(settings.mqttPin1 > 0 || settings.mqttPin2 > 0 || settings.mqttPin3 > 0 || settings.mqttPin4 > 0 ){
    pinState[0] = 1;

    aktPin[1] = settings.mqttPin1 ;
    aktPin[2] = settings.mqttPin2 ;
    aktPin[3] = settings.mqttPin3 ;
    aktPin[4] = settings.mqttPin4 ;

    sensors.sensorPins = 0;
    for( uint8_t i = 1 ; i <= 4 ; i++){
      Serial.print("MQTT Pin ");
      Serial.print(i);
      Serial.print(" : ");
      Serial.print(aktPin[i]);
      if(aktPin[i] > 0 ){
        pinMode(aktPin[i], INPUT);
        Serial.println(" > activated");
        pinState[i] = -1 ;
        sensors.sensorPins++;
      }else{
        Serial.println(" > inactiv");
        pinState[i] = -2 ;
      }
    }
  }else{
      Serial.println("No Pins for transmitting enabled");
  }
}



/**
 * Set up UDP servers (both v5 and v6).  Clean up old ones if necessary.
 */
void initMilightUdpServers() {
  if (udpServers) {
    for (int i = 0; i < numUdpServers; i++) {
      if (udpServers[i]) {
        delete udpServers[i];
      }
    }

    delete udpServers;
  }

  udpServers = new MiLightUdpServer*[settings.numGatewayConfigs];
  numUdpServers = settings.numGatewayConfigs;

  for (size_t i = 0; i < settings.numGatewayConfigs; i++) {
    GatewayConfig* config = settings.gatewayConfigs[i];
    MiLightUdpServer* server = MiLightUdpServer::fromVersion(
      config->protocolVersion,
      milightClient,
      config->port,
      config->deviceId
    );

    if (server == NULL) {
      Serial.print(F("Error creating UDP server with protocol version: "));
      Serial.println(config->protocolVersion);
    } else {
      udpServers[i] = server;
      udpServers[i]->begin();
    }
  }
}

/**
 * Milight RF packet handler.
 *
 * Called both when a packet is sent locally, and when an intercepted packet
 * is read.
 */
void onPacketSentHandler(uint8_t* packet, const MiLightRemoteConfig& config) {
  StaticJsonBuffer<200> buffer;
  JsonObject& result = buffer.createObject();

  BulbId bulbId = config.packetFormatter->parsePacket(packet, result);

  // set LED mode for a packet movement
  ledStatus->oneshot(settings.ledModePacket, settings.ledModePacketCount);

  if (&bulbId == &DEFAULT_BULB_ID) {
    Serial.println(F("Skipping packet handler because packet was not decoded"));
    return;
  }

  const MiLightRemoteConfig& remoteConfig =
    *MiLightRemoteConfig::fromType(bulbId.deviceType);

  // update state to reflect changes from this packet
  GroupState* groupState = stateStore->get(bulbId);

  if (groupState != NULL) {
    groupState->patch(result);
    stateStore->set(bulbId, *groupState);
  }

  if (mqttClient) {
    // Sends the state delta derived from the raw packet
    char output[200];
    result.printTo(output);
    mqttClient->sendUpdate(remoteConfig, bulbId.deviceId, bulbId.groupId, output);

    // Sends the entire state
    if (groupState != NULL) {
      bulbStateUpdater->enqueueUpdate(bulbId, *groupState);
    }
  }

  httpServer->handlePacketSent(packet, remoteConfig);
}

/**
 * Listen for packets on one radio config.  Cycles through all configs as its
 * called.
 */
void handleListen() {
  if (! settings.listenRepeats) {
    return;
  }

  MiLightRadio* radio = milightClient->switchRadio(currentRadioType++ % milightClient->getNumRadios());

  for (size_t i = 0; i < settings.listenRepeats; i++) {
    if (milightClient->available()) {
      uint8_t readPacket[MILIGHT_MAX_PACKET_LENGTH];
      size_t packetLen = milightClient->read(readPacket);

      const MiLightRemoteConfig* remoteConfig = MiLightRemoteConfig::fromReceivedPacket(
        radio->config(),
        readPacket,
        packetLen
      );

      if (remoteConfig == NULL) {
        // This can happen under normal circumstances, so not an error condition
#ifdef DEBUG_PRINTF
        Serial.println(F("WARNING: Couldn't find remote for received packet"));
#endif
        return;
      }

      // update state to reflect this packet
      onPacketSentHandler(readPacket, *remoteConfig);
    }
  }
}

/**
 * Called when MqttClient#update is first being processed.  Stop sending updates
 * and aggregate state changes until the update is finished.
 */
void onUpdateBegin() {
  if (bulbStateUpdater) {
    bulbStateUpdater->disable();
  }
}

/**
 * Called when MqttClient#update is finished processing.  Re-enable state
 * updates, which will flush accumulated state changes.
 */
void onUpdateEnd() {
  if (bulbStateUpdater) {
    bulbStateUpdater->enable();
  }
}

/**
 * Apply what's in the Settings object.
 */
void applySettings() {
  if (milightClient) {
    delete milightClient;
  }
  if (radioFactory) {
    delete radioFactory;
  }
  if (mqttClient) {
    delete mqttClient;
    delete bulbStateUpdater;

    mqttClient = NULL;
    bulbStateUpdater = NULL;
  }
  if (stateStore) {
    delete stateStore;
  }

  pinInit();

  radioFactory = MiLightRadioFactory::fromSettings(settings);

  if (radioFactory == NULL) {
    Serial.println(F("ERROR: unable to construct radio factory"));
  }

  stateStore = new GroupStateStore(MILIGHT_MAX_STATE_ITEMS, settings.stateFlushInterval);

  milightClient = new MiLightClient(
    radioFactory,
    stateStore,
    &settings
  );
  milightClient->begin();
  milightClient->onPacketSent(onPacketSentHandler);
  milightClient->onUpdateBegin(onUpdateBegin);
  milightClient->onUpdateEnd(onUpdateEnd);
  milightClient->setResendCount(settings.packetRepeats);

  if (settings.mqttServer().length() > 0) {
    mqttClient = new MqttClient(settings, milightClient);
    mqttClient->begin();
    bulbStateUpdater = new BulbStateUpdater(settings, *mqttClient, *stateStore);
  }

  initMilightUdpServers();

  if (discoveryServer) {
    delete discoveryServer;
    discoveryServer = NULL;
  }
  if (settings.discoveryPort != 0) {
    discoveryServer = new MiLightDiscoveryServer(settings);
    discoveryServer->begin();
  }

  // update LED pin and operating mode
  if (ledStatus) {
    ledStatus->changePin(settings.ledPin);
    ledStatus->continuous(settings.ledModeOperating);
  }

}

/**
 *
 */
bool shouldRestart() {
  if (! settings.isAutoRestartEnabled()) {
    return false;
  }

  return settings.getAutoRestartPeriod()*60*1000 < millis();
}

// give a bit of time to update the status LED
void handleLED() {
  ledStatus->handle();
}


// functions for the aditional features
void pinRead() {
  if(pinState[0] < 0 ){
    return;
  }
  sensors.sensorPinStates = "";
  for( uint8_t i = 1 ; i <= 4 ; i++){
    if(pinState[i] > -2 ){
      int x = digitalRead(aktPin[i]);
      if(x != pinState[i]){
        Serial.print("Pin: ");
        Serial.print(i);
        Serial.print(" state changes from ");
        Serial.print( pinState[i] );
        Serial.print(" to ");
        Serial.println( x );
        pinState[i] = x;
        mqttClient->sendSensor("pin" + String(i) , String(x).c_str() );
      }
    }
    sensors.sensorPinStates += String(pinState[i]).c_str();
    sensors.sensorPinStates += ";";
  }
}
void i2cRead(){
  bool gotTemp = false , gotHum = false;
  if(si7021.sensorExists()){
    gotTemp = true , gotHum = true;
    temp = float(si7021.getCelsiusHundredths()) / 100;
    hum = float(si7021.getHumidityBasisPoints()) / 100;
    sensors.sensorSI7021hum = hum;
    sensors.sensorSI7021temp = temp;

    Serial.println("I2C SI7021 - Reading Data");
    Serial.print("Temperature ");
    Serial.print( temp );
    Serial.println("°C");
    mqttClient->sendSensor("Temperature", String(temp).c_str() );
    Serial.print("Humidity    ");
    Serial.print( hum );
    Serial.println(" RH");
    mqttClient->sendSensor("Humidity", String(hum).c_str() );
  }
  if(bme280.begin()) {
      bme280.read(pres, temp, hum, BME280::TempUnit_Celsius, BME280::PresUnit_hPa);
      sensors.sensorBME280pres = pres;
      sensors.sensorBME280hum = hum;
      sensors.sensorBME280temp = temp;

      Serial.println("I2C BME280 - Reading Data");
      Serial.print("Temperature ");
      Serial.print( temp );
      Serial.println("°C");
      if(gotTemp){
        mqttClient->sendSensor("2Temperature", String(temp).c_str() );
      }else{
        mqttClient->sendSensor("Temperature", String(temp).c_str() );
      }
      if(hum > 0.0){
          Serial.print("Humidity    ");
          Serial.print( hum );
          Serial.println(" RH");
          if(gotHum){
            mqttClient->sendSensor("2Humidity", String(hum).c_str() );
          }else{
            mqttClient->sendSensor("Humidity", String(hum).c_str() );
          }
      }
      Serial.print("Pressure    ");
      Serial.print( pres );
      Serial.println(" hPa");
      mqttClient->sendSensor("Presure", String(pres).c_str() );
  }
  if(bh1750.begin()) {
      sensors.sensorBH1750lux = String(bh1750.readLightLevel()).c_str();
      Serial.println("I2C BH1750 - Reading Data");
      Serial.print("Light       ");
      Serial.print(bh1750.readLightLevel());
      Serial.println(" lx");
      mqttClient->sendSensor("Luminosity", String(bh1750.readLightLevel()).c_str() );
  }
}




void setup() {

  // User given Hostname
  EEPROM.begin(512);
  String staticHostName ;
  for (int i = 0; i < 32 ; ++i){
      if(EEPROM.read(i) != 0 ){
        staticHostName += char(EEPROM.read(i));
      }else{
        i = 32;
      }
    }
  WiFi.hostname(staticHostName) ;


  Serial.begin(115200);
  String ssid = "ESP" + String(ESP.getChipId());

  // load up our persistent settings from the file system
  SPIFFS.begin();
  Settings::load(settings);
  applySettings();

  Sensors::load(sensors);
  Names::load(names);


// Aditional setup for features
  if( settings.hostname.length() > 0  ){
    if(settings.hostname != staticHostName){
        Serial.print("Settings Hostname: ");
        Serial.println(settings.hostname);
        Serial.print("EEPROM Hostname: ");
        Serial.println(staticHostName);
        Serial.println("Saving own Hostname to EEPROM");
        for (u_int i = 0; i < settings.hostname.length(); ++i){
            EEPROM.write(i, settings.hostname[i]);
        }
        for (int i = settings.hostname.length() ; i < 32 ; ++i){
            EEPROM.write(i, 0);
        }

        EEPROM.end();
        EEPROM.commit();
        Serial.println("Hostname is now in EEPROM");
        Serial.println("Reboot");
        ESP.restart();
    }else{
        Serial.println("Settings Hostname and EEPROM are equal");
    }
    Serial.print("Settings Hostname: ");
    Serial.println(settings.hostname);
  }
  if ( settings.staticIp.length() > 0  && settings.staticMask.length() > 0  && settings.staticGate.length() > 0 ){
    Serial.println("Init Static IP");
    Serial.print("Settings IP: ");
    Serial.println(settings.staticIp);
    wifiManager.setSTAStaticIPConfig(settings._staticIp, settings._staticGate, settings._staticMask);
    Serial.println("Setting up static IP");
  }else{
    Serial.println("Init DHCP IP");
    if(settings._staticGate != WiFi.gatewayIP() || settings._staticMask != WiFi.subnetMask() ){
        settings._staticGate = WiFi.gatewayIP();
        settings._staticMask = WiFi.subnetMask();
        settings.staticGate = WiFi.gatewayIP().toString();
        settings.staticMask = WiFi.subnetMask().toString();
        Serial.println("Save new Settings");
        settings.save();
    }
    Serial.println("Setting up dynamic IP");
  }

  Serial.println("Init I2C Bus");
  if(settings.sdaPin > 0 && settings.sclPin > 0 ){
      Serial.print("I2C SDA Pin : ");
      Serial.println(settings.sdaPin);
      Serial.print("I2C SCL Pin : ");
      Serial.println(settings.sclPin);
    //  i2c.setup(0, settings.sdaPin, settings.sclPin, i2c.SLOW);
      Wire.begin(settings.sdaPin, settings.sclPin );
      if(!si7021.begin(settings.sdaPin, settings.sclPin)) {
          Serial.println("I2C Could not find SI7021 sensor");
      }else{
          Serial.println("I2C SI7021 sensor detetected");
          sensors.sensorSI7021 = true;
      }
      if(!bme280.begin()) {
          Serial.println("I2C Could not find BME280 sensor");
      }else{
          Serial.println("I2C BME280 sensor detetected");
          sensors.sensorBME280 = true;
      }
      if(!bh1750.begin()) {
          Serial.println("I2C Could not find BH1750 sensor");
      }else{
          Serial.println("I2C BH1750 sensor detetected");
          sensors.sensorBH1750 = true;
      }
      tmr.every(settings.mqttPushInterval , i2cRead);
  }else{
      Serial.println("I2C not configured");
  }

  Serial.println("Init Pin Read");
  pinInit();


  Serial.println(F("Init Over the Air Update Service"));
  ArduinoOTA.setHostname(settings.hostname.c_str());
  ArduinoOTA.setPassword(settings.otaPass.c_str());
  ArduinoOTA.begin();


  // set up the LED status for wifi configuration
  ledStatus = new LEDStatus(settings.ledPin);
  ledStatus->continuous(settings.ledModeWifiConfig);

  // start up the wifi manager
  if (! MDNS.begin("milight-hub")) {
    Serial.println(F("Error setting up MDNS responder"));
  }
  // tell Wifi manager to call us during the setup.  Note that this "setSetupLoopCallback" is an addition
  // made to Wifi manager in a private fork.  As of this writing, WifiManager has a new feature coming that
  // allows the "autoConnect" method to be non-blocking which can implement this same functionality.  However,
  // that change is only on the development branch so we are going to continue to use this fork until
  // that is merged and ready.
  wifiManager.setSetupLoopCallback(handleLED);
  wifiManager.setConfigPortalTimeout(180);
  if (wifiManager.autoConnect(ssid.c_str(), "milightHub")) {
    // set LED mode for successful operation
    ledStatus->continuous(settings.ledModeOperating);
    Serial.println(F("Wifi connected succesfully\n"));

    // if the config portal was started, make sure to turn off the config AP
    WiFi.mode(WIFI_STA);
  } else {
    // set LED mode for Wifi failed
    ledStatus->continuous(settings.ledModeWifiFailed);
    Serial.println(F("Wifi failed.  Restarting in 10 seconds.\n"));

    delay(10000);
    ESP.restart();
  }


  MDNS.addService("http", "tcp", 80);

  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName("ESP8266 MiLight Gateway");
  SSDP.setSerialNumber(ESP.getChipId());
  SSDP.setURL("/");
  SSDP.setDeviceType("upnp:rootdevice");
  SSDP.begin();

  httpServer = new MiLightHttpServer(settings, sensors, names , milightClient, stateStore);
  httpServer->onSettingsSaved(applySettings);
  httpServer->on("/description.xml", HTTP_GET, []() { SSDP.schema(httpServer->client()); });
  httpServer->begin();

  Serial.printf_P(PSTR("Setup complete (version %s)\n"), QUOTE(MILIGHT_HUB_VERSION));

  i2cRead();
}

void loop() {
  httpServer->handleClient();

  if (mqttClient) {
    mqttClient->handleClient();
    bulbStateUpdater->loop();
  }

  if (udpServers) {
    for (size_t i = 0; i < settings.numGatewayConfigs; i++) {
      udpServers[i]->handleClient();
    }
  }

  if (discoveryServer) {
    discoveryServer->handleClient();
  }

  handleListen();

  stateStore->limitedFlush();

  // update LED with status
  ledStatus->handle();

// Aditional handles...
  tmr.update();
  pinRead();
  ArduinoOTA.handle();

  if (shouldRestart()) {
    Serial.println(F("Auto-restart triggered. Restarting..."));
    ESP.restart();
  }
}

#endif