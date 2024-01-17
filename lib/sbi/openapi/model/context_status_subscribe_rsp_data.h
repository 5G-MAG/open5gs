/*
 * context_status_subscribe_rsp_data.h
 *
 * Data within ContextStatusSubscribe Response
 */

#ifndef _OpenAPI_context_status_subscribe_rsp_data_H_
#define _OpenAPI_context_status_subscribe_rsp_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "context_status_event_report.h"
#include "context_status_subscription.h"
#include "mbs_context_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_status_subscribe_rsp_data_s OpenAPI_context_status_subscribe_rsp_data_t;
typedef struct OpenAPI_context_status_subscribe_rsp_data_s {
    struct OpenAPI_context_status_subscription_s *subscription;
    OpenAPI_list_t *report_list;
    struct OpenAPI_mbs_context_info_s *mbs_context_info;
} OpenAPI_context_status_subscribe_rsp_data_t;

OpenAPI_context_status_subscribe_rsp_data_t *OpenAPI_context_status_subscribe_rsp_data_create(
    OpenAPI_context_status_subscription_t *subscription,
    OpenAPI_list_t *report_list,
    OpenAPI_mbs_context_info_t *mbs_context_info
);
void OpenAPI_context_status_subscribe_rsp_data_free(OpenAPI_context_status_subscribe_rsp_data_t *context_status_subscribe_rsp_data);
OpenAPI_context_status_subscribe_rsp_data_t *OpenAPI_context_status_subscribe_rsp_data_parseFromJSON(cJSON *context_status_subscribe_rsp_dataJSON);
cJSON *OpenAPI_context_status_subscribe_rsp_data_convertToJSON(OpenAPI_context_status_subscribe_rsp_data_t *context_status_subscribe_rsp_data);
OpenAPI_context_status_subscribe_rsp_data_t *OpenAPI_context_status_subscribe_rsp_data_copy(OpenAPI_context_status_subscribe_rsp_data_t *dst, OpenAPI_context_status_subscribe_rsp_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_status_subscribe_rsp_data_H_ */

