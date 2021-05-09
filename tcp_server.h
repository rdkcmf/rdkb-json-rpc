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
#ifndef _RPC_PROCESS_H
#define _RPC_PROCESS_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/**
 * @brief Structure used to hold the details client connections to the server.
 */
typedef struct client_list_t
{
  int fd;                            /* Client socket fd. */
  struct client_list_t *next; /*  Pointer to the next node in the linked list. */
}client_list_t;

/**
 * @brief Structure to hold the details of server and to define its
 * required callback functions.
 */
typedef struct rpc_server_t
{
  uint32_t port;                                        /* Server port number. */
  int (*func_disconnect)(int fd);                       /* Callback invoked when connection disconnected. */
  int (*func_connect)(int fd);                          /* Callback invoked when connection established. */
  int (*func_process)(int fd, char *buf, uint32_t len); /* Callback invoked when receive message. */
  unsigned char running;                                /* Flag indicates thread is running or not. */
}rpc_server_data_t;

/**
 * @brief API will start the server socket and listen for the client connections.
 * @param Filled rpc_server_data_t structure contains the port number, callback
 * functions needs to be invoked when connect/disconnect connections or receive
 * message on socket.
 * @return RETURN_OK if socket thread created successfully else returned error.
 */
int json_rpc_server_run(rpc_server_data_t *server);

/**
 * @brief Send the data packet to the client
 * Make sure all the data packet has been successfully send over the socket.
 * @param socket file descriptor to use
 * @param buffer pointing to the json message
 * @return RETURN_OK on success , RETURN_ERR else.
 */
int json_rpc_server_send_data(const int sockfd, const char *buffer);

/**
 * @brief Utility API used to verify server socket thread is running or not.
 * @return RETURN TRUE if server is running else FALSE returned.
 */
int json_rpc_server_is_running();

#endif
