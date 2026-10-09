
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_update_200_response.h"

OpenAPI_context_update_200_response_t *OpenAPI_context_update_200_response_create(
    OpenAPI_context_update_rsp_data_t *json_data,
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
    OpenAPI_context_update_200_response_t *context_update_200_response_local_var = ogs_malloc(sizeof(OpenAPI_context_update_200_response_t));
    ogs_assert(context_update_200_response_local_var);

    context_update_200_response_local_var->json_data = json_data;
    context_update_200_response_local_var->binary_data_n2_information1 = binary_data_n2_information1;
    context_update_200_response_local_var->binary_data_n2_information2 = binary_data_n2_information2;
    context_update_200_response_local_var->binary_data_n2_information3 = binary_data_n2_information3;
    context_update_200_response_local_var->binary_data_n2_information4 = binary_data_n2_information4;
    context_update_200_response_local_var->binary_data_n2_information5 = binary_data_n2_information5;
    context_update_200_response_local_var->binary_data_n2_information6 = binary_data_n2_information6;
    context_update_200_response_local_var->binary_data_n2_information7 = binary_data_n2_information7;
    context_update_200_response_local_var->binary_data_n2_information8 = binary_data_n2_information8;
    context_update_200_response_local_var->binary_data_n2_information9 = binary_data_n2_information9;
    context_update_200_response_local_var->binary_data_n2_information10 = binary_data_n2_information10;

    return context_update_200_response_local_var;
}

void OpenAPI_context_update_200_response_free(OpenAPI_context_update_200_response_t *context_update_200_response)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_update_200_response) {
        return;
    }
    if (context_update_200_response->json_data) {
        OpenAPI_context_update_rsp_data_free(context_update_200_response->json_data);
        context_update_200_response->json_data = NULL;
    }
    if (context_update_200_response->binary_data_n2_information1) {
        ogs_free(context_update_200_response->binary_data_n2_information1->data);
        context_update_200_response->binary_data_n2_information1 = NULL;
    }
    if (context_update_200_response->binary_data_n2_information2) {
        ogs_free(context_update_200_response->binary_data_n2_information2->data);
        context_update_200_response->binary_data_n2_information2 = NULL;
    }
    if (context_update_200_response->binary_data_n2_information3) {
        ogs_free(context_update_200_response->binary_data_n2_information3->data);
        context_update_200_response->binary_data_n2_information3 = NULL;
    }
    if (context_update_200_response->binary_data_n2_information4) {
        ogs_free(context_update_200_response->binary_data_n2_information4->data);
        context_update_200_response->binary_data_n2_information4 = NULL;
    }
    if (context_update_200_response->binary_data_n2_information5) {
        ogs_free(context_update_200_response->binary_data_n2_information5->data);
        context_update_200_response->binary_data_n2_information5 = NULL;
    }
    if (context_update_200_response->binary_data_n2_information6) {
        ogs_free(context_update_200_response->binary_data_n2_information6->data);
        context_update_200_response->binary_data_n2_information6 = NULL;
    }
    if (context_update_200_response->binary_data_n2_information7) {
        ogs_free(context_update_200_response->binary_data_n2_information7->data);
        context_update_200_response->binary_data_n2_information7 = NULL;
    }
    if (context_update_200_response->binary_data_n2_information8) {
        ogs_free(context_update_200_response->binary_data_n2_information8->data);
        context_update_200_response->binary_data_n2_information8 = NULL;
    }
    if (context_update_200_response->binary_data_n2_information9) {
        ogs_free(context_update_200_response->binary_data_n2_information9->data);
        context_update_200_response->binary_data_n2_information9 = NULL;
    }
    if (context_update_200_response->binary_data_n2_information10) {
        ogs_free(context_update_200_response->binary_data_n2_information10->data);
        context_update_200_response->binary_data_n2_information10 = NULL;
    }
    ogs_free(context_update_200_response);
}

