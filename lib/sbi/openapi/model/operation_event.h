/*
 * operation_event.h
 *
 * Operation Event for a Broadcast MBS Session.
 */

#ifndef _OpenAPI_operation_event_H_
#define _OpenAPI_operation_event_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "ngran_failure_event.h"
#include "op_event_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_operation_event_s OpenAPI_operation_event_t;
typedef struct OpenAPI_operation_event_s {
    OpenAPI_op_event_type_e op_event_type;
    char *amf_id;
    OpenAPI_list_t *ngran_failure_event_list;
} OpenAPI_operation_event_t;

OpenAPI_operation_event_t *OpenAPI_operation_event_create(
    OpenAPI_op_event_type_e op_event_type,
    char *amf_id,
    OpenAPI_list_t *ngran_failure_event_list
);
void OpenAPI_operation_event_free(OpenAPI_operation_event_t *operation_event);
OpenAPI_operation_event_t *OpenAPI_operation_event_parseFromJSON(cJSON *operation_eventJSON);
cJSON *OpenAPI_operation_event_convertToJSON(OpenAPI_operation_event_t *operation_event);
OpenAPI_operation_event_t *OpenAPI_operation_event_copy(OpenAPI_operation_event_t *dst, OpenAPI_operation_event_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_operation_event_H_ */

