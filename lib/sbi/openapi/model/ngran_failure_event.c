
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ngran_failure_event.h"

OpenAPI_ngran_failure_event_t *OpenAPI_ngran_failure_event_create(
    OpenAPI_global_ran_node_id_t *ngran_id,
    OpenAPI_ngran_failure_indication_t *ngran_failure_indication
)
{
    OpenAPI_ngran_failure_event_t *ngran_failure_event_local_var = ogs_malloc(sizeof(OpenAPI_ngran_failure_event_t));
    ogs_assert(ngran_failure_event_local_var);

    ngran_failure_event_local_var->ngran_id = ngran_id;
    ngran_failure_event_local_var->ngran_failure_indication = ngran_failure_indication;

    return ngran_failure_event_local_var;
}

void OpenAPI_ngran_failure_event_free(OpenAPI_ngran_failure_event_t *ngran_failure_event)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == ngran_failure_event) {
        return;
    }
    if (ngran_failure_event->ngran_id) {
        OpenAPI_global_ran_node_id_free(ngran_failure_event->ngran_id);
        ngran_failure_event->ngran_id = NULL;
    }
    if (ngran_failure_event->ngran_failure_indication) {
        OpenAPI_ngran_failure_indication_free(ngran_failure_event->ngran_failure_indication);
        ngran_failure_event->ngran_failure_indication = NULL;
    }
    ogs_free(ngran_failure_event);
}

cJSON *OpenAPI_ngran_failure_event_convertToJSON(OpenAPI_ngran_failure_event_t *ngran_failure_event)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (ngran_failure_event == NULL) {
        ogs_error("OpenAPI_ngran_failure_event_convertToJSON() failed [NgranFailureEvent]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!ngran_failure_event->ngran_id) {
        ogs_error("OpenAPI_ngran_failure_event_convertToJSON() failed [ngran_id]");
        return NULL;
    }
    cJSON *ngran_id_local_JSON = OpenAPI_global_ran_node_id_convertToJSON(ngran_failure_event->ngran_id);
    if (ngran_id_local_JSON == NULL) {
        ogs_error("OpenAPI_ngran_failure_event_convertToJSON() failed [ngran_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "ngranId", ngran_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ngran_failure_event_convertToJSON() failed [ngran_id]");
        goto end;
    }

    if (!ngran_failure_event->ngran_failure_indication) {
        ogs_error("OpenAPI_ngran_failure_event_convertToJSON() failed [ngran_failure_indication]");
        return NULL;
    }
    cJSON *ngran_failure_indication_local_JSON = OpenAPI_ngran_failure_indication_convertToJSON(ngran_failure_event->ngran_failure_indication);
    if (ngran_failure_indication_local_JSON == NULL) {
        ogs_error("OpenAPI_ngran_failure_event_convertToJSON() failed [ngran_failure_indication]");
        goto end;
    }
    cJSON_AddItemToObject(item, "ngranFailureIndication", ngran_failure_indication_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ngran_failure_event_convertToJSON() failed [ngran_failure_indication]");
        goto end;
    }

end:
    return item;
}

OpenAPI_ngran_failure_event_t *OpenAPI_ngran_failure_event_parseFromJSON(cJSON *ngran_failure_eventJSON)
{
    OpenAPI_ngran_failure_event_t *ngran_failure_event_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *ngran_id = NULL;
    OpenAPI_global_ran_node_id_t *ngran_id_local_nonprim = NULL;
    cJSON *ngran_failure_indication = NULL;
    OpenAPI_ngran_failure_indication_t *ngran_failure_indication_local_nonprim = NULL;
    ngran_id = cJSON_GetObjectItemCaseSensitive(ngran_failure_eventJSON, "ngranId");
    if (!ngran_id) {
        ogs_error("OpenAPI_ngran_failure_event_parseFromJSON() failed [ngran_id]");
        goto end;
    }
    ngran_id_local_nonprim = OpenAPI_global_ran_node_id_parseFromJSON(ngran_id);
    if (!ngran_id_local_nonprim) {
        ogs_error("OpenAPI_global_ran_node_id_parseFromJSON failed [ngran_id]");
        goto end;
    }

    ngran_failure_indication = cJSON_GetObjectItemCaseSensitive(ngran_failure_eventJSON, "ngranFailureIndication");
    if (!ngran_failure_indication) {
        ogs_error("OpenAPI_ngran_failure_event_parseFromJSON() failed [ngran_failure_indication]");
        goto end;
    }
    ngran_failure_indication_local_nonprim = OpenAPI_ngran_failure_indication_parseFromJSON(ngran_failure_indication);
    if (!ngran_failure_indication_local_nonprim) {
        ogs_error("OpenAPI_ngran_failure_indication_parseFromJSON failed [ngran_failure_indication]");
        goto end;
    }

    ngran_failure_event_local_var = OpenAPI_ngran_failure_event_create (
        ngran_id_local_nonprim,
        ngran_failure_indication_local_nonprim
    );

    return ngran_failure_event_local_var;
end:
    if (ngran_id_local_nonprim) {
        OpenAPI_global_ran_node_id_free(ngran_id_local_nonprim);
        ngran_id_local_nonprim = NULL;
    }
    if (ngran_failure_indication_local_nonprim) {
        OpenAPI_ngran_failure_indication_free(ngran_failure_indication_local_nonprim);
        ngran_failure_indication_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_ngran_failure_event_t *OpenAPI_ngran_failure_event_copy(OpenAPI_ngran_failure_event_t *dst, OpenAPI_ngran_failure_event_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_ngran_failure_event_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_ngran_failure_event_convertToJSON() failed");
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

    OpenAPI_ngran_failure_event_free(dst);
    dst = OpenAPI_ngran_failure_event_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

