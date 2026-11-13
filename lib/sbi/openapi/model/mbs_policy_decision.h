/*
 * mbs_policy_decision.h
 *
 * Represents the parameters constituting an MBS Policy Decision. 
 */

#ifndef _OpenAPI_mbs_policy_decision_H_
#define _OpenAPI_mbs_policy_decision_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_policy_decision_s OpenAPI_mbs_policy_decision_t;
#include "mbs_charging_data.h"
#include "mbs_pcc_rule.h"
#include "mbs_pcrt.h"
#include "mbs_qos_char.h"
#include "mbs_qos_dec.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_policy_decision_s {
    bool is_mbs_pcc_rules_null;
    OpenAPI_list_t* mbs_pcc_rules;
    OpenAPI_list_t* mbs_qos_decs;
    OpenAPI_list_t* mbs_qos_chars;
    char *auth_mbs_sess_ambr;
    bool is_mbs_pcrts_null;
    OpenAPI_list_t *mbs_pcrts;
    bool is_mbs_charging_data_null;
    OpenAPI_list_t* mbs_charging_data;
};

OpenAPI_mbs_policy_decision_t *OpenAPI_mbs_policy_decision_create(
    bool is_mbs_pcc_rules_null,
    OpenAPI_list_t* mbs_pcc_rules,
    OpenAPI_list_t* mbs_qos_decs,
    OpenAPI_list_t* mbs_qos_chars,
    char *auth_mbs_sess_ambr,
    bool is_mbs_pcrts_null,
    OpenAPI_list_t *mbs_pcrts,
    bool is_mbs_charging_data_null,
    OpenAPI_list_t* mbs_charging_data
);
void OpenAPI_mbs_policy_decision_free(OpenAPI_mbs_policy_decision_t *mbs_policy_decision);
OpenAPI_mbs_policy_decision_t *OpenAPI_mbs_policy_decision_parseFromJSON(cJSON *mbs_policy_decisionJSON);
cJSON *OpenAPI_mbs_policy_decision_convertToJSON(OpenAPI_mbs_policy_decision_t *mbs_policy_decision);
OpenAPI_mbs_policy_decision_t *OpenAPI_mbs_policy_decision_copy(OpenAPI_mbs_policy_decision_t *dst, OpenAPI_mbs_policy_decision_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_policy_decision_H_ */

