/*
 * espclock4.cpp
 *
 * Copyright 2021 Victor Chew
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "espclock4.h"

// Ordinary variables - not persisted across deep sleep
static bool shouldSaveConfig = false;
static char param_tz[48] = "UTC", param_url[128] = DEFAULT_SCRIPT_URL;
static char buf_timezone[48] = "", buf_clock_time[10] = "", buf_script_url[128] = DEFAULT_SCRIPT_URL;
static esp_sleep_wakeup_cause_t wake_cause;
static AsyncWebServer server(80);
static DNSServer dns;
static AsyncWiFiManager wifimgr(&server, &dns);
AsyncWiFiManagerParameter form_clockTime("clockTime", "Time on clock (12-hr HHMMSS)", buf_clock_time, sizeof(buf_clock_time)-1,
  "type=\"number\" autocomplete=\"off\"");
AsyncWiFiManagerParameter form_timezone("timezone", "TZ database timezone code", buf_timezone, sizeof(buf_timezone)-1);
AsyncWiFiManagerParameter form_scriptUrl("scriptUrl", "URL to ESPCLOCK script", buf_script_url, sizeof(buf_script_url)-1);

#ifdef DEBUG 
  void status_vars(const char* prefix) {
    debug("%s: wcause=%d, wreason=%d, ct=%02d:%02d:%02d, nt=%02d:%02d:%02d, pause_clock=%d, tickpin=%d, tick_action=%d, "
      "tick_delay=%d, sleep_count=%05d, sleep_interval=%05d, adc_vdd=%d, adc_vddl=%d, adc_vddh=%d, tune_level=%d, ulp_timer=%d, ulp_call_count=%d, dbg=%d",
      prefix, wake_cause, _get(VAR_WAKE_REASON), _get(VAR_CLK_HH), _get(VAR_CLK_MM), _get(VAR_CLK_SS), _get(VAR_NET_HH), _get(VAR_NET_MM), _get(VAR_NET_SS), 
      _get(VAR_PAUSE_CLOCK), _get(VAR_TICKPIN), _get(VAR_TICK_ACTION), _get(VAR_TICK_DELAY), _get(VAR_SLEEP_COUNT), _get(VAR_SLEEP_INTERVAL),
      _get(VAR_ADC_VDD), _get(VAR_ADC_VDDL), _get(VAR_ADC_VDDH), _get(VAR_TUNE_LEVEL), VAR_ULP_TIMER(), _get(VAR_ULP_CALL_COUNT), _get(VAR_DEBUG)
    );
  }
#else // !DEBUG
  void status_vars(const char* prefix) {}
#endif // DEBUG

// Application Javascript to be injected into WiFiManager's config page
#define QUOTE(...) #__VA_ARGS__
const char* jscript = QUOTE(
  <script>
    document.addEventListener('DOMContentLoaded', tzinit, false); 
    function tzinit() {
      document.getElementById("timezone").value = Intl.DateTimeFormat().resolvedOptions().timeZone;
    }
  </script>
);

/*
 * PHP script can be hosted on your own server:
 *
 * <?php
 * if (isset($_REQUEST['tz'])) {
 *   $time = new DateTime();
 *   $time->setTimezone(new DateTimeZone($_REQUEST['tz']));
 *   print $time->format('h:i:s');
 * }
 * ?>
 *
 * Or you can use the version hosted at http://espclock.randseq.org/now.php
 *
 * List of timezones can be found here: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
 */

// ESP.restart() is a software-level reset which occasionally causes the ULP to malfunction after restart.
// See: https://esp32.com/viewtopic.php?t=9298
// This function uses the RTC WDT functionality to induce a hardware-level reset that is akin to pulsing the EN pin low.
void rtc_reset() {
  delay(100);
  rtc_wdt_protect_off();
  rtc_wdt_disable();
  rtc_wdt_set_length_of_reset_signal(RTC_WDT_SYS_RESET_SIG, RTC_WDT_LENGTH_3_2us);
  rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_RTC);
  rtc_wdt_set_time(RTC_WDT_STAGE0, 500);
  rtc_wdt_enable();
  rtc_wdt_protect_on();
  while(true);
}

