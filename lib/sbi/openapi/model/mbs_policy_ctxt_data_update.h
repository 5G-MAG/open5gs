/*
 * mbs_policy_ctxt_data_update.h
 *
 * MbsPolicyCtxtDataUpdate of TS 29.537 (Npcf_MBSPolicyControl): the parameters to request the modification of an MBS Policy Association (mbsErrorReport is not modelled)
 */

#ifndef _OpenAPI_mbs_policy_ctxt_data_update_H_
#define _OpenAPI_mbs_policy_ctxt_data_update_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_service_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_policy_ctxt_data_update_s OpenAPI_mbs_policy_ctxt_data_update_t;
typedef struct OpenAPI_mbs_policy_ctxt_data_update_s {
    struct OpenAPI_mbs_service_info_s *mbs_serv_info;
    OpenAPI_list_t *mbs_pcrts;
} OpenAPI_mbs_policy_ctxt_data_update_t;

OpenAPI_mbs_policy_ctxt_data_update_t *OpenAPI_mbs_policy_ctxt_data_update_create(
    OpenAPI_mbs_service_info_t *mbs_serv_info,
    OpenAPI_list_t *mbs_pcrts
);
void OpenAPI_mbs_policy_ctxt_data_update_free(OpenAPI_mbs_policy_ctxt_data_update_t *mbs_policy_ctxt_data_update);
OpenAPI_mbs_policy_ctxt_data_update_t *OpenAPI_mbs_policy_ctxt_data_update_parseFromJSON(cJSON *mbs_policy_ctxt_data_updateJSON);
cJSON *OpenAPI_mbs_policy_ctxt_data_update_convertToJSON(OpenAPI_mbs_policy_ctxt_data_update_t *mbs_policy_ctxt_data_update);
OpenAPI_mbs_policy_ctxt_data_update_t *OpenAPI_mbs_policy_ctxt_data_update_copy(OpenAPI_mbs_policy_ctxt_data_update_t *dst, OpenAPI_mbs_policy_ctxt_data_update_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_policy_ctxt_data_update_H_ */

