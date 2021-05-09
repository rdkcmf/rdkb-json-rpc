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
#ifndef _JSON_HAL_SRV_H
#define _JSON_HAL_SRV_H

#include <stdio.h>
#include <json-c/json.h>
#include "json_hal_common.h"

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

#define BUF_512 512


typedef enum _eNotificationType
{
    NOTIFICATION_TYPE_ON_CHANGE = 0,
#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
    NOTIFICATION_TYPE_ON_CHANGE_SYNC,
    NOTIFICATION_TYPE_ON_CHANGE_SYNC_TIMEOUT,
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT
    NOTIFICATION_TYPE_INVALID
}eNotificationType_t;


/* getSchemaResponse message */
typedef struct _hal_schema_response_t
{
    char filepath[2048];
}hal_schema_response_t;

/* subscribeEvent request message */
typedef struct _hal_subscribe_event_request_t
{
    char name[BUF_512];
    eNotificationType_t type;
}hal_subscribe_event_request_t;

/**
 * @brief Typedefed action callback handler routine.
 * @param (IN) json_object instance pointing to request json message
 * @param (IN) params_count: Contains the number of parameters in the  request.
 * @param (OUT) json_object instance contains the reply json message.
 * @return RETURN_OK in success case else returned RETURN_ERR.
 */
typedef int (*action_callback) (const json_object* request_msg, const int params_count, json_object* reply_msg);

/**
 * @brief  Initialise the rpc server.
 * @param  hal_conf_path  String contains the configuration file path
 * @return RETURN_OK if initialisation is successful else RETURN_ERR.
 *
 */
int json_hal_server_init(const char *hal_conf_path);
/**
 * @brief Register handler callback routines to the HAL server module.
 *
 * Register the callback functions against the action names with HAL server module.
 *
 * @param (IN) Action name indicates function. This action name could be embed in the request message
 * @param (IN) Funcion pointer is the actual callback function. This callback should invoked when HAL server received a request for action_name
 * @return RETURN_OK if initialisation is successful else RETURN_ERR.
 */
int  json_hal_server_register_action_callback(const char *action_name, const action_callback callback);

/**
 * @brief Start the server socket thread.
 * This will start the server socket and listen for client connections and requests.
 * @return RETURN_OK if socket thread started else return RETURN_ERR.
 */
int json_hal_server_run();

/**
 * @brief Publish events to the client.
 * Application can send their event notifications to the subscribed clients.
 * @param (IN) event name
 * @param (IN) event value
 * @return RETURN_OK if success else returned RETURN_ERR.
 */
int json_hal_server_publish_event(const char *event_name, const char *event_value);

/**
 * @brief Clean up function
 * Application needs to call this API once they complete their process.
 * This API will free all the allocated memory and do the clean up
 * process for library.
 * @return RETURN_OK in success case else RETURN_ERR.
 */
int json_hal_server_terminate();

/**
 * @brief Server application can use this API to unpack JSON subscribeEvent request
 * @param (IN) jmsg -  Pointer to JSON input object
 * @param (IN) index - Index of subscribeEvent data in JSON request
 * @param (IN) param - Pointer to hal_subscribe_event_request_t structure
 * @return RETURN_OK in success case else RETURN_ERR.
 */
int json_hal_get_subscribe_event_request(json_object *jmsg, int index, hal_subscribe_event_request_t *param);

/**
 * @brief Server application can use this API to pack a getSchemaResponse message
 * @param (IN) jreply - Pointer to JSON reply object
 * @param (IN) param - Pointer to hal_getparam_response_t message
 * @return RETURN_OK in success case else RETURN_ERR.
 */
int json_hal_add_schema_response(json_object *jreply, hal_schema_response_t *param);

/**
 * @brief Server application can use this API to pack add result status into the json response/
 * @param (IN) jreply - Pointer to JSON reply object
 * @param (IN) param - enum result which indicates SUCCESS or FAILURE.
 * @return RETURN_OK in success case else RETURN_ERR.
 */
int json_hal_add_result_status(json_object *jreply, eResult_t result);
#endif
