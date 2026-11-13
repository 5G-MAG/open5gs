
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_qos_char.h"

OpenAPI_mbs_qos_char_t *OpenAPI_mbs_qos_char_create(
    int _5qi,
    int priority_level,
    OpenAPI_qos_resource_type_e resource_type,
    int packet_delay_budget,
    char *packet_error_rate,
    bool is_aver_window,
    int aver_window,
    int mbs_max_data_burst_vol
)
{
    OpenAPI_mbs_qos_char_t *mbs_qos_char_local_var = ogs_malloc(sizeof(OpenAPI_mbs_qos_char_t));
    ogs_assert(mbs_qos_char_local_var);

    mbs_qos_char_local_var->_5qi = _5qi;
    mbs_qos_char_local_var->priority_level = priority_level;
    mbs_qos_char_local_var->resource_type = resource_type;
    mbs_qos_char_local_var->packet_delay_budget = packet_delay_budget;
    mbs_qos_char_local_var->packet_error_rate = packet_error_rate;
    mbs_qos_char_local_var->is_aver_window = is_aver_window;
    mbs_qos_char_local_var->aver_window = aver_window;
    mbs_qos_char_local_var->mbs_max_data_burst_vol = mbs_max_data_burst_vol;

    return mbs_qos_char_local_var;
}

void OpenAPI_mbs_qos_char_free(OpenAPI_mbs_qos_char_t *mbs_qos_char)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_qos_char) {
        return;
    }
    if (mbs_qos_char->packet_error_rate) {
        ogs_free(mbs_qos_char->packet_error_rate);
        mbs_qos_char->packet_error_rate = NULL;
    }
    ogs_free(mbs_qos_char);
}

cJSON *OpenAPI_mbs_qos_char_convertToJSON(OpenAPI_mbs_qos_char_t *mbs_qos_char)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_qos_char == NULL) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [MbsQosChar]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (cJSON_AddNumberToObject(item, "5qi", mbs_qos_char->_5qi) == NULL) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [_5qi]");
        goto end;
    }

    if (cJSON_AddNumberToObject(item, "priorityLevel", mbs_qos_char->priority_level) == NULL) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [priority_level]");
        goto end;
    }

    if (mbs_qos_char->resource_type == OpenAPI_qos_resource_type_NULL) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [resource_type]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "resourceType", OpenAPI_qos_resource_type_ToString(mbs_qos_char->resource_type)) == NULL) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [resource_type]");
        goto end;
    }

    if (cJSON_AddNumberToObject(item, "packetDelayBudget", mbs_qos_char->packet_delay_budget) == NULL) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [packet_delay_budget]");
        goto end;
    }

    if (!mbs_qos_char->packet_error_rate) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [packet_error_rate]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "packetErrorRate", mbs_qos_char->packet_error_rate) == NULL) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [packet_error_rate]");
        goto end;
    }

    if (mbs_qos_char->is_aver_window) {
    if (cJSON_AddNumberToObject(item, "averWindow", mbs_qos_char->aver_window) == NULL) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [aver_window]");
        goto end;
    }
    }

    if (cJSON_AddNumberToObject(item, "mbsMaxDataBurstVol", mbs_qos_char->mbs_max_data_burst_vol) == NULL) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed [mbs_max_data_burst_vol]");
        goto end;
    }

end:
    return item;
}

OpenAPI_mbs_qos_char_t *OpenAPI_mbs_qos_char_parseFromJSON(cJSON *mbs_qos_charJSON)
{
    OpenAPI_mbs_qos_char_t *mbs_qos_char_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *_5qi = NULL;
    cJSON *priority_level = NULL;
    cJSON *resource_type = NULL;
    OpenAPI_qos_resource_type_e resource_typeVariable = 0;
    cJSON *packet_delay_budget = NULL;
    cJSON *packet_error_rate = NULL;
    cJSON *aver_window = NULL;
    cJSON *mbs_max_data_burst_vol = NULL;
    _5qi = cJSON_GetObjectItemCaseSensitive(mbs_qos_charJSON, "5qi");
    if (!_5qi) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [_5qi]");
        goto end;
    }
    if (!cJSON_IsNumber(_5qi)) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [_5qi]");
        goto end;
    }

    priority_level = cJSON_GetObjectItemCaseSensitive(mbs_qos_charJSON, "priorityLevel");
    if (!priority_level) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [priority_level]");
        goto end;
    }
    if (!cJSON_IsNumber(priority_level)) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [priority_level]");
        goto end;
    }

    resource_type = cJSON_GetObjectItemCaseSensitive(mbs_qos_charJSON, "resourceType");
    if (!resource_type) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [resource_type]");
        goto end;
    }
    if (!cJSON_IsString(resource_type)) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [resource_type]");
        goto end;
    }
    resource_typeVariable = OpenAPI_qos_resource_type_FromString(resource_type->valuestring);

    packet_delay_budget = cJSON_GetObjectItemCaseSensitive(mbs_qos_charJSON, "packetDelayBudget");
    if (!packet_delay_budget) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [packet_delay_budget]");
        goto end;
    }
    if (!cJSON_IsNumber(packet_delay_budget)) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [packet_delay_budget]");
        goto end;
    }

    packet_error_rate = cJSON_GetObjectItemCaseSensitive(mbs_qos_charJSON, "packetErrorRate");
    if (!packet_error_rate) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [packet_error_rate]");
        goto end;
    }
    if (!cJSON_IsString(packet_error_rate)) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [packet_error_rate]");
        goto end;
    }

    aver_window = cJSON_GetObjectItemCaseSensitive(mbs_qos_charJSON, "averWindow");
    if (aver_window) {
    if (!cJSON_IsNumber(aver_window)) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [aver_window]");
        goto end;
    }
    }

    mbs_max_data_burst_vol = cJSON_GetObjectItemCaseSensitive(mbs_qos_charJSON, "mbsMaxDataBurstVol");
    if (!mbs_max_data_burst_vol) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [mbs_max_data_burst_vol]");
        goto end;
    }
    if (!cJSON_IsNumber(mbs_max_data_burst_vol)) {
        ogs_error("OpenAPI_mbs_qos_char_parseFromJSON() failed [mbs_max_data_burst_vol]");
        goto end;
    }

    mbs_qos_char_local_var = OpenAPI_mbs_qos_char_create (
        
        _5qi->valuedouble,
        
        priority_level->valuedouble,
        resource_typeVariable,
        
        packet_delay_budget->valuedouble,
        ogs_strdup(packet_error_rate->valuestring),
        aver_window ? true : false,
        aver_window ? aver_window->valuedouble : 0,
        
        mbs_max_data_burst_vol->valuedouble
    );

    return mbs_qos_char_local_var;
end:
    return NULL;
}

OpenAPI_mbs_qos_char_t *OpenAPI_mbs_qos_char_copy(OpenAPI_mbs_qos_char_t *dst, OpenAPI_mbs_qos_char_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_qos_char_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_qos_char_convertToJSON() failed");
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

    OpenAPI_mbs_qos_char_free(dst);
    dst = OpenAPI_mbs_qos_char_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

