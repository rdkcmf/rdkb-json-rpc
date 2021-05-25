
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
 * Json RPC test client application.
 */
#include <stdio.h>
#include "json_hal_client.h"
#include <json-c/json.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

/**
 * Test API -  Send a valid JSON RPC request to server and check response.
 */
static int send_json_rpc_request(json_object *jrequest);

/**
 * Global varaiable used to verify we got event from server.
 */
int got_event = FALSE;

static void usage(char *appname)
{
    printf("usage: %s <configuration file> <example request json file> \n", appname);
    printf(" \t configuration file : json formatted file contains the schema path and server port number\n");
    printf(" \t example request json file : json formatted file contains the request message \n");
    printf(" \t Example: %s /etc/rdk/conf/xdsl_manager_conf.json example_setParameters_req.json \n", appname);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        usage(argv[0]);
        exit(0);
    }

    int rc = RETURN_ERR;
    if (argv[1] == NULL || argv[2] == NULL)
    {
        printf("Invalid argument \n");
        exit(0);
    }

    /**
     * Manager application can start client library by invoking this API.
     **/
    rc = json_hal_client_init(argv[1]);
    assert(rc == RETURN_OK); /* Failure. Assert if client intialisation failed. */

    /**
     * Run client socket thread.
     */
    rc = json_hal_client_run();
    assert(rc == RETURN_OK); /* Failure. Client socket is not started. */

    int is_client_connected = FALSE;
    int retry_count = 0;

    /**
     * Make sure connected to the server.
     */
    while (retry_count < 120)
    {
        /**
         * make sure connection established with server.
         */
        if (!json_hal_is_client_connected())
        {
            sleep(1);
            retry_count++;
        }
        else
        {
            printf("Hal-client connected to the rpc server \n");
            is_client_connected = TRUE;
            break;
        }
    }
    assert(is_client_connected == TRUE); /* Failure. Assert if client connection failed. */

    /**
     * Read request from the json file.
     */
    char json_path[128] = {'\0'};
    strncpy(json_path, argv[2], sizeof(json_path));
    FILE *fp = NULL;
    json_object *parsed_json = NULL;
    char buffer[2048] = {0};
    fp = fopen(json_path, "r");
    if(fp == NULL) {
        printf("json file not found %s \n", json_path);
        return -1;
    }

    if (fread(buffer, sizeof(buffer), 1, fp) != 1)
    {
        // printf("Incomplete read from json file\n");
    }

    fclose(fp);

    /* create a json request from json file */
    parsed_json = json_tokener_parse(buffer);

    rc = send_json_rpc_request(parsed_json);
    if (rc != RETURN_OK)
        printf("Test execution failed \n");
    else
        printf("Test execution successful \n");

    /**
     * Clean up client.
     **/
    rc = json_hal_client_terminate();
    assert(rc == RETURN_OK);

    return 0;
}

/**
 * Json request valid case.
 * {
  "module":"dslhal",
  "version":"1.0.0",
  "action":"action name",
  "reqId":"000003E9",
  "params":[
    {
      "name":"Device.DSL.Line.1.Enable",
      "value":true
    }
  ]
}
*/
static int send_json_rpc_request(json_object *jrequest)
{
    json_object *reply_msg;

    int rc = RETURN_OK;

    if(jrequest == NULL ) {
        return RETURN_ERR;
    }

    printf("JSON Request message = %s \n", json_object_to_json_string_ext(jrequest, JSON_C_TO_STRING_PRETTY));
    rc = json_hal_client_send_and_get_reply(jrequest, &reply_msg);
    if (rc < 0)
    {
        printf("[%s][%d] RPC message failed \n", __FUNCTION__, __LINE__);
    }
    else
    {
        printf("Response message = %s \n", json_object_to_json_string_ext(reply_msg, JSON_C_TO_STRING_PRETTY));
    }

    json_object_put(jrequest);
    json_object_put(reply_msg);
    jrequest = NULL;
    reply_msg = NULL;
    return rc;
}
