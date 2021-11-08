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
#include "json_hal_server.h"
#include "tcp_server.h"
#include "json_rpc_common.h"
#include "json_schema_validator_wrapper.h"
#include "utlist.h"
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <stdbool.h>
#include <json-c/json.h>
#include <json-c/json_tokener.h>
#include <json-c/json_util.h>
#include <unistd.h>
#include <errno.h>


#ifndef HAVE_JSON_TOKENER_GET_PARSE_END
#define json_tokener_get_parse_end(tok) ((tok)->char_offset)
#endif


#define SEND_EVENT_SUBSCRIPTION_TIMEOUT              10         //seconds
#define SEND_EVENT_SUBSCRIPTION_LOOP_TIME            100000     //microseconds


typedef enum
{
    NO_EVENT_MSG = 0,
    WAIT_EVENT_REPLY_MSG,
    EVENT_REPLY_SUCCESS,
    EVENT_REPLY_ERROR
} event_subscription_status_t;

/**
 * @brief This structure is used to hold the details of
 * hal registered functions.
 */
typedef struct action_callback_list_t
{
    char function_name[MAX_FUNCTION_LEN]; /* Method name */
    action_callback cb;                   /* Callback function invoked when server receive a message. Passed string formatted message buffer.*/
    struct action_callback_list_t *next;  /* Pointer to the next node in the linked list. */
} action_callback_list_t;


/**
 * @brief This structure is used to hold the details of
 * event message status.
 */
typedef struct event_subscription_msg_status
{
    char req_id[BUF_64];                    /* Request ID */
    event_subscription_status_t status;     /* Status of the Event message*/
} event_subscription_msg_status_t;


/**
 * @brief This structure is used to hold the details of
 * event subscribed clients.
 */
typedef struct event_subscriptions_list_t
{
    int fd;                                     /* Client socket fd. */
    char event[BUF_512];                        /* Event name. */
    eNotificationType_t event_type;             /* Notification Type */
    event_subscription_msg_status_t last_msg;   /* Status of the last event message sent*/
    struct event_subscriptions_list_t *next;    /* Pointer to the next node in the linked list. */
} event_subscriptions_list_t;

/**
 * @brief Structure used to hold the details client connections to the server.
 */
typedef struct client_connections_t
{
    int fd;                            /* Client socket fd. */
    struct client_connections_t *next; /*  Pointer to the next node in the linked list. */
} client_connections_t;

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
 * @brief Mutex instance to track the rpc event subscriptions.
 */
pthread_mutex_t gm_subscription_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Global structure instance to store HAL server initial configuration.
 */
static hal_config_t g_server_config ={0};

/**
 * @brief Global structure pointer to hold all the client rpc connections
 */
static action_callback_list_t *g_hal_functions_list = NULL;

/**
 * @brief Global structure pointer to hold all the client rpc event subscriptions.
 */
static event_subscriptions_list_t *g_event_subscriptions_list = NULL;

/**
 * @brief Global structure which filled the information to start socket server. */
static rpc_server_data_t g_rpc_server;

/* Global variable which is used as sequence number and returned to client. */
static int g_seqnumber = DEFAULT_SEQ_START_NUMBER;

/**
 * @brief Utility API to respond `Not Supported` response to client if a registered method
 * not found for the action from vendor software.
 * @param (IN) String hold the sequence id of the request.
 * @param (IN) Type of response
 * @return json response.
 */
static json_object *create_json_reply_msg(const char *req_id, const response_msg_type_t type);

/**
 * @brief Intialisation of event subscription data
 * @param  evevent subscription structure instance
 * @return return RETURN_OK if successful else return RETURN_ERR.
 */
static int initialise_event_subscription_data(event_subscriptions_list_t *event_subs_data);

/**
 * @brief API which will pass json request and retrieve event name, type and notification type
 * from json request message.
 * @param (IN) json request message
 * @param (OUT) Parse event data and filled into rpc_event_subs_data structure instance
 * @return json response.
 */
static int get_event_subscription_data_from_msg(const json_object *jmsg, event_subscriptions_list_t *rpc_event_subs_data);


#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
/**
 * @brief API which will pass json reply message and retrieve event data and result
 * from json request message.
 * @param (IN) json request message
 * @param (OUT) Parse event data and update rpc_event_subs_data structure instance
 * @return json response.
 */
static int get_event_reply_data_from_msg(const json_object *jmsg, event_subscriptions_list_t *rpc_event_subs_data);
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT

/**
 * @brief API which will prepare an event json message and replied json object.
 * @param (IN) Event name
 * @param (IN) Event value
 * @return json response contained event message
 */
static json_object *create_publish_event_msg(const char *event_name, const char *event_val);

/**
 * @brief Add subscribed client details into the subscription list.
 * @param (IN) Pointer to rpc_event_subscription structure contains the event data
 */
static void add_event_subscription_to_list(event_subscriptions_list_t *subs);