// Convert ADC reading to voltage using calibrated Vref
// Source: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html
uint32_t adc_to_voltage(uint16_t adc) {
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  return esp_adc_cal_raw_to_voltage(adc, &adc_chars);
}

// Fatal error encountered eg. filesystem cannot be initialized etc.
// Just stop everything.
void fatal_error() {
  esp_deep_sleep_start();
}

void init_vars() {
  memset((void*)RTC_SLOW_MEM, 0, 8192);
  _set(VAR_SLEEP_INTERVAL, TUNE_INTERVALS[_get(VAR_TUNE_LEVEL)]);
  _set(VAR_ULP_TIMERH, HI_WORD(DEF_ULP_TIMER));
  _set(VAR_ULP_TIMERL, LO_WORD(DEF_ULP_TIMER));
  _set(VAR_STACK_PTR, VAR_STACK_REGION);
  _set(VAR_ADC_VDDH, 4095);
  for (uint16_t adc=2000; adc<4096; adc++) {
    uint32_t v = adc_to_voltage(adc);
    if (_get(VAR_ADC_VDDL) == 0 && v >= SUPPLY_VLOW/2) _set(VAR_ADC_VDDL, adc);
    if (_get(VAR_ADC_VDDH) == 4095 && v >= SUPPLY_VHIGH/2) _set(VAR_ADC_VDDH, adc);
  }
}

void init_gpio_pin(gpio_num_t pin, rtc_gpio_mode_t state, int level) {
  rtc_gpio_init(pin);
  rtc_gpio_set_direction(pin, state);
  rtc_gpio_set_drive_capability(pin, GPIO_DRIVE_CAP_3);
  rtc_gpio_set_level(pin, level); 
}

void init_gpio() {
  esp_deep_sleep_disable_rom_logging(); // Suppress boot messages
  rtc_gpio_isolate(GPIO_NUM_12); // Reduce current drain through pullups/pulldowns
  rtc_gpio_isolate(GPIO_NUM_15);
  init_gpio_pin(TICKPIN1_GPIO, RTC_GPIO_MODE_OUTPUT_ONLY, LOW);
  init_gpio_pin(TICKPIN2_GPIO, RTC_GPIO_MODE_OUTPUT_ONLY, LOW);
  init_gpio_pin(RESETBTN_PIN_GPIO, RTC_GPIO_MODE_INPUT_OUTPUT, HIGH);
}

void init_adc() {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(VDD_CHANNEL, ADC_ATTEN_DB_11);
  int sum = 0;
  for (int i=0; i<8; i++) sum += adc1_get_raw(VDD_CHANNEL);
  _set(VAR_ADC_VDD, sum/8);
  adc1_ulp_enable(); // This has to be done _after_ using adc1_get_raw(], otherwise I_ADC() will block
}

void load_and_run_ulp() {
  ulp_set_wakeup_period(0, VAR_ULP_TIMER());
  size_t size = sizeof(ulp_code) / sizeof(ulp_insn_t);
  int rc = patched_ulp_process_macros_and_load(ULP_PROG_START, ulp_code, &size);
  if (rc != ESP_OK) {
    debug("patched_ulp_process_macros_and_load() error: %d", rc);
    fatal_error();
  }
  debug("load_and_run_ulp: ULP code size=%d", size);
  // Calibrate 8M/256 clock against XTAL and patch up I_DELAY() instructions with recalibrated values
  uint32_t rtc_8md256_period;
  while(true) {
    rtc_8md256_period = rtc_clk_cal(RTC_CAL_8MD256, 100);
    if (rtc_8md256_period > 0) break;
    debug("load_and_run_ulp: rtc_clk_cal() timed out");
    delay(500);
  }
  uint32_t rtc_fast_freq_hz = 1000000ULL * (1 << RTC_CLK_CAL_FRACT) * 256 / rtc_8md256_period;
  uint32_t ulp_cycles_1ms = round((1.0/1000)/(1.0/rtc_fast_freq_hz));
  for (int i=0; i<size; i++) {
    int old_inst = RTC_SLOW_MEM[ULP_PROG_START+i];
    if ((old_inst & 0xffff0000) == 0x40000000) {
      int interval = old_inst & 0x0000ffff;
      interval *= ulp_cycles_1ms / 8000.0;
      RTC_SLOW_MEM[ULP_PROG_START+i] = 0x40000000 | interval; 
    }
  }
  ulp_run(ULP_PROG_START);
}

