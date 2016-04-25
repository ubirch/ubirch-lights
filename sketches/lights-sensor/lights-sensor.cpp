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
#include <Base64.h>
#include <jsmn.h>

extern "C" {
#include <i2c.h>
#include <isl29125.h>
#include <avrnacl.h>
#include <avrsleep.h>
}

#ifndef BAUD
#   define BAUD 9600
#endif

// default wakup interval in seconds
#define DEFAULT_INTERVAL 5*60

#define LED 13
#define WATCHDOG 6

// protocol version check
#define PROTOCOL_VERSION_MIN "0.0"
// json keys
#define P_SIGNATURE "s"
#define P_VERSION "v"
#define P_PAYLOAD "p"
#define P_SENSITIVITY "s"
#define P_IR_FILTER "ir"
#define P_INTERVAL "i"

UbirchSIM800 sim800h = UbirchSIM800();

// this counts up as long as we don't have a reset
static int loop_counter = 1;

// internal sensor state
static uint16_t interval = DEFAULT_INTERVAL;
static uint8_t rgb_config = ISL_MODE_375LUX;
static uint8_t rgb_ir_filter = ISL_FILTER_IR_NONE + 0x20;

static int jsoneq(const char *json, jsmntok_t &token, const char *key) {
  if (token.type == JSMN_STRING &&
      strlen(key) == (size_t) (token.end - token.start) &&
      strncmp(json + token.start, key, (size_t) (token.end - token.start)) == 0) {
    return 0;
  }
  return -1;
}

// JSMN helper function to print the current token for debugging
static inline void print_token(const char *response, jsmntok_t &token) {
  Serial.write(response + token.start, (size_t) (token.end - token.start));
  Serial.println();
}

// convert a number of characters into an unsigned integer value
unsigned int to_uint(const char *ptr, size_t len) {
  unsigned int ret = 0;
  for (uint8_t i = 0; i < len; i++) {
    ret = (ret * 10) + (ptr[i] - '0');
  }
  return ret;
}

/*!
 * Process the JSON response from the backend. It should contain configuration
 * parameters that need to be set. The response must be signed and will be
 * checked for signature match and protocol version.
 *
 * @param response the string response
 */
void process_response(const char *response) {
  jsmntok_t *token;
  jsmn_parser parser;
  jsmn_init(&parser);

  // identify the number of tokens in our response, we expect 13
  const uint8_t token_count = (const uint8_t) jsmn_parse(&parser, response, strlen(response), NULL, 0);
  token = (jsmntok_t *) malloc(sizeof(*token) * token_count);

  // TODO check token count and return if too many

  // reset parser, parse and store tokens
  jsmn_init(&parser);
  if (jsmn_parse(&parser, response, strlen(response), token, token_count) == token_count) {
    uint8_t index = 0;
    if (token[0].type == JSMN_OBJECT) {
      while(++index < token_count) {
        char sig[crypto_hash_BYTES];

        if (jsoneq(response, token[index], P_VERSION) == 0 && token[index + 1].type == JSMN_STRING) {
          index++;
          if (strncmp_P(response + token[index].start, PSTR(PROTOCOL_VERSION_MIN), 3) != 0) {
            Serial.print(F("protocol version mismatch: "));
            print_token(response, token[index]);
            // do not continue if the version does not match
            break;
          }
        } else if (jsoneq(response, token[index], P_SIGNATURE) == 0 && token[index + 1].type == JSMN_STRING) {
          index++;
          Serial.print(F("signature: "));
          print_token(response, token[index]);
          // decode signature and store in sig
          base64_decode(sig, (char *) (response + token[index].start), token[index].end - token[index].start);
        } else if (jsoneq(response, token[index], P_PAYLOAD) == 0 && token[index + 1].type == JSMN_OBJECT) {
          index++;
          Serial.print(F("payload: "));
          print_token(response, token[index]);

          // check signature
          uint8_t payload_length = (uint8_t) (token[index].end - token[index].start);
          char *payload = (char *) malloc((size_t) (15 + payload_length + 1));
          char *payload_hash = (char *) malloc(crypto_hash_BYTES);
          sim800h.IMEI(payload);
          strncpy(payload + 15, response + token[index].start, payload_length);
          payload[15 + payload_length] = '\0';

          // hash payload and check whether it matches the signature hash
          crypto_hash_sha512((unsigned char *) payload_hash, (const unsigned char *) payload, strlen(payload));
          bool signature_verified = !memcmp(sig, payload_hash, crypto_hash_BYTES);

          // free the payload and its hash
          free(payload);
          free(payload_hash);

          // if the signature is verified, parse the contents
          if (signature_verified) {
            Serial.println(F("Signature verified: OK"));
            // only loop through as many keys as there are in the payload
            uint8_t pkeys = (uint8_t) token[index].size;
            while(pkeys-- && ++index < token_count) {
              if (jsoneq(response, token[index], P_SENSITIVITY) == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
                index++;
                Serial.print(F("Sensitivity: "));
                if (*(response + token[index].start) - '0') {
                  Serial.println("10K lux");
                  rgb_config = ISL_MODE_10KLUX;
                } else {
                  Serial.println("375 lux");
                  rgb_config = ISL_MODE_375LUX;
                }
              } else if (jsoneq(response, token[index], P_IR_FILTER) == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
                index++;
                Serial.print(F("Infrared filter: 0x"));
                rgb_ir_filter = to_uint(response + token[index].start, (size_t) token[index].end - token[index].start);
                Serial.println(rgb_ir_filter, 16);
              } else if (jsoneq(response, token[index], P_INTERVAL) == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
                index++;
                Serial.print(F("Interval: "));
                interval = to_uint(response + token[index].start, (size_t) token[index].end - token[index].start);
                Serial.print(interval);
                Serial.println(F("s"));
              } else {
                Serial.print(F("unknown payload key: "));
                print_token(response, token[index]);
                index++;
              }
            }
          } else {
            Serial.println(F("Signature failed to verify!"));
            break;
          }
        } else {
          Serial.print(F("unknown key: "));
          print_token(response, token[index]);
          index++;
        }
      }
    }
  }
  free(token);
}

