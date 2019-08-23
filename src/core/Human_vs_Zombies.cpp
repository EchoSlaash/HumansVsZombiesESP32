#include "Human_vs_Zombies.h"

const uint16_t PixelCount = 24; // this example assumes 4 pixels, making it
                                // smaller will cause a failure
const uint8_t PixelPin = 4;
NeoPixelBus<NeoGrbFeature, NeoEsp32I2s1800KbpsMethod> strip(PixelCount,
                                                            PixelPin);

#define colorSaturation 128

RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor purple(1, 0, 1);
RgbColor white(colorSaturation);
RgbColor black(0);
RgbColor yellow(1, 1, 0);

esp_err_t event_handler(void *ctx, system_event_t *event);
xTimerHandle WiFiScannerTimerHandler;

HumanVsZombies &HumanVsZombies::getInstance() {
  static HumanVsZombies instance;

  return instance;
}

TaskHandle_t gameManagerTaskHandle;
void gameManagerTask(void *parameter);

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {}

static void WIFIScanner(xTimerHandle pxTimer) {
  /* Code Shoud to run every WIFI_SCAN_PERIOD */
#ifdef USE_IDF_METHOD
  /* */
  wifi_scan_config_t scanConfig;
  HumanVsZombies::getInstance().scanNetworks(scanConfig);
#else
  DynamicJsonBuffer json;
  JsonObject &root = json.createObject();

  /* TODO: USE parameters of scanNetworks, like: uint32_t max_ms_per_chan */
  /* int16_t WiFiScanClass::scanNetworks(
      bool async, bool show_hidden, bool passive,
      uint32_t max_ms_per_chan)

   * Start scan WiFi networks available
   * @param async         run in async mode
   * @param show_hidden   show hidden networks
   * @return Number of discovered networks
   */
  int n = WiFi.scanNetworks();
  /**
   * INFO: WiFi.scanComplete()
   * called to get the scan state in Async mode
   * @return scan result or status
   *          -1 if scan not fin
   *          -2 if scan not triggered
   */
  while (WiFi.scanComplete() < 0) {
    delay(1);
  }

  if (n) {
    DEBUG_LOG("Start WiFi Scan");
    for (uint8_t i = 0; i < n; i++) {
      if ((WiFi.SSID(i) == DEFAULT_GAME_HUMAN) ||
          WiFi.SSID(i) == DEFAULT_GAME_ZOMBIE) {
        root.createNestedObject(WiFi.BSSIDstr(i));
        JsonObject &obj = root[WiFi.BSSIDstr(i)];
        obj["bssid"] = WiFi.BSSIDstr(i);
        obj["ssid"] = WiFi.SSID(i);
        obj["rssi"] = WiFi.RSSI(i);
        obj["channel"] = String(WiFi.channel(i));
        obj["secure"] = String(WiFi.encryptionType(i));
#ifdef ESP8266
        obj["hidden"] = String(WiFi.isHidden(i) ? "true" : "false");
#endif
      }
      /*
       *  delete last scan result from RAM
       */
    }
    WiFi.scanDelete();

#if COOL_LEVEL >= COOL_DEBUG
    DEBUG_LOG("WiFi Scan Result: ");
    root.prettyPrintTo(Serial); /* Print only in Debug Mode  */
    Serial.println();
#endif

    /* TODO: Time to Process the result */
  }
#if COOL_LEVEL >= COOL_DEBUG
  else {
    DEBUG_LOG("No NetworK Found: ");
  }
#endif
#endif
}

int16_t HumanVsZombies::getIndexOfMaximumValue(int8_t *array, int size) {
  int16_t maxIndex = 0;
  int16_t max = array[maxIndex];
  for (uint16_t i = 0; i < size; i++) {
    if (max < array[i]) {
      max = array[i];
      maxIndex = i;
    }
  }
  return maxIndex;
}

