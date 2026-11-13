
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_qos_dec.h"

OpenAPI_mbs_qos_dec_t *OpenAPI_mbs_qos_dec_create(
    char *mbs_qos_id,
    bool is__5qi,
    int _5qi,
    bool is_priority_level,
    int priority_level,
    char *mbr_dl,
    char *gbr_dl,
    OpenAPI_arp_t *arp,
    bool is_aver_window,
    int aver_window,
    bool is_mbs_max_data_burst_vol,
    int mbs_max_data_burst_vol
)
{
    OpenAPI_mbs_qos_dec_t *mbs_qos_dec_local_var = ogs_malloc(sizeof(OpenAPI_mbs_qos_dec_t));
    ogs_assert(mbs_qos_dec_local_var);

    mbs_qos_dec_local_var->mbs_qos_id = mbs_qos_id;
    mbs_qos_dec_local_var->is__5qi = is__5qi;
    mbs_qos_dec_local_var->_5qi = _5qi;
    mbs_qos_dec_local_var->is_priority_level = is_priority_level;
    mbs_qos_dec_local_var->priority_level = priority_level;
    mbs_qos_dec_local_var->mbr_dl = mbr_dl;
    mbs_qos_dec_local_var->gbr_dl = gbr_dl;
    mbs_qos_dec_local_var->arp = arp;
    mbs_qos_dec_local_var->is_aver_window = is_aver_window;
    mbs_qos_dec_local_var->aver_window = aver_window;
    mbs_qos_dec_local_var->is_mbs_max_data_burst_vol = is_mbs_max_data_burst_vol;
    mbs_qos_dec_local_var->mbs_max_data_burst_vol = mbs_max_data_burst_vol;

    return mbs_qos_dec_local_var;
}

void OpenAPI_mbs_qos_dec_free(OpenAPI_mbs_qos_dec_t *mbs_qos_dec)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_qos_dec) {
        return;
    }
    if (mbs_qos_dec->mbs_qos_id) {
        ogs_free(mbs_qos_dec->mbs_qos_id);
        mbs_qos_dec->mbs_qos_id = NULL;
    }
    if (mbs_qos_dec->mbr_dl) {
        ogs_free(mbs_qos_dec->mbr_dl);
        mbs_qos_dec->mbr_dl = NULL;
    }
    if (mbs_qos_dec->gbr_dl) {
        ogs_free(mbs_qos_dec->gbr_dl);
        mbs_qos_dec->gbr_dl = NULL;
    }
    if (mbs_qos_dec->arp) {
        OpenAPI_arp_free(mbs_qos_dec->arp);
        mbs_qos_dec->arp = NULL;
    }
    ogs_free(mbs_qos_dec);
}

