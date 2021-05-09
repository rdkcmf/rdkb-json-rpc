# JSON HAL Server library

* JSON HAL Server Library.
* Vendor Software can register their callbacks for certain well defined actions to the HAL server library. These callback will be invoked by the server library when we get client requests.
* Vendor software can publish their events like link status to Server library.


## How to use it

* Link `libjson-hal-server.so` in the application.
* Include `json_hal_server.h` header in the application.

## Public APIs

* int json_hal_server_init(const char *hal_conf_path) -> Initialize the hal server module. Pass the configuration file contains the schema path and server port as argument to the API.

* int json_hal_server_register_action_callback(const char *action_name,(void*)callback) -> Register vendor software callback functions to the HAL server library.
* void json_hal_server_run() -> Start the server socket thread. This will start the server socket and listen for client connections and requests.
* int json_hal_server_publish_event(char *event_name, char *event_value) -> Publish events to the client. Application can send their event notifications to the subscribed clients.

## Example usage

```code [hal- server]
  /**
     * HAL Server Initialization.
     * Entry point for the server. Initialize the server software.
     */
    rc = json_hal_server_init(config_file);
    assert(rc == RETURN_OK);
    printf("Test case - HAL Server init successful  \n");

    /**
     *  Register RPC methods and its callback function.
     *  Register all the supported methods and its action callback with rpc server library.
     */
    json_hal_server_register_action_callback("setParameters", setparam_rpc_cb);
    json_hal_server_register_action_callback("getParameters", getparam_rpc_cb);
    json_hal_server_register_action_callback("subscribeEvent", subs_event_cb);

    /**
     *  Started the HAL server socket.
     *  Server socket created and listen for the client connections at this point.
     **/
    rc = json_hal_server_run();
    assert(rc == RETURN_OK);
    ...
```
## How it works

This server demon using unix tcp socket mechanism for IPC. And json-rpc using for the rpc mechanism.

When server socket received a request from the client, socket server main thread will invoke the registered callback function for processing message. This function will parse the json request and check for reqId and action name. Each request should have the request id and action name. And then check the global list of rpc supported functions to find the handler of the requested rpc method. If a match found, corresponding callback function being invoked and executed. It then responds back with the Json response. Once rpc_socket_process received the json response then it will send the buffer data back to client.

## Dependency

Its code depends with `json-c (0.11)`.

# JSON HAL Client library

* JSON client library embedded the TCP socket logic to connect and manage the rpc requests and responses from or to rpc server demon.
* This library maintains a state machine to handle the socket connection, and requests.
* All the data exchange between client and server via Json messages.

## How to use it

* Link `libjson_hal_client.so` in the application.
* Include `json_hal_client.h` header in the application.

## Public APIs
* int json_hal_client_init(const char *hal_conf_path) -> Initialize the hal client module. Pass the configuration file contains the schema path and server port as argument to the API.

* int json_hal_client_send_and_get_reply(const char *request,json_object**  reply_msg ) -> Send the request message to the server socket, sync the response from server and return back the filled data to caller.
* int json_hal_client_subscribe_event(event_callback callback, char* event_message) -> Register the callback function to notify for the events.
* int json_hal_client_terminate() -> Clean up function.
* int json_hal_is_client_connected() -> Check the client is successfully connected to the server.

## Example usage

```code [hal client]
int usage ()
{
    /* Start client socket.
     * manager application can start client library by invoking this API. */

    rc = json_hal_client_init(config_file);
    ...

    /**
     * Start client socket.
     */
    rc = json_hal_client_run();
    ...

    /**
     * Make sure client connected to server.
     */
    int is_client_connected = FALSE;
    int retry_count = 0;

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
    ...
}

/**
 * Subscribe event message.
 */
static int subscribe_event_message_success()
{
    ...
    rc = json_hal_client_subscribe_event(eventcb, msg);
    ..
}
```
## How it works

This client demon using unix tcp socket mechanism for IPC. And json-rpc using for the rpc mechanism.

Manager application can init and start the client library. This will start the client socket and connected to the server socket.
Manager can create json request and invoked json_hal_client_send_and_get_reply() call, This API is a blocking call, send the json request to server. Once API receive response
from server, it will cross check the reqId and fill the data into the buffer and send back to manager. Manager can unpack the response message and do the necessary actions.


## Dependency
Its generic code depends only with `json-c (0.11)` library.

## Test Application

* As part of library , provided both client and server test applications:
`test_json_hal_cli`
`test_json_hal_srv`

# How to run test applications

* Run server application `test_json_hal_srv`
```Usage
    test_json_rpc_srv test_json_rpc_srv <configuration file>
    Eg: test_json_rpc_srv "xdsl_config.json"
```

* Run client `test_json_hal_cli`
```Usage
    test_json_hal_cli <configuration file> <example request json file>
        Note: Example json request is embedded in the file and passed to client application

    Eg: test_json_hal_cli "xdsl_config.json" "example_setParam.json"

    //example_setPara.json
    {
        "module": "xdslhal",
        "version": "0.0.1",
        "action": "setParameters",
        "reqId" : "100001",
        "params":
        [
             {"name":"Device.DSL.Line.1.Enable","type":"boolean","value":true}
        ]
    }
```