cmake_minimum_required(VERSION 3.26)
project(Nature VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

file(GLOB_RECURSE SOURCES 
    "${SOURCE_DIR}/*.cpp"
    "${SOURCE_DIR}/*.h"
    "${INCLUDE_DIR}/*.h"
)

# Create library for standalone/DSP
add_library(nature_dsp STATIC ${SOURCES})

target_include_directories(nature_dsp PUBLIC
    ${INCLUDE_DIR}
    ${SOURCE_DIR}
)

# AUv3 Plugin (if building for macOS)
if(APPLE)
    # AUv3 plugin configuration here
    message(STATUS "Configuring AUv3 plugin for Nature")
endif()
