
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_session_subscription.h"

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
)
{
    OpenAPI_mbs_session_subscription_t *mbs_session_subscription_local_var = ogs_malloc(sizeof(OpenAPI_mbs_session_subscription_t));
    ogs_assert(mbs_session_subscription_local_var);

    mbs_session_subscription_local_var->mbs_session_id = mbs_session_id;
    mbs_session_subscription_local_var->is_area_session_id = is_area_session_id;
    mbs_session_subscription_local_var->area_session_id = area_session_id;
    mbs_session_subscription_local_var->event_list = event_list;
    mbs_session_subscription_local_var->notify_uri = notify_uri;
    mbs_session_subscription_local_var->notify_correlation_id = notify_correlation_id;
    mbs_session_subscription_local_var->expiry_time = expiry_time;
    mbs_session_subscription_local_var->nfc_instance_id = nfc_instance_id;
    mbs_session_subscription_local_var->mbs_session_subsc_uri = mbs_session_subsc_uri;

    return mbs_session_subscription_local_var;
}

void OpenAPI_mbs_session_subscription_free(OpenAPI_mbs_session_subscription_t *mbs_session_subscription)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_session_subscription) {
        return;
    }
    if (mbs_session_subscription->mbs_session_id) {
        OpenAPI_mbs_session_id_free(mbs_session_subscription->mbs_session_id);
        mbs_session_subscription->mbs_session_id = NULL;
    }
    if (mbs_session_subscription->event_list) {
        OpenAPI_list_for_each(mbs_session_subscription->event_list, node) {
            OpenAPI_mbs_session_event_free(node->data);
        }
        OpenAPI_list_free(mbs_session_subscription->event_list);
        mbs_session_subscription->event_list = NULL;
    }
    if (mbs_session_subscription->notify_uri) {
        ogs_free(mbs_session_subscription->notify_uri);
        mbs_session_subscription->notify_uri = NULL;
    }
    if (mbs_session_subscription->notify_correlation_id) {
        ogs_free(mbs_session_subscription->notify_correlation_id);
        mbs_session_subscription->notify_correlation_id = NULL;
    }
    if (mbs_session_subscription->expiry_time) {
        ogs_free(mbs_session_subscription->expiry_time);
        mbs_session_subscription->expiry_time = NULL;
    }
    if (mbs_session_subscription->nfc_instance_id) {
        ogs_free(mbs_session_subscription->nfc_instance_id);
        mbs_session_subscription->nfc_instance_id = NULL;
    }
    if (mbs_session_subscription->mbs_session_subsc_uri) {
        ogs_free(mbs_session_subscription->mbs_session_subsc_uri);
        mbs_session_subscription->mbs_session_subsc_uri = NULL;
    }
    ogs_free(mbs_session_subscription);
}

