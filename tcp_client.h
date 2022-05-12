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

#ifndef _JSON_RPC_CLIENT_PROCESS_H
#define _JSON_RPC_CLIENT_PROCESS_H
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <syslog.h>
#include <pthread.h>
#include "json_rpc_common.h"

#define SERVER_HOST "127.0.0.1"
#define INVALID_SOCKFD -1
#define LOOP_TIMEOUT 250000 // timeout in microseconds.

/**
 * @brief Structure to hold the client socket connection.
 */
typedef struct rpc_client_data_t {
    int sock; /* Client socket fd */
    int state; /* Current state of client socket state machine. */
    int port; /* Server Port number to connect. */
    char host[BUF_32]; /* Host name. */
    int RUNNING; /* Flag indicates state machine is running or not. */
    char buffer[MAX_BUFFER_SIZE]; /* Buffer contains the message. */
    int (*func_connected)(int); /* Callback invoked when connection established. */
    int (*func_disconnected)(int); /* Callback invoked when connection disconnected. */
    int (*func_parse)(const int, const char*, const int); /* Callback invoked when client got response from server. */
    int (*func_idle)(void); /* Callback invoked whenever client is not receiving reponse after send the request. */
}rpc_client_data_t;

/**
 * @brief Start the socket client thread and connected to server socket.
 * @param (IN) Received filled structure which defines the callback [To be invoked when connect/disconnect/Idle/Get Response] and
 * server port to which socket needs to be connected.
 * @return RETURN_OK if thread created successfully else returned RETURN_ERR.
 */
int json_rpc_client_run(struct rpc_client_data_t*);

/**
 * @brief Send the data packet to the server
 * Make sure all the data packet has been successfully send over the socket.
 * @param socket file descriptor to use
 * @param buffer pointing to the json message
 * @return RETURN_OK on success , RETURN_ERR else.
 */
int json_rpc_client_send_data(const int sockfd, const char *buffer);

/**
 * @brief Utility API used to verify client socket thread is running or not.
 * @return RETURN TRUE if server is running else FALSE returned.
 */
int json_rpc_client_is_running();

#endif //_JSON_RPC_CLIENT_PROCESS_H
