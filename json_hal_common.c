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

#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include "json_hal_common.h"
#include "json_rpc_common.h"

int json_hal_get_param(json_object *jmsg, int index, eActionType action, hal_param_t *param)
{
    json_object *jparams = NULL;
    json_object *jparam = NULL;
    json_object *jparamobject = NULL;

    const char *param_type = NULL;

    if(jmsg == NULL || param == NULL) {
        return RETURN_ERR;
    }

    memset(param, 0, sizeof(hal_param_t));

    if (json_object_object_get_ex(jmsg, JSON_RPC_FIELD_PARAMS, &jparams)) {
        jparam = json_object_array_get_idx(jparams, index);
    }
    else {
        return RETURN_ERR;
    }

    switch(action) {
        case GET_REQUEST_MESSAGE:
        case DELETE_REQUEST_MESSAGE:
            if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_NAME, &jparamobject)) {
                strncpy(param->name, json_object_get_string(jparamobject), sizeof(param->name));
            }
            else {
                return RETURN_ERR;
            }
        break;

        case SET_REQUEST_MESSAGE:
        case GET_RESPONSE_MESSAGE:
            if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_NAME, &jparamobject)) {
                strncpy(param->name, json_object_get_string(jparamobject), sizeof(param->name));
            }
            else {
                return RETURN_ERR;
            }
            if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_TYPE, &jparamobject)) {
                param_type = json_object_get_string(jparamobject);
                if(strncmp(param_type, JSON_RPC_FIELD_TYPE_STRING, strlen(JSON_RPC_FIELD_TYPE_STRING)) == 0) {
                    if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_VALUE, &jparamobject)) {
                        strncpy(param->value, json_object_get_string(jparamobject), sizeof(param->value));
                    }
                    else {
                        return RETURN_ERR;
                    }
                    param->type = PARAM_STRING;
            }
            else if(strncmp(param_type, JSON_RPC_FIELD_TYPE_HEX_BINARY, strlen(JSON_RPC_FIELD_TYPE_HEX_BINARY)) == 0) {
                if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_VALUE, &jparamobject)) {
                    strncpy(param->value, json_object_get_string(jparamobject), sizeof(param->value));
                }
                else {
                    return RETURN_ERR;
                }
                param->type = PARAM_HEXBINARY;
            }
            else if(strncmp(param_type, JSON_RPC_FIELD_TYPE_BASE64, strlen(JSON_RPC_FIELD_TYPE_BASE64)) == 0) {
                if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_VALUE, &jparamobject)) {
                    strncpy(param->value, json_object_get_string(jparamobject), sizeof(param->value));
                }
                else {
                    return RETURN_ERR;
                }
                param->type = PARAM_BASE64;
            }
            else if(strncmp(param_type, JSON_RPC_FIELD_TYPE_BOOLEAN, strlen(JSON_RPC_FIELD_TYPE_BOOLEAN)) == 0) {
                if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_VALUE, &jparamobject)) {
                    bool bool_value = json_object_get_boolean(jparamobject);
                    snprintf(param->value, sizeof(param->value), "%d", bool_value);
                }
                param->type = PARAM_BOOLEAN;
            }
            else if(strncmp(param_type, JSON_RPC_FIELD_TYPE_INTEGER, strlen(JSON_RPC_FIELD_TYPE_INTEGER)) == 0) {
                if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_VALUE, &jparamobject)) {
                    int int_value = json_object_get_int(jparamobject);
                    snprintf(param->value, sizeof(param->value), "%d", int_value);
                }
                param->type = PARAM_INTEGER;
            }
            else if(strncmp(param_type, JSON_RPC_FIELD_TYPE_UNSIGNED_INTEGER, strlen(JSON_RPC_FIELD_TYPE_UNSIGNED_INTEGER))==0) {
                if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_VALUE, &jparamobject)) {
                    unsigned int uint_value = json_object_get_int(jparamobject);
                    snprintf(param->value, sizeof(param->value), "%d", uint_value);
                }
                param->type = PARAM_UNSIGNED_INTEGER;
            }
            else if(strncmp(param_type, JSON_RPC_FIELD_TYPE_LONG, strlen(JSON_RPC_FIELD_TYPE_LONG)) == 0) {
                if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_VALUE, &jparamobject)) {
                    long long_value = json_object_get_int64(jparamobject);
                    snprintf(param->value, sizeof(param->value), "%ld", long_value);
                }
                param->type = PARAM_LONG;
            }
            else if(strncmp(param_type, JSON_RPC_FIELD_TYPE_UNSIGNED_LONG, strlen(JSON_RPC_FIELD_TYPE_UNSIGNED_LONG)) == 0) {
                if (json_object_object_get_ex(jparam, JSON_RPC_FIELD_PARAM_VALUE, &jparamobject)) {
                    unsigned long ulong_value = json_object_get_int64(jparamobject);
                    snprintf(param->value, sizeof(param->value), "%ld", ulong_value);
                }
                param->type = PARAM_UNSIGNED_LONG;
            }
        }
        else {
            return RETURN_ERR;
        }
        break;
        case PUBLISHEVENT_RESPONSE_MESSAGE:
        default:
            LOGINFO("No action required!!!\n");
            break;
    }

    return RETURN_OK;
}

