/*
 * context_update_req_data.h
 *
 * Data within ContextUpdate Request
 */

#ifndef _OpenAPI_context_update_req_data_H_
#define _OpenAPI_context_update_req_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "global_ran_node_id.h"
#include "mbs_service_area.h"
#include "mbs_service_area_info.h"
#include "n2_mbs_sm_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_update_req_data_s OpenAPI_context_update_req_data_t;
typedef enum { OpenAPI_context_update_req_data_NONGAPSIGNALLINGIND_NULL = 0, OpenAPI_context_update_req_data_NONGAPSIGNALLINGIND__true } OpenAPI_context_update_req_data_no_ngap_signalling_ind_e;

char* OpenAPI_context_update_req_data_no_ngap_signalling_ind_ToString(OpenAPI_context_update_req_data_no_ngap_signalling_ind_e no_ngap_signalling_ind);

OpenAPI_context_update_req_data_no_ngap_signalling_ind_e OpenAPI_context_update_req_data_no_ngap_signalling_ind_FromString(char* no_ngap_signalling_ind);
typedef struct OpenAPI_context_update_req_data_s {
    struct OpenAPI_mbs_service_area_s *mbs_service_area;
    OpenAPI_list_t *mbs_service_area_info_list;
    struct OpenAPI_n2_mbs_sm_info_s *n2_mbs_sm_info;
    OpenAPI_list_t *ran_id_list;
    OpenAPI_context_update_req_data_no_ngap_signalling_ind_e no_ngap_signalling_ind;
    char *notify_uri;
    bool is_max_response_time;
    int max_response_time;
    bool is_n2_mbs_info_change_ind;
    int n2_mbs_info_change_ind;
} OpenAPI_context_update_req_data_t;

OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_create(
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_list_t *mbs_service_area_info_list,
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info,
    OpenAPI_list_t *ran_id_list,
    OpenAPI_context_update_req_data_no_ngap_signalling_ind_e no_ngap_signalling_ind,
    char *notify_uri,
    bool is_max_response_time,
    int max_response_time,
    bool is_n2_mbs_info_change_ind,
    int n2_mbs_info_change_ind
);
void OpenAPI_context_update_req_data_free(OpenAPI_context_update_req_data_t *context_update_req_data);
OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_parseFromJSON(cJSON *context_update_req_dataJSON);
cJSON *OpenAPI_context_update_req_data_convertToJSON(OpenAPI_context_update_req_data_t *context_update_req_data);
OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_copy(OpenAPI_context_update_req_data_t *dst, OpenAPI_context_update_req_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_update_req_data_H_ */

