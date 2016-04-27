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
#include <i2c.h>
#include <isl29125.h>
#include <avrsleep.h>
#include <freeram.h>
#include <alloca.h>

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

// protocol version check
#define PROTOCOL_VERSION_MIN "0.0"
// json keys
#define P_SIGNATURE "s"
#define P_VERSION "v"
#define P_PAYLOAD "p"
#define P_SENSITIVITY "s"
#define P_IR_FILTER "ir"
#define P_INTERVAL "i"

// error flags
#define E_SENSOR_FAILED 0b00000001
#define E_PROTOCOL_FAIL 0b00000010
#define E_SIG_VRFY_FAIL 0b00000100
#define E_NO_MEMORY     0b10000000
#define E_NO_CONNECTION 0b01000000

UbirchSIM800 sim800h = UbirchSIM800();

// this counts up as long as we don't have a reset
static uint16_t loop_counter = 1;
static uint8_t error_flag = 0x00;

// internal sensor state
static uint16_t interval = DEFAULT_INTERVAL;
static uint8_t sensitivity = ISL_MODE_375LUX;
static uint8_t infrared_filter = ISL_FILTER_IR_MAX;

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
static unsigned int to_uint(const char *ptr, size_t len) {
  unsigned int ret = 0;
  for (uint8_t i = 0; i < len; i++) {
    ret = (ret * 10) + (ptr[i] - '0');
  }
  return ret;
}

// print a hex representation of the has byte array
static inline void print_hash(char *sig) {
  Serial.print(F("[HASH] "));
  for (uint8_t i = 0; i < crypto_hash_BYTES; i++) Serial.print((unsigned char) sig[i], 16);
  Serial.println();
}

/*!
 * Process the JSON response from the backend. It should contain configuration
 * parameters that need to be set. The response must be signed and will be
 * checked for signature match and protocol version.
 *
 * @param response the request response, will be freed here
 * @param payload the extracted payload, if signature is verified
 */
void process_response(char *response, char *&payload, char *&signature) {
  char tmp_signature[crypto_hash_BYTES], *tmp_payload = NULL;

  jsmntok_t *token;
  jsmn_parser parser;
  jsmn_init(&parser);

  // identify the number of tokens in our response, we expect 13
  const uint8_t token_count = (const uint8_t) jsmn_parse(&parser, response, strlen(response), NULL, 0);
  token = (jsmntok_t *) malloc(sizeof(*token) * token_count);

  // TODO check token count and return if too many

  // reset parser, parse and store tokens
  jsmn_init(&parser);
  const int parsed_token_count = jsmn_parse(&parser, response, strlen(response), token, token_count);
  if (parsed_token_count == token_count) {
    uint8_t index = 0;
    if (token[0].type == JSMN_OBJECT) {
      while (++index < token_count) {
        if (jsoneq(response, token[index], P_VERSION) == 0 && token[index + 1].type == JSMN_STRING) {
          index++;
          if (strncmp_P(response + token[index].start, PSTR(PROTOCOL_VERSION_MIN), 3) != 0) {
            Serial.print(F("protocol version mismatch: "));
            print_token(response, token[index]);

            // do not continue if the version does not match, free already copied payload or sig
            error_flag |= E_PROTOCOL_FAIL;
            break;
          }
        } else if (jsoneq(response, token[index], P_SIGNATURE) == 0 && token[index + 1].type == JSMN_STRING) {
          index++;
          Serial.print(F("signature: "));
          print_token(response, token[index]);

          // extract signature and decode it
          base64_decode(tmp_signature, (response + token[index].start), token[index].end - token[index].start);
          print_hash(tmp_signature);

        } else if (jsoneq(response, token[index], P_PAYLOAD) == 0 && token[index + 1].type == JSMN_OBJECT) {
          index++;
          Serial.print(F("payload: "));
          print_token(response, token[index]);

          // extract payload from json
          uint8_t tmp_payload_length = (uint8_t) (token[index].end - token[index].start);
          // allocate the temporary payload on the stack, we will copy it to heap after freeing some space
          tmp_payload = (char *) alloca((size_t) (16 + tmp_payload_length));
          sim800h.IMEI(tmp_payload);
          strncpy(tmp_payload + 15, response + token[index].start, tmp_payload_length);
          tmp_payload[15 + tmp_payload_length] = '\0';

          index += 2 * token[index].size;
        } else {
          // simply ignore unknown keys
          Serial.print(F("unknown key: "));
          print_token(response, token[index]);
          index++;
        }
      }
    }
  }

  // free used heap, the token and the no longer used response
  free(token);
  free(response);

  // copy the locally (stack) allocated payload and signature to heap
  payload = strdup(tmp_payload);
  signature = strdup(tmp_signature);
}

