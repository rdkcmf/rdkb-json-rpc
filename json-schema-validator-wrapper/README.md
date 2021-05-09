# JSON Schema Validator Wrapper Library

* JSON Schema validator wrapper library
* This library used to validate the json samples against the json schema. For this purpose, we have C++ json schema validator library. To use this library, C applications required a wrapper library with C linkage to use actual C++ validator library.


## How to use it

* Link `libjson_schema_validator_wrapper` in the application.
* Include `json_schema_validator_wrapper.h` header in the application.

## Public APIs

* int json_validator_init(const char *json_schema_path);
* int json_validator_validate_request(const char *json_string);
* int json_validator_terminate();

## Example usage

```code [hal- server]
 int ret;
 ret = json_validator_init("/tmp/json_schema.json");
 ...
 ret = json_validator_validate_request(buffer); /* buffer contains json message. */
 ...
 json_validator_terminate();
```

## Dependency

Its code depends with `pboettch/json-schema-validator` and `nlohmann/json ` library.
https://github.com/pboettch/json-schema-validator
https://github.com/nlohmann/json

## Test Application

* A test application `test_json_schema_validator` provided as part of library.
* Run test application:
```Usage
 test_schema_validator <schema.json> <sample.json>
```