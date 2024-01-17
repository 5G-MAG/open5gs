
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_service_info.h"

OpenAPI_mbs_service_info_t *OpenAPI_mbs_service_info_create(
    OpenAPI_list_t* mbs_media_comps,
    OpenAPI_reserv_priority_e mbs_sdf_res_prio,
    char *af_app_id,
    char *mbs_session_ambr
)
{
    OpenAPI_mbs_service_info_t *mbs_service_info_local_var = ogs_malloc(sizeof(OpenAPI_mbs_service_info_t));
    ogs_assert(mbs_service_info_local_var);

    mbs_service_info_local_var->mbs_media_comps = mbs_media_comps;
    mbs_service_info_local_var->mbs_sdf_res_prio = mbs_sdf_res_prio;
    mbs_service_info_local_var->af_app_id = af_app_id;
    mbs_service_info_local_var->mbs_session_ambr = mbs_session_ambr;

    return mbs_service_info_local_var;
}

void OpenAPI_mbs_service_info_free(OpenAPI_mbs_service_info_t *mbs_service_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_service_info) {
        return;
    }
    if (mbs_service_info->mbs_media_comps) {
        OpenAPI_list_for_each(mbs_service_info->mbs_media_comps, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_media_comp_rm_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_service_info->mbs_media_comps);
        mbs_service_info->mbs_media_comps = NULL;
    }
    if (mbs_service_info->af_app_id) {
        ogs_free(mbs_service_info->af_app_id);
        mbs_service_info->af_app_id = NULL;
    }
    if (mbs_service_info->mbs_session_ambr) {
        ogs_free(mbs_service_info->mbs_session_ambr);
        mbs_service_info->mbs_session_ambr = NULL;
    }
    ogs_free(mbs_service_info);
}

