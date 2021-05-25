#
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

project(json_schema_validator_wrapper)
find_package(PkgConfig REQUIRED)
set(SOURCES json_schema_validator_wrapper.cpp)

add_library(json_schema_validator_wrapper SHARED ${SOURCES})
set_target_properties(json_schema_validator_wrapper PROPERTIES PUBLIC_HEADER  "json_schema_validator_wrapper.h")
set_target_properties(json_schema_validator_wrapper PROPERTIES VERSION 0 SOVERSION 0 )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
target_link_libraries(json_schema_validator_wrapper nlohmann_json_schema_validator)

install(TARGETS json_schema_validator_wrapper
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/)

# Test Application
add_executable(test_json_schema_validator test_json_schema_validator.c)
target_include_directories(test_json_schema_validator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(test_json_schema_validator
    json_schema_validator_wrapper
    nlohmann_json_schema_validator)

install(TARGETS test_json_schema_validator
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
