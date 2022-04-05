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

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include "json_hal_client.h"
#include "tcp_client.h"
#include "utlist.h"
#include <json-c/json_tokener.h>
#include <json-c/json_util.h>

#ifndef HAVE_JSON_TOKENER_GET_PARSE_END
#define json_tokener_get_parse_end(tok) ((tok)->char_offset)
#endif

//Ticker timeout for aprox. 10s (40 x 250ms)
#define SEND_MSG_TICKER_TIMEOUT         40


/* global variable to keep connection state. */
static int g_connected = FALSE;

/* Global variable which is used as sequence number and returned to client. */
static int g_req_id = DEFAULT_SEQ_START_NUMBER;

/**
 * @brief Structure to keep rpc message tracking for the requests.
 */
typedef struct request_msg_tracking_t
{
    int sequence;                        /* Sequence number of the request message. */
    pthread_mutex_t lock;                /* Mutex lock associated with the request message. */
    pthread_cond_t msg_rcvd;             /* Conditional wait associated with the request message. */
    char buffer[MAX_BUFFER_SIZE];        /* String holds the request/response message. */
    int len;                             /* Length of the buffer. */
    int rc;                              /* Return code, RETURN_OK if response got else RETURN_ERR. */
    int ticker;                          /* Ticket to manage the timeout value. */
    struct request_msg_tracking_t *next; /* Pointer to the next request in the request's linked list. */
} request_msg_tracking_t;

/**
 * @brief Structure to keep rpc event tracking for the subscriptions.
 */
typedef struct event_tracking_t
{
    char event_name[BUF_512];              /* Event name .*/
    char event_notification_type[BUF_512]; /* Event notification type. */
    void (*event_cb)(const char *, const int);         /* Callback needs to be invoked. */
    struct event_tracking_t *next;         /* Pointer to the next request in the request's linked list. */
} event_tracking_t;

/*
 * @brief Global structure object to keep tracking of client socket management.
 */
static rpc_client_data_t g_rpc_client;

/*
 * @brief Global structure object to keep tracking of client's requests.
 */
static request_msg_tracking_t *g_request_msg_tracking = NULL;

/*
 * @brief Global structure object to keep tracking of client's event requests.
 */
static event_tracking_t *g_event_tracking;

/**
 * @brief Mutex instance to track the rpc responses from the server.
 */
static pthread_mutex_t gm_request_msg_tracking_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Mutex instance to track the rpc event subscriptions.
 */
static pthread_mutex_t gm_event_tracking_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Global object to store hal client configuration.
 */
static hal_config_t g_hal_client_config = {0};

/**
 * @brief Callback function to monitor for the timeout expired for the
 * rpc request.
 */
static int request_idle_cb();

/**
 * @brief Callback which is notfied when a client connection
 * established to server.
 * @param Receive fd of connected client
 */
static int client_connected_cb(const int fd);

/**
 * @brief Callback which is notfied when a client disconnected
 * from server.
 * @param Receive fd of disconnected client
 */
static int client_disconnected_cb(const int fd);

/**
 * @brief Callback which is notified when a client socket
 * received event/response from server.
 * @param fd of the connected client
 * @param buffer contains the response in json format
 * @param length of the output buffer
 * @return RETURN_OK if callback executed successfull else return RETURN_ERR
 */
static int response_parse_cb(const int fd, const char *buffer, const int len);

/**
 * @brief Delete the rpc request from the list.
 * @param sequence number of message.
 * @return RETURN_OK if method executed successfull else return RETURN_ERR
 */
static int request_delete_cb(const int sequence);

/**
 * @brief Check the rpc request is expired wthout getting response.
 * @return RETURN_OK if method executed successfull else return RETURN_ERR
 */
static int request_tracking_cb();

/**
 * @brief Send the json formatted request to server.
 *
 * @param Structure which holds the socket connection info
 * @param Json request message
 * @return RETURN_OK if method executed successfull else return RETURN_ERR
 */
static int json_message_send(const rpc_client_data_t *s, const json_object *jmsg);

/**
 * @brief Get a random number to assign as sequence number for the
 * rpc request/response.
 * @return random integer for sequencing the message.
 */
static unsigned int get_req_id();

/**
 * @brief Prepare event subscription message.
 * @param Full DML based path of the event
 * @param Notification type of event
 * @return json_object instance contains the json subscription message.
 */
