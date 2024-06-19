/*
 * operation_status.h
 *
 * 
 */

#ifndef _OpenAPI_operation_status_H_
#define _OpenAPI_operation_status_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_operation_status_NULL = 0, OpenAPI_operation_status_MBS_SESSION_START_COMPLETE, OpenAPI_operation_status_MBS_SESSION_START_INCOMPLETE, OpenAPI_operation_status_MBS_SESSION_UPDATE_COMPLETE, OpenAPI_operation_status_MBS_SESSION_UPDATE_INCOMPLETE } OpenAPI_operation_status_e;

char* OpenAPI_operation_status_ToString(OpenAPI_operation_status_e operation_status);

OpenAPI_operation_status_e OpenAPI_operation_status_FromString(char* operation_status);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_operation_status_H_ */

