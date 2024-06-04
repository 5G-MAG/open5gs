
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_create_req_data.h"

OpenAPI_context_create_req_data_t *OpenAPI_context_create_req_data_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    OpenAPI_list_t *mbs_service_area_info_list,
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info,
    char *notify_uri,
    bool is_max_response_time,
    int max_response_time,
    OpenAPI_snssai_t *snssai,
    char *mbsmf_id,
    char *mbsmf_service_inst_id
)
{
    OpenAPI_context_create_req_data_t *context_create_req_data_local_var = ogs_malloc(sizeof(OpenAPI_context_create_req_data_t));
    ogs_assert(context_create_req_data_local_var);

    context_create_req_data_local_var->mbs_session_id = mbs_session_id;
    context_create_req_data_local_var->mbs_service_area_info_list = mbs_service_area_info_list;
    context_create_req_data_local_var->mbs_service_area = mbs_service_area;
    context_create_req_data_local_var->n2_mbs_sm_info = n2_mbs_sm_info;
    context_create_req_data_local_var->notify_uri = notify_uri;
    context_create_req_data_local_var->is_max_response_time = is_max_response_time;
    context_create_req_data_local_var->max_response_time = max_response_time;
    context_create_req_data_local_var->snssai = snssai;
    context_create_req_data_local_var->mbsmf_id = mbsmf_id;
    context_create_req_data_local_var->mbsmf_service_inst_id = mbsmf_service_inst_id;

    return context_create_req_data_local_var;
}

void OpenAPI_context_create_req_data_free(OpenAPI_context_create_req_data_t *context_create_req_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_create_req_data) {
        return;
    }
    if (context_create_req_data->mbs_session_id) {
        OpenAPI_mbs_session_id_free(context_create_req_data->mbs_session_id);
        context_create_req_data->mbs_session_id = NULL;
    }
    if (context_create_req_data->mbs_service_area_info_list) {
        OpenAPI_list_for_each(context_create_req_data->mbs_service_area_info_list, node) {
            OpenAPI_mbs_service_area_info_free(node->data);
        }
        OpenAPI_list_free(context_create_req_data->mbs_service_area_info_list);
        context_create_req_data->mbs_service_area_info_list = NULL;
    }
    if (context_create_req_data->mbs_service_area) {
        OpenAPI_mbs_service_area_free(context_create_req_data->mbs_service_area);
        context_create_req_data->mbs_service_area = NULL;
    }
    if (context_create_req_data->n2_mbs_sm_info) {
        OpenAPI_n2_mbs_sm_info_free(context_create_req_data->n2_mbs_sm_info);
        context_create_req_data->n2_mbs_sm_info = NULL;
    }
    if (context_create_req_data->notify_uri) {
        ogs_free(context_create_req_data->notify_uri);
        context_create_req_data->notify_uri = NULL;
    }
    if (context_create_req_data->snssai) {
        OpenAPI_snssai_free(context_create_req_data->snssai);
        context_create_req_data->snssai = NULL;
    }
    if (context_create_req_data->mbsmf_id) {
        ogs_free(context_create_req_data->mbsmf_id);
        context_create_req_data->mbsmf_id = NULL;
    }
    if (context_create_req_data->mbsmf_service_inst_id) {
        ogs_free(context_create_req_data->mbsmf_service_inst_id);
        context_create_req_data->mbsmf_service_inst_id = NULL;
    }
    ogs_free(context_create_req_data);
}

