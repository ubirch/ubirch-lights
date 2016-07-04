/*!
 * ubirch lights sensor (ubirch #1 r0.1)
 *
 * Adapted from Stephan Nollers code: https://github.com/snoller/Ardufona2Thingspeak
 *
 * copy config_template.h to config.h and set the correct values
 *
 * Copyright 2015 ubirch GmbH (http://www.ubirch.com)
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

#include "config.h"

#include <Arduino.h>
#include <UbirchSIM800.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_BME280.h>
#include <Base64.h>
#include <jsmn.h>
#include <i2c.h>
#include <avrsleep.h>
#include <freeram.h>

extern "C" {
#include <avrnacl.h>
}

#ifndef BAUD
#   define BAUD 9600
#endif

// default wakup interval in seconds
#define DEFAULT_INTERVAL 5*60

#define LED 13
#define WATCHDOG 6

UbirchSIM800 sim800h = UbirchSIM800();
Adafruit_BMP085 sensor = Adafruit_BMP085();
//Adafruit_BME280 sensor = Adafruit_BME280();

// this counts up as long as we don't have a reset
static uint16_t loop_counter = 1;
static uint8_t error_flag = 0x00;

// internal sensor state
static uint16_t interval = DEFAULT_INTERVAL;

// convert a number of characters into an unsigned integer value
static unsigned int to_uint(const char *ptr, size_t len) {
  unsigned int ret = 0;
  for (uint8_t i = 0; i < len; i++) {
    ret = (ret * 10) + (ptr[i] - '0');
  }
  return ret;
}

// print a hex representation of the has byte array
static inline void print_hash(const char *sig) {
  Serial.print(F("[HASH] "));
  for (uint8_t i = 0; i < crypto_hash_BYTES; i++) Serial.print((unsigned char) sig[i], 16);
  Serial.println();
}

/*!
 * Send samples sensor data to the backend. The payload message will be signed
 * using a board specific key.
 */
void send_sensor_data() {
  uint16_t red = 0, green = 0, blue = 0;
  uint16_t bat_status = 0, bat_percent = 0, bat_voltage = 0;
  char *lat = NULL, *lon = NULL, *date = NULL, *time = NULL;
  char *message;

  float temperature = sensor.readTemperature();
  float pressure = sensor.readPressure();
  float humidity = 0.0/*sensor.readHumidity()*/;

  // read battery status
  sim800h.battery(bat_status, bat_percent, bat_voltage);

  message = (char *)malloc(128);
  sprintf_P(message,
            PSTR("{\"data\":{\"t\":%.2f,\"p\":%.0f,\"h\":%.2f}}"),
            temperature, pressure, humidity);


  Serial.print(F("message: '"));
  Serial.print(message);
  Serial.println(F("'"));
  Serial.println(strlen(message));

  // send the request
  unsigned long response_length;
  unsigned int http_status;
  http_status = sim800h.HTTP_post(PUSH_URL, response_length, message, strlen(message));
  // free message after it's been sent
  free(message);

  Serial.print(http_status);
  Serial.print(F(" ("));
  Serial.print(response_length);
  Serial.println(F(")"));

  if (http_status != 200) {
    Serial.println(F("HTTP POST failed"));
  } else {
    if (response_length < 300) {
      char *response = (char *) malloc((size_t) response_length + 1);
      response[response_length] = '\0';

      // we need to read the response in little chunks, else the
      // software serial will just return trash, omissions etc.
      uint32_t pos = 0;
      do pos += sim800h.HTTP_read(response + pos, pos, SIM800_BUFSIZE); while (pos < response_length);

      Serial.print(F("RESPONSE: '"));
      Serial.print(response);
      Serial.println(F("'"));

      free(response);
    } else {
      Serial.print(F("HTTP RESPONSE too long: "));
      Serial.println(response_length);
    }
  }
}

/*!
 * Initial setup.
 */
void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(LED, OUTPUT);
  pinMode(WATCHDOG, INPUT);

  digitalWrite(LED, HIGH);
  sleep(1);
  digitalWrite(LED, LOW);
  sleep(1);

  // edit APN settings in config.h
  sim800h.setAPN(F(FONA_APN), F(FONA_USER), F(FONA_PASS));

  if(!sensor.begin()) {
    Serial.println("Sensor not detected");
  }
}

/*!
 * Main loop. Initialized the mobile network initiates the
 * RGB data sending. Will sleep a set amount of seconds before
 * it finishes.
 */
void loop() {
  digitalWrite(LED, HIGH);
  pinMode(WATCHDOG, INPUT);

  // wake up the SIM800
  if (sim800h.wakeup()) {
    // try to connect and enable GPRS, send if successful
    uint8_t tries;
    for (tries = 2; tries > 0; tries--) {
      if (sim800h.registerNetwork(60000) && sim800h.enableGPRS()) {
        send_sensor_data();
        break;
      }
      Serial.println();
      Serial.println(F("mobile network failed"));
    }
  }
  sim800h.shutdown();

  pinMode(WATCHDOG, OUTPUT);
  digitalWrite(LED, LOW);
  loop_counter++;

  Serial.print(F("sleeping for "));
  Serial.print(interval);
  Serial.println(F("s"));
  delay(100);

  // sleep interval seconds (put MCU in low power mode)
  sleep(interval);
}
