/*
 * mbs_report.h
 *
 * Contains information about the MBS Policy Decision level failure(s) and/or the MBS PCC rule level failure(s). 
 */

#ifndef _OpenAPI_mbs_report_H_
#define _OpenAPI_mbs_report_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_report_s OpenAPI_mbs_report_t;
#include "mbs_failure_code.h"
#include "mbs_pcc_rule_status.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_report_s {
    OpenAPI_list_t *mbs_pcc_rule_ids;
    OpenAPI_mbs_pcc_rule_status_e mbs_pcc_rule_status;
    OpenAPI_mbs_failure_code_e failure_code;
};

OpenAPI_mbs_report_t *OpenAPI_mbs_report_create(
    OpenAPI_list_t *mbs_pcc_rule_ids,
    OpenAPI_mbs_pcc_rule_status_e mbs_pcc_rule_status,
    OpenAPI_mbs_failure_code_e failure_code
);
void OpenAPI_mbs_report_free(OpenAPI_mbs_report_t *mbs_report);
OpenAPI_mbs_report_t *OpenAPI_mbs_report_parseFromJSON(cJSON *mbs_reportJSON);
cJSON *OpenAPI_mbs_report_convertToJSON(OpenAPI_mbs_report_t *mbs_report);
OpenAPI_mbs_report_t *OpenAPI_mbs_report_copy(OpenAPI_mbs_report_t *dst, OpenAPI_mbs_report_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_report_H_ */

