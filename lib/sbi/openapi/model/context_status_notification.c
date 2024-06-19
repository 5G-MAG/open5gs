
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_status_notification.h"

char *OpenAPI_released_indcontext_status_notification_ToString(OpenAPI_context_status_notification_released_ind_e released_ind)
{
    const char *released_indArray[] =  { "NULL", "true" };
    size_t sizeofArray = sizeof(released_indArray) / sizeof(released_indArray[0]);
    if (released_ind < sizeofArray)
        return (char *)released_indArray[released_ind];
    else
        return (char *)"Unknown";
}

OpenAPI_context_status_notification_released_ind_e OpenAPI_released_indcontext_status_notification_FromString(char* released_ind)
{
    int stringToReturn = 0;
    const char *released_indArray[] =  { "NULL", "true" };
    size_t sizeofArray = sizeof(released_indArray) / sizeof(released_indArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(released_ind, released_indArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}
OpenAPI_context_status_notification_t *OpenAPI_context_status_notification_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_list_t *n2_mbs_sm_info_list,
    OpenAPI_list_t *operation_events,
    OpenAPI_operation_status_e operation_status,
    OpenAPI_context_status_notification_released_ind_e released_ind
)
{
    OpenAPI_context_status_notification_t *context_status_notification_local_var = ogs_malloc(sizeof(OpenAPI_context_status_notification_t));
    ogs_assert(context_status_notification_local_var);

    context_status_notification_local_var->mbs_session_id = mbs_session_id;
    context_status_notification_local_var->is_area_session_id = is_area_session_id;
    context_status_notification_local_var->area_session_id = area_session_id;
    context_status_notification_local_var->n2_mbs_sm_info_list = n2_mbs_sm_info_list;
    context_status_notification_local_var->operation_events = operation_events;
    context_status_notification_local_var->operation_status = operation_status;
    context_status_notification_local_var->released_ind = released_ind;

    return context_status_notification_local_var;
}

void OpenAPI_context_status_notification_free(OpenAPI_context_status_notification_t *context_status_notification)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_status_notification) {
        return;
    }
    if (context_status_notification->mbs_session_id) {
        OpenAPI_mbs_session_id_free(context_status_notification->mbs_session_id);
        context_status_notification->mbs_session_id = NULL;
    }
    if (context_status_notification->n2_mbs_sm_info_list) {
        OpenAPI_list_for_each(context_status_notification->n2_mbs_sm_info_list, node) {
            OpenAPI_n2_mbs_sm_info_free(node->data);
        }
        OpenAPI_list_free(context_status_notification->n2_mbs_sm_info_list);
        context_status_notification->n2_mbs_sm_info_list = NULL;
    }
    if (context_status_notification->operation_events) {
        OpenAPI_list_for_each(context_status_notification->operation_events, node) {
            OpenAPI_operation_event_free(node->data);
        }
        OpenAPI_list_free(context_status_notification->operation_events);
        context_status_notification->operation_events = NULL;
    }
    ogs_free(context_status_notification);
}

