/*
 * operation_status.h
 *
 * Status of a Broadcast MBS session start or update operation.
 */

#ifndef _OpenAPI_operation_status_H_
#define _OpenAPI_operation_status_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "operation_status_any_of.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_operation_status_s OpenAPI_operation_status_t;
typedef struct OpenAPI_operation_status_s {
} OpenAPI_operation_status_t;

OpenAPI_operation_status_t *OpenAPI_operation_status_create(
);
void OpenAPI_operation_status_free(OpenAPI_operation_status_t *operation_status);
OpenAPI_operation_status_t *OpenAPI_operation_status_parseFromJSON(cJSON *operation_statusJSON);
cJSON *OpenAPI_operation_status_convertToJSON(OpenAPI_operation_status_t *operation_status);
OpenAPI_operation_status_t *OpenAPI_operation_status_copy(OpenAPI_operation_status_t *dst, OpenAPI_operation_status_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_operation_status_H_ */

