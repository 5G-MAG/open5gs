
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_session_event_report.h"

OpenAPI_mbs_session_event_report_t *OpenAPI_mbs_session_event_report_create(
    OpenAPI_mbs_session_event_type_e event_type,
    char *time_stamp,
    OpenAPI_ingress_tun_addr_info_t *ingress_tun_addr_info,
    OpenAPI_broadcast_delivery_status_e broadcast_del_status
)
{
    OpenAPI_mbs_session_event_report_t *mbs_session_event_report_local_var = ogs_malloc(sizeof(OpenAPI_mbs_session_event_report_t));
    ogs_assert(mbs_session_event_report_local_var);

    mbs_session_event_report_local_var->event_type = event_type;
    mbs_session_event_report_local_var->time_stamp = time_stamp;
    mbs_session_event_report_local_var->ingress_tun_addr_info = ingress_tun_addr_info;
    mbs_session_event_report_local_var->broadcast_del_status = broadcast_del_status;

    return mbs_session_event_report_local_var;
}

void OpenAPI_mbs_session_event_report_free(OpenAPI_mbs_session_event_report_t *mbs_session_event_report)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_session_event_report) {
        return;
    }
    if (mbs_session_event_report->time_stamp) {
        ogs_free(mbs_session_event_report->time_stamp);
        mbs_session_event_report->time_stamp = NULL;
    }
    if (mbs_session_event_report->ingress_tun_addr_info) {
        OpenAPI_ingress_tun_addr_info_free(mbs_session_event_report->ingress_tun_addr_info);
        mbs_session_event_report->ingress_tun_addr_info = NULL;
    }
    ogs_free(mbs_session_event_report);
}

cJSON *OpenAPI_mbs_session_event_report_convertToJSON(OpenAPI_mbs_session_event_report_t *mbs_session_event_report)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_session_event_report == NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_convertToJSON() failed [MbsSessionEventReport]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_session_event_report->event_type == OpenAPI_mbs_session_event_type_NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_convertToJSON() failed [event_type]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "eventType", OpenAPI_mbs_session_event_type_ToString(mbs_session_event_report->event_type)) == NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_convertToJSON() failed [event_type]");
        goto end;
    }

    if (mbs_session_event_report->time_stamp) {
    if (cJSON_AddStringToObject(item, "timeStamp", mbs_session_event_report->time_stamp) == NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_convertToJSON() failed [time_stamp]");
        goto end;
    }
    }

    if (mbs_session_event_report->ingress_tun_addr_info) {
    cJSON *ingress_tun_addr_info_local_JSON = OpenAPI_ingress_tun_addr_info_convertToJSON(mbs_session_event_report->ingress_tun_addr_info);
    if (ingress_tun_addr_info_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_convertToJSON() failed [ingress_tun_addr_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "ingressTunAddrInfo", ingress_tun_addr_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_convertToJSON() failed [ingress_tun_addr_info]");
        goto end;
    }
    }

    if (mbs_session_event_report->broadcast_del_status != OpenAPI_broadcast_delivery_status_NULL) {
    if (cJSON_AddStringToObject(item, "broadcastDelStatus", OpenAPI_broadcast_delivery_status_ToString(mbs_session_event_report->broadcast_del_status)) == NULL) {
        ogs_error("OpenAPI_mbs_session_event_report_convertToJSON() failed [broadcast_del_status]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_session_event_report_t *OpenAPI_mbs_session_event_report_parseFromJSON(cJSON *mbs_session_event_reportJSON)
{
    OpenAPI_mbs_session_event_report_t *mbs_session_event_report_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *event_type = NULL;
    OpenAPI_mbs_session_event_type_e event_typeVariable = 0;
    cJSON *time_stamp = NULL;
    cJSON *ingress_tun_addr_info = NULL;
    OpenAPI_ingress_tun_addr_info_t *ingress_tun_addr_info_local_nonprim = NULL;
    cJSON *broadcast_del_status = NULL;
    OpenAPI_broadcast_delivery_status_e broadcast_del_statusVariable = 0;
    event_type = cJSON_GetObjectItemCaseSensitive(mbs_session_event_reportJSON, "eventType");
    if (!event_type) {
        ogs_error("OpenAPI_mbs_session_event_report_parseFromJSON() failed [event_type]");
        goto end;
    }
    if (!cJSON_IsString(event_type)) {
        ogs_error("OpenAPI_mbs_session_event_report_parseFromJSON() failed [event_type]");
        goto end;
    }
    event_typeVariable = OpenAPI_mbs_session_event_type_FromString(event_type->valuestring);

    time_stamp = cJSON_GetObjectItemCaseSensitive(mbs_session_event_reportJSON, "timeStamp");
    if (time_stamp) {
    if (!cJSON_IsString(time_stamp) && !cJSON_IsNull(time_stamp)) {
        ogs_error("OpenAPI_mbs_session_event_report_parseFromJSON() failed [time_stamp]");
        goto end;
    }
    }

    ingress_tun_addr_info = cJSON_GetObjectItemCaseSensitive(mbs_session_event_reportJSON, "ingressTunAddrInfo");
    if (ingress_tun_addr_info) {
    ingress_tun_addr_info_local_nonprim = OpenAPI_ingress_tun_addr_info_parseFromJSON(ingress_tun_addr_info);
    if (!ingress_tun_addr_info_local_nonprim) {
        ogs_error("OpenAPI_ingress_tun_addr_info_parseFromJSON failed [ingress_tun_addr_info]");
        goto end;
    }
    }

    broadcast_del_status = cJSON_GetObjectItemCaseSensitive(mbs_session_event_reportJSON, "broadcastDelStatus");
    if (broadcast_del_status) {
    if (!cJSON_IsString(broadcast_del_status)) {
        ogs_error("OpenAPI_mbs_session_event_report_parseFromJSON() failed [broadcast_del_status]");
        goto end;
    }
    broadcast_del_statusVariable = OpenAPI_broadcast_delivery_status_FromString(broadcast_del_status->valuestring);
    }

    mbs_session_event_report_local_var = OpenAPI_mbs_session_event_report_create (
        event_typeVariable,
        time_stamp && !cJSON_IsNull(time_stamp) ? ogs_strdup(time_stamp->valuestring) : NULL,
        ingress_tun_addr_info ? ingress_tun_addr_info_local_nonprim : NULL,
        broadcast_del_status ? broadcast_del_statusVariable : 0
    );

    return mbs_session_event_report_local_var;
end:
    if (ingress_tun_addr_info_local_nonprim) {
        OpenAPI_ingress_tun_addr_info_free(ingress_tun_addr_info_local_nonprim);
        ingress_tun_addr_info_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_mbs_session_event_report_t *OpenAPI_mbs_session_event_report_copy(OpenAPI_mbs_session_event_report_t *dst, OpenAPI_mbs_session_event_report_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_session_event_report_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_session_event_report_convertToJSON() failed");
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

    OpenAPI_mbs_session_event_report_free(dst);
    dst = OpenAPI_mbs_session_event_report_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

