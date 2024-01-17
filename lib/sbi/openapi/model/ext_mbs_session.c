
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ext_mbs_session.h"

OpenAPI_ext_mbs_session_t *OpenAPI_ext_mbs_session_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_tmgi_alloc_req,
    int tmgi_alloc_req,
    OpenAPI_tmgi_t *tmgi,
    char *expiration_time,
    OpenAPI_mbs_service_type_e service_type,
    bool is_location_dependent,
    int location_dependent,
    bool is_area_session_id,
    int area_session_id,
    bool is_ingress_tun_addr_req,
    int ingress_tun_addr_req,
    OpenAPI_list_t *ingress_tun_addr,
    OpenAPI_ssm_t *ssm,
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_external_mbs_service_area_t *ext_mbs_service_area,
    char *dnn,
    OpenAPI_snssai_t *snssai,
    char *activation_time,
    char *start_time,
    char *termination_time,
    OpenAPI_mbs_service_info_t *mbs_serv_info,
    OpenAPI_mbs_session_subscription_t *mbs_session_subsc,
    OpenAPI_mbs_session_activity_status_e activity_status,
    bool is_any_ue_ind,
    int any_ue_ind,
    OpenAPI_list_t *mbs_fsa_id_list,
    OpenAPI_mbs_security_context_t *mbs_security_context,
    bool is_contact_pcf_ind,
    int contact_pcf_ind
)
{
    OpenAPI_ext_mbs_session_t *ext_mbs_session_local_var = ogs_malloc(sizeof(OpenAPI_ext_mbs_session_t));
    ogs_assert(ext_mbs_session_local_var);

    ext_mbs_session_local_var->mbs_session_id = mbs_session_id;
    ext_mbs_session_local_var->is_tmgi_alloc_req = is_tmgi_alloc_req;
    ext_mbs_session_local_var->tmgi_alloc_req = tmgi_alloc_req;
    ext_mbs_session_local_var->tmgi = tmgi;
    ext_mbs_session_local_var->expiration_time = expiration_time;
    ext_mbs_session_local_var->service_type = service_type;
    ext_mbs_session_local_var->is_location_dependent = is_location_dependent;
    ext_mbs_session_local_var->location_dependent = location_dependent;
    ext_mbs_session_local_var->is_area_session_id = is_area_session_id;
    ext_mbs_session_local_var->area_session_id = area_session_id;
    ext_mbs_session_local_var->is_ingress_tun_addr_req = is_ingress_tun_addr_req;
    ext_mbs_session_local_var->ingress_tun_addr_req = ingress_tun_addr_req;
    ext_mbs_session_local_var->ingress_tun_addr = ingress_tun_addr;
    ext_mbs_session_local_var->ssm = ssm;
    ext_mbs_session_local_var->mbs_service_area = mbs_service_area;
    ext_mbs_session_local_var->ext_mbs_service_area = ext_mbs_service_area;
    ext_mbs_session_local_var->dnn = dnn;
    ext_mbs_session_local_var->snssai = snssai;
    ext_mbs_session_local_var->activation_time = activation_time;
    ext_mbs_session_local_var->start_time = start_time;
    ext_mbs_session_local_var->termination_time = termination_time;
    ext_mbs_session_local_var->mbs_serv_info = mbs_serv_info;
    ext_mbs_session_local_var->mbs_session_subsc = mbs_session_subsc;
    ext_mbs_session_local_var->activity_status = activity_status;
    ext_mbs_session_local_var->is_any_ue_ind = is_any_ue_ind;
    ext_mbs_session_local_var->any_ue_ind = any_ue_ind;
    ext_mbs_session_local_var->mbs_fsa_id_list = mbs_fsa_id_list;
    ext_mbs_session_local_var->mbs_security_context = mbs_security_context;
    ext_mbs_session_local_var->is_contact_pcf_ind = is_contact_pcf_ind;
    ext_mbs_session_local_var->contact_pcf_ind = contact_pcf_ind;

    return ext_mbs_session_local_var;
}

