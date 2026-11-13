
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_n2_message_transfer_rsp_data.h"

OpenAPI_mbs_n2_message_transfer_rsp_data_t *OpenAPI_mbs_n2_message_transfer_rsp_data_create(
    OpenAPI_n2_information_transfer_result_e result,
    char *supported_features,
    OpenAPI_list_t *failure_list
)
{
    OpenAPI_mbs_n2_message_transfer_rsp_data_t *mbs_n2_message_transfer_rsp_data_local_var = ogs_malloc(sizeof(OpenAPI_mbs_n2_message_transfer_rsp_data_t));
    ogs_assert(mbs_n2_message_transfer_rsp_data_local_var);

    mbs_n2_message_transfer_rsp_data_local_var->result = result;
    mbs_n2_message_transfer_rsp_data_local_var->supported_features = supported_features;
    mbs_n2_message_transfer_rsp_data_local_var->failure_list = failure_list;

    return mbs_n2_message_transfer_rsp_data_local_var;
}

void OpenAPI_mbs_n2_message_transfer_rsp_data_free(OpenAPI_mbs_n2_message_transfer_rsp_data_t *mbs_n2_message_transfer_rsp_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_n2_message_transfer_rsp_data) {
        return;
    }
    if (mbs_n2_message_transfer_rsp_data->supported_features) {
        ogs_free(mbs_n2_message_transfer_rsp_data->supported_features);
        mbs_n2_message_transfer_rsp_data->supported_features = NULL;
    }
    if (mbs_n2_message_transfer_rsp_data->failure_list) {
        OpenAPI_list_for_each(mbs_n2_message_transfer_rsp_data->failure_list, node) {
            OpenAPI_ran_failure_free(node->data);
        }
        OpenAPI_list_free(mbs_n2_message_transfer_rsp_data->failure_list);
        mbs_n2_message_transfer_rsp_data->failure_list = NULL;
    }
    ogs_free(mbs_n2_message_transfer_rsp_data);
}

cJSON *OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON(OpenAPI_mbs_n2_message_transfer_rsp_data_t *mbs_n2_message_transfer_rsp_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_n2_message_transfer_rsp_data == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON() failed [MbsN2MessageTransferRspData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_n2_message_transfer_rsp_data->result == OpenAPI_n2_information_transfer_result_NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON() failed [result]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "result", OpenAPI_n2_information_transfer_result_ToString(mbs_n2_message_transfer_rsp_data->result)) == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON() failed [result]");
        goto end;
    }

    if (mbs_n2_message_transfer_rsp_data->supported_features) {
    if (cJSON_AddStringToObject(item, "supportedFeatures", mbs_n2_message_transfer_rsp_data->supported_features) == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON() failed [supported_features]");
        goto end;
    }
    }

    if (mbs_n2_message_transfer_rsp_data->failure_list) {
    cJSON *failure_listList = cJSON_AddArrayToObject(item, "failureList");
    if (failure_listList == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON() failed [failure_list]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_n2_message_transfer_rsp_data->failure_list, node) {
        cJSON *itemLocal = OpenAPI_ran_failure_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON() failed [failure_list]");
            goto end;
        }
        cJSON_AddItemToArray(failure_listList, itemLocal);
    }
    }

end:
    return item;
}

OpenAPI_mbs_n2_message_transfer_rsp_data_t *OpenAPI_mbs_n2_message_transfer_rsp_data_parseFromJSON(cJSON *mbs_n2_message_transfer_rsp_dataJSON)
{
    OpenAPI_mbs_n2_message_transfer_rsp_data_t *mbs_n2_message_transfer_rsp_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *result = NULL;
    OpenAPI_n2_information_transfer_result_e resultVariable = 0;
    cJSON *supported_features = NULL;
    cJSON *failure_list = NULL;
    OpenAPI_list_t *failure_listList = NULL;
    result = cJSON_GetObjectItemCaseSensitive(mbs_n2_message_transfer_rsp_dataJSON, "result");
    if (!result) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_parseFromJSON() failed [result]");
        goto end;
    }
    if (!cJSON_IsString(result)) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_parseFromJSON() failed [result]");
        goto end;
    }
    resultVariable = OpenAPI_n2_information_transfer_result_FromString(result->valuestring);

    supported_features = cJSON_GetObjectItemCaseSensitive(mbs_n2_message_transfer_rsp_dataJSON, "supportedFeatures");
    if (supported_features) {
    if (!cJSON_IsString(supported_features) && !cJSON_IsNull(supported_features)) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_parseFromJSON() failed [supported_features]");
        goto end;
    }
    }

    failure_list = cJSON_GetObjectItemCaseSensitive(mbs_n2_message_transfer_rsp_dataJSON, "failureList");
    if (failure_list) {
        cJSON *failure_list_local = NULL;
        if (!cJSON_IsArray(failure_list)) {
            ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_parseFromJSON() failed [failure_list]");
            goto end;
        }

        failure_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(failure_list_local, failure_list) {
            if (!cJSON_IsObject(failure_list_local)) {
                ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_parseFromJSON() failed [failure_list]");
                goto end;
            }
            OpenAPI_ran_failure_t *failure_listItem = OpenAPI_ran_failure_parseFromJSON(failure_list_local);
            if (!failure_listItem) {
                ogs_error("No failure_listItem");
                goto end;
            }
            OpenAPI_list_add(failure_listList, failure_listItem);
        }
    }

    mbs_n2_message_transfer_rsp_data_local_var = OpenAPI_mbs_n2_message_transfer_rsp_data_create (
        resultVariable,
        supported_features && !cJSON_IsNull(supported_features) ? ogs_strdup(supported_features->valuestring) : NULL,
        failure_list ? failure_listList : NULL
    );

    return mbs_n2_message_transfer_rsp_data_local_var;
end:
    if (failure_listList) {
        OpenAPI_list_for_each(failure_listList, node) {
            OpenAPI_ran_failure_free(node->data);
        }
        OpenAPI_list_free(failure_listList);
        failure_listList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_n2_message_transfer_rsp_data_t *OpenAPI_mbs_n2_message_transfer_rsp_data_copy(OpenAPI_mbs_n2_message_transfer_rsp_data_t *dst, OpenAPI_mbs_n2_message_transfer_rsp_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON() failed");
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

    OpenAPI_mbs_n2_message_transfer_rsp_data_free(dst);
    dst = OpenAPI_mbs_n2_message_transfer_rsp_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

