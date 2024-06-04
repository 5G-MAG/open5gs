
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "_mbs_contexts_post_request.h"

OpenAPI__mbs_contexts_post_request_t *OpenAPI__mbs_contexts_post_request_create(
    OpenAPI_context_status_notification_t *json_data,
    OpenAPI_binary_t* binary_data_n2_information1,
    OpenAPI_binary_t* binary_data_n2_information2,
    OpenAPI_binary_t* binary_data_n2_information3,
    OpenAPI_binary_t* binary_data_n2_information4,
    OpenAPI_binary_t* binary_data_n2_information5,
    OpenAPI_binary_t* binary_data_n2_information6,
    OpenAPI_binary_t* binary_data_n2_information7,
    OpenAPI_binary_t* binary_data_n2_information8,
    OpenAPI_binary_t* binary_data_n2_information9,
    OpenAPI_binary_t* binary_data_n2_information10
)
{
    OpenAPI__mbs_contexts_post_request_t *_mbs_contexts_post_request_local_var = ogs_malloc(sizeof(OpenAPI__mbs_contexts_post_request_t));
    ogs_assert(_mbs_contexts_post_request_local_var);

    _mbs_contexts_post_request_local_var->json_data = json_data;
    _mbs_contexts_post_request_local_var->binary_data_n2_information1 = binary_data_n2_information1;
    _mbs_contexts_post_request_local_var->binary_data_n2_information2 = binary_data_n2_information2;
    _mbs_contexts_post_request_local_var->binary_data_n2_information3 = binary_data_n2_information3;
    _mbs_contexts_post_request_local_var->binary_data_n2_information4 = binary_data_n2_information4;
    _mbs_contexts_post_request_local_var->binary_data_n2_information5 = binary_data_n2_information5;
    _mbs_contexts_post_request_local_var->binary_data_n2_information6 = binary_data_n2_information6;
    _mbs_contexts_post_request_local_var->binary_data_n2_information7 = binary_data_n2_information7;
    _mbs_contexts_post_request_local_var->binary_data_n2_information8 = binary_data_n2_information8;
    _mbs_contexts_post_request_local_var->binary_data_n2_information9 = binary_data_n2_information9;
    _mbs_contexts_post_request_local_var->binary_data_n2_information10 = binary_data_n2_information10;

    return _mbs_contexts_post_request_local_var;
}

void OpenAPI__mbs_contexts_post_request_free(OpenAPI__mbs_contexts_post_request_t *_mbs_contexts_post_request)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == _mbs_contexts_post_request) {
        return;
    }
    if (_mbs_contexts_post_request->json_data) {
        OpenAPI_context_status_notification_free(_mbs_contexts_post_request->json_data);
        _mbs_contexts_post_request->json_data = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information1) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information1->data);
        _mbs_contexts_post_request->binary_data_n2_information1 = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information2) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information2->data);
        _mbs_contexts_post_request->binary_data_n2_information2 = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information3) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information3->data);
        _mbs_contexts_post_request->binary_data_n2_information3 = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information4) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information4->data);
        _mbs_contexts_post_request->binary_data_n2_information4 = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information5) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information5->data);
        _mbs_contexts_post_request->binary_data_n2_information5 = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information6) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information6->data);
        _mbs_contexts_post_request->binary_data_n2_information6 = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information7) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information7->data);
        _mbs_contexts_post_request->binary_data_n2_information7 = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information8) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information8->data);
        _mbs_contexts_post_request->binary_data_n2_information8 = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information9) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information9->data);
        _mbs_contexts_post_request->binary_data_n2_information9 = NULL;
    }
    if (_mbs_contexts_post_request->binary_data_n2_information10) {
        ogs_free(_mbs_contexts_post_request->binary_data_n2_information10->data);
        _mbs_contexts_post_request->binary_data_n2_information10 = NULL;
    }
    ogs_free(_mbs_contexts_post_request);
}

