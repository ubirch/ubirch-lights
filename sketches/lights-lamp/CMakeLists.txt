add_executable_avr(lights-lamp lights-lamp.cpp)
add_dependencies(lights-lamp compile-nacl)
target_link_libraries(lights-lamp arduino-core SoftwareSerial nacl)

# special external dependencies can be added like this and will be downloaded once
# arguments: <target> <name> <git url>
target_sketch_library(lights-lamp common "")
target_sketch_library(lights-lamp ubirch-sim800 "git@github.com:ubirch/ubirch-sim800.git")
target_sketch_library(lights-lamp arduino-base64 "https://github.com/adamvr/arduino-base64")
target_sketch_library(lights-lamp jsmn https://github.com/zserge/jsmn)
target_sketch_library(lights-lamp Adafruit_NeoPixel https://github.com/adafruit/Adafruit_NeoPixel)


# copy the config.h.template to config.h in case it is not there; it is ignored by .git!
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.h")
    message(AUTHOR_WARNING "In directory 'sketches/lamp': installing the template config.h, please edit!")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.template ${CMAKE_CURRENT_SOURCE_DIR}/config.h COPYONLY)
endif ()

