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

#define led 13
#define trigger 6

UbirchSIM800 sim800h = UbirchSIM800();

static int loop_counter = 1;

static uint16_t interval = DEFAULT_INTERVAL;
static uint8_t rgb_config = ISL_MODE_375LUX;
static uint8_t rgb_ir_filter = ISL_FILTER_IR_NONE + 0x20;

static int jsoneq(const char *json, jsmntok_t *token, const char *key) {
  if (token->type == JSMN_STRING &&
      strlen(key) == (size_t) (token->end - token->start) &&
      strncmp(json + token->start, key, (size_t) (token->end - token->start)) == 0) {
    return 0;
  }
  return -1;
}

static inline void print_token(const char *response, jsmntok_t &token) {
  Serial.write(response + token.start, (size_t) (token.end - token.start));
  Serial.println();
}

static inline void print_hash(char *sig) {
  Serial.print(F("[HASH] "));
  for (uint8_t i = 0; i < crypto_hash_BYTES; i++) Serial.print((unsigned char) sig[i], 16);
  Serial.println();
}

unsigned int to_uint(const char *ptr, size_t len) {
  unsigned int ret = 0;
  for (uint8_t i = 0; i < len; i++) {
    ret = (ret * 10) + (ptr[i] - '0');
  }
  return ret;
}

void process_response(const char *response) {
  jsmntok_t *token;
  jsmn_parser parser;
  jsmn_init(&parser);

  // identify the number of tokens in our response, we expect 13
  const uint8_t token_count = (const uint8_t) jsmn_parse(&parser, response, strlen(response), NULL, 0);
  token = (jsmntok_t *) malloc(sizeof(*token) * token_count);

  // reset parser, parse and store tokens
  jsmn_init(&parser);
  if (jsmn_parse(&parser, response, strlen(response), token, token_count) == token_count && token_count < 15) {
    uint8_t index = 0;
    if (token[index].type == JSMN_OBJECT) {
      while (index++ < token_count) {
        char sig[crypto_hash_BYTES];
        if (jsoneq(response, &token[index], "v") == 0) {
          Serial.print(F("protocol version: "));
          print_token(response, token[++index]);
        } else if (jsoneq(response, &token[index], "s") == 0) {
          Serial.print(F("signature: "));
          print_token(response, token[++index]);
          base64_decode(sig, (char *) (response + token[index].start), token[index].end - token[index].start);

          print_hash(sig);

        } else if (jsoneq(response, &token[index], "p") == 0 && token[index + 1].type == JSMN_OBJECT) {
          Serial.print(F("payload: "));
          print_token(response, token[++index]);

          // check signature
          uint8_t payload_length = (uint8_t) (token[index].end - token[index].start);
          char *payload = (char *) malloc((size_t) (15 + payload_length + 1));
          char *payload_hash = (char *) malloc(crypto_hash_BYTES);
          sim800h.IMEI(payload);
          strncpy(payload + 15, response + token[index].start, payload_length);
          payload[15 + payload_length] = '\0';

          Serial.println(payload);

          crypto_hash_sha512((unsigned char *) payload_hash, (const unsigned char *) payload, strlen(payload));
          bool signature_verified = !memcmp(sig, payload_hash, crypto_hash_BYTES);

          print_hash(payload_hash);

          // free the payload and its hash
          free(payload);
          free(payload_hash);

          // if the signature is verified, parse the contents
          if (signature_verified) {
            Serial.println(F("Signature verified: OK"));
            uint8_t expected_tokens = 3;
            while (expected_tokens && index++ < token_count) {
              if (jsoneq(response, &token[index], "s") == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
                index++;
                Serial.print(F("Sensitivity: "));
                if (*(response + token[index].start) - '0') {
                  rgb_config = ISL_MODE_10KLUX;
                } else {
                  rgb_config = ISL_MODE_375LUX;
                }
                expected_tokens--;
              } else if (jsoneq(response, &token[index], "ir") == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
                index++;
                Serial.print(F("Infrared filter: 0x"));
                rgb_ir_filter = to_uint(response + token[index].start, (size_t) token[index].end - token[index].start);
                Serial.println(rgb_ir_filter, 16);
                expected_tokens--;
              } else if (jsoneq(response, &token[index], "i") == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
                index++;
                Serial.print(F("Interval: "));
                interval = to_uint(response + token[index].start, (size_t) token[index].end - token[index].start);
                Serial.print(interval);
                Serial.println("s");
                expected_tokens--;
              }
            }
          } else {
            Serial.println(F("Signature failed to verify!"));
          }
        }
      }
    }
  }
  free(token);
}

void getRGB(uint8_t &red, uint8_t &green, uint8_t &blue) {
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
  char *lat = NULL, *lon = NULL;
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
            red1, green1, blue1, lat == NULL ? "" : lat, lon == NULL ? "" : lon, bat_percent, loop_counter);

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
  char date[9], time[9], tz[4];
  sim800h.time(date, time, tz);
  Serial.print(date);
  Serial.print(" ");
  Serial.print(time);
  Serial.println(tz);

  sim800h.shutdown();
  delay(100);
}


void setup() {
  Serial.begin(115200);
  Serial.println("WELCOME!");

  pinMode(led, OUTPUT);
  pinMode(trigger, INPUT);

  digitalWrite(led, HIGH);
  delay(500);
  digitalWrite(led, LOW);
  delay(500);

  // edit APN settings in config.h
  sim800h.setAPN(F(FONA_APN), F(FONA_USER), F(FONA_PASS));
}

void loop() {
  digitalWrite(led, HIGH);
  pinMode(trigger, INPUT);

  SendGPS();

  pinMode(trigger, OUTPUT);
  digitalWrite(led, LOW);
  loop_counter++;
  Serial.print(F("sleeping for "));
  Serial.print(interval);
  Serial.println(F("s"));
  delay(100);

  sleep(interval);
}
