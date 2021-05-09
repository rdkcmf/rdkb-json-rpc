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

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "tcp_client.h"
#include <sys/time.h>

#define LOOP_TIMEOUT 250000 // timeout in microseconds.

/**
 * Enum to handle different states of connection thread.
 */
typedef enum _SOCKET_TRANISTION_STAGE_
{
    SOCKET_INIT = 0,
    SOCKET_CONNECT,
    SOCKET_RECEIVE
} SOCKET_TRANISTION_STAGE;

/**
 * Global variable to store the server thread running status.
 */
static int g_rpc_client_running_status = FALSE;

/**
 * @brief Socket client main thread
 * This thread maintains a state maching to handle the connection and response from server.
 * @param Filled structure contains to estbalish server connection and its required callbacks.
 */
static void *rpc_client_handler(void *paramPtr);

int json_rpc_client_send_data(const int sockfd, const char *buffer)
{
    POINTER_ASSERT(buffer != NULL);
    int total_bytes_sent = 0; // how many bytes we've sent
    int total_bytes_left = 0; // how many we have left to send
    int ret = RETURN_OK;

    total_bytes_left = strlen(buffer);
    while (total_bytes_sent < total_bytes_left)
    {
        ret = send(sockfd, buffer + total_bytes_sent, total_bytes_left, 0);
        if (ret == RETURN_ERR)
        {
            LOGERROR("Failed to send the response message over socket, [%d] bytes left to send", total_bytes_left);
            return RETURN_ERR;
        }
        total_bytes_sent += ret;
        total_bytes_left -= ret;
    }
    return RETURN_OK;
}

int json_rpc_client_run(rpc_client_data_t *s)
{
    int rc = RETURN_OK;
    pthread_t socket_thread;
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    s->sock = INVALID_SOCKFD;
    rc = pthread_create(&socket_thread, &attributes, rpc_client_handler, s);
    if (rc != RETURN_OK)
    {
        LOGERROR("Failed to start sever socket");
    }

    pthread_attr_destroy(&attributes);
    return rc;
}

/* State machine to manage client connection and response. */
static void *rpc_client_handler(void *paramPtr)
{
    int rc;
    int sret;
    int max_sd;
    struct timeval tv;
    fd_set read_set;

    struct rpc_client_data_t *params = (rpc_client_data_t *)paramPtr;
    struct sockaddr_in server;

    if (NULL == params)
    {
        LOGERROR("Invalid params to start thread \n");
        pthread_exit(NULL);
    }
    pthread_detach(pthread_self());

    params->RUNNING = TRUE;
    params->state = SOCKET_INIT;
    g_rpc_client_running_status = TRUE;

    while(params->RUNNING == TRUE)
    {
        switch (params->state)
        {
            case SOCKET_INIT:
                params->sock = socket(AF_INET, SOCK_STREAM, 0);
                if(params->sock == -1) {
                    LOGERROR("Could not create socket, Error Number : %d, Error : %s", errno, strerror(errno));
                    pthread_exit(NULL);
                }
                fcntl(params->sock, F_SETFL, O_NONBLOCK);
                server.sin_addr.s_addr = inet_addr(params->host);
                server.sin_family = AF_INET;
                server.sin_port = htons(params->port);
                params->state = SOCKET_CONNECT;
                break;   // State == 0

            case SOCKET_CONNECT:
                rc = 0;
                rc = connect(params->sock, (struct sockaddr*)&server, sizeof(server));
                if(rc < 0) {
                    switch (errno) {
                        case EBADF:
                        case EISCONN:
                        case EADDRNOTAVAIL:
                            LOGERROR("connect failed, Error Number : %d, Error : %s", errno, strerror(errno));
                            close(params->sock);
                            params->sock = INVALID_SOCKFD;
                            params->state = SOCKET_INIT;
                        case EINPROGRESS:
                        case EALREADY:
                        case ECONNREFUSED:
                        default    :    sleep(1);
                                        break;
                    }
                }else {
                    if(params->func_connected != NULL) {
                        params->func_connected(params->sock);
                    }
                    params->state = SOCKET_RECEIVE;
                }
                break;   // State == 1
            case SOCKET_RECEIVE:
                rc = 0;
                sret = 0;
                memset(params->buffer, 0, MAX_BUFFER_SIZE);
                FD_ZERO(&read_set);
                FD_SET(params->sock,&read_set);
                max_sd = params->sock;

                /* Wait up to 250 milliseconds */
                tv.tv_sec = 0;
                tv.tv_usec = LOOP_TIMEOUT;
                sret = select(max_sd + 1, &read_set, NULL, NULL, &tv);
                if (sret < 0) {
                    LOGERROR("select() failed, Error Number : %d, Error : %s", errno, strerror(errno));
                    break;
                } else if(sret == 0) {
                    /* Time out */
                    break;
                }

            if (FD_ISSET(params->sock, &read_set)) {
                rc = recv(params->sock, params->buffer, MAX_BUFFER_SIZE, 0);
                if(rc < 0) {
                    LOGERROR("recv failed, Error Number : %d, Error : %s", errno, strerror(errno));
                    break;
                }
                else if(rc == 0) {
                    if (params->func_disconnected != NULL) {
                        params->func_disconnected(params->sock);
                    }
                    close(params->sock);
                    params->sock = INVALID_SOCKFD;
                    params->state = SOCKET_INIT;
                    break;
                }
                else { //rc > 0
                    if(params->func_parse != NULL) {
                        params->func_parse(params->sock, params->buffer, rc);
                    }
                } // Got a reponse for something!
                
            } else {
                LOGERROR("sock desc is not set");
            }
            break;  // End of state == SOCKET_RECEIVE
        } // End of switch
        
        
        //IDLE
        if (params->func_idle != NULL)
        {
            params->func_idle();
        }
        
    } // End of RUNNING;
    /**
     * Close the socket.
     * Exit thread.
    */
    if (params)
    {
        close(params->sock);
        params->sock = INVALID_SOCKFD;
    }
    g_rpc_client_running_status = FALSE;
    pthread_exit(0);
}

/**
 * @brief Utility API used to verify client socket thread is running or not.
 * @return RETURN TRUE if server is running else FALSE returned.
 */
inline int json_rpc_client_is_running()
{
    return g_rpc_client_running_status;
}
