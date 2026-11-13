/*
 * mbs_failure_code.h
 *
 * 
 */

#ifndef _OpenAPI_mbs_failure_code_H_
#define _OpenAPI_mbs_failure_code_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_mbs_failure_code_NULL = 0, OpenAPI_mbs_failure_code_NF_MALFUNCTION, OpenAPI_mbs_failure_code_NF_RESOURCES_UNAVAILABLE, OpenAPI_mbs_failure_code_RESOURCE_ALLOCATION_FAILURE, OpenAPI_mbs_failure_code_MBS_QOS_VALIDATION_FAILURE, OpenAPI_mbs_failure_code_NO_MBS_QOS_FLOW, OpenAPI_mbs_failure_code_MBS_QOS_DECISION_ERROR, OpenAPI_mbs_failure_code_MBS_POLICY_PARAM_ERROR } OpenAPI_mbs_failure_code_e;

char* OpenAPI_mbs_failure_code_ToString(OpenAPI_mbs_failure_code_e mbs_failure_code);

OpenAPI_mbs_failure_code_e OpenAPI_mbs_failure_code_FromString(char* mbs_failure_code);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_failure_code_H_ */

