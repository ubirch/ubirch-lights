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

# NaCL AVR lib (sha256)
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../../avrnacl-20140813)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../avrnacl-20140813/avrnacl_small/obj)

# add the sketches directory (contains sketch directories and
# a "libraries" dir where dependent libs are extracted
add_sketches(sketches)
