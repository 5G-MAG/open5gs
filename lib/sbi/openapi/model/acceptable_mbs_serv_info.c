
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "acceptable_mbs_serv_info.h"

OpenAPI_acceptable_mbs_serv_info_t *OpenAPI_acceptable_mbs_serv_info_create(
    OpenAPI_list_t* acc_mbs_serv_info,
    char *acc_max_mbs_bw
)
{
    OpenAPI_acceptable_mbs_serv_info_t *acceptable_mbs_serv_info_local_var = ogs_malloc(sizeof(OpenAPI_acceptable_mbs_serv_info_t));
    ogs_assert(acceptable_mbs_serv_info_local_var);

    acceptable_mbs_serv_info_local_var->acc_mbs_serv_info = acc_mbs_serv_info;
    acceptable_mbs_serv_info_local_var->acc_max_mbs_bw = acc_max_mbs_bw;

    return acceptable_mbs_serv_info_local_var;
}

void OpenAPI_acceptable_mbs_serv_info_free(OpenAPI_acceptable_mbs_serv_info_t *acceptable_mbs_serv_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == acceptable_mbs_serv_info) {
        return;
    }
    if (acceptable_mbs_serv_info->acc_mbs_serv_info) {
        OpenAPI_list_for_each(acceptable_mbs_serv_info->acc_mbs_serv_info, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_media_comp_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(acceptable_mbs_serv_info->acc_mbs_serv_info);
        acceptable_mbs_serv_info->acc_mbs_serv_info = NULL;
    }
    if (acceptable_mbs_serv_info->acc_max_mbs_bw) {
        ogs_free(acceptable_mbs_serv_info->acc_max_mbs_bw);
        acceptable_mbs_serv_info->acc_max_mbs_bw = NULL;
    }
    ogs_free(acceptable_mbs_serv_info);
}

cJSON *OpenAPI_acceptable_mbs_serv_info_convertToJSON(OpenAPI_acceptable_mbs_serv_info_t *acceptable_mbs_serv_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (acceptable_mbs_serv_info == NULL) {
        ogs_error("OpenAPI_acceptable_mbs_serv_info_convertToJSON() failed [AcceptableMbsServInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (acceptable_mbs_serv_info->acc_mbs_serv_info) {
    cJSON *acc_mbs_serv_info = cJSON_AddObjectToObject(item, "accMbsServInfo");
    if (acc_mbs_serv_info == NULL) {
        ogs_error("OpenAPI_acceptable_mbs_serv_info_convertToJSON() failed [acc_mbs_serv_info]");
        goto end;
    }
    cJSON *localMapObject = acc_mbs_serv_info;
    if (acceptable_mbs_serv_info->acc_mbs_serv_info) {
        OpenAPI_list_for_each(acceptable_mbs_serv_info->acc_mbs_serv_info, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            if (localKeyValue == NULL) {
                ogs_error("OpenAPI_acceptable_mbs_serv_info_convertToJSON() failed [acc_mbs_serv_info]");
                goto end;
            }
            if (localKeyValue->key == NULL) {
                ogs_error("OpenAPI_acceptable_mbs_serv_info_convertToJSON() failed [acc_mbs_serv_info]");
                goto end;
            }
            cJSON *itemLocal = localKeyValue->value ?
                OpenAPI_mbs_media_comp_convertToJSON(localKeyValue->value) :
                cJSON_CreateNull();
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_acceptable_mbs_serv_info_convertToJSON() failed [inner]");
                goto end;
            }
            cJSON_AddItemToObject(localMapObject, localKeyValue->key, itemLocal);
        }
    }
    }

    if (acceptable_mbs_serv_info->acc_max_mbs_bw) {
    if (cJSON_AddStringToObject(item, "accMaxMbsBw", acceptable_mbs_serv_info->acc_max_mbs_bw) == NULL) {
        ogs_error("OpenAPI_acceptable_mbs_serv_info_convertToJSON() failed [acc_max_mbs_bw]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_acceptable_mbs_serv_info_t *OpenAPI_acceptable_mbs_serv_info_parseFromJSON(cJSON *acceptable_mbs_serv_infoJSON)
{
    OpenAPI_acceptable_mbs_serv_info_t *acceptable_mbs_serv_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *acc_mbs_serv_info = NULL;
    OpenAPI_list_t *acc_mbs_serv_infoList = NULL;
    cJSON *acc_max_mbs_bw = NULL;
    acc_mbs_serv_info = cJSON_GetObjectItemCaseSensitive(acceptable_mbs_serv_infoJSON, "accMbsServInfo");
    if (acc_mbs_serv_info) {
        cJSON *acc_mbs_serv_info_local_map = NULL;
        if (!cJSON_IsObject(acc_mbs_serv_info) && !cJSON_IsNull(acc_mbs_serv_info)) {
            ogs_error("OpenAPI_acceptable_mbs_serv_info_parseFromJSON() failed [acc_mbs_serv_info]");
            goto end;
        }
        if (cJSON_IsObject(acc_mbs_serv_info)) {
            acc_mbs_serv_infoList = OpenAPI_list_create();
            OpenAPI_map_t *localMapKeyPair = NULL;
            cJSON_ArrayForEach(acc_mbs_serv_info_local_map, acc_mbs_serv_info) {
                cJSON *localMapObject = acc_mbs_serv_info_local_map;
                if (cJSON_IsObject(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(
                        ogs_strdup(localMapObject->string), OpenAPI_mbs_media_comp_parseFromJSON(localMapObject));
                } else if (cJSON_IsNull(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(ogs_strdup(localMapObject->string), NULL);
                } else {
                    ogs_error("OpenAPI_acceptable_mbs_serv_info_parseFromJSON() failed [inner]");
                    goto end;
                }
                OpenAPI_list_add(acc_mbs_serv_infoList, localMapKeyPair);
            }
        }
    }

    acc_max_mbs_bw = cJSON_GetObjectItemCaseSensitive(acceptable_mbs_serv_infoJSON, "accMaxMbsBw");
    if (acc_max_mbs_bw) {
    if (!cJSON_IsString(acc_max_mbs_bw) && !cJSON_IsNull(acc_max_mbs_bw)) {
        ogs_error("OpenAPI_acceptable_mbs_serv_info_parseFromJSON() failed [acc_max_mbs_bw]");
        goto end;
    }
    }

    acceptable_mbs_serv_info_local_var = OpenAPI_acceptable_mbs_serv_info_create (
        acc_mbs_serv_info ? acc_mbs_serv_infoList : NULL,
        acc_max_mbs_bw && !cJSON_IsNull(acc_max_mbs_bw) ? ogs_strdup(acc_max_mbs_bw->valuestring) : NULL
    );

    return acceptable_mbs_serv_info_local_var;
end:
    if (acc_mbs_serv_infoList) {
        OpenAPI_list_for_each(acc_mbs_serv_infoList, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*) node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_media_comp_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(acc_mbs_serv_infoList);
        acc_mbs_serv_infoList = NULL;
    }
    return NULL;
}

OpenAPI_acceptable_mbs_serv_info_t *OpenAPI_acceptable_mbs_serv_info_copy(OpenAPI_acceptable_mbs_serv_info_t *dst, OpenAPI_acceptable_mbs_serv_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_acceptable_mbs_serv_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_acceptable_mbs_serv_info_convertToJSON() failed");
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

    OpenAPI_acceptable_mbs_serv_info_free(dst);
    dst = OpenAPI_acceptable_mbs_serv_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

