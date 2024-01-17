
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_status_subscribe_rsp_data.h"

OpenAPI_context_status_subscribe_rsp_data_t *OpenAPI_context_status_subscribe_rsp_data_create(
    OpenAPI_context_status_subscription_t *subscription,
    OpenAPI_list_t *report_list,
    OpenAPI_mbs_context_info_t *mbs_context_info
)
{
    OpenAPI_context_status_subscribe_rsp_data_t *context_status_subscribe_rsp_data_local_var = ogs_malloc(sizeof(OpenAPI_context_status_subscribe_rsp_data_t));
    ogs_assert(context_status_subscribe_rsp_data_local_var);

    context_status_subscribe_rsp_data_local_var->subscription = subscription;
    context_status_subscribe_rsp_data_local_var->report_list = report_list;
    context_status_subscribe_rsp_data_local_var->mbs_context_info = mbs_context_info;

    return context_status_subscribe_rsp_data_local_var;
}

void OpenAPI_context_status_subscribe_rsp_data_free(OpenAPI_context_status_subscribe_rsp_data_t *context_status_subscribe_rsp_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_status_subscribe_rsp_data) {
        return;
    }
    if (context_status_subscribe_rsp_data->subscription) {
        OpenAPI_context_status_subscription_free(context_status_subscribe_rsp_data->subscription);
        context_status_subscribe_rsp_data->subscription = NULL;
    }
    if (context_status_subscribe_rsp_data->report_list) {
        OpenAPI_list_for_each(context_status_subscribe_rsp_data->report_list, node) {
            OpenAPI_context_status_event_report_free(node->data);
        }
        OpenAPI_list_free(context_status_subscribe_rsp_data->report_list);
        context_status_subscribe_rsp_data->report_list = NULL;
    }
    if (context_status_subscribe_rsp_data->mbs_context_info) {
        OpenAPI_mbs_context_info_free(context_status_subscribe_rsp_data->mbs_context_info);
        context_status_subscribe_rsp_data->mbs_context_info = NULL;
    }
    ogs_free(context_status_subscribe_rsp_data);
}

