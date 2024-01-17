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
#include "context_update_action.h"
#include "global_ran_node_id.h"
#include "mbs_session_id.h"
#include "n2_mbs_sm_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_update_req_data_s OpenAPI_context_update_req_data_t;
typedef enum { OpenAPI_context_update_req_data_LEAVEIND_NULL = 0, OpenAPI_context_update_req_data_LEAVEIND__true } OpenAPI_context_update_req_data_leave_ind_e;

char* OpenAPI_context_update_req_data_leave_ind_ToString(OpenAPI_context_update_req_data_leave_ind_e leave_ind);

OpenAPI_context_update_req_data_leave_ind_e OpenAPI_context_update_req_data_leave_ind_FromString(char* leave_ind);
typedef struct OpenAPI_context_update_req_data_s {
    char *nfc_instance_id;
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    bool is_area_session_id;
    int area_session_id;
    OpenAPI_context_update_action_e requested_action;
    char *dl_tunnel_info;
    struct OpenAPI_n2_mbs_sm_info_s *n2_mbs_sm_info;
    struct OpenAPI_global_ran_node_id_s *ran_node_id;
    OpenAPI_context_update_req_data_leave_ind_e leave_ind;
} OpenAPI_context_update_req_data_t;

OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_create(
    char *nfc_instance_id,
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_context_update_action_e requested_action,
    char *dl_tunnel_info,
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info,
    OpenAPI_global_ran_node_id_t *ran_node_id,
    OpenAPI_context_update_req_data_leave_ind_e leave_ind
);
void OpenAPI_context_update_req_data_free(OpenAPI_context_update_req_data_t *context_update_req_data);
OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_parseFromJSON(cJSON *context_update_req_dataJSON);
cJSON *OpenAPI_context_update_req_data_convertToJSON(OpenAPI_context_update_req_data_t *context_update_req_data);
OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_copy(OpenAPI_context_update_req_data_t *dst, OpenAPI_context_update_req_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_update_req_data_H_ */

