# SPDX-License-Identifier: Apache-2.0
# Air Quality Monitor (Thread version)

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(air_quality_thread LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

target_sources(app PRIVATE
    src/main.cpp
    src/Sensor.cpp
    src/Buffer.cpp
    src/Network.cpp
    src/Mqtt.cpp
)

target_include_directories(app PRIVATE src)