int json_hal_add_param(json_object *jreply, eActionType action, hal_param_t *param)
{
    json_object *jparams = NULL;
    json_object *jparam = NULL;

    int int_value = 0;
    unsigned int uint_value = 0;
    long long_value = 0;
    unsigned ulong_value = 0;

    if(jreply == NULL || param == NULL) {
        return RETURN_ERR;
    }

    switch(action) {
        case SET_REQUEST_MESSAGE:
        case GET_RESPONSE_MESSAGE:
            if(json_object_object_get_ex(jreply, JSON_RPC_FIELD_PARAMS, &jparams)) {
                jparam = json_object_new_object();
                json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_NAME, json_object_new_string(param->name));

                switch(param->type) {
                    case PARAM_STRING:
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_TYPE, json_object_new_string(JSON_RPC_FIELD_TYPE_STRING));
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_string(param->value));
                        break;
                    case PARAM_HEXBINARY:
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_TYPE, json_object_new_string(JSON_RPC_FIELD_TYPE_HEX_BINARY));
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_string(param->value));
                        break;
                    case PARAM_BASE64:
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_TYPE, json_object_new_string(JSON_RPC_FIELD_TYPE_BASE64));
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_string(param->value));
                        break;
                    case PARAM_BOOLEAN:
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_TYPE, json_object_new_string(JSON_RPC_FIELD_TYPE_BOOLEAN));
                        if(strcmp(param->value, "true") == 0 || strcmp(param->value, "TRUE") == 0) {
                            json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_boolean(1));
                        }
                        else if(strcmp(param->value, "false") == 0 || strcmp(param->value, "FALSE") == 0) {
                            json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_boolean(0));
                        }
                        else {
                            return RETURN_ERR;
                        }
                        break;
                    case PARAM_INTEGER:
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_TYPE, json_object_new_string(JSON_RPC_FIELD_TYPE_INTEGER));
                        int_value = atoi(param->value);
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_int(int_value));
                        break;
                    case PARAM_UNSIGNED_INTEGER:
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_TYPE, json_object_new_string(JSON_RPC_FIELD_TYPE_UNSIGNED_INTEGER));
                        uint_value = atoi(param->value);
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_int(uint_value));
                        break;
                    case PARAM_LONG:
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_TYPE, json_object_new_string(JSON_RPC_FIELD_TYPE_LONG));
                        long_value = atol(param->value);
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_int64(long_value));
                        break;
                    case PARAM_UNSIGNED_LONG:
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_TYPE, json_object_new_string(JSON_RPC_FIELD_TYPE_UNSIGNED_LONG));
                        ulong_value = atol(param->value);
                        json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_int64(ulong_value));
                        break;
                }
                json_object_array_put_idx(jparams, json_object_array_length(jparams), jparam);
            }
            else {
                return RETURN_ERR;
            }
            break;

        case PUBLISHEVENT_RESPONSE_MESSAGE:
            if(json_object_object_get_ex(jreply, JSON_RPC_FIELD_PARAMS, &jparams)) {
                json_object_object_add(jparams, JSON_RPC_FIELD_PARAM_NAME, json_object_new_string(param->name));
                json_object_object_add(jparams, JSON_RPC_FIELD_PARAM_VALUE, json_object_new_string(param->value));
            }
            else {
                return RETURN_ERR;
            }
        break;
        case GET_REQUEST_MESSAGE:
        case DELETE_REQUEST_MESSAGE:
            if(json_object_object_get_ex(jreply, JSON_RPC_FIELD_PARAMS, &jparams)) {
                jparam = json_object_new_object();
                json_object_object_add(jparam, JSON_RPC_FIELD_PARAM_NAME, json_object_new_string(param->name));
                json_object_array_put_idx(jparams, json_object_array_length(jparams), jparam);
            }
            else {
                return RETURN_ERR;
            }
        break;
    }

    return RETURN_OK;
}

