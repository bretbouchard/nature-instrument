cmake_minimum_required(VERSION 3.15)

project(NatureDSP)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add nature DSP source
set(NATURE_MARCO_DSP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(NATURE_MARCO_DSP_SRC "${NATURE_MARCO_DSP_ROOT}/plugins/dsp/src/dsp")
set(NATURE_MARCO_DSP_INCLUDE "${NATURE_MARCO_DSP_ROOT}/plugins/dsp/include")

# Create static library
add_library(NatureDSP STATIC

    # Wrapper files
    NatureDSP.cpp
    NatureDSP.h

    # Nature Marco DSP implementation
    ${NATURE_MARCO_DSP_SRC}/NaturePureDSP.cpp
)

# Include paths
target_include_directories(NatureDSP PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NATURE_MARCO_DSP_INCLUDE}
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../include/dsp"
)

# Compiler flags for iOS
target_compile_options(NatureDSP PUBLIC
    -fobjc-arc
    -fembed-bitcode
)

# Architecture settings for iOS
if(IOS_PLATFORM)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "15.0")
endif()
