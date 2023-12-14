
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tmgi_allocated.h"

OpenAPI_tmgi_allocated_t *OpenAPI_tmgi_allocated_create(
    OpenAPI_list_t *tmgi_list,
    char *expiration_time,
    char *nid
)
{
    OpenAPI_tmgi_allocated_t *tmgi_allocated_local_var = ogs_malloc(sizeof(OpenAPI_tmgi_allocated_t));
    ogs_assert(tmgi_allocated_local_var);

    tmgi_allocated_local_var->tmgi_list = tmgi_list;
    tmgi_allocated_local_var->expiration_time = expiration_time;
    tmgi_allocated_local_var->nid = nid;

    return tmgi_allocated_local_var;
}

void OpenAPI_tmgi_allocated_free(OpenAPI_tmgi_allocated_t *tmgi_allocated)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == tmgi_allocated) {
        return;
    }
    if (tmgi_allocated->tmgi_list) {
        OpenAPI_list_for_each(tmgi_allocated->tmgi_list, node) {
            OpenAPI_tmgi_free(node->data);
        }
        OpenAPI_list_free(tmgi_allocated->tmgi_list);
        tmgi_allocated->tmgi_list = NULL;
    }
    if (tmgi_allocated->expiration_time) {
        ogs_free(tmgi_allocated->expiration_time);
        tmgi_allocated->expiration_time = NULL;
    }
    if (tmgi_allocated->nid) {
        ogs_free(tmgi_allocated->nid);
        tmgi_allocated->nid = NULL;
    }
    ogs_free(tmgi_allocated);
}

cJSON *OpenAPI_tmgi_allocated_convertToJSON(OpenAPI_tmgi_allocated_t *tmgi_allocated)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (tmgi_allocated == NULL) {
        ogs_error("OpenAPI_tmgi_allocated_convertToJSON() failed [TmgiAllocated]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!tmgi_allocated->tmgi_list) {
        ogs_error("OpenAPI_tmgi_allocated_convertToJSON() failed [tmgi_list]");
        return NULL;
    }
    cJSON *tmgi_listList = cJSON_AddArrayToObject(item, "tmgiList");
    if (tmgi_listList == NULL) {
        ogs_error("OpenAPI_tmgi_allocated_convertToJSON() failed [tmgi_list]");
        goto end;
    }
    OpenAPI_list_for_each(tmgi_allocated->tmgi_list, node) {
        cJSON *itemLocal = OpenAPI_tmgi_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_tmgi_allocated_convertToJSON() failed [tmgi_list]");
            goto end;
        }
        cJSON_AddItemToArray(tmgi_listList, itemLocal);
    }

    if (!tmgi_allocated->expiration_time) {
        ogs_error("OpenAPI_tmgi_allocated_convertToJSON() failed [expiration_time]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "expirationTime", tmgi_allocated->expiration_time) == NULL) {
        ogs_error("OpenAPI_tmgi_allocated_convertToJSON() failed [expiration_time]");
        goto end;
    }

    if (tmgi_allocated->nid) {
    if (cJSON_AddStringToObject(item, "nid", tmgi_allocated->nid) == NULL) {
        ogs_error("OpenAPI_tmgi_allocated_convertToJSON() failed [nid]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_tmgi_allocated_t *OpenAPI_tmgi_allocated_parseFromJSON(cJSON *tmgi_allocatedJSON)
{
    OpenAPI_tmgi_allocated_t *tmgi_allocated_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *tmgi_list = NULL;
    OpenAPI_list_t *tmgi_listList = NULL;
    cJSON *expiration_time = NULL;
    cJSON *nid = NULL;
    tmgi_list = cJSON_GetObjectItemCaseSensitive(tmgi_allocatedJSON, "tmgiList");
    if (!tmgi_list) {
        ogs_error("OpenAPI_tmgi_allocated_parseFromJSON() failed [tmgi_list]");
        goto end;
    }
        cJSON *tmgi_list_local = NULL;
        if (!cJSON_IsArray(tmgi_list)) {
            ogs_error("OpenAPI_tmgi_allocated_parseFromJSON() failed [tmgi_list]");
            goto end;
        }

        tmgi_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(tmgi_list_local, tmgi_list) {
            if (!cJSON_IsObject(tmgi_list_local)) {
                ogs_error("OpenAPI_tmgi_allocated_parseFromJSON() failed [tmgi_list]");
                goto end;
            }
            OpenAPI_tmgi_t *tmgi_listItem = OpenAPI_tmgi_parseFromJSON(tmgi_list_local);
            if (!tmgi_listItem) {
                ogs_error("No tmgi_listItem");
                goto end;
            }
            OpenAPI_list_add(tmgi_listList, tmgi_listItem);
        }

    expiration_time = cJSON_GetObjectItemCaseSensitive(tmgi_allocatedJSON, "expirationTime");
    if (!expiration_time) {
        ogs_error("OpenAPI_tmgi_allocated_parseFromJSON() failed [expiration_time]");
        goto end;
    }
    if (!cJSON_IsString(expiration_time) && !cJSON_IsNull(expiration_time)) {
        ogs_error("OpenAPI_tmgi_allocated_parseFromJSON() failed [expiration_time]");
        goto end;
    }

    nid = cJSON_GetObjectItemCaseSensitive(tmgi_allocatedJSON, "nid");
    if (nid) {
    if (!cJSON_IsString(nid) && !cJSON_IsNull(nid)) {
        ogs_error("OpenAPI_tmgi_allocated_parseFromJSON() failed [nid]");
        goto end;
    }
    }

    tmgi_allocated_local_var = OpenAPI_tmgi_allocated_create (
        tmgi_listList,
        ogs_strdup(expiration_time->valuestring),
        nid && !cJSON_IsNull(nid) ? ogs_strdup(nid->valuestring) : NULL
    );

    return tmgi_allocated_local_var;
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

OpenAPI_tmgi_allocated_t *OpenAPI_tmgi_allocated_copy(OpenAPI_tmgi_allocated_t *dst, OpenAPI_tmgi_allocated_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_tmgi_allocated_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_tmgi_allocated_convertToJSON() failed");
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

    OpenAPI_tmgi_allocated_free(dst);
    dst = OpenAPI_tmgi_allocated_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