/*!
 * Verify payload using the given signature.
 *
 * @param payload the json payload to check (including prepended key!)
 * @param signature the signature for verification
 * @return true if the verification was successful
 */
bool verify_payload(const char *payload, const char *signature) {
  // don't even start if we get NULLs
  if (payload == NULL || signature == NULL) return false;

  // first verify the payload signature
  // required ram for signature verification: (PAYLOAD + IMEI) + SHA512 + HASH SIZE
  const int required_ram = (strlen(payload) + 1) + 669 + crypto_hash_BYTES;
  const int free_sram = query_free_sram();

  Serial.print(F("payload verification: "));
  Serial.print(free_sram);
  Serial.print(F(" byte free ("));
  Serial.print(required_ram);
  Serial.println(F(" byte required)"));

  bool signature_verified = false;
  if (required_ram < free_sram) {

    // hash payload and check whether it matches the signature hash
    char *payload_hash = (char *) malloc(crypto_hash_BYTES);

    crypto_hash_sha512((unsigned char *) payload_hash, (const unsigned char *) payload, strlen(payload));
    print_hash(payload_hash);

    signature_verified = !memcmp(signature, payload_hash, crypto_hash_BYTES);
    if(!signature_verified) error_flag |= E_SIG_VRFY_FAIL;

    // free the payload hash
    free(payload_hash);
  } else {
    Serial.println(F("payload too large, may crash..."));
    error_flag |= E_NO_MEMORY;
  }

  return signature_verified;
}

/*!
 * Process payload and set configuration parameters from it.
 * @param payload the payload to use, should be checked
 */
