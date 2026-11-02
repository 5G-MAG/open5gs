
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_policy_ctxt_data.h"

OpenAPI_mbs_policy_ctxt_data_t *OpenAPI_mbs_policy_ctxt_data_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    char *dnn,
    OpenAPI_snssai_t *snssai,
    bool is_area_sess_pol_id,
    int area_sess_pol_id,
    OpenAPI_mbs_service_info_t *mbs_serv_info,
    char *supp_feat
)
{
    OpenAPI_mbs_policy_ctxt_data_t *local_var = ogs_malloc(sizeof(OpenAPI_mbs_policy_ctxt_data_t));
    ogs_assert(local_var);

    local_var->mbs_session_id = mbs_session_id;
    local_var->dnn = dnn;
    local_var->snssai = snssai;
    local_var->is_area_sess_pol_id = is_area_sess_pol_id;
    local_var->area_sess_pol_id = area_sess_pol_id;
    local_var->mbs_serv_info = mbs_serv_info;
    local_var->supp_feat = supp_feat;

    return local_var;
}

void OpenAPI_mbs_policy_ctxt_data_free(OpenAPI_mbs_policy_ctxt_data_t *mbs_policy_ctxt_data)
{
    if (NULL == mbs_policy_ctxt_data) {
        return;
    }
    if (mbs_policy_ctxt_data->mbs_session_id) {
        OpenAPI_mbs_session_id_free(mbs_policy_ctxt_data->mbs_session_id);
        mbs_policy_ctxt_data->mbs_session_id = NULL;
    }
    if (mbs_policy_ctxt_data->dnn) {
        ogs_free(mbs_policy_ctxt_data->dnn);
        mbs_policy_ctxt_data->dnn = NULL;
    }
    if (mbs_policy_ctxt_data->snssai) {
        OpenAPI_snssai_free(mbs_policy_ctxt_data->snssai);
        mbs_policy_ctxt_data->snssai = NULL;
    }
    if (mbs_policy_ctxt_data->mbs_serv_info) {
        OpenAPI_mbs_service_info_free(mbs_policy_ctxt_data->mbs_serv_info);
        mbs_policy_ctxt_data->mbs_serv_info = NULL;
    }
    if (mbs_policy_ctxt_data->supp_feat) {
        ogs_free(mbs_policy_ctxt_data->supp_feat);
        mbs_policy_ctxt_data->supp_feat = NULL;
    }
    ogs_free(mbs_policy_ctxt_data);
}

