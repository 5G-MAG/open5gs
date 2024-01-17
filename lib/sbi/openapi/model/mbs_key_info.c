
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_key_info.h"

OpenAPI_mbs_key_info_t *OpenAPI_mbs_key_info_create(
    char *key_domain_id,
    char *msk_id,
    char *msk,
    char *msk_lifetime,
    char *mtk_id,
    char *mtk
)
{
    OpenAPI_mbs_key_info_t *mbs_key_info_local_var = ogs_malloc(sizeof(OpenAPI_mbs_key_info_t));
    ogs_assert(mbs_key_info_local_var);

    mbs_key_info_local_var->key_domain_id = key_domain_id;
    mbs_key_info_local_var->msk_id = msk_id;
    mbs_key_info_local_var->msk = msk;
    mbs_key_info_local_var->msk_lifetime = msk_lifetime;
    mbs_key_info_local_var->mtk_id = mtk_id;
    mbs_key_info_local_var->mtk = mtk;

    return mbs_key_info_local_var;
}

void OpenAPI_mbs_key_info_free(OpenAPI_mbs_key_info_t *mbs_key_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_key_info) {
        return;
    }
    if (mbs_key_info->key_domain_id) {
        ogs_free(mbs_key_info->key_domain_id);
        mbs_key_info->key_domain_id = NULL;
    }
    if (mbs_key_info->msk_id) {
        ogs_free(mbs_key_info->msk_id);
        mbs_key_info->msk_id = NULL;
    }
    if (mbs_key_info->msk) {
        ogs_free(mbs_key_info->msk);
        mbs_key_info->msk = NULL;
    }
    if (mbs_key_info->msk_lifetime) {
        ogs_free(mbs_key_info->msk_lifetime);
        mbs_key_info->msk_lifetime = NULL;
    }
    if (mbs_key_info->mtk_id) {
        ogs_free(mbs_key_info->mtk_id);
        mbs_key_info->mtk_id = NULL;
    }
    if (mbs_key_info->mtk) {
        ogs_free(mbs_key_info->mtk);
        mbs_key_info->mtk = NULL;
    }
    ogs_free(mbs_key_info);
}

cJSON *OpenAPI_mbs_key_info_convertToJSON(OpenAPI_mbs_key_info_t *mbs_key_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_key_info == NULL) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed [MbsKeyInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_key_info->key_domain_id) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed [key_domain_id]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "keyDomainId", mbs_key_info->key_domain_id) == NULL) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed [key_domain_id]");
        goto end;
    }

    if (!mbs_key_info->msk_id) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed [msk_id]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "mskId", mbs_key_info->msk_id) == NULL) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed [msk_id]");
        goto end;
    }

    if (mbs_key_info->msk) {
    if (cJSON_AddStringToObject(item, "msk", mbs_key_info->msk) == NULL) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed [msk]");
        goto end;
    }
    }

    if (mbs_key_info->msk_lifetime) {
    if (cJSON_AddStringToObject(item, "mskLifetime", mbs_key_info->msk_lifetime) == NULL) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed [msk_lifetime]");
        goto end;
    }
    }

    if (mbs_key_info->mtk_id) {
    if (cJSON_AddStringToObject(item, "mtkId", mbs_key_info->mtk_id) == NULL) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed [mtk_id]");
        goto end;
    }
    }

    if (mbs_key_info->mtk) {
    if (cJSON_AddStringToObject(item, "mtk", mbs_key_info->mtk) == NULL) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed [mtk]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_key_info_t *OpenAPI_mbs_key_info_parseFromJSON(cJSON *mbs_key_infoJSON)
{
    OpenAPI_mbs_key_info_t *mbs_key_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *key_domain_id = NULL;
    cJSON *msk_id = NULL;
    cJSON *msk = NULL;
    cJSON *msk_lifetime = NULL;
    cJSON *mtk_id = NULL;
    cJSON *mtk = NULL;
    key_domain_id = cJSON_GetObjectItemCaseSensitive(mbs_key_infoJSON, "keyDomainId");
    if (!key_domain_id) {
        ogs_error("OpenAPI_mbs_key_info_parseFromJSON() failed [key_domain_id]");
        goto end;
    }
    if (!cJSON_IsString(key_domain_id)) {
        ogs_error("OpenAPI_mbs_key_info_parseFromJSON() failed [key_domain_id]");
        goto end;
    }

    msk_id = cJSON_GetObjectItemCaseSensitive(mbs_key_infoJSON, "mskId");
    if (!msk_id) {
        ogs_error("OpenAPI_mbs_key_info_parseFromJSON() failed [msk_id]");
        goto end;
    }
    if (!cJSON_IsString(msk_id)) {
        ogs_error("OpenAPI_mbs_key_info_parseFromJSON() failed [msk_id]");
        goto end;
    }

    msk = cJSON_GetObjectItemCaseSensitive(mbs_key_infoJSON, "msk");
    if (msk) {
    if (!cJSON_IsString(msk) && !cJSON_IsNull(msk)) {
        ogs_error("OpenAPI_mbs_key_info_parseFromJSON() failed [msk]");
        goto end;
    }
    }

    msk_lifetime = cJSON_GetObjectItemCaseSensitive(mbs_key_infoJSON, "mskLifetime");
    if (msk_lifetime) {
    if (!cJSON_IsString(msk_lifetime) && !cJSON_IsNull(msk_lifetime)) {
        ogs_error("OpenAPI_mbs_key_info_parseFromJSON() failed [msk_lifetime]");
        goto end;
    }
    }

    mtk_id = cJSON_GetObjectItemCaseSensitive(mbs_key_infoJSON, "mtkId");
    if (mtk_id) {
    if (!cJSON_IsString(mtk_id) && !cJSON_IsNull(mtk_id)) {
        ogs_error("OpenAPI_mbs_key_info_parseFromJSON() failed [mtk_id]");
        goto end;
    }
    }

    mtk = cJSON_GetObjectItemCaseSensitive(mbs_key_infoJSON, "mtk");
    if (mtk) {
    if (!cJSON_IsString(mtk) && !cJSON_IsNull(mtk)) {
        ogs_error("OpenAPI_mbs_key_info_parseFromJSON() failed [mtk]");
        goto end;
    }
    }

    mbs_key_info_local_var = OpenAPI_mbs_key_info_create (
        ogs_strdup(key_domain_id->valuestring),
        ogs_strdup(msk_id->valuestring),
        msk && !cJSON_IsNull(msk) ? ogs_strdup(msk->valuestring) : NULL,
        msk_lifetime && !cJSON_IsNull(msk_lifetime) ? ogs_strdup(msk_lifetime->valuestring) : NULL,
        mtk_id && !cJSON_IsNull(mtk_id) ? ogs_strdup(mtk_id->valuestring) : NULL,
        mtk && !cJSON_IsNull(mtk) ? ogs_strdup(mtk->valuestring) : NULL
    );

    return mbs_key_info_local_var;
end:
    return NULL;
}

OpenAPI_mbs_key_info_t *OpenAPI_mbs_key_info_copy(OpenAPI_mbs_key_info_t *dst, OpenAPI_mbs_key_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_key_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_key_info_convertToJSON() failed");
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

    OpenAPI_mbs_key_info_free(dst);
    dst = OpenAPI_mbs_key_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