cJSON *OpenAPI_context_status_notification_convertToJSON(OpenAPI_context_status_notification_t *context_status_notification)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_status_notification == NULL) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [ContextStatusNotification]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!context_status_notification->mbs_session_id) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [mbs_session_id]");
        return NULL;
    }
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(context_status_notification->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [mbs_session_id]");
        goto end;
    }

    if (context_status_notification->is_area_session_id) {
    if (cJSON_AddNumberToObject(item, "areaSessionId", context_status_notification->area_session_id) == NULL) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [area_session_id]");
        goto end;
    }
    }

    if (context_status_notification->n2_mbs_sm_info_list) {
    cJSON *n2_mbs_sm_info_listList = cJSON_AddArrayToObject(item, "n2MbsSmInfoList");
    if (n2_mbs_sm_info_listList == NULL) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [n2_mbs_sm_info_list]");
        goto end;
    }
    OpenAPI_list_for_each(context_status_notification->n2_mbs_sm_info_list, node) {
        cJSON *itemLocal = OpenAPI_n2_mbs_sm_info_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [n2_mbs_sm_info_list]");
            goto end;
        }
        cJSON_AddItemToArray(n2_mbs_sm_info_listList, itemLocal);
    }
    }

    if (context_status_notification->operation_events) {
    cJSON *operation_eventsList = cJSON_AddArrayToObject(item, "operationEvents");
    if (operation_eventsList == NULL) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [operation_events]");
        goto end;
    }
    OpenAPI_list_for_each(context_status_notification->operation_events, node) {
        cJSON *itemLocal = OpenAPI_operation_event_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [operation_events]");
            goto end;
        }
        cJSON_AddItemToArray(operation_eventsList, itemLocal);
    }
    }

    if (context_status_notification->operation_status != OpenAPI_operation_status_NULL) {
    if (cJSON_AddStringToObject(item, "operationStatus", OpenAPI_operation_status_ToString(context_status_notification->operation_status)) == NULL) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [operation_status]");
        goto end;
    }
    }

    if (context_status_notification->released_ind != OpenAPI_context_status_notification_RELEASEDIND_NULL) {
    if (cJSON_AddStringToObject(item, "releasedInd", OpenAPI_released_indcontext_status_notification_ToString(context_status_notification->released_ind)) == NULL) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed [released_ind]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_status_notification_t *OpenAPI_context_status_notification_parseFromJSON(cJSON *context_status_notificationJSON)
{
    OpenAPI_context_status_notification_t *context_status_notification_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *area_session_id = NULL;
    cJSON *n2_mbs_sm_info_list = NULL;
    OpenAPI_list_t *n2_mbs_sm_info_listList = NULL;
    cJSON *operation_events = NULL;
    OpenAPI_list_t *operation_eventsList = NULL;
    cJSON *operation_status = NULL;
    OpenAPI_operation_status_e operation_statusVariable = 0;
    cJSON *released_ind = NULL;
    OpenAPI_context_status_notification_released_ind_e released_indVariable = 0;
    mbs_session_id = cJSON_GetObjectItemCaseSensitive(context_status_notificationJSON, "mbsSessionId");
    if (!mbs_session_id) {
        ogs_error("OpenAPI_context_status_notification_parseFromJSON() failed [mbs_session_id]");
        goto end;
    }
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }

    area_session_id = cJSON_GetObjectItemCaseSensitive(context_status_notificationJSON, "areaSessionId");
    if (area_session_id) {
    if (!cJSON_IsNumber(area_session_id)) {
        ogs_error("OpenAPI_context_status_notification_parseFromJSON() failed [area_session_id]");
        goto end;
    }
    }

    n2_mbs_sm_info_list = cJSON_GetObjectItemCaseSensitive(context_status_notificationJSON, "n2MbsSmInfoList");
    if (n2_mbs_sm_info_list) {
        cJSON *n2_mbs_sm_info_list_local = NULL;
        if (!cJSON_IsArray(n2_mbs_sm_info_list)) {
            ogs_error("OpenAPI_context_status_notification_parseFromJSON() failed [n2_mbs_sm_info_list]");
            goto end;
        }

        n2_mbs_sm_info_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(n2_mbs_sm_info_list_local, n2_mbs_sm_info_list) {
            if (!cJSON_IsObject(n2_mbs_sm_info_list_local)) {
                ogs_error("OpenAPI_context_status_notification_parseFromJSON() failed [n2_mbs_sm_info_list]");
                goto end;
            }
            OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info_listItem = OpenAPI_n2_mbs_sm_info_parseFromJSON(n2_mbs_sm_info_list_local);
            if (!n2_mbs_sm_info_listItem) {
                ogs_error("No n2_mbs_sm_info_listItem");
                goto end;
            }
            OpenAPI_list_add(n2_mbs_sm_info_listList, n2_mbs_sm_info_listItem);
        }
    }

    operation_events = cJSON_GetObjectItemCaseSensitive(context_status_notificationJSON, "operationEvents");
    if (operation_events) {
        cJSON *operation_events_local = NULL;
        if (!cJSON_IsArray(operation_events)) {
            ogs_error("OpenAPI_context_status_notification_parseFromJSON() failed [operation_events]");
            goto end;
        }

        operation_eventsList = OpenAPI_list_create();

        cJSON_ArrayForEach(operation_events_local, operation_events) {
            if (!cJSON_IsObject(operation_events_local)) {
                ogs_error("OpenAPI_context_status_notification_parseFromJSON() failed [operation_events]");
                goto end;
            }
            OpenAPI_operation_event_t *operation_eventsItem = OpenAPI_operation_event_parseFromJSON(operation_events_local);
            if (!operation_eventsItem) {
                ogs_error("No operation_eventsItem");
                goto end;
            }
            OpenAPI_list_add(operation_eventsList, operation_eventsItem);
        }
    }

    operation_status = cJSON_GetObjectItemCaseSensitive(context_status_notificationJSON, "operationStatus");
    if (operation_status) {
    if (!cJSON_IsString(operation_status)) {
        ogs_error("OpenAPI_context_status_notification_parseFromJSON() failed [operation_status]");
        goto end;
    }
    operation_statusVariable = OpenAPI_operation_status_FromString(operation_status->valuestring);
    }

    released_ind = cJSON_GetObjectItemCaseSensitive(context_status_notificationJSON, "releasedInd");
    if (released_ind) {
    if (!cJSON_IsString(released_ind)) {
        ogs_error("OpenAPI_context_status_notification_parseFromJSON() failed [released_ind]");
        goto end;
    }
    released_indVariable = OpenAPI_released_indcontext_status_notification_FromString(released_ind->valuestring);
    }

    context_status_notification_local_var = OpenAPI_context_status_notification_create (
        mbs_session_id_local_nonprim,
        area_session_id ? true : false,
        area_session_id ? area_session_id->valuedouble : 0,
        n2_mbs_sm_info_list ? n2_mbs_sm_info_listList : NULL,
        operation_events ? operation_eventsList : NULL,
        operation_status ? operation_statusVariable : 0,
        released_ind ? released_indVariable : 0
    );

    return context_status_notification_local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (n2_mbs_sm_info_listList) {
        OpenAPI_list_for_each(n2_mbs_sm_info_listList, node) {
            OpenAPI_n2_mbs_sm_info_free(node->data);
        }
        OpenAPI_list_free(n2_mbs_sm_info_listList);
        n2_mbs_sm_info_listList = NULL;
    }
    if (operation_eventsList) {
        OpenAPI_list_for_each(operation_eventsList, node) {
            OpenAPI_operation_event_free(node->data);
        }
        OpenAPI_list_free(operation_eventsList);
        operation_eventsList = NULL;
    }
    return NULL;
}

OpenAPI_context_status_notification_t *OpenAPI_context_status_notification_copy(OpenAPI_context_status_notification_t *dst, OpenAPI_context_status_notification_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_status_notification_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_status_notification_convertToJSON() failed");
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

    OpenAPI_context_status_notification_free(dst);
    dst = OpenAPI_context_status_notification_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

