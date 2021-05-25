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
#include "json_schema_validator_wrapper.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char**argv)
{
    if (argc != 3)
    {
        printf("Usage: test_schema_validator <schema.json> <sample.json> \n");
        printf("Example: test_schema_validator /data/sky_schema.json /data/sample.json  \n");
        exit(0);
    }

    int ret;
    char *buffer = NULL;
    long length;

    ret = json_validator_init(argv[1]);
    assert(ret == VALIDATOR_RETURN_OK);

    /* Read json request from file and store it into string. */
    FILE *f = fopen(argv[2], "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (char *)malloc(length);
        if (buffer)
        {
            if (fread(buffer, 1, length, f) != length)
            {
                printf("Incomplete read from json sample file\n");
            }
        }
        fclose(f);
    }

	double time_spent = 0.0;
    clock_t begin = clock();
    if (buffer)
    {
        printf("json sample = %s \n", buffer);
        ret = json_validator_validate_request(buffer);
        if (ret != VALIDATOR_RETURN_OK)
        {
            printf("Validating json sample against schema is failed \n");
        }
        else
        {
            printf("Validating json sample against schema is successful \n");
        }
        free(buffer);
    }
    else
    {
        printf("Failed to get the json message from file \n");
    }
    clock_t end = clock();
    time_spent += (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time taken to validate request is %f seconds \n", time_spent);

    json_validator_terminate();

    return 0;
}
