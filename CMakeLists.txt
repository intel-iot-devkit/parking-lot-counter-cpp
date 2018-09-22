cmake_minimum_required(VERSION 2.8.11)
project(FAC)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# Fancy colorized output messages
string(ASCII 27 Esc)
set(CR "${Esc}[m")
set(Red "${Esc}[1;31m")
set(Green "${Esc}[1;32m")
set(Blue "${Esc}[1;34m")

# Allow users to manually set OpenCV path if desired
set(OpenCV_DIR CACHE PATH "Path to Intel OpenCV library")

message(STATUS "${Blue}Intel Shopper Gaze Monitor${CR}")

# Check OS
message(STATUS "${Blue}Checking OS version...${CR}")

set(REQUIRED_OS_ID "Ubuntu")
set(REQUIRED_OS_VERSION "16.04")
include(osdetect)

# Check for Intel OpenVINO
message(STATUS "${Blue}Checking prerequisites...${CR}")

find_package(DemoOpenVINO)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "${Red}Intel OpenVINO was not found!\n${Blue}Have you sourced the ${Green}setupvars.sh${Blue} script provided by the Intel OpenVINO?${CR}")
endif( )

include_directories(${OpenCV_INCLUDE_DIRS})

# Install paho MQTT dependency
include(pahomqtt)

# Application executables
set(MONITOR monitor)
set(DSOURCES src/main.cpp src/mqtt.cpp)
add_executable(${MONITOR} ${DSOURCES})
add_dependencies(${MONITOR} pahomqtt)
set_target_properties(${MONITOR} ${TRAINER} PROPERTIES COMPILE_FLAGS "-pthread -std=c++11")
target_link_libraries (${MONITOR} ${OpenCV_LIBS} pthread paho-mqtt3cs)

# Install
install(TARGETS ${MONITOR} DESTINATION bin)