bool HumanVsZombies::wifiConfig() {
  /* */
  DEBUG_LOG("Wifi Start Config");

#ifdef USE_IDF_METHOD

  system_init();
  tcpip_adapter_init();

  //   ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
  //   ESP_EVENT_ANY_ID,
  //                                              &event_handler, NULL));
  //   ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
  //   IP_EVENT_STA_GOT_IP,
  //                                              &event_handler, NULL));

  /**
   * Set up an access point
   * @param ssid              Pointer to the SSID (max 63 char).
   * @param passphrase        (for WPA2 min 8 char, for open use NULL)
   * @param channel           WiFi channel number, 1 - 13.
   * @param ssid_hidden       Network cloaking (0 = broadcast SSID, 1 = hide
   * SSID)
   * @param max_connection    Max simultaneous connected clients, 1 - 4.
   */

  wifi_config_t wifi_config;
  memset(&wifi_config, 0, sizeof(wifi_config_t));
  strcpy(reinterpret_cast<char *>(wifi_config.ap.ssid), DEFAULT_GAME_PLAYER);
  wifi_config.ap.channel = DEFAULT_GAME_CHANNEL;
  wifi_config.ap.ssid_len =
      strlen(reinterpret_cast<char *>(wifi_config.ap.ssid));

  wifi_config.ap.ssid_hidden = 0;
  wifi_config.ap.max_connection = 4;
  wifi_config.ap.beacon_interval = 100;

  if (!DEFAULT_GAME_PLAYER_PWD || strlen(DEFAULT_GAME_PLAYER_PWD) == 0) {
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    *wifi_config.ap.password = 0;
  } else {
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    strlcpy(reinterpret_cast<char *>(wifi_config.ap.password),
            DEFAULT_GAME_PLAYER_PWD, sizeof(wifi_config.ap.password));
  }

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  DEBUG_LOG("Wifi Config Done");
#else
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(DEFAULT_GAME_SSID, DEFAULT_GAME_PWD, DEFAULT_GAME_CHANNEL,
              DEFAULT_GAME_SSID_HIDDEN, DEFAULT_GAME_MAX_CONNECTION);
  WiFi.disconnect();
#endif
}

bool HumanVsZombies::boot() {

  Serial.begin(250000);

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  this->wifiConfig();

  strip.Begin();
  strip.Show();

  for (uint8_t i = 0; i < 24; i++) {
    strip.SetPixelColor(i, black);
  }

  for (uint8_t i = 12; i < 24; i++) {
#if PLAYER_AS == HUMAN
    strip.SetPixelColor(i, green);
#else
#if PLAYER_AS == ZOMBIE
    strip.SetPixelColor(i, red);
#else
    strip.SetPixelColor(i, yellow);
#endif
#endif
  }

  strip.Show();

  /* Setup Timer CallBack to run every BATTERYCHECK_PERIOD defined time */
  WiFiScannerTimerHandler =
      xTimerCreate("WiFiScannerTimerHandler",       /* name */
                   pdMS_TO_TICKS(WIFI_SCAN_PERIOD), /* period/time */
                   pdTRUE,                          /* auto reload */
                   (void *)0,                       /* timer ID */
                   WIFIScanner);                    /* callback */
  if (WiFiScannerTimerHandler == NULL) {
    ERROR_LOG("Fail to Create WiFiScannerTimerHandler daemon");
  }
  /* Start Timer */
  if (xTimerStart(WiFiScannerTimerHandler, 0) != pdPASS) {
    ERROR_LOG("Fail to start WiFiScannerTimerHandler daemon");
  }
}

#ifdef USE_IDF_METHOD
int16_t HumanVsZombies::scanNetworks(wifi_scan_config_t &config) {

  config.ssid = 0;  /* If the SSID is not NULL, it is only the AP with the
                       same  SSID that can be scanned. */
  config.bssid = 0; /* If the BSSID is not NULL, it is only the AP with the
                       same BSSID that can be scanned.  */
  config.channel =
      DEFAULT_GAME_CHANNEL; /* If “channel” is 0, there will be an all-channel
            scan; otherwise, there will be a specific-channel scan. */
  config.show_hidden = false;
  config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  config.scan_time.active.min = WIFI_SCAN_MIN_X_CH;
  config.scan_time.active.max = WIFI_SCAN_MAX_X_CH;

  switch (esp_wifi_scan_start(&config, true)) {
    /* @return
     *    - ESP_OK: succeed
     *    - ESP_ERR_WIFI_NOT_INIT: WiFi is not initialized by esp_wifi_init
     *    - ESP_ERR_WIFI_NOT_STARTED: WiFi was not started by esp_wifi_start
     *    - ESP_ERR_WIFI_TIMEOUT: blocking scan is timeout
     *    - ESP_ERR_WIFI_STATE: wifi still connecting when invoke
     * esp_wifi_scan_start
     *    - others: refer to error code in esp_err.h
     * */
  case ESP_OK:
    /* code */
    // DEBUG_LOG("ESP_OK: WiFi Scan succeed!");
    break;
  case ESP_ERR_WIFI_NOT_INIT:
    // DEBUG_LOG("ESP_ERR_WIFI_NOT_INIT");
    break;
  case ESP_ERR_WIFI_NOT_STARTED:
    // DEBUG_LOG("ESP_ERR_WIFI_NOT_STARTED");
    break;
  case ESP_ERR_WIFI_TIMEOUT:
    // DEBUG_LOG("ESP_ERR_WIFI_TIMEOUT");
    break;
  case ESP_ERR_WIFI_STATE:
    // DEBUG_LOG("ESP_ERR_WIFI_STATE");
    break;
  }

  return WIFI_SCAN_FAILED;
}

