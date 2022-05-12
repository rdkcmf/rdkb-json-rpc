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

#ifndef _JSON_HAL_CLIENT_H
#define _JSON_HAL_CLIENT_H

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

/**
 * @brief Typedefed event callback handler routine.
 * @param (IN) Buffer pointing to the event message
 * @param (IN) Length of the event message
 */
typedef void (*event_callback) (const char* event_msg, const int event_msg_length);

/**
 * @brief Initialise the hal client module.
 * @param (IN) String contains the configuration file path
 * @return RETURN_OK if successful else RETURN_ERR.
 */
int json_hal_client_init(const char *hal_conf_path);

/**
 * @brief Start the client socket thread.
 * @return RETURN_OK if socket thread started else return RETURN_ERR.
 */
int json_hal_client_run ();

/**
 * @brief Send the request message to the server socket, sync the response
 * from server and return back the filled data to caller.
 *
 * This API will send the requested message to the server and wait for the
 * response from the server. This API is blocked  until we get a proper
 * response from the server or timed out happened.
 *
 * @param (IN)  Json object pointing to the request
 * @param (OUT) Json object stores the response message
 * @return RETURN_OK if message has been send to server and get response from server
 * @note This is a blocking call, and will unblock if client get response from server or
 * timeout happened because no data received from server.
 */
int json_hal_client_send_and_get_reply(const json_object *request, json_object** reply);

/**
 * @brief Send the request message to the server socket, sync the response
 * from server and return back the filled data to caller.
 *
 * This API will send the requested message to the server and wait for the
 * response from the server. This API is blocked  until we get a proper
 * response from the server or timed out happened.
 *
 * @param (IN)  Json object pointing to the request
 * @param (IN)  the timeout period in second.
 * @param (OUT) Json object stores the response message
 * @return RETURN_OK if message has been send to server and get response from server
 * @note This is a blocking call, and will unblock if client get response from server or
 * timeout happened because no data received from server.
 */
int json_hal_client_send_and_get_reply_with_timeout(const json_object *jrequest_msg, int timeout, json_object **reply_msg);
/**
 * @brief Create and return the header json message to the caller.
 * Header contains module, version, action and seqid.
 * @param action_name
 * @return json object pointing to the request message heeader.
 */
json_object *json_hal_client_get_request_header(const char *action_name);

/**
 * @brief Register the callback function to notify for the events
 *
 * This API register the callback function with HAL client library. HAL
 * client library will store the requested callback function to its global
 * callback function pointer and invoke this whenever it gets event from server
 * side.
 * @param (IN) Callback method. This method will be invoke when client receive events from server.
 * @param (IN) Full DML based path of the event
 * @param (IN) event_notification_type contains the notification type for the event
 * @return RETURN_OK if client connection established else RETURN_ERR.
 */
int json_hal_client_subscribe_event(event_callback callback, const char* event_name, const char* event_notification_type);

/**
 * @brief Clean up function
 *
 * This API will free all the allocated memory and do the clean up
 * process for library.
 * @return RETURN_OK in success case else RETURN_ERR.
 */
int json_hal_client_terminate();

/**
 * @brief Check the client is successfully connected to the server
 *
 * Utility function to check whether client is successfully registered
 * with the server application.
 * @return TRUE if connected else return FALSE
 */
int json_hal_is_client_connected();

/**
 * @brief Application can use this API to pack
          parse and check the status of Result message.
 * @param (IN) json_msg  - Json message
 * @param (OUT) status - Pointer to json_bool, store TRUE if Success status received
 *        else FALSE stored
 * @return RETURN_OK in success case else RETURN_ERR.
 */
int json_hal_get_result_status(const json_object *json_msg, json_bool *status);

/**
 * @brief Application can use this API to get
 *        total number of params embedded in json request/response
 *
 * @param (IN) json_msg - String to hold json message
 *
 * @return Number of parameters in the json message.
 */
int json_hal_get_total_param_count(const json_object *json_msg);

#endif //_JSON_HAL_CLIENT_H
