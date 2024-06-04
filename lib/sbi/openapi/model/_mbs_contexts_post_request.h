/*
 * _mbs_contexts_post_request.h
 *
 * 
 */

#ifndef _OpenAPI__mbs_contexts_post_request_H_
#define _OpenAPI__mbs_contexts_post_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "context_status_notification.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI__mbs_contexts_post_request_s OpenAPI__mbs_contexts_post_request_t;
typedef struct OpenAPI__mbs_contexts_post_request_s {
    struct OpenAPI_context_status_notification_s *json_data;
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
} OpenAPI__mbs_contexts_post_request_t;

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
);
void OpenAPI__mbs_contexts_post_request_free(OpenAPI__mbs_contexts_post_request_t *_mbs_contexts_post_request);
OpenAPI__mbs_contexts_post_request_t *OpenAPI__mbs_contexts_post_request_parseFromJSON(cJSON *_mbs_contexts_post_requestJSON);
cJSON *OpenAPI__mbs_contexts_post_request_convertToJSON(OpenAPI__mbs_contexts_post_request_t *_mbs_contexts_post_request);
OpenAPI__mbs_contexts_post_request_t *OpenAPI__mbs_contexts_post_request_copy(OpenAPI__mbs_contexts_post_request_t *dst, OpenAPI__mbs_contexts_post_request_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI__mbs_contexts_post_request_H_ */