void OpenAPI_ext_mbs_session_free(OpenAPI_ext_mbs_session_t *ext_mbs_session)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == ext_mbs_session) {
        return;
    }
    if (ext_mbs_session->mbs_session_id) {
        OpenAPI_mbs_session_id_free(ext_mbs_session->mbs_session_id);
        ext_mbs_session->mbs_session_id = NULL;
    }
    if (ext_mbs_session->tmgi) {
        OpenAPI_tmgi_free(ext_mbs_session->tmgi);
        ext_mbs_session->tmgi = NULL;
    }
    if (ext_mbs_session->expiration_time) {
        ogs_free(ext_mbs_session->expiration_time);
        ext_mbs_session->expiration_time = NULL;
    }
    if (ext_mbs_session->ingress_tun_addr) {
        OpenAPI_list_for_each(ext_mbs_session->ingress_tun_addr, node) {
            OpenAPI_tunnel_address_free(node->data);
        }
        OpenAPI_list_free(ext_mbs_session->ingress_tun_addr);
        ext_mbs_session->ingress_tun_addr = NULL;
    }
    if (ext_mbs_session->ssm) {
        OpenAPI_ssm_free(ext_mbs_session->ssm);
        ext_mbs_session->ssm = NULL;
    }
    if (ext_mbs_session->mbs_service_area) {
        OpenAPI_mbs_service_area_free(ext_mbs_session->mbs_service_area);
        ext_mbs_session->mbs_service_area = NULL;
    }
    if (ext_mbs_session->ext_mbs_service_area) {
        OpenAPI_external_mbs_service_area_free(ext_mbs_session->ext_mbs_service_area);
        ext_mbs_session->ext_mbs_service_area = NULL;
    }
    if (ext_mbs_session->dnn) {
        ogs_free(ext_mbs_session->dnn);
        ext_mbs_session->dnn = NULL;
    }
    if (ext_mbs_session->snssai) {
        OpenAPI_snssai_free(ext_mbs_session->snssai);
        ext_mbs_session->snssai = NULL;
    }
    if (ext_mbs_session->activation_time) {
        ogs_free(ext_mbs_session->activation_time);
        ext_mbs_session->activation_time = NULL;
    }
    if (ext_mbs_session->start_time) {
        ogs_free(ext_mbs_session->start_time);
        ext_mbs_session->start_time = NULL;
    }
    if (ext_mbs_session->termination_time) {
        ogs_free(ext_mbs_session->termination_time);
        ext_mbs_session->termination_time = NULL;
    }
    if (ext_mbs_session->mbs_serv_info) {
        OpenAPI_mbs_service_info_free(ext_mbs_session->mbs_serv_info);
        ext_mbs_session->mbs_serv_info = NULL;
    }
    if (ext_mbs_session->mbs_session_subsc) {
        OpenAPI_mbs_session_subscription_free(ext_mbs_session->mbs_session_subsc);
        ext_mbs_session->mbs_session_subsc = NULL;
    }
    if (ext_mbs_session->mbs_fsa_id_list) {
        OpenAPI_list_for_each(ext_mbs_session->mbs_fsa_id_list, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(ext_mbs_session->mbs_fsa_id_list);
        ext_mbs_session->mbs_fsa_id_list = NULL;
    }
    if (ext_mbs_session->mbs_security_context) {
        OpenAPI_mbs_security_context_free(ext_mbs_session->mbs_security_context);
        ext_mbs_session->mbs_security_context = NULL;
    }
    ogs_free(ext_mbs_session);
}