cJSON *OpenAPI_mbs_qos_dec_convertToJSON(OpenAPI_mbs_qos_dec_t *mbs_qos_dec)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_qos_dec == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [MbsQosDec]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_qos_dec->mbs_qos_id) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [mbs_qos_id]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "mbsQosId", mbs_qos_dec->mbs_qos_id) == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [mbs_qos_id]");
        goto end;
    }

    if (mbs_qos_dec->is__5qi) {
    if (cJSON_AddNumberToObject(item, "5qi", mbs_qos_dec->_5qi) == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [_5qi]");
        goto end;
    }
    }

    if (mbs_qos_dec->is_priority_level) {
    if (cJSON_AddNumberToObject(item, "priorityLevel", mbs_qos_dec->priority_level) == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [priority_level]");
        goto end;
    }
    }

    if (mbs_qos_dec->mbr_dl) {
    if (cJSON_AddStringToObject(item, "mbrDl", mbs_qos_dec->mbr_dl) == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [mbr_dl]");
        goto end;
    }
    }

    if (mbs_qos_dec->gbr_dl) {
    if (cJSON_AddStringToObject(item, "gbrDl", mbs_qos_dec->gbr_dl) == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [gbr_dl]");
        goto end;
    }
    }

    if (mbs_qos_dec->arp) {
    cJSON *arp_local_JSON = OpenAPI_arp_convertToJSON(mbs_qos_dec->arp);
    if (arp_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [arp]");
        goto end;
    }
    cJSON_AddItemToObject(item, "arp", arp_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [arp]");
        goto end;
    }
    }

    if (mbs_qos_dec->is_aver_window) {
    if (cJSON_AddNumberToObject(item, "averWindow", mbs_qos_dec->aver_window) == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [aver_window]");
        goto end;
    }
    }

    if (mbs_qos_dec->is_mbs_max_data_burst_vol) {
    if (cJSON_AddNumberToObject(item, "mbsMaxDataBurstVol", mbs_qos_dec->mbs_max_data_burst_vol) == NULL) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed [mbs_max_data_burst_vol]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_qos_dec_t *OpenAPI_mbs_qos_dec_parseFromJSON(cJSON *mbs_qos_decJSON)
{
    OpenAPI_mbs_qos_dec_t *mbs_qos_dec_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_qos_id = NULL;
    cJSON *_5qi = NULL;
    cJSON *priority_level = NULL;
    cJSON *mbr_dl = NULL;
    cJSON *gbr_dl = NULL;
    cJSON *arp = NULL;
    OpenAPI_arp_t *arp_local_nonprim = NULL;
    cJSON *aver_window = NULL;
    cJSON *mbs_max_data_burst_vol = NULL;
    mbs_qos_id = cJSON_GetObjectItemCaseSensitive(mbs_qos_decJSON, "mbsQosId");
    if (!mbs_qos_id) {
        ogs_error("OpenAPI_mbs_qos_dec_parseFromJSON() failed [mbs_qos_id]");
        goto end;
    }
    if (!cJSON_IsString(mbs_qos_id)) {
        ogs_error("OpenAPI_mbs_qos_dec_parseFromJSON() failed [mbs_qos_id]");
        goto end;
    }

    _5qi = cJSON_GetObjectItemCaseSensitive(mbs_qos_decJSON, "5qi");
    if (_5qi) {
    if (!cJSON_IsNumber(_5qi)) {
        ogs_error("OpenAPI_mbs_qos_dec_parseFromJSON() failed [_5qi]");
        goto end;
    }
    }

    priority_level = cJSON_GetObjectItemCaseSensitive(mbs_qos_decJSON, "priorityLevel");
    if (priority_level) {
    if (!cJSON_IsNumber(priority_level)) {
        ogs_error("OpenAPI_mbs_qos_dec_parseFromJSON() failed [priority_level]");
        goto end;
    }
    }

    mbr_dl = cJSON_GetObjectItemCaseSensitive(mbs_qos_decJSON, "mbrDl");
    if (mbr_dl) {
    if (!cJSON_IsString(mbr_dl) && !cJSON_IsNull(mbr_dl)) {
        ogs_error("OpenAPI_mbs_qos_dec_parseFromJSON() failed [mbr_dl]");
        goto end;
    }
    }

    gbr_dl = cJSON_GetObjectItemCaseSensitive(mbs_qos_decJSON, "gbrDl");
    if (gbr_dl) {
    if (!cJSON_IsString(gbr_dl) && !cJSON_IsNull(gbr_dl)) {
        ogs_error("OpenAPI_mbs_qos_dec_parseFromJSON() failed [gbr_dl]");
        goto end;
    }
    }

    arp = cJSON_GetObjectItemCaseSensitive(mbs_qos_decJSON, "arp");
    if (arp) {
    arp_local_nonprim = OpenAPI_arp_parseFromJSON(arp);
    if (!arp_local_nonprim) {
        ogs_error("OpenAPI_arp_parseFromJSON failed [arp]");
        goto end;
    }
    }

    aver_window = cJSON_GetObjectItemCaseSensitive(mbs_qos_decJSON, "averWindow");
    if (aver_window) {
    if (!cJSON_IsNumber(aver_window)) {
        ogs_error("OpenAPI_mbs_qos_dec_parseFromJSON() failed [aver_window]");
        goto end;
    }
    }

    mbs_max_data_burst_vol = cJSON_GetObjectItemCaseSensitive(mbs_qos_decJSON, "mbsMaxDataBurstVol");
    if (mbs_max_data_burst_vol) {
    if (!cJSON_IsNumber(mbs_max_data_burst_vol)) {
        ogs_error("OpenAPI_mbs_qos_dec_parseFromJSON() failed [mbs_max_data_burst_vol]");
        goto end;
    }
    }

    mbs_qos_dec_local_var = OpenAPI_mbs_qos_dec_create (
        ogs_strdup(mbs_qos_id->valuestring),
        _5qi ? true : false,
        _5qi ? _5qi->valuedouble : 0,
        priority_level ? true : false,
        priority_level ? priority_level->valuedouble : 0,
        mbr_dl && !cJSON_IsNull(mbr_dl) ? ogs_strdup(mbr_dl->valuestring) : NULL,
        gbr_dl && !cJSON_IsNull(gbr_dl) ? ogs_strdup(gbr_dl->valuestring) : NULL,
        arp ? arp_local_nonprim : NULL,
        aver_window ? true : false,
        aver_window ? aver_window->valuedouble : 0,
        mbs_max_data_burst_vol ? true : false,
        mbs_max_data_burst_vol ? mbs_max_data_burst_vol->valuedouble : 0
    );

    return mbs_qos_dec_local_var;
end:
    if (arp_local_nonprim) {
        OpenAPI_arp_free(arp_local_nonprim);
        arp_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_mbs_qos_dec_t *OpenAPI_mbs_qos_dec_copy(OpenAPI_mbs_qos_dec_t *dst, OpenAPI_mbs_qos_dec_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_qos_dec_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_qos_dec_convertToJSON() failed");
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

    OpenAPI_mbs_qos_dec_free(dst);
    dst = OpenAPI_mbs_qos_dec_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

