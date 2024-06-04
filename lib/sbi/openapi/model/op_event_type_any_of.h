/*
 * op_event_type_any_of.h
 *
 * 
 */

#ifndef _OpenAPI_op_event_type_any_of_H_
#define _OpenAPI_op_event_type_any_of_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_op_event_type_any_of_NULL = 0, OpenAPI_op_event_type_any_of_AMF_CHANGE, OpenAPI_op_event_type_any_of_NG_RAN_EVENT } OpenAPI_op_event_type_any_of_e;

char* OpenAPI_op_event_type_any_of_ToString(OpenAPI_op_event_type_any_of_e op_event_type_any_of);

OpenAPI_op_event_type_any_of_e OpenAPI_op_event_type_any_of_FromString(char* op_event_type_any_of);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_op_event_type_any_of_H_ */

