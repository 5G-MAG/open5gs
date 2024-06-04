
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "operation_status.h"

OpenAPI_operation_status_t *OpenAPI_operation_status_create(
)
{
    OpenAPI_operation_status_t *operation_status_local_var = ogs_malloc(sizeof(OpenAPI_operation_status_t));
    ogs_assert(operation_status_local_var);


    return operation_status_local_var;
}

void OpenAPI_operation_status_free(OpenAPI_operation_status_t *operation_status)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == operation_status) {
        return;
    }
    ogs_free(operation_status);
}

cJSON *OpenAPI_operation_status_convertToJSON(OpenAPI_operation_status_t *operation_status)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (operation_status == NULL) {
        ogs_error("OpenAPI_operation_status_convertToJSON() failed [OperationStatus]");
        return NULL;
    }

    item = cJSON_CreateObject();
end:
    return item;
}

OpenAPI_operation_status_t *OpenAPI_operation_status_parseFromJSON(cJSON *operation_statusJSON)
{
    OpenAPI_operation_status_t *operation_status_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    operation_status_local_var = OpenAPI_operation_status_create (
    );

    return operation_status_local_var;
end:
    return NULL;
}

OpenAPI_operation_status_t *OpenAPI_operation_status_copy(OpenAPI_operation_status_t *dst, OpenAPI_operation_status_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_operation_status_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_operation_status_convertToJSON() failed");
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

    OpenAPI_operation_status_free(dst);
    dst = OpenAPI_operation_status_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

