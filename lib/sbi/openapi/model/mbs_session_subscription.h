/*
 * mbs_session_subscription.h
 *
 * MBS session subscription
 */

#ifndef _OpenAPI_mbs_session_subscription_H_
#define _OpenAPI_mbs_session_subscription_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_session_event.h"
#include "mbs_session_id.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_session_subscription_s OpenAPI_mbs_session_subscription_t;
typedef struct OpenAPI_mbs_session_subscription_s {
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    bool is_area_session_id;
    int area_session_id;
    OpenAPI_list_t *event_list;
    char *notify_uri;
    char *notify_correlation_id;
    char *expiry_time;
    char *nfc_instance_id;
    char *mbs_session_subsc_uri;
} OpenAPI_mbs_session_subscription_t;

OpenAPI_mbs_session_subscription_t *OpenAPI_mbs_session_subscription_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_list_t *event_list,
    char *notify_uri,
    char *notify_correlation_id,
    char *expiry_time,
    char *nfc_instance_id,
    char *mbs_session_subsc_uri
);
void OpenAPI_mbs_session_subscription_free(OpenAPI_mbs_session_subscription_t *mbs_session_subscription);
OpenAPI_mbs_session_subscription_t *OpenAPI_mbs_session_subscription_parseFromJSON(cJSON *mbs_session_subscriptionJSON);
cJSON *OpenAPI_mbs_session_subscription_convertToJSON(OpenAPI_mbs_session_subscription_t *mbs_session_subscription);
OpenAPI_mbs_session_subscription_t *OpenAPI_mbs_session_subscription_copy(OpenAPI_mbs_session_subscription_t *dst, OpenAPI_mbs_session_subscription_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_session_subscription_H_ */

