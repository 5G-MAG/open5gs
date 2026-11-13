/*
 * mbs_pcc_rule.h
 *
 * Represents the parameters constituting an MBS PCC rule.
 */

#ifndef _OpenAPI_mbs_pcc_rule_H_
#define _OpenAPI_mbs_pcc_rule_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_pcc_rule_s OpenAPI_mbs_pcc_rule_t;

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_pcc_rule_s {
    char *mbs_pcc_rule_id;
    OpenAPI_list_t *mbs_dl_ip_flow_info;
    bool is_precedence;
    int precedence;
    OpenAPI_list_t *ref_mbs_qos_dec;
    bool is_ref_mbs_chg_data_null;
    OpenAPI_list_t *ref_mbs_chg_data;
};

OpenAPI_mbs_pcc_rule_t *OpenAPI_mbs_pcc_rule_create(
    char *mbs_pcc_rule_id,
    OpenAPI_list_t *mbs_dl_ip_flow_info,
    bool is_precedence,
    int precedence,
    OpenAPI_list_t *ref_mbs_qos_dec,
    bool is_ref_mbs_chg_data_null,
    OpenAPI_list_t *ref_mbs_chg_data
);
void OpenAPI_mbs_pcc_rule_free(OpenAPI_mbs_pcc_rule_t *mbs_pcc_rule);
OpenAPI_mbs_pcc_rule_t *OpenAPI_mbs_pcc_rule_parseFromJSON(cJSON *mbs_pcc_ruleJSON);
cJSON *OpenAPI_mbs_pcc_rule_convertToJSON(OpenAPI_mbs_pcc_rule_t *mbs_pcc_rule);
OpenAPI_mbs_pcc_rule_t *OpenAPI_mbs_pcc_rule_copy(OpenAPI_mbs_pcc_rule_t *dst, OpenAPI_mbs_pcc_rule_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_pcc_rule_H_ */

