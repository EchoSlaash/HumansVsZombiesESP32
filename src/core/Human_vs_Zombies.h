#define USE_IDF_METHOD
#ifdef USE_IDF_METHOD
#include "WiFiType.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <esp_event_loop.h>
#else
#include "WiFi.h"
#endif
#include "nvs_flash.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <CoolLog.h>
#include <NeoPixelBus.h>

#define WIFI_SCAN_PERIOD 500
#define WIFI_SCAN_MIN_X_CH 10
#define WIFI_SCAN_MAX_X_CH 100

/* Set the SSID and Password via project configuration, or can set directly here
 */

#define PLAYER_AS "Human"
//#define PLAYER_AS "Zombie"

#ifdef PLAYER_AS == "Human"
#define DEFAULT_GAME_PLAYER "Human"
#define DEFAULT_GAME_PLAYER_PWD "HumanHuman"
#else
#ifdef PLAYER_AS "Zombie"
#define DEFAULT_GAME_PLAYER "Zombie"
#define DEFAULT_GAME_PLAYER_PWD "ZombieZombie"
#else
#define DEFAULT_GAME_PLAYER "Item"
#define DEFAULT_GAME_PLAYER_PWD "ItemItem"
#endif
#endif

#define DEFAULT_GAME_HUMAN "Human"
#define DEFAULT_GAME_ZOMBIE "Zombie"
#define DEFAULT_GAME_ITEM "Item"
#define DEFAULT_GAME_SSID "Human"
#define DEFAULT_GAME_PWD "HumanHuman"

/*         If “channel” is 0, there will be an all-channel
           scan; otherwise, there will be a specific-channel scan. */
#define DEFAULT_GAME_CHANNEL 1

#define DEFAULT_GAME_SSID_HIDDEN false
#define DEFAULT_GAME_MAX_CONNECTION 1

#if CONFIG_EXAMPLE_WIFI_ALL_CHANNEL_SCAN
#define DEFAULT_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#elif CONFIG_EXAMPLE_WIFI_FAST_SCAN
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#else
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#endif /*CONFIG_EXAMPLE_SCAN_METHOD*/

#if CONFIG_EXAMPLE_WIFI_CONNECT_AP_BY_SIGNAL
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_EXAMPLE_WIFI_CONNECT_AP_BY_SECURITY
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#else
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#endif /*CONFIG_EXAMPLE_SORT_METHOD*/

#if CONFIG_EXAMPLE_FAST_SCAN_THRESHOLD
#define DEFAULT_RSSI CONFIG_EXAMPLE_FAST_SCAN_MINIMUM_SIGNAL
#if CONFIG_EXAMPLE_FAST_SCAN_WEAKEST_AUTHMODE_OPEN
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#elif CONFIG_EXAMPLE_FAST_SCAN_WEAKEST_AUTHMODE_WEP
#define DEFAULT_AUTHMODE WIFI_AUTH_WEP
#elif CONFIG_EXAMPLE_FAST_SCAN_WEAKEST_AUTHMODE_WPA
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA_PSK
#elif CONFIG_EXAMPLE_FAST_SCAN_WEAKEST_AUTHMODE_WPA2
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA2_PSK
#else
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif
#else
#define DEFAULT_RSSI -127
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif /*CONFIG_EXAMPLE_FAST_SCAN_THRESHOLD*/

class HumanVsZombies {

public:
  typedef struct {
    uint8_t HumansDetected = 0;
    int8_t HumansSignalPower[128];
    uint8_t ZombiesDetected = 0;
    int8_t ZombiesSignalPower[128];
    uint8_t ItemsDetected = 0;
    int8_t ItemsSignalPower[128];
  } detectedPlayersStruct;

  static HumanVsZombies &getInstance();
  HumanVsZombies(HumanVsZombies const &) = delete;
  void operator=(HumanVsZombies const &) = delete;
  bool boot();
  bool wifiConfig();
  int parseJsonWiFiScan(JsonArray &jsonArray);
  void processDetectedPlayers(detectedPlayersStruct &detectedPlayers);
  int16_t getIndexOfMaximumValue(int8_t *array, int size);
  int8_t lifePoints = 100;
  uint8_t humanDistance = 100;
  uint8_t zombieDistance = 100;
  uint8_t itemDistance = 100;
  uint16_t humanIndex = 0;
  uint16_t zombieIndex = 0;
  uint16_t itemIndex = 0;
  uint8_t timeOnItemRange = 0;
  bool infected = false;
  void itemCare();
  int8_t updateLifePoints(uint8_t enemyDistance);
  int8_t careLifePoints(uint8_t friendlyHumanDistance);
  int8_t mixedLifePoints(uint8_t enemyDistance, uint8_t friendlyHumanDistance);
  HumanVsZombies() {}
#ifdef USE_IDF_METHOD

  int16_t scanNetworks(wifi_scan_config_t &config);
  String bssidConvertToDec(uint8_t *data);

protected:
  static void *_getScanInfoByIndex(int i);
#endif
};