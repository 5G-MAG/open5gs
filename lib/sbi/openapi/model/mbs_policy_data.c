
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_policy_data.h"

OpenAPI_mbs_policy_data_t *OpenAPI_mbs_policy_data_create(
    OpenAPI_mbs_policy_ctxt_data_t *mbs_policy_ctxt_data,
    cJSON *mbs_policies,
    char *supp_feat
)
{
    OpenAPI_mbs_policy_data_t *local_var = ogs_malloc(sizeof(OpenAPI_mbs_policy_data_t));
    ogs_assert(local_var);

    local_var->mbs_policy_ctxt_data = mbs_policy_ctxt_data;
    local_var->mbs_policies = mbs_policies;
    local_var->supp_feat = supp_feat;

    return local_var;
}

void OpenAPI_mbs_policy_data_free(OpenAPI_mbs_policy_data_t *mbs_policy_data)
{
    if (NULL == mbs_policy_data) {
        return;
    }
    if (mbs_policy_data->mbs_policy_ctxt_data) {
        OpenAPI_mbs_policy_ctxt_data_free(mbs_policy_data->mbs_policy_ctxt_data);
        mbs_policy_data->mbs_policy_ctxt_data = NULL;
    }
    if (mbs_policy_data->mbs_policies) {
        cJSON_Delete(mbs_policy_data->mbs_policies);
        mbs_policy_data->mbs_policies = NULL;
    }
    if (mbs_policy_data->supp_feat) {
        ogs_free(mbs_policy_data->supp_feat);
        mbs_policy_data->supp_feat = NULL;
    }
    ogs_free(mbs_policy_data);
}

cJSON *OpenAPI_mbs_policy_data_convertToJSON(OpenAPI_mbs_policy_data_t *mbs_policy_data)
{
    cJSON *item = NULL;

    if (mbs_policy_data == NULL) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [MbsPolicyData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_policy_data->mbs_policy_ctxt_data) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [mbs_policy_ctxt_data]");
        return NULL;
    }
    cJSON *ctxt_local_JSON = OpenAPI_mbs_policy_ctxt_data_convertToJSON(mbs_policy_data->mbs_policy_ctxt_data);
    if (ctxt_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [mbs_policy_ctxt_data]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsPolicyCtxtData", ctxt_local_JSON);

    if (mbs_policy_data->mbs_policies) {
    cJSON *policies = cJSON_Duplicate(mbs_policy_data->mbs_policies, 1);
    if (policies == NULL) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [mbs_policies]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsPolicies", policies);
    }

    if (mbs_policy_data->supp_feat) {
    if (cJSON_AddStringToObject(item, "suppFeat", mbs_policy_data->supp_feat) == NULL) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [supp_feat]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_policy_data_t *OpenAPI_mbs_policy_data_parseFromJSON(cJSON *mbs_policy_dataJSON)
{
    OpenAPI_mbs_policy_data_t *local_var = NULL;
    cJSON *mbs_policy_ctxt_data = NULL;
    OpenAPI_mbs_policy_ctxt_data_t *mbs_policy_ctxt_data_local_nonprim = NULL;
    cJSON *mbs_policies = NULL;
    cJSON *mbs_policies_copy = NULL;
    cJSON *supp_feat = NULL;

    mbs_policy_ctxt_data = cJSON_GetObjectItemCaseSensitive(mbs_policy_dataJSON, "mbsPolicyCtxtData");
    if (!mbs_policy_ctxt_data) {
        ogs_error("OpenAPI_mbs_policy_data_parseFromJSON() failed [mbs_policy_ctxt_data]");
        goto end;
    }
    mbs_policy_ctxt_data_local_nonprim = OpenAPI_mbs_policy_ctxt_data_parseFromJSON(mbs_policy_ctxt_data);
    if (!mbs_policy_ctxt_data_local_nonprim) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_parseFromJSON failed [mbs_policy_ctxt_data]");
        goto end;
    }

    mbs_policies = cJSON_GetObjectItemCaseSensitive(mbs_policy_dataJSON, "mbsPolicies");
    if (mbs_policies) {
    if (!cJSON_IsObject(mbs_policies)) {
        ogs_error("OpenAPI_mbs_policy_data_parseFromJSON() failed [mbs_policies]");
        goto end;
    }
    mbs_policies_copy = cJSON_Duplicate(mbs_policies, 1);
    if (!mbs_policies_copy) {
        ogs_error("OpenAPI_mbs_policy_data_parseFromJSON() failed [mbs_policies]");
        goto end;
    }
    }

    supp_feat = cJSON_GetObjectItemCaseSensitive(mbs_policy_dataJSON, "suppFeat");
    if (supp_feat) {
    if (!cJSON_IsString(supp_feat) && !cJSON_IsNull(supp_feat)) {
        ogs_error("OpenAPI_mbs_policy_data_parseFromJSON() failed [supp_feat]");
        goto end;
    }
    }

    local_var = OpenAPI_mbs_policy_data_create (
        mbs_policy_ctxt_data_local_nonprim,
        mbs_policies_copy,
        supp_feat && !cJSON_IsNull(supp_feat) ? ogs_strdup(supp_feat->valuestring) : NULL
    );

    return local_var;
end:
    if (mbs_policy_ctxt_data_local_nonprim) {
        OpenAPI_mbs_policy_ctxt_data_free(mbs_policy_ctxt_data_local_nonprim);
        mbs_policy_ctxt_data_local_nonprim = NULL;
    }
    if (mbs_policies_copy) {
        cJSON_Delete(mbs_policies_copy);
        mbs_policies_copy = NULL;
    }
    return NULL;
}

OpenAPI_mbs_policy_data_t *OpenAPI_mbs_policy_data_copy(OpenAPI_mbs_policy_data_t *dst, OpenAPI_mbs_policy_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_policy_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed");
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

    OpenAPI_mbs_policy_data_free(dst);
    dst = OpenAPI_mbs_policy_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