static json_object *create_event_subscription_message(const char *event_dml_path, const char *event_notification_type);

int json_hal_client_init(const char *hal_conf_path)
{
    POINTER_ASSERT(hal_conf_path != NULL);

    /**
     * Parse the configuration file and retrieve the required
     * configuration for server.
     */
    int ret = RETURN_OK;
    ret = json_hal_load_config(hal_conf_path, &g_hal_client_config);
    if (ret != RETURN_OK)
    {
        LOGERROR("Failed to initialize client library \n");
        return ret;
    }
    g_hal_client_config.request_timeout_period = IDLE_TIMEOUT_PERIOD;
    g_rpc_client.port = g_hal_client_config.server_port_number;
    strcpy(g_rpc_client.host, SERVER_HOST);
    g_rpc_client.func_idle = request_idle_cb;
    g_rpc_client.func_connected = client_connected_cb;
    g_rpc_client.func_disconnected = client_disconnected_cb;
    g_rpc_client.func_parse = response_parse_cb;

    return ret;
}

int json_hal_client_run()
{
    int rc = RETURN_OK;
    rc = json_rpc_client_run(&g_rpc_client);
    if (rc != RETURN_OK)
    {
        LOGERROR("Failed to start client socket thread");
    }
    return rc;
}

static int request_idle_cb()
{
    request_tracking_cb();
    usleep(g_hal_client_config.request_timeout_period);
    return RETURN_OK;
}

static int client_connected_cb(const int fd)
{
    LOGINFO("connect on fd=%d", fd);
    g_connected = TRUE;
    return RETURN_OK;
}

static int client_disconnected_cb(const int fd)
{
    LOGINFO("disconnected on fd=%d", fd);
    g_connected = FALSE;
    return RETURN_OK;
}

#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
/**
 * @brief Enum to identify the response type for the request.
 */
typedef enum response_msg_type_t
{
    RESPONSE_SUCCESS = 0,
    RESPONSE_FAILURE,
    RESPONSE_NOT_SUPPORTED
} response_msg_type_t;

/**
 * @brief API is create a json response to send to the client for
 * Not Supported RPC request.
 * @param req_id (IN) String holds the request's request id
 * @return json response.
 */
static json_object *create_json_reply_event_msg(const char *event_name, const char *req_id, const response_msg_type_t type)
{

    if (NULL == req_id)
    {
        LOGERROR("Invalid argument");
        return NULL;
    }

    json_object *jparamval = json_object_new_object();
    switch (type)
    {
    case RESPONSE_SUCCESS:
        json_object_object_add(jparamval, JSON_RPC_PARAM_STATUS_FIELD, json_object_new_string(JSON_RPC_STATUS_SUCCESS));
        break;
    case RESPONSE_NOT_SUPPORTED:
        json_object_object_add(jparamval, JSON_RPC_PARAM_STATUS_FIELD, json_object_new_string(JSON_RPC_STATUS_NOT_SUPPORTED));
        break;
    case RESPONSE_FAILURE:
        json_object_object_add(jparamval, JSON_RPC_PARAM_STATUS_FIELD, json_object_new_string(JSON_RPC_STATUS_FAILED));
        break;
    }

    //Add event ename
    json_object *jevent_name = json_object_new_object();
    json_object_object_add(jevent_name, JSON_RPC_FIELD_PARAM_NAME, json_object_new_string(event_name));


    /**
     * Create message.
     */
    json_object *jreply = json_object_new_object();
    /* Header */
    json_object_object_add(jreply, JSON_RPC_FIELD_MODULE, json_object_new_string(g_hal_client_config.hal_module_name));
    json_object_object_add(jreply, JSON_RPC_FIELD_VERSION, json_object_new_string(g_hal_client_config.hal_module_version));
    json_object_object_add(jreply, JSON_RPC_FIELD_ACTION, json_object_new_string(JSON_RPC_ACTION_RESULT));
    json_object_object_add(jreply, JSON_RPC_FIELD_ID, json_object_new_string(req_id));

    /* Event name */
    json_object *jparam_array = json_object_new_array();
    json_object_array_add(jparam_array, jevent_name);
    json_object_object_add(jreply, JSON_RPC_FIELD_PARAMS, jparam_array);

    /* Result */
    json_object_object_add(jreply, JSON_RPC_FILED_RESULT, jparamval);

    return jreply;
}
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT

