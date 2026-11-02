/*
 * mbs_policy_data.h
 *
 * MbsPolicyData of TS 29.537 (Npcf_MBSPolicyControl): the MBS policy data provisioned as part of an MBS Policy Association (mbsPolicies, the MbsPolicyDecision, is carried as the JSON document)
 */

#ifndef _OpenAPI_mbs_policy_data_H_
#define _OpenAPI_mbs_policy_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_policy_ctxt_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_policy_data_s OpenAPI_mbs_policy_data_t;
typedef struct OpenAPI_mbs_policy_data_s {
    struct OpenAPI_mbs_policy_ctxt_data_s *mbs_policy_ctxt_data;
    cJSON *mbs_policies;
    char *supp_feat;
} OpenAPI_mbs_policy_data_t;

OpenAPI_mbs_policy_data_t *OpenAPI_mbs_policy_data_create(
    OpenAPI_mbs_policy_ctxt_data_t *mbs_policy_ctxt_data,
    cJSON *mbs_policies,
    char *supp_feat
);
void OpenAPI_mbs_policy_data_free(OpenAPI_mbs_policy_data_t *mbs_policy_data);
OpenAPI_mbs_policy_data_t *OpenAPI_mbs_policy_data_parseFromJSON(cJSON *mbs_policy_dataJSON);
cJSON *OpenAPI_mbs_policy_data_convertToJSON(OpenAPI_mbs_policy_data_t *mbs_policy_data);
OpenAPI_mbs_policy_data_t *OpenAPI_mbs_policy_data_copy(OpenAPI_mbs_policy_data_t *dst, OpenAPI_mbs_policy_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_policy_data_H_ */

