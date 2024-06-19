/*
 * ngran_failure_indication.h
 *
 * 
 */

#ifndef _OpenAPI_ngran_failure_indication_H_
#define _OpenAPI_ngran_failure_indication_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_ngran_failure_indication_NULL = 0, OpenAPI_ngran_failure_indication_NG_RAN_RESTART_OR_START, OpenAPI_ngran_failure_indication_NG_RAN_FAILURE_WITHOUT_RESTART, OpenAPI_ngran_failure_indication_NG_RAN_NOT_REACHABLE, OpenAPI_ngran_failure_indication_NG_RAN_REQUIRED_RELEASE } OpenAPI_ngran_failure_indication_e;

char* OpenAPI_ngran_failure_indication_ToString(OpenAPI_ngran_failure_indication_e ngran_failure_indication);

OpenAPI_ngran_failure_indication_e OpenAPI_ngran_failure_indication_FromString(char* ngran_failure_indication);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_ngran_failure_indication_H_ */