esp_err_t event_handler(void *ctx, system_event_t *event) {
  if (event->event_id == SYSTEM_EVENT_SCAN_DONE) {
    /* Hard coded task launch, TODO: Use Semaphores or queue */
    xTaskCreatePinnedToCore(
        gameManagerTask,        /* Function to implement the task */
        "gameManagerTask",      /* Name of the task */
        5000,                   /* Stack size in words */
        NULL,                   /* Task input parameter */
        6,                      /* Priority of the task */
        &gameManagerTaskHandle, /* Task handle. */
        1);                     /* Core where the task should run */
  }
  return ESP_OK;
}

String HumanVsZombies::bssidConvertToDec(uint8_t *data) {
  String bssid;
  for (uint8_t i = 0; i < 5; i++) {
    char c[3];
    itoa(data[i], c, 10);
    bssid += c;
    bssid += ':';
  }
  bssid[bssid.length() - 1] = '\0';
  return bssid;
}

void gameManagerTask(void *parameter) {
  uint16_t apCount = 0;
  esp_wifi_scan_get_ap_num(&apCount);
  // DEBUG_VAR("Number of access points found: ",
  //           event->event_info.scan_done.number);
  // if (apCount == 0) {
  //   return ESP_OK;
  // }
  /* Here is better to create a indipendent task with big heap capacity */

  DynamicJsonBuffer json;
  JsonArray &root = json.createArray();

  if (apCount) {
    wifi_ap_record_t *list =
        (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, list));
    for (int16_t i = 0; i < apCount; i++) {
      // String bssidHex =
      //     HumanVsZombies::getInstance().bssidConvertToDec(list[i].bssid);
      String SSID = (char *)list[i].ssid;
      if ((SSID == DEFAULT_GAME_HUMAN) || (SSID == DEFAULT_GAME_ZOMBIE) || (SSID == DEFAULT_GAME_ITEM)) {
        /* Create a array of JsonObject */
        DynamicJsonBuffer jsonWifi;
        JsonObject &wifiRoot = json.createObject();
        wifiRoot["ssid"] = SSID;
        wifiRoot["rssi"] = list[i].rssi;
        // wifiRoot["bssidHex"] = bssidHex;
        JsonArray &bssidArray = wifiRoot.createNestedArray("bssid");
        uint8_t *recoveredBssid = list[i].bssid;
        for (uint8_t x = 0; x < 5; x++) {
          bssidArray.add(recoveredBssid[x]);
        }
        root.add(wifiRoot);
      }
    }

    free(list);
#if COOL_LEVEL >= COOL_DEBUG
    DEBUG_LOG("WiFi Scan Result: ");
    root.prettyPrintTo(Serial); /* Print only in Debug Mode  */
    Serial.println();
#endif
    /* TODO: Time to Process the result saved in the json */
    HumanVsZombies::getInstance().parseJsonWiFiScan(root);
    root.end();
    json.clear();
  }
  vTaskDelete(NULL);
}

#endif

