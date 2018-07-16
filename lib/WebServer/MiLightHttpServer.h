#include <WebServer.h>
#include <MiLightClient.h>
#include <Settings.h>
#include <Sensors.h>
#include <Names.h>

#include <WebSocketsServer.h>
#include <GroupStateStore.h>

#ifndef _MILIGHT_HTTP_SERVER
#define _MILIGHT_HTTP_SERVER

#define MAX_DOWNLOAD_ATTEMPTS 3

typedef std::function<void(void)> SettingsSavedHandler;
typedef std::function<void(void)> NamesSavedHandler;

const char TEXT_PLAIN[] PROGMEM = "text/plain";
const char APPLICATION_JSON[] = "application/json";

class MiLightHttpServer {
public:
  MiLightHttpServer(Settings& settings, Sensors& sensors, Names& names, MiLightClient*& milightClient, GroupStateStore*& stateStore)
    : server(80),
      wsServer(WebSocketsServer(81)),
      numWsClients(0),
      milightClient(milightClient),
      settings(settings),
      names(names),
      sensors(sensors),
      stateStore(stateStore)
  {
    this->applySettings(settings);
  }

  void begin();
  void handleClient();
  void onSettingsSaved(SettingsSavedHandler handler);
  void on(const char* path, HTTPMethod method, ESP8266WebServer::THandlerFunction handler);
  void handlePacketSent(uint8_t* packet, const MiLightRemoteConfig& config);
  WiFiClient client();

protected:
  ESP8266WebServer::THandlerFunction handleServeFile(
    const char* filename,
    const char* contentType,
    const char* defaultText = NULL);

  void serveSettings();
  void serveNames();
  void serveSensors();
  bool serveFile(const char* file, const char* contentType = "text/html");
  ESP8266WebServer::THandlerFunction handleUpdateFile(const char* filename);
  ESP8266WebServer::THandlerFunction handleServe_P(const char* data, size_t length);
  void applySettings(Settings& settings);
  void sendGroupState(BulbId& bulbId, GroupState& state);

  void handleUpdateSettings();
  void handleUpdateNames();
  void handleUpdateSettingsPost();
  void handleUpdateNamesPost();
  void handleGetRadioConfigs();
  void handleAbout();
  void handleSystemPost();
  void handleFirmwareUpload();
  void handleFirmwarePost();
  void handleListenGateway(const UrlTokenBindings* urlBindings);
  void handleSendRaw(const UrlTokenBindings* urlBindings);
  void handleUpdateGroup(const UrlTokenBindings* urlBindings);
  void handleGetGroup(const UrlTokenBindings* urlBindings);

  void handleRequest(const JsonObject& request);
  void handleWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);

  File updateFile;

  WebServer server;
  WebSocketsServer wsServer;
  Settings& settings;
  Names& names;
  Sensors& sensors;
  MiLightClient*& milightClient;
  GroupStateStore*& stateStore;
  SettingsSavedHandler settingsSavedHandler;
  NamesSavedHandler namesSavedHandler;
  size_t numWsClients;
  ESP8266WebServer::THandlerFunction _handleRootPage;

};

#endif
