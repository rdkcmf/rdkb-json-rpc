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

#ifndef _JSON_RPC_COMMON_HEADER_H
#define _JSON_RPC_COMMON_HEADER_H

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef RETURN_OK
#define RETURN_OK 0
#endif

#ifndef RETURN_ERR
#define RETURN_ERR -1
#endif

#define JSON_RPC_FIELD_MODULE "module"
#define JSON_RPC_FIELD_VERSION "version"
#define JSON_RPC_FIELD_ACTION "action"
#define JSON_RPC_FIELD_ID "reqId"
#define JSON_RPC_FIELD_PARAMS "params"
#define JSON_RPC_FIELD_PARAM_NAME "name"
#define JSON_RPC_FIELD_PARAM_VALUE "value"
#define JSON_RPC_FIELD_PARAM_TYPE "type"
#define JSON_RPC_FIELD_SCHEMA_INFO "SchemaInfo"
#define JSON_RPC_FIELD_CONFIGURE_OBJECT "configureObject"
#define JSON_RPC_FIELD_SCHEMA_FILEPATH "FilePath"

#define JSON_RPC_FIELD_PARAM_NOTIFICATION_TYPE "notificationType"
#define JSON_RPC_FIELD_PARAM_NOTIFICATION_TYPE_ON_CHANGE "onChange"
#ifdef JSON_BLOCKING_SUBSCRIBE_EVENT
#define JSON_RPC_FIELD_PARAM_NOTIFICATION_TYPE_ON_CHANGE_SYNC "onChangeSync"
#define JSON_RPC_FIELD_PARAM_NOTIFICATION_TYPE_ON_CHANGE_SYNC_TIMEOUT "onChangeSyncTimeout"
#endif //JSON_BLOCKING_SUBSCRIBE_EVENT
#define JSON_RPC_FILED_RESULT "Result"
#define JSON_RPC_SUBSCRIBE_EVENT_ACTION_NAME "subscribeEvent"
#define JSON_RPC_PUBLISH_EVENT_ACTION_NAME "publishEvent"

#define JSON_RPC_FIELD_TYPE_STRING "string"
#define JSON_RPC_FIELD_TYPE_INTEGER "int"
#define JSON_RPC_FIELD_TYPE_BOOLEAN "boolean"
#define JSON_RPC_FIELD_TYPE_LONG "long"
#define JSON_RPC_FIELD_TYPE_UNSIGNED_INTEGER "unsignedInt"
#define JSON_RPC_FIELD_TYPE_UNSIGNED_LONG "unsignedLong"
#define JSON_RPC_FIELD_TYPE_HEX_BINARY "hexBinary"
#define JSON_RPC_FIELD_TYPE_BASE64 "base64"

#define JSON_RPC_PARAM_ARR_INDEX 0

#define JSON_RPC_STATUS_FAILED "Failed"
#define JSON_RPC_STATUS_SUCCESS "Success"
#define JSON_RPC_PARAM_STATUS_FIELD "Status"
#define JSON_RPC_STATUS_NOT_SUPPORTED "Not Supported"

#define JSON_RPC_ACTION_GET_PARAM "getParameters"
#define JSON_RPC_ACTION_GET_PARAM_RESPONSE "getParametersResponse"
#define JSON_RPC_ACTION_RESULT "result"
#define JSON_RPC_ACTION_GET_SCHEMA "getSchema"
#define JSON_RPC_ACTION_GET_SCHEMA_RESPONSE "getSchemaResponse"

#define BUF_32 32
#define BUF_64 64
#define BUF_128 128
#define BUF_256 256
#define MAX_BUFFER_SIZE 16384
#define MAX_FUNCTION_LEN 64
#define DEFAULT_SEQ_START_NUMBER 100
#define INVALD_LENGTH -1
#define IDLE_TIMEOUT_PERIOD 2000

/* Logging for the RPC module. */
#define LOGERROR(fmt, arg...) \
  fprintf(stderr, "ERROR[%s.%u]: " fmt "\n", __FUNCTION__, __LINE__, ##arg)

#define LOGINFO(fmt, arg...) \
  fprintf(stderr, "NOTICE:[%s.%u]: " fmt "\n", __FUNCTION__, __LINE__, ##arg)

#define POINTER_ASSERT(expr)                   \
  if (!(expr))                                 \
  {                                            \
    LOGERROR("Invalid parameter error!!!"); \
    return RETURN_ERR;                         \
  }

/* A no-return value version of POINTER_ASSERT. */
#define POINTER_ASSERT_V(expr)                   \
  if (!(expr))                                 \
  {                                            \
    LOGERROR("Invalid parameter error!!!"); \
  }

#endif
