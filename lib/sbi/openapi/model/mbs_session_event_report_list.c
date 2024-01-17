
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_session_event_report_list.h"

OpenAPI_mbs_session_event_report_list_t *OpenAPI_mbs_session_event_report_list_create(
    OpenAPI_list_t *event_report_list,
    char *notify_correlation_id
)
{
    OpenAPI_mbs_session_event_report_list_t *mbs_session_event_report_list_local_var = ogs_malloc(sizeof(OpenAPI_mbs_session_event_report_list_t));
    ogs_assert(mbs_session_event_report_list_local_var);

    mbs_session_event_report_list_local_var->event_report_list = event_report_list;
    mbs_session_event_report_list_local_var->notify_correlation_id = notify_correlation_id;

    return mbs_session_event_report_list_local_var;
}

void OpenAPI_mbs_session_event_report_list_free(OpenAPI_mbs_session_event_report_list_t *mbs_session_event_report_list)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_session_event_report_list) {
        return;
    }
    if (mbs_session_event_report_list->event_report_list) {
        OpenAPI_list_for_each(mbs_session_event_report_list->event_report_list, node) {
            OpenAPI_mbs_session_event_report_free(node->data);
        }
        OpenAPI_list_free(mbs_session_event_report_list->event_report_list);
        mbs_session_event_report_list->event_report_list = NULL;
    }
    if (mbs_session_event_report_list->notify_correlation_id) {
        ogs_free(mbs_session_event_report_list->notify_correlation_id);
        mbs_session_event_report_list->notify_correlation_id = NULL;
    }
    ogs_free(mbs_session_event_report_list);
}

cJSON *OpenAPI_mbs_session_event_report_list_convertToJSON(OpenAPI_mbs_session_event_report_list_t *mbs_session_event_report_list)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_session_event_report_list == NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_list_convertToJSON() failed [MbsSessionEventReportList]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_session_event_report_list->event_report_list) {
        ogs_error("OpenAPI_mbs_session_event_report_list_convertToJSON() failed [event_report_list]");
        return NULL;
    }
    cJSON *event_report_listList = cJSON_AddArrayToObject(item, "eventReportList");
    if (event_report_listList == NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_list_convertToJSON() failed [event_report_list]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_session_event_report_list->event_report_list, node) {
        cJSON *itemLocal = OpenAPI_mbs_session_event_report_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_mbs_session_event_report_list_convertToJSON() failed [event_report_list]");
            goto end;
        }
        cJSON_AddItemToArray(event_report_listList, itemLocal);
    }

    if (mbs_session_event_report_list->notify_correlation_id) {
    if (cJSON_AddStringToObject(item, "notifyCorrelationId", mbs_session_event_report_list->notify_correlation_id) == NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_list_convertToJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_session_event_report_list_t *OpenAPI_mbs_session_event_report_list_parseFromJSON(cJSON *mbs_session_event_report_listJSON)
{
    OpenAPI_mbs_session_event_report_list_t *mbs_session_event_report_list_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *event_report_list = NULL;
    OpenAPI_list_t *event_report_listList = NULL;
    cJSON *notify_correlation_id = NULL;
    event_report_list = cJSON_GetObjectItemCaseSensitive(mbs_session_event_report_listJSON, "eventReportList");
    if (!event_report_list) {
        ogs_error("OpenAPI_mbs_session_event_report_list_parseFromJSON() failed [event_report_list]");
        goto end;
    }
        cJSON *event_report_list_local = NULL;
        if (!cJSON_IsArray(event_report_list)) {
            ogs_error("OpenAPI_mbs_session_event_report_list_parseFromJSON() failed [event_report_list]");
            goto end;
        }

        event_report_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(event_report_list_local, event_report_list) {
            if (!cJSON_IsObject(event_report_list_local)) {
                ogs_error("OpenAPI_mbs_session_event_report_list_parseFromJSON() failed [event_report_list]");
                goto end;
            }
            OpenAPI_mbs_session_event_report_t *event_report_listItem = OpenAPI_mbs_session_event_report_parseFromJSON(event_report_list_local);
            if (!event_report_listItem) {
                ogs_error("No event_report_listItem");
                goto end;
            }
            OpenAPI_list_add(event_report_listList, event_report_listItem);
        }

    notify_correlation_id = cJSON_GetObjectItemCaseSensitive(mbs_session_event_report_listJSON, "notifyCorrelationId");
    if (notify_correlation_id) {
    if (!cJSON_IsString(notify_correlation_id) && !cJSON_IsNull(notify_correlation_id)) {
        ogs_error("OpenAPI_mbs_session_event_report_list_parseFromJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

    mbs_session_event_report_list_local_var = OpenAPI_mbs_session_event_report_list_create (
        event_report_listList,
        notify_correlation_id && !cJSON_IsNull(notify_correlation_id) ? ogs_strdup(notify_correlation_id->valuestring) : NULL
    );

    return mbs_session_event_report_list_local_var;
end:
    if (event_report_listList) {
        OpenAPI_list_for_each(event_report_listList, node) {
            OpenAPI_mbs_session_event_report_free(node->data);
        }
        OpenAPI_list_free(event_report_listList);
        event_report_listList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_session_event_report_list_t *OpenAPI_mbs_session_event_report_list_copy(OpenAPI_mbs_session_event_report_list_t *dst, OpenAPI_mbs_session_event_report_list_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_session_event_report_list_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_session_event_report_list_convertToJSON() failed");
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

    OpenAPI_mbs_session_event_report_list_free(dst);
    dst = OpenAPI_mbs_session_event_report_list_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

