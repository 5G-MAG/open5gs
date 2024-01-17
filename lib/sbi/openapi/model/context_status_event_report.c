
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_status_event_report.h"

OpenAPI_context_status_event_report_t *OpenAPI_context_status_event_report_create(
    OpenAPI_context_status_event_type_e event_type,
    char *time_stamp,
    OpenAPI_qos_info_t *qos_info,
    OpenAPI_mbs_session_activity_status_e status_info,
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_list_t* mbs_service_area_info_list,
    OpenAPI_multicast_transport_address_change_info_t *multicast_trans_add_info,
    OpenAPI_mbs_security_context_t *mbs_security_context
)
{
    OpenAPI_context_status_event_report_t *context_status_event_report_local_var = ogs_malloc(sizeof(OpenAPI_context_status_event_report_t));
    ogs_assert(context_status_event_report_local_var);

    context_status_event_report_local_var->event_type = event_type;
    context_status_event_report_local_var->time_stamp = time_stamp;
    context_status_event_report_local_var->qos_info = qos_info;
    context_status_event_report_local_var->status_info = status_info;
    context_status_event_report_local_var->mbs_service_area = mbs_service_area;
    context_status_event_report_local_var->mbs_service_area_info_list = mbs_service_area_info_list;
    context_status_event_report_local_var->multicast_trans_add_info = multicast_trans_add_info;
    context_status_event_report_local_var->mbs_security_context = mbs_security_context;

    return context_status_event_report_local_var;
}

void OpenAPI_context_status_event_report_free(OpenAPI_context_status_event_report_t *context_status_event_report)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_status_event_report) {
        return;
    }
    if (context_status_event_report->time_stamp) {
        ogs_free(context_status_event_report->time_stamp);
        context_status_event_report->time_stamp = NULL;
    }
    if (context_status_event_report->qos_info) {
        OpenAPI_qos_info_free(context_status_event_report->qos_info);
        context_status_event_report->qos_info = NULL;
    }
    if (context_status_event_report->mbs_service_area) {
        OpenAPI_mbs_service_area_free(context_status_event_report->mbs_service_area);
        context_status_event_report->mbs_service_area = NULL;
    }
    if (context_status_event_report->mbs_service_area_info_list) {
        OpenAPI_list_for_each(context_status_event_report->mbs_service_area_info_list, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_service_area_info_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(context_status_event_report->mbs_service_area_info_list);
        context_status_event_report->mbs_service_area_info_list = NULL;
    }
    if (context_status_event_report->multicast_trans_add_info) {
        OpenAPI_multicast_transport_address_change_info_free(context_status_event_report->multicast_trans_add_info);
        context_status_event_report->multicast_trans_add_info = NULL;
    }
    if (context_status_event_report->mbs_security_context) {
        OpenAPI_mbs_security_context_free(context_status_event_report->mbs_security_context);
        context_status_event_report->mbs_security_context = NULL;
    }
    ogs_free(context_status_event_report);
}

