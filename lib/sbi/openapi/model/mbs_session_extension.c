
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_session_extension.h"

OpenAPI_mbs_session_extension_t *OpenAPI_mbs_session_extension_create(
    OpenAPI_mbs_security_context_t *mbs_security_context,
    bool is_contact_pcf_ind,
    int contact_pcf_ind,
    bool is_area_session_policy_id,
    int area_session_policy_id
)
{
    OpenAPI_mbs_session_extension_t *mbs_session_extension_local_var = ogs_malloc(sizeof(OpenAPI_mbs_session_extension_t));
    ogs_assert(mbs_session_extension_local_var);

    mbs_session_extension_local_var->mbs_security_context = mbs_security_context;
    mbs_session_extension_local_var->is_contact_pcf_ind = is_contact_pcf_ind;
    mbs_session_extension_local_var->contact_pcf_ind = contact_pcf_ind;
    mbs_session_extension_local_var->is_area_session_policy_id = is_area_session_policy_id;
    mbs_session_extension_local_var->area_session_policy_id = area_session_policy_id;

    return mbs_session_extension_local_var;
}

void OpenAPI_mbs_session_extension_free(OpenAPI_mbs_session_extension_t *mbs_session_extension)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_session_extension) {
        return;
    }
    if (mbs_session_extension->mbs_security_context) {
        OpenAPI_mbs_security_context_free(mbs_session_extension->mbs_security_context);
        mbs_session_extension->mbs_security_context = NULL;
    }
    ogs_free(mbs_session_extension);
}

cJSON *OpenAPI_mbs_session_extension_convertToJSON(OpenAPI_mbs_session_extension_t *mbs_session_extension)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_session_extension == NULL) {
        ogs_error("OpenAPI_mbs_session_extension_convertToJSON() failed [MbsSessionExtension]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_session_extension->mbs_security_context) {
    cJSON *mbs_security_context_local_JSON = OpenAPI_mbs_security_context_convertToJSON(mbs_session_extension->mbs_security_context);
    if (mbs_security_context_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_session_extension_convertToJSON() failed [mbs_security_context]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSecurityContext", mbs_security_context_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_session_extension_convertToJSON() failed [mbs_security_context]");
        goto end;
    }
    }

    if (mbs_session_extension->is_contact_pcf_ind) {
    if (cJSON_AddBoolToObject(item, "contactPcfInd", mbs_session_extension->contact_pcf_ind) == NULL) {
        ogs_error("OpenAPI_mbs_session_extension_convertToJSON() failed [contact_pcf_ind]");
        goto end;
    }
    }

    if (mbs_session_extension->is_area_session_policy_id) {
    if (cJSON_AddNumberToObject(item, "areaSessionPolicyId", mbs_session_extension->area_session_policy_id) == NULL) {
        ogs_error("OpenAPI_mbs_session_extension_convertToJSON() failed [area_session_policy_id]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_session_extension_t *OpenAPI_mbs_session_extension_parseFromJSON(cJSON *mbs_session_extensionJSON)
{
    OpenAPI_mbs_session_extension_t *mbs_session_extension_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_security_context = NULL;
    OpenAPI_mbs_security_context_t *mbs_security_context_local_nonprim = NULL;
    cJSON *contact_pcf_ind = NULL;
    cJSON *area_session_policy_id = NULL;
    mbs_security_context = cJSON_GetObjectItemCaseSensitive(mbs_session_extensionJSON, "mbsSecurityContext");
    if (mbs_security_context) {
    mbs_security_context_local_nonprim = OpenAPI_mbs_security_context_parseFromJSON(mbs_security_context);
    if (!mbs_security_context_local_nonprim) {
        ogs_error("OpenAPI_mbs_security_context_parseFromJSON failed [mbs_security_context]");
        goto end;
    }
    }

    contact_pcf_ind = cJSON_GetObjectItemCaseSensitive(mbs_session_extensionJSON, "contactPcfInd");
    if (contact_pcf_ind) {
    if (!cJSON_IsBool(contact_pcf_ind)) {
        ogs_error("OpenAPI_mbs_session_extension_parseFromJSON() failed [contact_pcf_ind]");
        goto end;
    }
    }

    area_session_policy_id = cJSON_GetObjectItemCaseSensitive(mbs_session_extensionJSON, "areaSessionPolicyId");
    if (area_session_policy_id) {
    if (!cJSON_IsNumber(area_session_policy_id)) {
        ogs_error("OpenAPI_mbs_session_extension_parseFromJSON() failed [area_session_policy_id]");
        goto end;
    }
    }

    mbs_session_extension_local_var = OpenAPI_mbs_session_extension_create (
        mbs_security_context ? mbs_security_context_local_nonprim : NULL,
        contact_pcf_ind ? true : false,
        contact_pcf_ind ? contact_pcf_ind->valueint : 0,
        area_session_policy_id ? true : false,
        area_session_policy_id ? area_session_policy_id->valuedouble : 0
    );

    return mbs_session_extension_local_var;
end:
    if (mbs_security_context_local_nonprim) {
        OpenAPI_mbs_security_context_free(mbs_security_context_local_nonprim);
        mbs_security_context_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_mbs_session_extension_t *OpenAPI_mbs_session_extension_copy(OpenAPI_mbs_session_extension_t *dst, OpenAPI_mbs_session_extension_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_session_extension_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_session_extension_convertToJSON() failed");
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

    OpenAPI_mbs_session_extension_free(dst);
    dst = OpenAPI_mbs_session_extension_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

