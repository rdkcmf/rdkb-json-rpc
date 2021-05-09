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
 * @file json_hal_client_event_test.c
 * @description This is a sample application to test the link event subscription and
 * management.
 *
 * In this test application, we are doing the following demonstration:
 *    - Subscribe for 2 link events.
 *    - Register callback for handling these events.
 *    - Make sure we are getting events in all the cases like server send multiple events simultaneously.
 *
 * Dependency:
 *    - Server test application should run to full fill the requests.
 */

#include <stdio.h>
#include <json-c/json.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <stdint.h>
#include "json_hal_client.h"
#include "json_rpc_common.h"
#include <string.h>
#include <unistd.h>

/**
 * Callback Function handler - Invoked when RPC server issue an event.
 */
static void eventcb(const char *, const int);

/**
 * Utility API to show the usage of application.
 */
static void usage(char *appname);

/**
 * Here we are demonstrate subscription of 2 events.
 *  Device.DSL.Line.1.LinkStaus  -> xDSL link
 *  Device.Ethernet.Interface.1.LinkStatus  -> Ethernet event.
 *
 *  The event names are samples and server simulate these event notifications.
 */
#define DSL_EVENT_PATH "Device.DSL.Line.1.LinkStatus"
#define ETH_EVENT_PATH "Device.Ethernet.Interface.1.LinkStatus"
#define EVENT_INTERVAL "onChange"
#define EVENT_INTERVAL_WITH_SYNC "onChangeSync"

/**
 * APIs to send subscription message.
 */
static int send_subscription_msg ();

static void usage(char *appname)
{
    LOGINFO("usage: %s <configuration file> ", appname);
    LOGINFO(" \t configuration file : json formatted file contains the schema path and server port number");
    LOGINFO(" \t Example: %s /etc/rdk/conf/xdsl_manager_conf.json", appname);
}

static int send_subscription_msg ()
{
    int rc = RETURN_ERR;
    rc = json_hal_client_subscribe_event(eventcb, DSL_EVENT_PATH, EVENT_INTERVAL);
    if (rc != RETURN_OK) {
        LOGINFO ("Failed to subscribe [%s] event", DSL_EVENT_PATH);
    }


#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
    rc = json_hal_client_subscribe_event(eventcb, ETH_EVENT_PATH, EVENT_INTERVAL_WITH_SYNC);
#else
    rc = json_hal_client_subscribe_event(eventcb, ETH_EVENT_PATH, EVENT_INTERVAL);
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT
    if (rc != RETURN_OK) {
        LOGINFO ("Failed to subscribe [%s] event", ETH_EVENT_PATH);
    }

    return rc;
}

/**
 * Event callback function.
 */
static void eventcb(const char *msg, const int len)
{
    assert(msg != NULL);

    LOGINFO("Event callback invoked -> Event Msg = %s", __FUNCTION__, __LINE__, msg);

    /* Parse message and find event received. */
    json_object *jobj = json_tokener_parse(msg);
    assert(jobj != NULL);

    json_object *msg_param = NULL;
    json_object *msg_param_val = NULL;
    char event_name[256] = {'\0'};
    char event_val[256] = {'\0'};

    if (json_object_object_get_ex(jobj, "params", &msg_param))
    {
        json_object *jsubs_param_array = json_object_array_get_idx(msg_param, 0);
        if (jsubs_param_array == NULL)
        {
            LOGINFO("Failed to get params data from subscription json message \n");
            json_object_put(jobj);
            assert(0);
        }

        if (json_object_object_get_ex(jsubs_param_array, "name", &msg_param_val))
        {
            strcpy(event_name, json_object_get_string(msg_param_val));
            LOGINFO("Event name = %s", event_name);
        }
        else
        {
            LOGINFO("Failed to get event name data from subscription json message");
            json_object_put(jobj);
            assert(0);
        }

        if (json_object_object_get_ex(jsubs_param_array, "value", &msg_param_val))
        {
            strcpy(event_val, json_object_get_string(msg_param_val));
            LOGINFO("Event value = %s", event_val);
        }
        else
        {
            LOGINFO("Failed to get event value data from subscription json message");
            json_object_put(jobj);
            assert(0);
        }
    }

    /**
     * Check which event we have received.
     */
    if (strncmp(event_name, DSL_EVENT_PATH, strlen(DSL_EVENT_PATH)) == 0)
    {
        LOGINFO("Event - [%s][%s]", event_name, event_val);
    }
    else if (strncmp(event_name, ETH_EVENT_PATH, strlen(ETH_EVENT_PATH)) == 0)
    {
        LOGINFO("Event - [%s][%s]", event_name, event_val);
    }

    json_object_put(jobj);
}

int main (int argc, char **argv)
{
    if (argc != 2) {
        usage (argv[0]);
        exit(0);
    }

    int rc = RETURN_ERR;

    /**
     * Manager application can start client library by invoking this API.
     **/
    rc = json_hal_client_init(argv[1]);
    assert (rc == RETURN_OK); /* Failure. Assert if client initialization failed. */

     /**
     * Run client socket thread.
     */
    rc = json_hal_client_run();
    assert(rc == RETURN_OK); /* Failure. Client socket is not started. */

    /**
     * Subscribe link events.
     */
    rc = send_subscription_msg ();
    assert(rc == RETURN_OK); /* Event subscription failed. */

    while (1) {
        sleep (1);
    }

    return 0;
}
