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

# Detects current OS

find_program(LSB_RELEASE lsb_release)
if(LSB_RELEASE-NOTFOUND)
    message(FATAL_ERROR "${Red}Unsupported OS! This demo is for ${REQUIRED_OS_ID} ${REQUIRED_OS_VERSION}.${CR}")
endif()
execute_process(COMMAND ${LSB_RELEASE} -is
    OUTPUT_VARIABLE LSB_RELEASE_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT LSB_RELEASE_ID STREQUAL ${REQUIRED_OS_ID})
    message(STATUS "OS found: ${LSB_RELEASE_ID}")
    message(FATAL_ERROR "${Red}Unsupported OS! This demo is for ${REQUIRED_OS_ID} ${REQUIRED_OS_VERSION}.${CR}")
endif()
execute_process(COMMAND ${LSB_RELEASE} -rs
    OUTPUT_VARIABLE LSB_RELEASE_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(LSB_RELEASE_VERSION VERSION_LESS ${REQUIRED_OS_VERSION})
    message(FATAL_ERROR "${Red}Unsupported OS version! This demo is for ${REQUIRED_OS_ID} ${REQUIRED_OS_VERSION}.${CR}")
elseif(LSB_RELEASE_VERSION VERSION_GREATER ${REQUIRED_OS_VERSION})
    message(WARNING "${Red}This demo has not been tested on ${REQUIRED_OS_ID} ${LSB_RELEASE_VERSION}.${CR}")
endif()