/**
 * @brief Remove subscribed client details from the subscription list.
 * @param (IN) fd of the incoming client.
 * @return null
 */
static void remove_event_subscription_from_list(int fd);

/**
 * @brief Retreive rpc handler function from the list
 * Traverse through the list and if a match found with the
 * compared function, returned the structure object. This
 * contains the callback handler for the rpc function.
 *
 * @param  (IN) Function name indicates rpc function.
 * @return Pointer to action_callback_list_t struct which contains the callback handler.
 */
static action_callback_list_t *get_registered_rpc_action_by_name(char *func_name);

/**
 * @brief Callback function being invoked when a client connected to server.
 * @param connected client fd.
 * @return return RETURN_OK if successful else return RETURN_ERR.
 */
static int client_connected_cb(int fd);

/**
 * @brief Callback function being invoked when a client disconnected from server.
 * @param disconnected client fd.
 * @return return RETURN_OK if successful else return RETURN_ERR.
 */
static int client_disconnected_cb(int fd);

/**
 * @brief This callback function invoked when the socket received request from client side.
 * @param fd of the requested client.
 * @param buffer contains the json formatted data.
 * @param length of the buffer
 */
static int message_process_cb(int fd, char *buffer, int len);

/**
 * @brief Send the data packet to the client
 *
 * @param socket file descriptor to use
 * @param json message
 * @return RETURN_OK on success , RETURN_ERR else.
 */
static int socket_send(const int sockfd, const json_object *jmsg);

/**
 * @brief Get a random number to assign as sequence number for the
 * rpc request/response.
 * @return random integer for sequencing the message.
 */
static unsigned int get_sequence_number(void);

/**
 * @brief API to create json response header
 * Based on the input action name, constructed a json header message and returned
 * @param (IN) String hold the sequence id of the request.
 * @param (IN) String hold the request action name
 * @return json response.
 */
static json_object *prepare_json_response_header(const char *action_name, const char *req_id);

int json_hal_server_init(const char *hal_conf_path)
{
    POINTER_ASSERT (hal_conf_path != NULL);

    /* Initialisation. */
    int ret_code = RETURN_OK;

    /**
     * Parse the configuration file and retrieve the required
     * configuration for server.
     */
    ret_code = json_hal_load_config(hal_conf_path, &g_server_config);
    if (ret_code != RETURN_OK)
    {
        LOGERROR ("Failed to load server configurations \n");
        return ret_code;
    }

    /**
     * Initialise g_rpc_server global object for the server socket
     * connection management.
     */
    memset(&g_rpc_server, 0, sizeof(g_rpc_server));
    g_rpc_server.port = g_server_config.server_port_number;
    g_rpc_server.running = FALSE;

    /* Callback initialisation. */
    g_rpc_server.func_connect = (void *)client_connected_cb;
    g_rpc_server.func_process = (void *)message_process_cb;
    g_rpc_server.func_disconnect = (void *)client_disconnected_cb;

#ifdef JSON_SCHEMA_VALIDATION_ENABLED
    ret_code = json_validator_init(g_server_config.hal_schema_path);
#endif

    return ret_code;
}

int json_hal_server_run()
{
    int rc = RETURN_OK;
    rc = json_rpc_server_run(&g_rpc_server);
    if (rc != RETURN_OK)
    {
        LOGERROR("Failed to start json rpc server \n");
    }
    return rc;
}

static int client_connected_cb(int fd)
{
    LOGINFO("Client connection established on fd [%d]", fd);
    return RETURN_OK;
}

/**
 * @brief callback function being invoked when a new client
 * disconnected to server.
 * @param fd of the connected client.
 */
static int client_disconnected_cb(int fd)
{
    LOGINFO("Client connection disconnected");
    remove_event_subscription_from_list(fd);
    return RETURN_OK;
}

