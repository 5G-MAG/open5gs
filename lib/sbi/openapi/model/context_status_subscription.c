
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_status_subscription.h"

OpenAPI_context_status_subscription_t *OpenAPI_context_status_subscription_create(
    char *nfc_instance_id,
    OpenAPI_mbs_session_id_t *mbs_session_id,
    OpenAPI_list_t *event_list,
    char *notify_uri,
    char *notify_correlation_id,
    char *expiry_time
)
{
    OpenAPI_context_status_subscription_t *context_status_subscription_local_var = ogs_malloc(sizeof(OpenAPI_context_status_subscription_t));
    ogs_assert(context_status_subscription_local_var);

    context_status_subscription_local_var->nfc_instance_id = nfc_instance_id;
    context_status_subscription_local_var->mbs_session_id = mbs_session_id;
    context_status_subscription_local_var->event_list = event_list;
    context_status_subscription_local_var->notify_uri = notify_uri;
    context_status_subscription_local_var->notify_correlation_id = notify_correlation_id;
    context_status_subscription_local_var->expiry_time = expiry_time;

    return context_status_subscription_local_var;
}

void OpenAPI_context_status_subscription_free(OpenAPI_context_status_subscription_t *context_status_subscription)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_status_subscription) {
        return;
    }
    if (context_status_subscription->nfc_instance_id) {
        ogs_free(context_status_subscription->nfc_instance_id);
        context_status_subscription->nfc_instance_id = NULL;
    }
    if (context_status_subscription->mbs_session_id) {
        OpenAPI_mbs_session_id_free(context_status_subscription->mbs_session_id);
        context_status_subscription->mbs_session_id = NULL;
    }
    if (context_status_subscription->event_list) {
        OpenAPI_list_for_each(context_status_subscription->event_list, node) {
            OpenAPI_context_status_event_free(node->data);
        }
        OpenAPI_list_free(context_status_subscription->event_list);
        context_status_subscription->event_list = NULL;
    }
    if (context_status_subscription->notify_uri) {
        ogs_free(context_status_subscription->notify_uri);
        context_status_subscription->notify_uri = NULL;
    }
    if (context_status_subscription->notify_correlation_id) {
        ogs_free(context_status_subscription->notify_correlation_id);
        context_status_subscription->notify_correlation_id = NULL;
    }
    if (context_status_subscription->expiry_time) {
        ogs_free(context_status_subscription->expiry_time);
        context_status_subscription->expiry_time = NULL;
    }
    ogs_free(context_status_subscription);
}