cJSON *OpenAPI_context_status_event_report_convertToJSON(OpenAPI_context_status_event_report_t *context_status_event_report)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_status_event_report == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [ContextStatusEventReport]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (context_status_event_report->event_type == OpenAPI_context_status_event_type_NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [event_type]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "eventType", OpenAPI_context_status_event_type_ToString(context_status_event_report->event_type)) == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [event_type]");
        goto end;
    }

    if (!context_status_event_report->time_stamp) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [time_stamp]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "timeStamp", context_status_event_report->time_stamp) == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [time_stamp]");
        goto end;
    }

    if (context_status_event_report->qos_info) {
    cJSON *qos_info_local_JSON = OpenAPI_qos_info_convertToJSON(context_status_event_report->qos_info);
    if (qos_info_local_JSON == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [qos_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "qosInfo", qos_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [qos_info]");
        goto end;
    }
    }

    if (context_status_event_report->status_info != OpenAPI_mbs_session_activity_status_NULL) {
    if (cJSON_AddStringToObject(item, "statusInfo", OpenAPI_mbs_session_activity_status_ToString(context_status_event_report->status_info)) == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [status_info]");
        goto end;
    }
    }

    if (context_status_event_report->mbs_service_area) {
    cJSON *mbs_service_area_local_JSON = OpenAPI_mbs_service_area_convertToJSON(context_status_event_report->mbs_service_area);
    if (mbs_service_area_local_JSON == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsServiceArea", mbs_service_area_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [mbs_service_area]");
        goto end;
    }
    }

    if (context_status_event_report->mbs_service_area_info_list) {
    cJSON *mbs_service_area_info_list = cJSON_AddObjectToObject(item, "mbsServiceAreaInfoList");
    if (mbs_service_area_info_list == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [mbs_service_area_info_list]");
        goto end;
    }
    cJSON *localMapObject = mbs_service_area_info_list;
    if (context_status_event_report->mbs_service_area_info_list) {
        OpenAPI_list_for_each(context_status_event_report->mbs_service_area_info_list, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            if (localKeyValue == NULL) {
                ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [mbs_service_area_info_list]");
                goto end;
            }
            if (localKeyValue->key == NULL) {
                ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [mbs_service_area_info_list]");
                goto end;
            }
            cJSON *itemLocal = localKeyValue->value ?
                OpenAPI_mbs_service_area_info_convertToJSON(localKeyValue->value) :
                cJSON_CreateNull();
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [inner]");
                goto end;
            }
            cJSON_AddItemToObject(localMapObject, localKeyValue->key, itemLocal);
        }
    }
    }

    if (context_status_event_report->multicast_trans_add_info) {
    cJSON *multicast_trans_add_info_local_JSON = OpenAPI_multicast_transport_address_change_info_convertToJSON(context_status_event_report->multicast_trans_add_info);
    if (multicast_trans_add_info_local_JSON == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [multicast_trans_add_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "multicastTransAddInfo", multicast_trans_add_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [multicast_trans_add_info]");
        goto end;
    }
    }

    if (context_status_event_report->mbs_security_context) {
    cJSON *mbs_security_context_local_JSON = OpenAPI_mbs_security_context_convertToJSON(context_status_event_report->mbs_security_context);
    if (mbs_security_context_local_JSON == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [mbs_security_context]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSecurityContext", mbs_security_context_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed [mbs_security_context]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_status_event_report_t *OpenAPI_context_status_event_report_parseFromJSON(cJSON *context_status_event_reportJSON)
{
    OpenAPI_context_status_event_report_t *context_status_event_report_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *event_type = NULL;
    OpenAPI_context_status_event_type_e event_typeVariable = 0;
    cJSON *time_stamp = NULL;
    cJSON *qos_info = NULL;
    OpenAPI_qos_info_t *qos_info_local_nonprim = NULL;
    cJSON *status_info = NULL;
    OpenAPI_mbs_session_activity_status_e status_infoVariable = 0;
    cJSON *mbs_service_area = NULL;
    OpenAPI_mbs_service_area_t *mbs_service_area_local_nonprim = NULL;
    cJSON *mbs_service_area_info_list = NULL;
    OpenAPI_list_t *mbs_service_area_info_listList = NULL;
    cJSON *multicast_trans_add_info = NULL;
    OpenAPI_multicast_transport_address_change_info_t *multicast_trans_add_info_local_nonprim = NULL;
    cJSON *mbs_security_context = NULL;
    OpenAPI_mbs_security_context_t *mbs_security_context_local_nonprim = NULL;
    event_type = cJSON_GetObjectItemCaseSensitive(context_status_event_reportJSON, "eventType");
    if (!event_type) {
        ogs_error("OpenAPI_context_status_event_report_parseFromJSON() failed [event_type]");
        goto end;
    }
    if (!cJSON_IsString(event_type)) {
        ogs_error("OpenAPI_context_status_event_report_parseFromJSON() failed [event_type]");
        goto end;
    }
    event_typeVariable = OpenAPI_context_status_event_type_FromString(event_type->valuestring);

    time_stamp = cJSON_GetObjectItemCaseSensitive(context_status_event_reportJSON, "timeStamp");
    if (!time_stamp) {
        ogs_error("OpenAPI_context_status_event_report_parseFromJSON() failed [time_stamp]");
        goto end;
    }
    if (!cJSON_IsString(time_stamp) && !cJSON_IsNull(time_stamp)) {
        ogs_error("OpenAPI_context_status_event_report_parseFromJSON() failed [time_stamp]");
        goto end;
    }

    qos_info = cJSON_GetObjectItemCaseSensitive(context_status_event_reportJSON, "qosInfo");
    if (qos_info) {
    qos_info_local_nonprim = OpenAPI_qos_info_parseFromJSON(qos_info);
    if (!qos_info_local_nonprim) {
        ogs_error("OpenAPI_qos_info_parseFromJSON failed [qos_info]");
        goto end;
    }
    }

    status_info = cJSON_GetObjectItemCaseSensitive(context_status_event_reportJSON, "statusInfo");
    if (status_info) {
    if (!cJSON_IsString(status_info)) {
        ogs_error("OpenAPI_context_status_event_report_parseFromJSON() failed [status_info]");
        goto end;
    }
    status_infoVariable = OpenAPI_mbs_session_activity_status_FromString(status_info->valuestring);
    }

    mbs_service_area = cJSON_GetObjectItemCaseSensitive(context_status_event_reportJSON, "mbsServiceArea");
    if (mbs_service_area) {
    mbs_service_area_local_nonprim = OpenAPI_mbs_service_area_parseFromJSON(mbs_service_area);
    if (!mbs_service_area_local_nonprim) {
        ogs_error("OpenAPI_mbs_service_area_parseFromJSON failed [mbs_service_area]");
        goto end;
    }
    }

    mbs_service_area_info_list = cJSON_GetObjectItemCaseSensitive(context_status_event_reportJSON, "mbsServiceAreaInfoList");
    if (mbs_service_area_info_list) {
        cJSON *mbs_service_area_info_list_local_map = NULL;
        if (!cJSON_IsObject(mbs_service_area_info_list) && !cJSON_IsNull(mbs_service_area_info_list)) {
            ogs_error("OpenAPI_context_status_event_report_parseFromJSON() failed [mbs_service_area_info_list]");
            goto end;
        }
        if (cJSON_IsObject(mbs_service_area_info_list)) {
            mbs_service_area_info_listList = OpenAPI_list_create();
            OpenAPI_map_t *localMapKeyPair = NULL;
            cJSON_ArrayForEach(mbs_service_area_info_list_local_map, mbs_service_area_info_list) {
                cJSON *localMapObject = mbs_service_area_info_list_local_map;
                if (cJSON_IsObject(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(
                        ogs_strdup(localMapObject->string), OpenAPI_mbs_service_area_info_parseFromJSON(localMapObject));
                } else if (cJSON_IsNull(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(ogs_strdup(localMapObject->string), NULL);
                } else {
                    ogs_error("OpenAPI_context_status_event_report_parseFromJSON() failed [inner]");
                    goto end;
                }
                OpenAPI_list_add(mbs_service_area_info_listList, localMapKeyPair);
            }
        }
    }

    multicast_trans_add_info = cJSON_GetObjectItemCaseSensitive(context_status_event_reportJSON, "multicastTransAddInfo");
    if (multicast_trans_add_info) {
    multicast_trans_add_info_local_nonprim = OpenAPI_multicast_transport_address_change_info_parseFromJSON(multicast_trans_add_info);
    if (!multicast_trans_add_info_local_nonprim) {
        ogs_error("OpenAPI_multicast_transport_address_change_info_parseFromJSON failed [multicast_trans_add_info]");
        goto end;
    }
    }

    mbs_security_context = cJSON_GetObjectItemCaseSensitive(context_status_event_reportJSON, "mbsSecurityContext");
    if (mbs_security_context) {
    mbs_security_context_local_nonprim = OpenAPI_mbs_security_context_parseFromJSON(mbs_security_context);
    if (!mbs_security_context_local_nonprim) {
        ogs_error("OpenAPI_mbs_security_context_parseFromJSON failed [mbs_security_context]");
        goto end;
    }
    }

    context_status_event_report_local_var = OpenAPI_context_status_event_report_create (
        event_typeVariable,
        ogs_strdup(time_stamp->valuestring),
        qos_info ? qos_info_local_nonprim : NULL,
        status_info ? status_infoVariable : 0,
        mbs_service_area ? mbs_service_area_local_nonprim : NULL,
        mbs_service_area_info_list ? mbs_service_area_info_listList : NULL,
        multicast_trans_add_info ? multicast_trans_add_info_local_nonprim : NULL,
        mbs_security_context ? mbs_security_context_local_nonprim : NULL
    );

    return context_status_event_report_local_var;
end:
    if (qos_info_local_nonprim) {
        OpenAPI_qos_info_free(qos_info_local_nonprim);
        qos_info_local_nonprim = NULL;
    }
    if (mbs_service_area_local_nonprim) {
        OpenAPI_mbs_service_area_free(mbs_service_area_local_nonprim);
        mbs_service_area_local_nonprim = NULL;
    }
    if (mbs_service_area_info_listList) {
        OpenAPI_list_for_each(mbs_service_area_info_listList, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*) node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_service_area_info_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_service_area_info_listList);
        mbs_service_area_info_listList = NULL;
    }
    if (multicast_trans_add_info_local_nonprim) {
        OpenAPI_multicast_transport_address_change_info_free(multicast_trans_add_info_local_nonprim);
        multicast_trans_add_info_local_nonprim = NULL;
    }
    if (mbs_security_context_local_nonprim) {
        OpenAPI_mbs_security_context_free(mbs_security_context_local_nonprim);
        mbs_security_context_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_context_status_event_report_t *OpenAPI_context_status_event_report_copy(OpenAPI_context_status_event_report_t *dst, OpenAPI_context_status_event_report_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_status_event_report_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_status_event_report_convertToJSON() failed");
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

    OpenAPI_context_status_event_report_free(dst);
    dst = OpenAPI_context_status_event_report_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