static int message_process_cb(int fd, char *buffer, int len)
{

    if (NULL == buffer)
    {
        LOGERROR("Invalid buffer data received\n");
        return RETURN_ERR;
    }

    json_object *returnObj = NULL;
    action_callback_list_t *rpc = NULL;
    char req_id[BUF_64] = {'\0'};
    char action_name[BUF_64] = {'\0'};
    int ret_code = RETURN_OK;
    json_object *jreply_msg = NULL;
    int depth = JSON_TOKENER_DEFAULT_DEPTH;
    json_tokener* tok = NULL;
    json_object* jobj = NULL;

    tok = json_tokener_new_ex(depth);
    if (!tok)
    {
        LOGERROR("Unable to allocate json_tokener: %s", strerror(errno));
        return RETURN_ERR;
    }

    json_tokener_set_flags(tok, JSON_TOKENER_STRICT
#ifdef JSON_TOKENER_ALLOW_TRAILING_CHARS
        | JSON_TOKENER_ALLOW_TRAILING_CHARS
#endif
    );

    for(int start_pos = 0; start_pos < len; start_pos += json_tokener_get_parse_end(tok))
    {
        jobj = json_tokener_parse_ex(tok, &buffer[start_pos], len - start_pos);
        enum json_tokener_error jerr = json_tokener_get_error(tok);
        int parse_end = json_tokener_get_parse_end(tok);
        if (jobj == NULL && jerr != json_tokener_continue)
        {
            char* aterr = (start_pos + parse_end < len)
                ? &buffer[start_pos + parse_end]
                : "";
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
            if (!json_object_object_get_ex(jobj, JSON_RPC_FIELD_ID, &returnObj))
            {
                LOGERROR("Json request doesn't have sequence number/id.");
                json_object_put(jobj);
                continue;
            }
            /* Got reqid. */
            strncpy(req_id, json_object_get_string(returnObj), sizeof(req_id));

            /* Retrieve action name. */
            if (!json_object_object_get_ex(jobj, JSON_RPC_FIELD_ACTION, &returnObj))
            {
                LOGERROR("Json request doesn't contain the action field");
                json_object_put(jobj);
                continue;
            }
            /* Got action name. */
            strncpy(action_name, json_object_get_string(returnObj), sizeof(action_name));

            /* Retrieve any callback regsitered from this action. */
            rpc = get_registered_rpc_action_by_name(action_name);
            if (rpc == NULL)
            {
                
#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
                if (strncmp(action_name, JSON_RPC_ACTION_RESULT, strlen(JSON_RPC_ACTION_RESULT)) == 0)
                {
                    /**
                     * Case-1: Action Result.
                     * In this case, we check the reqid and update the request status.
                     */
                    event_subscriptions_list_t event_subs;
                    event_subscriptions_list_t *subs = NULL;
                    memset(&event_subs, 0, sizeof(event_subs));
                    ret_code = get_event_reply_data_from_msg(jobj, &event_subs);
                    if (ret_code != RETURN_OK)
                    {
                        LOGERROR("Failed to get event data from request message ");
                        json_object_put(jobj);
                        continue;
                    }

                    pthread_mutex_lock(&gm_subscription_mutex);

                    //Check all events subscribed
                    LL_FOREACH(g_event_subscriptions_list, subs)
                    {
                        if (!strncmp(subs->event, event_subs.event, strlen(event_subs.event)))
                        {
                            if(subs->fd == fd)
                            {
                                if (!strncmp(subs->last_msg.req_id, event_subs.last_msg.req_id, strlen(event_subs.last_msg.req_id)))
                                {
                                    subs->last_msg.status = event_subs.last_msg.status;
                                }
                            }
                        }
                    }
                    pthread_mutex_unlock(&gm_subscription_mutex);

                }
                else
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT
                {
                    /**
                     * Case-2: No supported method registered for requested action.
                     * In this case, reply with NotSupported response.
                     */
                    LOGINFO("METHOD %s not supported\n", action_name);

                    /* Sending missing/unsupported API reply to client. */
                    json_object *jreply = create_json_reply_msg(req_id, RESPONSE_NOT_SUPPORTED);
                    if (socket_send(fd, jreply) != RETURN_OK)
                    {
                        LOGERROR("Failed to send the data to client \n");
                    }
                    json_object_put(jreply);
                }
                
                json_object_put(jobj);
                
            }
            else
            {
                /**
                 * Case-3: Found registered RPC handler for
                 * requested action.
                 */
                if (rpc->cb != NULL)
                {
                    int cb_rc = RETURN_OK;
                    /**
                     * In the JSON request message, parametes contains as array of objects.
                     * So length of the array indicates number of params in the request. Check
                     * JSON request has `params` array of request, if so calculate its length.
                     * This can pass as an argument to regsitered callback, so user can know
                     * number of arguments in the request has.
                     */
                    json_object *temp_jobject = NULL;
                    int req_param_count = 0;
                    if (json_object_object_get_ex(jobj, JSON_RPC_FIELD_PARAMS, &temp_jobject))
                    {
                        req_param_count = json_object_array_length(temp_jobject);
        #ifdef DEBUG_ENABLED
                        LOGINFO("Request parameter count = %d \n", req_param_count);
        #endif
                    }

                    jreply_msg = prepare_json_response_header(action_name, req_id);
                    /**
                     * Callback routine invoked.
                     */
                    cb_rc = rpc->cb(jobj, req_param_count, jreply_msg);
                    if (cb_rc == RETURN_OK)
                    {
        /**
                         * Validate the Json response message against the schema.
                         * If the response is not valid as per schema, we are sending back a
                         * Not Supported response to the client application.
                         */
#ifdef JSON_SCHEMA_VALIDATION_ENABLED
                        const char *reply_msg_str = json_object_to_json_string_ext(jreply_msg, JSON_C_TO_STRING_PRETTY);
                        if (json_validator_validate_request(reply_msg_str) != RETURN_OK)
                        {
                            LOGERROR("Invalid JSON response, not validated against schema \n");
                            /* Sending unsupported reply to client. */
                            json_object *jreply_msg_not_supported = create_json_reply_msg(req_id, RESPONSE_NOT_SUPPORTED);
                            if (socket_send(fd, jreply_msg_not_supported) != RETURN_OK)
                            {
                                LOGERROR("Failed to send the data to client \n");
                            }
                            json_object_put(jreply_msg); /* Frees json buffer will release string buffer too. */
                            json_object_put(jreply_msg_not_supported);
                            json_object_put(jobj);
                            continue;
                        }
#endif
                        /* Send response message to client. */
                        if (socket_send(fd, jreply_msg) != RETURN_OK)
                        {
                            LOGERROR("Failed to send response back to client");
                        }
                        /* Free the reply message object. */
                        json_object_put(jreply_msg);
                    }
                    else /* Callback failed to execute. */
                    {
                        LOGERROR("Callback failed to execute the request");
                        /**
                         * Notify failed response back to requester.
                         */
                        json_object *jfailure_msg = create_json_reply_msg(req_id, RESPONSE_FAILURE);
                        if (socket_send(fd, jfailure_msg) != RETURN_OK)
                        {
                            LOGERROR("Failed to send the data to client \n");
                        }
                        json_object_put(jfailure_msg);
                        json_object_put(jobj);
                        json_object_put(jreply_msg);
                        continue;
                    }

                    /**
                     * In case of event subscription request, we have to update the
                     * global list to store the subscribed client's details.
                     * So update event subscription global list and invoke cb to notify
                     * vendor software about subscription.
                     */
                    if (strncmp(action_name, JSON_RPC_SUBSCRIBE_EVENT_ACTION_NAME, strlen(JSON_RPC_SUBSCRIBE_EVENT_ACTION_NAME)) == 0)
                    {
                        event_subscriptions_list_t event_subs;
                        memset(&event_subs, 0, sizeof(event_subs));

                        ret_code = initialise_event_subscription_data(&event_subs);
                        if (ret_code != RETURN_OK)
                        {
                            LOGERROR("Failed to initialise event data");
                            json_object_put(jobj);
                            return RETURN_ERR;
                        }
                        
                        ret_code = get_event_subscription_data_from_msg(jobj, &event_subs);
                        if (ret_code != RETURN_OK)
                        {
                            LOGERROR("Failed to get event data from request message ");
                            json_object_put(jobj);
                            break;
                        }
                        
                        event_subs.fd = fd;
                        add_event_subscription_to_list(&event_subs);
                    }
                    json_object_put(jobj);
                }
                else
                {
                    /**
                    * Case-3: No handler for supported method for requested action.
                    * In this case, reply with NotSupported response.
                    */
                    LOGINFO("METHOD %s handler not found\n", action_name);

                    /* Sending missing/unsupported API reply to client. */
                    json_object *jreply = create_json_reply_msg(req_id, RESPONSE_NOT_SUPPORTED);
                    if (socket_send(fd, jreply) != RETURN_OK)
                    {
                        LOGERROR("Failed to send the data to client \n");
                    }
                    json_object_put(jreply);
                    json_object_put(jobj);
                }
            }
        } /* End of jobj != NULL */
    } /* End of for */

    json_tokener_free(tok);

    return RETURN_OK;
}