void process_payload(char *payload) {
  jsmntok_t *token;
  jsmn_parser parser;
  jsmn_init(&parser);

  // identify the number of tokens in our response, we expect 13
  const uint8_t token_count = (const uint8_t) jsmn_parse(&parser, payload, strlen(payload), NULL, 0);
  token = (jsmntok_t *) malloc(sizeof(*token) * token_count);

  // reset parser, parse and store tokens
  jsmn_init(&parser);
  if (jsmn_parse(&parser, payload, strlen(payload), token, token_count) == token_count) {
    uint8_t index = 0;
    if (token[0].type == JSMN_OBJECT) {
      while (++index < token_count) {
        if (jsoneq(payload, token[index], P_SENSITIVITY) == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
          index++;
          Serial.print(F("sensitivity: "));
          if (*(payload + token[index].start) - '0') {
            Serial.println(F("10K lux"));
            sensitivity = ISL_MODE_10KLUX;
          } else {
            Serial.println(F("375 lux"));
            sensitivity = ISL_MODE_375LUX;
          }
        } else if (jsoneq(payload, token[index], P_IR_FILTER) == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
          index++;
          Serial.print(F("infrared filter: 0x"));
          infrared_filter = to_uint(payload + token[index].start,
                                    (size_t) token[index].end - token[index].start);
          Serial.println(infrared_filter, 16);
        } else if (jsoneq(payload, token[index], P_INTERVAL) == 0 && token[index + 1].type == JSMN_PRIMITIVE) {
          index++;
          Serial.print(F("Interval: "));
          interval = to_uint(payload + token[index].start, (size_t) token[index].end - token[index].start);
          Serial.print(interval);
          Serial.println(F("s"));
        } else {
          Serial.print(F("unknown payload key: "));
          print_token(payload, token[index]);
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
bool sample_rgb(uint16_t &red, uint16_t &green, uint16_t &blue) {
  i2c_init(I2C_SPEED_400KHZ);

  if (!isl_reset()) {
    Serial.println(F("ISL29125 reset failed"));
    error_flag |= E_SENSOR_FAILED;
    return false;
  }
  if (!isl_set(ISL_R_COLOR_MODE, sensitivity | ISL_MODE_16BIT | ISL_MODE_RGB)) {
    Serial.println(F("ISL29125 ir config failed"));
    error_flag |= E_SENSOR_FAILED;
    return false;
  }
  if (!isl_set(ISL_R_FILTERING, infrared_filter)) {
    Serial.println(F("ISL29125 set infrared filtering failed"));
    error_flag |= E_SENSOR_FAILED;
    return false;
  }


  // wait for the conversion cycle to be done, this just indicates there is a cycle
  // in progress. the actual r,g,b values are always available from the last cycle
  for (uint8_t colors = 0; colors < 3; colors++) {
    uint8_t timeout = 150;
    while (!(isl_get(ISL_R_STATUS) & ISL_STATUS_ADC_DONE) && --timeout) delay(1);
  }

  red = isl_read_red();
  green = isl_read_green();
  blue = isl_read_blue();

  return true;
}

/*!
 * Send samples sensor data to the backend. The payload message will be signed
 * using a board specific key.
 */
void send_sensor_data() {
  uint16_t red = 0, green = 0, blue = 0;
  uint16_t bat_status = 0, bat_percent = 0, bat_voltage = 0;
  char *lat = NULL, *lon = NULL, *date = NULL, *time = NULL;
  char *payload, *payload_hash, *auth_hash;
  char *signature, *message;

  // do an initial sampling
  sample_rgb(red, green, blue);

  // auto-compensate for brightness
  if (sensitivity == ISL_MODE_375LUX && red > 65000 && green > 65000 && blue > 65000) {
    sensitivity = ISL_MODE_10KLUX;
    sample_rgb(red, green, blue);
  } else if (sensitivity == ISL_MODE_10KLUX && red < 200 && green < 200 && blue < 200) {
    sensitivity = ISL_MODE_375LUX;
    sample_rgb(red, green, blue);
  }

  Serial.print(F("RGB: "));
  if (sensitivity == ISL_MODE_375LUX) Serial.print(F("375LUX:"));
  else Serial.print(F("10kLUX:"));
  Serial.print(red);
  Serial.print(F(":"));
  Serial.print(green);
  Serial.print(F(":"));
  Serial.println(blue);

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
  // Example: '123456789012345{"r":44,"g":33,"b":22,"s":0,"lat":"12.475886","lon":"51.505264","bat":100,"lps":99999}'
  sprintf_P(payload + 15,
            PSTR("{\"r\":%u,\"g\":%u,\"b\":%u,\"s\":%1u,\"la\":\"%s\",\"lo\":\"%s\",\"ba\":%3u,\"lp\":%u,\"e\":%u}"),
            red, green, blue, sensitivity == ISL_MODE_375LUX ? 0 : 1,
            lat == NULL ? "" : lat, lon == NULL ? "" : lon,
            bat_percent, loop_counter, error_flag);
  error_flag = 0;

  // free latitude and longitude
  free(lat);
  free(lon);
  free(date);
  free(time);

  // create hashes from the payload structure as well as the IMEI (key), sig buffer is used twice!
  signature = (char *) malloc(crypto_hash_BYTES);

  payload_hash = (char *) malloc(89);
  crypto_hash((unsigned char *) signature, (const unsigned char *) payload, strlen(payload));
  base64_encode(payload_hash, signature, crypto_hash_BYTES);

  auth_hash = (char *) malloc(89);
  Serial.print(F("payload hash: "));
  Serial.println(payload_hash);

  crypto_hash((unsigned char *) signature, (const unsigned char *) payload, 15);
  base64_encode(auth_hash, signature, crypto_hash_BYTES);

  // free signature buffer
  free(signature);
  signature = NULL;

  Serial.print(F("auth hash   : "));
  Serial.println(auth_hash);

  // finally, compile the message
  message = (char *) malloc(300);
  sprintf_P(message,
            PSTR("{\"v\":\"0.0.1\",\"a\":\"%s\",\"s\":\"%s\",\"p\":%s}"),
            auth_hash, payload_hash, payload + 15);

  // free payload and hashes
  free(payload);
  payload = NULL;
  free(payload_hash);
  free(auth_hash);

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

      // process response, extract payload and signature
      process_response(response, payload, signature);
      // verify and process payload
      if (verify_payload(payload, signature)) {
        Serial.println(F("signature verified OK"));
        process_payload(payload + 15);
      } else {
        Serial.println(F("signature failed to verify"));
      }
      free(signature);
      free(payload);

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
        Serial.print(query_free_sram());
        Serial.println(F(" byte free"));

        send_sensor_data();

        Serial.print(query_free_sram());
        Serial.println(F(" byte free"));

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