cJSON *OpenAPI_context_update_200_response_convertToJSON(OpenAPI_context_update_200_response_t *context_update_200_response)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_update_200_response == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [ContextUpdate_200_response]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (context_update_200_response->json_data) {
    cJSON *json_data_local_JSON = OpenAPI_context_update_rsp_data_convertToJSON(context_update_200_response->json_data);
    if (json_data_local_JSON == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [json_data]");
        goto end;
    }
    cJSON_AddItemToObject(item, "jsonData", json_data_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [json_data]");
        goto end;
    }
    }

    if (context_update_200_response->binary_data_n2_information1) {
    char* encoded_str_binary_data_n2_information1 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information1->data,context_update_200_response->binary_data_n2_information1->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information1", encoded_str_binary_data_n2_information1) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information1]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information1);
    }

    if (context_update_200_response->binary_data_n2_information2) {
    char* encoded_str_binary_data_n2_information2 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information2->data,context_update_200_response->binary_data_n2_information2->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information2", encoded_str_binary_data_n2_information2) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information2]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information2);
    }

    if (context_update_200_response->binary_data_n2_information3) {
    char* encoded_str_binary_data_n2_information3 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information3->data,context_update_200_response->binary_data_n2_information3->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information3", encoded_str_binary_data_n2_information3) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information3]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information3);
    }

    if (context_update_200_response->binary_data_n2_information4) {
    char* encoded_str_binary_data_n2_information4 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information4->data,context_update_200_response->binary_data_n2_information4->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information4", encoded_str_binary_data_n2_information4) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information4]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information4);
    }

    if (context_update_200_response->binary_data_n2_information5) {
    char* encoded_str_binary_data_n2_information5 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information5->data,context_update_200_response->binary_data_n2_information5->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information5", encoded_str_binary_data_n2_information5) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information5]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information5);
    }

    if (context_update_200_response->binary_data_n2_information6) {
    char* encoded_str_binary_data_n2_information6 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information6->data,context_update_200_response->binary_data_n2_information6->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information6", encoded_str_binary_data_n2_information6) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information6]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information6);
    }

    if (context_update_200_response->binary_data_n2_information7) {
    char* encoded_str_binary_data_n2_information7 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information7->data,context_update_200_response->binary_data_n2_information7->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information7", encoded_str_binary_data_n2_information7) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information7]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information7);
    }

    if (context_update_200_response->binary_data_n2_information8) {
    char* encoded_str_binary_data_n2_information8 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information8->data,context_update_200_response->binary_data_n2_information8->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information8", encoded_str_binary_data_n2_information8) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information8]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information8);
    }

    if (context_update_200_response->binary_data_n2_information9) {
    char* encoded_str_binary_data_n2_information9 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information9->data,context_update_200_response->binary_data_n2_information9->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information9", encoded_str_binary_data_n2_information9) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information9]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information9);
    }

    if (context_update_200_response->binary_data_n2_information10) {
    char* encoded_str_binary_data_n2_information10 = OpenAPI_base64encode(context_update_200_response->binary_data_n2_information10->data,context_update_200_response->binary_data_n2_information10->len);
    if (cJSON_AddStringToObject(item, "binaryDataN2Information10", encoded_str_binary_data_n2_information10) == NULL) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed [binary_data_n2_information10]");
        goto end;
    }
    ogs_free(encoded_str_binary_data_n2_information10);
    }

end:
    return item;
}

