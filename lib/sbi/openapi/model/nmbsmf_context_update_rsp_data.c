
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nmbsmf_context_update_rsp_data.h"

OpenAPI_nmbsmf_context_update_rsp_data_t *OpenAPI_nmbsmf_context_update_rsp_data_create(
    OpenAPI_ssm_t *ll_ssm,
    bool is_c_teid,
    int c_teid,
    OpenAPI_nmbsmf_n2_mbs_sm_info_t *n2_mbs_sm_info
)
{
    OpenAPI_nmbsmf_context_update_rsp_data_t *nmbsmf_context_update_rsp_data_local_var = ogs_malloc(sizeof(OpenAPI_nmbsmf_context_update_rsp_data_t));
    ogs_assert(nmbsmf_context_update_rsp_data_local_var);

    nmbsmf_context_update_rsp_data_local_var->ll_ssm = ll_ssm;
    nmbsmf_context_update_rsp_data_local_var->is_c_teid = is_c_teid;
    nmbsmf_context_update_rsp_data_local_var->c_teid = c_teid;
    nmbsmf_context_update_rsp_data_local_var->n2_mbs_sm_info = n2_mbs_sm_info;

    return nmbsmf_context_update_rsp_data_local_var;
}

void OpenAPI_nmbsmf_context_update_rsp_data_free(OpenAPI_nmbsmf_context_update_rsp_data_t *nmbsmf_context_update_rsp_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == nmbsmf_context_update_rsp_data) {
        return;
    }
    if (nmbsmf_context_update_rsp_data->ll_ssm) {
        OpenAPI_ssm_free(nmbsmf_context_update_rsp_data->ll_ssm);
        nmbsmf_context_update_rsp_data->ll_ssm = NULL;
    }
    if (nmbsmf_context_update_rsp_data->n2_mbs_sm_info) {
        OpenAPI_nmbsmf_n2_mbs_sm_info_free(nmbsmf_context_update_rsp_data->n2_mbs_sm_info);
        nmbsmf_context_update_rsp_data->n2_mbs_sm_info = NULL;
    }
    ogs_free(nmbsmf_context_update_rsp_data);
}

cJSON *OpenAPI_nmbsmf_context_update_rsp_data_convertToJSON(OpenAPI_nmbsmf_context_update_rsp_data_t *nmbsmf_context_update_rsp_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (nmbsmf_context_update_rsp_data == NULL) {
        ogs_error("OpenAPI_nmbsmf_context_update_rsp_data_convertToJSON() failed [NmbsmfContextUpdateRspData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (nmbsmf_context_update_rsp_data->ll_ssm) {
    cJSON *ll_ssm_local_JSON = OpenAPI_ssm_convertToJSON(nmbsmf_context_update_rsp_data->ll_ssm);
    if (ll_ssm_local_JSON == NULL) {
        ogs_error("OpenAPI_nmbsmf_context_update_rsp_data_convertToJSON() failed [ll_ssm]");
        goto end;
    }
    cJSON_AddItemToObject(item, "llSsm", ll_ssm_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_nmbsmf_context_update_rsp_data_convertToJSON() failed [ll_ssm]");
        goto end;
    }
    }

    if (nmbsmf_context_update_rsp_data->is_c_teid) {
    if (cJSON_AddNumberToObject(item, "cTeid", nmbsmf_context_update_rsp_data->c_teid) == NULL) {
        ogs_error("OpenAPI_nmbsmf_context_update_rsp_data_convertToJSON() failed [c_teid]");
        goto end;
    }
    }

    if (nmbsmf_context_update_rsp_data->n2_mbs_sm_info) {
    cJSON *n2_mbs_sm_info_local_JSON = OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON(nmbsmf_context_update_rsp_data->n2_mbs_sm_info);
    if (n2_mbs_sm_info_local_JSON == NULL) {
        ogs_error("OpenAPI_nmbsmf_context_update_rsp_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "n2MbsSmInfo", n2_mbs_sm_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_nmbsmf_context_update_rsp_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_nmbsmf_context_update_rsp_data_t *OpenAPI_nmbsmf_context_update_rsp_data_parseFromJSON(cJSON *nmbsmf_context_update_rsp_dataJSON)
{
    OpenAPI_nmbsmf_context_update_rsp_data_t *nmbsmf_context_update_rsp_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *ll_ssm = NULL;
    OpenAPI_ssm_t *ll_ssm_local_nonprim = NULL;
    cJSON *c_teid = NULL;
    cJSON *n2_mbs_sm_info = NULL;
    OpenAPI_nmbsmf_n2_mbs_sm_info_t *n2_mbs_sm_info_local_nonprim = NULL;
    ll_ssm = cJSON_GetObjectItemCaseSensitive(nmbsmf_context_update_rsp_dataJSON, "llSsm");
    if (ll_ssm) {
    ll_ssm_local_nonprim = OpenAPI_ssm_parseFromJSON(ll_ssm);
    if (!ll_ssm_local_nonprim) {
        ogs_error("OpenAPI_ssm_parseFromJSON failed [ll_ssm]");
        goto end;
    }
    }

    c_teid = cJSON_GetObjectItemCaseSensitive(nmbsmf_context_update_rsp_dataJSON, "cTeid");
    if (c_teid) {
    if (!cJSON_IsNumber(c_teid)) {
        ogs_error("OpenAPI_nmbsmf_context_update_rsp_data_parseFromJSON() failed [c_teid]");
        goto end;
    }
    }

    n2_mbs_sm_info = cJSON_GetObjectItemCaseSensitive(nmbsmf_context_update_rsp_dataJSON, "n2MbsSmInfo");
    if (n2_mbs_sm_info) {
    n2_mbs_sm_info_local_nonprim = OpenAPI_nmbsmf_n2_mbs_sm_info_parseFromJSON(n2_mbs_sm_info);
    if (!n2_mbs_sm_info_local_nonprim) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_parseFromJSON failed [n2_mbs_sm_info]");
        goto end;
    }
    }

    nmbsmf_context_update_rsp_data_local_var = OpenAPI_nmbsmf_context_update_rsp_data_create (
        ll_ssm ? ll_ssm_local_nonprim : NULL,
        c_teid ? true : false,
        c_teid ? c_teid->valuedouble : 0,
        n2_mbs_sm_info ? n2_mbs_sm_info_local_nonprim : NULL
    );

    return nmbsmf_context_update_rsp_data_local_var;
end:
    if (ll_ssm_local_nonprim) {
        OpenAPI_ssm_free(ll_ssm_local_nonprim);
        ll_ssm_local_nonprim = NULL;
    }
    if (n2_mbs_sm_info_local_nonprim) {
        OpenAPI_nmbsmf_n2_mbs_sm_info_free(n2_mbs_sm_info_local_nonprim);
        n2_mbs_sm_info_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_nmbsmf_context_update_rsp_data_t *OpenAPI_nmbsmf_context_update_rsp_data_copy(OpenAPI_nmbsmf_context_update_rsp_data_t *dst, OpenAPI_nmbsmf_context_update_rsp_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_nmbsmf_context_update_rsp_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_nmbsmf_context_update_rsp_data_convertToJSON() failed");
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

    OpenAPI_nmbsmf_context_update_rsp_data_free(dst);
    dst = OpenAPI_nmbsmf_context_update_rsp_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

