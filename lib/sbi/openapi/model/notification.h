/*
 * notification.h
 *
 * Data within Notify Request
 */

#ifndef _OpenAPI_notification_H_
#define _OpenAPI_notification_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_notification_s OpenAPI_notification_t;
#include "mbs_session_id.h"
#include "ran_failure.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_notification_s {
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    bool is_area_session_id;
    int area_session_id;
    OpenAPI_list_t *failure_list;
    char *notify_correlation_id;
};

OpenAPI_notification_t *OpenAPI_notification_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_list_t *failure_list,
    char *notify_correlation_id
);
void OpenAPI_notification_free(OpenAPI_notification_t *notification);
OpenAPI_notification_t *OpenAPI_notification_parseFromJSON(cJSON *notificationJSON);
cJSON *OpenAPI_notification_convertToJSON(OpenAPI_notification_t *notification);
OpenAPI_notification_t *OpenAPI_notification_copy(OpenAPI_notification_t *dst, OpenAPI_notification_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_notification_H_ */