OpenAPI_context_update_200_response_t *OpenAPI_context_update_200_response_parseFromJSON(cJSON *context_update_200_responseJSON)
{
    OpenAPI_context_update_200_response_t *context_update_200_response_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *json_data = NULL;
    OpenAPI_context_update_rsp_data_t *json_data_local_nonprim = NULL;
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
    json_data = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "jsonData");
    if (json_data) {
    json_data_local_nonprim = OpenAPI_context_update_rsp_data_parseFromJSON(json_data);
    if (!json_data_local_nonprim) {
        ogs_error("OpenAPI_context_update_rsp_data_parseFromJSON failed [json_data]");
        goto end;
    }
    }

    binary_data_n2_information1 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information1");
    if (binary_data_n2_information1) {
    decoded_str_binary_data_n2_information1 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information1);
    if (!cJSON_IsString(binary_data_n2_information1)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information1]");
        goto end;
    }
    decoded_str_binary_data_n2_information1->data = OpenAPI_base64decode(binary_data_n2_information1->valuestring, strlen(binary_data_n2_information1->valuestring), &decoded_str_binary_data_n2_information1->len);
    if (!decoded_str_binary_data_n2_information1->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information1]");
        goto end;
    }
    }

    binary_data_n2_information2 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information2");
    if (binary_data_n2_information2) {
    decoded_str_binary_data_n2_information2 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information2);
    if (!cJSON_IsString(binary_data_n2_information2)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information2]");
        goto end;
    }
    decoded_str_binary_data_n2_information2->data = OpenAPI_base64decode(binary_data_n2_information2->valuestring, strlen(binary_data_n2_information2->valuestring), &decoded_str_binary_data_n2_information2->len);
    if (!decoded_str_binary_data_n2_information2->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information2]");
        goto end;
    }
    }

    binary_data_n2_information3 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information3");
    if (binary_data_n2_information3) {
    decoded_str_binary_data_n2_information3 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information3);
    if (!cJSON_IsString(binary_data_n2_information3)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information3]");
        goto end;
    }
    decoded_str_binary_data_n2_information3->data = OpenAPI_base64decode(binary_data_n2_information3->valuestring, strlen(binary_data_n2_information3->valuestring), &decoded_str_binary_data_n2_information3->len);
    if (!decoded_str_binary_data_n2_information3->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information3]");
        goto end;
    }
    }

    binary_data_n2_information4 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information4");
    if (binary_data_n2_information4) {
    decoded_str_binary_data_n2_information4 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information4);
    if (!cJSON_IsString(binary_data_n2_information4)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information4]");
        goto end;
    }
    decoded_str_binary_data_n2_information4->data = OpenAPI_base64decode(binary_data_n2_information4->valuestring, strlen(binary_data_n2_information4->valuestring), &decoded_str_binary_data_n2_information4->len);
    if (!decoded_str_binary_data_n2_information4->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information4]");
        goto end;
    }
    }

    binary_data_n2_information5 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information5");
    if (binary_data_n2_information5) {
    decoded_str_binary_data_n2_information5 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information5);
    if (!cJSON_IsString(binary_data_n2_information5)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information5]");
        goto end;
    }
    decoded_str_binary_data_n2_information5->data = OpenAPI_base64decode(binary_data_n2_information5->valuestring, strlen(binary_data_n2_information5->valuestring), &decoded_str_binary_data_n2_information5->len);
    if (!decoded_str_binary_data_n2_information5->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information5]");
        goto end;
    }
    }

    binary_data_n2_information6 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information6");
    if (binary_data_n2_information6) {
    decoded_str_binary_data_n2_information6 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information6);
    if (!cJSON_IsString(binary_data_n2_information6)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information6]");
        goto end;
    }
    decoded_str_binary_data_n2_information6->data = OpenAPI_base64decode(binary_data_n2_information6->valuestring, strlen(binary_data_n2_information6->valuestring), &decoded_str_binary_data_n2_information6->len);
    if (!decoded_str_binary_data_n2_information6->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information6]");
        goto end;
    }
    }

    binary_data_n2_information7 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information7");
    if (binary_data_n2_information7) {
    decoded_str_binary_data_n2_information7 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information7);
    if (!cJSON_IsString(binary_data_n2_information7)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information7]");
        goto end;
    }
    decoded_str_binary_data_n2_information7->data = OpenAPI_base64decode(binary_data_n2_information7->valuestring, strlen(binary_data_n2_information7->valuestring), &decoded_str_binary_data_n2_information7->len);
    if (!decoded_str_binary_data_n2_information7->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information7]");
        goto end;
    }
    }

    binary_data_n2_information8 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information8");
    if (binary_data_n2_information8) {
    decoded_str_binary_data_n2_information8 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information8);
    if (!cJSON_IsString(binary_data_n2_information8)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information8]");
        goto end;
    }
    decoded_str_binary_data_n2_information8->data = OpenAPI_base64decode(binary_data_n2_information8->valuestring, strlen(binary_data_n2_information8->valuestring), &decoded_str_binary_data_n2_information8->len);
    if (!decoded_str_binary_data_n2_information8->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information8]");
        goto end;
    }
    }

    binary_data_n2_information9 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information9");
    if (binary_data_n2_information9) {
    decoded_str_binary_data_n2_information9 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information9);
    if (!cJSON_IsString(binary_data_n2_information9)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information9]");
        goto end;
    }
    decoded_str_binary_data_n2_information9->data = OpenAPI_base64decode(binary_data_n2_information9->valuestring, strlen(binary_data_n2_information9->valuestring), &decoded_str_binary_data_n2_information9->len);
    if (!decoded_str_binary_data_n2_information9->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information9]");
        goto end;
    }
    }

    binary_data_n2_information10 = cJSON_GetObjectItemCaseSensitive(context_update_200_responseJSON, "binaryDataN2Information10");
    if (binary_data_n2_information10) {
    decoded_str_binary_data_n2_information10 = ogs_malloc(sizeof(OpenAPI_binary_t));
    ogs_assert(decoded_str_binary_data_n2_information10);
    if (!cJSON_IsString(binary_data_n2_information10)) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information10]");
        goto end;
    }
    decoded_str_binary_data_n2_information10->data = OpenAPI_base64decode(binary_data_n2_information10->valuestring, strlen(binary_data_n2_information10->valuestring), &decoded_str_binary_data_n2_information10->len);
    if (!decoded_str_binary_data_n2_information10->data) {
        ogs_error("OpenAPI_context_update_200_response_parseFromJSON() failed [binary_data_n2_information10]");
        goto end;
    }
    }

    context_update_200_response_local_var = OpenAPI_context_update_200_response_create (
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

    return context_update_200_response_local_var;
end:
    if (json_data_local_nonprim) {
        OpenAPI_context_update_rsp_data_free(json_data_local_nonprim);
        json_data_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_context_update_200_response_t *OpenAPI_context_update_200_response_copy(OpenAPI_context_update_200_response_t *dst, OpenAPI_context_update_200_response_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_update_200_response_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_update_200_response_convertToJSON() failed");
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

    OpenAPI_context_update_200_response_free(dst);
    dst = OpenAPI_context_update_200_response_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

