/*
 * context_status_event.h
 *
 * Context Status Event
 */

#ifndef _OpenAPI_context_status_event_H_
#define _OpenAPI_context_status_event_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "context_status_event_type.h"
#include "reporting_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_status_event_s OpenAPI_context_status_event_t;
typedef struct OpenAPI_context_status_event_s {
    OpenAPI_context_status_event_type_e event_type;
    bool is_immediate_report_ind;
    int immediate_report_ind;
    OpenAPI_reporting_mode_e reporting_mode;
} OpenAPI_context_status_event_t;

OpenAPI_context_status_event_t *OpenAPI_context_status_event_create(
    OpenAPI_context_status_event_type_e event_type,
    bool is_immediate_report_ind,
    int immediate_report_ind,
    OpenAPI_reporting_mode_e reporting_mode
);
void OpenAPI_context_status_event_free(OpenAPI_context_status_event_t *context_status_event);
OpenAPI_context_status_event_t *OpenAPI_context_status_event_parseFromJSON(cJSON *context_status_eventJSON);
cJSON *OpenAPI_context_status_event_convertToJSON(OpenAPI_context_status_event_t *context_status_event);
OpenAPI_context_status_event_t *OpenAPI_context_status_event_copy(OpenAPI_context_status_event_t *dst, OpenAPI_context_status_event_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_status_event_H_ */