cJSON *OpenAPI_mbs_policy_ctxt_data_convertToJSON(OpenAPI_mbs_policy_ctxt_data_t *mbs_policy_ctxt_data)
{
    cJSON *item = NULL;

    if (mbs_policy_ctxt_data == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [MbsPolicyCtxtData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_policy_ctxt_data->mbs_session_id) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [mbs_session_id]");
        return NULL;
    }
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(mbs_policy_ctxt_data->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }

    if (mbs_policy_ctxt_data->dnn) {
    if (cJSON_AddStringToObject(item, "dnn", mbs_policy_ctxt_data->dnn) == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [dnn]");
        goto end;
    }
    }

    if (mbs_policy_ctxt_data->snssai) {
    cJSON *snssai_local_JSON = OpenAPI_snssai_convertToJSON(mbs_policy_ctxt_data->snssai);
    if (snssai_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [snssai]");
        goto end;
    }
    cJSON_AddItemToObject(item, "snssai", snssai_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [snssai]");
        goto end;
    }
    }

    if (mbs_policy_ctxt_data->is_area_sess_pol_id) {
    if (cJSON_AddNumberToObject(item, "areaSessPolId", mbs_policy_ctxt_data->area_sess_pol_id) == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [area_sess_pol_id]");
        goto end;
    }
    }

    if (mbs_policy_ctxt_data->mbs_serv_info) {
    cJSON *mbs_serv_info_local_JSON = OpenAPI_mbs_service_info_convertToJSON(mbs_policy_ctxt_data->mbs_serv_info);
    if (mbs_serv_info_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [mbs_serv_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsServInfo", mbs_serv_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [mbs_serv_info]");
        goto end;
    }
    }

    if (mbs_policy_ctxt_data->supp_feat) {
    if (cJSON_AddStringToObject(item, "suppFeat", mbs_policy_ctxt_data->supp_feat) == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed [supp_feat]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_policy_ctxt_data_t *OpenAPI_mbs_policy_ctxt_data_parseFromJSON(cJSON *mbs_policy_ctxt_dataJSON)
{
    OpenAPI_mbs_policy_ctxt_data_t *local_var = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *dnn = NULL;
    cJSON *snssai = NULL;
    OpenAPI_snssai_t *snssai_local_nonprim = NULL;
    cJSON *area_sess_pol_id = NULL;
    cJSON *mbs_serv_info = NULL;
    OpenAPI_mbs_service_info_t *mbs_serv_info_local_nonprim = NULL;
    cJSON *supp_feat = NULL;

    mbs_session_id = cJSON_GetObjectItemCaseSensitive(mbs_policy_ctxt_dataJSON, "mbsSessionId");
    if (!mbs_session_id) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_parseFromJSON() failed [mbs_session_id]");
        goto end;
    }
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }

    dnn = cJSON_GetObjectItemCaseSensitive(mbs_policy_ctxt_dataJSON, "dnn");
    if (dnn) {
    if (!cJSON_IsString(dnn) && !cJSON_IsNull(dnn)) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_parseFromJSON() failed [dnn]");
        goto end;
    }
    }

    snssai = cJSON_GetObjectItemCaseSensitive(mbs_policy_ctxt_dataJSON, "snssai");
    if (snssai) {
    snssai_local_nonprim = OpenAPI_snssai_parseFromJSON(snssai);
    if (!snssai_local_nonprim) {
        ogs_error("OpenAPI_snssai_parseFromJSON failed [snssai]");
        goto end;
    }
    }

    area_sess_pol_id = cJSON_GetObjectItemCaseSensitive(mbs_policy_ctxt_dataJSON, "areaSessPolId");
    if (area_sess_pol_id) {
    if (!cJSON_IsNumber(area_sess_pol_id)) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_parseFromJSON() failed [area_sess_pol_id]");
        goto end;
    }
    }

    mbs_serv_info = cJSON_GetObjectItemCaseSensitive(mbs_policy_ctxt_dataJSON, "mbsServInfo");
    if (mbs_serv_info) {
    mbs_serv_info_local_nonprim = OpenAPI_mbs_service_info_parseFromJSON(mbs_serv_info);
    if (!mbs_serv_info_local_nonprim) {
        ogs_error("OpenAPI_mbs_service_info_parseFromJSON failed [mbs_serv_info]");
        goto end;
    }
    }

    supp_feat = cJSON_GetObjectItemCaseSensitive(mbs_policy_ctxt_dataJSON, "suppFeat");
    if (supp_feat) {
    if (!cJSON_IsString(supp_feat) && !cJSON_IsNull(supp_feat)) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_parseFromJSON() failed [supp_feat]");
        goto end;
    }
    }

    local_var = OpenAPI_mbs_policy_ctxt_data_create (
        mbs_session_id_local_nonprim,
        dnn && !cJSON_IsNull(dnn) ? ogs_strdup(dnn->valuestring) : NULL,
        snssai ? snssai_local_nonprim : NULL,
        area_sess_pol_id ? true : false,
        area_sess_pol_id ? area_sess_pol_id->valuedouble : 0,
        mbs_serv_info ? mbs_serv_info_local_nonprim : NULL,
        supp_feat && !cJSON_IsNull(supp_feat) ? ogs_strdup(supp_feat->valuestring) : NULL
    );

    return local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (snssai_local_nonprim) {
        OpenAPI_snssai_free(snssai_local_nonprim);
        snssai_local_nonprim = NULL;
    }
    if (mbs_serv_info_local_nonprim) {
        OpenAPI_mbs_service_info_free(mbs_serv_info_local_nonprim);
        mbs_serv_info_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_mbs_policy_ctxt_data_t *OpenAPI_mbs_policy_ctxt_data_copy(OpenAPI_mbs_policy_ctxt_data_t *dst, OpenAPI_mbs_policy_ctxt_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_policy_ctxt_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_convertToJSON() failed");
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

    OpenAPI_mbs_policy_ctxt_data_free(dst);
    dst = OpenAPI_mbs_policy_ctxt_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