cJSON *OpenAPI_context_status_subscription_convertToJSON(OpenAPI_context_status_subscription_t *context_status_subscription)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_status_subscription == NULL) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [ContextStatusSubscription]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!context_status_subscription->nfc_instance_id) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [nfc_instance_id]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "nfcInstanceId", context_status_subscription->nfc_instance_id) == NULL) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [nfc_instance_id]");
        goto end;
    }

    if (!context_status_subscription->mbs_session_id) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [mbs_session_id]");
        return NULL;
    }
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(context_status_subscription->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [mbs_session_id]");
        goto end;
    }

    if (!context_status_subscription->event_list) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [event_list]");
        return NULL;
    }
    cJSON *event_listList = cJSON_AddArrayToObject(item, "eventList");
    if (event_listList == NULL) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [event_list]");
        goto end;
    }
    OpenAPI_list_for_each(context_status_subscription->event_list, node) {
        cJSON *itemLocal = OpenAPI_context_status_event_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [event_list]");
            goto end;
        }
        cJSON_AddItemToArray(event_listList, itemLocal);
    }

    if (!context_status_subscription->notify_uri) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [notify_uri]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "notifyUri", context_status_subscription->notify_uri) == NULL) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [notify_uri]");
        goto end;
    }

    if (context_status_subscription->notify_correlation_id) {
    if (cJSON_AddStringToObject(item, "notifyCorrelationId", context_status_subscription->notify_correlation_id) == NULL) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

    if (context_status_subscription->expiry_time) {
    if (cJSON_AddStringToObject(item, "expiryTime", context_status_subscription->expiry_time) == NULL) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed [expiry_time]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_status_subscription_t *OpenAPI_context_status_subscription_parseFromJSON(cJSON *context_status_subscriptionJSON)
{
    OpenAPI_context_status_subscription_t *context_status_subscription_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *nfc_instance_id = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *event_list = NULL;
    OpenAPI_list_t *event_listList = NULL;
    cJSON *notify_uri = NULL;
    cJSON *notify_correlation_id = NULL;
    cJSON *expiry_time = NULL;
    nfc_instance_id = cJSON_GetObjectItemCaseSensitive(context_status_subscriptionJSON, "nfcInstanceId");
    if (!nfc_instance_id) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [nfc_instance_id]");
        goto end;
    }
    if (!cJSON_IsString(nfc_instance_id)) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [nfc_instance_id]");
        goto end;
    }

    mbs_session_id = cJSON_GetObjectItemCaseSensitive(context_status_subscriptionJSON, "mbsSessionId");
    if (!mbs_session_id) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [mbs_session_id]");
        goto end;
    }
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }

    event_list = cJSON_GetObjectItemCaseSensitive(context_status_subscriptionJSON, "eventList");
    if (!event_list) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [event_list]");
        goto end;
    }
        cJSON *event_list_local = NULL;
        if (!cJSON_IsArray(event_list)) {
            ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [event_list]");
            goto end;
        }

        event_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(event_list_local, event_list) {
            if (!cJSON_IsObject(event_list_local)) {
                ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [event_list]");
                goto end;
            }
            OpenAPI_context_status_event_t *event_listItem = OpenAPI_context_status_event_parseFromJSON(event_list_local);
            if (!event_listItem) {
                ogs_error("No event_listItem");
                goto end;
            }
            OpenAPI_list_add(event_listList, event_listItem);
        }

    notify_uri = cJSON_GetObjectItemCaseSensitive(context_status_subscriptionJSON, "notifyUri");
    if (!notify_uri) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [notify_uri]");
        goto end;
    }
    if (!cJSON_IsString(notify_uri)) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [notify_uri]");
        goto end;
    }

    notify_correlation_id = cJSON_GetObjectItemCaseSensitive(context_status_subscriptionJSON, "notifyCorrelationId");
    if (notify_correlation_id) {
    if (!cJSON_IsString(notify_correlation_id) && !cJSON_IsNull(notify_correlation_id)) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

    expiry_time = cJSON_GetObjectItemCaseSensitive(context_status_subscriptionJSON, "expiryTime");
    if (expiry_time) {
    if (!cJSON_IsString(expiry_time) && !cJSON_IsNull(expiry_time)) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON() failed [expiry_time]");
        goto end;
    }
    }

    context_status_subscription_local_var = OpenAPI_context_status_subscription_create (
        ogs_strdup(nfc_instance_id->valuestring),
        mbs_session_id_local_nonprim,
        event_listList,
        ogs_strdup(notify_uri->valuestring),
        notify_correlation_id && !cJSON_IsNull(notify_correlation_id) ? ogs_strdup(notify_correlation_id->valuestring) : NULL,
        expiry_time && !cJSON_IsNull(expiry_time) ? ogs_strdup(expiry_time->valuestring) : NULL
    );

    return context_status_subscription_local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (event_listList) {
        OpenAPI_list_for_each(event_listList, node) {
            OpenAPI_context_status_event_free(node->data);
        }
        OpenAPI_list_free(event_listList);
        event_listList = NULL;
    }
    return NULL;
}

OpenAPI_context_status_subscription_t *OpenAPI_context_status_subscription_copy(OpenAPI_context_status_subscription_t *dst, OpenAPI_context_status_subscription_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_status_subscription_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_status_subscription_convertToJSON() failed");
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

    OpenAPI_context_status_subscription_free(dst);
    dst = OpenAPI_context_status_subscription_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

