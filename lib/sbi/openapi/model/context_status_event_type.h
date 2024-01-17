/*
 * context_status_event_type.h
 *
 * 
 */

#ifndef _OpenAPI_context_status_event_type_H_
#define _OpenAPI_context_status_event_type_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_context_status_event_type_NULL = 0, OpenAPI_context_status_event_type_QOS_INFO, OpenAPI_context_status_event_type_STATUS_INFO, OpenAPI_context_status_event_type_SERVICE_AREA_INFO, OpenAPI_context_status_event_type_SESSION_RELEASE, OpenAPI_context_status_event_type_MULT_TRANS_ADD_CHANGE, OpenAPI_context_status_event_type_SECURITY_INFO } OpenAPI_context_status_event_type_e;

char* OpenAPI_context_status_event_type_ToString(OpenAPI_context_status_event_type_e context_status_event_type);

OpenAPI_context_status_event_type_e OpenAPI_context_status_event_type_FromString(char* context_status_event_type);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_status_event_type_H_ */

