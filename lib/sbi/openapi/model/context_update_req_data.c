
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_update_req_data.h"

char *OpenAPI_leave_indcontext_update_req_data_ToString(OpenAPI_context_update_req_data_leave_ind_e leave_ind)
{
    const char *leave_indArray[] =  { "NULL", "true" };
    size_t sizeofArray = sizeof(leave_indArray) / sizeof(leave_indArray[0]);
    if (leave_ind < sizeofArray)
        return (char *)leave_indArray[leave_ind];
    else
        return (char *)"Unknown";
}

OpenAPI_context_update_req_data_leave_ind_e OpenAPI_leave_indcontext_update_req_data_FromString(char* leave_ind)
{
    int stringToReturn = 0;
    const char *leave_indArray[] =  { "NULL", "true" };
    size_t sizeofArray = sizeof(leave_indArray) / sizeof(leave_indArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(leave_ind, leave_indArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}
OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_create(
    char *nfc_instance_id,
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_area_session_id,
    int area_session_id,
    OpenAPI_context_update_action_e requested_action,
    char *dl_tunnel_info,
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info,
    OpenAPI_global_ran_node_id_t *ran_node_id,
    OpenAPI_context_update_req_data_leave_ind_e leave_ind
)
{
    OpenAPI_context_update_req_data_t *context_update_req_data_local_var = ogs_malloc(sizeof(OpenAPI_context_update_req_data_t));
    ogs_assert(context_update_req_data_local_var);

    context_update_req_data_local_var->nfc_instance_id = nfc_instance_id;
    context_update_req_data_local_var->mbs_session_id = mbs_session_id;
    context_update_req_data_local_var->is_area_session_id = is_area_session_id;
    context_update_req_data_local_var->area_session_id = area_session_id;
    context_update_req_data_local_var->requested_action = requested_action;
    context_update_req_data_local_var->dl_tunnel_info = dl_tunnel_info;
    context_update_req_data_local_var->n2_mbs_sm_info = n2_mbs_sm_info;
    context_update_req_data_local_var->ran_node_id = ran_node_id;
    context_update_req_data_local_var->leave_ind = leave_ind;

    return context_update_req_data_local_var;
}

void OpenAPI_context_update_req_data_free(OpenAPI_context_update_req_data_t *context_update_req_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_update_req_data) {
        return;
    }
    if (context_update_req_data->nfc_instance_id) {
        ogs_free(context_update_req_data->nfc_instance_id);
        context_update_req_data->nfc_instance_id = NULL;
    }
    if (context_update_req_data->mbs_session_id) {
        OpenAPI_mbs_session_id_free(context_update_req_data->mbs_session_id);
        context_update_req_data->mbs_session_id = NULL;
    }
    if (context_update_req_data->dl_tunnel_info) {
        ogs_free(context_update_req_data->dl_tunnel_info);
        context_update_req_data->dl_tunnel_info = NULL;
    }
    if (context_update_req_data->n2_mbs_sm_info) {
        OpenAPI_n2_mbs_sm_info_free(context_update_req_data->n2_mbs_sm_info);
        context_update_req_data->n2_mbs_sm_info = NULL;
    }
    if (context_update_req_data->ran_node_id) {
        OpenAPI_global_ran_node_id_free(context_update_req_data->ran_node_id);
        context_update_req_data->ran_node_id = NULL;
    }
    ogs_free(context_update_req_data);
}

cJSON *OpenAPI_context_update_req_data_convertToJSON(OpenAPI_context_update_req_data_t *context_update_req_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_update_req_data == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [ContextUpdateReqData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!context_update_req_data->nfc_instance_id) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [nfc_instance_id]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "nfcInstanceId", context_update_req_data->nfc_instance_id) == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [nfc_instance_id]");
        goto end;
    }

    if (!context_update_req_data->mbs_session_id) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [mbs_session_id]");
        return NULL;
    }
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(context_update_req_data->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [mbs_session_id]");
        goto end;
    }

    if (context_update_req_data->is_area_session_id) {
    if (cJSON_AddNumberToObject(item, "areaSessionId", context_update_req_data->area_session_id) == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [area_session_id]");
        goto end;
    }
    }

    if (context_update_req_data->requested_action != OpenAPI_context_update_action_NULL) {
    if (cJSON_AddStringToObject(item, "requestedAction", OpenAPI_context_update_action_ToString(context_update_req_data->requested_action)) == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [requested_action]");
        goto end;
    }
    }

    if (context_update_req_data->dl_tunnel_info) {
    if (cJSON_AddStringToObject(item, "dlTunnelInfo", context_update_req_data->dl_tunnel_info) == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [dl_tunnel_info]");
        goto end;
    }
    }

    if (context_update_req_data->n2_mbs_sm_info) {
    cJSON *n2_mbs_sm_info_local_JSON = OpenAPI_n2_mbs_sm_info_convertToJSON(context_update_req_data->n2_mbs_sm_info);
    if (n2_mbs_sm_info_local_JSON == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "n2MbsSmInfo", n2_mbs_sm_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [n2_mbs_sm_info]");
        goto end;
    }
    }

    if (context_update_req_data->ran_node_id) {
    cJSON *ran_node_id_local_JSON = OpenAPI_global_ran_node_id_convertToJSON(context_update_req_data->ran_node_id);
    if (ran_node_id_local_JSON == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [ran_node_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "ranNodeId", ran_node_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [ran_node_id]");
        goto end;
    }
    }

    if (context_update_req_data->leave_ind != OpenAPI_context_update_req_data_LEAVEIND_NULL) {
    if (cJSON_AddStringToObject(item, "leaveInd", OpenAPI_leave_indcontext_update_req_data_ToString(context_update_req_data->leave_ind)) == NULL) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed [leave_ind]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_parseFromJSON(cJSON *context_update_req_dataJSON)
{
    OpenAPI_context_update_req_data_t *context_update_req_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *nfc_instance_id = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *area_session_id = NULL;
    cJSON *requested_action = NULL;
    OpenAPI_context_update_action_e requested_actionVariable = 0;
    cJSON *dl_tunnel_info = NULL;
    cJSON *n2_mbs_sm_info = NULL;
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info_local_nonprim = NULL;
    cJSON *ran_node_id = NULL;
    OpenAPI_global_ran_node_id_t *ran_node_id_local_nonprim = NULL;
    cJSON *leave_ind = NULL;
    OpenAPI_context_update_req_data_leave_ind_e leave_indVariable = 0;
    nfc_instance_id = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "nfcInstanceId");
    if (!nfc_instance_id) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [nfc_instance_id]");
        goto end;
    }
    if (!cJSON_IsString(nfc_instance_id)) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [nfc_instance_id]");
        goto end;
    }

    mbs_session_id = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "mbsSessionId");
    if (!mbs_session_id) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [mbs_session_id]");
        goto end;
    }
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }

    area_session_id = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "areaSessionId");
    if (area_session_id) {
    if (!cJSON_IsNumber(area_session_id)) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [area_session_id]");
        goto end;
    }
    }

    requested_action = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "requestedAction");
    if (requested_action) {
    if (!cJSON_IsString(requested_action)) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [requested_action]");
        goto end;
    }
    requested_actionVariable = OpenAPI_context_update_action_FromString(requested_action->valuestring);
    }

    dl_tunnel_info = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "dlTunnelInfo");
    if (dl_tunnel_info) {
    if (!cJSON_IsString(dl_tunnel_info) && !cJSON_IsNull(dl_tunnel_info)) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [dl_tunnel_info]");
        goto end;
    }
    }

    n2_mbs_sm_info = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "n2MbsSmInfo");
    if (n2_mbs_sm_info) {
    n2_mbs_sm_info_local_nonprim = OpenAPI_n2_mbs_sm_info_parseFromJSON(n2_mbs_sm_info);
    if (!n2_mbs_sm_info_local_nonprim) {
        ogs_error("OpenAPI_n2_mbs_sm_info_parseFromJSON failed [n2_mbs_sm_info]");
        goto end;
    }
    }

    ran_node_id = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "ranNodeId");
    if (ran_node_id) {
    ran_node_id_local_nonprim = OpenAPI_global_ran_node_id_parseFromJSON(ran_node_id);
    if (!ran_node_id_local_nonprim) {
        ogs_error("OpenAPI_global_ran_node_id_parseFromJSON failed [ran_node_id]");
        goto end;
    }
    }

    leave_ind = cJSON_GetObjectItemCaseSensitive(context_update_req_dataJSON, "leaveInd");
    if (leave_ind) {
    if (!cJSON_IsString(leave_ind)) {
        ogs_error("OpenAPI_context_update_req_data_parseFromJSON() failed [leave_ind]");
        goto end;
    }
    leave_indVariable = OpenAPI_leave_indcontext_update_req_data_FromString(leave_ind->valuestring);
    }

    context_update_req_data_local_var = OpenAPI_context_update_req_data_create (
        ogs_strdup(nfc_instance_id->valuestring),
        mbs_session_id_local_nonprim,
        area_session_id ? true : false,
        area_session_id ? area_session_id->valuedouble : 0,
        requested_action ? requested_actionVariable : 0,
        dl_tunnel_info && !cJSON_IsNull(dl_tunnel_info) ? ogs_strdup(dl_tunnel_info->valuestring) : NULL,
        n2_mbs_sm_info ? n2_mbs_sm_info_local_nonprim : NULL,
        ran_node_id ? ran_node_id_local_nonprim : NULL,
        leave_ind ? leave_indVariable : 0
    );

    return context_update_req_data_local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (n2_mbs_sm_info_local_nonprim) {
        OpenAPI_n2_mbs_sm_info_free(n2_mbs_sm_info_local_nonprim);
        n2_mbs_sm_info_local_nonprim = NULL;
    }
    if (ran_node_id_local_nonprim) {
        OpenAPI_global_ran_node_id_free(ran_node_id_local_nonprim);
        ran_node_id_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_context_update_req_data_t *OpenAPI_context_update_req_data_copy(OpenAPI_context_update_req_data_t *dst, OpenAPI_context_update_req_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_update_req_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_update_req_data_convertToJSON() failed");
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

    OpenAPI_context_update_req_data_free(dst);
    dst = OpenAPI_context_update_req_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