cJSON *OpenAPI_ext_mbs_session_convertToJSON(OpenAPI_ext_mbs_session_t *ext_mbs_session)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (ext_mbs_session == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [ExtMbsSession]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (ext_mbs_session->mbs_session_id) {
    cJSON *mbs_session_id_local_JSON = OpenAPI_mbs_session_id_convertToJSON(ext_mbs_session->mbs_session_id);
    if (mbs_session_id_local_JSON == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionId", mbs_session_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_session_id]");
        goto end;
    }
    }

    if (ext_mbs_session->is_tmgi_alloc_req) {
    if (cJSON_AddBoolToObject(item, "tmgiAllocReq", ext_mbs_session->tmgi_alloc_req) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [tmgi_alloc_req]");
        goto end;
    }
    }

    if (ext_mbs_session->tmgi) {
    cJSON *tmgi_local_JSON = OpenAPI_tmgi_convertToJSON(ext_mbs_session->tmgi);
    if (tmgi_local_JSON == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [tmgi]");
        goto end;
    }
    cJSON_AddItemToObject(item, "tmgi", tmgi_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [tmgi]");
        goto end;
    }
    }

    if (ext_mbs_session->expiration_time) {
    if (cJSON_AddStringToObject(item, "expirationTime", ext_mbs_session->expiration_time) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [expiration_time]");
        goto end;
    }
    }

    if (ext_mbs_session->service_type == OpenAPI_mbs_service_type_NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [service_type]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "serviceType", OpenAPI_mbs_service_type_ToString(ext_mbs_session->service_type)) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [service_type]");
        goto end;
    }

    if (ext_mbs_session->is_location_dependent) {
    if (cJSON_AddBoolToObject(item, "locationDependent", ext_mbs_session->location_dependent) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [location_dependent]");
        goto end;
    }
    }

    if (ext_mbs_session->is_area_session_id) {
    if (cJSON_AddNumberToObject(item, "areaSessionId", ext_mbs_session->area_session_id) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [area_session_id]");
        goto end;
    }
    }

    if (ext_mbs_session->is_ingress_tun_addr_req) {
    if (cJSON_AddBoolToObject(item, "ingressTunAddrReq", ext_mbs_session->ingress_tun_addr_req) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [ingress_tun_addr_req]");
        goto end;
    }
    }

    if (ext_mbs_session->ingress_tun_addr) {
    cJSON *ingress_tun_addrList = cJSON_AddArrayToObject(item, "ingressTunAddr");
    if (ingress_tun_addrList == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [ingress_tun_addr]");
        goto end;
    }
    OpenAPI_list_for_each(ext_mbs_session->ingress_tun_addr, node) {
        cJSON *itemLocal = OpenAPI_tunnel_address_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [ingress_tun_addr]");
            goto end;
        }
        cJSON_AddItemToArray(ingress_tun_addrList, itemLocal);
    }
    }

    if (ext_mbs_session->ssm) {
    cJSON *ssm_local_JSON = OpenAPI_ssm_convertToJSON(ext_mbs_session->ssm);
    if (ssm_local_JSON == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [ssm]");
        goto end;
    }
    cJSON_AddItemToObject(item, "ssm", ssm_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [ssm]");
        goto end;
    }
    }

    if (ext_mbs_session->mbs_service_area) {
    cJSON *mbs_service_area_local_JSON = OpenAPI_mbs_service_area_convertToJSON(ext_mbs_session->mbs_service_area);
    if (mbs_service_area_local_JSON == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsServiceArea", mbs_service_area_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    }

    if (ext_mbs_session->ext_mbs_service_area) {
    cJSON *ext_mbs_service_area_local_JSON = OpenAPI_external_mbs_service_area_convertToJSON(ext_mbs_session->ext_mbs_service_area);
    if (ext_mbs_service_area_local_JSON == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [ext_mbs_service_area]");
        goto end;
    }
    cJSON_AddItemToObject(item, "extMbsServiceArea", ext_mbs_service_area_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [ext_mbs_service_area]");
        goto end;
    }
    }

    if (ext_mbs_session->dnn) {
    if (cJSON_AddStringToObject(item, "dnn", ext_mbs_session->dnn) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [dnn]");
        goto end;
    }
    }

    if (ext_mbs_session->snssai) {
    cJSON *snssai_local_JSON = OpenAPI_snssai_convertToJSON(ext_mbs_session->snssai);
    if (snssai_local_JSON == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [snssai]");
        goto end;
    }
    cJSON_AddItemToObject(item, "snssai", snssai_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [snssai]");
        goto end;
    }
    }

    if (ext_mbs_session->activation_time) {
    if (cJSON_AddStringToObject(item, "activationTime", ext_mbs_session->activation_time) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [activation_time]");
        goto end;
    }
    }

    if (ext_mbs_session->start_time) {
    if (cJSON_AddStringToObject(item, "startTime", ext_mbs_session->start_time) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [start_time]");
        goto end;
    }
    }

    if (ext_mbs_session->termination_time) {
    if (cJSON_AddStringToObject(item, "terminationTime", ext_mbs_session->termination_time) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [termination_time]");
        goto end;
    }
    }

    if (ext_mbs_session->mbs_serv_info) {
    cJSON *mbs_serv_info_local_JSON = OpenAPI_mbs_service_info_convertToJSON(ext_mbs_session->mbs_serv_info);
    if (mbs_serv_info_local_JSON == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_serv_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsServInfo", mbs_serv_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_serv_info]");
        goto end;
    }
    }

    if (ext_mbs_session->mbs_session_subsc) {
    cJSON *mbs_session_subsc_local_JSON = OpenAPI_mbs_session_subscription_convertToJSON(ext_mbs_session->mbs_session_subsc);
    if (mbs_session_subsc_local_JSON == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_session_subsc]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSessionSubsc", mbs_session_subsc_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_session_subsc]");
        goto end;
    }
    }

    if (ext_mbs_session->activity_status != OpenAPI_mbs_session_activity_status_NULL) {
    if (cJSON_AddStringToObject(item, "activityStatus", OpenAPI_mbs_session_activity_status_ToString(ext_mbs_session->activity_status)) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [activity_status]");
        goto end;
    }
    }

    if (ext_mbs_session->is_any_ue_ind) {
    if (cJSON_AddBoolToObject(item, "anyUeInd", ext_mbs_session->any_ue_ind) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [any_ue_ind]");
        goto end;
    }
    }

    if (ext_mbs_session->mbs_fsa_id_list) {
    cJSON *mbs_fsa_id_listList = cJSON_AddArrayToObject(item, "mbsFsaIdList");
    if (mbs_fsa_id_listList == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_fsa_id_list]");
        goto end;
    }
    OpenAPI_list_for_each(ext_mbs_session->mbs_fsa_id_list, node) {
        if (cJSON_AddStringToObject(mbs_fsa_id_listList, "", (char*)node->data) == NULL) {
            ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_fsa_id_list]");
            goto end;
        }
    }
    }

    if (ext_mbs_session->mbs_security_context) {
    cJSON *mbs_security_context_local_JSON = OpenAPI_mbs_security_context_convertToJSON(ext_mbs_session->mbs_security_context);
    if (mbs_security_context_local_JSON == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_security_context]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSecurityContext", mbs_security_context_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [mbs_security_context]");
        goto end;
    }
    }

    if (ext_mbs_session->is_contact_pcf_ind) {
    if (cJSON_AddBoolToObject(item, "contactPcfInd", ext_mbs_session->contact_pcf_ind) == NULL) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed [contact_pcf_ind]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_ext_mbs_session_t *OpenAPI_ext_mbs_session_parseFromJSON(cJSON *ext_mbs_sessionJSON)
{
    OpenAPI_ext_mbs_session_t *ext_mbs_session_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_session_id = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_local_nonprim = NULL;
    cJSON *tmgi_alloc_req = NULL;
    cJSON *tmgi = NULL;
    OpenAPI_tmgi_t *tmgi_local_nonprim = NULL;
    cJSON *expiration_time = NULL;
    cJSON *service_type = NULL;
    OpenAPI_mbs_service_type_e service_typeVariable = 0;
    cJSON *location_dependent = NULL;
    cJSON *area_session_id = NULL;
    cJSON *ingress_tun_addr_req = NULL;
    cJSON *ingress_tun_addr = NULL;
    OpenAPI_list_t *ingress_tun_addrList = NULL;
    cJSON *ssm = NULL;
    OpenAPI_ssm_t *ssm_local_nonprim = NULL;
    cJSON *mbs_service_area = NULL;
    OpenAPI_mbs_service_area_t *mbs_service_area_local_nonprim = NULL;
    cJSON *ext_mbs_service_area = NULL;
    OpenAPI_external_mbs_service_area_t *ext_mbs_service_area_local_nonprim = NULL;
    cJSON *dnn = NULL;
    cJSON *snssai = NULL;
    OpenAPI_snssai_t *snssai_local_nonprim = NULL;
    cJSON *activation_time = NULL;
    cJSON *start_time = NULL;
    cJSON *termination_time = NULL;
    cJSON *mbs_serv_info = NULL;
    OpenAPI_mbs_service_info_t *mbs_serv_info_local_nonprim = NULL;
    cJSON *mbs_session_subsc = NULL;
    OpenAPI_mbs_session_subscription_t *mbs_session_subsc_local_nonprim = NULL;
    cJSON *activity_status = NULL;
    OpenAPI_mbs_session_activity_status_e activity_statusVariable = 0;
    cJSON *any_ue_ind = NULL;
    cJSON *mbs_fsa_id_list = NULL;
    OpenAPI_list_t *mbs_fsa_id_listList = NULL;
    cJSON *mbs_security_context = NULL;
    OpenAPI_mbs_security_context_t *mbs_security_context_local_nonprim = NULL;
    cJSON *contact_pcf_ind = NULL;
    mbs_session_id = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "mbsSessionId");
    if (mbs_session_id) {
    mbs_session_id_local_nonprim = OpenAPI_mbs_session_id_parseFromJSON(mbs_session_id);
    if (!mbs_session_id_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_id_parseFromJSON failed [mbs_session_id]");
        goto end;
    }
    }

    tmgi_alloc_req = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "tmgiAllocReq");
    if (tmgi_alloc_req) {
    if (!cJSON_IsBool(tmgi_alloc_req)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [tmgi_alloc_req]");
        goto end;
    }
    }

    tmgi = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "tmgi");
    if (tmgi) {
    tmgi_local_nonprim = OpenAPI_tmgi_parseFromJSON(tmgi);
    if (!tmgi_local_nonprim) {
        ogs_error("OpenAPI_tmgi_parseFromJSON failed [tmgi]");
        goto end;
    }
    }

    expiration_time = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "expirationTime");
    if (expiration_time) {
    if (!cJSON_IsString(expiration_time) && !cJSON_IsNull(expiration_time)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [expiration_time]");
        goto end;
    }
    }

    service_type = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "serviceType");
    if (!service_type) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [service_type]");
        goto end;
    }
    if (!cJSON_IsString(service_type)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [service_type]");
        goto end;
    }
    service_typeVariable = OpenAPI_mbs_service_type_FromString(service_type->valuestring);

    location_dependent = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "locationDependent");
    if (location_dependent) {
    if (!cJSON_IsBool(location_dependent)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [location_dependent]");
        goto end;
    }
    }

    area_session_id = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "areaSessionId");
    if (area_session_id) {
    if (!cJSON_IsNumber(area_session_id)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [area_session_id]");
        goto end;
    }
    }

    ingress_tun_addr_req = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "ingressTunAddrReq");
    if (ingress_tun_addr_req) {
    if (!cJSON_IsBool(ingress_tun_addr_req)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [ingress_tun_addr_req]");
        goto end;
    }
    }

    ingress_tun_addr = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "ingressTunAddr");
    if (ingress_tun_addr) {
        cJSON *ingress_tun_addr_local = NULL;
        if (!cJSON_IsArray(ingress_tun_addr)) {
            ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [ingress_tun_addr]");
            goto end;
        }

        ingress_tun_addrList = OpenAPI_list_create();

        cJSON_ArrayForEach(ingress_tun_addr_local, ingress_tun_addr) {
            if (!cJSON_IsObject(ingress_tun_addr_local)) {
                ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [ingress_tun_addr]");
                goto end;
            }
            OpenAPI_tunnel_address_t *ingress_tun_addrItem = OpenAPI_tunnel_address_parseFromJSON(ingress_tun_addr_local);
            if (!ingress_tun_addrItem) {
                ogs_error("No ingress_tun_addrItem");
                goto end;
            }
            OpenAPI_list_add(ingress_tun_addrList, ingress_tun_addrItem);
        }
    }

    ssm = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "ssm");
    if (ssm) {
    ssm_local_nonprim = OpenAPI_ssm_parseFromJSON(ssm);
    if (!ssm_local_nonprim) {
        ogs_error("OpenAPI_ssm_parseFromJSON failed [ssm]");
        goto end;
    }
    }

    mbs_service_area = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "mbsServiceArea");
    if (mbs_service_area) {
    mbs_service_area_local_nonprim = OpenAPI_mbs_service_area_parseFromJSON(mbs_service_area);
    if (!mbs_service_area_local_nonprim) {
        ogs_error("OpenAPI_mbs_service_area_parseFromJSON failed [mbs_service_area]");
        goto end;
    }
    }

    ext_mbs_service_area = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "extMbsServiceArea");
    if (ext_mbs_service_area) {
    ext_mbs_service_area_local_nonprim = OpenAPI_external_mbs_service_area_parseFromJSON(ext_mbs_service_area);
    if (!ext_mbs_service_area_local_nonprim) {
        ogs_error("OpenAPI_external_mbs_service_area_parseFromJSON failed [ext_mbs_service_area]");
        goto end;
    }
    }

    dnn = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "dnn");
    if (dnn) {
    if (!cJSON_IsString(dnn) && !cJSON_IsNull(dnn)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [dnn]");
        goto end;
    }
    }

    snssai = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "snssai");
    if (snssai) {
    snssai_local_nonprim = OpenAPI_snssai_parseFromJSON(snssai);
    if (!snssai_local_nonprim) {
        ogs_error("OpenAPI_snssai_parseFromJSON failed [snssai]");
        goto end;
    }
    }

    activation_time = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "activationTime");
    if (activation_time) {
    if (!cJSON_IsString(activation_time) && !cJSON_IsNull(activation_time)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [activation_time]");
        goto end;
    }
    }

    start_time = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "startTime");
    if (start_time) {
    if (!cJSON_IsString(start_time) && !cJSON_IsNull(start_time)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [start_time]");
        goto end;
    }
    }

    termination_time = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "terminationTime");
    if (termination_time) {
    if (!cJSON_IsString(termination_time) && !cJSON_IsNull(termination_time)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [termination_time]");
        goto end;
    }
    }

    mbs_serv_info = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "mbsServInfo");
    if (mbs_serv_info) {
    mbs_serv_info_local_nonprim = OpenAPI_mbs_service_info_parseFromJSON(mbs_serv_info);
    if (!mbs_serv_info_local_nonprim) {
        ogs_error("OpenAPI_mbs_service_info_parseFromJSON failed [mbs_serv_info]");
        goto end;
    }
    }

    mbs_session_subsc = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "mbsSessionSubsc");
    if (mbs_session_subsc) {
    mbs_session_subsc_local_nonprim = OpenAPI_mbs_session_subscription_parseFromJSON(mbs_session_subsc);
    if (!mbs_session_subsc_local_nonprim) {
        ogs_error("OpenAPI_mbs_session_subscription_parseFromJSON failed [mbs_session_subsc]");
        goto end;
    }
    }

    activity_status = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "activityStatus");
    if (activity_status) {
    if (!cJSON_IsString(activity_status)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [activity_status]");
        goto end;
    }
    activity_statusVariable = OpenAPI_mbs_session_activity_status_FromString(activity_status->valuestring);
    }

    any_ue_ind = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "anyUeInd");
    if (any_ue_ind) {
    if (!cJSON_IsBool(any_ue_ind)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [any_ue_ind]");
        goto end;
    }
    }

    mbs_fsa_id_list = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "mbsFsaIdList");
    if (mbs_fsa_id_list) {
        cJSON *mbs_fsa_id_list_local = NULL;
        if (!cJSON_IsArray(mbs_fsa_id_list)) {
            ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [mbs_fsa_id_list]");
            goto end;
        }

        mbs_fsa_id_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(mbs_fsa_id_list_local, mbs_fsa_id_list) {
            double *localDouble = NULL;
            int *localInt = NULL;
            if (!cJSON_IsString(mbs_fsa_id_list_local)) {
                ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [mbs_fsa_id_list]");
                goto end;
            }
            OpenAPI_list_add(mbs_fsa_id_listList, ogs_strdup(mbs_fsa_id_list_local->valuestring));
        }
    }

    mbs_security_context = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "mbsSecurityContext");
    if (mbs_security_context) {
    mbs_security_context_local_nonprim = OpenAPI_mbs_security_context_parseFromJSON(mbs_security_context);
    if (!mbs_security_context_local_nonprim) {
        ogs_error("OpenAPI_mbs_security_context_parseFromJSON failed [mbs_security_context]");
        goto end;
    }
    }

    contact_pcf_ind = cJSON_GetObjectItemCaseSensitive(ext_mbs_sessionJSON, "contactPcfInd");
    if (contact_pcf_ind) {
    if (!cJSON_IsBool(contact_pcf_ind)) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON() failed [contact_pcf_ind]");
        goto end;
    }
    }

    ext_mbs_session_local_var = OpenAPI_ext_mbs_session_create (
        mbs_session_id ? mbs_session_id_local_nonprim : NULL,
        tmgi_alloc_req ? true : false,
        tmgi_alloc_req ? tmgi_alloc_req->valueint : 0,
        tmgi ? tmgi_local_nonprim : NULL,
        expiration_time && !cJSON_IsNull(expiration_time) ? ogs_strdup(expiration_time->valuestring) : NULL,
        service_typeVariable,
        location_dependent ? true : false,
        location_dependent ? location_dependent->valueint : 0,
        area_session_id ? true : false,
        area_session_id ? area_session_id->valuedouble : 0,
        ingress_tun_addr_req ? true : false,
        ingress_tun_addr_req ? ingress_tun_addr_req->valueint : 0,
        ingress_tun_addr ? ingress_tun_addrList : NULL,
        ssm ? ssm_local_nonprim : NULL,
        mbs_service_area ? mbs_service_area_local_nonprim : NULL,
        ext_mbs_service_area ? ext_mbs_service_area_local_nonprim : NULL,
        dnn && !cJSON_IsNull(dnn) ? ogs_strdup(dnn->valuestring) : NULL,
        snssai ? snssai_local_nonprim : NULL,
        activation_time && !cJSON_IsNull(activation_time) ? ogs_strdup(activation_time->valuestring) : NULL,
        start_time && !cJSON_IsNull(start_time) ? ogs_strdup(start_time->valuestring) : NULL,
        termination_time && !cJSON_IsNull(termination_time) ? ogs_strdup(termination_time->valuestring) : NULL,
        mbs_serv_info ? mbs_serv_info_local_nonprim : NULL,
        mbs_session_subsc ? mbs_session_subsc_local_nonprim : NULL,
        activity_status ? activity_statusVariable : 0,
        any_ue_ind ? true : false,
        any_ue_ind ? any_ue_ind->valueint : 0,
        mbs_fsa_id_list ? mbs_fsa_id_listList : NULL,
        mbs_security_context ? mbs_security_context_local_nonprim : NULL,
        contact_pcf_ind ? true : false,
        contact_pcf_ind ? contact_pcf_ind->valueint : 0
    );

    return ext_mbs_session_local_var;
