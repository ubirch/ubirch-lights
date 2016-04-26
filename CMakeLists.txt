#=====================================================================================
# ubirch #1 cmake
#=====================================================================================
cmake_minimum_required(VERSION 3.0)

#this is the place where you can set configuration options
include(config.cmake)

# the toolchain file (see https://github.com/ubirch/avr-arduino-cmake)
set(CMAKE_TOOLCHAIN_FILE cmake/avr-arduino-toolchain.cmake)

# project name and code types we use
project(ubirch C CXX ASM)

# NaCL AVR lib (sha256), special handling for an external makefile
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sketches/libraries/avrnacl-20140813/config.in" NACLCONFIG)
string(REGEX REPLACE "TARGET_DEVICE=[^\n]*" "TARGET_DEVICE=${MCU}" NACLCONFIG ${NACLCONFIG})
string(REGEX REPLACE "CPUFREQ=[^\n]*" "CPUFREQ=${F_CPU}" NACLCONFIG ${NACLCONFIG})
string(REGEX REPLACE "CC=[^\n]*" "CC=${AVRC}" NACLCONFIG ${NACLCONFIG})
string(REGEX REPLACE "OBJCOPY=[^\n]*" "OBJCOPY=${OBJCOPY}" NACLCONFIG ${NACLCONFIG})
string(REGEX REPLACE "AR=[^\n]*" "AR=${AVRAR}" NACLCONFIG ${NACLCONFIG})
string(REGEX REPLACE "STRIP=[^\n]*" "STRIP=${AVRSTRIP}" NACLCONFIG ${NACLCONFIG})
string(REGEX REPLACE "DEVICE_FILE=[^\n]*" "DEVICE_FILE=${SERIAL_DEV}" NACLCONFIG ${NACLCONFIG})
message(STATUS ${NACLCONFIG})
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/sketches/libraries/avrnacl-20140813/config" "${NACLCONFIG}")
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/sketches/libraries/avrnacl-20140813/avrnacl_small/obj/libnacl.a
  COMMAND make
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sketches/libraries/avrnacl-20140813
)
add_custom_target(compile-nacl
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sketches/libraries/avrnacl-20140813/avrnacl_small/obj/libnacl.a
)
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/sketches//libraries/avrnacl-20140813)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/sketches/libraries/avrnacl-20140813/avrnacl_small/obj)

# add the sketches directory (contains sketch directories and
# a "libraries" dir where dependent libs are extracted
add_sketches(sketches)
