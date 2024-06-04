/*
 * context_create_req_data.h
 *
 * Data within ContextCreate Request
 */

#ifndef _OpenAPI_context_create_req_data_H_
#define _OpenAPI_context_create_req_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_service_area.h"
#include "mbs_service_area_info.h"
#include "mbs_session_id.h"
#include "n2_mbs_sm_info.h"
#include "snssai.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_create_req_data_s OpenAPI_context_create_req_data_t;
typedef struct OpenAPI_context_create_req_data_s {
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    OpenAPI_list_t *mbs_service_area_info_list;
    struct OpenAPI_mbs_service_area_s *mbs_service_area;
    struct OpenAPI_n2_mbs_sm_info_s *n2_mbs_sm_info;
    char *notify_uri;
    bool is_max_response_time;
    int max_response_time;
    struct OpenAPI_snssai_s *snssai;
    char *mbsmf_id;
    char *mbsmf_service_inst_id;
} OpenAPI_context_create_req_data_t;

OpenAPI_context_create_req_data_t *OpenAPI_context_create_req_data_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    OpenAPI_list_t *mbs_service_area_info_list,
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info,
    char *notify_uri,
    bool is_max_response_time,
    int max_response_time,
    OpenAPI_snssai_t *snssai,
    char *mbsmf_id,
    char *mbsmf_service_inst_id
);
void OpenAPI_context_create_req_data_free(OpenAPI_context_create_req_data_t *context_create_req_data);
OpenAPI_context_create_req_data_t *OpenAPI_context_create_req_data_parseFromJSON(cJSON *context_create_req_dataJSON);
cJSON *OpenAPI_context_create_req_data_convertToJSON(OpenAPI_context_create_req_data_t *context_create_req_data);
OpenAPI_context_create_req_data_t *OpenAPI_context_create_req_data_copy(OpenAPI_context_create_req_data_t *dst, OpenAPI_context_create_req_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_create_req_data_H_ */

