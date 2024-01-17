
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "qos_info.h"

OpenAPI_qos_info_t *OpenAPI_qos_info_create(
    OpenAPI_list_t *qos_flows_add_mod_request_list,
    OpenAPI_list_t *qos_flows_rel_request_list
)
{
    OpenAPI_qos_info_t *qos_info_local_var = ogs_malloc(sizeof(OpenAPI_qos_info_t));
    ogs_assert(qos_info_local_var);

    qos_info_local_var->qos_flows_add_mod_request_list = qos_flows_add_mod_request_list;
    qos_info_local_var->qos_flows_rel_request_list = qos_flows_rel_request_list;

    return qos_info_local_var;
}

void OpenAPI_qos_info_free(OpenAPI_qos_info_t *qos_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == qos_info) {
        return;
    }
    if (qos_info->qos_flows_add_mod_request_list) {
        OpenAPI_list_for_each(qos_info->qos_flows_add_mod_request_list, node) {
            OpenAPI_qos_flow_add_modify_request_item_free(node->data);
        }
        OpenAPI_list_free(qos_info->qos_flows_add_mod_request_list);
        qos_info->qos_flows_add_mod_request_list = NULL;
    }
    if (qos_info->qos_flows_rel_request_list) {
        OpenAPI_list_for_each(qos_info->qos_flows_rel_request_list, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(qos_info->qos_flows_rel_request_list);
        qos_info->qos_flows_rel_request_list = NULL;
    }
    ogs_free(qos_info);
}

cJSON *OpenAPI_qos_info_convertToJSON(OpenAPI_qos_info_t *qos_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (qos_info == NULL) {
        ogs_error("OpenAPI_qos_info_convertToJSON() failed [QosInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (qos_info->qos_flows_add_mod_request_list) {
    cJSON *qos_flows_add_mod_request_listList = cJSON_AddArrayToObject(item, "qosFlowsAddModRequestList");
    if (qos_flows_add_mod_request_listList == NULL) {
        ogs_error("OpenAPI_qos_info_convertToJSON() failed [qos_flows_add_mod_request_list]");
        goto end;
    }
    OpenAPI_list_for_each(qos_info->qos_flows_add_mod_request_list, node) {
        cJSON *itemLocal = OpenAPI_qos_flow_add_modify_request_item_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_qos_info_convertToJSON() failed [qos_flows_add_mod_request_list]");
            goto end;
        }
        cJSON_AddItemToArray(qos_flows_add_mod_request_listList, itemLocal);
    }
    }

    if (qos_info->qos_flows_rel_request_list) {
    cJSON *qos_flows_rel_request_listList = cJSON_AddArrayToObject(item, "qosFlowsRelRequestList");
    if (qos_flows_rel_request_listList == NULL) {
        ogs_error("OpenAPI_qos_info_convertToJSON() failed [qos_flows_rel_request_list]");
        goto end;
    }
    OpenAPI_list_for_each(qos_info->qos_flows_rel_request_list, node) {
        if (node->data == NULL) {
            ogs_error("OpenAPI_qos_info_convertToJSON() failed [qos_flows_rel_request_list]");
            goto end;
        }
        if (cJSON_AddNumberToObject(qos_flows_rel_request_listList, "", *(double *)node->data) == NULL) {
            ogs_error("OpenAPI_qos_info_convertToJSON() failed [qos_flows_rel_request_list]");
            goto end;
        }
    }
    }

end:
    return item;
}

OpenAPI_qos_info_t *OpenAPI_qos_info_parseFromJSON(cJSON *qos_infoJSON)
{
    OpenAPI_qos_info_t *qos_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *qos_flows_add_mod_request_list = NULL;
    OpenAPI_list_t *qos_flows_add_mod_request_listList = NULL;
    cJSON *qos_flows_rel_request_list = NULL;
    OpenAPI_list_t *qos_flows_rel_request_listList = NULL;
    qos_flows_add_mod_request_list = cJSON_GetObjectItemCaseSensitive(qos_infoJSON, "qosFlowsAddModRequestList");
    if (qos_flows_add_mod_request_list) {
        cJSON *qos_flows_add_mod_request_list_local = NULL;
        if (!cJSON_IsArray(qos_flows_add_mod_request_list)) {
            ogs_error("OpenAPI_qos_info_parseFromJSON() failed [qos_flows_add_mod_request_list]");
            goto end;
        }

        qos_flows_add_mod_request_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(qos_flows_add_mod_request_list_local, qos_flows_add_mod_request_list) {
            if (!cJSON_IsObject(qos_flows_add_mod_request_list_local)) {
                ogs_error("OpenAPI_qos_info_parseFromJSON() failed [qos_flows_add_mod_request_list]");
                goto end;
            }
            OpenAPI_qos_flow_add_modify_request_item_t *qos_flows_add_mod_request_listItem = OpenAPI_qos_flow_add_modify_request_item_parseFromJSON(qos_flows_add_mod_request_list_local);
            if (!qos_flows_add_mod_request_listItem) {
                ogs_error("No qos_flows_add_mod_request_listItem");
                goto end;
            }
            OpenAPI_list_add(qos_flows_add_mod_request_listList, qos_flows_add_mod_request_listItem);
        }
    }

    qos_flows_rel_request_list = cJSON_GetObjectItemCaseSensitive(qos_infoJSON, "qosFlowsRelRequestList");
    if (qos_flows_rel_request_list) {
        cJSON *qos_flows_rel_request_list_local = NULL;
        if (!cJSON_IsArray(qos_flows_rel_request_list)) {
            ogs_error("OpenAPI_qos_info_parseFromJSON() failed [qos_flows_rel_request_list]");
            goto end;
        }

        qos_flows_rel_request_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(qos_flows_rel_request_list_local, qos_flows_rel_request_list) {
            double *localDouble = NULL;
            int *localInt = NULL;
            if (!cJSON_IsNumber(qos_flows_rel_request_list_local)) {
                ogs_error("OpenAPI_qos_info_parseFromJSON() failed [qos_flows_rel_request_list]");
                goto end;
            }
            localDouble = (double *)ogs_calloc(1, sizeof(double));
            if (!localDouble) {
                ogs_error("OpenAPI_qos_info_parseFromJSON() failed [qos_flows_rel_request_list]");
                goto end;
            }
            *localDouble = qos_flows_rel_request_list_local->valuedouble;
            OpenAPI_list_add(qos_flows_rel_request_listList, localDouble);
        }
    }

    qos_info_local_var = OpenAPI_qos_info_create (
        qos_flows_add_mod_request_list ? qos_flows_add_mod_request_listList : NULL,
        qos_flows_rel_request_list ? qos_flows_rel_request_listList : NULL
    );

    return qos_info_local_var;
end:
    if (qos_flows_add_mod_request_listList) {
        OpenAPI_list_for_each(qos_flows_add_mod_request_listList, node) {
            OpenAPI_qos_flow_add_modify_request_item_free(node->data);
        }
        OpenAPI_list_free(qos_flows_add_mod_request_listList);
        qos_flows_add_mod_request_listList = NULL;
    }
    if (qos_flows_rel_request_listList) {
        OpenAPI_list_for_each(qos_flows_rel_request_listList, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(qos_flows_rel_request_listList);
        qos_flows_rel_request_listList = NULL;
    }
    return NULL;
}

OpenAPI_qos_info_t *OpenAPI_qos_info_copy(OpenAPI_qos_info_t *dst, OpenAPI_qos_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_qos_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_qos_info_convertToJSON() failed");
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

    OpenAPI_qos_info_free(dst);
    dst = OpenAPI_qos_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