// Read config parameters from flash
#define SKIP_RTC_VARS 1
void load_config(int whichvars = 0) {
  // Read JSON as string
  if (!FILESYS.exists(CONFIG_FILE)) return;
  File file = FILESYS.open(CONFIG_FILE, FILE_READ);
  if (!file) fatal_error();
  String json; 
  while(file.available()) json += (char)file.read();
  file.close();
  // debug("load_config(): json = %s", json.c_str());
  if (json.length() == 0) return;
  // Parse JSON
  DynamicJsonDocument dict(512);
  deserializeJson(dict, json);
  strncpy(param_tz, dict["tz"], sizeof(param_tz)-1);
  strncpy(param_url, dict["url"], sizeof(param_url)-1);
  if (whichvars != SKIP_RTC_VARS) {
    if (dict.containsKey("hh")) {
      _set(VAR_CLK_HH, dict["hh"]);  
      _set(VAR_NET_HH, dict["hh"]);  
    }
    if (dict.containsKey("mm")) {
      _set(VAR_CLK_MM, dict["mm"]);  
      _set(VAR_NET_MM, dict["mm"]);  
    }
    if (dict.containsKey("ss")) {
      _set(VAR_CLK_SS, dict["ss"]);  
      _set(VAR_NET_SS, dict["ss"]);  
    }
    if (dict.containsKey("tickpin")) {
      _set(VAR_TICKPIN, dict["tickpin"]);
    }
    if (dict.containsKey("tune_level")) {
      _set(VAR_TUNE_LEVEL, dict["tune_level"]);
      _set(VAR_SLEEP_INTERVAL, TUNE_INTERVALS[_get(VAR_TUNE_LEVEL)]);
    }
    if (dict.containsKey("ulp_timer")) {
      int ulp_timer = dict["ulp_timer"];
      _set(VAR_ULP_TIMERH, HI_WORD(ulp_timer));
      _set(VAR_ULP_TIMERL, LO_WORD(ulp_timer));
    }
  }
//  debug("load_config(): ctime=%02d:%02d:%02d, ntime=%02d:%02d:%02d, tz=%s", 
//    _get(VAR_CLK_HH), _get(VAR_CLK_MM), _get(VAR_CLK_SS), _get(VAR_NET_HH), _get(VAR_NET_MM), _get(VAR_NET_SS), param_tz);
}

// Write config parameters to flash
void save_config() {
  DynamicJsonDocument dict(512);
  dict["tz"] = param_tz;
  dict["url"] = param_url;
  dict["hh"] = _get(VAR_CLK_HH);
  dict["mm"] = _get(VAR_CLK_MM);
  dict["ss"] = _get(VAR_CLK_SS);
  dict["tickpin"] = _get(VAR_TICKPIN);
  dict["tune_level"] = _get(VAR_TUNE_LEVEL);
  dict["ulp_timer"] = VAR_ULP_TIMER();
  File file = FILESYS.open(CONFIG_FILE, FILE_WRITE);
  if (!file) fatal_error();
  serializeJson(dict, file);
  file.close();
  debug("save_config(): ctime=%02d:%02d:%02d, ntime=%02d:%02d:%02d, tz=%s", 
    _get(VAR_CLK_HH), _get(VAR_CLK_MM), _get(VAR_CLK_SS), _get(VAR_NET_HH), _get(VAR_NET_MM), _get(VAR_NET_SS), param_tz);
}