cJSON *OpenAPI_mbs_session_subscription_convertToJSON(OpenAPI_mbs_session_subscription_t *mbs_session_subscription)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_session_subscription == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [MbsSessionSubscription]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_session_subscription->mbs_session_id) {
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(mbs_session_subscription->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    }

    if (mbs_session_subscription->is_area_session_id) {
    if (cJSON_AddNumberToObject(item, "areaSessionId", mbs_session_subscription->area_session_id) == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [area_session_id]");
        goto end;
    }
    }

    if (!mbs_session_subscription->event_list) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [event_list]");
        return NULL;
    }
    cJSON *event_listList = cJSON_AddArrayToObject(item, "eventList");
    if (event_listList == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [event_list]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_session_subscription->event_list, node) {
        cJSON *itemLocal = OpenAPI_mbs_session_event_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [event_list]");
            goto end;
        }
        cJSON_AddItemToArray(event_listList, itemLocal);
    }

    if (!mbs_session_subscription->notify_uri) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [notify_uri]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "notifyUri", mbs_session_subscription->notify_uri) == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [notify_uri]");
        goto end;
    }

    if (mbs_session_subscription->notify_correlation_id) {
    if (cJSON_AddStringToObject(item, "notifyCorrelationId", mbs_session_subscription->notify_correlation_id) == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

    if (mbs_session_subscription->expiry_time) {
    if (cJSON_AddStringToObject(item, "expiryTime", mbs_session_subscription->expiry_time) == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [expiry_time]");
        goto end;
    }
    }

    if (mbs_session_subscription->nfc_instance_id) {
    if (cJSON_AddStringToObject(item, "nfcInstanceId", mbs_session_subscription->nfc_instance_id) == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [nfc_instance_id]");
        goto end;
    }
    }

    if (mbs_session_subscription->mbs_session_subsc_uri) {
    if (cJSON_AddStringToObject(item, "mbsSessionSubscUri", mbs_session_subscription->mbs_session_subsc_uri) == NULL) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed [mbs_session_subsc_uri]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_session_subscription_t *OpenAPI_mbs_session_subscription_parseFromJSON(cJSON *mbs_session_subscriptionJSON)
{
    OpenAPI_mbs_session_subscription_t *mbs_session_subscription_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *area_session_id = NULL;
    cJSON *event_list = NULL;
    OpenAPI_list_t *event_listList = NULL;
    cJSON *notify_uri = NULL;
    cJSON *notify_correlation_id = NULL;
    cJSON *expiry_time = NULL;
    cJSON *nfc_instance_id = NULL;
    cJSON *mbs_session_subsc_uri = NULL;
    mbs_session_id = cJSON_GetObjectItemCaseSensitive(mbs_session_subscriptionJSON, "mbsSessionId");
    if (mbs_session_id) {
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }
    }

    area_session_id = cJSON_GetObjectItemCaseSensitive(mbs_session_subscriptionJSON, "areaSessionId");
    if (area_session_id) {
    if (!cJSON_IsNumber(area_session_id)) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [area_session_id]");
        goto end;
    }
    }

    event_list = cJSON_GetObjectItemCaseSensitive(mbs_session_subscriptionJSON, "eventList");
    if (!event_list) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [event_list]");
        goto end;
    }
        cJSON *event_list_local = NULL;
        if (!cJSON_IsArray(event_list)) {
            ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [event_list]");
            goto end;
        }

        event_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(event_list_local, event_list) {
            if (!cJSON_IsObject(event_list_local)) {
                ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [event_list]");
                goto end;
            }
            OpenAPI_mbs_session_event_t *event_listItem = OpenAPI_mbs_session_event_parseFromJSON(event_list_local);
            if (!event_listItem) {
                ogs_error("No event_listItem");
                goto end;
            }
            OpenAPI_list_add(event_listList, event_listItem);
        }

    notify_uri = cJSON_GetObjectItemCaseSensitive(mbs_session_subscriptionJSON, "notifyUri");
    if (!notify_uri) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [notify_uri]");
        goto end;
    }
    if (!cJSON_IsString(notify_uri)) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [notify_uri]");
        goto end;
    }

    notify_correlation_id = cJSON_GetObjectItemCaseSensitive(mbs_session_subscriptionJSON, "notifyCorrelationId");
    if (notify_correlation_id) {
    if (!cJSON_IsString(notify_correlation_id) && !cJSON_IsNull(notify_correlation_id)) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [notify_correlation_id]");
        goto end;
    }
    }

    expiry_time = cJSON_GetObjectItemCaseSensitive(mbs_session_subscriptionJSON, "expiryTime");
    if (expiry_time) {
    if (!cJSON_IsString(expiry_time) && !cJSON_IsNull(expiry_time)) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [expiry_time]");
        goto end;
    }
    }

    nfc_instance_id = cJSON_GetObjectItemCaseSensitive(mbs_session_subscriptionJSON, "nfcInstanceId");
    if (nfc_instance_id) {
    if (!cJSON_IsString(nfc_instance_id) && !cJSON_IsNull(nfc_instance_id)) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [nfc_instance_id]");
        goto end;
    }
    }

    mbs_session_subsc_uri = cJSON_GetObjectItemCaseSensitive(mbs_session_subscriptionJSON, "mbsSessionSubscUri");
    if (mbs_session_subsc_uri) {
    if (!cJSON_IsString(mbs_session_subsc_uri) && !cJSON_IsNull(mbs_session_subsc_uri)) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON() failed [mbs_session_subsc_uri]");
        goto end;
    }
    }

    mbs_session_subscription_local_var = OpenAPI_mbs_session_subscription_create (
        mbs_session_id ? mbs_session_id_local_nonprim : NULL,
        area_session_id ? true : false,
        area_session_id ? area_session_id->valuedouble : 0,
        event_listList,
        ogs_strdup(notify_uri->valuestring),
        notify_correlation_id && !cJSON_IsNull(notify_correlation_id) ? ogs_strdup(notify_correlation_id->valuestring) : NULL,
        expiry_time && !cJSON_IsNull(expiry_time) ? ogs_strdup(expiry_time->valuestring) : NULL,
        nfc_instance_id && !cJSON_IsNull(nfc_instance_id) ? ogs_strdup(nfc_instance_id->valuestring) : NULL,
        mbs_session_subsc_uri && !cJSON_IsNull(mbs_session_subsc_uri) ? ogs_strdup(mbs_session_subsc_uri->valuestring) : NULL
    );

    return mbs_session_subscription_local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (event_listList) {
        OpenAPI_list_for_each(event_listList, node) {
            OpenAPI_mbs_session_event_free(node->data);
        }
        OpenAPI_list_free(event_listList);
        event_listList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_session_subscription_t *OpenAPI_mbs_session_subscription_copy(OpenAPI_mbs_session_subscription_t *dst, OpenAPI_mbs_session_subscription_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_session_subscription_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_session_subscription_convertToJSON() failed");
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

    OpenAPI_mbs_session_subscription_free(dst);
    dst = OpenAPI_mbs_session_subscription_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