static inline void get_token(json_tokener **pp_token)
{
    *pp_token = NULL;
    int depth = JSON_TOKENER_DEFAULT_DEPTH;

    *pp_token = json_tokener_new_ex(depth);
    if (!(*pp_token))
    {
        LOGERROR("Unable to allocate json_tokener: %s", strerror(errno));
        return;
    }

    json_tokener_set_flags(*pp_token, JSON_TOKENER_STRICT
#ifdef JSON_TOKENER_ALLOW_TRAILING_CHARS
        | JSON_TOKENER_ALLOW_TRAILING_CHARS
#endif
    );

    return;
}
static int response_parse_cb(const int fd, const char *buffer, const int len)
{

    if (NULL == buffer)
    {
        LOGERROR("Invalid buffer data received\n");
        return RETURN_ERR;
    }

    json_object *returnObj = NULL;
    int id = 0;
    const char *action_name = NULL;
    json_tokener* tok = NULL;
    json_object* jobj = NULL;
    const char *event_buf = NULL;
    int event_buf_len = 0;
    int parse_end_expected = len;
    char* aterr = "";

    get_token(&tok);
    if (NULL == tok)
    {
        LOGERROR("Invalid token\n");
        return RETURN_ERR;
    }

    int start_pos = 0;
    while (start_pos != len)
    {
        jobj = json_tokener_parse_ex(tok, &buffer[start_pos], parse_end_expected - start_pos);
        enum json_tokener_error jerr = json_tokener_get_error(tok);
        int parse_end = json_tokener_get_parse_end(tok);
        aterr = (start_pos + parse_end < parse_end_expected)
            ? &buffer[start_pos + parse_end]
            : "";

        /* If JSON messages glue together, need to parse multiple messages from one buffer */
        if ((jobj == NULL) &&
            (jerr == json_tokener_error_parse_unexpected) &&
            (aterr[0]=='{'))
        {
            parse_end_expected = parse_end;
            LOGERROR("Continue to parse again from %d to %d with new token",
                start_pos, parse_end_expected);
            json_tokener_free(tok);
            get_token(&tok);
            if (NULL == tok)
            {
                LOGERROR("Invalid token\n");
                return RETURN_ERR;
            }
            continue;
        }
        else if (jobj == NULL && jerr != json_tokener_continue)
        {
            fflush(stdout);
            int fail_offset = start_pos + parse_end;
            LOGERROR("Failed at offset %d: %s %c\n", fail_offset, json_tokener_error_desc(jerr), aterr[0]);
            json_tokener_free(tok);
            return RETURN_ERR;
        }

        if (jobj != NULL)
        {
            /**
             * Parse the response json message to identify event publish or response
             * for rpc request. In case of event `"action": "publishEvent"` is contained
             * in the response message. In case of event, invoke the registered callback function.
             *
             */
            if (json_object_object_get_ex(jobj, JSON_RPC_FIELD_ACTION, &returnObj))
            {
                action_name = json_object_get_string(returnObj);
                if (strncmp(action_name, JSON_RPC_PUBLISH_EVENT_ACTION_NAME, strlen(JSON_RPC_PUBLISH_EVENT_ACTION_NAME)) == 0)
                {
                    LOGINFO("Event response found");
                    json_object* jevent_msg_param = NULL;

                    /**
                     * Parse `params` field, find the event name. Compare event  name with susbcribed
                     * list and invoke callback if a match found.
                     */
                    if (json_object_object_get_ex(jobj, JSON_RPC_FIELD_PARAMS, &jevent_msg_param))
                    {
                        json_object* jevent_param_array = json_object_array_get_idx(jevent_msg_param, JSON_RPC_PARAM_ARR_INDEX);
                        json_object* jevent_msg_param_val = NULL;
                        if (json_object_object_get_ex(jevent_param_array, JSON_RPC_FIELD_PARAM_NAME, &jevent_msg_param_val))
                        {
                            char event_name[BUF_512] = { '\0' };
                            strncpy(event_name, json_object_get_string(jevent_msg_param_val), sizeof(event_name));
                            LOGINFO("Event name = %s", event_name);
                            event_tracking_t* events = NULL;
                            pthread_mutex_lock(&gm_event_tracking_lock);
                            LL_FOREACH(g_event_tracking, events)
                            {
                                if (!strncmp(events->event_name, event_name, strlen(event_name)))
                                {
                                    if (events->event_cb != NULL)
                                    {
                                        LOGINFO("Event callback invoked");
                                        event_buf = json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PRETTY);
                                        event_buf_len = strlen(event_buf);
#ifdef DEBUG_ENABLED
                                        LOGINFO("Event Msg = %s \n", event_buf);
#endif
                                        events->event_cb(event_buf, event_buf_len);
                                        event_buf = NULL; // reset buffer.

#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
                                        /**
                                         * Notify the result back to requester.
                                         */
                                        /* Retrieve message request id (sequence number). */
                                        if (!json_object_object_get_ex(jobj, JSON_RPC_FIELD_ID, &returnObj))
                                        {
                                            LOGERROR("Json request doesn't have sequence number/id.");
                                            json_object_put(jobj);
                                            continue;
                                        }
                                        
                                        /* Get reqid. */
                                        char req_id[BUF_64] = {'\0'};
                                        strncpy(req_id, json_object_get_string(returnObj), sizeof(req_id));
                                         
                                         
                                        json_object *jreply_msg = create_json_reply_event_msg(event_name, req_id, RESPONSE_SUCCESS);
                                        if(json_message_send(&g_rpc_client, jreply_msg) != RETURN_OK)
                                        {
                                            LOGERROR("Failed to send the data to client \n");
                                        }

                                        json_object_put(jreply_msg);
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT

                                    }
                                }
                            }
                            pthread_mutex_unlock(&gm_event_tracking_lock);
                        }
                    }
                    else
                    {
                        LOGERROR("not found any event subscription for this event");
                    }
                }
                else
                {
                    /* Response for RPC  request. */
                    if (json_object_object_get_ex(jobj, JSON_RPC_FIELD_ID, &returnObj))
                    {
                        id = (int)strtol(json_object_get_string(returnObj), NULL, 16);
                    }
                    else
                    {
                        json_object_put(jobj);
                        json_tokener_free(tok);
                        return RETURN_ERR;
                    }
                    /* Check for the list to identify the rpc request
                    * and if a matching id found, fill its buffer with response
                    * and send the msg_rcd signal.
                    */
                    pthread_mutex_lock(&gm_request_msg_tracking_lock);
                    request_msg_tracking_t* tmp, * rpc;
                    LL_FOREACH_SAFE(g_request_msg_tracking, rpc, tmp)
                    {
                        if (rpc->sequence == id)
                        {
                            LL_DELETE(g_request_msg_tracking, rpc);
                            memset(rpc->buffer, 0, len + 1);
                            strncpy(rpc->buffer, buffer, len);
                            rpc->rc = RETURN_OK;
                            rpc->len = len;
                            pthread_mutex_lock(&rpc->lock);
                            pthread_cond_signal(&rpc->msg_rcvd);
                            pthread_mutex_unlock(&rpc->lock);
                            break;
                        }
                    }
                    pthread_mutex_unlock(&gm_request_msg_tracking_lock);
                }
            }
            json_object_put(jobj);
        } /* End of jobj != NULL */

        start_pos += json_tokener_get_parse_end(tok);
        parse_end_expected = len;
        if (start_pos >= len)
        {
            json_tokener_free(tok);
            return RETURN_OK;
        }
    } /* End of while */

    json_tokener_free(tok);
    return RETURN_OK;
}


