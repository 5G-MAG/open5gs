/*
 * mbs_n2_message_transfer_req_data.h
 *
 * Data within MBS N2 Message Transfer Request
 */

#ifndef _OpenAPI_mbs_n2_message_transfer_req_data_H_
#define _OpenAPI_mbs_n2_message_transfer_req_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_n2_message_transfer_req_data_s OpenAPI_mbs_n2_message_transfer_req_data_t;
#include "global_ran_node_id.h"
#include "mbs_session_id.h"
#include "n2_mbs_sm_info.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_n2_message_transfer_req_data_s {
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    bool is_area_session_id;
    int area_session_id;
    struct OpenAPI_n2_mbs_sm_info_s *n2_mbs_sm_info;
    char *supported_features;
    OpenAPI_list_t *ran_node_id_list;
    char *notify_uri;
    char *notify_correlation_id;
};

OpenAPI_mbs_n2_message_transfer_req_data_t *OpenAPI_mbs_n2_message_transfer_req_data_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info,
    char *supported_features,
    OpenAPI_list_t *ran_node_id_list,
    char *notify_uri,
    char *notify_correlation_id
);
void OpenAPI_mbs_n2_message_transfer_req_data_free(OpenAPI_mbs_n2_message_transfer_req_data_t *mbs_n2_message_transfer_req_data);
OpenAPI_mbs_n2_message_transfer_req_data_t *OpenAPI_mbs_n2_message_transfer_req_data_parseFromJSON(cJSON *mbs_n2_message_transfer_req_dataJSON);
cJSON *OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON(OpenAPI_mbs_n2_message_transfer_req_data_t *mbs_n2_message_transfer_req_data);
OpenAPI_mbs_n2_message_transfer_req_data_t *OpenAPI_mbs_n2_message_transfer_req_data_copy(OpenAPI_mbs_n2_message_transfer_req_data_t *dst, OpenAPI_mbs_n2_message_transfer_req_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_n2_message_transfer_req_data_H_ */