/**
 * @brief Update event details into global event subscription list.
 * @param Pointer to event_subscriptions_list_t struct which contains event data
 */
static void add_event_subscription_to_list(event_subscriptions_list_t *event_subs_data)
{
    POINTER_ASSERT_V(event_subs_data != NULL);
    event_subscriptions_list_t *subs = NULL;
    subs = (event_subscriptions_list_t *)calloc(1, sizeof(event_subscriptions_list_t));
    POINTER_ASSERT_V(subs != NULL);
    subs->fd = event_subs_data->fd;
    strcpy(subs->event, event_subs_data->event);
    subs->event_type = event_subs_data->event_type;
    strcpy(subs->last_msg.req_id, event_subs_data->last_msg.req_id);
    subs->last_msg.status = event_subs_data->last_msg.status;
    pthread_mutex_lock(&gm_subscription_mutex);
    LL_APPEND(g_event_subscriptions_list, subs);
    pthread_mutex_unlock(&gm_subscription_mutex);
}

/**
 * @brief Remove event subscription data from global subscription list.
 * @param client fd
 */
static void remove_event_subscription_from_list(int fd)
{
    event_subscriptions_list_t *tmp, *subs;
    pthread_mutex_lock(&gm_subscription_mutex);
    LL_FOREACH_SAFE(g_event_subscriptions_list, subs, tmp)
    {
        if (subs->fd == fd)
        {
            LL_DELETE(g_event_subscriptions_list, subs);
            free(subs);
        }
    }
    pthread_mutex_unlock(&gm_subscription_mutex);
}