/* Keep tracking the requests whether its getting a response within a timeout period,
 * else unlock its mutex and returned. */
static int request_tracking_cb()
{
    request_msg_tracking_t *tmp, *rpc;
    pthread_mutex_lock(&gm_request_msg_tracking_lock);
    LL_FOREACH_SAFE(g_request_msg_tracking, rpc, tmp)
    {
        rpc->ticker--;
        if (rpc->ticker <= 0)
        {
            LL_DELETE(g_request_msg_tracking, rpc);
            rpc->rc = RETURN_ERR;
            rpc->len = INVALD_LENGTH;
            pthread_mutex_lock(&rpc->lock);
            pthread_cond_signal(&rpc->msg_rcvd);
            pthread_mutex_unlock(&rpc->lock);

            LOGERROR("Message Expired on Sequence %d\r\n", rpc->sequence);
        }
    }
    pthread_mutex_unlock(&gm_request_msg_tracking_lock);
    return RETURN_OK;
}

/* Delete the rpc request from the list. */
static int request_delete_cb(const int sequence)
{
    request_msg_tracking_t *tmp, *rpc;
    pthread_mutex_lock(&gm_request_msg_tracking_lock);
    LL_FOREACH_SAFE(g_request_msg_tracking, rpc, tmp)
    {
        if (rpc->sequence == sequence)
        {
            LL_DELETE(g_request_msg_tracking, rpc);
            LOGERROR("Message Deleted on Sequence %d\r\n", rpc->sequence);
            break;
        }
    }
    pthread_mutex_unlock(&gm_request_msg_tracking_lock);
    return RETURN_OK;
}

