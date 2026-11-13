/*
 * mbs_pcc_rule_status.h
 *
 * 
 */

#ifndef _OpenAPI_mbs_pcc_rule_status_H_
#define _OpenAPI_mbs_pcc_rule_status_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_mbs_pcc_rule_status_NULL = 0, OpenAPI_mbs_pcc_rule_status_ACTIVE, OpenAPI_mbs_pcc_rule_status_INACTIVE } OpenAPI_mbs_pcc_rule_status_e;

char* OpenAPI_mbs_pcc_rule_status_ToString(OpenAPI_mbs_pcc_rule_status_e mbs_pcc_rule_status);

OpenAPI_mbs_pcc_rule_status_e OpenAPI_mbs_pcc_rule_status_FromString(char* mbs_pcc_rule_status);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_pcc_rule_status_H_ */

