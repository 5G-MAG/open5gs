/*
 * context_update_action.h
 *
 * 
 */

#ifndef _OpenAPI_context_update_action_H_
#define _OpenAPI_context_update_action_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_context_update_action_NULL = 0, OpenAPI_context_update_action_START, OpenAPI_context_update_action_TERMINATE } OpenAPI_context_update_action_e;

char* OpenAPI_context_update_action_ToString(OpenAPI_context_update_action_e context_update_action);

OpenAPI_context_update_action_e OpenAPI_context_update_action_FromString(char* context_update_action);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_update_action_H_ */