/*!
 * Sample light data via the external RGB sensor.
 *
 * @param red part - passed by reference
 * @param green part - passed by reference
 * @param blue part - passed by reference
 */
void sample_rgb(uint8_t &red, uint8_t &green, uint8_t &blue) {
  i2c_init(I2C_SPEED_400KHZ);

  isl_reset();
  isl_set(ISL_R_COLOR_MODE, rgb_config | ISL_MODE_12BIT | ISL_MODE_RGB);
  isl_set(ISL_R_FILTERING, rgb_ir_filter);

  while (!(isl_get(ISL_R_STATUS) & ISL_STATUS_ADC_DONE)) Serial.write('%');
  Serial.println();
  for (uint8_t n = 0; n < 5; n++) {
    red = isl_read_red8();
    green = isl_read_green8();
    blue = isl_read_blue8();
  }

  Serial.print(F("RGB: "));
  Serial.print(red);
  Serial.print(F(":"));
  Serial.print(green);
  Serial.print(F(":"));
  Serial.println(blue);
}

/*!
 * Send samples sensor data to the backend. The payload message will be signed
 * using a board specific key.
 */
void send_sensor_data() {
  uint8_t red = 0, green = 0, blue = 0;
  uint16_t bat_status = 0, bat_percent = 0, bat_voltage = 0;
  char *lat = NULL, *lon = NULL, *date = NULL, *time = NULL;
  char *payload, *payload_hash, *auth_hash;
  char *sig, *message;

  // sample light data
  sample_rgb(red, green, blue);

  // read battery status
  sim800h.battery(bat_status, bat_percent, bat_voltage);
  // read GSM approx. location
  if (sim800h.location(lat, lon, date, time)) {
    Serial.print(F(">>> "));
    Serial.print(date);
    Serial.print(F(" "));
    Serial.println(time);
  }

  // read device IMEI, which is our key
  payload = (char *) malloc(128);
  if (sim800h.IMEI(payload)) {
    Serial.print(F("authorization: "));
    Serial.println(payload);
  } else {
    Serial.println(F("IMEI not found, can't send"));
    free(payload);
    free(lat);
    free(lon);
    free(date);
    free(time);
    return;
  }

  // hashed payload structure IMEI{DATA}
  // Example: '123456789012345{"r":44,"g":33,"b":22,"lat":"12.475886","lon":"51.505264","bat":100,"lps":99999}'
  sprintf_P(payload + 15,
            PSTR("{\"r\":%3d,\"g\":%3d,\"b\":%3d,\"la\":\"%s\",\"lo\":\"%s\",\"ba\":%3d,\"lp\":%d}"),
            red, green, blue, lat == NULL ? "" : lat, lon == NULL ? "" : lon, bat_percent, loop_counter);

  // free latitude and longitude
  free(lat);
  free(lon);
  free(date);
  free(time);

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

      // parse response json
      process_response(response);

      // free the response buffer
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
  delay(100);
  digitalWrite(LED, LOW);
  delay(100);

  // edit APN settings in config.h
  sim800h.setAPN(F(FONA_APN), F(FONA_USER), F(FONA_PASS));
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
    for (uint8_t tries = 2; tries > 0; tries--) {
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