// Parse config values entered in WifiManager's form
void parse_config() {
  char clocktime[7] = {0};
  strncpy(param_tz, form_timezone.getValue(), sizeof(param_tz) - 1);
  strncpy(param_url, form_scriptUrl.getValue(), sizeof(param_url) - 1);
  strncpy(clocktime, form_clockTime.getValue(), sizeof(clocktime) - 1); 
  int clock = atoi(clocktime);
  if (clock < 10000) clock *= 100;
  int ss = clock % 100; if (ss >= 60) ss = 0;
  int mm = (clock / 100) % 100; if (mm >= 60) mm = 0;
  int hh = clock / 10000; while(hh >= 12) hh -= 12;
  _set(VAR_CLK_HH, hh); _set(VAR_CLK_MM, mm); _set(VAR_CLK_SS, ss); 
  _set(VAR_NET_HH, hh); _set(VAR_NET_MM, mm); _set(VAR_NET_SS, ss); 
}

// Source: https://github.com/espressif/arduino-esp32/issues/400
void clear_wifi_credentials() { 
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg); //initiate and allocate wifi resources (does not matter if connection fails)
  delay(2000); //wait a bit
  esp_wifi_restore();
  delay(1000);
}

char* getClockName() {
  static char clockname[64];
  uint64_t chipid = ESP.getEfuseMac();
  uint16_t chip = (uint16_t)(chipid >> 32);
  snprintf(clockname, sizeof(clockname), "ESPCLOCK4-%04X%08X", chip, (uint32_t)chipid);
  return clockname;
}

// Connect to WiFi using WiFiManager
bool init_wifi(int timeout = 10) {
  wifimgr.setDebugOutput(false);
  wifimgr.setCustomHeadElement(jscript);
  wifimgr.addParameter(&form_clockTime);
  wifimgr.addParameter(&form_timezone);
  wifimgr.addParameter(&form_scriptUrl);
	
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Note: built-in LED for ESP32 D1 Mini is active high

  bool success = false;
  if (FILESYS.exists(CONFIG_FILE)) {
//    debug("wifimgr.autoConnect()");
    wifimgr.setConfigPortalTimeout(timeout);
    success = wifimgr.autoConnect(getClockName());
    debug("wifimgr.autoConnect(); success = %d", success);
  } else {
    debug("wifimgr.startConfigPortal()");
    success = wifimgr.startConfigPortal(getClockName());
    parse_config();
    save_config();
    debug("wifimgr.startConfigPortal(); success = %d", success);
  }
  
	digitalWrite(LED_BUILTIN, LOW);

  if (!success) return false;

  WiFi.setAutoReconnect(true);  
  WiFi.persistent(true);

  return true; 
}

bool get_nettime() {
  String url = param_url;
  String tz = param_tz;
  tz.replace("/", "%2F");
  url.replace("[tz]", tz);
  WiFiClient wifi;
  HTTPClient http;
  http.begin(wifi, url.c_str());
  int rc = http.GET();
  if (rc == HTTP_CODE_OK) {
    String payload = http.getString();
    if (payload.length() >= 8) {
      int hh = payload.substring(0, 2).toInt();
      int mm = payload.substring(3, 5).toInt();
      int ss = payload.substring(6, 8).toInt();
      if (hh >= 12) hh -= 12;
      if (hh < 0 || hh > 23) hh = 0;
      if (mm < 0 || mm > 59) mm = 0;
      if (ss < 0 || ss > 59) ss = 0;
      _set(VAR_DEBUG, 0);
      _set(VAR_NET_HH, hh);
      _set(VAR_NET_MM, mm);
      _set(VAR_NET_SS, ss);
      return true;
    }
  }
  return false;
}

