
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_media_comp.h"

OpenAPI_mbs_media_comp_t *OpenAPI_mbs_media_comp_create(
    int mbs_med_comp_num,
    OpenAPI_list_t *mbs_flow_descs,
    OpenAPI_reserv_priority_e mbs_sdf_res_prio,
    OpenAPI_mbs_media_info_t *mbs_media_info,
    char *qos_ref,
    OpenAPI_mbs_qo_s_req_t *mbs_qo_s_req
)
{
    OpenAPI_mbs_media_comp_t *mbs_media_comp_local_var = ogs_malloc(sizeof(OpenAPI_mbs_media_comp_t));
    ogs_assert(mbs_media_comp_local_var);

    mbs_media_comp_local_var->mbs_med_comp_num = mbs_med_comp_num;
    mbs_media_comp_local_var->mbs_flow_descs = mbs_flow_descs;
    mbs_media_comp_local_var->mbs_sdf_res_prio = mbs_sdf_res_prio;
    mbs_media_comp_local_var->mbs_media_info = mbs_media_info;
    mbs_media_comp_local_var->qos_ref = qos_ref;
    mbs_media_comp_local_var->mbs_qo_s_req = mbs_qo_s_req;

    return mbs_media_comp_local_var;
}

void OpenAPI_mbs_media_comp_free(OpenAPI_mbs_media_comp_t *mbs_media_comp)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_media_comp) {
        return;
    }
    if (mbs_media_comp->mbs_flow_descs) {
        OpenAPI_list_for_each(mbs_media_comp->mbs_flow_descs, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_media_comp->mbs_flow_descs);
        mbs_media_comp->mbs_flow_descs = NULL;
    }
    if (mbs_media_comp->mbs_media_info) {
        OpenAPI_mbs_media_info_free(mbs_media_comp->mbs_media_info);
        mbs_media_comp->mbs_media_info = NULL;
    }
    if (mbs_media_comp->qos_ref) {
        ogs_free(mbs_media_comp->qos_ref);
        mbs_media_comp->qos_ref = NULL;
    }
    if (mbs_media_comp->mbs_qo_s_req) {
        OpenAPI_mbs_qo_s_req_free(mbs_media_comp->mbs_qo_s_req);
        mbs_media_comp->mbs_qo_s_req = NULL;
    }
    ogs_free(mbs_media_comp);
}

