/*
 * ngran_failure_indication_any_of.h
 *
 * 
 */

#ifndef _OpenAPI_ngran_failure_indication_any_of_H_
#define _OpenAPI_ngran_failure_indication_any_of_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_ngran_failure_indication_any_of_NULL = 0, OpenAPI_ngran_failure_indication_any_of_NG_RAN_RESTART_OR_START, OpenAPI_ngran_failure_indication_any_of_NG_RAN_FAILURE_WITHOUT_RESTART, OpenAPI_ngran_failure_indication_any_of_NG_RAN_NOT_REACHABLE, OpenAPI_ngran_failure_indication_any_of_NG_RAN_REQUIRED_RELEASE } OpenAPI_ngran_failure_indication_any_of_e;

char* OpenAPI_ngran_failure_indication_any_of_ToString(OpenAPI_ngran_failure_indication_any_of_e ngran_failure_indication_any_of);

OpenAPI_ngran_failure_indication_any_of_e OpenAPI_ngran_failure_indication_any_of_FromString(char* ngran_failure_indication_any_of);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_ngran_failure_indication_any_of_H_ */

