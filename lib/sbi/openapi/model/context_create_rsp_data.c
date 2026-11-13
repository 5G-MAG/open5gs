
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_create_rsp_data.h"

OpenAPI_context_create_rsp_data_t *OpenAPI_context_create_rsp_data_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    OpenAPI_list_t *n2_mbs_sm_info_list,
    OpenAPI_operation_status_e operation_status
)
{
    OpenAPI_context_create_rsp_data_t *context_create_rsp_data_local_var = ogs_malloc(sizeof(OpenAPI_context_create_rsp_data_t));
    ogs_assert(context_create_rsp_data_local_var);

    context_create_rsp_data_local_var->mbs_session_id = mbs_session_id;
    context_create_rsp_data_local_var->n2_mbs_sm_info_list = n2_mbs_sm_info_list;
    context_create_rsp_data_local_var->operation_status = operation_status;

    return context_create_rsp_data_local_var;
}

void OpenAPI_context_create_rsp_data_free(OpenAPI_context_create_rsp_data_t *context_create_rsp_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_create_rsp_data) {
        return;
    }
    if (context_create_rsp_data->mbs_session_id) {
        OpenAPI_mbs_session_id_free(context_create_rsp_data->mbs_session_id);
        context_create_rsp_data->mbs_session_id = NULL;
    }
    if (context_create_rsp_data->n2_mbs_sm_info_list) {
        OpenAPI_list_for_each(context_create_rsp_data->n2_mbs_sm_info_list, node) {
            OpenAPI_n2_mbs_sm_info_free(node->data);
        }
        OpenAPI_list_free(context_create_rsp_data->n2_mbs_sm_info_list);
        context_create_rsp_data->n2_mbs_sm_info_list = NULL;
    }
    ogs_free(context_create_rsp_data);
}

cJSON *OpenAPI_context_create_rsp_data_convertToJSON(OpenAPI_context_create_rsp_data_t *context_create_rsp_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_create_rsp_data == NULL) {
        ogs_error("OpenAPI_context_create_rsp_data_convertToJSON() failed [ContextCreateRspData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!context_create_rsp_data->mbs_session_id) {
        ogs_error("OpenAPI_context_create_rsp_data_convertToJSON() failed [mbs_session_id]");
        return NULL;
    }
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(context_create_rsp_data->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_context_create_rsp_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_create_rsp_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }

    if (context_create_rsp_data->n2_mbs_sm_info_list) {
    cJSON *n2_mbs_sm_info_listList = cJSON_AddArrayToObject(item, "n2MbsSmInfoList");
    if (n2_mbs_sm_info_listList == NULL) {
        ogs_error("OpenAPI_context_create_rsp_data_convertToJSON() failed [n2_mbs_sm_info_list]");
        goto end;
    }
    OpenAPI_list_for_each(context_create_rsp_data->n2_mbs_sm_info_list, node) {
        cJSON *itemLocal = OpenAPI_n2_mbs_sm_info_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_context_create_rsp_data_convertToJSON() failed [n2_mbs_sm_info_list]");
            goto end;
        }
        cJSON_AddItemToArray(n2_mbs_sm_info_listList, itemLocal);
    }
    }

    if (context_create_rsp_data->operation_status != OpenAPI_operation_status_NULL) {
    if (cJSON_AddStringToObject(item, "operationStatus", OpenAPI_operation_status_ToString(context_create_rsp_data->operation_status)) == NULL) {
        ogs_error("OpenAPI_context_create_rsp_data_convertToJSON() failed [operation_status]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_create_rsp_data_t *OpenAPI_context_create_rsp_data_parseFromJSON(cJSON *context_create_rsp_dataJSON)
{
    OpenAPI_context_create_rsp_data_t *context_create_rsp_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *n2_mbs_sm_info_list = NULL;
    OpenAPI_list_t *n2_mbs_sm_info_listList = NULL;
    cJSON *operation_status = NULL;
    OpenAPI_operation_status_e operation_statusVariable = 0;
    mbs_session_id = cJSON_GetObjectItemCaseSensitive(context_create_rsp_dataJSON, "mbsSessionId");
    if (!mbs_session_id) {
        ogs_error("OpenAPI_context_create_rsp_data_parseFromJSON() failed [mbs_session_id]");
        goto end;
    }
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }

    n2_mbs_sm_info_list = cJSON_GetObjectItemCaseSensitive(context_create_rsp_dataJSON, "n2MbsSmInfoList");
    if (n2_mbs_sm_info_list) {
        cJSON *n2_mbs_sm_info_list_local = NULL;
        if (!cJSON_IsArray(n2_mbs_sm_info_list)) {
            ogs_error("OpenAPI_context_create_rsp_data_parseFromJSON() failed [n2_mbs_sm_info_list]");
            goto end;
        }

        n2_mbs_sm_info_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(n2_mbs_sm_info_list_local, n2_mbs_sm_info_list) {
            if (!cJSON_IsObject(n2_mbs_sm_info_list_local)) {
                ogs_error("OpenAPI_context_create_rsp_data_parseFromJSON() failed [n2_mbs_sm_info_list]");
                goto end;
            }
            OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info_listItem = OpenAPI_n2_mbs_sm_info_parseFromJSON(n2_mbs_sm_info_list_local);
            if (!n2_mbs_sm_info_listItem) {
                ogs_error("No n2_mbs_sm_info_listItem");
                goto end;
            }
            OpenAPI_list_add(n2_mbs_sm_info_listList, n2_mbs_sm_info_listItem);
        }
    }

    operation_status = cJSON_GetObjectItemCaseSensitive(context_create_rsp_dataJSON, "operationStatus");
    if (operation_status) {
    if (!cJSON_IsString(operation_status)) {
        ogs_error("OpenAPI_context_create_rsp_data_parseFromJSON() failed [operation_status]");
        goto end;
    }
    operation_statusVariable = OpenAPI_operation_status_FromString(operation_status->valuestring);
    }

    context_create_rsp_data_local_var = OpenAPI_context_create_rsp_data_create (
        mbs_session_id_local_nonprim,
        n2_mbs_sm_info_list ? n2_mbs_sm_info_listList : NULL,
        operation_status ? operation_statusVariable : 0
    );

    return context_create_rsp_data_local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (n2_mbs_sm_info_listList) {
        OpenAPI_list_for_each(n2_mbs_sm_info_listList, node) {
            OpenAPI_n2_mbs_sm_info_free(node->data);
        }
        OpenAPI_list_free(n2_mbs_sm_info_listList);
        n2_mbs_sm_info_listList = NULL;
    }
    return NULL;
}

OpenAPI_context_create_rsp_data_t *OpenAPI_context_create_rsp_data_copy(OpenAPI_context_create_rsp_data_t *dst, OpenAPI_context_create_rsp_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_create_rsp_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_create_rsp_data_convertToJSON() failed");
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

    OpenAPI_context_create_rsp_data_free(dst);
    dst = OpenAPI_context_create_rsp_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

