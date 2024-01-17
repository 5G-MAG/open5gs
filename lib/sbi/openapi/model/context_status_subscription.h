/*
 * context_status_subscription.h
 *
 * Context Status Subscription
 */

#ifndef _OpenAPI_context_status_subscription_H_
#define _OpenAPI_context_status_subscription_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "context_status_event.h"
#include "mbs_session_id.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_status_subscription_s OpenAPI_context_status_subscription_t;
typedef struct OpenAPI_context_status_subscription_s {
    char *nfc_instance_id;
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    OpenAPI_list_t *event_list;
    char *notify_uri;
    char *notify_correlation_id;
    char *expiry_time;
} OpenAPI_context_status_subscription_t;

OpenAPI_context_status_subscription_t *OpenAPI_context_status_subscription_create(
    char *nfc_instance_id,
    OpenAPI_mbs_session_id_t *mbs_session_id,
    OpenAPI_list_t *event_list,
    char *notify_uri,
    char *notify_correlation_id,
    char *expiry_time
);
void OpenAPI_context_status_subscription_free(OpenAPI_context_status_subscription_t *context_status_subscription);
OpenAPI_context_status_subscription_t *OpenAPI_context_status_subscription_parseFromJSON(cJSON *context_status_subscriptionJSON);
cJSON *OpenAPI_context_status_subscription_convertToJSON(OpenAPI_context_status_subscription_t *context_status_subscription);
OpenAPI_context_status_subscription_t *OpenAPI_context_status_subscription_copy(OpenAPI_context_status_subscription_t *dst, OpenAPI_context_status_subscription_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_status_subscription_H_ */

