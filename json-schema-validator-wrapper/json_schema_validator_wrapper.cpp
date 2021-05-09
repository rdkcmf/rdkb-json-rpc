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
#include <nlohmann/json-schema.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include "json_schema_validator_wrapper.h"

#ifdef __cplusplus
extern "C"
{
#endif

    using nlohmann::json;
    using nlohmann::json_schema::json_validator;

    class custom_error_handler : public nlohmann::json_schema::basic_error_handler
    {
        void error(const nlohmann::json::json_pointer &ptr, const json &instance, const std::string &message) override
        {
            nlohmann::json_schema::basic_error_handler::error(ptr, instance, message);
            std::cerr << "ERROR: '" << ptr << "' - '" << instance << "': " << message << "\n";
        }
    };

    /* Global json_validator object. */
    json_validator *validator = NULL;

    /*
     * @brief - Populate the json schema. This API will set the given schema to the
     * validator library.
     * @param String holds the schemafile name with the full path
     * @return VALIDATOR_RETURN_OK if validator successfull loads the schema else returned error code.
     */
    static int json_validator_populate_schema(const char *json_schema_path);

    int json_validator_init(const char* json_schema_path)
    {
        if (json_schema_path ==  nullptr)
        {
            std::cerr << "Invalid argument received" << std::endl;
            return VALIDATOR_RETURN_INVALID_ARGUMENTS;
        }

        int rc = VALIDATOR_RETURN_OK;
        validator = new json_validator();
        if (validator == nullptr)
        {
            std::cerr << "ERROR: Failed to allocate json validator object " << std::endl;
            return VALIDATOR_RETURN_MEM_ALLOC_FAILED;
        }

        /* Set root json schema. */
        rc = json_validator_populate_schema(json_schema_path);
        return rc;
    }

    int json_validator_terminate()
    {
        if (validator != nullptr)
        {
            delete validator;
            validator = nullptr;
        }

        return VALIDATOR_RETURN_OK;
    }

    int json_validator_validate_request(const char *json_string)
    {

        if (json_string == nullptr)
        {
            std::cerr << "Invalid memory" << std::endl;
            return VALIDATOR_RETURN_INVALID_ARGUMENTS;
        }

        /* C string to c++ string format. */
        std::string json_str(json_string);
        json json_request = json::parse(json_str);
#ifdef DEBUG_ENABLED
        std::cout << "json request message \n"
            << std::setw(4) << json_request << std::endl;
#endif
        custom_error_handler json_err;
        validator->validate(json_request, json_err);

        if (json_err)
        {
            std::cerr << "schema validation failed" << std::endl;
            return VALIDATOR_RETURN_ERR;
        }

        return VALIDATOR_RETURN_OK;
    }

    static int json_validator_populate_schema(const char *json_schema_path)
    {
        if (json_schema_path == nullptr)
        {
            std::cerr << "ERROR: Empty schema path" << std::endl;
            return VALIDATOR_RETURN_INVALID_ARGUMENTS;
        }

        json schema;
        std::ifstream f(json_schema_path);
        if (!f.good())
        {
            std::cerr << "Failed to open schema file for reading" << std::endl;
            return VALIDATOR_RETURN_ERR;
        }

        try
        {
            f >> schema;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Reading schema failed" << std::endl;
            std::cerr << e.what() << std::endl;
            return VALIDATOR_RETURN_ERR;
        }

        if (validator)
        {
            try
            {
                // insert this schema as the root to the validator
                // this resolves remote-schemas, sub-schemas and references via the given loader-function
                validator->set_root_schema(schema);
            }
            catch (const std::exception &e)
            {
                std::cerr << "setting root schema failed" << std::endl;
                std::cerr << e.what() << std::endl;
                return VALIDATOR_RETURN_ERR;
            }
        }
        else
        {
            std::cerr << "validator object not found" << std::endl;
            return VALIDATOR_RETURN_ERR;
        }

        return VALIDATOR_RETURN_OK;
    }


#ifdef __cplusplus
}
#endif
