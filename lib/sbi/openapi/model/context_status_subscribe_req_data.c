
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_status_subscribe_req_data.h"

OpenAPI_context_status_subscribe_req_data_t *OpenAPI_context_status_subscribe_req_data_create(
    OpenAPI_context_status_subscription_t *subscription
)
{
    OpenAPI_context_status_subscribe_req_data_t *context_status_subscribe_req_data_local_var = ogs_malloc(sizeof(OpenAPI_context_status_subscribe_req_data_t));
    ogs_assert(context_status_subscribe_req_data_local_var);

    context_status_subscribe_req_data_local_var->subscription = subscription;

    return context_status_subscribe_req_data_local_var;
}

void OpenAPI_context_status_subscribe_req_data_free(OpenAPI_context_status_subscribe_req_data_t *context_status_subscribe_req_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_status_subscribe_req_data) {
        return;
    }
    if (context_status_subscribe_req_data->subscription) {
        OpenAPI_context_status_subscription_free(context_status_subscribe_req_data->subscription);
        context_status_subscribe_req_data->subscription = NULL;
    }
    ogs_free(context_status_subscribe_req_data);
}

cJSON *OpenAPI_context_status_subscribe_req_data_convertToJSON(OpenAPI_context_status_subscribe_req_data_t *context_status_subscribe_req_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_status_subscribe_req_data == NULL) {
        ogs_error("OpenAPI_context_status_subscribe_req_data_convertToJSON() failed [ContextStatusSubscribeReqData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!context_status_subscribe_req_data->subscription) {
        ogs_error("OpenAPI_context_status_subscribe_req_data_convertToJSON() failed [subscription]");
        return NULL;
    }
    cJSON *subscription_local_JSON = OpenAPI_context_status_subscription_convertToJSON(context_status_subscribe_req_data->subscription);
    if (subscription_local_JSON == NULL) {
        ogs_error("OpenAPI_context_status_subscribe_req_data_convertToJSON() failed [subscription]");
        goto end;
    }
    cJSON_AddItemToObject(item, "subscription", subscription_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_status_subscribe_req_data_convertToJSON() failed [subscription]");
        goto end;
    }

end:
    return item;
}

OpenAPI_context_status_subscribe_req_data_t *OpenAPI_context_status_subscribe_req_data_parseFromJSON(cJSON *context_status_subscribe_req_dataJSON)
{
    OpenAPI_context_status_subscribe_req_data_t *context_status_subscribe_req_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *subscription = NULL;
    OpenAPI_context_status_subscription_t *subscription_local_nonprim = NULL;
    subscription = cJSON_GetObjectItemCaseSensitive(context_status_subscribe_req_dataJSON, "subscription");
    if (!subscription) {
        ogs_error("OpenAPI_context_status_subscribe_req_data_parseFromJSON() failed [subscription]");
        goto end;
    }
    subscription_local_nonprim = OpenAPI_context_status_subscription_parseFromJSON(subscription);
    if (!subscription_local_nonprim) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON failed [subscription]");
        goto end;
    }

    context_status_subscribe_req_data_local_var = OpenAPI_context_status_subscribe_req_data_create (
        subscription_local_nonprim
    );

    return context_status_subscribe_req_data_local_var;
end:
    if (subscription_local_nonprim) {
        OpenAPI_context_status_subscription_free(subscription_local_nonprim);
        subscription_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_context_status_subscribe_req_data_t *OpenAPI_context_status_subscribe_req_data_copy(OpenAPI_context_status_subscribe_req_data_t *dst, OpenAPI_context_status_subscribe_req_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_status_subscribe_req_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_status_subscribe_req_data_convertToJSON() failed");
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

    OpenAPI_context_status_subscribe_req_data_free(dst);
    dst = OpenAPI_context_status_subscribe_req_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

