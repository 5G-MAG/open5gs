
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_media_info.h"

OpenAPI_mbs_media_info_t *OpenAPI_mbs_media_info_create(
    OpenAPI_media_type_e mbs_med_type,
    char *max_req_mbs_bw_dl,
    char *min_req_mbs_bw_dl,
    OpenAPI_list_t *codecs
)
{
    OpenAPI_mbs_media_info_t *mbs_media_info_local_var = ogs_malloc(sizeof(OpenAPI_mbs_media_info_t));
    ogs_assert(mbs_media_info_local_var);

    mbs_media_info_local_var->mbs_med_type = mbs_med_type;
    mbs_media_info_local_var->max_req_mbs_bw_dl = max_req_mbs_bw_dl;
    mbs_media_info_local_var->min_req_mbs_bw_dl = min_req_mbs_bw_dl;
    mbs_media_info_local_var->codecs = codecs;

    return mbs_media_info_local_var;
}

void OpenAPI_mbs_media_info_free(OpenAPI_mbs_media_info_t *mbs_media_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_media_info) {
        return;
    }
    if (mbs_media_info->max_req_mbs_bw_dl) {
        ogs_free(mbs_media_info->max_req_mbs_bw_dl);
        mbs_media_info->max_req_mbs_bw_dl = NULL;
    }
    if (mbs_media_info->min_req_mbs_bw_dl) {
        ogs_free(mbs_media_info->min_req_mbs_bw_dl);
        mbs_media_info->min_req_mbs_bw_dl = NULL;
    }
    if (mbs_media_info->codecs) {
        OpenAPI_list_for_each(mbs_media_info->codecs, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_media_info->codecs);
        mbs_media_info->codecs = NULL;
    }
    ogs_free(mbs_media_info);
}

cJSON *OpenAPI_mbs_media_info_convertToJSON(OpenAPI_mbs_media_info_t *mbs_media_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_media_info == NULL) {
        ogs_error("OpenAPI_mbs_media_info_convertToJSON() failed [MbsMediaInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_media_info->mbs_med_type != OpenAPI_media_type_NULL) {
    if (cJSON_AddStringToObject(item, "mbsMedType", OpenAPI_media_type_ToString(mbs_media_info->mbs_med_type)) == NULL) {
        ogs_error("OpenAPI_mbs_media_info_convertToJSON() failed [mbs_med_type]");
        goto end;
    }
    }

    if (mbs_media_info->max_req_mbs_bw_dl) {
    if (cJSON_AddStringToObject(item, "maxReqMbsBwDl", mbs_media_info->max_req_mbs_bw_dl) == NULL) {
        ogs_error("OpenAPI_mbs_media_info_convertToJSON() failed [max_req_mbs_bw_dl]");
        goto end;
    }
    }

    if (mbs_media_info->min_req_mbs_bw_dl) {
    if (cJSON_AddStringToObject(item, "minReqMbsBwDl", mbs_media_info->min_req_mbs_bw_dl) == NULL) {
        ogs_error("OpenAPI_mbs_media_info_convertToJSON() failed [min_req_mbs_bw_dl]");
        goto end;
    }
    }

    if (mbs_media_info->codecs) {
    cJSON *codecsList = cJSON_AddArrayToObject(item, "codecs");
    if (codecsList == NULL) {
        ogs_error("OpenAPI_mbs_media_info_convertToJSON() failed [codecs]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_media_info->codecs, node) {
        if (cJSON_AddStringToObject(codecsList, "", (char*)node->data) == NULL) {
            ogs_error("OpenAPI_mbs_media_info_convertToJSON() failed [codecs]");
            goto end;
        }
    }
    }

end:
    return item;
}

OpenAPI_mbs_media_info_t *OpenAPI_mbs_media_info_parseFromJSON(cJSON *mbs_media_infoJSON)
{
    OpenAPI_mbs_media_info_t *mbs_media_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_med_type = NULL;
    OpenAPI_media_type_e mbs_med_typeVariable = 0;
    cJSON *max_req_mbs_bw_dl = NULL;
    cJSON *min_req_mbs_bw_dl = NULL;
    cJSON *codecs = NULL;
    OpenAPI_list_t *codecsList = NULL;
    mbs_med_type = cJSON_GetObjectItemCaseSensitive(mbs_media_infoJSON, "mbsMedType");
    if (mbs_med_type) {
    if (!cJSON_IsString(mbs_med_type)) {
        ogs_error("OpenAPI_mbs_media_info_parseFromJSON() failed [mbs_med_type]");
        goto end;
    }
    mbs_med_typeVariable = OpenAPI_media_type_FromString(mbs_med_type->valuestring);
    }

    max_req_mbs_bw_dl = cJSON_GetObjectItemCaseSensitive(mbs_media_infoJSON, "maxReqMbsBwDl");
    if (max_req_mbs_bw_dl) {
    if (!cJSON_IsString(max_req_mbs_bw_dl) && !cJSON_IsNull(max_req_mbs_bw_dl)) {
        ogs_error("OpenAPI_mbs_media_info_parseFromJSON() failed [max_req_mbs_bw_dl]");
        goto end;
    }
    }

    min_req_mbs_bw_dl = cJSON_GetObjectItemCaseSensitive(mbs_media_infoJSON, "minReqMbsBwDl");
    if (min_req_mbs_bw_dl) {
    if (!cJSON_IsString(min_req_mbs_bw_dl) && !cJSON_IsNull(min_req_mbs_bw_dl)) {
        ogs_error("OpenAPI_mbs_media_info_parseFromJSON() failed [min_req_mbs_bw_dl]");
        goto end;
    }
    }

    codecs = cJSON_GetObjectItemCaseSensitive(mbs_media_infoJSON, "codecs");
    if (codecs) {
        cJSON *codecs_local = NULL;
        if (!cJSON_IsArray(codecs)) {
            ogs_error("OpenAPI_mbs_media_info_parseFromJSON() failed [codecs]");
            goto end;
        }

        codecsList = OpenAPI_list_create();

        cJSON_ArrayForEach(codecs_local, codecs) {
            double *localDouble = NULL;
            int *localInt = NULL;
            if (!cJSON_IsString(codecs_local)) {
                ogs_error("OpenAPI_mbs_media_info_parseFromJSON() failed [codecs]");
                goto end;
            }
            OpenAPI_list_add(codecsList, ogs_strdup(codecs_local->valuestring));
        }
    }

    mbs_media_info_local_var = OpenAPI_mbs_media_info_create (
        mbs_med_type ? mbs_med_typeVariable : 0,
        max_req_mbs_bw_dl && !cJSON_IsNull(max_req_mbs_bw_dl) ? ogs_strdup(max_req_mbs_bw_dl->valuestring) : NULL,
        min_req_mbs_bw_dl && !cJSON_IsNull(min_req_mbs_bw_dl) ? ogs_strdup(min_req_mbs_bw_dl->valuestring) : NULL,
        codecs ? codecsList : NULL
    );

    return mbs_media_info_local_var;
end:
    if (codecsList) {
        OpenAPI_list_for_each(codecsList, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(codecsList);
        codecsList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_media_info_t *OpenAPI_mbs_media_info_copy(OpenAPI_mbs_media_info_t *dst, OpenAPI_mbs_media_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_media_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_media_info_convertToJSON() failed");
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

    OpenAPI_mbs_media_info_free(dst);
    dst = OpenAPI_mbs_media_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

