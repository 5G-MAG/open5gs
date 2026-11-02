/*
 * mbs_policy_ctxt_data.h
 *
 * MbsPolicyCtxtData of TS 29.537 (Npcf_MBSPolicyControl): the parameters used to request the creation of an MBS Policy Association
 */

#ifndef _OpenAPI_mbs_policy_ctxt_data_H_
#define _OpenAPI_mbs_policy_ctxt_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_session_id.h"
#include "snssai.h"
#include "mbs_service_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_policy_ctxt_data_s OpenAPI_mbs_policy_ctxt_data_t;
typedef struct OpenAPI_mbs_policy_ctxt_data_s {
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    char *dnn;
    struct OpenAPI_snssai_s *snssai;
    bool is_area_sess_pol_id;
    int area_sess_pol_id;
    struct OpenAPI_mbs_service_info_s *mbs_serv_info;
    char *supp_feat;
} OpenAPI_mbs_policy_ctxt_data_t;

OpenAPI_mbs_policy_ctxt_data_t *OpenAPI_mbs_policy_ctxt_data_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    char *dnn,
    OpenAPI_snssai_t *snssai,
    bool is_area_sess_pol_id,
    int area_sess_pol_id,
    OpenAPI_mbs_service_info_t *mbs_serv_info,
    char *supp_feat
);
void OpenAPI_mbs_policy_ctxt_data_free(OpenAPI_mbs_policy_ctxt_data_t *mbs_policy_ctxt_data);
OpenAPI_mbs_policy_ctxt_data_t *OpenAPI_mbs_policy_ctxt_data_parseFromJSON(cJSON *mbs_policy_ctxt_dataJSON);
cJSON *OpenAPI_mbs_policy_ctxt_data_convertToJSON(OpenAPI_mbs_policy_ctxt_data_t *mbs_policy_ctxt_data);
OpenAPI_mbs_policy_ctxt_data_t *OpenAPI_mbs_policy_ctxt_data_copy(OpenAPI_mbs_policy_ctxt_data_t *dst, OpenAPI_mbs_policy_ctxt_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_policy_ctxt_data_H_ */

