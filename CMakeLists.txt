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

# JSON schema validator wrapper
add_subdirectory(json-schema-validator-wrapper)

# JSON HAL Server Library
project(json_hal_server)
find_package(PkgConfig REQUIRED)
set(SOURCES json_hal_server.c json_hal_common.c tcp_server.c)
add_library(json_hal_server SHARED ${SOURCES})
set_target_properties(json_hal_server PROPERTIES PUBLIC_HEADER  "json_hal_server.h;json_hal_common.h")
set_target_properties(json_hal_server PROPERTIES VERSION 0 SOVERSION 0 )
target_include_directories(json_hal_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/json-schema-validator-wrapper)
target_compile_options(json_hal_server PRIVATE -Wall -Werror -Wno-error=discarded-qualifiers)

# JSON_BLOCKING_SUBSCRIBE_EVENT
if(JSON_BLOCKING_SUBSCRIBE_EVENT)
target_compile_definitions(json_hal_server PUBLIC -DJSON_BLOCKING_SUBSCRIBE_EVENT)
endif(JSON_BLOCKING_SUBSCRIBE_EVENT)
unset(JSON_BLOCKING_SUBSCRIBE_EVENT)

# Schema validation.
if(JSON_SCHEMA_VALIDATION_ENABLED)
target_compile_definitions(json_hal_server PUBLIC -DJSON_SCHEMA_VALIDATION_ENABLED)
endif(JSON_SCHEMA_VALIDATION_ENABLED)
unset(JSON_SCHEMA_VALIDATION_ENABLED)

target_include_directories(json_hal_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/json-rpc-server
    ${CMAKE_CURRENT_SOURCE_DIR}/json-rpc-common/)
target_link_libraries(json_hal_server json-c json_schema_validator_wrapper pthread)

install(TARGETS json_hal_server
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})


# JSON HAL Client Library
project(json_hal_client)
find_package(PkgConfig REQUIRED)
set(SOURCES json_hal_client.c json_hal_common.c tcp_client.c)
add_library(json_hal_client SHARED ${SOURCES})
target_compile_options(json_hal_client PRIVATE -Wall -Werror -Wno-error=discarded-qualifiers)
set_target_properties(json_hal_client PROPERTIES PUBLIC_HEADER  "json_hal_client.h")
set_target_properties(json_hal_client PROPERTIES VERSION 0 SOVERSION 0 )

# JSON_BLOCKING_SUBSCRIBE_EVENT
if(JSON_BLOCKING_SUBSCRIBE_EVENT)
target_compile_definitions(json_hal_client PUBLIC -DJSON_BLOCKING_SUBSCRIBE_EVENT)
endif(JSON_BLOCKING_SUBSCRIBE_EVENT)
unset(JSON_BLOCKING_SUBSCRIBE_EVENT)

target_include_directories(json_hal_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/json-rpc-common)
target_link_libraries(json_hal_client json-c pthread)

install(TARGETS json_hal_client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Sample Test Applications.
add_subdirectory(samples)
