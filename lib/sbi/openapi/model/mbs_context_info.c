
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_context_info.h"

OpenAPI_mbs_context_info_t *OpenAPI_mbs_context_info_create(
    char *start_time,
    bool is_any_ue_ind,
    int any_ue_ind,
    OpenAPI_ssm_t *ll_ssm,
    bool is_c_teid,
    int c_teid,
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_list_t* mbs_service_area_info_list
)
{
    OpenAPI_mbs_context_info_t *mbs_context_info_local_var = ogs_malloc(sizeof(OpenAPI_mbs_context_info_t));
    ogs_assert(mbs_context_info_local_var);

    mbs_context_info_local_var->start_time = start_time;
    mbs_context_info_local_var->is_any_ue_ind = is_any_ue_ind;
    mbs_context_info_local_var->any_ue_ind = any_ue_ind;
    mbs_context_info_local_var->ll_ssm = ll_ssm;
    mbs_context_info_local_var->is_c_teid = is_c_teid;
    mbs_context_info_local_var->c_teid = c_teid;
    mbs_context_info_local_var->mbs_service_area = mbs_service_area;
    mbs_context_info_local_var->mbs_service_area_info_list = mbs_service_area_info_list;

    return mbs_context_info_local_var;
}

void OpenAPI_mbs_context_info_free(OpenAPI_mbs_context_info_t *mbs_context_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_context_info) {
        return;
    }
    if (mbs_context_info->start_time) {
        ogs_free(mbs_context_info->start_time);
        mbs_context_info->start_time = NULL;
    }
    if (mbs_context_info->ll_ssm) {
        OpenAPI_ssm_free(mbs_context_info->ll_ssm);
        mbs_context_info->ll_ssm = NULL;
    }
    if (mbs_context_info->mbs_service_area) {
        OpenAPI_mbs_service_area_free(mbs_context_info->mbs_service_area);
        mbs_context_info->mbs_service_area = NULL;
    }
    if (mbs_context_info->mbs_service_area_info_list) {
        OpenAPI_list_for_each(mbs_context_info->mbs_service_area_info_list, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_service_area_info_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_context_info->mbs_service_area_info_list);
        mbs_context_info->mbs_service_area_info_list = NULL;
    }
    ogs_free(mbs_context_info);
}

cJSON *OpenAPI_mbs_context_info_convertToJSON(OpenAPI_mbs_context_info_t *mbs_context_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_context_info == NULL) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [MbsContextInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_context_info->start_time) {
    if (cJSON_AddStringToObject(item, "startTime", mbs_context_info->start_time) == NULL) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [start_time]");
        goto end;
    }
    }

    if (mbs_context_info->is_any_ue_ind) {
    if (cJSON_AddBoolToObject(item, "anyUeInd", mbs_context_info->any_ue_ind) == NULL) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [any_ue_ind]");
        goto end;
    }
    }

    if (mbs_context_info->ll_ssm) {
    cJSON *ll_ssm_local_JSON = OpenAPI_ssm_convertToJSON(mbs_context_info->ll_ssm);
    if (ll_ssm_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [ll_ssm]");
        goto end;
    }
    cJSON_AddItemToObject(item, "llSsm", ll_ssm_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [ll_ssm]");
        goto end;
    }
    }

    if (mbs_context_info->is_c_teid) {
    if (cJSON_AddNumberToObject(item, "cTeid", mbs_context_info->c_teid) == NULL) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [c_teid]");
        goto end;
    }
    }

    if (mbs_context_info->mbs_service_area) {
    cJSON *mbs_service_area_local_JSON = OpenAPI_mbs_service_area_convertToJSON(mbs_context_info->mbs_service_area);
    if (mbs_service_area_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsServiceArea", mbs_service_area_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    }

    if (mbs_context_info->mbs_service_area_info_list) {
    cJSON *mbs_service_area_info_list = cJSON_AddObjectToObject(item, "mbsServiceAreaInfoList");
    if (mbs_service_area_info_list == NULL) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [mbs_service_area_info_list]");
        goto end;
    }
    cJSON *localMapObject = mbs_service_area_info_list;
    if (mbs_context_info->mbs_service_area_info_list) {
        OpenAPI_list_for_each(mbs_context_info->mbs_service_area_info_list, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            if (localKeyValue == NULL) {
                ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [mbs_service_area_info_list]");
                goto end;
            }
            if (localKeyValue->key == NULL) {
                ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [mbs_service_area_info_list]");
                goto end;
            }
            cJSON *itemLocal = localKeyValue->value ?
                OpenAPI_mbs_service_area_info_convertToJSON(localKeyValue->value) :
                cJSON_CreateNull();
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed [inner]");
                goto end;
            }
            cJSON_AddItemToObject(localMapObject, localKeyValue->key, itemLocal);
        }
    }
    }

end:
    return item;
}

