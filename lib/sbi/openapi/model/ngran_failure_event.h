/*
 * ngran_failure_event.h
 *
 * NG-RAN failure event for a NG-RAN
 */

#ifndef _OpenAPI_ngran_failure_event_H_
#define _OpenAPI_ngran_failure_event_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "global_ran_node_id.h"
#include "ngran_failure_indication.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_ngran_failure_event_s OpenAPI_ngran_failure_event_t;
typedef struct OpenAPI_ngran_failure_event_s {
    struct OpenAPI_global_ran_node_id_s *ngran_id;
    OpenAPI_ngran_failure_indication_e ngran_failure_indication;
} OpenAPI_ngran_failure_event_t;

OpenAPI_ngran_failure_event_t *OpenAPI_ngran_failure_event_create(
    OpenAPI_global_ran_node_id_t *ngran_id,
    OpenAPI_ngran_failure_indication_e ngran_failure_indication
);
void OpenAPI_ngran_failure_event_free(OpenAPI_ngran_failure_event_t *ngran_failure_event);
OpenAPI_ngran_failure_event_t *OpenAPI_ngran_failure_event_parseFromJSON(cJSON *ngran_failure_eventJSON);
cJSON *OpenAPI_ngran_failure_event_convertToJSON(OpenAPI_ngran_failure_event_t *ngran_failure_event);
OpenAPI_ngran_failure_event_t *OpenAPI_ngran_failure_event_copy(OpenAPI_ngran_failure_event_t *dst, OpenAPI_ngran_failure_event_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_ngran_failure_event_H_ */

