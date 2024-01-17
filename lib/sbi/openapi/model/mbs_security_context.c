
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_security_context.h"

OpenAPI_mbs_security_context_t *OpenAPI_mbs_security_context_create(
    OpenAPI_list_t* key_list
)
{
    OpenAPI_mbs_security_context_t *mbs_security_context_local_var = ogs_malloc(sizeof(OpenAPI_mbs_security_context_t));
    ogs_assert(mbs_security_context_local_var);

    mbs_security_context_local_var->key_list = key_list;

    return mbs_security_context_local_var;
}

void OpenAPI_mbs_security_context_free(OpenAPI_mbs_security_context_t *mbs_security_context)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_security_context) {
        return;
    }
    if (mbs_security_context->key_list) {
        OpenAPI_list_for_each(mbs_security_context->key_list, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_key_info_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_security_context->key_list);
        mbs_security_context->key_list = NULL;
    }
    ogs_free(mbs_security_context);
}

cJSON *OpenAPI_mbs_security_context_convertToJSON(OpenAPI_mbs_security_context_t *mbs_security_context)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_security_context == NULL) {
        ogs_error("OpenAPI_mbs_security_context_convertToJSON() failed [MbsSecurityContext]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_security_context->key_list) {
        ogs_error("OpenAPI_mbs_security_context_convertToJSON() failed [key_list]");
        return NULL;
    }
    cJSON *key_list = cJSON_AddObjectToObject(item, "keyList");
    if (key_list == NULL) {
        ogs_error("OpenAPI_mbs_security_context_convertToJSON() failed [key_list]");
        goto end;
    }
    cJSON *localMapObject = key_list;
    if (mbs_security_context->key_list) {
        OpenAPI_list_for_each(mbs_security_context->key_list, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            if (localKeyValue == NULL) {
                ogs_error("OpenAPI_mbs_security_context_convertToJSON() failed [key_list]");
                goto end;
            }
            if (localKeyValue->key == NULL) {
                ogs_error("OpenAPI_mbs_security_context_convertToJSON() failed [key_list]");
                goto end;
            }
            cJSON *itemLocal = localKeyValue->value ?
                OpenAPI_mbs_key_info_convertToJSON(localKeyValue->value) :
                cJSON_CreateNull();
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_mbs_security_context_convertToJSON() failed [inner]");
                goto end;
            }
            cJSON_AddItemToObject(localMapObject, localKeyValue->key, itemLocal);
        }
    }

end:
    return item;
}

OpenAPI_mbs_security_context_t *OpenAPI_mbs_security_context_parseFromJSON(cJSON *mbs_security_contextJSON)
{
    OpenAPI_mbs_security_context_t *mbs_security_context_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *key_list = NULL;
    OpenAPI_list_t *key_listList = NULL;
    key_list = cJSON_GetObjectItemCaseSensitive(mbs_security_contextJSON, "keyList");
    if (!key_list) {
        ogs_error("OpenAPI_mbs_security_context_parseFromJSON() failed [key_list]");
        goto end;
    }
        cJSON *key_list_local_map = NULL;
        if (!cJSON_IsObject(key_list) && !cJSON_IsNull(key_list)) {
            ogs_error("OpenAPI_mbs_security_context_parseFromJSON() failed [key_list]");
            goto end;
        }
        if (cJSON_IsObject(key_list)) {
            key_listList = OpenAPI_list_create();
            OpenAPI_map_t *localMapKeyPair = NULL;
            cJSON_ArrayForEach(key_list_local_map, key_list) {
                cJSON *localMapObject = key_list_local_map;
                if (cJSON_IsObject(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(
                        ogs_strdup(localMapObject->string), OpenAPI_mbs_key_info_parseFromJSON(localMapObject));
                } else if (cJSON_IsNull(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(ogs_strdup(localMapObject->string), NULL);
                } else {
                    ogs_error("OpenAPI_mbs_security_context_parseFromJSON() failed [inner]");
                    goto end;
                }
                OpenAPI_list_add(key_listList, localMapKeyPair);
            }
        }

    mbs_security_context_local_var = OpenAPI_mbs_security_context_create (
        key_listList
    );

    return mbs_security_context_local_var;
end:
    if (key_listList) {
        OpenAPI_list_for_each(key_listList, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*) node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_key_info_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(key_listList);
        key_listList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_security_context_t *OpenAPI_mbs_security_context_copy(OpenAPI_mbs_security_context_t *dst, OpenAPI_mbs_security_context_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_security_context_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_security_context_convertToJSON() failed");
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

    OpenAPI_mbs_security_context_free(dst);
    dst = OpenAPI_mbs_security_context_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

