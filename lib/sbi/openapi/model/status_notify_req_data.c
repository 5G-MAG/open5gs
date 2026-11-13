
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "status_notify_req_data.h"

OpenAPI_status_notify_req_data_t *OpenAPI_status_notify_req_data_create(
    OpenAPI_mbs_session_event_report_list_t *event_list
)
{
    OpenAPI_status_notify_req_data_t *status_notify_req_data_local_var = ogs_malloc(sizeof(OpenAPI_status_notify_req_data_t));
    ogs_assert(status_notify_req_data_local_var);

    status_notify_req_data_local_var->event_list = event_list;

    return status_notify_req_data_local_var;
}

void OpenAPI_status_notify_req_data_free(OpenAPI_status_notify_req_data_t *status_notify_req_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == status_notify_req_data) {
        return;
    }
    if (status_notify_req_data->event_list) {
        OpenAPI_mbs_session_event_report_list_free(status_notify_req_data->event_list);
        status_notify_req_data->event_list = NULL;
    }
    ogs_free(status_notify_req_data);
}

cJSON *OpenAPI_status_notify_req_data_convertToJSON(OpenAPI_status_notify_req_data_t *status_notify_req_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (status_notify_req_data == NULL) {
        ogs_error("OpenAPI_status_notify_req_data_convertToJSON() failed [StatusNotifyReqData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!status_notify_req_data->event_list) {
        ogs_error("OpenAPI_status_notify_req_data_convertToJSON() failed [event_list]");
        return NULL;
    }
    cJSON *event_list_local_JSON = OpenAPI_mbs_session_event_report_list_convertToJSON(status_notify_req_data->event_list);
    if (event_list_local_JSON == NULL) {
        ogs_error("OpenAPI_status_notify_req_data_convertToJSON() failed [event_list]");
        goto end;
    }
    cJSON_AddItemToObject(item, "eventList", event_list_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_status_notify_req_data_convertToJSON() failed [event_list]");
        goto end;
    }

end:
    return item;
}

OpenAPI_status_notify_req_data_t *OpenAPI_status_notify_req_data_parseFromJSON(cJSON *status_notify_req_dataJSON)
{
    OpenAPI_status_notify_req_data_t *status_notify_req_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *event_list = NULL;
    OpenAPI_mbs_session_event_report_list_t *event_list_local_nonprim = NULL;
    event_list = cJSON_GetObjectItemCaseSensitive(status_notify_req_dataJSON, "eventList");
    if (!event_list) {
        ogs_error("OpenAPI_status_notify_req_data_parseFromJSON() failed [event_list]");
        goto end;
    }
    event_list_local_nonprim = OpenAPI_mbs_session_event_report_list_parseFromJSON(event_list);
    if (!event_list_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_event_report_list_parseFromJSON failed [event_list]");
        goto end;
    }

    status_notify_req_data_local_var = OpenAPI_status_notify_req_data_create (
        event_list_local_nonprim
    );

    return status_notify_req_data_local_var;
end:
    if (event_list_local_nonprim) {
        OpenAPI_mbs_session_event_report_list_free(event_list_local_nonprim);
        event_list_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_status_notify_req_data_t *OpenAPI_status_notify_req_data_copy(OpenAPI_status_notify_req_data_t *dst, OpenAPI_status_notify_req_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_status_notify_req_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_status_notify_req_data_convertToJSON() failed");
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

    OpenAPI_status_notify_req_data_free(dst);
    dst = OpenAPI_status_notify_req_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