// Get network time and match against internal network time to adjust ULP interval so that we get as close as possible to 1sec
void tune_ulp_timer() {
  int nethh = _get(VAR_NET_HH), netmm = _get(VAR_NET_MM), netss = _get(VAR_NET_SS), old_sleep_count = _get(VAR_SLEEP_COUNT);
  if (!init_wifi() || !get_nettime()) {
    debug("tune_ulp_timer() failed: ct=%02d:%02d:%02d, nt=%02d:%02d:%02d, tune_level=%d, ulp_sleep=%d, vlow=%d, vhigh=%d, vdd=%d", 
      _get(VAR_CLK_HH), _get(VAR_CLK_MM), _get(VAR_CLK_SS), _get(VAR_NET_HH), _get(VAR_NET_MM), _get(VAR_NET_SS), _get(VAR_TUNE_LEVEL), VAR_ULP_TIMER(), 
      adc_to_voltage(_get(VAR_ADC_VDDL))*2, adc_to_voltage(_get(VAR_ADC_VDDH))*2, adc_to_voltage(_get(VAR_ADC_VDD))*2);
    return;
  }
  int offset = _get(VAR_SLEEP_COUNT) - old_sleep_count;
  time_t diff = ((nethh * 3600L) + (netmm * 60L) + (netss + offset)) - ((_get(VAR_NET_HH) * 3600L) + (_get(VAR_NET_MM) * 60L) + _get(VAR_NET_SS));
  if (abs(diff) > 60) {
    char prefix[512]; 
    sprintf(prefix, "tune_ulp_timer() aborted (mac=%s, old_nt=%02d:%02d:%02d, offset=%d, diff=%d)", 
      WiFi.macAddress().c_str(), nethh, netmm, netss, offset, diff);
    status_vars(prefix);
    return; // Do not adjust timer if net time is off by > 60secs
  }
  float multipler = (float)(_get(VAR_SLEEP_INTERVAL) + diff) / (_get(VAR_SLEEP_INTERVAL));
  int old_timer = VAR_ULP_TIMER();
  int new_timer = old_timer * multipler;
  {
    char prefix[512]; 
    sprintf(prefix, "tune_ulp_timer() update (mac=%s, old_nt=%02d:%02d:%02d, offset=%d, diff=%d, multiplier=%f, old_ulp_sleep=%d, new_ulp_sleep=%d)", 
      WiFi.macAddress().c_str(), nethh, netmm, netss, offset, diff, multipler, old_timer, new_timer);
    status_vars(prefix);
  }
  int tune_level = _get(VAR_TUNE_LEVEL);
  if (tune_level < (sizeof(TUNE_INTERVALS)/sizeof(int))-1) {
    tune_level += 1;
    _set(VAR_TUNE_LEVEL, tune_level);
    _set(VAR_SLEEP_INTERVAL, TUNE_INTERVALS[tune_level]);
  }
  if (abs(diff) <= 5) return; // Do not adjust ULP timer if net time is off by <= 5secs
  _set(VAR_ULP_TIMERH, HI_WORD(new_timer));
  _set(VAR_ULP_TIMERL, LO_WORD(new_timer));
  if (old_timer != new_timer) { 
    ulp_set_wakeup_period(0, VAR_ULP_TIMER());
  }
}

void startup() {
  // Perform factory reset?
  pinMode(RESETBTN_PIN_GPIO, INPUT_PULLUP);
  bool reset_btn = !digitalRead(RESETBTN_PIN_GPIO);
  if (reset_btn && FILESYS.exists(CONFIG_FILE)) {
    delay(500);
    FILESYS.remove(CONFIG_FILE);
    clear_wifi_credentials();
    rtc_reset();
  }

  // Init config and overwrite with config from flash if available
  init_vars();
  load_config();

  // Initialize ADC and GPIO
  init_adc(); _set(VAR_ADC_VDD, _get(VAR_ADC_VDDH));
  init_gpio();

  // Skip if VDD is below minimum threshold
  if (_get(VAR_ADC_VDD) >= _get(VAR_ADC_VDDL)) {
    if (!FILESYS.exists(CONFIG_FILE)) {
      init_wifi();
      status("startup(): factory reset");
    } else {
      status("startup(): normal");
    }
    // Schedule net time update in 5s
    _set(VAR_UPDATE_PENDING, 5);
  }
}

