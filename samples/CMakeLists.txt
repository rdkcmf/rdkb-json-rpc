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

project(json-rpc-test)
find_package(PkgConfig REQUIRED)

#TEST - HAL Server test application
add_executable(test_json_hal_srv  json_hal_server_test.c)

if(JSON_SCHEMA_VALIDATION_ENABLED)
target_compile_definitions(json_hal_server PUBLIC -DJSON_SCHEMA_VALIDATION_ENABLED)
endif(JSON_SCHEMA_VALIDATION_ENABLED)
unset(JSON_SCHEMA_VALIDATION_ENABLED)
target_include_directories(test_json_hal_srv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_CURRENT_SOURCE_DIR}/../json-rpc-common)

target_link_libraries(test_json_hal_srv
    json_hal_server
    pthread)

install(TARGETS test_json_hal_srv
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

#TEST - HAL client test application
add_executable(test_json_hal_cli  json_hal_client_test.c)
target_include_directories(test_json_hal_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_CURRENT_SOURCE_DIR}/../json-rpc-common)

target_link_libraries(test_json_hal_cli
    json_hal_client
    pthread)

install(TARGETS test_json_hal_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

#TEST - Event simulation client application
add_executable(test_json_hal_event  json_hal_client_event_test.c)
target_include_directories(test_json_hal_event PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../ ${CMAKE_CURRENT_SOURCE_DIR}/../json-rpc-common)

target_link_libraries(test_json_hal_event
    json_hal_client
    pthread)

install(TARGETS test_json_hal_event
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

