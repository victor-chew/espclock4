/*
 * debug.h
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

#ifdef STATUS

#include <Syslog.h>

char* getClockName();

void status(const char *format, ...) {
  WiFiUDP udpClient;
  Syslog syslog(udpClient, "192.168.1.2", 514, getClockName(), "", LOG_KERN, SYSLOG_PROTO_BSD);

  char buf[1024];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  
  Serial.println(buf);

  if (WiFi.status() == WL_CONNECTED) { 
    syslog.log(buf); 
    delay(500); 
  }
}

#else // !STATUS

void status(const char *format, ...) {}

#endif // STATUS

#ifdef DEBUG 

void init_debug() {
  Serial.begin(115200);
}

void debug(const char *format, ...) {
  char buf[300];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);
  Serial.println(buf);
}

#else // !DEBUG

void init_debug() {}
void debug(const char *format, ...) {}

#endif // DEBUG

