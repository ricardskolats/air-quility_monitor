# SPDX-License-Identifier: Apache-2.0
# Air Quality Monitor - CMake Build File

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(air_quality_monitor LANGUAGES C CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
target_sources(app PRIVATE
    src/main.cpp
    src/Sensor.cpp
    src/Buffer.cpp
    src/Network.cpp
    src/Mqtt.cpp
    src/Uploader.cpp
)

# Include path
target_include_directories(app PRIVATE src)
