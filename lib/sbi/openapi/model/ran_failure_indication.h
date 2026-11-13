/*
 * ran_failure_indication.h
 *
 * Indicates a NG-RAN failure event
 */

#ifndef _OpenAPI_ran_failure_indication_H_
#define _OpenAPI_ran_failure_indication_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_ran_failure_indication_NULL = 0, OpenAPI_ran_failure_indication_NG_RAN_FAILURE_WITHOUT_RESTART, OpenAPI_ran_failure_indication_NG_RAN_NOT_REACHABLE } OpenAPI_ran_failure_indication_e;

char* OpenAPI_ran_failure_indication_ToString(OpenAPI_ran_failure_indication_e ran_failure_indication);

OpenAPI_ran_failure_indication_e OpenAPI_ran_failure_indication_FromString(char* ran_failure_indication);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_ran_failure_indication_H_ */

