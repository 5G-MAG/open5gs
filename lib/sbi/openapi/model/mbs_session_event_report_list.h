/*
 * mbs_session_event_report_list.h
 *
 * MBS session event report list
 */

#ifndef _OpenAPI_mbs_session_event_report_list_H_
#define _OpenAPI_mbs_session_event_report_list_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_session_event_report.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_session_event_report_list_s OpenAPI_mbs_session_event_report_list_t;
typedef struct OpenAPI_mbs_session_event_report_list_s {
    OpenAPI_list_t *event_report_list;
    char *notify_correlation_id;
} OpenAPI_mbs_session_event_report_list_t;

OpenAPI_mbs_session_event_report_list_t *OpenAPI_mbs_session_event_report_list_create(
    OpenAPI_list_t *event_report_list,
    char *notify_correlation_id
);
void OpenAPI_mbs_session_event_report_list_free(OpenAPI_mbs_session_event_report_list_t *mbs_session_event_report_list);
OpenAPI_mbs_session_event_report_list_t *OpenAPI_mbs_session_event_report_list_parseFromJSON(cJSON *mbs_session_event_report_listJSON);
cJSON *OpenAPI_mbs_session_event_report_list_convertToJSON(OpenAPI_mbs_session_event_report_list_t *mbs_session_event_report_list);
OpenAPI_mbs_session_event_report_list_t *OpenAPI_mbs_session_event_report_list_copy(OpenAPI_mbs_session_event_report_list_t *dst, OpenAPI_mbs_session_event_report_list_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_session_event_report_list_H_ */

