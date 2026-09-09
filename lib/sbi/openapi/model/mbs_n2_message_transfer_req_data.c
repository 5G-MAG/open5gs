
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_n2_message_transfer_req_data.h"

OpenAPI_mbs_n2_message_transfer_req_data_t *OpenAPI_mbs_n2_message_transfer_req_data_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_mbs_comm_n2_mbs_sm_info_t *n2_mbs_sm_info,
    char *supported_features
)
{
    OpenAPI_mbs_n2_message_transfer_req_data_t *mbs_n2_message_transfer_req_data_local_var = ogs_malloc(sizeof(OpenAPI_mbs_n2_message_transfer_req_data_t));
    ogs_assert(mbs_n2_message_transfer_req_data_local_var);

    mbs_n2_message_transfer_req_data_local_var->mbs_session_id = mbs_session_id;
    mbs_n2_message_transfer_req_data_local_var->is_area_session_id = is_area_session_id;
    mbs_n2_message_transfer_req_data_local_var->area_session_id = area_session_id;
    mbs_n2_message_transfer_req_data_local_var->n2_mbs_sm_info = n2_mbs_sm_info;
    mbs_n2_message_transfer_req_data_local_var->supported_features = supported_features;

    return mbs_n2_message_transfer_req_data_local_var;
}

void OpenAPI_mbs_n2_message_transfer_req_data_free(OpenAPI_mbs_n2_message_transfer_req_data_t *mbs_n2_message_transfer_req_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_n2_message_transfer_req_data) {
        return;
    }
    if (mbs_n2_message_transfer_req_data->mbs_session_id) {
        OpenAPI_mbs_session_id_free(mbs_n2_message_transfer_req_data->mbs_session_id);
        mbs_n2_message_transfer_req_data->mbs_session_id = NULL;
    }
    if (mbs_n2_message_transfer_req_data->n2_mbs_sm_info) {
        OpenAPI_mbs_comm_n2_mbs_sm_info_free(mbs_n2_message_transfer_req_data->n2_mbs_sm_info);
        mbs_n2_message_transfer_req_data->n2_mbs_sm_info = NULL;
    }
    if (mbs_n2_message_transfer_req_data->supported_features) {
        ogs_free(mbs_n2_message_transfer_req_data->supported_features);
        mbs_n2_message_transfer_req_data->supported_features = NULL;
    }
    ogs_free(mbs_n2_message_transfer_req_data);
}

cJSON *OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON(OpenAPI_mbs_n2_message_transfer_req_data_t *mbs_n2_message_transfer_req_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_n2_message_transfer_req_data == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed [MbsN2MessageTransferReqData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_n2_message_transfer_req_data->mbs_session_id) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed [mbs_session_id]");
        return NULL;
    }
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(mbs_n2_message_transfer_req_data->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }

    if (mbs_n2_message_transfer_req_data->is_area_session_id) {
    if (cJSON_AddNumberToObject(item, "areaSessionId", mbs_n2_message_transfer_req_data->area_session_id) == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed [area_session_id]");
        goto end;
    }
    }

    if (!mbs_n2_message_transfer_req_data->n2_mbs_sm_info) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        return NULL;
    }
    cJSON *n2_mbs_sm_info_local_JSON = OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON(mbs_n2_message_transfer_req_data->n2_mbs_sm_info);
    if (n2_mbs_sm_info_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "n2MbsSmInfo", n2_mbs_sm_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }

    if (mbs_n2_message_transfer_req_data->supported_features) {
    if (cJSON_AddStringToObject(item, "supportedFeatures", mbs_n2_message_transfer_req_data->supported_features) == NULL) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed [supported_features]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_n2_message_transfer_req_data_t *OpenAPI_mbs_n2_message_transfer_req_data_parseFromJSON(cJSON *mbs_n2_message_transfer_req_dataJSON)
{
    OpenAPI_mbs_n2_message_transfer_req_data_t *mbs_n2_message_transfer_req_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *area_session_id = NULL;
    cJSON *n2_mbs_sm_info = NULL;
    OpenAPI_mbs_comm_n2_mbs_sm_info_t *n2_mbs_sm_info_local_nonprim = NULL;
    cJSON *supported_features = NULL;
    mbs_session_id = cJSON_GetObjectItemCaseSensitive(mbs_n2_message_transfer_req_dataJSON, "mbsSessionId");
    if (!mbs_session_id) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_parseFromJSON() failed [mbs_session_id]");
        goto end;
    }
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }

    area_session_id = cJSON_GetObjectItemCaseSensitive(mbs_n2_message_transfer_req_dataJSON, "areaSessionId");
    if (area_session_id) {
    if (!cJSON_IsNumber(area_session_id)) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_parseFromJSON() failed [area_session_id]");
        goto end;
    }
    }

    n2_mbs_sm_info = cJSON_GetObjectItemCaseSensitive(mbs_n2_message_transfer_req_dataJSON, "n2MbsSmInfo");
    if (!n2_mbs_sm_info) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_parseFromJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    n2_mbs_sm_info_local_nonprim = OpenAPI_mbs_comm_n2_mbs_sm_info_parseFromJSON(n2_mbs_sm_info);
    if (!n2_mbs_sm_info_local_nonprim) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_parseFromJSON failed [n2_mbs_sm_info]");
        goto end;
    }

    supported_features = cJSON_GetObjectItemCaseSensitive(mbs_n2_message_transfer_req_dataJSON, "supportedFeatures");
    if (supported_features) {
    if (!cJSON_IsString(supported_features) && !cJSON_IsNull(supported_features)) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_parseFromJSON() failed [supported_features]");
        goto end;
    }
    }

    mbs_n2_message_transfer_req_data_local_var = OpenAPI_mbs_n2_message_transfer_req_data_create (
        mbs_session_id_local_nonprim,
        area_session_id ? true : false,
        area_session_id ? area_session_id->valuedouble : 0,
        n2_mbs_sm_info_local_nonprim,
        supported_features && !cJSON_IsNull(supported_features) ? ogs_strdup(supported_features->valuestring) : NULL
    );

    return mbs_n2_message_transfer_req_data_local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (n2_mbs_sm_info_local_nonprim) {
        OpenAPI_mbs_comm_n2_mbs_sm_info_free(n2_mbs_sm_info_local_nonprim);
        n2_mbs_sm_info_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_mbs_n2_message_transfer_req_data_t *OpenAPI_mbs_n2_message_transfer_req_data_copy(OpenAPI_mbs_n2_message_transfer_req_data_t *dst, OpenAPI_mbs_n2_message_transfer_req_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_n2_message_transfer_req_data_convertToJSON() failed");
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

    OpenAPI_mbs_n2_message_transfer_req_data_free(dst);
    dst = OpenAPI_mbs_n2_message_transfer_req_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

