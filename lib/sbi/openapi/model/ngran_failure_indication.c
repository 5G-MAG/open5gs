
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ngran_failure_indication.h"

OpenAPI_ngran_failure_indication_t *OpenAPI_ngran_failure_indication_create(
)
{
    OpenAPI_ngran_failure_indication_t *ngran_failure_indication_local_var = ogs_malloc(sizeof(OpenAPI_ngran_failure_indication_t));
    ogs_assert(ngran_failure_indication_local_var);


    return ngran_failure_indication_local_var;
}

void OpenAPI_ngran_failure_indication_free(OpenAPI_ngran_failure_indication_t *ngran_failure_indication)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == ngran_failure_indication) {
        return;
    }
    ogs_free(ngran_failure_indication);
}

cJSON *OpenAPI_ngran_failure_indication_convertToJSON(OpenAPI_ngran_failure_indication_t *ngran_failure_indication)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (ngran_failure_indication == NULL) {
        ogs_error("OpenAPI_ngran_failure_indication_convertToJSON() failed [NgranFailureIndication]");
        return NULL;
    }

    item = cJSON_CreateObject();
end:
    return item;
}

OpenAPI_ngran_failure_indication_t *OpenAPI_ngran_failure_indication_parseFromJSON(cJSON *ngran_failure_indicationJSON)
{
    OpenAPI_ngran_failure_indication_t *ngran_failure_indication_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    ngran_failure_indication_local_var = OpenAPI_ngran_failure_indication_create (
    );

    return ngran_failure_indication_local_var;
end:
    return NULL;
}

OpenAPI_ngran_failure_indication_t *OpenAPI_ngran_failure_indication_copy(OpenAPI_ngran_failure_indication_t *dst, OpenAPI_ngran_failure_indication_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_ngran_failure_indication_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_ngran_failure_indication_convertToJSON() failed");
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

    OpenAPI_ngran_failure_indication_free(dst);
    dst = OpenAPI_ngran_failure_indication_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