cJSON *OpenAPI_mbs_media_comp_convertToJSON(OpenAPI_mbs_media_comp_t *mbs_media_comp)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_media_comp == NULL) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [MbsMediaComp]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (cJSON_AddNumberToObject(item, "mbsMedCompNum", mbs_media_comp->mbs_med_comp_num) == NULL) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [mbs_med_comp_num]");
        goto end;
    }

    if (mbs_media_comp->mbs_flow_descs) {
    cJSON *mbs_flow_descsList = cJSON_AddArrayToObject(item, "mbsFlowDescs");
    if (mbs_flow_descsList == NULL) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [mbs_flow_descs]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_media_comp->mbs_flow_descs, node) {
        if (cJSON_AddStringToObject(mbs_flow_descsList, "", (char*)node->data) == NULL) {
            ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [mbs_flow_descs]");
            goto end;
        }
    }
    }

    if (mbs_media_comp->mbs_sdf_res_prio != OpenAPI_reserv_priority_NULL) {
    if (cJSON_AddStringToObject(item, "mbsSdfResPrio", OpenAPI_reserv_priority_ToString(mbs_media_comp->mbs_sdf_res_prio)) == NULL) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [mbs_sdf_res_prio]");
        goto end;
    }
    }

    if (mbs_media_comp->mbs_media_info) {
    cJSON *mbs_media_info_local_JSON = OpenAPI_mbs_media_info_convertToJSON(mbs_media_comp->mbs_media_info);
    if (mbs_media_info_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [mbs_media_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsMediaInfo", mbs_media_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [mbs_media_info]");
        goto end;
    }
    }

    if (mbs_media_comp->qos_ref) {
    if (cJSON_AddStringToObject(item, "qosRef", mbs_media_comp->qos_ref) == NULL) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [qos_ref]");
        goto end;
    }
    }

    if (mbs_media_comp->mbs_qo_s_req) {
    cJSON *mbs_qo_s_req_local_JSON = OpenAPI_mbs_qo_s_req_convertToJSON(mbs_media_comp->mbs_qo_s_req);
    if (mbs_qo_s_req_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [mbs_qo_s_req]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsQoSReq", mbs_qo_s_req_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed [mbs_qo_s_req]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_media_comp_t *OpenAPI_mbs_media_comp_parseFromJSON(cJSON *mbs_media_compJSON)
{
    OpenAPI_mbs_media_comp_t *mbs_media_comp_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_med_comp_num = NULL;
    cJSON *mbs_flow_descs = NULL;
    OpenAPI_list_t *mbs_flow_descsList = NULL;
    cJSON *mbs_sdf_res_prio = NULL;
    OpenAPI_reserv_priority_e mbs_sdf_res_prioVariable = 0;
    cJSON *mbs_media_info = NULL;
    OpenAPI_mbs_media_info_t *mbs_media_info_local_nonprim = NULL;
    cJSON *qos_ref = NULL;
    cJSON *mbs_qo_s_req = NULL;
    OpenAPI_mbs_qo_s_req_t *mbs_qo_s_req_local_nonprim = NULL;
    mbs_med_comp_num = cJSON_GetObjectItemCaseSensitive(mbs_media_compJSON, "mbsMedCompNum");
    if (!mbs_med_comp_num) {
        ogs_error("OpenAPI_mbs_media_comp_parseFromJSON() failed [mbs_med_comp_num]");
        goto end;
    }
    if (!cJSON_IsNumber(mbs_med_comp_num)) {
        ogs_error("OpenAPI_mbs_media_comp_parseFromJSON() failed [mbs_med_comp_num]");
        goto end;
    }

    mbs_flow_descs = cJSON_GetObjectItemCaseSensitive(mbs_media_compJSON, "mbsFlowDescs");
    if (mbs_flow_descs) {
        cJSON *mbs_flow_descs_local = NULL;
        if (!cJSON_IsArray(mbs_flow_descs)) {
            ogs_error("OpenAPI_mbs_media_comp_parseFromJSON() failed [mbs_flow_descs]");
            goto end;
        }

        mbs_flow_descsList = OpenAPI_list_create();

        cJSON_ArrayForEach(mbs_flow_descs_local, mbs_flow_descs) {
            double *localDouble = NULL;
            int *localInt = NULL;
            if (!cJSON_IsString(mbs_flow_descs_local)) {
                ogs_error("OpenAPI_mbs_media_comp_parseFromJSON() failed [mbs_flow_descs]");
                goto end;
            }
            OpenAPI_list_add(mbs_flow_descsList, ogs_strdup(mbs_flow_descs_local->valuestring));
        }
    }

    mbs_sdf_res_prio = cJSON_GetObjectItemCaseSensitive(mbs_media_compJSON, "mbsSdfResPrio");
    if (mbs_sdf_res_prio) {
    if (!cJSON_IsString(mbs_sdf_res_prio)) {
        ogs_error("OpenAPI_mbs_media_comp_parseFromJSON() failed [mbs_sdf_res_prio]");
        goto end;
    }
    mbs_sdf_res_prioVariable = OpenAPI_reserv_priority_FromString(mbs_sdf_res_prio->valuestring);
    }

    mbs_media_info = cJSON_GetObjectItemCaseSensitive(mbs_media_compJSON, "mbsMediaInfo");
    if (mbs_media_info) {
    mbs_media_info_local_nonprim = OpenAPI_mbs_media_info_parseFromJSON(mbs_media_info);
    if (!mbs_media_info_local_nonprim) {
        ogs_error("OpenAPI_mbs_media_info_parseFromJSON failed [mbs_media_info]");
        goto end;
    }
    }

    qos_ref = cJSON_GetObjectItemCaseSensitive(mbs_media_compJSON, "qosRef");
    if (qos_ref) {
    if (!cJSON_IsString(qos_ref) && !cJSON_IsNull(qos_ref)) {
        ogs_error("OpenAPI_mbs_media_comp_parseFromJSON() failed [qos_ref]");
        goto end;
    }
    }

    mbs_qo_s_req = cJSON_GetObjectItemCaseSensitive(mbs_media_compJSON, "mbsQoSReq");
    if (mbs_qo_s_req) {
    mbs_qo_s_req_local_nonprim = OpenAPI_mbs_qo_s_req_parseFromJSON(mbs_qo_s_req);
    if (!mbs_qo_s_req_local_nonprim) {
        ogs_error("OpenAPI_mbs_qo_s_req_parseFromJSON failed [mbs_qo_s_req]");
        goto end;
    }
    }

    mbs_media_comp_local_var = OpenAPI_mbs_media_comp_create (
        
        mbs_med_comp_num->valuedouble,
        mbs_flow_descs ? mbs_flow_descsList : NULL,
        mbs_sdf_res_prio ? mbs_sdf_res_prioVariable : 0,
        mbs_media_info ? mbs_media_info_local_nonprim : NULL,
        qos_ref && !cJSON_IsNull(qos_ref) ? ogs_strdup(qos_ref->valuestring) : NULL,
        mbs_qo_s_req ? mbs_qo_s_req_local_nonprim : NULL
    );

    return mbs_media_comp_local_var;
end:
    if (mbs_flow_descsList) {
        OpenAPI_list_for_each(mbs_flow_descsList, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_flow_descsList);
        mbs_flow_descsList = NULL;
    }
    if (mbs_media_info_local_nonprim) {
        OpenAPI_mbs_media_info_free(mbs_media_info_local_nonprim);
        mbs_media_info_local_nonprim = NULL;
    }
    if (mbs_qo_s_req_local_nonprim) {
        OpenAPI_mbs_qo_s_req_free(mbs_qo_s_req_local_nonprim);
        mbs_qo_s_req_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_mbs_media_comp_t *OpenAPI_mbs_media_comp_copy(OpenAPI_mbs_media_comp_t *dst, OpenAPI_mbs_media_comp_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_media_comp_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_media_comp_convertToJSON() failed");
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

    OpenAPI_mbs_media_comp_free(dst);
    dst = OpenAPI_mbs_media_comp_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

