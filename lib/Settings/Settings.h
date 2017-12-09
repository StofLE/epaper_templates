#include <functional>
#include <ArduinoJson.h>
#include <Timezone.h>
#include <Timezones.h>
#include <StringStream.h>

#ifndef _SETTINGS_H
#define _SETTINGS_H

#ifndef BITMAP_DIRECTORY_PREFIX
#define BITMAP_DIRECTORY_PREFIX "/bitmaps/"
#endif

#ifndef TEMPLATE_DIRECTORY_PREFIX
#define TEMPLATE_DIRECTORY_PREFIX "/templates/"
#endif

#define XQUOTE(x) #x
#define QUOTE(x) XQUOTE(x)

#ifndef FIRMWARE_VARIANT
#define FIRMWARE_VARIANT unknown
#endif

#ifndef MILIGHT_HUB_VERSION
#define MILIGHT_HUB_VERSION unknown
#endif

#ifndef MILIGHT_MAX_STATE_ITEMS
#define MILIGHT_MAX_STATE_ITEMS 100
#endif

#ifndef MILIGHT_MAX_STALE_MQTT_GROUPS
#define MILIGHT_MAX_STALE_MQTT_GROUPS 10
#endif

#define SETTINGS_FILE  "/config.json"
#define SETTINGS_TERMINATOR '\0'

#define DEFAULT_MQTT_PORT 1883

class Settings {
public:
  typedef std::function<void()> TSettingsUpdateFn;

  Settings();
  ~Settings();

  void onUpdate(TSettingsUpdateFn fn);

  bool hasAuthSettings();

  static void deserialize(Settings& settings, String json);
  static void load(Settings& settings);
  String toJson(const bool prettyPrint = true);
  void save();
  void serialize(Stream& stream, const bool prettyPrint = false);
  void patch(JsonObject& obj);

  Timezone& getTimezone();
  void setTimezone(const String& timezone);

  String adminUsername;
  String adminPassword;

  String mqttServer();
  uint16_t mqttPort();
  String _mqttServer;
  String mqttUsername;
  String mqttPassword;
  String mqttVariablesTopicPattern;
  unsigned long fullRefreshPeriod;
  String hostname;
  String templatePath;

protected:
  template <typename T>
  void setIfPresent(JsonObject& obj, const char* key, T& var) {
    if (obj.containsKey(key)) {
      var = obj.get<T>(key);
    }
  }

  TSettingsUpdateFn onUpdateFn;
  String timezoneName;
};

#endif