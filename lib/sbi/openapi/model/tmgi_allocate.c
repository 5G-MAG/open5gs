
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tmgi_allocate.h"

OpenAPI_tmgi_allocate_t *OpenAPI_tmgi_allocate_create(
    bool is_tmgi_number,
    int tmgi_number,
    OpenAPI_list_t *tmgi_list
)
{
    OpenAPI_tmgi_allocate_t *tmgi_allocate_local_var = ogs_malloc(sizeof(OpenAPI_tmgi_allocate_t));
    ogs_assert(tmgi_allocate_local_var);

    tmgi_allocate_local_var->is_tmgi_number = is_tmgi_number;
    tmgi_allocate_local_var->tmgi_number = tmgi_number;
    tmgi_allocate_local_var->tmgi_list = tmgi_list;

    return tmgi_allocate_local_var;
}

void OpenAPI_tmgi_allocate_free(OpenAPI_tmgi_allocate_t *tmgi_allocate)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == tmgi_allocate) {
        return;
    }
    if (tmgi_allocate->tmgi_list) {
        OpenAPI_list_for_each(tmgi_allocate->tmgi_list, node) {
            OpenAPI_tmgi_free(node->data);
        }
        OpenAPI_list_free(tmgi_allocate->tmgi_list);
        tmgi_allocate->tmgi_list = NULL;
    }
    ogs_free(tmgi_allocate);
}

cJSON *OpenAPI_tmgi_allocate_convertToJSON(OpenAPI_tmgi_allocate_t *tmgi_allocate)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (tmgi_allocate == NULL) {
        ogs_error("OpenAPI_tmgi_allocate_convertToJSON() failed [TmgiAllocate]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (tmgi_allocate->is_tmgi_number) {
    if (cJSON_AddNumberToObject(item, "tmgiNumber", tmgi_allocate->tmgi_number) == NULL) {
        ogs_error("OpenAPI_tmgi_allocate_convertToJSON() failed [tmgi_number]");
        goto end;
    }
    }

    if (tmgi_allocate->tmgi_list) {
    cJSON *tmgi_listList = cJSON_AddArrayToObject(item, "tmgiList");
    if (tmgi_listList == NULL) {
        ogs_error("OpenAPI_tmgi_allocate_convertToJSON() failed [tmgi_list]");
        goto end;
    }
    OpenAPI_list_for_each(tmgi_allocate->tmgi_list, node) {
        cJSON *itemLocal = OpenAPI_tmgi_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_tmgi_allocate_convertToJSON() failed [tmgi_list]");
            goto end;
        }
        cJSON_AddItemToArray(tmgi_listList, itemLocal);
    }
    }

end:
    return item;
}

OpenAPI_tmgi_allocate_t *OpenAPI_tmgi_allocate_parseFromJSON(cJSON *tmgi_allocateJSON)
{
    OpenAPI_tmgi_allocate_t *tmgi_allocate_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *tmgi_number = NULL;
    cJSON *tmgi_list = NULL;
    OpenAPI_list_t *tmgi_listList = NULL;
    tmgi_number = cJSON_GetObjectItemCaseSensitive(tmgi_allocateJSON, "tmgiNumber");
    if (tmgi_number) {
    if (!cJSON_IsNumber(tmgi_number)) {
        ogs_error("OpenAPI_tmgi_allocate_parseFromJSON() failed [tmgi_number]");
        goto end;
    }
    }

    tmgi_list = cJSON_GetObjectItemCaseSensitive(tmgi_allocateJSON, "tmgiList");
    if (tmgi_list) {
        cJSON *tmgi_list_local = NULL;
        if (!cJSON_IsArray(tmgi_list)) {
            ogs_error("OpenAPI_tmgi_allocate_parseFromJSON() failed [tmgi_list]");
            goto end;
        }

        tmgi_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(tmgi_list_local, tmgi_list) {
            if (!cJSON_IsObject(tmgi_list_local)) {
                ogs_error("OpenAPI_tmgi_allocate_parseFromJSON() failed [tmgi_list]");
                goto end;
            }
            OpenAPI_tmgi_t *tmgi_listItem = OpenAPI_tmgi_parseFromJSON(tmgi_list_local);
            if (!tmgi_listItem) {
                ogs_error("No tmgi_listItem");
                goto end;
            }
            OpenAPI_list_add(tmgi_listList, tmgi_listItem);
        }
    }

    tmgi_allocate_local_var = OpenAPI_tmgi_allocate_create (
        tmgi_number ? true : false,
        tmgi_number ? tmgi_number->valuedouble : 0,
        tmgi_list ? tmgi_listList : NULL
    );

    return tmgi_allocate_local_var;
end:
    if (tmgi_listList) {
        OpenAPI_list_for_each(tmgi_listList, node) {
            OpenAPI_tmgi_free(node->data);
        }
        OpenAPI_list_free(tmgi_listList);
        tmgi_listList = NULL;
    }
    return NULL;
}

OpenAPI_tmgi_allocate_t *OpenAPI_tmgi_allocate_copy(OpenAPI_tmgi_allocate_t *dst, OpenAPI_tmgi_allocate_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_tmgi_allocate_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_tmgi_allocate_convertToJSON() failed");
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

    OpenAPI_tmgi_allocate_free(dst);
    dst = OpenAPI_tmgi_allocate_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