cJSON *OpenAPI_context_create_req_data_convertToJSON(OpenAPI_context_create_req_data_t *context_create_req_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_create_req_data == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [ContextCreateReqData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!context_create_req_data->mbs_session_id) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [mbs_session_id]");
        return NULL;
    }
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(context_create_req_data->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }

    if (context_create_req_data->mbs_service_area_info_list) {
    cJSON *mbs_service_area_info_listList = cJSON_AddArrayToObject(item, "mbsServiceAreaInfoList");
    if (mbs_service_area_info_listList == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [mbs_service_area_info_list]");
        goto end;
    }
    OpenAPI_list_for_each(context_create_req_data->mbs_service_area_info_list, node) {
        cJSON *itemLocal = OpenAPI_mbs_service_area_info_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [mbs_service_area_info_list]");
            goto end;
        }
        cJSON_AddItemToArray(mbs_service_area_info_listList, itemLocal);
    }
    }

    if (context_create_req_data->mbs_service_area) {
    cJSON *mbs_service_area_local_JSON = OpenAPI_mbs_service_area_convertToJSON(context_create_req_data->mbs_service_area);
    if (mbs_service_area_local_JSON == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsServiceArea", mbs_service_area_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    }

    if (!context_create_req_data->n2_mbs_sm_info) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        return NULL;
    }
    cJSON *n2_mbs_sm_info_local_JSON = OpenAPI_n2_mbs_sm_info_convertToJSON(context_create_req_data->n2_mbs_sm_info);
    if (n2_mbs_sm_info_local_JSON == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "n2MbsSmInfo", n2_mbs_sm_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }

    if (!context_create_req_data->notify_uri) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [notify_uri]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "notifyUri", context_create_req_data->notify_uri) == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [notify_uri]");
        goto end;
    }

    if (context_create_req_data->is_max_response_time) {
    if (cJSON_AddNumberToObject(item, "maxResponseTime", context_create_req_data->max_response_time) == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [max_response_time]");
        goto end;
    }
    }

    if (!context_create_req_data->snssai) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [snssai]");
        return NULL;
    }
    cJSON *snssai_local_JSON = OpenAPI_snssai_convertToJSON(context_create_req_data->snssai);
    if (snssai_local_JSON == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [snssai]");
        goto end;
    }
    cJSON_AddItemToObject(item, "snssai", snssai_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [snssai]");
        goto end;
    }

    if (context_create_req_data->mbsmf_id) {
    if (cJSON_AddStringToObject(item, "mbsmfId", context_create_req_data->mbsmf_id) == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [mbsmf_id]");
        goto end;
    }
    }

    if (context_create_req_data->mbsmf_service_inst_id) {
    if (cJSON_AddStringToObject(item, "mbsmfServiceInstId", context_create_req_data->mbsmf_service_inst_id) == NULL) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed [mbsmf_service_inst_id]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_create_req_data_t *OpenAPI_context_create_req_data_parseFromJSON(cJSON *context_create_req_dataJSON)
{
    OpenAPI_context_create_req_data_t *context_create_req_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *mbs_service_area_info_list = NULL;
    OpenAPI_list_t *mbs_service_area_info_listList = NULL;
    cJSON *mbs_service_area = NULL;
    OpenAPI_mbs_service_area_t *mbs_service_area_local_nonprim = NULL;
    cJSON *n2_mbs_sm_info = NULL;
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info_local_nonprim = NULL;
    cJSON *notify_uri = NULL;
    cJSON *max_response_time = NULL;
    cJSON *snssai = NULL;
    OpenAPI_snssai_t *snssai_local_nonprim = NULL;
    cJSON *mbsmf_id = NULL;
    cJSON *mbsmf_service_inst_id = NULL;
    mbs_session_id = cJSON_GetObjectItemCaseSensitive(context_create_req_dataJSON, "mbsSessionId");
    if (!mbs_session_id) {
        ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [mbs_session_id]");
        goto end;
    }
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }

    mbs_service_area_info_list = cJSON_GetObjectItemCaseSensitive(context_create_req_dataJSON, "mbsServiceAreaInfoList");
    if (mbs_service_area_info_list) {
        cJSON *mbs_service_area_info_list_local = NULL;
        if (!cJSON_IsArray(mbs_service_area_info_list)) {
            ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [mbs_service_area_info_list]");
            goto end;
        }

        mbs_service_area_info_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(mbs_service_area_info_list_local, mbs_service_area_info_list) {
            if (!cJSON_IsObject(mbs_service_area_info_list_local)) {
                ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [mbs_service_area_info_list]");
                goto end;
            }
            OpenAPI_mbs_service_area_info_t *mbs_service_area_info_listItem = OpenAPI_mbs_service_area_info_parseFromJSON(mbs_service_area_info_list_local);
            if (!mbs_service_area_info_listItem) {
                ogs_error("No mbs_service_area_info_listItem");
                goto end;
            }
            OpenAPI_list_add(mbs_service_area_info_listList, mbs_service_area_info_listItem);
        }
    }

    mbs_service_area = cJSON_GetObjectItemCaseSensitive(context_create_req_dataJSON, "mbsServiceArea");
    if (mbs_service_area) {
    mbs_service_area_local_nonprim = OpenAPI_mbs_service_area_parseFromJSON(mbs_service_area);
    if (!mbs_service_area_local_nonprim) {
        ogs_error("OpenAPI_mbs_service_area_parseFromJSON failed [mbs_service_area]");
        goto end;
    }
    }

    n2_mbs_sm_info = cJSON_GetObjectItemCaseSensitive(context_create_req_dataJSON, "n2MbsSmInfo");
    if (!n2_mbs_sm_info) {
        ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    n2_mbs_sm_info_local_nonprim = OpenAPI_n2_mbs_sm_info_parseFromJSON(n2_mbs_sm_info);
    if (!n2_mbs_sm_info_local_nonprim) {
        ogs_error("OpenAPI_n2_mbs_sm_info_parseFromJSON failed [n2_mbs_sm_info]");
        goto end;
    }

    notify_uri = cJSON_GetObjectItemCaseSensitive(context_create_req_dataJSON, "notifyUri");
    if (!notify_uri) {
        ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [notify_uri]");
        goto end;
    }
    if (!cJSON_IsString(notify_uri)) {
        ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [notify_uri]");
        goto end;
    }

    max_response_time = cJSON_GetObjectItemCaseSensitive(context_create_req_dataJSON, "maxResponseTime");
    if (max_response_time) {
    if (!cJSON_IsNumber(max_response_time)) {
        ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [max_response_time]");
        goto end;
    }
    }

    snssai = cJSON_GetObjectItemCaseSensitive(context_create_req_dataJSON, "snssai");
    if (!snssai) {
        ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [snssai]");
        goto end;
    }
    snssai_local_nonprim = OpenAPI_snssai_parseFromJSON(snssai);
    if (!snssai_local_nonprim) {
        ogs_error("OpenAPI_snssai_parseFromJSON failed [snssai]");
        goto end;
    }

    mbsmf_id = cJSON_GetObjectItemCaseSensitive(context_create_req_dataJSON, "mbsmfId");
    if (mbsmf_id) {
    if (!cJSON_IsString(mbsmf_id) && !cJSON_IsNull(mbsmf_id)) {
        ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [mbsmf_id]");
        goto end;
    }
    }

    mbsmf_service_inst_id = cJSON_GetObjectItemCaseSensitive(context_create_req_dataJSON, "mbsmfServiceInstId");
    if (mbsmf_service_inst_id) {
    if (!cJSON_IsString(mbsmf_service_inst_id) && !cJSON_IsNull(mbsmf_service_inst_id)) {
        ogs_error("OpenAPI_context_create_req_data_parseFromJSON() failed [mbsmf_service_inst_id]");
        goto end;
    }
    }

    context_create_req_data_local_var = OpenAPI_context_create_req_data_create (
        mbs_session_id_local_nonprim,
        mbs_service_area_info_list ? mbs_service_area_info_listList : NULL,
        mbs_service_area ? mbs_service_area_local_nonprim : NULL,
        n2_mbs_sm_info_local_nonprim,
        ogs_strdup(notify_uri->valuestring),
        max_response_time ? true : false,
        max_response_time ? max_response_time->valuedouble : 0,
        snssai_local_nonprim,
        mbsmf_id && !cJSON_IsNull(mbsmf_id) ? ogs_strdup(mbsmf_id->valuestring) : NULL,
        mbsmf_service_inst_id && !cJSON_IsNull(mbsmf_service_inst_id) ? ogs_strdup(mbsmf_service_inst_id->valuestring) : NULL
    );

    return context_create_req_data_local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (mbs_service_area_info_listList) {
        OpenAPI_list_for_each(mbs_service_area_info_listList, node) {
            OpenAPI_mbs_service_area_info_free(node->data);
        }
        OpenAPI_list_free(mbs_service_area_info_listList);
        mbs_service_area_info_listList = NULL;
    }
    if (mbs_service_area_local_nonprim) {
        OpenAPI_mbs_service_area_free(mbs_service_area_local_nonprim);
        mbs_service_area_local_nonprim = NULL;
    }
    if (n2_mbs_sm_info_local_nonprim) {
        OpenAPI_n2_mbs_sm_info_free(n2_mbs_sm_info_local_nonprim);
        n2_mbs_sm_info_local_nonprim = NULL;
    }
    if (snssai_local_nonprim) {
        OpenAPI_snssai_free(snssai_local_nonprim);
        snssai_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_context_create_req_data_t *OpenAPI_context_create_req_data_copy(OpenAPI_context_create_req_data_t *dst, OpenAPI_context_create_req_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_create_req_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_create_req_data_convertToJSON() failed");
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

    OpenAPI_context_create_req_data_free(dst);
    dst = OpenAPI_context_create_req_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

