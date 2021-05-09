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

/**
 * Json RPC test server application.
 */

#include <stdio.h>
#include "json_hal_server.h"
#include <pthread.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "json_rpc_common.h"
#define DSL_LINK_EVENT "Device.DSL.Line.1.LinkStatus"
#define ETH_LINK_EVENT "Device.Ethernet.Interface.1.LinkStatus"
#define LINK_UP "Up"
#define LINK_DOWN "Down"

bool evt_thread_started = FALSE;
int evt_subs_index = -1;
char schemaPath[256] = {'\0'};

/**
 * @brief global list to store event susbscriptions.
 */
static hal_subscribe_event_request_t gsubs_event_list[5];

/**
 * Thread - Simulate event dispatch in specific intervals.
 */
static void *rpc_event_dispatch_thread(void *paramPtr);

/**
 * RPC callback handler - RPC method registered with Server for `setParameters` action.
 */
static int setparam_rpc_cb(const json_object *inJson, int param_count, json_object *jreply);

/**
 * RPC callback handler - RPC method registered with Server for `getParameters` action.
 */
static int getparam_rpc_cb(const json_object *inJson, int param_count, json_object *jreply);

/**
 * RPC callback handler - Event subsctiption.
 */
static int subs_event_cb(const json_object *inJson, int param_count, json_object *jreply);

/**
 * RPC callback handler - RPC method registered with Server for `deleteObject` action.
 */
static int deleteobj_rpc_cb(const json_object *inJson, int param_count, json_object *jreply);


/**
 * Signal handler.
 * Do cleanup and exit.
 */
static void sig_handler(int signo)
{
    if (signo == SIGUSR1 || signo == SIGKILL || signo == SIGINT)
    {
        int rc = RETURN_OK;
        rc = json_hal_server_terminate();
        assert(rc == RETURN_OK);
        LOGINFO("Cleanup done, exiting \n");
        exit(0);
    }
}

/**
 * Start event dispatch thread.
 */
static int start_event_dispatch_thread()
{
    int rc;
    pthread_t socket_thread;
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    rc = pthread_create(&socket_thread, &attributes, rpc_event_dispatch_thread, NULL);
    pthread_attr_destroy(&attributes);
    evt_thread_started = TRUE;
    return rc;
}

/**
 * Event Dispatching thread,
 * Thread creates an event message and sending every 30 seconds
 */
static void *rpc_event_dispatch_thread(void *paramPtr)
{

    pthread_detach(pthread_self());
    while (1)
    {
        /**
         * Here we are demonstrating multiple events simultaneously to client.
         * Client should able to get both pf these link events.
         */
        for (int i = 0; i < 2; ++i) {
            if (strcmp(gsubs_event_list[i].name, DSL_LINK_EVENT) == 0) {
                json_hal_server_publish_event(DSL_LINK_EVENT, LINK_UP);
            }else if (strcmp(gsubs_event_list[i].name, ETH_LINK_EVENT) == 0) {
                json_hal_server_publish_event(ETH_LINK_EVENT, LINK_DOWN);
            }
        }

        /** Wait for sometime for sending the next set of events. */
        sleep(10);
    }

    return NULL;
}

int main(int argc, char **argv)
{

    if (argc != 2)
    {
        LOGINFO("Usage: test_json_rpc_srv <configuration file> \n");
        exit(0);
    }
    int rc = RETURN_ERR;

    /**
     * RPC Server Initialisation.
     * Entry point for the server. Initialise the server software.
     */
    rc = json_hal_server_init(argv[1]);
    assert(rc == RETURN_OK);
    LOGINFO("Test case - Server init successful  \n");

    /**
     *  Register RPC methods and its callback function.
     *  Register all the supported methods and its action callback with rpc server library.
     */
    json_hal_server_register_action_callback("setParameters", setparam_rpc_cb);
    json_hal_server_register_action_callback("getParameters", getparam_rpc_cb);
    json_hal_server_register_action_callback("deleteObject", deleteobj_rpc_cb);
    json_hal_server_register_action_callback("subscribeEvent", subs_event_cb);

    /**
     *  Started the server socket.
     *  Server socket created and listen for the client connections at this point.
     **/
    rc = json_hal_server_run();
    assert(rc == RETURN_OK);

    /* Signal Handling. */
    signal(SIGINT, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGUSR1, sig_handler);

    while (1)
    {
        sleep(1);
    }
    return 0;
}

/**
 * RPC callback function. setParameters()
 * This callback invoked when a client request received and it has the action name matched with registered methods.
 **/
static int setparam_rpc_cb(const json_object *jmsg, int param_count, json_object *jreply)
{
    int index = 0;
    hal_param_t param;

    if (jmsg == NULL || jreply == NULL)
    {
        LOGINFO("Invalid memory \n");
        return RETURN_ERR;
    }

    for(index = 0; index <param_count; index++)
    {
        /* Unpack the JSON and polulate the data into request_param object */
        if( json_hal_get_param(jmsg, index, SET_REQUEST_MESSAGE, &param) != RETURN_OK)
        {
            return RETURN_ERR;
        }
        /**
         * The param object can be parsed to get the datamodel name, its type and
         * the value to be set.
         */

        /**
         * Once unpacking done, do the platform side actions.
         * Do all the lower layer driver calls.
         */

        /*
         * Response back with success/failure.
         * Pack the json response and reply back.
         */
        if (json_hal_add_result_status(jreply, RESULT_SUCCESS) != RETURN_OK)
        {
             return RETURN_ERR;
        }
    }

    LOGINFO("[%s] json reply msg = %s \n", __FUNCTION__, json_object_to_json_string_ext(jreply, JSON_C_TO_STRING_PRETTY));
    return RETURN_OK;
}