int json_hal_server_register_action_callback(const char *action_name, const action_callback callback)
{
    /* Check function already regsistered. */
    action_callback_list_t *temp = NULL;
    LL_FOREACH(g_hal_functions_list, temp)
    {
        if (!strcmp(temp->function_name, action_name))
        {
            LOGINFO("[%s] already registered, no need to reregister\n", action_name);
            return RETURN_ERR;
        }
    }

    action_callback_list_t *rpc = NULL;
    rpc = (action_callback_list_t *)malloc(sizeof(action_callback_list_t));
    POINTER_ASSERT(rpc != NULL);

    strcpy(rpc->function_name, action_name);
    rpc->cb = callback;
    LL_APPEND(g_hal_functions_list, rpc);
    return RETURN_OK;
}

static action_callback_list_t *get_registered_rpc_action_by_name(char *func_name)
{
    action_callback_list_t *rpc = NULL;
    LL_FOREACH(g_hal_functions_list, rpc)
    {
        if (!strcmp(rpc->function_name, func_name))
            return rpc;
    }
    return NULL;
}


#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
static int wait_publish_event_reply(const char *event_name)
{
    int ret = RETURN_OK;
    event_subscriptions_list_t *subs = NULL;
    bool waiting_event_reply = TRUE;
    bool msg_timeout = FALSE;

    //starting timer
    struct timespec initial_time;
    clock_gettime(CLOCK_MONOTONIC, &initial_time);

    while(waiting_event_reply && !msg_timeout)
    {
        pthread_mutex_lock(&gm_subscription_mutex);

        waiting_event_reply = FALSE;
        msg_timeout = TRUE;

        LL_FOREACH(g_event_subscriptions_list, subs)
        {
            if (!strncmp(subs->event, event_name, strlen(event_name)))
            {
                //if event is blocking
                if((subs->event_type == NOTIFICATION_TYPE_ON_CHANGE_SYNC_TIMEOUT)
                || (subs->event_type == NOTIFICATION_TYPE_ON_CHANGE_SYNC))
                {
                    if(subs->last_msg.status == WAIT_EVENT_REPLY_MSG)
                    {
                        waiting_event_reply = TRUE;

                        //if event has no timeout
                        if(subs->event_type == NOTIFICATION_TYPE_ON_CHANGE_SYNC)
                        {
                            msg_timeout = FALSE;
                        }
                        else
                        {
                            struct timespec current_time;
                            clock_gettime(CLOCK_MONOTONIC, &current_time);
                            if((current_time.tv_sec - initial_time.tv_sec) <= SEND_EVENT_SUBSCRIPTION_TIMEOUT)
                            {
                                msg_timeout = FALSE;
                            }
                        }
                    }
                    else if(subs->last_msg.status != EVENT_REPLY_SUCCESS)
                    {
                        LOGERROR("Failed to receive the event reply message \n");
                        ret = RETURN_ERR;
                    }
                }
            }
        }

        pthread_mutex_unlock(&gm_subscription_mutex);


        usleep(SEND_EVENT_SUBSCRIPTION_LOOP_TIME);
    }

    //in case of timeout
    if(waiting_event_reply && msg_timeout)
    {
        LOGERROR("JSON Hal Server wait timed out\n");
        ret = RETURN_ERR;
    }
    
    return ret;
}
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT

int json_hal_server_publish_event(const char *event_name, const char *event_value)
{
    int ret = RETURN_OK;
    event_subscriptions_list_t *subs = NULL;
#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
    bool publish_event_blocking = FALSE;
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT
    
    pthread_mutex_lock(&gm_subscription_mutex);

    LL_FOREACH(g_event_subscriptions_list, subs)
    {
        if (!strncmp(subs->event, event_name, strlen(event_name)))
        {
            LOGINFO("Find registered client for event");
            json_object *jevent_msg = create_publish_event_msg(event_name, event_value);
            POINTER_ASSERT(jevent_msg != NULL);

#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
            if((subs->event_type == NOTIFICATION_TYPE_ON_CHANGE_SYNC_TIMEOUT)
            || (subs->event_type == NOTIFICATION_TYPE_ON_CHANGE_SYNC))
            {
                publish_event_blocking = TRUE;
                
                json_object *returnObj = NULL;
                if (!json_object_object_get_ex(jevent_msg, JSON_RPC_FIELD_ID, &returnObj))
                {
                    LOGERROR("Json request doesn't have sequence number/id.");
                }
                /* Got reqid. */
                strncpy(subs->last_msg.req_id, json_object_get_string(returnObj), sizeof(subs->last_msg.req_id));
                subs->last_msg.status = WAIT_EVENT_REPLY_MSG;
            }
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT

            /* Send message to client. */
            if (socket_send(subs->fd, jevent_msg) != RETURN_OK)
            {
                LOGERROR("Failed to send the data to client \n");
                ret = RETURN_ERR;
#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
                publish_event_blocking = FALSE;
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT
            }

            json_object_put(jevent_msg); // Free json object. Its freed buffer memory too.
        }
    }

    pthread_mutex_unlock(&gm_subscription_mutex);


#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
    //Check the answer
    if(publish_event_blocking)
    {
        wait_publish_event_reply(event_name);
    }
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT


    return ret;
}

