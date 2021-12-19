/*
 * espclock4.h
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

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <HTTPClient.h>
#include <EEPROM.h>

// Uncomment to perform stress test of fastforward/reverse clock movement
//#define STRESS_TEST

// Uncomment to make status logging function "status()" via syslog available
//#define STATUS

// Uncomment to make debugging function "serial()" via serial interface available
//#define DEBUG

#include "debug.h"

// Default values
#define DEFAULT_SCRIPT_URL      "http://espclock.randseq.org/now.php?tz=[tz]"
#define CONFIG_FILE             "/espclock.ini"
#define FILESYS                 LittleFS

// Utility macros
#define LO_WORD(x)            ((uint16_t)((x) & 0x0000ffff))
#define HI_WORD(x)            ((uint16_t)(((x) & 0xffff0000) >> 16))
#define MAKE_INT(hi, lo)      (((uint32_t)(hi) << 16) | (uint32_t)(lo))
#define _get(var)             (RTC_SLOW_MEM[var] & 0xffff)
#define _set(var, value)      RTC_SLOW_MEM[var] = value
#define DEF_ULP_TIMER         (((1000/ULP_CALL_PER_SEC)-MAX_PULSE_MS)*1000)
#define VAR_ULP_TIMER()       MAKE_INT(_get(VAR_ULP_TIMERH), _get(VAR_ULP_TIMERL))

// Tuning intervals for ULP timer so that we get as close to 1sec/tick as possible
// Start with: 5min, 15min, 30min, 1hr, 2hr (max)
int TUNE_INTERVALS[] = { 5*60, 15*60, 30*60, 60*60, 2*60*60 };

// ULP program
#include "ulpcode.h"
