
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nmbsmf_n2_mbs_sm_info.h"

OpenAPI_nmbsmf_n2_mbs_sm_info_t *OpenAPI_nmbsmf_n2_mbs_sm_info_create(
    OpenAPI_ngap_ie_type_e ngap_ie_type,
    OpenAPI_ref_to_binary_data_t *ngap_data
)
{
    OpenAPI_nmbsmf_n2_mbs_sm_info_t *nmbsmf_n2_mbs_sm_info_local_var = ogs_malloc(sizeof(OpenAPI_nmbsmf_n2_mbs_sm_info_t));
    ogs_assert(nmbsmf_n2_mbs_sm_info_local_var);

    nmbsmf_n2_mbs_sm_info_local_var->ngap_ie_type = ngap_ie_type;
    nmbsmf_n2_mbs_sm_info_local_var->ngap_data = ngap_data;

    return nmbsmf_n2_mbs_sm_info_local_var;
}

void OpenAPI_nmbsmf_n2_mbs_sm_info_free(OpenAPI_nmbsmf_n2_mbs_sm_info_t *nmbsmf_n2_mbs_sm_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == nmbsmf_n2_mbs_sm_info) {
        return;
    }
    if (nmbsmf_n2_mbs_sm_info->ngap_data) {
        OpenAPI_ref_to_binary_data_free(nmbsmf_n2_mbs_sm_info->ngap_data);
        nmbsmf_n2_mbs_sm_info->ngap_data = NULL;
    }
    ogs_free(nmbsmf_n2_mbs_sm_info);
}

cJSON *OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON(OpenAPI_nmbsmf_n2_mbs_sm_info_t *nmbsmf_n2_mbs_sm_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (nmbsmf_n2_mbs_sm_info == NULL) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON() failed [NmbsmfN2MbsSmInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (nmbsmf_n2_mbs_sm_info->ngap_ie_type == OpenAPI_ngap_ie_type_NULL) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON() failed [ngap_ie_type]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "ngapIeType", OpenAPI_ngap_ie_type_ToString(nmbsmf_n2_mbs_sm_info->ngap_ie_type)) == NULL) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON() failed [ngap_ie_type]");
        goto end;
    }

    if (!nmbsmf_n2_mbs_sm_info->ngap_data) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON() failed [ngap_data]");
        return NULL;
    }
    cJSON *ngap_data_local_JSON = OpenAPI_ref_to_binary_data_convertToJSON(nmbsmf_n2_mbs_sm_info->ngap_data);
    if (ngap_data_local_JSON == NULL) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON() failed [ngap_data]");
        goto end;
    }
    cJSON_AddItemToObject(item, "ngapData", ngap_data_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON() failed [ngap_data]");
        goto end;
    }

end:
    return item;
}

OpenAPI_nmbsmf_n2_mbs_sm_info_t *OpenAPI_nmbsmf_n2_mbs_sm_info_parseFromJSON(cJSON *nmbsmf_n2_mbs_sm_infoJSON)
{
    OpenAPI_nmbsmf_n2_mbs_sm_info_t *nmbsmf_n2_mbs_sm_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *ngap_ie_type = NULL;
    OpenAPI_ngap_ie_type_e ngap_ie_typeVariable = 0;
    cJSON *ngap_data = NULL;
    OpenAPI_ref_to_binary_data_t *ngap_data_local_nonprim = NULL;
    ngap_ie_type = cJSON_GetObjectItemCaseSensitive(nmbsmf_n2_mbs_sm_infoJSON, "ngapIeType");
    if (!ngap_ie_type) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_parseFromJSON() failed [ngap_ie_type]");
        goto end;
    }
    if (!cJSON_IsString(ngap_ie_type)) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_parseFromJSON() failed [ngap_ie_type]");
        goto end;
    }
    ngap_ie_typeVariable = OpenAPI_ngap_ie_type_FromString(ngap_ie_type->valuestring);

    ngap_data = cJSON_GetObjectItemCaseSensitive(nmbsmf_n2_mbs_sm_infoJSON, "ngapData");
    if (!ngap_data) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_parseFromJSON() failed [ngap_data]");
        goto end;
    }
    ngap_data_local_nonprim = OpenAPI_ref_to_binary_data_parseFromJSON(ngap_data);
    if (!ngap_data_local_nonprim) {
        ogs_error("OpenAPI_ref_to_binary_data_parseFromJSON failed [ngap_data]");
        goto end;
    }

    nmbsmf_n2_mbs_sm_info_local_var = OpenAPI_nmbsmf_n2_mbs_sm_info_create (
        ngap_ie_typeVariable,
        ngap_data_local_nonprim
    );

    return nmbsmf_n2_mbs_sm_info_local_var;
end:
    if (ngap_data_local_nonprim) {
        OpenAPI_ref_to_binary_data_free(ngap_data_local_nonprim);
        ngap_data_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_nmbsmf_n2_mbs_sm_info_t *OpenAPI_nmbsmf_n2_mbs_sm_info_copy(OpenAPI_nmbsmf_n2_mbs_sm_info_t *dst, OpenAPI_nmbsmf_n2_mbs_sm_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_nmbsmf_n2_mbs_sm_info_convertToJSON() failed");
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

    OpenAPI_nmbsmf_n2_mbs_sm_info_free(dst);
    dst = OpenAPI_nmbsmf_n2_mbs_sm_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