/**
 * @brief API is create a json response to send to the client for
 * Not Supported RPC request.
 * @param req_id (IN) String holds the request's request id
 * @return json response.
 */
static json_object *create_json_reply_msg(const char *req_id, const response_msg_type_t type)
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

    json_object *jreply = json_object_new_object();
    json_object_object_add(jreply, JSON_RPC_FIELD_MODULE, json_object_new_string(g_server_config.hal_module_name));
    json_object_object_add(jreply, JSON_RPC_FIELD_VERSION, json_object_new_string(g_server_config.hal_module_version));
    json_object_object_add(jreply, JSON_RPC_FIELD_ACTION, json_object_new_string(JSON_RPC_ACTION_RESULT));
    json_object_object_add(jreply, JSON_RPC_FIELD_ID, json_object_new_string(req_id));
    json_object_object_add(jreply, JSON_RPC_FILED_RESULT, jparamval);

    return jreply;
}

int json_hal_server_terminate()
{

    /* Stop server socket thread and close all the current connections. */
    g_rpc_server.running = FALSE;

    /**
     * Make sure server thread stopped and closed all client sockets.
     * Timeout after 4 seconds if server is not terminated.
     */
    int counter = IDLE_TIMEOUT_PERIOD;
    do
    {
        if (json_rpc_server_is_running() == FALSE)
        {
            LOGINFO("Server socket thread terminated gracefully");
            break;
        }

        usleep(IDLE_TIMEOUT_PERIOD);
        counter--;
    } while (counter > 0);

    /* Free the global lists for the rpc registered functions and event subscriptions. */
    action_callback_list_t *tmp, *rpc;
    LL_FOREACH_SAFE(g_hal_functions_list, rpc, tmp)
    {
        LL_DELETE(g_hal_functions_list, rpc);
        free(rpc);
    }

    if (g_hal_functions_list != NULL)
    {
        free(g_hal_functions_list);
        g_hal_functions_list = NULL;
    }

    /* Delete event subscription list. */
    event_subscriptions_list_t *tmp_event, *rpc_event;
    LL_FOREACH_SAFE(g_event_subscriptions_list, rpc_event, tmp_event)
    {
        LL_DELETE(g_event_subscriptions_list, rpc_event);
        free(rpc_event);
    }

    if (g_event_subscriptions_list != NULL)
    {
        free(g_event_subscriptions_list);
        g_event_subscriptions_list = NULL;
    }
#ifdef JSON_SCHEMA_VALIDATION_ENABLED
    /* Terminate validator object. */
    json_validator_terminate();
#endif // JSON_SCHEMA_VALIDATION_ENABLED

    return RETURN_OK;
}



static int initialise_event_subscription_data(event_subscriptions_list_t *event_subs_data)
{
    POINTER_ASSERT(event_subs_data != NULL);

    memset(event_subs_data, 0, sizeof(event_subscriptions_list_t));

    event_subs_data->event_type = NOTIFICATION_TYPE_INVALID;
    event_subs_data->last_msg.status = NO_EVENT_MSG;

    return RETURN_OK;
}

/**
 * Parse and get event subscription data.
 */
static int get_event_subscription_data_from_msg(const json_object *jmsg, event_subscriptions_list_t *rpc_event_subs_data)
{
    POINTER_ASSERT(jmsg != NULL);
    POINTER_ASSERT(rpc_event_subs_data != NULL);

    int ret = RETURN_OK;
    hal_subscribe_event_request_t req_param;

    ret = json_hal_get_subscribe_event_request(jmsg, JSON_RPC_PARAM_ARR_INDEX, &req_param);
    if(ret == RETURN_OK)
    {
        strncpy(rpc_event_subs_data->event, req_param.name, sizeof(rpc_event_subs_data->event));
        rpc_event_subs_data->event_type = req_param.type;
    }
    else
    {
        LOGERROR("Json request doesn't contain the params field");
        json_object_put(jmsg);
        return RETURN_ERR;
    }

    return RETURN_OK;
}


#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
/**
 * Parse and get event subscription data.
 */
