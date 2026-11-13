/*
 * create_req_data.h
 *
 * Data within Create Request
 */

#ifndef _OpenAPI_create_req_data_H_
#define _OpenAPI_create_req_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_create_req_data_s OpenAPI_create_req_data_t;
#include "ext_mbs_session.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_create_req_data_s {
    struct OpenAPI_ext_mbs_session_s *mbs_session;
    char *supported_features;
};

OpenAPI_create_req_data_t *OpenAPI_create_req_data_create(
    OpenAPI_ext_mbs_session_t *mbs_session,
    char *supported_features
);
void OpenAPI_create_req_data_free(OpenAPI_create_req_data_t *create_req_data);
OpenAPI_create_req_data_t *OpenAPI_create_req_data_parseFromJSON(cJSON *create_req_dataJSON);
cJSON *OpenAPI_create_req_data_convertToJSON(OpenAPI_create_req_data_t *create_req_data);
OpenAPI_create_req_data_t *OpenAPI_create_req_data_copy(OpenAPI_create_req_data_t *dst, OpenAPI_create_req_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_create_req_data_H_ */