/**
 * Send the request message to the server socket, sync the response
 * from server and return back with the filled data to caller.
 *
 * This API will send the requested message to the server and wait for the
 * response from the server. This API is blocked  until we get a proper
 * response from the server or timed out happened.
 *
 * Internally it maintains a mutex lock and send the data to server. This mutex
 * lock unlocked once we get response from server or when the timeout period expired.
 */
int json_hal_client_send_and_get_reply(const json_object *jrequest_msg, json_object **reply_msg)
{
    POINTER_ASSERT(jrequest_msg != NULL);

    request_msg_tracking_t *rpc;
    int rc = RETURN_ERR;

    rpc = (request_msg_tracking_t *)calloc(1, sizeof(request_msg_tracking_t));
    POINTER_ASSERT(rpc != NULL);

    /**
     * Find reqId from the json request message.
     */
    json_object *jrequest_msg_param = NULL;
    int request_msg_req_id = 0;

    if (json_object_object_get_ex(jrequest_msg, JSON_RPC_FIELD_ID, &jrequest_msg_param))
    {
        request_msg_req_id = (int)strtol(json_object_get_string(jrequest_msg_param), NULL, 16);
    }
    else
    {
        LOGERROR("Failed to get reqId field from json request message \n");
        free(rpc);
        rpc = NULL;
        return RETURN_ERR;
    }

    pthread_mutex_init(&rpc->lock, NULL);
    pthread_cond_init(&rpc->msg_rcvd, NULL);

    pthread_mutex_lock(&rpc->lock);
    rpc->sequence = request_msg_req_id;

    //Timeout period.
    rpc->ticker = SEND_MSG_TICKER_TIMEOUT;
    rpc->rc = RETURN_ERR;

    LL_APPEND(g_request_msg_tracking, rpc);

    rc = json_message_send(&g_rpc_client, jrequest_msg);
    if (rc != RETURN_OK)
    {
        LOGERROR("Failed to send the request to server");
        pthread_mutex_unlock(&rpc->lock);
        pthread_mutex_destroy(&rpc->lock);
        pthread_cond_destroy(&rpc->msg_rcvd);
        request_delete_cb(request_msg_req_id);
        free(rpc);
        rpc = NULL;
        return RETURN_ERR;
    }

    /* Wait for the signal of msg_rcvd.
     * msg_rcvd signal will be sent by the thread receiving the messages 
     * or after the message timeout.
     */
    pthread_cond_wait(&rpc->msg_rcvd, &rpc->lock);
    pthread_mutex_unlock(&rpc->lock);
    pthread_mutex_destroy(&rpc->lock);
    pthread_cond_destroy(&rpc->msg_rcvd);
    request_delete_cb(request_msg_req_id);

    rc = rpc->rc;
    /* Got response and fill it back for requester. */
    if (rpc->rc >= 0)
    {
        *reply_msg = json_tokener_parse(rpc->buffer);
    }
    else
    {
        LOGERROR("Failed to execute rpc client request, sequence = [%d], rc = [%d] \n", request_msg_req_id, rc);
    }

    if (rpc)
    {
        free(rpc);
        rpc = NULL;
    }
    return rc;
}
/* Event callback register. */
int json_hal_client_subscribe_event(event_callback eventcb, const char *event_path_name, const char *event_notification_type)
{
    POINTER_ASSERT(event_path_name != NULL);
    POINTER_ASSERT(event_notification_type != NULL);

    int rc = RETURN_ERR;
    json_object *reply_msg;
    json_object *jsubs_msg = create_event_subscription_message(event_path_name, event_notification_type);
    POINTER_ASSERT(jsubs_msg != NULL);

    LOGINFO("Event subscription message = %s", json_object_to_json_string_ext(jsubs_msg, JSON_C_TO_STRING_PRETTY));
    rc = json_hal_client_send_and_get_reply(jsubs_msg, &reply_msg);
    if (rc < 0)
    {
        LOGERROR("Failed to subscribe event %s \n", event_path_name);
        json_object_put(jsubs_msg);
        return RETURN_ERR;
    }

    json_object_put(jsubs_msg);
    json_object_put(reply_msg);
    LOGINFO("Event %s subscribed", event_path_name);

    /* Store the event subscription data into global `event_tracking_t` list. */
    event_tracking_t *eventsubs = NULL;
    eventsubs = (event_tracking_t *)malloc(sizeof(event_tracking_t));
    if (NULL == eventsubs)
    {
        LOGERROR("Failed to allocate memory \n");
        return RETURN_ERR;
    }
    eventsubs->event_cb = eventcb;
    strncpy(eventsubs->event_name, event_path_name, sizeof(eventsubs->event_name));
    strncpy(eventsubs->event_notification_type, event_notification_type, sizeof(eventsubs->event_notification_type));
    pthread_mutex_lock(&gm_event_tracking_lock);
    LL_APPEND(g_event_tracking, eventsubs);
    pthread_mutex_unlock(&gm_event_tracking_lock);

    return RETURN_OK;
}