static int get_event_reply_data_from_msg(const json_object *jmsg, event_subscriptions_list_t *rpc_event_subs_data)
{
    POINTER_ASSERT(jmsg != NULL);
    POINTER_ASSERT(rpc_event_subs_data != NULL);

    json_object *jreturnObj = NULL;
    if (json_object_object_get_ex(jmsg, JSON_RPC_FILED_RESULT, &jreturnObj))
    {
        //RESULT
        json_object *jresultObj = NULL;
        if (json_object_object_get_ex(jreturnObj, JSON_RPC_PARAM_STATUS_FIELD, &jresultObj))
        {
            const char *result = json_object_get_string(jresultObj);
            if (strncmp(result, JSON_RPC_STATUS_SUCCESS, strlen(JSON_RPC_STATUS_SUCCESS)) == 0)
            {
                rpc_event_subs_data->last_msg.status = EVENT_REPLY_SUCCESS;
            }
            else
            {
                rpc_event_subs_data->last_msg.status = EVENT_REPLY_ERROR;
            }
        }
        else
        {
            LOGERROR("Failed to get Status param from json response message");
            json_object_put(jmsg);
            return RETURN_ERR;
        }

        //NAME
        if (json_object_object_get_ex(jmsg, JSON_RPC_FIELD_PARAMS, &jreturnObj))
        {
            json_object *jresponse_params = json_object_array_get_idx(jreturnObj, JSON_RPC_PARAM_ARR_INDEX);
            json_object *jresponse_param_value = NULL;




            if (json_object_object_get_ex(jresponse_params, JSON_RPC_FIELD_PARAM_NAME, &jresponse_param_value))
            {
                strncpy(rpc_event_subs_data->event, json_object_get_string(jresponse_param_value), sizeof(rpc_event_subs_data->event));
            }
            else
            {
                LOGERROR("Json request doesn't contain the event name field");
                json_object_put(jmsg);
                return RETURN_ERR;
            }
        }

        //REQID
        if (json_object_object_get_ex(jmsg, JSON_RPC_FIELD_ID, &jreturnObj))
        {
            strncpy(rpc_event_subs_data->last_msg.req_id, json_object_get_string(jreturnObj), sizeof(rpc_event_subs_data->last_msg.req_id));
        }
        else
        {
            LOGERROR("Json request doesn't contain the reqid field");
            json_object_put(jmsg);
            return RETURN_ERR;
        }

    }
    else
    {
        LOGERROR("Json request doesn't contain the params field");
        json_object_put(jmsg);
        return RETURN_ERR;
    }

    return RETURN_OK;
}
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT

/**
 * Event message creation.
*/
static json_object *create_publish_event_msg(const char *event_name, const char *event_val)
{
    if (event_name == NULL || event_val == NULL)
    {
        LOGERROR("Invalid argument");
        return NULL;
    }

    json_object *jmsg = json_object_new_object();
    json_object_object_add(jmsg, JSON_RPC_FIELD_MODULE, json_object_new_string(g_server_config.hal_module_name));
    json_object_object_add(jmsg, JSON_RPC_FIELD_VERSION, json_object_new_string(g_server_config.hal_module_version));
    json_object_object_add(jmsg, JSON_RPC_FIELD_ACTION, json_object_new_string(JSON_RPC_PUBLISH_EVENT_ACTION_NAME));
    json_object_object_add(jmsg, JSON_RPC_FIELD_ID, json_object_new_int(get_sequence_number()));

    json_object *jarr = json_object_new_array();
    json_object *jparam = json_object_new_object();
    json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_NAME, json_object_new_string(event_name));
    json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_string(event_val));
    json_object_array_add(jarr, jparam);
    json_object_object_add(jmsg, JSON_RPC_FIELD_PARAMS, jarr);

    return jmsg;
}

static int socket_send(const int sockfd, const json_object *jmsg)
{
    char *response_msg_buffer = NULL;
    int rc = RETURN_OK;
    POINTER_ASSERT(jmsg != NULL);
    response_msg_buffer = json_object_to_json_string_ext(jmsg, JSON_C_TO_STRING_PRETTY);
    POINTER_ASSERT(response_msg_buffer != NULL);

    rc = json_rpc_server_send_data(sockfd, response_msg_buffer);
    if (rc != RETURN_OK)
    {
        LOGERROR("Failed to send the response back to client");
    }
    return rc;
}

unsigned int get_sequence_number(void)
{
    g_seqnumber++;
    if (g_seqnumber > INT_MAX)
        g_seqnumber = DEFAULT_SEQ_START_NUMBER;

    return g_seqnumber;
}

