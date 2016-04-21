add_executable_avr(lights-sensor lights-sensor.cpp)
target_link_libraries(lights-sensor arduino-core SoftwareSerial nacl)

# special external dependencies can be added like this and will be downloaded once
# arguments: <target> <name> <git url>
target_sketch_library(lights-sensor i2c "")
target_sketch_library(lights-sensor isl29125 "")
target_sketch_library(lights-sensor ubirch-sim800 "git@github.com:ubirch/ubirch-sim800.git")
target_sketch_library(lights-sensor arduino-base64 "https://github.com/adamvr/arduino-base64")

# copy the config.h.template to config.h in case it is not there; it is ignored by .git!
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.h")
    message(AUTHOR_WARNING "In directory 'sketches/fewl': installing the template config.h, please edit!")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.template ${CMAKE_CURRENT_SOURCE_DIR}/config.h COPYONLY)
endif()

