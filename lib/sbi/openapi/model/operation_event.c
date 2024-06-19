
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "operation_event.h"

OpenAPI_operation_event_t *OpenAPI_operation_event_create(
    OpenAPI_op_event_type_e op_event_type,
    char *amf_id,
    OpenAPI_list_t *ngran_failure_event_list
)
{
    OpenAPI_operation_event_t *operation_event_local_var = ogs_malloc(sizeof(OpenAPI_operation_event_t));
    ogs_assert(operation_event_local_var);

    operation_event_local_var->op_event_type = op_event_type;
    operation_event_local_var->amf_id = amf_id;
    operation_event_local_var->ngran_failure_event_list = ngran_failure_event_list;

    return operation_event_local_var;
}

void OpenAPI_operation_event_free(OpenAPI_operation_event_t *operation_event)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == operation_event) {
        return;
    }
    if (operation_event->amf_id) {
        ogs_free(operation_event->amf_id);
        operation_event->amf_id = NULL;
    }
    if (operation_event->ngran_failure_event_list) {
        OpenAPI_list_for_each(operation_event->ngran_failure_event_list, node) {
            OpenAPI_ngran_failure_event_free(node->data);
        }
        OpenAPI_list_free(operation_event->ngran_failure_event_list);
        operation_event->ngran_failure_event_list = NULL;
    }
    ogs_free(operation_event);
}

cJSON *OpenAPI_operation_event_convertToJSON(OpenAPI_operation_event_t *operation_event)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (operation_event == NULL) {
        ogs_error("OpenAPI_operation_event_convertToJSON() failed [OperationEvent]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (operation_event->op_event_type == OpenAPI_op_event_type_NULL) {
        ogs_error("OpenAPI_operation_event_convertToJSON() failed [op_event_type]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "opEventType", OpenAPI_op_event_type_ToString(operation_event->op_event_type)) == NULL) {
        ogs_error("OpenAPI_operation_event_convertToJSON() failed [op_event_type]");
        goto end;
    }

    if (operation_event->amf_id) {
    if (cJSON_AddStringToObject(item, "amfId", operation_event->amf_id) == NULL) {
        ogs_error("OpenAPI_operation_event_convertToJSON() failed [amf_id]");
        goto end;
    }
    }

    if (operation_event->ngran_failure_event_list) {
    cJSON *ngran_failure_event_listList = cJSON_AddArrayToObject(item, "ngranFailureEventList");
    if (ngran_failure_event_listList == NULL) {
        ogs_error("OpenAPI_operation_event_convertToJSON() failed [ngran_failure_event_list]");
        goto end;
    }
    OpenAPI_list_for_each(operation_event->ngran_failure_event_list, node) {
        cJSON *itemLocal = OpenAPI_ngran_failure_event_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_operation_event_convertToJSON() failed [ngran_failure_event_list]");
            goto end;
        }
        cJSON_AddItemToArray(ngran_failure_event_listList, itemLocal);
    }
    }

end:
    return item;
}

OpenAPI_operation_event_t *OpenAPI_operation_event_parseFromJSON(cJSON *operation_eventJSON)
{
    OpenAPI_operation_event_t *operation_event_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *op_event_type = NULL;
    OpenAPI_op_event_type_e op_event_typeVariable = 0;
    cJSON *amf_id = NULL;
    cJSON *ngran_failure_event_list = NULL;
    OpenAPI_list_t *ngran_failure_event_listList = NULL;
    op_event_type = cJSON_GetObjectItemCaseSensitive(operation_eventJSON, "opEventType");
    if (!op_event_type) {
        ogs_error("OpenAPI_operation_event_parseFromJSON() failed [op_event_type]");
        goto end;
    }
    if (!cJSON_IsString(op_event_type)) {
        ogs_error("OpenAPI_operation_event_parseFromJSON() failed [op_event_type]");
        goto end;
    }
    op_event_typeVariable = OpenAPI_op_event_type_FromString(op_event_type->valuestring);

    amf_id = cJSON_GetObjectItemCaseSensitive(operation_eventJSON, "amfId");
    if (amf_id) {
    if (!cJSON_IsString(amf_id) && !cJSON_IsNull(amf_id)) {
        ogs_error("OpenAPI_operation_event_parseFromJSON() failed [amf_id]");
        goto end;
    }
    }

    ngran_failure_event_list = cJSON_GetObjectItemCaseSensitive(operation_eventJSON, "ngranFailureEventList");
    if (ngran_failure_event_list) {
        cJSON *ngran_failure_event_list_local = NULL;
        if (!cJSON_IsArray(ngran_failure_event_list)) {
            ogs_error("OpenAPI_operation_event_parseFromJSON() failed [ngran_failure_event_list]");
            goto end;
        }

        ngran_failure_event_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(ngran_failure_event_list_local, ngran_failure_event_list) {
            if (!cJSON_IsObject(ngran_failure_event_list_local)) {
                ogs_error("OpenAPI_operation_event_parseFromJSON() failed [ngran_failure_event_list]");
                goto end;
            }
            OpenAPI_ngran_failure_event_t *ngran_failure_event_listItem = OpenAPI_ngran_failure_event_parseFromJSON(ngran_failure_event_list_local);
            if (!ngran_failure_event_listItem) {
                ogs_error("No ngran_failure_event_listItem");
                goto end;
            }
            OpenAPI_list_add(ngran_failure_event_listList, ngran_failure_event_listItem);
        }
    }

    operation_event_local_var = OpenAPI_operation_event_create (
        op_event_typeVariable,
        amf_id && !cJSON_IsNull(amf_id) ? ogs_strdup(amf_id->valuestring) : NULL,
        ngran_failure_event_list ? ngran_failure_event_listList : NULL
    );

    return operation_event_local_var;
end:
    if (ngran_failure_event_listList) {
        OpenAPI_list_for_each(ngran_failure_event_listList, node) {
            OpenAPI_ngran_failure_event_free(node->data);
        }
        OpenAPI_list_free(ngran_failure_event_listList);
        ngran_failure_event_listList = NULL;
    }
    return NULL;
}

OpenAPI_operation_event_t *OpenAPI_operation_event_copy(OpenAPI_operation_event_t *dst, OpenAPI_operation_event_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_operation_event_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_operation_event_convertToJSON() failed");
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

    OpenAPI_operation_event_free(dst);
    dst = OpenAPI_operation_event_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

