# Copyright (c) 2018 Intel Corporation.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# Use system variables set by CV SDK
if(NOT DEFINED OpenCV_DIR AND DEFINED ENV{OpenCV_DIR})
    set(OpenCV_DIR "$ENV{OpenCV_DIR}")
endif()

if(NOT DEFINED OpenCV_DIR)
    if(EXISTS "${INTEL_CVSDK_DIR}/opencv/share/OpenCV" )
        set(OpenCV_DIR "${INTEL_CVSDK_DIR}/opencv/share/OpenCV")
    elseif(EXISTS "$ENV{INTEL_CVSDK_DIR}/opencv/share/OpenCV")
        set(OpenCV_DIR "$ENV{INTEL_CVSDK_DIR}/opencv/share/OpenCV")
    elseif(EXISTS "/opt/intel/computer_vision_sdk/opencv/share/OpenCV")
        set(OpenCV_DIR "/opt/intel/computer_vision_sdk/opencv/share/OpenCV")
    endif()
endif()

# Avoid system installed OpenCV
find_package(OpenCV PATHS ${OpenCV_DIR} QUIET NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)

if(OpenCV_FOUND)
    message(STATUS "Intel OpenVINO was found")
    message(STATUS "OpenCV_INCLUDE_DIRS=${OpenCV_INCLUDE_DIRS}")
    message(STATUS "OpenCV_LIBS=${OpenCV_LIBS}")
endif()