cJSON *OpenAPI__mbs_contexts_post_request_convertToJSON(OpenAPI__mbs_contexts_post_request_t *_mbs_contexts_post_request)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (_mbs_contexts_post_request == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [_mbs_contexts_post_request]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (_mbs_contexts_post_request->json_data) {
    cJSON *json_data_local_JSON = OpenAPI_context_status_notification_convertToJSON(_mbs_contexts_post_request->json_data);
    if (json_data_local_JSON == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [json_data]");
        goto end;
    }
    cJSON_AddItemToObject(item, "jsonData", json_data_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [json_data]");
        goto end;
    }
    }

    if (_mbs_contexts_post_request->binary_data_n2_information1) {
    char* encoded_str_binary_data_n2_information1 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information1->data,_mbs_contexts_post_request->binary_data_n2_information1->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information1", encoded_str_binary_data_n2_information1) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information1]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information1);
    }

    if (_mbs_contexts_post_request->binary_data_n2_information2) {
    char* encoded_str_binary_data_n2_information2 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information2->data,_mbs_contexts_post_request->binary_data_n2_information2->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information2", encoded_str_binary_data_n2_information2) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information2]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information2);
    }

    if (_mbs_contexts_post_request->binary_data_n2_information3) {
    char* encoded_str_binary_data_n2_information3 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information3->data,_mbs_contexts_post_request->binary_data_n2_information3->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information3", encoded_str_binary_data_n2_information3) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information3]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information3);
    }

    if (_mbs_contexts_post_request->binary_data_n2_information4) {
    char* encoded_str_binary_data_n2_information4 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information4->data,_mbs_contexts_post_request->binary_data_n2_information4->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information4", encoded_str_binary_data_n2_information4) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information4]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information4);
    }

    if (_mbs_contexts_post_request->binary_data_n2_information5) {
    char* encoded_str_binary_data_n2_information5 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information5->data,_mbs_contexts_post_request->binary_data_n2_information5->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information5", encoded_str_binary_data_n2_information5) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information5]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information5);
    }

    if (_mbs_contexts_post_request->binary_data_n2_information6) {
    char* encoded_str_binary_data_n2_information6 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information6->data,_mbs_contexts_post_request->binary_data_n2_information6->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information6", encoded_str_binary_data_n2_information6) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information6]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information6);
    }

    if (_mbs_contexts_post_request->binary_data_n2_information7) {
    char* encoded_str_binary_data_n2_information7 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information7->data,_mbs_contexts_post_request->binary_data_n2_information7->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information7", encoded_str_binary_data_n2_information7) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information7]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information7);
    }

    if (_mbs_contexts_post_request->binary_data_n2_information8) {
    char* encoded_str_binary_data_n2_information8 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information8->data,_mbs_contexts_post_request->binary_data_n2_information8->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information8", encoded_str_binary_data_n2_information8) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information8]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information8);
    }

    if (_mbs_contexts_post_request->binary_data_n2_information9) {
    char* encoded_str_binary_data_n2_information9 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information9->data,_mbs_contexts_post_request->binary_data_n2_information9->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information9", encoded_str_binary_data_n2_information9) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information9]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information9);
    }

    if (_mbs_contexts_post_request->binary_data_n2_information10) {
    char* encoded_str_binary_data_n2_information10 = OpenAPI_base64encode(_mbs_contexts_post_request->binary_data_n2_information10->data,_mbs_contexts_post_request->binary_data_n2_information10->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information10", encoded_str_binary_data_n2_information10) == NULL) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed [binary_data_n2_information10]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information10);
    }

end:
    return item;
}

OpenAPI__mbs_contexts_post_request_t *OpenAPI__mbs_contexts_post_request_parseFromJSON(cJSON *_mbs_contexts_post_requestJSON)
{
    OpenAPI__mbs_contexts_post_request_t *_mbs_contexts_post_request_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *json_data = NULL;
    OpenAPI_context_status_notification_t *json_data_local_nonprim = NULL;
    cJSON *binary_data_n2_information1 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information1 = NULL;
    cJSON *binary_data_n2_information2 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information2 = NULL;
    cJSON *binary_data_n2_information3 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information3 = NULL;
    cJSON *binary_data_n2_information4 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information4 = NULL;
    cJSON *binary_data_n2_information5 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information5 = NULL;
    cJSON *binary_data_n2_information6 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information6 = NULL;
    cJSON *binary_data_n2_information7 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information7 = NULL;
    cJSON *binary_data_n2_information8 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information8 = NULL;
    cJSON *binary_data_n2_information9 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information9 = NULL;
    cJSON *binary_data_n2_information10 = NULL;
    OpenAPI_binary_t *decoded_str_binary_data_n2_information10 = NULL;
    json_data = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "jsonData");
    if (json_data) {
    json_data_local_nonprim = OpenAPI_context_status_notification_parseFromJSON(json_data);
    if (!json_data_local_nonprim) {
        ogs_error("OpenAPI_context_status_notification_parseFromJSON failed [json_data]");
        goto end;
    }
    }

    binary_data_n2_information1 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information1");
    if (binary_data_n2_information1) {
    decoded_str_binary_data_n2_information1 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information1);
    if (!cJSON_IsString(binary_data_n2_information1)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information1]");
        goto end;
    }
    decoded_str_binary_data_n2_information1->data = OpenAPI_base64decode(binary_data_n2_information1->valuestring, strlen(binary_data_n2_information1->valuestring), &decoded_str_binary_data_n2_information1->len);
    if (!decoded_str_binary_data_n2_information1->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information1]");
        goto end;
    }
    }

    binary_data_n2_information2 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information2");
    if (binary_data_n2_information2) {
    decoded_str_binary_data_n2_information2 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information2);
    if (!cJSON_IsString(binary_data_n2_information2)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information2]");
        goto end;
    }
    decoded_str_binary_data_n2_information2->data = OpenAPI_base64decode(binary_data_n2_information2->valuestring, strlen(binary_data_n2_information2->valuestring), &decoded_str_binary_data_n2_information2->len);
    if (!decoded_str_binary_data_n2_information2->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information2]");
        goto end;
    }
    }

    binary_data_n2_information3 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information3");
    if (binary_data_n2_information3) {
    decoded_str_binary_data_n2_information3 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information3);
    if (!cJSON_IsString(binary_data_n2_information3)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information3]");
        goto end;
    }
    decoded_str_binary_data_n2_information3->data = OpenAPI_base64decode(binary_data_n2_information3->valuestring, strlen(binary_data_n2_information3->valuestring), &decoded_str_binary_data_n2_information3->len);
    if (!decoded_str_binary_data_n2_information3->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information3]");
        goto end;
    }
    }

    binary_data_n2_information4 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information4");
    if (binary_data_n2_information4) {
    decoded_str_binary_data_n2_information4 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information4);
    if (!cJSON_IsString(binary_data_n2_information4)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information4]");
        goto end;
    }
    decoded_str_binary_data_n2_information4->data = OpenAPI_base64decode(binary_data_n2_information4->valuestring, strlen(binary_data_n2_information4->valuestring), &decoded_str_binary_data_n2_information4->len);
    if (!decoded_str_binary_data_n2_information4->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information4]");
        goto end;
    }
    }

    binary_data_n2_information5 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information5");
    if (binary_data_n2_information5) {
    decoded_str_binary_data_n2_information5 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information5);
    if (!cJSON_IsString(binary_data_n2_information5)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information5]");
        goto end;
    }
    decoded_str_binary_data_n2_information5->data = OpenAPI_base64decode(binary_data_n2_information5->valuestring, strlen(binary_data_n2_information5->valuestring), &decoded_str_binary_data_n2_information5->len);
    if (!decoded_str_binary_data_n2_information5->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information5]");
        goto end;
    }
    }

    binary_data_n2_information6 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information6");
    if (binary_data_n2_information6) {
    decoded_str_binary_data_n2_information6 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information6);
    if (!cJSON_IsString(binary_data_n2_information6)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information6]");
        goto end;
    }
    decoded_str_binary_data_n2_information6->data = OpenAPI_base64decode(binary_data_n2_information6->valuestring, strlen(binary_data_n2_information6->valuestring), &decoded_str_binary_data_n2_information6->len);
    if (!decoded_str_binary_data_n2_information6->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information6]");
        goto end;
    }
    }

    binary_data_n2_information7 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information7");
    if (binary_data_n2_information7) {
    decoded_str_binary_data_n2_information7 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information7);
    if (!cJSON_IsString(binary_data_n2_information7)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information7]");
        goto end;
    }
    decoded_str_binary_data_n2_information7->data = OpenAPI_base64decode(binary_data_n2_information7->valuestring, strlen(binary_data_n2_information7->valuestring), &decoded_str_binary_data_n2_information7->len);
    if (!decoded_str_binary_data_n2_information7->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information7]");
        goto end;
    }
    }

    binary_data_n2_information8 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information8");
    if (binary_data_n2_information8) {
    decoded_str_binary_data_n2_information8 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information8);
    if (!cJSON_IsString(binary_data_n2_information8)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information8]");
        goto end;
    }
    decoded_str_binary_data_n2_information8->data = OpenAPI_base64decode(binary_data_n2_information8->valuestring, strlen(binary_data_n2_information8->valuestring), &decoded_str_binary_data_n2_information8->len);
    if (!decoded_str_binary_data_n2_information8->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information8]");
        goto end;
    }
    }

    binary_data_n2_information9 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information9");
    if (binary_data_n2_information9) {
    decoded_str_binary_data_n2_information9 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information9);
    if (!cJSON_IsString(binary_data_n2_information9)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information9]");
        goto end;
    }
    decoded_str_binary_data_n2_information9->data = OpenAPI_base64decode(binary_data_n2_information9->valuestring, strlen(binary_data_n2_information9->valuestring), &decoded_str_binary_data_n2_information9->len);
    if (!decoded_str_binary_data_n2_information9->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information9]");
        goto end;
    }
    }

    binary_data_n2_information10 = cJSON_GetObjectItemCaseSensitive(_mbs_contexts_post_requestJSON, "binaryDataN2Information10");
    if (binary_data_n2_information10) {
    decoded_str_binary_data_n2_information10 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information10);
    if (!cJSON_IsString(binary_data_n2_information10)) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information10]");
        goto end;
    }
    decoded_str_binary_data_n2_information10->data = OpenAPI_base64decode(binary_data_n2_information10->valuestring, strlen(binary_data_n2_information10->valuestring), &decoded_str_binary_data_n2_information10->len);
    if (!decoded_str_binary_data_n2_information10->data) {
        ogs_error("OpenAPI__mbs_contexts_post_request_parseFromJSON() failed [binary_data_n2_information10]");
        goto end;
    }
    }

    _mbs_contexts_post_request_local_var = OpenAPI__mbs_contexts_post_request_create (
        json_data ? json_data_local_nonprim : NULL,
        binary_data_n2_information1 ? decoded_str_binary_data_n2_information1 : NULL,
        binary_data_n2_information2 ? decoded_str_binary_data_n2_information2 : NULL,
        binary_data_n2_information3 ? decoded_str_binary_data_n2_information3 : NULL,
        binary_data_n2_information4 ? decoded_str_binary_data_n2_information4 : NULL,
        binary_data_n2_information5 ? decoded_str_binary_data_n2_information5 : NULL,
        binary_data_n2_information6 ? decoded_str_binary_data_n2_information6 : NULL,
        binary_data_n2_information7 ? decoded_str_binary_data_n2_information7 : NULL,
        binary_data_n2_information8 ? decoded_str_binary_data_n2_information8 : NULL,
        binary_data_n2_information9 ? decoded_str_binary_data_n2_information9 : NULL,
        binary_data_n2_information10 ? decoded_str_binary_data_n2_information10 : NULL
    );

    return _mbs_contexts_post_request_local_var;
end:
    if (json_data_local_nonprim) {
        OpenAPI_context_status_notification_free(json_data_local_nonprim);
        json_data_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI__mbs_contexts_post_request_t *OpenAPI__mbs_contexts_post_request_copy(OpenAPI__mbs_contexts_post_request_t *dst, OpenAPI__mbs_contexts_post_request_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI__mbs_contexts_post_request_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI__mbs_contexts_post_request_convertToJSON() failed");
        return NULL;
    }

    content = cJSON_Print(item);
    cJSON_Delete(item);

    if (!content) {
        ogs_error("cJSON_Print() failed");
        return NULL;
    }

    item = cJSON_Parse(content);
    ogs_free(content);
    if (!item) {
        ogs_error("cJSON_Parse() failed");
        return NULL;
    }

    OpenAPI__mbs_contexts_post_request_free(dst);
    dst = OpenAPI__mbs_contexts_post_request_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