cJSON *OpenAPI_mbs_service_info_convertToJSON(OpenAPI_mbs_service_info_t *mbs_service_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_service_info == NULL) {
        ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed [MbsServiceInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_service_info->mbs_media_comps) {
        ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed [mbs_media_comps]");
        return NULL;
    }
    cJSON *mbs_media_comps = cJSON_AddObjectToObject(item, "mbsMediaComps");
    if (mbs_media_comps == NULL) {
        ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed [mbs_media_comps]");
        goto end;
    }
    cJSON *localMapObject = mbs_media_comps;
    if (mbs_service_info->mbs_media_comps) {
        OpenAPI_list_for_each(mbs_service_info->mbs_media_comps, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            if (localKeyValue == NULL) {
                ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed [mbs_media_comps]");
                goto end;
            }
            if (localKeyValue->key == NULL) {
                ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed [mbs_media_comps]");
                goto end;
            }
            cJSON *itemLocal = localKeyValue->value ?
                OpenAPI_mbs_media_comp_rm_convertToJSON(localKeyValue->value) :
                cJSON_CreateNull();
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed [inner]");
                goto end;
            }
            cJSON_AddItemToObject(localMapObject, localKeyValue->key, itemLocal);
        }
    }

    if (mbs_service_info->mbs_sdf_res_prio != OpenAPI_reserv_priority_NULL) {
    if (cJSON_AddStringToObject(item, "mbsSdfResPrio", OpenAPI_reserv_priority_ToString(mbs_service_info->mbs_sdf_res_prio)) == NULL) {
        ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed [mbs_sdf_res_prio]");
        goto end;
    }
    }

    if (mbs_service_info->af_app_id) {
    if (cJSON_AddStringToObject(item, "afAppId", mbs_service_info->af_app_id) == NULL) {
        ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed [af_app_id]");
        goto end;
    }
    }

    if (mbs_service_info->mbs_session_ambr) {
    if (cJSON_AddStringToObject(item, "mbsSessionAmbr", mbs_service_info->mbs_session_ambr) == NULL) {
        ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed [mbs_session_ambr]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_service_info_t *OpenAPI_mbs_service_info_parseFromJSON(cJSON *mbs_service_infoJSON)
{
    OpenAPI_mbs_service_info_t *mbs_service_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_media_comps = NULL;
    OpenAPI_list_t *mbs_media_compsList = NULL;
    cJSON *mbs_sdf_res_prio = NULL;
    OpenAPI_reserv_priority_e mbs_sdf_res_prioVariable = 0;
    cJSON *af_app_id = NULL;
    cJSON *mbs_session_ambr = NULL;
    mbs_media_comps = cJSON_GetObjectItemCaseSensitive(mbs_service_infoJSON, "mbsMediaComps");
    if (!mbs_media_comps) {
        ogs_error("OpenAPI_mbs_service_info_parseFromJSON() failed [mbs_media_comps]");
        goto end;
    }
        cJSON *mbs_media_comps_local_map = NULL;
        if (!cJSON_IsObject(mbs_media_comps) && !cJSON_IsNull(mbs_media_comps)) {
            ogs_error("OpenAPI_mbs_service_info_parseFromJSON() failed [mbs_media_comps]");
            goto end;
        }
        if (cJSON_IsObject(mbs_media_comps)) {
            mbs_media_compsList = OpenAPI_list_create();
            OpenAPI_map_t *localMapKeyPair = NULL;
            cJSON_ArrayForEach(mbs_media_comps_local_map, mbs_media_comps) {
                cJSON *localMapObject = mbs_media_comps_local_map;
                if (cJSON_IsObject(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(
                        ogs_strdup(localMapObject->string), OpenAPI_mbs_media_comp_rm_parseFromJSON(localMapObject));
                } else if (cJSON_IsNull(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(ogs_strdup(localMapObject->string), NULL);
                } else {
                    ogs_error("OpenAPI_mbs_service_info_parseFromJSON() failed [inner]");
                    goto end;
                }
                OpenAPI_list_add(mbs_media_compsList, localMapKeyPair);
            }
        }

    mbs_sdf_res_prio = cJSON_GetObjectItemCaseSensitive(mbs_service_infoJSON, "mbsSdfResPrio");
    if (mbs_sdf_res_prio) {
    if (!cJSON_IsString(mbs_sdf_res_prio)) {
        ogs_error("OpenAPI_mbs_service_info_parseFromJSON() failed [mbs_sdf_res_prio]");
        goto end;
    }
    mbs_sdf_res_prioVariable = OpenAPI_reserv_priority_FromString(mbs_sdf_res_prio->valuestring);
    }

    af_app_id = cJSON_GetObjectItemCaseSensitive(mbs_service_infoJSON, "afAppId");
    if (af_app_id) {
    if (!cJSON_IsString(af_app_id) && !cJSON_IsNull(af_app_id)) {
        ogs_error("OpenAPI_mbs_service_info_parseFromJSON() failed [af_app_id]");
        goto end;
    }
    }

    mbs_session_ambr = cJSON_GetObjectItemCaseSensitive(mbs_service_infoJSON, "mbsSessionAmbr");
    if (mbs_session_ambr) {
    if (!cJSON_IsString(mbs_session_ambr) && !cJSON_IsNull(mbs_session_ambr)) {
        ogs_error("OpenAPI_mbs_service_info_parseFromJSON() failed [mbs_session_ambr]");
        goto end;
    }
    }

    mbs_service_info_local_var = OpenAPI_mbs_service_info_create (
        mbs_media_compsList,
        mbs_sdf_res_prio ? mbs_sdf_res_prioVariable : 0,
        af_app_id && !cJSON_IsNull(af_app_id) ? ogs_strdup(af_app_id->valuestring) : NULL,
        mbs_session_ambr && !cJSON_IsNull(mbs_session_ambr) ? ogs_strdup(mbs_session_ambr->valuestring) : NULL
    );

    return mbs_service_info_local_var;
end:
    if (mbs_media_compsList) {
        OpenAPI_list_for_each(mbs_media_compsList, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*) node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_media_comp_rm_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_media_compsList);
        mbs_media_compsList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_service_info_t *OpenAPI_mbs_service_info_copy(OpenAPI_mbs_service_info_t *dst, OpenAPI_mbs_service_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_service_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_service_info_convertToJSON() failed");
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

    OpenAPI_mbs_service_info_free(dst);
    dst = OpenAPI_mbs_service_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

