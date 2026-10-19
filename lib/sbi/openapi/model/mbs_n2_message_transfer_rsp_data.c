
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_n2_message_transfer_rsp_data.h"

OpenAPI_mbs_n2_message_transfer_rsp_data_t *OpenAPI_mbs_n2_message_transfer_rsp_data_create(
    OpenAPI_n2_information_transfer_result_e result,
    char *supported_features
)
{
    OpenAPI_mbs_n2_message_transfer_rsp_data_t *mbs_n2_message_transfer_rsp_data_local_var = ogs_malloc(sizeof(OpenAPI_mbs_n2_message_transfer_rsp_data_t));
    ogs_assert(mbs_n2_message_transfer_rsp_data_local_var);

    mbs_n2_message_transfer_rsp_data_local_var->result = result;
    mbs_n2_message_transfer_rsp_data_local_var->supported_features = supported_features;

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

    mbs_n2_message_transfer_rsp_data_local_var = OpenAPI_mbs_n2_message_transfer_rsp_data_create (
        resultVariable,
        supported_features && !cJSON_IsNull(supported_features) ? ogs_strdup(supported_features->valuestring) : NULL
    );

    return mbs_n2_message_transfer_rsp_data_local_var;
end:
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

