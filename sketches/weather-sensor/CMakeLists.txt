add_executable_avr(weather-sensor weather-sensor.cpp)
add_dependencies(weather-sensor compile-nacl)
target_link_libraries(weather-sensor arduino-core SoftwareSerial nacl)

# special external dependencies can be added like this and will be downloaded once
# arguments: <target> <name> <git url>
target_sketch_library(weather-sensor i2c "")
target_sketch_library(weather-sensor common "")
target_sketch_library(weather-sensor adafruit-sensor "https://github.com/adafruit/Adafruit_Sensor")
target_sketch_library(weather-sensor adafruit-bme280 "https://github.com/adafruit/Adafruit_BME280_Library")
target_sketch_library(weather-sensor adafruit-bmp085 "https://github.com/adafruit/Adafruit-BMP085-Library")
target_sketch_library(weather-sensor sparkfun-oled "https://github.com/sparkfun/SparkFun_Micro_OLED_Arduino_Library")
target_sketch_library(weather-sensor ubirch-sim800 "git@github.com:ubirch/ubirch-sim800.git")
target_sketch_library(weather-sensor arduino-base64 "https://github.com/adamvr/arduino-base64")
target_sketch_library(weather-sensor jsmn https://github.com/zserge/jsmn)

# copy the config.h.template to config.h in case it is not there; it is ignored by .git!
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.h")
    message(AUTHOR_WARNING "In directory 'sketches/weather-sensor': installing the template config.h, please edit!")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.template ${CMAKE_CURRENT_SOURCE_DIR}/config.h COPYONLY)
endif()