OpenAPI_mbs_context_info_t *OpenAPI_mbs_context_info_parseFromJSON(cJSON *mbs_context_infoJSON)
{
    OpenAPI_mbs_context_info_t *mbs_context_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *start_time = NULL;
    cJSON *any_ue_ind = NULL;
    cJSON *ll_ssm = NULL;
    OpenAPI_ssm_t *ll_ssm_local_nonprim = NULL;
    cJSON *c_teid = NULL;
    cJSON *mbs_service_area = NULL;
    OpenAPI_mbs_service_area_t *mbs_service_area_local_nonprim = NULL;
    cJSON *mbs_service_area_info_list = NULL;
    OpenAPI_list_t *mbs_service_area_info_listList = NULL;
    start_time = cJSON_GetObjectItemCaseSensitive(mbs_context_infoJSON, "startTime");
    if (start_time) {
    if (!cJSON_IsString(start_time) && !cJSON_IsNull(start_time)) {
        ogs_error("OpenAPI_mbs_context_info_parseFromJSON() failed [start_time]");
        goto end;
    }
    }

    any_ue_ind = cJSON_GetObjectItemCaseSensitive(mbs_context_infoJSON, "anyUeInd");
    if (any_ue_ind) {
    if (!cJSON_IsBool(any_ue_ind)) {
        ogs_error("OpenAPI_mbs_context_info_parseFromJSON() failed [any_ue_ind]");
        goto end;
    }
    }

    ll_ssm = cJSON_GetObjectItemCaseSensitive(mbs_context_infoJSON, "llSsm");
    if (ll_ssm) {
    ll_ssm_local_nonprim = OpenAPI_ssm_parseFromJSON(ll_ssm);
    if (!ll_ssm_local_nonprim) {
        ogs_error("OpenAPI_ssm_parseFromJSON failed [ll_ssm]");
        goto end;
    }
    }

    c_teid = cJSON_GetObjectItemCaseSensitive(mbs_context_infoJSON, "cTeid");
    if (c_teid) {
    if (!cJSON_IsNumber(c_teid)) {
        ogs_error("OpenAPI_mbs_context_info_parseFromJSON() failed [c_teid]");
        goto end;
    }
    }

    mbs_service_area = cJSON_GetObjectItemCaseSensitive(mbs_context_infoJSON, "mbsServiceArea");
    if (mbs_service_area) {
    mbs_service_area_local_nonprim = OpenAPI_mbs_service_area_parseFromJSON(mbs_service_area);
    if (!mbs_service_area_local_nonprim) {
        ogs_error("OpenAPI_mbs_service_area_parseFromJSON failed [mbs_service_area]");
        goto end;
    }
    }

    mbs_service_area_info_list = cJSON_GetObjectItemCaseSensitive(mbs_context_infoJSON, "mbsServiceAreaInfoList");
    if (mbs_service_area_info_list) {
        cJSON *mbs_service_area_info_list_local_map = NULL;
        if (!cJSON_IsObject(mbs_service_area_info_list) && !cJSON_IsNull(mbs_service_area_info_list)) {
            ogs_error("OpenAPI_mbs_context_info_parseFromJSON() failed [mbs_service_area_info_list]");
            goto end;
        }
        if (cJSON_IsObject(mbs_service_area_info_list)) {
            mbs_service_area_info_listList = OpenAPI_list_create();
            OpenAPI_map_t *localMapKeyPair = NULL;
            cJSON_ArrayForEach(mbs_service_area_info_list_local_map, mbs_service_area_info_list) {
                cJSON *localMapObject = mbs_service_area_info_list_local_map;
                if (cJSON_IsObject(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(
                        ogs_strdup(localMapObject->string), OpenAPI_mbs_service_area_info_parseFromJSON(localMapObject));
                } else if (cJSON_IsNull(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(ogs_strdup(localMapObject->string), NULL);
                } else {
                    ogs_error("OpenAPI_mbs_context_info_parseFromJSON() failed [inner]");
                    goto end;
                }
                OpenAPI_list_add(mbs_service_area_info_listList, localMapKeyPair);
            }
        }
    }

    mbs_context_info_local_var = OpenAPI_mbs_context_info_create (
        start_time && !cJSON_IsNull(start_time) ? ogs_strdup(start_time->valuestring) : NULL,
        any_ue_ind ? true : false,
        any_ue_ind ? any_ue_ind->valueint : 0,
        ll_ssm ? ll_ssm_local_nonprim : NULL,
        c_teid ? true : false,
        c_teid ? c_teid->valuedouble : 0,
        mbs_service_area ? mbs_service_area_local_nonprim : NULL,
        mbs_service_area_info_list ? mbs_service_area_info_listList : NULL
    );

    return mbs_context_info_local_var;
end:
    if (ll_ssm_local_nonprim) {
        OpenAPI_ssm_free(ll_ssm_local_nonprim);
        ll_ssm_local_nonprim = NULL;
    }
    if (mbs_service_area_local_nonprim) {
        OpenAPI_mbs_service_area_free(mbs_service_area_local_nonprim);
        mbs_service_area_local_nonprim = NULL;
    }
    if (mbs_service_area_info_listList) {
        OpenAPI_list_for_each(mbs_service_area_info_listList, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*) node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_service_area_info_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_service_area_info_listList);
        mbs_service_area_info_listList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_context_info_t *OpenAPI_mbs_context_info_copy(OpenAPI_mbs_context_info_t *dst, OpenAPI_mbs_context_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_context_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_context_info_convertToJSON() failed");
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

    OpenAPI_mbs_context_info_free(dst);
    dst = OpenAPI_mbs_context_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

