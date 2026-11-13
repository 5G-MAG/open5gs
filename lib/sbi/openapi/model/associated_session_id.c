
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "associated_session_id.h"

OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_create(
    OpenAPI_ip_addr_t *source_ip_addr,
    OpenAPI_ip_addr_t *dest_ip_addr
)
{
    OpenAPI_associated_session_id_t *associated_session_id_local_var = ogs_malloc(sizeof(OpenAPI_associated_session_id_t));
    ogs_assert(associated_session_id_local_var);

    associated_session_id_local_var->source_ip_addr = source_ip_addr;
    associated_session_id_local_var->dest_ip_addr = dest_ip_addr;

    return associated_session_id_local_var;
}

void OpenAPI_associated_session_id_free(OpenAPI_associated_session_id_t *associated_session_id)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == associated_session_id) {
        return;
    }
    if (associated_session_id->source_ip_addr) {
        OpenAPI_ip_addr_free(associated_session_id->source_ip_addr);
        associated_session_id->source_ip_addr = NULL;
    }
    if (associated_session_id->dest_ip_addr) {
        OpenAPI_ip_addr_free(associated_session_id->dest_ip_addr);
        associated_session_id->dest_ip_addr = NULL;
    }
    ogs_free(associated_session_id);
}

cJSON *OpenAPI_associated_session_id_convertToJSON(OpenAPI_associated_session_id_t *associated_session_id)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (associated_session_id == NULL) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed [AssociatedSessionId]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!associated_session_id->source_ip_addr) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed [source_ip_addr]");
        return NULL;
    }
    cJSON *source_ip_addr_local_JSON = OpenAPI_ip_addr_convertToJSON(associated_session_id->source_ip_addr);
    if (source_ip_addr_local_JSON == NULL) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed [source_ip_addr]");
        goto end;
    }
    cJSON_AddItemToObject(item, "sourceIpAddr", source_ip_addr_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed [source_ip_addr]");
        goto end;
    }

    if (!associated_session_id->dest_ip_addr) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed [dest_ip_addr]");
        return NULL;
    }
    cJSON *dest_ip_addr_local_JSON = OpenAPI_ip_addr_convertToJSON(associated_session_id->dest_ip_addr);
    if (dest_ip_addr_local_JSON == NULL) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed [dest_ip_addr]");
        goto end;
    }
    cJSON_AddItemToObject(item, "destIpAddr", dest_ip_addr_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed [dest_ip_addr]");
        goto end;
    }

end:
    return item;
}

OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_parseFromJSON(cJSON *associated_session_idJSON)
{
    OpenAPI_associated_session_id_t *associated_session_id_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *source_ip_addr = NULL;
    OpenAPI_ip_addr_t *source_ip_addr_local_nonprim = NULL;
    cJSON *dest_ip_addr = NULL;
    OpenAPI_ip_addr_t *dest_ip_addr_local_nonprim = NULL;
    source_ip_addr = cJSON_GetObjectItemCaseSensitive(associated_session_idJSON, "sourceIpAddr");
    if (!source_ip_addr) {
        ogs_error("OpenAPI_associated_session_id_parseFromJSON() failed [source_ip_addr]");
        goto end;
    }
    source_ip_addr_local_nonprim = OpenAPI_ip_addr_parseFromJSON(source_ip_addr);
    if (!source_ip_addr_local_nonprim) {
        ogs_error("OpenAPI_ip_addr_parseFromJSON failed [source_ip_addr]");
        goto end;
    }

    dest_ip_addr = cJSON_GetObjectItemCaseSensitive(associated_session_idJSON, "destIpAddr");
    if (!dest_ip_addr) {
        ogs_error("OpenAPI_associated_session_id_parseFromJSON() failed [dest_ip_addr]");
        goto end;
    }
    dest_ip_addr_local_nonprim = OpenAPI_ip_addr_parseFromJSON(dest_ip_addr);
    if (!dest_ip_addr_local_nonprim) {
        ogs_error("OpenAPI_ip_addr_parseFromJSON failed [dest_ip_addr]");
        goto end;
    }

    associated_session_id_local_var = OpenAPI_associated_session_id_create (
        source_ip_addr_local_nonprim,
        dest_ip_addr_local_nonprim
    );

    return associated_session_id_local_var;
end:
    if (source_ip_addr_local_nonprim) {
        OpenAPI_ip_addr_free(source_ip_addr_local_nonprim);
        source_ip_addr_local_nonprim = NULL;
    }
    if (dest_ip_addr_local_nonprim) {
        OpenAPI_ip_addr_free(dest_ip_addr_local_nonprim);
        dest_ip_addr_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_copy(OpenAPI_associated_session_id_t *dst, OpenAPI_associated_session_id_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_associated_session_id_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed");
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

    OpenAPI_associated_session_id_free(dst);
    dst = OpenAPI_associated_session_id_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