end:
    if (mbs_session_id_local_nonprim) {
        OpenAPI_mbs_session_id_free(mbs_session_id_local_nonprim);
        mbs_session_id_local_nonprim = NULL;
    }
    if (tmgi_local_nonprim) {
        OpenAPI_tmgi_free(tmgi_local_nonprim);
        tmgi_local_nonprim = NULL;
    }
    if (ingress_tun_addrList) {
        OpenAPI_list_for_each(ingress_tun_addrList, node) {
            OpenAPI_tunnel_address_free(node->data);
        }
        OpenAPI_list_free(ingress_tun_addrList);
        ingress_tun_addrList = NULL;
    }
    if (ssm_local_nonprim) {
        OpenAPI_ssm_free(ssm_local_nonprim);
        ssm_local_nonprim = NULL;
    }
    if (mbs_service_area_local_nonprim) {
        OpenAPI_mbs_service_area_free(mbs_service_area_local_nonprim);
        mbs_service_area_local_nonprim = NULL;
    }
    if (ext_mbs_service_area_local_nonprim) {
        OpenAPI_external_mbs_service_area_free(ext_mbs_service_area_local_nonprim);
        ext_mbs_service_area_local_nonprim = NULL;
    }
    if (snssai_local_nonprim) {
        OpenAPI_snssai_free(snssai_local_nonprim);
        snssai_local_nonprim = NULL;
    }
    if (mbs_serv_info_local_nonprim) {
        OpenAPI_mbs_service_info_free(mbs_serv_info_local_nonprim);
        mbs_serv_info_local_nonprim = NULL;
    }
    if (mbs_session_subsc_local_nonprim) {
        OpenAPI_mbs_session_subscription_free(mbs_session_subsc_local_nonprim);
        mbs_session_subsc_local_nonprim = NULL;
    }
    if (mbs_fsa_id_listList) {
        OpenAPI_list_for_each(mbs_fsa_id_listList, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_fsa_id_listList);
        mbs_fsa_id_listList = NULL;
    }
    if (mbs_security_context_local_nonprim) {
        OpenAPI_mbs_security_context_free(mbs_security_context_local_nonprim);
        mbs_security_context_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_ext_mbs_session_t *OpenAPI_ext_mbs_session_copy(OpenAPI_ext_mbs_session_t *dst, OpenAPI_ext_mbs_session_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_ext_mbs_session_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_ext_mbs_session_convertToJSON() failed");
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

    OpenAPI_ext_mbs_session_free(dst);
    dst = OpenAPI_ext_mbs_session_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