/* Check client is connected. */
int json_hal_is_client_connected()
{
    return g_connected;
}

int json_hal_client_terminate()
{

    /* Stop client socket thread. */
    g_rpc_client.RUNNING = FALSE;

    /**
     * Make sure server thread stopped and closed all client sockets.
     * Timeout after 4 seconds if server is not terminated.
     */
    int counter = IDLE_TIMEOUT_PERIOD;
    do
    {
        if (json_rpc_client_is_running() == FALSE)
        {
            LOGINFO("Client socket thread terminated gracefully");
            break;
        }

        usleep(IDLE_TIMEOUT_PERIOD);
        counter--;
    } while (counter > 0);

    /* Free the global lists for the rpc requests and event subscriptions. */
    request_msg_tracking_t *tmp, *rpc;
    pthread_mutex_lock(&gm_request_msg_tracking_lock);
    LL_FOREACH_SAFE(g_request_msg_tracking, rpc, tmp)
    {
        LL_DELETE(g_request_msg_tracking, rpc);
        if (rpc)
        {
            free(rpc);
            rpc = NULL;
        }
    }

    if (g_request_msg_tracking != NULL)
    {
        free(g_request_msg_tracking);
        g_request_msg_tracking = NULL;
    }
    pthread_mutex_unlock(&gm_request_msg_tracking_lock);

    /* Delete event subscription list. */
    event_tracking_t *tmp_event, *rpc_event;
    pthread_mutex_lock(&gm_event_tracking_lock);
    LL_FOREACH_SAFE(g_event_tracking, rpc_event, tmp_event)
    {
        LL_DELETE(g_event_tracking, rpc_event);
        if (rpc_event)
        {
            free(rpc_event);
            rpc_event = NULL;
        }
    }

    if (g_event_tracking != NULL)
    {
        free(g_event_tracking);
        g_event_tracking = NULL;
    }
    pthread_mutex_unlock(&gm_event_tracking_lock);
    return RETURN_OK;
}

static int json_message_send(const rpc_client_data_t *client_sock, const json_object *jmsg)
{

    POINTER_ASSERT(jmsg != NULL);
    POINTER_ASSERT(client_sock != NULL);

    const char *response_msg_buffer = NULL;
    int rc = RETURN_ERR;
    response_msg_buffer = json_object_to_json_string_ext(jmsg, JSON_C_TO_STRING_PRETTY);
    POINTER_ASSERT(response_msg_buffer != NULL);

    if (client_sock->RUNNING == TRUE)
    {
        rc = json_rpc_client_send_data(client_sock->sock, response_msg_buffer);
        if (rc != RETURN_OK)
        {
            LOGERROR("Failed to send the request to server");
        }
    }
    return rc;
}

