
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_update_req_data.h"

char *OpenAPI_no_ngap_signalling_indcontext_update_req_data_ToString(OpenAPI_context_update_req_data_no_ngap_signalling_ind_e no_ngap_signalling_ind)
{
    const char *no_ngap_signalling_indArray[] =  { "NULL", "true" };
    size_t sizeofArray = sizeof(no_ngap_signalling_indArray) / sizeof(no_ngap_signalling_indArray[0]);
    if (no_ngap_signalling_ind < sizeofArray)
        return (char *)no_ngap_signalling_indArray[no_ngap_signalling_ind];
    else
        return (char *)"Unknown";
}

OpenAPI_context_update_req_data_no_ngap_signalling_ind_e OpenAPI_no_ngap_signalling_indcontext_update_req_data_FromString(char* no_ngap_signalling_ind)
{
    int stringToReturn = 0;
    const char *no_ngap_signalling_indArray[] =  { "NULL", "true" };
    size_t sizeofArray = sizeof(no_ngap_signalling_indArray) / sizeof(no_ngap_signalling_indArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(no_ngap_signalling_ind, no_ngap_signalling_indArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}
OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_create(
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_list_t *mbs_service_area_info_list,
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info,
    OpenAPI_list_t *ran_id_list,
    OpenAPI_context_update_req_data_no_ngap_signalling_ind_e no_ngap_signalling_ind,
    char *notify_uri,
    bool is_max_response_time,
    int max_response_time,
    bool is_n2_mbs_info_change_ind,
    int n2_mbs_info_change_ind
)
{
    OpenAPI_context_update_req_data_t *context_update_req_data_local_var = ogs_malloc(sizeof(OpenAPI_context_update_req_data_t));
    ogs_assert(context_update_req_data_local_var);

    context_update_req_data_local_var->mbs_service_area = mbs_service_area;
    context_update_req_data_local_var->mbs_service_area_info_list = mbs_service_area_info_list;
    context_update_req_data_local_var->n2_mbs_sm_info = n2_mbs_sm_info;
    context_update_req_data_local_var->ran_id_list = ran_id_list;
    context_update_req_data_local_var->no_ngap_signalling_ind = no_ngap_signalling_ind;
    context_update_req_data_local_var->notify_uri = notify_uri;
    context_update_req_data_local_var->is_max_response_time = is_max_response_time;
    context_update_req_data_local_var->max_response_time = max_response_time;
    context_update_req_data_local_var->is_n2_mbs_info_change_ind = is_n2_mbs_info_change_ind;
    context_update_req_data_local_var->n2_mbs_info_change_ind = n2_mbs_info_change_ind;

    return context_update_req_data_local_var;
}

void OpenAPI_context_update_req_data_free(OpenAPI_context_update_req_data_t *context_update_req_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_update_req_data) {
        return;
    }
    if (context_update_req_data->mbs_service_area) {
        OpenAPI_mbs_service_area_free(context_update_req_data->mbs_service_area);
        context_update_req_data->mbs_service_area = NULL;
    }
    if (context_update_req_data->mbs_service_area_info_list) {
        OpenAPI_list_for_each(context_update_req_data->mbs_service_area_info_list, node) {
            OpenAPI_mbs_service_area_info_free(node->data);
        }
        OpenAPI_list_free(context_update_req_data->mbs_service_area_info_list);
        context_update_req_data->mbs_service_area_info_list = NULL;
    }
    if (context_update_req_data->n2_mbs_sm_info) {
        OpenAPI_n2_mbs_sm_info_free(context_update_req_data->n2_mbs_sm_info);
        context_update_req_data->n2_mbs_sm_info = NULL;
    }
    if (context_update_req_data->ran_id_list) {
        OpenAPI_list_for_each(context_update_req_data->ran_id_list, node) {
            OpenAPI_global_ran_node_id_free(node->data);
        }
        OpenAPI_list_free(context_update_req_data->ran_id_list);
        context_update_req_data->ran_id_list = NULL;
    }
    if (context_update_req_data->notify_uri) {
        ogs_free(context_update_req_data->notify_uri);
        context_update_req_data->notify_uri = NULL;
    }
    ogs_free(context_update_req_data);
}

cJSON *OpenAPI_context_update_req_data_convertToJSON(OpenAPI_context_update_req_data_t *context_update_req_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_update_req_data == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [ContextUpdateReqData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (context_update_req_data->mbs_service_area) {
    cJSON *mbs_service_area_local_JSON = OpenAPI_mbs_service_area_convertToJSON(context_update_req_data->mbs_service_area);
    if (mbs_service_area_local_JSON == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsServiceArea", mbs_service_area_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    }

    if (context_update_req_data->mbs_service_area_info_list) {
    cJSON *mbs_service_area_info_listList = cJSON_AddArrayToObject(item, "mbsServiceAreaInfoList");
    if (mbs_service_area_info_listList == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [mbs_service_area_info_list]");
        goto end;
    }
    OpenAPI_list_for_each(context_update_req_data->mbs_service_area_info_list, node) {
        cJSON *itemLocal = OpenAPI_mbs_service_area_info_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [mbs_service_area_info_list]");
            goto end;
        }
        cJSON_AddItemToArray(mbs_service_area_info_listList, itemLocal);
    }
    }

    if (context_update_req_data->n2_mbs_sm_info) {
    cJSON *n2_mbs_sm_info_local_JSON = OpenAPI_n2_mbs_sm_info_convertToJSON(context_update_req_data->n2_mbs_sm_info);
    if (n2_mbs_sm_info_local_JSON == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "n2MbsSmInfo", n2_mbs_sm_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    }

    if (context_update_req_data->ran_id_list) {
    cJSON *ran_id_listList = cJSON_AddArrayToObject(item, "ranIdList");
    if (ran_id_listList == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [ran_id_list]");
        goto end;
    }
    OpenAPI_list_for_each(context_update_req_data->ran_id_list, node) {
        cJSON *itemLocal = OpenAPI_global_ran_node_id_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [ran_id_list]");
            goto end;
        }
        cJSON_AddItemToArray(ran_id_listList, itemLocal);
    }
    }

    if (context_update_req_data->no_ngap_signalling_ind != OpenAPI_context_update_req_data_NONGAPSIGNALLINGIND_NULL) {
    if (cJSON_AddStringToObject(item, "noNgapSignallingInd", OpenAPI_no_ngap_signalling_indcontext_update_req_data_ToString(context_update_req_data->no_ngap_signalling_ind)) == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [no_ngap_signalling_ind]");
        goto end;
    }
    }

    if (context_update_req_data->notify_uri) {
    if (cJSON_AddStringToObject(item, "notifyUri", context_update_req_data->notify_uri) == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [notify_uri]");
        goto end;
    }
    }

    if (context_update_req_data->is_max_response_time) {
    if (cJSON_AddNumberToObject(item, "maxResponseTime", context_update_req_data->max_response_time) == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [max_response_time]");
        goto end;
    }
    }

    if (context_update_req_data->is_n2_mbs_info_change_ind) {
    if (cJSON_AddBoolToObject(item, "n2MbsInfoChangeInd", context_update_req_data->n2_mbs_info_change_ind) == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [n2_mbs_info_change_ind]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_parseFromJSON(cJSON *context_update_req_dataJSON)
{
    OpenAPI_context_update_req_data_t *context_update_req_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_service_area = NULL;
    OpenAPI_mbs_service_area_t *mbs_service_area_local_nonprim = NULL;
    cJSON *mbs_service_area_info_list = NULL;
    OpenAPI_list_t *mbs_service_area_info_listList = NULL;
    cJSON *n2_mbs_sm_info = NULL;
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info_local_nonprim = NULL;
    cJSON *ran_id_list = NULL;
    OpenAPI_list_t *ran_id_listList = NULL;
    cJSON *no_ngap_signalling_ind = NULL;
    OpenAPI_context_update_req_data_no_ngap_signalling_ind_e no_ngap_signalling_indVariable = 0;
    cJSON *notify_uri = NULL;
    cJSON *max_response_time = NULL;
    cJSON *n2_mbs_info_change_ind = NULL;
    mbs_service_area = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "mbsServiceArea");
    if (mbs_service_area) {
    mbs_service_area_local_nonprim = OpenAPI_mbs_service_area_parseFromJSON(mbs_service_area);
    if (!mbs_service_area_local_nonprim) {
        ogs_error("OpenAPI_mbs_service_area_parseFromJSON failed [mbs_service_area]");
        goto end;
    }
    }

    mbs_service_area_info_list = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "mbsServiceAreaInfoList");
    if (mbs_service_area_info_list) {
        cJSON *mbs_service_area_info_list_local = NULL;
        if (!cJSON_IsArray(mbs_service_area_info_list)) {
            ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [mbs_service_area_info_list]");
            goto end;
        }

        mbs_service_area_info_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(mbs_service_area_info_list_local, mbs_service_area_info_list) {
            if (!cJSON_IsObject(mbs_service_area_info_list_local)) {
                ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [mbs_service_area_info_list]");
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

    n2_mbs_sm_info = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "n2MbsSmInfo");
    if (n2_mbs_sm_info) {
    n2_mbs_sm_info_local_nonprim = OpenAPI_n2_mbs_sm_info_parseFromJSON(n2_mbs_sm_info);
    if (!n2_mbs_sm_info_local_nonprim) {
        ogs_error("OpenAPI_n2_mbs_sm_info_parseFromJSON failed [n2_mbs_sm_info]");
        goto end;
    }
    }

    ran_id_list = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "ranIdList");
    if (ran_id_list) {
        cJSON *ran_id_list_local = NULL;
        if (!cJSON_IsArray(ran_id_list)) {
            ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [ran_id_list]");
            goto end;
        }

        ran_id_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(ran_id_list_local, ran_id_list) {
            if (!cJSON_IsObject(ran_id_list_local)) {
                ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [ran_id_list]");
                goto end;
            }
            OpenAPI_global_ran_node_id_t *ran_id_listItem = OpenAPI_global_ran_node_id_parseFromJSON(ran_id_list_local);
            if (!ran_id_listItem) {
                ogs_error("No ran_id_listItem");
                goto end;
            }
            OpenAPI_list_add(ran_id_listList, ran_id_listItem);
        }
    }

    no_ngap_signalling_ind = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "noNgapSignallingInd");
    if (no_ngap_signalling_ind) {
    if (!cJSON_IsString(no_ngap_signalling_ind)) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [no_ngap_signalling_ind]");
        goto end;
    }
    no_ngap_signalling_indVariable = OpenAPI_no_ngap_signalling_indcontext_update_req_data_FromString(no_ngap_signalling_ind->valuestring);
    }

    notify_uri = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "notifyUri");
    if (notify_uri) {
    if (!cJSON_IsString(notify_uri) && !cJSON_IsNull(notify_uri)) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [notify_uri]");
        goto end;
    }
    }

    max_response_time = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "maxResponseTime");
    if (max_response_time) {
    if (!cJSON_IsNumber(max_response_time)) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [max_response_time]");
        goto end;
    }
    }

    n2_mbs_info_change_ind = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "n2MbsInfoChangeInd");
    if (n2_mbs_info_change_ind) {
    if (!cJSON_IsBool(n2_mbs_info_change_ind)) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [n2_mbs_info_change_ind]");
        goto end;
    }
    }

    context_update_req_data_local_var = OpenAPI_context_update_req_data_create (
        mbs_service_area ? mbs_service_area_local_nonprim : NULL,
        mbs_service_area_info_list ? mbs_service_area_info_listList : NULL,
        n2_mbs_sm_info ? n2_mbs_sm_info_local_nonprim : NULL,
        ran_id_list ? ran_id_listList : NULL,
        no_ngap_signalling_ind ? no_ngap_signalling_indVariable : 0,
        notify_uri && !cJSON_IsNull(notify_uri) ? ogs_strdup(notify_uri->valuestring) : NULL,
        max_response_time ? true : false,
        max_response_time ? max_response_time->valuedouble : 0,
        n2_mbs_info_change_ind ? true : false,
        n2_mbs_info_change_ind ? n2_mbs_info_change_ind->valueint : 0
    );

    return context_update_req_data_local_var;
end:
    if (mbs_service_area_local_nonprim) {
        OpenAPI_mbs_service_area_free(mbs_service_area_local_nonprim);
        mbs_service_area_local_nonprim = NULL;
    }
    if (mbs_service_area_info_listList) {
        OpenAPI_list_for_each(mbs_service_area_info_listList, node) {
            OpenAPI_mbs_service_area_info_free(node->data);
        }
        OpenAPI_list_free(mbs_service_area_info_listList);
        mbs_service_area_info_listList = NULL;
    }
    if (n2_mbs_sm_info_local_nonprim) {
        OpenAPI_n2_mbs_sm_info_free(n2_mbs_sm_info_local_nonprim);
        n2_mbs_sm_info_local_nonprim = NULL;
    }
    if (ran_id_listList) {
        OpenAPI_list_for_each(ran_id_listList, node) {
            OpenAPI_global_ran_node_id_free(node->data);
        }
        OpenAPI_list_free(ran_id_listList);
        ran_id_listList = NULL;
    }
    return NULL;
}

OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_copy(OpenAPI_context_update_req_data_t *dst, OpenAPI_context_update_req_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_update_req_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed");
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

    OpenAPI_context_update_req_data_free(dst);
    dst = OpenAPI_context_update_req_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

