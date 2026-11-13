
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "status_subscribe_rsp_data.h"

OpenAPI_status_subscribe_rsp_data_t *OpenAPI_status_subscribe_rsp_data_create(
    OpenAPI_mbs_session_subscription_t *subscription,
    OpenAPI_mbs_session_event_report_list_t *event_list
)
{
    OpenAPI_status_subscribe_rsp_data_t *status_subscribe_rsp_data_local_var = ogs_malloc(sizeof(OpenAPI_status_subscribe_rsp_data_t));
    ogs_assert(status_subscribe_rsp_data_local_var);

    status_subscribe_rsp_data_local_var->subscription = subscription;
    status_subscribe_rsp_data_local_var->event_list = event_list;

    return status_subscribe_rsp_data_local_var;
}

void OpenAPI_status_subscribe_rsp_data_free(OpenAPI_status_subscribe_rsp_data_t *status_subscribe_rsp_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == status_subscribe_rsp_data) {
        return;
    }
    if (status_subscribe_rsp_data->subscription) {
        OpenAPI_mbs_session_subscription_free(status_subscribe_rsp_data->subscription);
        status_subscribe_rsp_data->subscription = NULL;
    }
    if (status_subscribe_rsp_data->event_list) {
        OpenAPI_mbs_session_event_report_list_free(status_subscribe_rsp_data->event_list);
        status_subscribe_rsp_data->event_list = NULL;
    }
    ogs_free(status_subscribe_rsp_data);
}

cJSON *OpenAPI_status_subscribe_rsp_data_convertToJSON(OpenAPI_status_subscribe_rsp_data_t *status_subscribe_rsp_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (status_subscribe_rsp_data == NULL) {
        ogs_error("OpenAPI_status_subscribe_rsp_data_convertToJSON() failed [StatusSubscribeRspData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!status_subscribe_rsp_data->subscription) {
        ogs_error("OpenAPI_status_subscribe_rsp_data_convertToJSON() failed [subscription]");
        return NULL;
    }
    cJSON *subscription_local_JSON = OpenAPI_mbs_session_subscription_convertToJSON(status_subscribe_rsp_data->subscription);
    if (subscription_local_JSON == NULL) {
        ogs_error("OpenAPI_status_subscribe_rsp_data_convertToJSON() failed [subscription]");
        goto end;
    }
    cJSON_AddItemToObject(item, "subscription", subscription_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_status_subscribe_rsp_data_convertToJSON() failed [subscription]");
        goto end;
    }

    if (status_subscribe_rsp_data->event_list) {
    cJSON *event_list_local_JSON = OpenAPI_mbs_session_event_report_list_convertToJSON(status_subscribe_rsp_data->event_list);
    if (event_list_local_JSON == NULL) {
        ogs_error("OpenAPI_status_subscribe_rsp_data_convertToJSON() failed [event_list]");
        goto end;
    }
    cJSON_AddItemToObject(item, "eventList", event_list_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_status_subscribe_rsp_data_convertToJSON() failed [event_list]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_status_subscribe_rsp_data_t *OpenAPI_status_subscribe_rsp_data_parseFromJSON(cJSON *status_subscribe_rsp_dataJSON)
{
    OpenAPI_status_subscribe_rsp_data_t *status_subscribe_rsp_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *subscription = NULL;
    OpenAPI_mbs_session_subscription_t *subscription_local_nonprim = NULL;
    cJSON *event_list = NULL;
    OpenAPI_mbs_session_event_report_list_t *event_list_local_nonprim = NULL;
    subscription = cJSON_GetObjectItemCaseSensitive(status_subscribe_rsp_dataJSON, "subscription");
    if (!subscription) {
        ogs_error("OpenAPI_status_subscribe_rsp_data_parseFromJSON() failed [subscription]");
        goto end;
    }
    subscription_local_nonprim = OpenAPI_mbs_session_subscription_parseFromJSON(subscription);
    if (!subscription_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON failed [subscription]");
        goto end;
    }

    event_list = cJSON_GetObjectItemCaseSensitive(status_subscribe_rsp_dataJSON, "eventList");
    if (event_list) {
    event_list_local_nonprim = OpenAPI_mbs_session_event_report_list_parseFromJSON(event_list);
    if (!event_list_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_event_report_list_parseFromJSON failed [event_list]");
        goto end;
    }
    }

    status_subscribe_rsp_data_local_var = OpenAPI_status_subscribe_rsp_data_create (
        subscription_local_nonprim,
        event_list ? event_list_local_nonprim : NULL
    );

    return status_subscribe_rsp_data_local_var;
end:
    if (subscription_local_nonprim) {
        OpenAPI_mbs_session_subscription_free(subscription_local_nonprim);
        subscription_local_nonprim = NULL;
    }
    if (event_list_local_nonprim) {
        OpenAPI_mbs_session_event_report_list_free(event_list_local_nonprim);
        event_list_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_status_subscribe_rsp_data_t *OpenAPI_status_subscribe_rsp_data_copy(OpenAPI_status_subscribe_rsp_data_t *dst, OpenAPI_status_subscribe_rsp_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_status_subscribe_rsp_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_status_subscribe_rsp_data_convertToJSON() failed");
        return NULL;
    }

    content = cJSON_Print(item);
    cJSON_Delete(item);

    if (!content) {
        ogs_error("cJSON_Print() failed");
        return NULL;
    }

    item = cJSON_Parse(content);
    ogs_free(content);
    if (!item) {
        ogs_error("cJSON_Parse() failed");
        return NULL;
    }

    OpenAPI_status_subscribe_rsp_data_free(dst);
    dst = OpenAPI_status_subscribe_rsp_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

