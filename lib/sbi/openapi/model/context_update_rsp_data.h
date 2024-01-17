/*
 * context_update_rsp_data.h
 *
 * Data within ContextUpdate Response
 */

#ifndef _OpenAPI_context_update_rsp_data_H_
#define _OpenAPI_context_update_rsp_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "n2_mbs_sm_info.h"
#include "ssm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_update_rsp_data_s OpenAPI_context_update_rsp_data_t;
typedef struct OpenAPI_context_update_rsp_data_s {
    struct OpenAPI_ssm_s *ll_ssm;
    bool is_c_teid;
    int c_teid;
    struct OpenAPI_n2_mbs_sm_info_s *n2_mbs_sm_info;
} OpenAPI_context_update_rsp_data_t;

OpenAPI_context_update_rsp_data_t *OpenAPI_context_update_rsp_data_create(
    OpenAPI_ssm_t *ll_ssm,
    bool is_c_teid,
    int c_teid,
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info
);
void OpenAPI_context_update_rsp_data_free(OpenAPI_context_update_rsp_data_t *context_update_rsp_data);
OpenAPI_context_update_rsp_data_t *OpenAPI_context_update_rsp_data_parseFromJSON(cJSON *context_update_rsp_dataJSON);
cJSON *OpenAPI_context_update_rsp_data_convertToJSON(OpenAPI_context_update_rsp_data_t *context_update_rsp_data);
OpenAPI_context_update_rsp_data_t *OpenAPI_context_update_rsp_data_copy(OpenAPI_context_update_rsp_data_t *dst, OpenAPI_context_update_rsp_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_update_rsp_data_H_ */

