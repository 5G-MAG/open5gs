/*
 * context_update_200_response.h
 *
 * 
 */

#ifndef _OpenAPI_context_update_200_response_H_
#define _OpenAPI_context_update_200_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "context_update_rsp_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_update_200_response_s OpenAPI_context_update_200_response_t;
typedef struct OpenAPI_context_update_200_response_s {
    struct OpenAPI_context_update_rsp_data_s *json_data;
    OpenAPI_binary_t* binary_data_n2_information;
} OpenAPI_context_update_200_response_t;

OpenAPI_context_update_200_response_t *OpenAPI_context_update_200_response_create(
    OpenAPI_context_update_rsp_data_t *json_data,
    OpenAPI_binary_t* binary_data_n2_information
);
void OpenAPI_context_update_200_response_free(OpenAPI_context_update_200_response_t *context_update_200_response);
OpenAPI_context_update_200_response_t *OpenAPI_context_update_200_response_parseFromJSON(cJSON *context_update_200_responseJSON);
cJSON *OpenAPI_context_update_200_response_convertToJSON(OpenAPI_context_update_200_response_t *context_update_200_response);
OpenAPI_context_update_200_response_t *OpenAPI_context_update_200_response_copy(OpenAPI_context_update_200_response_t *dst, OpenAPI_context_update_200_response_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_update_200_response_H_ */

