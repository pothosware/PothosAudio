########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.9)
project(PothosAudio CXX)

if(${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
    find_package(Pothos "0.6.0" CONFIG REQUIRED)
else()
    find_package(Pothos CONFIG REQUIRED) #in-tree build
endif()

########################################################################
# Find PortAudio
########################################################################
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})
find_package(Portaudio)

if (PORTAUDIO_FOUND)
    message(STATUS "PORTAUDIO_INCLUDE_DIRS: ${PORTAUDIO_INCLUDE_DIRS}")
    message(STATUS "PORTAUDIO_LIBRARIES: ${PORTAUDIO_LIBRARIES}")
    message(STATUS "PORTAUDIO_DEFINITIONS: ${PORTAUDIO_DEFINITIONS}")
else()
    message(WARNING "Pothos Audio blocks require PortAudio, skipping...")
endif()

########################################################################
# yaml-cpp library
########################################################################
if(NOT YAMLCPP_IN_TREE)
    find_package(YamlCpp)
endif(NOT YAMLCPP_IN_TREE)

if (YAMLCPP_FOUND)
    message(STATUS "YAMLCPP_VERSION: ${YAMLCPP_VERSION}")
    message(STATUS "YAMLCPP_INCLUDE_DIRS: ${YAMLCPP_INCLUDE_DIRS}")
    message(STATUS "YAMLCPP_LIBRARIES: ${YAMLCPP_LIBRARIES}")
else (YAMLCPP_FOUND)
    message(WARNING "Pothos Audio blocks require yaml-cpp, skipping...")
endif (YAMLCPP_FOUND)

########################################################################
## Feature registration
########################################################################
include(FeatureSummary)
include(CMakeDependentOption)
cmake_dependent_option(ENABLE_AUDIO "Enable Pothos Audio component" ON "Pothos_FOUND;PORTAUDIO_FOUND" OFF)
add_feature_info(Audio ENABLE_AUDIO "Streaming audio source and sink blocks")
if (NOT ENABLE_AUDIO)
    return()
endif()

########################################################################
## Build and install module
########################################################################
include_directories(${PORTAUDIO_INCLUDE_DIRS})
include_directories(${YAMLCPP_INCLUDE_DIRS})
add_definitions(${PORTAUDIO_DEFINITIONS})

#the number of frames to block on an IO call
#when no non-blocking frames are available.
add_definitions(-DMIN_FRAMES_BLOCKING=1024)

POTHOS_MODULE_UTIL(
    TARGET AudioSupport
    SOURCES
        AudioBlock.cpp
        AudioSource.cpp
        AudioSink.cpp
        AudioInfo.cpp
    LIBRARIES
        ${PORTAUDIO_LIBRARIES}
        ${YAMLCPP_LIBRARIES}
    DESTINATION audio
    ENABLE_DOCS
)
