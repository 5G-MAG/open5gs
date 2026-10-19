
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_comm_n2_mbs_sm_info.h"

OpenAPI_mbs_comm_n2_mbs_sm_info_t *OpenAPI_mbs_comm_n2_mbs_sm_info_create(
    OpenAPI_mbs_ngap_ie_type_e ngap_ie_type,
    OpenAPI_ref_to_binary_data_t *ngap_data
)
{
    OpenAPI_mbs_comm_n2_mbs_sm_info_t *mbs_comm_n2_mbs_sm_info_local_var = ogs_malloc(sizeof(OpenAPI_mbs_comm_n2_mbs_sm_info_t));
    ogs_assert(mbs_comm_n2_mbs_sm_info_local_var);

    mbs_comm_n2_mbs_sm_info_local_var->ngap_ie_type = ngap_ie_type;
    mbs_comm_n2_mbs_sm_info_local_var->ngap_data = ngap_data;

    return mbs_comm_n2_mbs_sm_info_local_var;
}

void OpenAPI_mbs_comm_n2_mbs_sm_info_free(OpenAPI_mbs_comm_n2_mbs_sm_info_t *mbs_comm_n2_mbs_sm_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_comm_n2_mbs_sm_info) {
        return;
    }
    if (mbs_comm_n2_mbs_sm_info->ngap_data) {
        OpenAPI_ref_to_binary_data_free(mbs_comm_n2_mbs_sm_info->ngap_data);
        mbs_comm_n2_mbs_sm_info->ngap_data = NULL;
    }
    ogs_free(mbs_comm_n2_mbs_sm_info);
}

cJSON *OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON(OpenAPI_mbs_comm_n2_mbs_sm_info_t *mbs_comm_n2_mbs_sm_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_comm_n2_mbs_sm_info == NULL) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON() failed [MBSCommN2MbsSmInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_comm_n2_mbs_sm_info->ngap_ie_type == OpenAPI_mbs_ngap_ie_type_NULL) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON() failed [ngap_ie_type]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "ngapIeType", OpenAPI_mbs_ngap_ie_type_ToString(mbs_comm_n2_mbs_sm_info->ngap_ie_type)) == NULL) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON() failed [ngap_ie_type]");
        goto end;
    }

    if (!mbs_comm_n2_mbs_sm_info->ngap_data) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON() failed [ngap_data]");
        return NULL;
    }
    cJSON *ngap_data_local_JSON = OpenAPI_ref_to_binary_data_convertToJSON(mbs_comm_n2_mbs_sm_info->ngap_data);
    if (ngap_data_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON() failed [ngap_data]");
        goto end;
    }
    cJSON_AddItemToObject(item, "ngapData", ngap_data_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON() failed [ngap_data]");
        goto end;
    }

end:
    return item;
}

OpenAPI_mbs_comm_n2_mbs_sm_info_t *OpenAPI_mbs_comm_n2_mbs_sm_info_parseFromJSON(cJSON *mbs_comm_n2_mbs_sm_infoJSON)
{
    OpenAPI_mbs_comm_n2_mbs_sm_info_t *mbs_comm_n2_mbs_sm_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *ngap_ie_type = NULL;
    OpenAPI_mbs_ngap_ie_type_e ngap_ie_typeVariable = 0;
    cJSON *ngap_data = NULL;
    OpenAPI_ref_to_binary_data_t *ngap_data_local_nonprim = NULL;
    ngap_ie_type = cJSON_GetObjectItemCaseSensitive(mbs_comm_n2_mbs_sm_infoJSON, "ngapIeType");
    if (!ngap_ie_type) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_parseFromJSON() failed [ngap_ie_type]");
        goto end;
    }
    if (!cJSON_IsString(ngap_ie_type)) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_parseFromJSON() failed [ngap_ie_type]");
        goto end;
    }
    ngap_ie_typeVariable = OpenAPI_mbs_ngap_ie_type_FromString(ngap_ie_type->valuestring);

    ngap_data = cJSON_GetObjectItemCaseSensitive(mbs_comm_n2_mbs_sm_infoJSON, "ngapData");
    if (!ngap_data) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_parseFromJSON() failed [ngap_data]");
        goto end;
    }
    ngap_data_local_nonprim = OpenAPI_ref_to_binary_data_parseFromJSON(ngap_data);
    if (!ngap_data_local_nonprim) {
        ogs_error("OpenAPI_ref_to_binary_data_parseFromJSON failed [ngap_data]");
        goto end;
    }

    mbs_comm_n2_mbs_sm_info_local_var = OpenAPI_mbs_comm_n2_mbs_sm_info_create (
        ngap_ie_typeVariable,
        ngap_data_local_nonprim
    );

    return mbs_comm_n2_mbs_sm_info_local_var;
end:
    if (ngap_data_local_nonprim) {
        OpenAPI_ref_to_binary_data_free(ngap_data_local_nonprim);
        ngap_data_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_mbs_comm_n2_mbs_sm_info_t *OpenAPI_mbs_comm_n2_mbs_sm_info_copy(OpenAPI_mbs_comm_n2_mbs_sm_info_t *dst, OpenAPI_mbs_comm_n2_mbs_sm_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON() failed");
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

    OpenAPI_mbs_comm_n2_mbs_sm_info_free(dst);
    dst = OpenAPI_mbs_comm_n2_mbs_sm_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

