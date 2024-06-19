/*
 * context_status_notification.h
 *
 * Data within ContextStatusNotify Request
 */

#ifndef _OpenAPI_context_status_notification_H_
#define _OpenAPI_context_status_notification_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_session_id.h"
#include "n2_mbs_sm_info.h"
#include "operation_event.h"
#include "operation_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_status_notification_s OpenAPI_context_status_notification_t;
typedef enum { OpenAPI_context_status_notification_RELEASEDIND_NULL = 0, OpenAPI_context_status_notification_RELEASEDIND__true } OpenAPI_context_status_notification_released_ind_e;

char* OpenAPI_context_status_notification_released_ind_ToString(OpenAPI_context_status_notification_released_ind_e released_ind);

OpenAPI_context_status_notification_released_ind_e OpenAPI_context_status_notification_released_ind_FromString(char* released_ind);
typedef struct OpenAPI_context_status_notification_s {
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    bool is_area_session_id;
    int area_session_id;
    OpenAPI_list_t *n2_mbs_sm_info_list;
    OpenAPI_list_t *operation_events;
    OpenAPI_operation_status_e operation_status;
    OpenAPI_context_status_notification_released_ind_e released_ind;
} OpenAPI_context_status_notification_t;

OpenAPI_context_status_notification_t *OpenAPI_context_status_notification_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_list_t *n2_mbs_sm_info_list,
    OpenAPI_list_t *operation_events,
    OpenAPI_operation_status_e operation_status,
    OpenAPI_context_status_notification_released_ind_e released_ind
);
void OpenAPI_context_status_notification_free(OpenAPI_context_status_notification_t *context_status_notification);
OpenAPI_context_status_notification_t *OpenAPI_context_status_notification_parseFromJSON(cJSON *context_status_notificationJSON);
cJSON *OpenAPI_context_status_notification_convertToJSON(OpenAPI_context_status_notification_t *context_status_notification);
OpenAPI_context_status_notification_t *OpenAPI_context_status_notification_copy(OpenAPI_context_status_notification_t *dst, OpenAPI_context_status_notification_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_status_notification_H_ */

