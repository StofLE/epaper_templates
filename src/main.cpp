#include <Arduino.h>
#include <EnvironmentConfig.h>

#include <GxEPD.h>
#include <GxGDEW042T2/GxGDEW042T2.cpp>
#include <GxIO/GxIO_SPI/GxIO_SPI.cpp>
#include <GxIO/GxIO.cpp>

#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Timezone.h>

#include <DisplayTemplateDriver.h>
#include <EpaperWebServer.h>
#include <MqttClient.h>

#if defined(ESP8266)
GxIO_Class io(SPI, SS, D3, D4);
GxEPD_Class display(io);
#elif defined(ESP32)
GxIO_Class io(SPI, SS, 17, 16);
GxEPD_Class display(io, 16, 4);
#endif

enum class WiFiState {
  CONNECTED, DISCONNECTED
};

Settings settings;

DisplayTemplateDriver driver(&display, settings);
EpaperWebServer* webServer = NULL;
MqttClient* mqttClient = NULL;

// Don't attempt to reconnect to wifi if we've never connected
volatile bool hasConnected = false; 

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

uint8_t lastSecond = 60;

void applySettings() {
  Serial.println(F("Applying settings"));

  if (hasConnected && settings.wifiSsid.length() > 0 && settings.wifiSsid != WiFi.SSID()) {
    Serial.println(F("Switching WiFi networks"));

    WiFi.disconnect(true);
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
  }

  if (mqttClient != NULL) {
    delete mqttClient;
    mqttClient = NULL;
  }

  if (webServer != NULL) {
    delete webServer;
    webServer = NULL;
  }

  if (settings.mqttServer().length() > 0) {
    mqttClient = new MqttClient(
      settings.mqttServer(),
      settings.mqttPort(),
      settings.mqttVariablesTopicPattern,
      settings.mqttUsername,
      settings.mqttPassword
    );
    mqttClient->onVariableUpdate([](const String& variable, const String& value) {
      driver.updateVariable(variable, value);
    });
    mqttClient->begin();
  }

  if (settings.templatePath.length() > 0) {
    driver.setTemplate(settings.templatePath);
  }

  webServer = new EpaperWebServer(driver, settings);
  webServer->begin();
}

void updateWiFiState(WiFiState state) {
  const char varName[] = "wifi_state";
  const String varValue = state == WiFiState::CONNECTED ? "connected" : "disconnected";

  driver.updateVariable(varName, varValue);
}

#if defined(ESP32)
  TimerHandle_t reconnectTimer;

  void wifiReconnectCallback(TimerHandle_t xTimer) {
    Serial.print(F("Attempting to reconnect to wifi... ssid = "));
    Serial.println(WiFi.SSID());

    // Force disconnect seems necessary on ESP32:
    // https://github.com/espressif/arduino-esp32/issues/653
    WiFi.disconnect(true);
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPassword.c_str());
    uint8_t result = WiFi.waitForConnectResult();

    if (result == WL_CONNECTED) {
      Serial.println(F("Reconnection successful!"));
    } else {
      Serial.println(F("Connection attempt failed..."));
      Serial.println(result);
      xTimerStart(xTimer, 0);
    }
  }

  void onWifiEvent(WiFiEvent_t event) {
    switch (event) {
      case SYSTEM_EVENT_STA_START:
        WiFi.setHostname(settings.hostname.c_str());
        break;

      case SYSTEM_EVENT_STA_DISCONNECTED:
        updateWiFiState(WiFiState::DISCONNECTED);

        if (hasConnected) {
          xTimerStart(reconnectTimer, 0);
        }
        break;

      case SYSTEM_EVENT_STA_GOT_IP:
        updateWiFiState(WiFiState::CONNECTED);
        xTimerStop(reconnectTimer, 0);
        hasConnected = true;
        break;
    }
  }
#elif defined(ESP8266)
  void onWiFiConnected(const WiFiEventStationModeGotIP& event) {
    updateWiFiState(WiFiState::CONNECTED);
  }
  void onWiFiDisconnected(const WiFiEventStationModeDisconnected& event) {
    updateWiFiState(WiFiState::DISCONNECTED);
  }
#endif

void wifiManagerConfigSaved() {
  Serial.println(F("Config saved"));

  settings.wifiSsid = WiFi.SSID();
  settings.wifiPassword = WiFi.psk();
  settings.save();

  // Restart for good measure
  ESP.restart();
}

void setup() {
  Serial.begin(115200);

#if defined(ESP8266)
  if (! SPIFFS.begin()) {
#elif defined(ESP32)
  if (! SPIFFS.begin(true)) {
#endif
    Serial.println(F("Failed to mount SPIFFS!"));
  }

  Settings::load(settings);
  settings.onUpdate(applySettings);

  driver.init();
  updateWiFiState(WiFiState::DISCONNECTED);

  WiFiManager wifiManager;

#if defined(ESP8266)
  WiFi.setAutoReconnect(true);
  WiFi.hostname(settings.hostname);
  WiFi.onStationModeGotIP(onWiFiConnected);
  WiFi.onStationModeDisconnected(onWiFiDisconnected);
#elif defined(ESP32)
  WiFi.setAutoReconnect(false);
  WiFi.onEvent(onWifiEvent);
  reconnectTimer = xTimerCreate(
    "wifiReconnectTimer",
    pdMS_TO_TICKS(2000),
    pdFALSE,
    (void*)0,
    wifiReconnectCallback
  );
#endif

  char setupSsid[20];
  sprintf(setupSsid, "epaper_%d", ESP_CHIP_ID());

  wifiManager.setSaveConfigCallback(wifiManagerConfigSaved);
  wifiManager.autoConnect(setupSsid, settings.setupApPassword.c_str());

  timeClient.begin();

  applySettings();
}

void loop() {
  if (timeClient.update() && lastSecond != second()) {
    lastSecond = second();
    driver.updateVariable("timestamp", String(timeClient.getEpochTime()));
  }

  driver.loop();
}
