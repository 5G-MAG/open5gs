/*
 * _mbs_contexts_post_200_response.h
 *
 * 
 */

#ifndef _OpenAPI__mbs_contexts_post_200_response_H_
#define _OpenAPI__mbs_contexts_post_200_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI__mbs_contexts_post_200_response_s OpenAPI__mbs_contexts_post_200_response_t;
#include "context_status_notification_response.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI__mbs_contexts_post_200_response_s {
    struct OpenAPI_context_status_notification_response_s *json_data;
    OpenAPI_binary_t* binary_data_n2_information1;
    OpenAPI_binary_t* binary_data_n2_information2;
    OpenAPI_binary_t* binary_data_n2_information3;
    OpenAPI_binary_t* binary_data_n2_information4;
    OpenAPI_binary_t* binary_data_n2_information5;
    OpenAPI_binary_t* binary_data_n2_information6;
    OpenAPI_binary_t* binary_data_n2_information7;
    OpenAPI_binary_t* binary_data_n2_information8;
    OpenAPI_binary_t* binary_data_n2_information9;
    OpenAPI_binary_t* binary_data_n2_information10;
};

OpenAPI__mbs_contexts_post_200_response_t *OpenAPI__mbs_contexts_post_200_response_create(
    OpenAPI_context_status_notification_response_t *json_data,
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
);
void OpenAPI__mbs_contexts_post_200_response_free(OpenAPI__mbs_contexts_post_200_response_t *_mbs_contexts_post_200_response);
OpenAPI__mbs_contexts_post_200_response_t *OpenAPI__mbs_contexts_post_200_response_parseFromJSON(cJSON *_mbs_contexts_post_200_responseJSON);
cJSON *OpenAPI__mbs_contexts_post_200_response_convertToJSON(OpenAPI__mbs_contexts_post_200_response_t *_mbs_contexts_post_200_response);
OpenAPI__mbs_contexts_post_200_response_t *OpenAPI__mbs_contexts_post_200_response_copy(OpenAPI__mbs_contexts_post_200_response_t *dst, OpenAPI__mbs_contexts_post_200_response_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI__mbs_contexts_post_200_response_H_ */