void wakeup_ulp() {
  // This needs to be done ASAP, otherwise ULP will hang at I_ADC()
  adc1_ulp_enable();

  // If VDD is below minimum level, save config to flash and fall back to deep sleep
  load_config(SKIP_RTC_VARS);
  if (_get(VAR_ADC_VDD) < _get(VAR_ADC_VDDL)) {
    save_config();
    return;
  }

  // Otherwise, handle ULP wakeup reason
  switch(_get(VAR_WAKE_REASON)) {
    case WAKE_UPDATE_NETTIME: {
      int oldhh = _get(VAR_NET_HH), oldmm = _get(VAR_NET_MM), oldss = _get(VAR_NET_SS);
      if (init_wifi()) {
        bool rc = get_nettime();
        char prefix[64]; 
        sprintf(prefix, "Update nettime (rc=%d; old_nt=%02d:%02d:%02d)", rc, oldhh, oldmm, oldss);
        status_vars(prefix);
      }
      save_config();
      break;
    }
    case WAKE_TUNE_ULP_TIMER: {
      tune_ulp_timer();
      save_config();
      if (!WiFi.isConnected()) {
        debug("WiFi disconnected; set sleep_interval to %d", 60);
        _set(VAR_SLEEP_INTERVAL, 60);
      }
      break;
    }
    case WAKE_RESET_BUTTON: {
      save_config();
      if (_get(VAR_PAUSE_CLOCK) == 1) {
        // For the next second, check if reset button is held down. If so, perform reset.
        bool long_press = true;
        for (int i=0; i<1000/5; i++) {
          if (digitalRead(RESETBTN_PIN_GPIO) == HIGH) {
            long_press = false;
            break;
          }
          delay(5);
        }
        if (long_press) {
          delay(500);
          rtc_reset();
        } else {
          if (init_wifi()) status_vars("Clocked paused");
          _set(VAR_PAUSE_CLOCK, 2);
        }
      }
      break;
    }
  }
}

void setup() {
  // Initialization
  setCpuFrequencyMhz(80); // Reduce CPU frequency to save power
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout for more stable battery operation
  init_debug();
  if (!FILESYS.begin(true)) fatal_error();

  // This function is called either due to initial powerup or ULP wakeup
  wake_cause = esp_sleep_get_wakeup_cause();
  #ifdef STRESS_TEST
    switch(wake_cause) {
      case ESP_SLEEP_WAKEUP_UNDEFINED: {
        init_vars(); 
        init_gpio();
        _set(VAR_CLK_SS, 4);
        _set(VAR_SLEEP_COUNT, 12*60*60);
        _set(VAR_TICK_ACTION, TICK_FWD);
        _set(VAR_TICK_DELAY, 5);
        while(digitalRead(RESETBTN_PIN_GPIO) == HIGH);
        break;
      }
      case ESP_SLEEP_WAKEUP_ULP: {
        delay(5000);
        int randnum = random(1,6)*5;
        int count = _get(VAR_SLEEP_COUNT);
        while(count == 0) delay(60000);
        if (randnum >= count) randnum = count;
        _set(VAR_SLEEP_COUNT, count - randnum);
        _set(VAR_TICK_ACTION, TICK_REV);
        _set(VAR_TICK_DELAY, randnum); 
        _set(VAR_PAUSE_CLOCK, 0);
        break;
      }
      default:
        break;
    }
  #else // !STRESS_TEST
    switch(wake_cause) {
      case ESP_SLEEP_WAKEUP_UNDEFINED:
        startup();
        status_vars("startup()");
        break;
      case ESP_SLEEP_WAKEUP_ULP:
        wakeup_ulp();
        break;
    }
  #endif // STRESS_TEST

  // Power off RTC FAST MEM during deep sleep to save power
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);

  // Sleep now and let ULP take over
  if (wake_cause == ESP_SLEEP_WAKEUP_UNDEFINED) load_and_run_ulp(); 
  esp_sleep_enable_ulp_wakeup(); 
  esp_deep_sleep_start();
}

void loop() {
  // Unused
}