json_object *json_hal_client_get_request_header(const char *action_name)
{
    if (action_name == NULL)
    {
        LOGERROR("Invalid argument \n");
        return NULL;
    }

    int req_id = 0;
    char id[17] = {'\0'};

    req_id = get_req_id();
    sprintf(id, "%8.8d", req_id);

    json_object *jmsg = json_object_new_object();
    json_object_object_add(jmsg, JSON_RPC_FIELD_MODULE, json_object_new_string(g_hal_client_config.hal_module_name));
    json_object_object_add(jmsg, JSON_RPC_FIELD_VERSION, json_object_new_string(g_hal_client_config.hal_module_version));
    json_object_object_add(jmsg, JSON_RPC_FIELD_ID, json_object_new_string(id));
    json_object_object_add(jmsg, JSON_RPC_FIELD_ACTION, json_object_new_string(action_name));

    /**
     * For all kind of requests except the `getSchema` request, request parameters will be in
     * the `params` array. So here we have created the emoty array and attached to the request
     * message. So manager application can use this array to append their parameters.
     */
    if (strncmp(action_name, JSON_RPC_ACTION_GET_SCHEMA, strlen(JSON_RPC_ACTION_GET_SCHEMA)) != 0)
    {
        /* Create params array and embed into the json_object. */
        json_object *jparam_array = json_object_new_array();
        json_object_object_add(jmsg, JSON_RPC_FIELD_PARAMS, jparam_array);
    }

    return jmsg;
}

static json_object *create_event_subscription_message(const char *event_dml_path, const char *event_notification_type)
{
    json_object *jsubs_msg = json_hal_client_get_request_header(JSON_RPC_SUBSCRIBE_EVENT_ACTION_NAME);
    if (jsubs_msg == NULL)
    {
        LOGERROR("Failed to get the json request message header");
        return NULL;
    }
    /**
     * Append event params to the request message.
     */
    json_object *jobj = json_object_new_object();
    json_object_object_add(jobj, JSON_RPC_FIELD_PARAM_NAME, json_object_new_string(event_dml_path));
    json_object_object_add(jobj, JSON_RPC_FIELD_PARAM_NOTIFICATION_TYPE, json_object_new_string(event_notification_type));

    json_object *jparams = NULL;
    if (json_object_object_get_ex(jsubs_msg, JSON_RPC_FIELD_PARAMS, &jparams))
    {
        json_object_array_add(jparams, jobj);
    }
    else
    {
        printf("Request doesn't have the Params filed, creating params array and appending params into it");
        /* Create params array and embed into the json_object. */
        json_object *jparam_array = json_object_new_array();
        json_object_array_add(jparam_array, jobj);
        json_object_object_add(jsubs_msg, JSON_RPC_FIELD_PARAMS, jparam_array);
    }

    return jsubs_msg;
}
int json_hal_get_result_status(const json_object *jobj, json_bool *status)
{
    if (jobj == NULL || status == NULL)
    {
        LOGERROR("Invalid argument");
        return RETURN_ERR;
    }

    int rc = RETURN_ERR;
    json_object *jreturnObj = NULL;

    if (json_object_object_get_ex(jobj, JSON_RPC_FILED_RESULT, &jreturnObj))
    {
        json_object *jresult = NULL;
        if (json_object_object_get_ex(jreturnObj, JSON_RPC_PARAM_STATUS_FIELD, &jresult))
        {
            const char *result = json_object_get_string(jresult);
            if (strncmp(result, JSON_RPC_STATUS_SUCCESS, strlen(JSON_RPC_STATUS_SUCCESS)) == 0)
            {
                *status = TRUE;
            }
            else
            {
                *status = FALSE;
            }

            rc = RETURN_OK;
        }
        else
        {
            LOGERROR("Failed to get Status param from json response message");
        }
    }
    else
    {
        LOGERROR("Failed to get Result field from json response message");
    }

    return rc;
}

int json_hal_get_total_param_count(const json_object *json_msg)
{
    int total_param_count = 0;
    json_object *jparams;
    if (json_object_object_get_ex(json_msg, JSON_RPC_FIELD_PARAMS, &jparams))
    {
        total_param_count = json_object_array_length(jparams);
    }

    return total_param_count;
}

unsigned int get_req_id(void)
{
    g_req_id++;
    if (g_req_id > INT_MAX)
        g_req_id = DEFAULT_SEQ_START_NUMBER;

    return g_req_id;
}