cJSON *OpenAPI_context_status_subscribe_rsp_data_convertToJSON(OpenAPI_context_status_subscribe_rsp_data_t *context_status_subscribe_rsp_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_status_subscribe_rsp_data == NULL) {
        ogs_error("OpenAPI_context_status_subscribe_rsp_data_convertToJSON() failed [ContextStatusSubscribeRspData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!context_status_subscribe_rsp_data->subscription) {
        ogs_error("OpenAPI_context_status_subscribe_rsp_data_convertToJSON() failed [subscription]");
        return NULL;
    }
    cJSON *subscription_local_JSON = OpenAPI_context_status_subscription_convertToJSON(context_status_subscribe_rsp_data->subscription);
    if (subscription_local_JSON == NULL) {
        ogs_error("OpenAPI_context_status_subscribe_rsp_data_convertToJSON() failed [subscription]");
        goto end;
    }
    cJSON_AddItemToObject(item, "subscription", subscription_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_status_subscribe_rsp_data_convertToJSON() failed [subscription]");
        goto end;
    }

    if (context_status_subscribe_rsp_data->report_list) {
    cJSON *report_listList = cJSON_AddArrayToObject(item, "reportList");
    if (report_listList == NULL) {
        ogs_error("OpenAPI_context_status_subscribe_rsp_data_convertToJSON() failed [report_list]");
        goto end;
    }
    OpenAPI_list_for_each(context_status_subscribe_rsp_data->report_list, node) {
        cJSON *itemLocal = OpenAPI_context_status_event_report_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_context_status_subscribe_rsp_data_convertToJSON() failed [report_list]");
            goto end;
        }
        cJSON_AddItemToArray(report_listList, itemLocal);
    }
    }

    if (context_status_subscribe_rsp_data->mbs_context_info) {
    cJSON *mbs_context_info_local_JSON = OpenAPI_mbs_context_info_convertToJSON(context_status_subscribe_rsp_data->mbs_context_info);
    if (mbs_context_info_local_JSON == NULL) {
        ogs_error("OpenAPI_context_status_subscribe_rsp_data_convertToJSON() failed [mbs_context_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsContextInfo", mbs_context_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_status_subscribe_rsp_data_convertToJSON() failed [mbs_context_info]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_status_subscribe_rsp_data_t *OpenAPI_context_status_subscribe_rsp_data_parseFromJSON(cJSON *context_status_subscribe_rsp_dataJSON)
{
    OpenAPI_context_status_subscribe_rsp_data_t *context_status_subscribe_rsp_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *subscription = NULL;
    OpenAPI_context_status_subscription_t *subscription_local_nonprim = NULL;
    cJSON *report_list = NULL;
    OpenAPI_list_t *report_listList = NULL;
    cJSON *mbs_context_info = NULL;
    OpenAPI_mbs_context_info_t *mbs_context_info_local_nonprim = NULL;
    subscription = cJSON_GetObjectItemCaseSensitive(context_status_subscribe_rsp_dataJSON, "subscription");
    if (!subscription) {
        ogs_error("OpenAPI_context_status_subscribe_rsp_data_parseFromJSON() failed [subscription]");
        goto end;
    }
    subscription_local_nonprim = OpenAPI_context_status_subscription_parseFromJSON(subscription);
    if (!subscription_local_nonprim) {
        ogs_error("OpenAPI_context_status_subscription_parseFromJSON failed [subscription]");
        goto end;
    }

    report_list = cJSON_GetObjectItemCaseSensitive(context_status_subscribe_rsp_dataJSON, "reportList");
    if (report_list) {
        cJSON *report_list_local = NULL;
        if (!cJSON_IsArray(report_list)) {
            ogs_error("OpenAPI_context_status_subscribe_rsp_data_parseFromJSON() failed [report_list]");
            goto end;
        }

        report_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(report_list_local, report_list) {
            if (!cJSON_IsObject(report_list_local)) {
                ogs_error("OpenAPI_context_status_subscribe_rsp_data_parseFromJSON() failed [report_list]");
                goto end;
            }
            OpenAPI_context_status_event_report_t *report_listItem = OpenAPI_context_status_event_report_parseFromJSON(report_list_local);
            if (!report_listItem) {
                ogs_error("No report_listItem");
                goto end;
            }
            OpenAPI_list_add(report_listList, report_listItem);
        }
    }

    mbs_context_info = cJSON_GetObjectItemCaseSensitive(context_status_subscribe_rsp_dataJSON, "mbsContextInfo");
    if (mbs_context_info) {
    mbs_context_info_local_nonprim = OpenAPI_mbs_context_info_parseFromJSON(mbs_context_info);
    if (!mbs_context_info_local_nonprim) {
        ogs_error("OpenAPI_mbs_context_info_parseFromJSON failed [mbs_context_info]");
        goto end;
    }
    }

    context_status_subscribe_rsp_data_local_var = OpenAPI_context_status_subscribe_rsp_data_create (
        subscription_local_nonprim,
        report_list ? report_listList : NULL,
        mbs_context_info ? mbs_context_info_local_nonprim : NULL
    );

    return context_status_subscribe_rsp_data_local_var;
end:
    if (subscription_local_nonprim) {
        OpenAPI_context_status_subscription_free(subscription_local_nonprim);
        subscription_local_nonprim = NULL;
    }
    if (report_listList) {
        OpenAPI_list_for_each(report_listList, node) {
            OpenAPI_context_status_event_report_free(node->data);
        }
        OpenAPI_list_free(report_listList);
        report_listList = NULL;
    }
    if (mbs_context_info_local_nonprim) {
        OpenAPI_mbs_context_info_free(mbs_context_info_local_nonprim);
        mbs_context_info_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_context_status_subscribe_rsp_data_t *OpenAPI_context_status_subscribe_rsp_data_copy(OpenAPI_context_status_subscribe_rsp_data_t *dst, OpenAPI_context_status_subscribe_rsp_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_status_subscribe_rsp_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_status_subscribe_rsp_data_convertToJSON() failed");
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

    OpenAPI_context_status_subscribe_rsp_data_free(dst);
    dst = OpenAPI_context_status_subscribe_rsp_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

