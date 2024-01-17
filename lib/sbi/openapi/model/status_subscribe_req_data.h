/*
 * status_subscribe_req_data.h
 *
 * Data within the StatusSubscribe Request
 */

#ifndef _OpenAPI_status_subscribe_req_data_H_
#define _OpenAPI_status_subscribe_req_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_session_subscription.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_status_subscribe_req_data_s OpenAPI_status_subscribe_req_data_t;
typedef struct OpenAPI_status_subscribe_req_data_s {
    struct OpenAPI_mbs_session_subscription_s *subscription;
} OpenAPI_status_subscribe_req_data_t;

OpenAPI_status_subscribe_req_data_t *OpenAPI_status_subscribe_req_data_create(
    OpenAPI_mbs_session_subscription_t *subscription
);
void OpenAPI_status_subscribe_req_data_free(OpenAPI_status_subscribe_req_data_t *status_subscribe_req_data);
OpenAPI_status_subscribe_req_data_t *OpenAPI_status_subscribe_req_data_parseFromJSON(cJSON *status_subscribe_req_dataJSON);
cJSON *OpenAPI_status_subscribe_req_data_convertToJSON(OpenAPI_status_subscribe_req_data_t *status_subscribe_req_data);
OpenAPI_status_subscribe_req_data_t *OpenAPI_status_subscribe_req_data_copy(OpenAPI_status_subscribe_req_data_t *dst, OpenAPI_status_subscribe_req_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_status_subscribe_req_data_H_ */