static json_object *prepare_json_response_header(const char *action_name, const char *req_id)
{

    if (action_name == NULL || req_id == NULL)
    {
        LOGERROR ("Invalid argument \n");
        return NULL;
    }
    /**
     *
     * Here we have three cases:
     *  Case-1: getParameter Request - In this case, we have to pass json object having header as well as `params` empty array object, so vendor can update the array object.
     *  Case-2: getSchema Request - In this case, we have to pass json object having header as well as  `SchemaInfo` json object.
     *  Case-3: All the other actions - In this case, we have to pass json object having header as well as `Result` json object, so vendor can update Result object.
     */
    json_object *jmsg = json_object_new_object();
    json_object_object_add(jmsg, JSON_RPC_FIELD_MODULE, json_object_new_string(g_server_config.hal_module_name));
    json_object_object_add(jmsg, JSON_RPC_FIELD_VERSION, json_object_new_string(g_server_config.hal_module_version));
    json_object_object_add(jmsg, JSON_RPC_FIELD_ID, json_object_new_string(req_id));
    if (strncmp(action_name, JSON_RPC_ACTION_GET_PARAM, strlen(JSON_RPC_ACTION_GET_PARAM)) == 0)
    {
        json_object_object_add(jmsg, JSON_RPC_FIELD_ACTION, json_object_new_string(JSON_RPC_ACTION_GET_PARAM_RESPONSE));
        /* Create params array and embed into the json_object. */
        json_object *jparam_array = json_object_new_array();
        json_object_object_add(jmsg, JSON_RPC_FIELD_PARAMS, jparam_array);
    }
    else if (strncmp(action_name, JSON_RPC_ACTION_GET_SCHEMA, strlen(JSON_RPC_ACTION_GET_SCHEMA)) == 0)
    {
        json_object_object_add(jmsg, JSON_RPC_FIELD_ACTION, json_object_new_string(JSON_RPC_ACTION_GET_SCHEMA_RESPONSE));
        /* Create sub json_object `Result` to strore result status. */
        json_object *jresult_message = json_object_new_object();
        json_object_object_add(jmsg, JSON_RPC_FIELD_SCHEMA_INFO, jresult_message);
    }
    else
    {
        /* For result response. */
        json_object_object_add(jmsg, JSON_RPC_FIELD_ACTION, json_object_new_string(JSON_RPC_ACTION_RESULT));
        /* Create sub json_object `SchemaInfo` to strore status. */
        json_object *jresult_message = json_object_new_object();
        json_object_object_add(jmsg, JSON_RPC_FILED_RESULT, jresult_message);
    }
    return jmsg;
}

int json_hal_add_result_status(json_object *jreply, eResult_t result)
{
    if(jreply == NULL ) {
        return RETURN_ERR;
    }

    json_object *jresult = json_object_new_object();
    switch(result)
    {
        case RESULT_SUCCESS:
            json_object_object_add(jresult, JSON_RPC_PARAM_STATUS_FIELD, json_object_new_string(JSON_RPC_STATUS_SUCCESS));
            break;
        case RESULT_FAILURE:
            json_object_object_add(jresult, JSON_RPC_PARAM_STATUS_FIELD, json_object_new_string(JSON_RPC_STATUS_FAILED));
            break;
    }
    json_object_object_add(jreply, JSON_RPC_FILED_RESULT, jresult);

    return RETURN_OK;
}

int json_hal_add_schema_response(json_object *jreply, hal_schema_response_t *param)
{
    json_object *jschemainfo = NULL;

    if(jreply == NULL || param == NULL) {
        return RETURN_ERR;
    }

    if(json_object_object_get_ex(jreply, JSON_RPC_FIELD_SCHEMA_INFO, &jschemainfo))
    {
        json_object_object_add(jschemainfo, JSON_RPC_FIELD_SCHEMA_FILEPATH, json_object_new_string(param->filepath));
    }
    else
    {
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int json_hal_get_subscribe_event_request(json_object *jmsg, int index, hal_subscribe_event_request_t *param)
{
    json_object *jparams = NULL;
    json_object *jparam = NULL;
    json_object *jparamobject = NULL;

    if(jmsg == NULL || param == NULL) {
        return RETURN_ERR;
    }

    memset(param, 0, sizeof(hal_subscribe_event_request_t));

    if (json_object_object_get_ex(jmsg, JSON_RPC_FIELD_PARAMS, &jparams))
    {
        printf("params field found in the json request \n");
    }
    else
    {
        return RETURN_ERR;
    }

    jparam = json_object_array_get_idx(jparams, index);
    if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_NAME, &jparamobject))
    {
        strncpy(param->name, json_object_get_string(jparamobject), sizeof(param->name));
    }
    else
    {
        return RETURN_ERR;
    }
    if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_NOTIFICATION_TYPE, &jparamobject))
    {
        
#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
        const char *notification_type = json_object_get_string(jparamobject);
        if(!strncmp(notification_type, JSON_RPC_FIELD_PARAM_NOTIFICATION_TYPE_ON_CHANGE, BUF_512))
        {
            param->type = NOTIFICATION_TYPE_ON_CHANGE;
        }
        else if(!strncmp(notification_type, JSON_RPC_FIELD_PARAM_NOTIFICATION_TYPE_ON_CHANGE_SYNC, BUF_512))
        {
            param->type = NOTIFICATION_TYPE_ON_CHANGE_SYNC;
        }
        else if(!strncmp(notification_type, JSON_RPC_FIELD_PARAM_NOTIFICATION_TYPE_ON_CHANGE_SYNC_TIMEOUT, BUF_512))
        {
            param->type = NOTIFICATION_TYPE_ON_CHANGE_SYNC_TIMEOUT;
        }
        else
        {
            param->type = NOTIFICATION_TYPE_ON_CHANGE;
            LOGERROR("Unkwon notification type '%s'. Using OnChange notification.", notification_type);
        }
        
#else
        param->type = NOTIFICATION_TYPE_ON_CHANGE;
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT
        
    }
    else
    {
        return RETURN_ERR;
    }

    return RETURN_OK;
}
