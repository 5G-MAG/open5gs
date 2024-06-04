/*
 * op_event_type.h
 *
 * Operation Event Type.
 */

#ifndef _OpenAPI_op_event_type_H_
#define _OpenAPI_op_event_type_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "op_event_type_any_of.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_op_event_type_s OpenAPI_op_event_type_t;
typedef struct OpenAPI_op_event_type_s {
} OpenAPI_op_event_type_t;

OpenAPI_op_event_type_t *OpenAPI_op_event_type_create(
);
void OpenAPI_op_event_type_free(OpenAPI_op_event_type_t *op_event_type);
OpenAPI_op_event_type_t *OpenAPI_op_event_type_parseFromJSON(cJSON *op_event_typeJSON);
cJSON *OpenAPI_op_event_type_convertToJSON(OpenAPI_op_event_type_t *op_event_type);
OpenAPI_op_event_type_t *OpenAPI_op_event_type_copy(OpenAPI_op_event_type_t *dst, OpenAPI_op_event_type_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_op_event_type_H_ */

