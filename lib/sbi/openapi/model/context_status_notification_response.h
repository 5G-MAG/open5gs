/*
 * context_status_notification_response.h
 *
 * Data within ContextStatusNotify Response
 */

#ifndef _OpenAPI_context_status_notification_response_H_
#define _OpenAPI_context_status_notification_response_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_session_id.h"
#include "n2_mbs_sm_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_status_notification_response_s OpenAPI_context_status_notification_response_t;
typedef struct OpenAPI_context_status_notification_response_s {
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    bool is_area_session_id;
    int area_session_id;
    OpenAPI_list_t *n2_mbs_sm_info_list;
} OpenAPI_context_status_notification_response_t;

OpenAPI_context_status_notification_response_t *OpenAPI_context_status_notification_response_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_list_t *n2_mbs_sm_info_list
);
void OpenAPI_context_status_notification_response_free(OpenAPI_context_status_notification_response_t *context_status_notification_response);
OpenAPI_context_status_notification_response_t *OpenAPI_context_status_notification_response_parseFromJSON(cJSON *context_status_notification_responseJSON);
cJSON *OpenAPI_context_status_notification_response_convertToJSON(OpenAPI_context_status_notification_response_t *context_status_notification_response);
OpenAPI_context_status_notification_response_t *OpenAPI_context_status_notification_response_copy(OpenAPI_context_status_notification_response_t *dst, OpenAPI_context_status_notification_response_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_status_notification_response_H_ */