int json_hal_load_config(const char *config_file, hal_config_t *config)
{
    POINTER_ASSERT(config_file != NULL);
    POINTER_ASSERT(config != NULL);

    FILE *fp = NULL;
    char buffer[BUF_256] = {0};

    json_object *parsed_json = NULL;
    json_object *schema = NULL;
    json_object *port = NULL;

    fp = fopen(config_file, "r");
    if (fp == NULL)
    {
        LOGERROR("json file not found %s \n", config_file);
        return RETURN_ERR;
    }
    if (fread(buffer, sizeof(buffer), 1, fp) != 1)
    {
        // LOGERROR("Unexpected amount read from configuration file\n");
    }
    fclose(fp);

    /**
     * Read schema path and server port number from
     * config file.
     */
    parsed_json = json_tokener_parse(buffer);
    POINTER_ASSERT(parsed_json != NULL);

    if (json_object_object_get_ex(parsed_json, HAL_SCHEMA_PATH, &schema))
    {
        strcpy(config->hal_schema_path, json_object_get_string(schema));
    }
    else
    {
        LOGERROR("Failed to get schema path from configuration file \n");
        json_object_put(parsed_json);
        return RETURN_ERR;
    }

    if (json_object_object_get_ex(parsed_json, SERVER_PORT, &port))
    {
        config->server_port_number = json_object_get_int(port);
    }
    else
    {
        LOGERROR("Failed to get server port from configuration file \n");
        json_object_put(parsed_json);
        return RETURN_ERR;
    }
    json_object_put(parsed_json);

    /**
     * Read module version and name from schema file.
     */
    char *buf = NULL;

    /**
     * Get schema file size.
     * Find the size and allocate the memory to store the file contents into buffer.
     */
    struct stat st;
    if(stat(config->hal_schema_path, &st) != 0)
    {
        LOGERROR("Failed to load the file \n");
        return RETURN_ERR;
    }
    size_t buffer_length = st.st_size;
    LOGINFO ("Buffer length = %zu \n",  buffer_length);

    buf = (char *) calloc (1, buffer_length + 1);
    if (buf == NULL)
    {
        LOGERROR("Failed to allocate memory to hold contents \n");
        return RETURN_ERR;
    }

    fp = fopen(config->hal_schema_path, "r");
    if (fp == NULL)
    {
        LOGERROR("Failed to load the shcema file \n");
        free (buf);
        return RETURN_ERR;
    }

    size_t num_bytes_read = 0;
    num_bytes_read = fread(buf, 1, buffer_length, fp);
    buf[num_bytes_read] = '\0';
    fclose(fp);

    json_object *jparsed = NULL;
    json_object *jdefinitions = NULL;
    json_object *jkey = NULL;
    json_object *jvalue = NULL;
    jparsed = json_tokener_parse(buf);
    POINTER_ASSERT(jparsed != NULL);

    json_object_object_get_ex(jparsed, "definitions", &jdefinitions);
    if (jdefinitions == NULL)
    {
        LOGERROR("Failed to get defintion from schema \n");
        free(buf);
        json_object_put(jparsed);
        return RETURN_ERR;
    }

    json_object_object_get_ex(jdefinitions, "schemaVersion", &jkey);
    if (jkey == NULL)
    {
        LOGERROR("Failed to get version from schema \n");
        free(buf);
        json_object_put(jparsed);
        return RETURN_ERR;
    }

    json_object_object_get_ex(jkey, "const", &jvalue);
    if (jvalue == NULL)
    {
        LOGERROR("Failed to get const field from schema \n");
        free(buf);
        json_object_put(jparsed);
        return RETURN_ERR;
    }
    strcpy(config->hal_module_version, json_object_get_string(jvalue));

    json_object_object_get_ex(jdefinitions, "moduleName", &jkey);
    if (jkey == NULL)
    {
        LOGERROR("Failed to get module name from schema \n");
        free(buf);
        json_object_put(jparsed);
        return RETURN_ERR;
    }

    json_object_object_get_ex(jkey, "const", &jvalue);
    if (jvalue == NULL)
    {
        LOGERROR("Failed to get const field from schema \n");
        free(buf);
        json_object_put(jparsed);
        return RETURN_ERR;
    }
    strcpy(config->hal_module_name, json_object_get_string(jvalue));

    free(buf);
    json_object_put(jparsed);
    return RETURN_OK;
}