int HumanVsZombies::parseJsonWiFiScan(JsonArray &jsonArray) {
  // DEBUG_LOG("Parsing jsonWiFi Scan result");
  uint16_t wifiSize = jsonArray.size();
  // DEBUG_VAR("wifiSize ", wifiSize);
  // int8_t rssi[wifiSize];
  detectedPlayersStruct detectedPlayers;

  for (uint8_t i = 0; i < wifiSize; ++i) {
    String detectedPlayer = jsonArray[i]["ssid"].asString();
    if (detectedPlayer == DEFAULT_GAME_HUMAN) {
      detectedPlayers.HumansSignalPower[detectedPlayers.HumansDetected] =
          jsonArray[i]["rssi"];
      // DEBUG_VAR(
      //     "Human PWD: ",
      //     detectedPlayers.HumansSignalPower[detectedPlayers.HumansDetected]);
      detectedPlayers.HumansDetected++;
    }
  }

  for (uint8_t i = 0; i < wifiSize; ++i) {
    String detectedPlayer = jsonArray[i]["ssid"].asString();
    if (detectedPlayer == DEFAULT_GAME_ZOMBIE) {
      detectedPlayers.ZombiesSignalPower[i] = jsonArray[i]["rssi"];
      // DEBUG_VAR(
      //     "Zombie PWD: ",
      //     detectedPlayers.ZombiesSignalPower[detectedPlayers.ZombiesDetected]);
      detectedPlayers.ZombiesDetected++;
    }
  }

  for (uint8_t i= 0; i< wifiSize; i++) {
    String detectedPlayer = jsonArray[i]["ssid"].asString();
    if (detectedPlayer == DEFAULT_GAME_ITEM) {
      detectedPlayers.ItemsSignalPower[i] = jsonArray[i]["rssi"];
      detectedPlayers.ItemsDetected++;
    }
  }
  #if (PLAYER_AS == HUMAN || PLAYER_AS == ZOMBIE)
  this->processDetectedPlayers(detectedPlayers);
  #endif
}

