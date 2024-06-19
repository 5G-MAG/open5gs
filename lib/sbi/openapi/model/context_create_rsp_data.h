/*
 * context_create_rsp_data.h
 *
 * Data within ContextCreate Response
 */

#ifndef _OpenAPI_context_create_rsp_data_H_
#define _OpenAPI_context_create_rsp_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_session_id.h"
#include "n2_mbs_sm_info.h"
#include "operation_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_create_rsp_data_s OpenAPI_context_create_rsp_data_t;
typedef struct OpenAPI_context_create_rsp_data_s {
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    OpenAPI_list_t *n2_mbs_sm_info_list;
    OpenAPI_operation_status_e operation_status;
} OpenAPI_context_create_rsp_data_t;

OpenAPI_context_create_rsp_data_t *OpenAPI_context_create_rsp_data_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    OpenAPI_list_t *n2_mbs_sm_info_list,
    OpenAPI_operation_status_e operation_status
);
void OpenAPI_context_create_rsp_data_free(OpenAPI_context_create_rsp_data_t *context_create_rsp_data);
OpenAPI_context_create_rsp_data_t *OpenAPI_context_create_rsp_data_parseFromJSON(cJSON *context_create_rsp_dataJSON);
cJSON *OpenAPI_context_create_rsp_data_convertToJSON(OpenAPI_context_create_rsp_data_t *context_create_rsp_data);
OpenAPI_context_create_rsp_data_t *OpenAPI_context_create_rsp_data_copy(OpenAPI_context_create_rsp_data_t *dst, OpenAPI_context_create_rsp_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_create_rsp_data_H_ */

