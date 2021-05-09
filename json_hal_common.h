/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#ifndef _JSON_HAL_COMMON_H
#define _JSON_HAL_COMMON_H

#include <stdio.h>
#include <json-c/json.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef RETURN_OK
#define RETURN_OK 0
#endif

#ifndef RETURN_ERR
#define RETURN_ERR -1
#endif

#define HAL_SCHEMA_PATH "hal_schema_path"
#define SERVER_PORT "server_port"

/**
 * @brief This structure is used to hold the client/server configuration
 * data. This contains the HAL module name, version and server port number.
 */
typedef struct _hal_config_t
{
    char hal_module_name[64];    /* HAL module name. */
    char hal_module_version[64]; /* HAL module version. */
    char hal_schema_path[256];   /* HAL JSON schema Path. */
    int server_port_number;      /* Server Port Number. */
    int request_timeout_period; /* Timeout period for request. */
} hal_config_t;

typedef enum _ParamType
{
    PARAM_BOOLEAN = 1,
    PARAM_STRING,
    PARAM_INTEGER,
    PARAM_UNSIGNED_INTEGER,
    PARAM_LONG,
    PARAM_UNSIGNED_LONG,
    PARAM_HEXBINARY,
    PARAM_BASE64
}eParamType;

typedef enum _eResult_t
{
    RESULT_SUCCESS = 0,
    RESULT_FAILURE
}eResult_t;

typedef enum _eActionType
{
    GET_REQUEST_MESSAGE,
    GET_RESPONSE_MESSAGE,
    SET_REQUEST_MESSAGE,
    DELETE_REQUEST_MESSAGE,
    PUBLISHEVENT_RESPONSE_MESSAGE
}eActionType;

typedef struct _hal_param_t
{
    char name[256];
    char value[2048];
    eParamType type;
}hal_param_t;


/**
 * @brief Application can use this API to unpack get/set/configure/delete JSON request
 * @param (IN) jmsg -  Pointer to JSON input object
 * @param (IN) index - Index of getParameter data in JSON request
 * @action (IN) action - Action type
 * @param (IN) param - Pointer to hal_param_t structure
 * @return RETURN_OK in success case else RETURN_ERR.
 */
int json_hal_get_param(json_object *jmsg, int index, eActionType action, hal_param_t *param);

/**
 * @brief Application can use this API to pack
          JSON response message for get/set/configure/delete requests
 * @param (IN) jreply - Pointer to JSON reply object
 * @param (IN) param - Pointer to hal_getparam_response_t message
 * @return RETURN_OK in success case else RETURN_ERR.
 */
int json_hal_add_param(json_object *jreply, eActionType action, hal_param_t *param);

/**
 * @brief Application can use this API to
 *        retrieve configuration from configuration file.
 * @param config_file - String contains the configuration file
 * @param config - Structure pointer to hold configuration data
 * @return RETURN_OK in success case else RETURN_ERR.
 */
int json_hal_load_config(const char *config_file, hal_config_t *config);
#endif
