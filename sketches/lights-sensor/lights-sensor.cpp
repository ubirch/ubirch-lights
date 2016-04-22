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

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <Arduino.h>
#include <UbirchSIM800.h>
#include <Base64.h>

extern "C" {
#include <avrnacl.h>
#include <i2c.h>
#include <isl29125.h>
}

#ifndef BAUD
#   define BAUD 9600
#endif

#define SLEEP_CYCLES 3350

#define led 13
#define trigger 6

UbirchSIM800 sim800h = UbirchSIM800();

static int loop_counter = 1;
static uint8_t sensor_config = ISL_MODE_375LUX | ISL_MODE_12BIT | ISL_MODE_RGB;
static uint8_t sensor_config_ir = ISL_FILTER_IR_NONE + 0x20;

void sleepabit(int howlong) {
  int i2 = 0;
  delay(100);
  while (i2 < (howlong / 8)) {
    cli();
    delay(100);
    // disable ADC
    //ADCSRA = 0;
    //prepare interrupts
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    // Set Watchdog settings:
    WDTCSR = (1 << WDIE) | (1 << WDE) | (1 << WDP3) | (0 << WDP2) | (0 << WDP1) | (1 << WDP0);
    sei();
    //wdt_reset();
    set_sleep_mode (SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    // turn off brown-out enable in software
    //MCUCR = bit (BODS) | bit (BODSE);
    //MCUCR = bit (BODS);
    sleep_cpu();
    // cancel sleep as a precaution
    sleep_disable();
    i2++;
  }
  wdt_disable();
}

void getRGB(uint8_t &red, uint8_t &green, uint8_t &blue) {
  i2c_init(I2C_SPEED_400KHZ);

  isl_reset();
  isl_set(ISL_R_COLOR_MODE, sensor_config);
  isl_set(ISL_R_FILTERING, sensor_config_ir);

  while (!(isl_get(ISL_R_STATUS) & ISL_STATUS_ADC_DONE)) Serial.write('%');
  Serial.println();
  for (uint8_t n = 0; n < 5; n++) {
    red = isl_read_red8();
    green = isl_read_green8();
    blue = isl_read_blue8();
  }
  Serial.println(F("RGB conversion done."));

  Serial.print(red);
  Serial.print(F(":"));
  Serial.print(green);
  Serial.print(F(":"));
  Serial.println(blue);
}

void SendGPS() {
  uint8_t red1 = 0, green1 = 0, blue1 = 0;
  uint16_t bat_status = 0, bat_percent = 0, bat_voltage = 0;
  char *lat, *lon;
  char *payload, *payload_hash, *auth_hash;
  char *sig, *message;

  getRGB(red1, green1, blue1);

  // wake up the SIM800 and start GPRS
  if (!sim800h.wakeup()) return;
  if (!sim800h.registerNetwork(60000)) return;
  if (!sim800h.enableGPRS()) return;

  // read battery status
  if (!sim800h.battery(bat_status, bat_percent, bat_voltage)) {
    Serial.println(F("BAT status failed"));
  }

  // read GSM approx. location
  sim800h.location(lat, lon);

  // read device IMEI, which is our key
  payload = (char *) malloc(128);
  if (!sim800h.IMEI(payload)) {
    Serial.println(F("IMEI not found, can't send"));
    free(payload);
    free(lat);
    free(lon);
    return;
  };

  // hashed payload structure IMEI{DATA}
  // Example: '123456789012345{"r":44,"g":33,"b":22,"lat":"12.475886","lon":"51.505264","bat":100,"lps":99999}'
  sprintf_P(payload + 15,
            PSTR("{\"r\":%3d,\"g\":%3d,\"b\":%3d,\"lat\":\"%s\",\"lon\":\"%s\",\"bat\":%3d,\"lps\":%d}"),
            red1, green1, blue1, lat, lon, bat_percent, loop_counter);

  // free latitude and longitude
  free(lat);
  free(lon);

  Serial.print("payload: '");
  Serial.print(payload);
  Serial.println("'");

  // create hashes from the payload structure as well as the IMEI (key), sig buffer is used twice!
  sig = (char *) malloc(crypto_hash_BYTES);

  payload_hash = (char *) malloc(89);
  crypto_hash((unsigned char *) sig, (const unsigned char *) payload, strlen(payload));
  base64_encode(payload_hash, sig, crypto_hash_BYTES);

  auth_hash = (char *) malloc(89);
  Serial.print(F("payload hash: "));
  Serial.println(payload_hash);

  crypto_hash((unsigned char *) sig, (const unsigned char *) payload, 15);
  base64_encode(auth_hash, sig, crypto_hash_BYTES);

  // free signature buffer
  free(sig);

  Serial.print(F("auth hash   : "));
  Serial.println(auth_hash);

  // finally, compile the message
  message = (char *) malloc(300);
  sprintf_P(message,
            PSTR("{\"v\":\"0.0.1\",\"a\":\"%s\",\"s\":\"%s\",\"p\":%s}"),
            auth_hash, payload_hash, payload + 15);

  // free payload and hashes
  free(payload);
  free(payload_hash);
  free(auth_hash);

  Serial.print(F("message: '"));
  Serial.print(message);
  Serial.println("'");
  Serial.println(strlen(message));

  // send the request
  unsigned long response_length;
  unsigned int http_status;
  http_status = sim800h.HTTP_post(PUSH_URL, response_length, message, strlen(message));
  // free message after it's been sent
  free(message);

  if (http_status != 200) {
    Serial.println(F("HTTP POST failed"));
    Serial.println(http_status);
    Serial.println(response_length);
  } else {
    if (response_length < 300) {
      Serial.println(F("== RESPONSE =="));
      //
      char *response = (char *) malloc((size_t) response_length);
      sim800h.HTTP_read(response, 9, (size_t) response_length);
      Serial.println(response);
      Serial.println("== END ==");
      // free the response buffer
      free(response);
    } else {
      Serial.print("HTTP RESPONSE too long: ");
      Serial.println(response_length);
    }
  }
  sim800h.shutdown();
}

void setup() {
  Serial.begin(115200);
  Serial.println("WELCOME!");

  pinMode(led, OUTPUT);
  pinMode(trigger, INPUT);
  digitalWrite(led, HIGH);
  delay(100);
  digitalWrite(led, LOW);
  delay(100);

  // edit APN settings in config.h
  sim800h.setAPN(F(FONA_APN), F(FONA_USER), F(FONA_PASS));
}

void loop() {
  digitalWrite(led, HIGH);
  pinMode(trigger, INPUT);

  delay(1000);
  SendGPS();

  pinMode(trigger, OUTPUT);
  digitalWrite(led, LOW);
  loop_counter++;

//  sleepabit(SLEEP_CYCLES);
  delay(1000);
}

// the ISR is necessary to allow the CPU from actually sleeping
ISR (WDT_vect) { }