void HumanVsZombies::processDetectedPlayers(
    detectedPlayersStruct &detectedPlayers) {

  if (detectedPlayers.HumansDetected)
    this->humanIndex = this->getIndexOfMaximumValue(
        detectedPlayers.HumansSignalPower, detectedPlayers.HumansDetected);
  if (detectedPlayers.ZombiesDetected)
    this->zombieIndex = this->getIndexOfMaximumValue(
        detectedPlayers.ZombiesSignalPower, detectedPlayers.ZombiesDetected);
  if (detectedPlayers.ItemsDetected)
    this->itemIndex = this->getIndexOfMaximumValue(
        detectedPlayers.ItemsSignalPower, detectedPlayers.ItemsDetected);

  // DEBUG_VAR("Detected Humans: ", detectedPlayers.HumansDetected);
  // DEBUG_VAR("Most near Human at index: ", humanIndex);

  // DEBUG_VAR("Detected Zombies: ", detectedPlayers.ZombiesDetected);
  // DEBUG_VAR("Most near Zombie at index: ", zombieIndex);

  this->humanDistance = abs(detectedPlayers.HumansSignalPower[humanIndex]);
  this->zombieDistance = abs(detectedPlayers.ZombiesSignalPower[zombieIndex]);
  this->itemDistance = abs(detectedPlayers.ItemsSignalPower[itemIndex]);

  // if (detectedPlayers.HumansDetected) {
  //   /* */
  // }

  /* TODO: Avoid the continus Led refresh */
  /* TODO: Update LifePoints */
  for (uint8_t i = 0; i < 12; i++) {
    strip.SetPixelColor(i, black);
  }

  /* Sometimes mapping bug, return directly 12, while the rrsi was 59, 69,
   * etc.. */
  uint8_t zombieLedDistance = map(zombieDistance, 128, 0, 0, 13);
  uint8_t humanLedDistance = map(humanDistance, 128, 0, 0, 13);
  uint8_t itemLedDistance = map(itemDistance, 128, 0, 0, 13);

/* Hardcore Fix to map bug */
#if PLAYER_AS == HUMAN
  if (zombieLedDistance < 13) {
    for (uint8_t i = 0; i < zombieLedDistance; i++) {
      strip.SetPixelColor(i, red);
    }
    strip.Show();
  }
#elif PLAYER_AS == ZOMBIE
  if (humanLedDistance < 13) {
    for (uint8_t i = 0; i < humanLedDistance; i++) {
      strip.SetPixelColor(i, blue);
    }
    strip.Show();
  }
#endif
  if ((detectedPlayers.ZombiesDetected > 0) &&
      (detectedPlayers.HumansDetected > 0)) {
#if PLAYER_AS == HUMAN
    uint8_t lifePointsLed = this->mixedLifePoints(zombieLedDistance, humanLedDistance);
    if (lifePointsLed < 13) {
      for (uint8_t i = 12 + (lifePointsLed); i<12; i--) {
        if (lifePoints>0)
          strip.SetPixelColor(i,green);
        if (lifePoints<0)
          strip.SetPixelColor(i, purple);
      }
      strip.Show();
    }
#elif PLAYER_AS == ZOMBIE
    uint8_t lifePointsLed = this->mixedLifePoints(0, zombieLedDistance);
    if (lifePointsLed < 13) {
      for (uint8_t i = 12 + (lifePointsLed); i<12; i--) {
        strip.SetPixelColor(i, red);
      }
      strip.Show();
    }
#endif
  } else {
    if (detectedPlayers.ZombiesDetected >= 1) {
#if PLAYER_AS == HUMAN
      uint8_t lifePointsLed = this->updateLifePoints(zombieLedDistance);
      for (uint8_t i = 12 + (lifePointsLed); i > 12; i--) {
        if (lifePoints > 0)
          strip.SetPixelColor(i, green);
        if (lifePoints < 0)
          strip.SetPixelColor(i, purple);
      }
#elif PLAYER_AS == ZOMBIE
    uint8_t lifePointsLed = this->careLifePoints(zombieLedDistance);
    for (uint8_t i = 12 + (lifePointsLed); i > 12; i--) {
      strip.SetPixelColor(i, red);
    }
#endif

  } else {
    if (detectedPlayers.HumansDetected > 0) {
      /* If there's no zombies restore LifePoints by friend */
      uint8_t lifePointsLed = this->careLifePoints(humanLedDistance);
      DEBUG_VAR("lifePointsLed: ", this->lifePoints);
      DEBUG_VAR("lifePointsLed: ", lifePointsLed);
      if (lifePointsLed < 13) {
        for (uint8_t i = 12 + (lifePointsLed); i > 12; i--) {
          if (lifePoints > 0)
            strip.SetPixelColor(i, green);
          if (lifePoints < 0)
            strip.SetPixelColor(i, purple);
          /* */
          strip.Show();
        }
      }
    }
  }
  }
  // Check if no one is detected (old fix for weird led blinking glitch)
  /*if ((detectedPlayers.ZombiesDetected == 0) &&
      (detectedPlayers.HumansDetected == 0)) {
    for (uint8_t i = 0; i < 12; i++) {
    strip.SetPixelColor(i, black);
    }
#if PLAYER_AS == HUMAN   
    uint8_t lifePointsLed = this->careLifePoints(0);
    for (uint8_t i = 12 + (lifePointsLed); i > 12; i--) {
      if (lifePoints > 0)
        strip.SetPixelColor(i, green);
      if (lifePoints < 0)
        strip.SetPixelColor(i, purple);
      strip.Show();
      }
#endif */
#if PLAYER_AS == HUMAN
  if (detectedPlayers.ZombiesDetected == 0) {
    for (uint8_t i = 0; i < 12; i++) {
      strip.SetPixelColor(i, black);
    }
    strip.Show();
  }
#elif PLAYER_AS == ZOMBIE
  if (detectedPlayers.HumansDetected == 0) {
    for (uint8_t i = 0; i < 12; i++) {
      strip.SetPixelColor(i, black);
    }
    strip.Show();
  }
#endif

  if (detectedPlayers.ItemsDetected > 0) {
    this->itemCare(itemLedDistance);
    DEBUG_VAR ("item1", itemStored[0]);
    DEBUG_VAR ("item2", itemStored[1]);
    DEBUG_VAR ("item3", itemStored[2]);
    DEBUG_VAR ("item4", itemStored[3]);
    
    //DEBUG_VAR("item is: ", item);
  }
  this->itemUse();
}

int8_t HumanVsZombies::careLifePoints(uint8_t allyDistance) {
  uint8_t lifePointsToRestore = 0;
  if (allyDistance > 7) {
    switch (allyDistance) {
    case 8:
      lifePointsToRestore = 1;
      break;
    case 9:
      lifePointsToRestore = 2;
      break;
    case 10:
      lifePointsToRestore = 4;
      break;
    case 11:
      lifePointsToRestore = 6;
      break;
    case 12:
      lifePointsToRestore = 8;
      break;
    }
  }
  uint8_t lifePointsLed = 0;
  this->lifePoints = (this->lifePoints + lifePointsToRestore);
  if (lifePoints < -100)
    lifePoints = -100;
  if (lifePoints > 100)
    lifePoints = 100;
  if (lifePoints > 0) {
    lifePointsLed = map(this->lifePoints, 0, 100, 0, 13);
    this->infected = false;
  }
  if (lifePoints < 0) {
    lifePointsLed = map(this->lifePoints, -100, 0, 13, 0);
  }
  return lifePointsLed;
}

