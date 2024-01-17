
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_status_notify_req_data.h"

OpenAPI_context_status_notify_req_data_t *OpenAPI_context_status_notify_req_data_create(
    OpenAPI_list_t *report_list,
    char *notify_correlation_id
)
{
    OpenAPI_context_status_notify_req_data_t *context_status_notify_req_data_local_var = ogs_malloc(sizeof(OpenAPI_context_status_notify_req_data_t));
    ogs_assert(context_status_notify_req_data_local_var);

    context_status_notify_req_data_local_var->report_list = report_list;
    context_status_notify_req_data_local_var->notify_correlation_id = notify_correlation_id;

    return context_status_notify_req_data_local_var;
}

void OpenAPI_context_status_notify_req_data_free(OpenAPI_context_status_notify_req_data_t *context_status_notify_req_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_status_notify_req_data) {
        return;
    }
    if (context_status_notify_req_data->report_list) {
        OpenAPI_list_for_each(context_status_notify_req_data->report_list, node) {
            OpenAPI_context_status_event_report_free(node->data);
        }
        OpenAPI_list_free(context_status_notify_req_data->report_list);
        context_status_notify_req_data->report_list = NULL;
    }
    if (context_status_notify_req_data->notify_correlation_id) {
        ogs_free(context_status_notify_req_data->notify_correlation_id);
        context_status_notify_req_data->notify_correlation_id = NULL;
    }
    ogs_free(context_status_notify_req_data);
}

cJSON *OpenAPI_context_status_notify_req_data_convertToJSON(OpenAPI_context_status_notify_req_data_t *context_status_notify_req_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_status_notify_req_data == NULL) {
        ogs_error("OpenAPI_context_status_notify_req_data_convertToJSON() failed [ContextStatusNotifyReqData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!context_status_notify_req_data->report_list) {
        ogs_error("OpenAPI_context_status_notify_req_data_convertToJSON() failed [report_list]");
        return NULL;
    }
    cJSON *report_listList = cJSON_AddArrayToObject(item, "reportList");
    if (report_listList == NULL) {
        ogs_error("OpenAPI_context_status_notify_req_data_convertToJSON() failed [report_list]");
        goto end;
    }
    OpenAPI_list_for_each(context_status_notify_req_data->report_list, node) {
        cJSON *itemLocal = OpenAPI_context_status_event_report_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_context_status_notify_req_data_convertToJSON() failed [report_list]");
            goto end;
        }
        cJSON_AddItemToArray(report_listList, itemLocal);
    }

    if (context_status_notify_req_data->notify_correlation_id) {
    if (cJSON_AddStringToObject(item, "notifyCorrelationId", context_status_notify_req_data->notify_correlation_id) == NULL) {
        ogs_error("OpenAPI_context_status_notify_req_data_convertToJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_status_notify_req_data_t *OpenAPI_context_status_notify_req_data_parseFromJSON(cJSON *context_status_notify_req_dataJSON)
{
    OpenAPI_context_status_notify_req_data_t *context_status_notify_req_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *report_list = NULL;
    OpenAPI_list_t *report_listList = NULL;
    cJSON *notify_correlation_id = NULL;
    report_list = cJSON_GetObjectItemCaseSensitive(context_status_notify_req_dataJSON, "reportList");
    if (!report_list) {
        ogs_error("OpenAPI_context_status_notify_req_data_parseFromJSON() failed [report_list]");
        goto end;
    }
        cJSON *report_list_local = NULL;
        if (!cJSON_IsArray(report_list)) {
            ogs_error("OpenAPI_context_status_notify_req_data_parseFromJSON() failed [report_list]");
            goto end;
        }

        report_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(report_list_local, report_list) {
            if (!cJSON_IsObject(report_list_local)) {
                ogs_error("OpenAPI_context_status_notify_req_data_parseFromJSON() failed [report_list]");
                goto end;
            }
            OpenAPI_context_status_event_report_t *report_listItem = OpenAPI_context_status_event_report_parseFromJSON(report_list_local);
            if (!report_listItem) {
                ogs_error("No report_listItem");
                goto end;
            }
            OpenAPI_list_add(report_listList, report_listItem);
        }

    notify_correlation_id = cJSON_GetObjectItemCaseSensitive(context_status_notify_req_dataJSON, "notifyCorrelationId");
    if (notify_correlation_id) {
    if (!cJSON_IsString(notify_correlation_id) && !cJSON_IsNull(notify_correlation_id)) {
        ogs_error("OpenAPI_context_status_notify_req_data_parseFromJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

    context_status_notify_req_data_local_var = OpenAPI_context_status_notify_req_data_create (
        report_listList,
        notify_correlation_id && !cJSON_IsNull(notify_correlation_id) ? ogs_strdup(notify_correlation_id->valuestring) : NULL
    );

    return context_status_notify_req_data_local_var;
end:
    if (report_listList) {
        OpenAPI_list_for_each(report_listList, node) {
            OpenAPI_context_status_event_report_free(node->data);
        }
        OpenAPI_list_free(report_listList);
        report_listList = NULL;
    }
    return NULL;
}

OpenAPI_context_status_notify_req_data_t *OpenAPI_context_status_notify_req_data_copy(OpenAPI_context_status_notify_req_data_t *dst, OpenAPI_context_status_notify_req_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_status_notify_req_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_status_notify_req_data_convertToJSON() failed");
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

    OpenAPI_context_status_notify_req_data_free(dst);
    dst = OpenAPI_context_status_notify_req_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

