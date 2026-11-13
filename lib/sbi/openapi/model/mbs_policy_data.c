
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_policy_data.h"

OpenAPI_mbs_policy_data_t *OpenAPI_mbs_policy_data_create(
    OpenAPI_mbs_policy_ctxt_data_t *mbs_policy_ctxt_data,
    OpenAPI_mbs_policy_decision_t *mbs_policies,
    char *supp_feat
)
{
    OpenAPI_mbs_policy_data_t *mbs_policy_data_local_var = ogs_malloc(sizeof(OpenAPI_mbs_policy_data_t));
    ogs_assert(mbs_policy_data_local_var);

    mbs_policy_data_local_var->mbs_policy_ctxt_data = mbs_policy_ctxt_data;
    mbs_policy_data_local_var->mbs_policies = mbs_policies;
    mbs_policy_data_local_var->supp_feat = supp_feat;

    return mbs_policy_data_local_var;
}

void OpenAPI_mbs_policy_data_free(OpenAPI_mbs_policy_data_t *mbs_policy_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_policy_data) {
        return;
    }
    if (mbs_policy_data->mbs_policy_ctxt_data) {
        OpenAPI_mbs_policy_ctxt_data_free(mbs_policy_data->mbs_policy_ctxt_data);
        mbs_policy_data->mbs_policy_ctxt_data = NULL;
    }
    if (mbs_policy_data->mbs_policies) {
        OpenAPI_mbs_policy_decision_free(mbs_policy_data->mbs_policies);
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
    OpenAPI_lnode_t *node = NULL;

    if (mbs_policy_data == NULL) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [MbsPolicyData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_policy_data->mbs_policy_ctxt_data) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [mbs_policy_ctxt_data]");
        return NULL;
    }
    cJSON *mbs_policy_ctxt_data_local_JSON = OpenAPI_mbs_policy_ctxt_data_convertToJSON(mbs_policy_data->mbs_policy_ctxt_data);
    if (mbs_policy_ctxt_data_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [mbs_policy_ctxt_data]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsPolicyCtxtData", mbs_policy_ctxt_data_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [mbs_policy_ctxt_data]");
        goto end;
    }

    if (mbs_policy_data->mbs_policies) {
    cJSON *mbs_policies_local_JSON = OpenAPI_mbs_policy_decision_convertToJSON(mbs_policy_data->mbs_policies);
    if (mbs_policies_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [mbs_policies]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsPolicies", mbs_policies_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_policy_data_convertToJSON() failed [mbs_policies]");
        goto end;
    }
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
    OpenAPI_mbs_policy_data_t *mbs_policy_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_policy_ctxt_data = NULL;
    OpenAPI_mbs_policy_ctxt_data_t *mbs_policy_ctxt_data_local_nonprim = NULL;
    cJSON *mbs_policies = NULL;
    OpenAPI_mbs_policy_decision_t *mbs_policies_local_nonprim = NULL;
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
    mbs_policies_local_nonprim = OpenAPI_mbs_policy_decision_parseFromJSON(mbs_policies);
    if (!mbs_policies_local_nonprim) {
        ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON failed [mbs_policies]");
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

    mbs_policy_data_local_var = OpenAPI_mbs_policy_data_create (
        mbs_policy_ctxt_data_local_nonprim,
        mbs_policies ? mbs_policies_local_nonprim : NULL,
        supp_feat && !cJSON_IsNull(supp_feat) ? ogs_strdup(supp_feat->valuestring) : NULL
    );

    return mbs_policy_data_local_var;
end:
    if (mbs_policy_ctxt_data_local_nonprim) {
        OpenAPI_mbs_policy_ctxt_data_free(mbs_policy_ctxt_data_local_nonprim);
        mbs_policy_ctxt_data_local_nonprim = NULL;
    }
    if (mbs_policies_local_nonprim) {
        OpenAPI_mbs_policy_decision_free(mbs_policies_local_nonprim);
        mbs_policies_local_nonprim = NULL;
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