int8_t HumanVsZombies::updateLifePoints(uint8_t enemyDistance) {
  for (uint8_t i = 12; i < 24; i++) {
    strip.SetPixelColor(i, black);
  }
  uint8_t damageOutput = 0;
  if (enemyDistance > 7) {
    switch (enemyDistance) {
    case 8:
      damageOutput = 1;
      break;
    case 9:
      damageOutput = 2;
      break;
    case 10:
      damageOutput = 4;
      break;
    case 11:
      damageOutput = 6;
      break;
    case 12:
      damageOutput = 8;
      break;
    }
  }
  uint8_t lifePointsLed = 0;
  this->lifePoints = (lifePoints - damageOutput);
  if (lifePoints < -100)
    lifePoints = -100;
  if (lifePoints > 100)
    lifePoints = 100;
  if (lifePoints > 0)
    lifePointsLed = map(this->lifePoints, 0, 100, 0, 13);
  if (lifePoints < 0) {
    lifePointsLed = map(this->lifePoints, -100, 0, 13, 0);
    this->infected = true;
    if (lifePointsLed > 11) {
      /* humans begin Zombie */
      // DEBUG_LOG("Now you are Zombie!");
    }
  }
  return lifePointsLed;
}

int8_t HumanVsZombies::mixedLifePoints(uint8_t enemyDistance, uint8_t allyDistance) {
  for (uint8_t i = 12; i < 24; i++) {
    strip.SetPixelColor(i, black);
  }

  uint8_t damageOutput = 0;
  if (enemyDistance > 7) {
    switch (enemyDistance) {
    case 8:
      damageOutput = 1;
      break;
    case 9:
      damageOutput = 2;
      break;
    case 10:
      damageOutput = 4;
      break;
    case 11:
      damageOutput = 6;
      break;
    case 12:
      damageOutput = 8;
      break;
    }
  }

uint8_t lifePointsToRestore = 0;
if (allyDistance > 7) {
  switch (allyDistance) {
  case 8:
    lifePointsToRestore = 1;
    break;
  case 9:
    lifePointsToRestore = 2;
    break;
  case 10:
    lifePointsToRestore = 4;
    break;
  case 11:
    lifePointsToRestore = 6;
    break;
  case 12:
    lifePointsToRestore = 8;
    break;
  }
}

uint8_t lifePointsLed = 0;
  this->lifePoints = (lifePoints - damageOutput + lifePointsToRestore);
  if (lifePoints < -100)
    lifePoints = -100;
  if (lifePoints > 100)
    lifePoints = 100;
  if (lifePoints > 0)
    lifePointsLed = map(this->lifePoints, 0, 100, 0, 13);
  if (lifePoints < 0) {
    lifePointsLed = map(this->lifePoints, -100, 0, 13, 0);
    this->infected = true;
  if (lifePointsLed > 11) {
      /* humans begin Zombie */
      // DEBUG_LOG("Now you are Zombie!");
    }
  }
  return lifePointsLed;
}

void HumanVsZombies::itemCare(uint8_t distanceToItem) {
  uint8_t itemToLoad = 0;
  if (distanceToItem> 7) {
    switch (distanceToItem) {
      case 8:
        itemToLoad = 1;
        break;
      case 9:
        itemToLoad = 2;
        break;
      case 10:
        itemToLoad = 4;
        break;
      case 11:
        itemToLoad = 6;
        break;
      case 12:
        itemToLoad = 8;
        break;
    }
  }
  itemLoad += itemToLoad;
  DEBUG_VAR("itemLoad:", itemLoad);

  if (itemLoad > 100) {
    itemLoad = 0;
    for (uint8_t itemNumber = 0; itemNumber < 4; itemNumber++)
    if (itemStored[itemNumber] == 0) {
      uint8_t item = 5;
      while((item > 4) || (item < 1)){
      item = esp_random();
      }
      itemStored[itemNumber] = item;
      itemNumber = 4;
    }
  }
}
void HumanVsZombies::itemUse() {
  bool itemButtonState;
}