/**
 * RPC callback function. getParameters()
 * This callback invoked when a client request received and it has the action name matched with registered methods.
 **/
static int getparam_rpc_cb(const json_object *jmsg, int param_count, json_object *jreply)
{
    int index = 0;
    hal_param_t param_request;
    hal_param_t param_response;

    if (jmsg == NULL || jreply == NULL)
    {
        LOGINFO("Invalid memory \n");
        return RETURN_ERR;
    }

    for(index = 0; index <param_count; index++)
    {
        /* Unpack the JSON and polulate the data into request_param object */
        if( json_hal_get_param(jmsg, index, GET_REQUEST_MESSAGE, &param_request) != RETURN_OK)
        {
            return RETURN_ERR;
        }

        /**
         * Once unpacking done, do the platform side actions.
         * Do all the lower layer driver calls.
        */

        /* Populating param_response object with sample data */
        strncpy(param_response.name, "Device.DSL.Line.1.Enable", sizeof(param_response.name));
        param_response.type = PARAM_BOOLEAN;
        strncpy(param_response.value, "true", sizeof(param_response.value));
        if( json_hal_add_param(jreply, GET_RESPONSE_MESSAGE, &param_response) != RETURN_OK)
        {
            return RETURN_ERR;
        }
        strncpy(param_response.name, "Device.DSL.Line.1.EnableDataGathering", sizeof(param_response.name));
        param_response.type = PARAM_BOOLEAN;
        strncpy(param_response.value, "true", sizeof(param_response.value));
        if( json_hal_add_param(jreply, GET_RESPONSE_MESSAGE, &param_response) != RETURN_OK)
        {
            return RETURN_ERR;
        }
        strncpy(param_response.name, "Device.DSL.Line.1.Status", sizeof(param_response.name));
        param_response.type = PARAM_STRING;
        strncpy(param_response.value, "Up", sizeof(param_response.value));
        if( json_hal_add_param(jreply, GET_RESPONSE_MESSAGE, &param_response) != RETURN_OK)
        {
            return RETURN_ERR;
        }
        strncpy(param_response.name, "Device.DSL.Line.1.SuccessFailureCause", sizeof(param_response.name));
        param_response.type = PARAM_UNSIGNED_INTEGER;
        strncpy(param_response.value, "0", sizeof(param_response.value));
        if( json_hal_add_param(jreply, GET_RESPONSE_MESSAGE, &param_response) != RETURN_OK)
        {
            return RETURN_ERR;
        }
    }

    LOGINFO("[%s] json reply msg = %s \n", __FUNCTION__, json_object_to_json_string_ext(jreply, JSON_C_TO_STRING_PRETTY));
    return RETURN_OK;
}

/**
 * RPC callback function. deleteObj callback method.
 * This callback invoked when a client request received and it has the action name matched with registered methods.
 **/
static int deleteobj_rpc_cb(const json_object *jmsg, int param_count, json_object *jreply)
{
    int index = 0;
    hal_param_t param;

    if (jmsg == NULL || jreply == NULL)
    {
        LOGINFO("Invalid memory \n");
        return RETURN_ERR;
    }

    for(index = 0; index <param_count; index++)
    {
        /* Unpack the JSON and polulate the data into request_param object */
        if( json_hal_get_param(jmsg, index, DELETE_REQUEST_MESSAGE, &param) != RETURN_OK)
        {
            return RETURN_ERR;
        }

        /**
         * Once unpacking done, do the platform side actions.
         * Do all the lower layer driver calls. The param object 
         * can be parsed to get the name of the object to be deleted
        */

        /*
         * Response back with success/failure.
         * Pack the json response and reply back.
         */
        if (json_hal_add_result_status(jreply, RESULT_SUCCESS) != RETURN_OK)
        {
            return RETURN_ERR;
        }
    }

    LOGINFO("[%s] json reply msg = %s \n", __FUNCTION__, json_object_to_json_string_ext(jreply, JSON_C_TO_STRING_PRETTY));
    return RETURN_OK;
}

static int subs_event_cb(const json_object *jmsg, int param_count, json_object *jreply)
{
    if (jmsg == NULL || jreply == NULL)
    {
        LOGINFO("Invalid memory");
        return RETURN_ERR;
    }

    int ret = RETURN_OK;
    int param_index = 0;
    hal_subscribe_event_request_t param_request;

    memset(&param_request, 0, sizeof(param_request));

    for (param_index = 0; param_index < param_count; param_index++)
    {
        /**
         * Unpack the request parameter and store it into param_request structure.
         */
        if (json_hal_get_subscribe_event_request(jmsg, param_index, &param_request) != RETURN_OK)
        {
            LOGINFO("Failed to get subscription data from the server");
            return RETURN_ERR;
        }

        /**
         * Store subscription data into global list.
         */
        evt_subs_index += 1;
        strncpy(gsubs_event_list[evt_subs_index].name, param_request.name, sizeof(gsubs_event_list[evt_subs_index].name));
        gsubs_event_list[evt_subs_index].type = param_request.type;
        LOGINFO("Subscribed [%s][%d]", gsubs_event_list[evt_subs_index].name, gsubs_event_list[evt_subs_index].type);
    }

    ret = json_hal_add_result_status(jreply, RESULT_SUCCESS);

    LOGINFO("[%s] Replly msg = %s", __FUNCTION__, json_object_to_json_string_ext(jreply, JSON_C_TO_STRING_PRETTY));

    /** 
     * Start a thread to simulate link events to client.
     * Thread needs to start only once.
     */
    if (!evt_thread_started) {
        start_event_dispatch_thread();
    }

    return ret;
}
