/*
 * context_update_request.h
 *
 * 
 */

#ifndef _OpenAPI_context_update_request_H_
#define _OpenAPI_context_update_request_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "context_update_req_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_update_request_s OpenAPI_context_update_request_t;
typedef struct OpenAPI_context_update_request_s {
    struct OpenAPI_context_update_req_data_s *json_data;
    OpenAPI_binary_t* binary_data_n2_information;
} OpenAPI_context_update_request_t;

OpenAPI_context_update_request_t *OpenAPI_context_update_request_create(
    OpenAPI_context_update_req_data_t *json_data,
    OpenAPI_binary_t* binary_data_n2_information
);
void OpenAPI_context_update_request_free(OpenAPI_context_update_request_t *context_update_request);
OpenAPI_context_update_request_t *OpenAPI_context_update_request_parseFromJSON(cJSON *context_update_requestJSON);
cJSON *OpenAPI_context_update_request_convertToJSON(OpenAPI_context_update_request_t *context_update_request);
OpenAPI_context_update_request_t *OpenAPI_context_update_request_copy(OpenAPI_context_update_request_t *dst, OpenAPI_context_update_request_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_update_request_H_ */

