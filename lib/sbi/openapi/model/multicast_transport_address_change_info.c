
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "multicast_transport_address_change_info.h"

OpenAPI_multicast_transport_address_change_info_t *OpenAPI_multicast_transport_address_change_info_create(
    OpenAPI_ssm_t *ll_ssm,
    int c_teid,
    bool is_area_session_id,
    int area_session_id
)
{
    OpenAPI_multicast_transport_address_change_info_t *multicast_transport_address_change_info_local_var = ogs_malloc(sizeof(OpenAPI_multicast_transport_address_change_info_t));
    ogs_assert(multicast_transport_address_change_info_local_var);

    multicast_transport_address_change_info_local_var->ll_ssm = ll_ssm;
    multicast_transport_address_change_info_local_var->c_teid = c_teid;
    multicast_transport_address_change_info_local_var->is_area_session_id = is_area_session_id;
    multicast_transport_address_change_info_local_var->area_session_id = area_session_id;

    return multicast_transport_address_change_info_local_var;
}

void OpenAPI_multicast_transport_address_change_info_free(OpenAPI_multicast_transport_address_change_info_t *multicast_transport_address_change_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == multicast_transport_address_change_info) {
        return;
    }
    if (multicast_transport_address_change_info->ll_ssm) {
        OpenAPI_ssm_free(multicast_transport_address_change_info->ll_ssm);
        multicast_transport_address_change_info->ll_ssm = NULL;
    }
    ogs_free(multicast_transport_address_change_info);
}

cJSON *OpenAPI_multicast_transport_address_change_info_convertToJSON(OpenAPI_multicast_transport_address_change_info_t *multicast_transport_address_change_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (multicast_transport_address_change_info == NULL) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_convertToJSON() failed [MulticastTransportAddressChangeInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!multicast_transport_address_change_info->ll_ssm) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_convertToJSON() failed [ll_ssm]");
        return NULL;
    }
    cJSON *ll_ssm_local_JSON = OpenAPI_ssm_convertToJSON(multicast_transport_address_change_info->ll_ssm);
    if (ll_ssm_local_JSON == NULL) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_convertToJSON() failed [ll_ssm]");
        goto end;
    }
    cJSON_AddItemToObject(item, "llSsm", ll_ssm_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_convertToJSON() failed [ll_ssm]");
        goto end;
    }

    if (cJSON_AddNumberToObject(item, "cTeid", multicast_transport_address_change_info->c_teid) == NULL) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_convertToJSON() failed [c_teid]");
        goto end;
    }

    if (multicast_transport_address_change_info->is_area_session_id) {
    if (cJSON_AddNumberToObject(item, "areaSessionId", multicast_transport_address_change_info->area_session_id) == NULL) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_convertToJSON() failed [area_session_id]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_multicast_transport_address_change_info_t *OpenAPI_multicast_transport_address_change_info_parseFromJSON(cJSON *multicast_transport_address_change_infoJSON)
{
    OpenAPI_multicast_transport_address_change_info_t *multicast_transport_address_change_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *ll_ssm = NULL;
    OpenAPI_ssm_t *ll_ssm_local_nonprim = NULL;
    cJSON *c_teid = NULL;
    cJSON *area_session_id = NULL;
    ll_ssm = cJSON_GetObjectItemCaseSensitive(multicast_transport_address_change_infoJSON, "llSsm");
    if (!ll_ssm) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_parseFromJSON() failed [ll_ssm]");
        goto end;
    }
    ll_ssm_local_nonprim = OpenAPI_ssm_parseFromJSON(ll_ssm);
    if (!ll_ssm_local_nonprim) {
        ogs_error("OpenAPI_ssm_parseFromJSON failed [ll_ssm]");
        goto end;
    }

    c_teid = cJSON_GetObjectItemCaseSensitive(multicast_transport_address_change_infoJSON, "cTeid");
    if (!c_teid) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_parseFromJSON() failed [c_teid]");
        goto end;
    }
    if (!cJSON_IsNumber(c_teid)) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_parseFromJSON() failed [c_teid]");
        goto end;
    }

    area_session_id = cJSON_GetObjectItemCaseSensitive(multicast_transport_address_change_infoJSON, "areaSessionId");
    if (area_session_id) {
    if (!cJSON_IsNumber(area_session_id)) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_parseFromJSON() failed [area_session_id]");
        goto end;
    }
    }

    multicast_transport_address_change_info_local_var = OpenAPI_multicast_transport_address_change_info_create (
        ll_ssm_local_nonprim,
        
        c_teid->valuedouble,
        area_session_id ? true : false,
        area_session_id ? area_session_id->valuedouble : 0
    );

    return multicast_transport_address_change_info_local_var;
end:
    if (ll_ssm_local_nonprim) {
        OpenAPI_ssm_free(ll_ssm_local_nonprim);
        ll_ssm_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_multicast_transport_address_change_info_t *OpenAPI_multicast_transport_address_change_info_copy(OpenAPI_multicast_transport_address_change_info_t *dst, OpenAPI_multicast_transport_address_change_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_multicast_transport_address_change_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_convertToJSON() failed");
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

    OpenAPI_multicast_transport_address_change_info_free(dst);
    dst = OpenAPI_multicast_transport_address_change_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

