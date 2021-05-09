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

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <syslog.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "tcp_server.h"
#include "json_rpc_common.h"
#include "utlist.h"

/**
 *  Pointer to hold client connection list.
 **/
static client_list_t *g_client_list;

/**
 * Global variable to store the server thread running status.
 */
static int g_rpc_server_running_status = FALSE;

/**
 * @brief Server socket handler thread routine.
 * @param Received filled rpc_server_data_t structure object (typecasted to void*) contains socket port and the callback functions needs to be execute
 * once receive connection request, data from client side etc.
 * This thread will exit when the terminate api invoked.
 */
static void *rpc_server_handler(void *arg);

int json_rpc_server_send_data(const int sockfd, const char *buffer)
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
#ifdef DEBUG_ENABLED
    LOGINFO("Response json message = %.*s", total_bytes_sent, buffer);
#endif
    return RETURN_OK;
}

int json_rpc_server_run(rpc_server_data_t *server)
{
    if (NULL == server)
    {
        LOGERROR("Invalid argument \n");
        return RETURN_ERR;
    }

    int rc = RETURN_OK;
    pthread_t socket_thread;
    pthread_attr_t attributes;

    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    rc = pthread_create(&socket_thread, &attributes, rpc_server_handler, (void *)server);
    if (rc != RETURN_OK)
    {
        LOGERROR("Failed to create server socket handler thread \n");
    }
    pthread_attr_destroy(&attributes);
    return rc;
}

static void *rpc_server_handler(void *arg)
{
    rpc_server_data_t *serverdata = NULL;
    int i, len, rc, on = 1;
    int listen_sd, max_sd, new_sd;
    int desc_ready = FALSE;
    char buffer[MAX_BUFFER_SIZE] = {"\0"};
    struct sockaddr_in addr;
    struct timeval timeout;
    fd_set master_set, working_set;

    if (arg == NULL)
    {
        LOGERROR("Invalid  memory \n");
        pthread_exit(0);
    }

    pthread_detach(pthread_self());

    serverdata = (rpc_server_data_t *)arg;
    g_client_list = NULL;
    serverdata->running = TRUE;
    listen_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sd < 0)
    {
        perror("socket() failed");
        goto EXIT;
    }
    rc = setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    if (rc < 0)
    {
        perror("setsockopt() failed");
        close(listen_sd);
        goto EXIT;
    }
    rc = ioctl(listen_sd, FIONBIO, (char *)&on);
    if (rc < 0)
    {
        perror("ioctl() failed");
        close(listen_sd);
        goto EXIT;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(serverdata->port);
    rc = bind(listen_sd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0)
    {
        perror("bind() failed");
        close(listen_sd);
        goto EXIT;
    }

    LOGINFO("server started at port %d \n", serverdata->port);
    rc = listen(listen_sd, 32);
    if (rc < 0)
    {
        perror("listen() failed");
        close(listen_sd);
        goto EXIT;
    }
    FD_ZERO(&master_set);
    max_sd = listen_sd;
    FD_SET(listen_sd, &master_set);
    do
    {
        g_rpc_server_running_status = TRUE;

        memcpy(&working_set, &master_set, sizeof(master_set));
        /* Wait up to 50 milliseconds */
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000;
        rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);
        if (rc < 0)
        {
            perror("select() failed");
            break;
        }
        desc_ready = rc;
        for (i = 0; i <= max_sd && desc_ready > 0; ++i)
        {
            if (FD_ISSET(i, &working_set))
            {
                desc_ready -= 1;
                if (i == listen_sd)
                {

                    new_sd = accept(listen_sd, NULL, NULL);
                    if (new_sd < 0)
                    {
                        if (errno != EWOULDBLOCK)
                        {
                            perror("accept() failed");
                        }
                        continue;
                    }
                    LOGINFO("New incoming connection - %d\r\n", new_sd);
                    /* Connect callback. */
                    if (serverdata->func_connect != NULL)
                    {
                        serverdata->func_connect(new_sd);
                    }
                    /* Store all the new client connection data into the list and update it into
                         * global list.
                         */
                    client_list_t *conn = NULL;
                    conn = (client_list_t *)malloc(sizeof(client_list_t));
                    conn->fd = new_sd;
                    LL_APPEND(g_client_list, conn);
                    /* Update client fd into reading set. */
                    FD_SET(new_sd, &master_set);
                    if (new_sd > max_sd)
                        max_sd = new_sd;
                }
                else
                {
                    memset(buffer, 0, MAX_BUFFER_SIZE);
                    /* Receive data from client. */
                    rc = recv(i, buffer, sizeof(buffer), 0);
                    if (rc < 0)
                    {
                        if (errno != EWOULDBLOCK)
                        {
                            perror(" recv() failed");
                        }
                        continue;
                    }
                    if (rc == 0)
                    {
                        LOGINFO("Connection closed on fd %d\n", i);
                        if (serverdata->func_disconnect != NULL)
                        {
                            serverdata->func_disconnect(i);
                        }

                        close(i);
                        FD_CLR(i, &master_set);
                        if (i == max_sd)
                        {
                            while (FD_ISSET(max_sd, &master_set) == FALSE)
                                max_sd -= 1;
                        }
                        /* Delete connection from the list once client got disconnected. */
                        client_list_t *tmp, *conn;
                        LL_FOREACH_SAFE(g_client_list, conn, tmp)
                        {
                            if (conn->fd == i)
                            {
                                LL_DELETE(g_client_list, conn);
                                free(conn);
                            }
                        }
                        continue;
                    }

                    /**********************************************/
                    /* Data was received                          */
                    /**********************************************/
                    len = rc;
                    if (serverdata->func_process != NULL)
                    {
                        serverdata->func_process(i, buffer, len);
                    }
                } /* End of existing connection is readable */
            }     /* End of if (FD_ISSET(i, &working_set)) */
        }         /* End of loop through selectable descriptors */

    } while (serverdata->running == TRUE);

    for (i = 0; i <= max_sd; ++i)
    {
        if (FD_ISSET(i, &master_set))
        {
            LOGINFO("Closing client [%d] connection", i);
            close(i);
        }
    }

    g_rpc_server_running_status = FALSE;

EXIT:
    pthread_exit(0);
}

/**
 * @brief Utility API used to verify server socket thread is running or not.
 * @return RETURN TRUE if server is running else FALSE returned.
 */
inline int json_rpc_server_is_running()
{
    return g_rpc_server_running_status;
}
