
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "notification.h"

OpenAPI_notification_t *OpenAPI_notification_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_list_t *failure_list,
    char *notify_correlation_id
)
{
    OpenAPI_notification_t *notification_local_var = ogs_malloc(sizeof(OpenAPI_notification_t));
    ogs_assert(notification_local_var);

    notification_local_var->mbs_session_id = mbs_session_id;
    notification_local_var->is_area_session_id = is_area_session_id;
    notification_local_var->area_session_id = area_session_id;
    notification_local_var->failure_list = failure_list;
    notification_local_var->notify_correlation_id = notify_correlation_id;

    return notification_local_var;
}

void OpenAPI_notification_free(OpenAPI_notification_t *notification)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == notification) {
        return;
    }
    if (notification->mbs_session_id) {
        OpenAPI_mbs_session_id_free(notification->mbs_session_id);
        notification->mbs_session_id = NULL;
    }
    if (notification->failure_list) {
        OpenAPI_list_for_each(notification->failure_list, node) {
            OpenAPI_ran_failure_free(node->data);
        }
        OpenAPI_list_free(notification->failure_list);
        notification->failure_list = NULL;
    }
    if (notification->notify_correlation_id) {
        ogs_free(notification->notify_correlation_id);
        notification->notify_correlation_id = NULL;
    }
    ogs_free(notification);
}

cJSON *OpenAPI_notification_convertToJSON(OpenAPI_notification_t *notification)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (notification == NULL) {
        ogs_error("OpenAPI_notification_convertToJSON() failed [Notification]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!notification->mbs_session_id) {
        ogs_error("OpenAPI_notification_convertToJSON() failed [mbs_session_id]");
        return NULL;
    }
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(notification->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_notification_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_notification_convertToJSON() failed [mbs_session_id]");
        goto end;
    }

    if (notification->is_area_session_id) {
    if (cJSON_AddNumberToObject(item, "areaSessionId", notification->area_session_id) == NULL) {
        ogs_error("OpenAPI_notification_convertToJSON() failed [area_session_id]");
        goto end;
    }
    }

    if (!notification->failure_list) {
        ogs_error("OpenAPI_notification_convertToJSON() failed [failure_list]");
        return NULL;
    }
    cJSON *failure_listList = cJSON_AddArrayToObject(item, "failureList");
    if (failure_listList == NULL) {
        ogs_error("OpenAPI_notification_convertToJSON() failed [failure_list]");
        goto end;
    }
    OpenAPI_list_for_each(notification->failure_list, node) {
        cJSON *itemLocal = OpenAPI_ran_failure_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_notification_convertToJSON() failed [failure_list]");
            goto end;
        }
        cJSON_AddItemToArray(failure_listList, itemLocal);
    }

    if (notification->notify_correlation_id) {
    if (cJSON_AddStringToObject(item, "notifyCorrelationId", notification->notify_correlation_id) == NULL) {
        ogs_error("OpenAPI_notification_convertToJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_notification_t *OpenAPI_notification_parseFromJSON(cJSON *notificationJSON)
{
    OpenAPI_notification_t *notification_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *area_session_id = NULL;
    cJSON *failure_list = NULL;
    OpenAPI_list_t *failure_listList = NULL;
    cJSON *notify_correlation_id = NULL;
    mbs_session_id = cJSON_GetObjectItemCaseSensitive(notificationJSON, "mbsSessionId");
    if (!mbs_session_id) {
        ogs_error("OpenAPI_notification_parseFromJSON() failed [mbs_session_id]");
        goto end;
    }
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }

    area_session_id = cJSON_GetObjectItemCaseSensitive(notificationJSON, "areaSessionId");
    if (area_session_id) {
    if (!cJSON_IsNumber(area_session_id)) {
        ogs_error("OpenAPI_notification_parseFromJSON() failed [area_session_id]");
        goto end;
    }
    }

    failure_list = cJSON_GetObjectItemCaseSensitive(notificationJSON, "failureList");
    if (!failure_list) {
        ogs_error("OpenAPI_notification_parseFromJSON() failed [failure_list]");
        goto end;
    }
        cJSON *failure_list_local = NULL;
        if (!cJSON_IsArray(failure_list)) {
            ogs_error("OpenAPI_notification_parseFromJSON() failed [failure_list]");
            goto end;
        }

        failure_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(failure_list_local, failure_list) {
            if (!cJSON_IsObject(failure_list_local)) {
                ogs_error("OpenAPI_notification_parseFromJSON() failed [failure_list]");
                goto end;
            }
            OpenAPI_ran_failure_t *failure_listItem = OpenAPI_ran_failure_parseFromJSON(failure_list_local);
            if (!failure_listItem) {
                ogs_error("No failure_listItem");
                goto end;
            }
            OpenAPI_list_add(failure_listList, failure_listItem);
        }

    notify_correlation_id = cJSON_GetObjectItemCaseSensitive(notificationJSON, "notifyCorrelationId");
    if (notify_correlation_id) {
    if (!cJSON_IsString(notify_correlation_id) && !cJSON_IsNull(notify_correlation_id)) {
        ogs_error("OpenAPI_notification_parseFromJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

    notification_local_var = OpenAPI_notification_create (
        mbs_session_id_local_nonprim,
        area_session_id ? true : false,
        area_session_id ? area_session_id->valuedouble : 0,
        failure_listList,
        notify_correlation_id && !cJSON_IsNull(notify_correlation_id) ? ogs_strdup(notify_correlation_id->valuestring) : NULL
    );

    return notification_local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (failure_listList) {
        OpenAPI_list_for_each(failure_listList, node) {
            OpenAPI_ran_failure_free(node->data);
        }
        OpenAPI_list_free(failure_listList);
        failure_listList = NULL;
    }
    return NULL;
}

OpenAPI_notification_t *OpenAPI_notification_copy(OpenAPI_notification_t *dst, OpenAPI_notification_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_notification_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_notification_convertToJSON() failed");
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

    OpenAPI_notification_free(dst);
    dst = OpenAPI_notification_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

