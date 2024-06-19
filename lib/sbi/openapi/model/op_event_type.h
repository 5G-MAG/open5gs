/*
 * op_event_type.h
 *
 * 
 */

#ifndef _OpenAPI_op_event_type_H_
#define _OpenAPI_op_event_type_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_op_event_type_NULL = 0, OpenAPI_op_event_type_AMF_CHANGE, OpenAPI_op_event_type_NG_RAN_EVENT } OpenAPI_op_event_type_e;

char* OpenAPI_op_event_type_ToString(OpenAPI_op_event_type_e op_event_type);

OpenAPI_op_event_type_e OpenAPI_op_event_type_FromString(char* op_event_type);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_op_event_type_H_ */

