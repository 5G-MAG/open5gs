/*
 * update_rsp_data.h
 *
 * Data within Update Response
 */

#ifndef _OpenAPI_update_rsp_data_H_
#define _OpenAPI_update_rsp_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_update_rsp_data_s OpenAPI_update_rsp_data_t;
#include "ext_mbs_session.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_update_rsp_data_s {
    struct OpenAPI_ext_mbs_session_s *mbs_session;
};

OpenAPI_update_rsp_data_t *OpenAPI_update_rsp_data_create(
    OpenAPI_ext_mbs_session_t *mbs_session
);
void OpenAPI_update_rsp_data_free(OpenAPI_update_rsp_data_t *update_rsp_data);
OpenAPI_update_rsp_data_t *OpenAPI_update_rsp_data_parseFromJSON(cJSON *update_rsp_dataJSON);
cJSON *OpenAPI_update_rsp_data_convertToJSON(OpenAPI_update_rsp_data_t *update_rsp_data);
OpenAPI_update_rsp_data_t *OpenAPI_update_rsp_data_copy(OpenAPI_update_rsp_data_t *dst, OpenAPI_update_rsp_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_update_rsp_data_H_ */

