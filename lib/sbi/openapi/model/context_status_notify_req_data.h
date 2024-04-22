/*
 * context_status_notify_req_data.h
 *
 * Context Status Notification
 */

#ifndef _OpenAPI_context_status_notify_req_data_H_
#define _OpenAPI_context_status_notify_req_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "context_status_event_report.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_status_notify_req_data_s OpenAPI_context_status_notify_req_data_t;
typedef struct OpenAPI_context_status_notify_req_data_s {
    OpenAPI_list_t *report_list;
    char *notify_correlation_id;
} OpenAPI_context_status_notify_req_data_t;

OpenAPI_context_status_notify_req_data_t *OpenAPI_context_status_notify_req_data_create(
    OpenAPI_list_t *report_list,
    char *notify_correlation_id
);
void OpenAPI_context_status_notify_req_data_free(OpenAPI_context_status_notify_req_data_t *context_status_notify_req_data);
OpenAPI_context_status_notify_req_data_t *OpenAPI_context_status_notify_req_data_parseFromJSON(cJSON *context_status_notify_req_dataJSON);
cJSON *OpenAPI_context_status_notify_req_data_convertToJSON(OpenAPI_context_status_notify_req_data_t *context_status_notify_req_data);
OpenAPI_context_status_notify_req_data_t *OpenAPI_context_status_notify_req_data_copy(OpenAPI_context_status_notify_req_data_t *dst, OpenAPI_context_status_notify_req_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_status_notify_req_data_H_ */
