
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_qo_s_req.h"

OpenAPI_mbs_qo_s_req_t *OpenAPI_mbs_qo_s_req_create(
    int _5qi,
    char *guar_bit_rate,
    char *max_bit_rate,
    bool is_aver_window,
    int aver_window,
    OpenAPI_arp_t *req_mbs_arp
)
{
    OpenAPI_mbs_qo_s_req_t *mbs_qo_s_req_local_var = ogs_malloc(sizeof(OpenAPI_mbs_qo_s_req_t));
    ogs_assert(mbs_qo_s_req_local_var);

    mbs_qo_s_req_local_var->_5qi = _5qi;
    mbs_qo_s_req_local_var->guar_bit_rate = guar_bit_rate;
    mbs_qo_s_req_local_var->max_bit_rate = max_bit_rate;
    mbs_qo_s_req_local_var->is_aver_window = is_aver_window;
    mbs_qo_s_req_local_var->aver_window = aver_window;
    mbs_qo_s_req_local_var->req_mbs_arp = req_mbs_arp;

    return mbs_qo_s_req_local_var;
}

void OpenAPI_mbs_qo_s_req_free(OpenAPI_mbs_qo_s_req_t *mbs_qo_s_req)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_qo_s_req) {
        return;
    }
    if (mbs_qo_s_req->guar_bit_rate) {
        ogs_free(mbs_qo_s_req->guar_bit_rate);
        mbs_qo_s_req->guar_bit_rate = NULL;
    }
    if (mbs_qo_s_req->max_bit_rate) {
        ogs_free(mbs_qo_s_req->max_bit_rate);
        mbs_qo_s_req->max_bit_rate = NULL;
    }
    if (mbs_qo_s_req->req_mbs_arp) {
        OpenAPI_arp_free(mbs_qo_s_req->req_mbs_arp);
        mbs_qo_s_req->req_mbs_arp = NULL;
    }
    ogs_free(mbs_qo_s_req);
}

cJSON *OpenAPI_mbs_qo_s_req_convertToJSON(OpenAPI_mbs_qo_s_req_t *mbs_qo_s_req)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_qo_s_req == NULL) {
        ogs_error("OpenAPI_mbs_qo_s_req_convertToJSON() failed [MbsQoSReq]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (cJSON_AddNumberToObject(item, "5qi", mbs_qo_s_req->_5qi) == NULL) {
        ogs_error("OpenAPI_mbs_qo_s_req_convertToJSON() failed [_5qi]");
        goto end;
    }

    if (mbs_qo_s_req->guar_bit_rate) {
    if (cJSON_AddStringToObject(item, "guarBitRate", mbs_qo_s_req->guar_bit_rate) == NULL) {
        ogs_error("OpenAPI_mbs_qo_s_req_convertToJSON() failed [guar_bit_rate]");
        goto end;
    }
    }

    if (mbs_qo_s_req->max_bit_rate) {
    if (cJSON_AddStringToObject(item, "maxBitRate", mbs_qo_s_req->max_bit_rate) == NULL) {
        ogs_error("OpenAPI_mbs_qo_s_req_convertToJSON() failed [max_bit_rate]");
        goto end;
    }
    }

    if (mbs_qo_s_req->is_aver_window) {
    if (cJSON_AddNumberToObject(item, "averWindow", mbs_qo_s_req->aver_window) == NULL) {
        ogs_error("OpenAPI_mbs_qo_s_req_convertToJSON() failed [aver_window]");
        goto end;
    }
    }

    if (mbs_qo_s_req->req_mbs_arp) {
    cJSON *req_mbs_arp_local_JSON = OpenAPI_arp_convertToJSON(mbs_qo_s_req->req_mbs_arp);
    if (req_mbs_arp_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_qo_s_req_convertToJSON() failed [req_mbs_arp]");
        goto end;
    }
    cJSON_AddItemToObject(item, "reqMbsArp", req_mbs_arp_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_qo_s_req_convertToJSON() failed [req_mbs_arp]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_qo_s_req_t *OpenAPI_mbs_qo_s_req_parseFromJSON(cJSON *mbs_qo_s_reqJSON)
{
    OpenAPI_mbs_qo_s_req_t *mbs_qo_s_req_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *_5qi = NULL;
    cJSON *guar_bit_rate = NULL;
    cJSON *max_bit_rate = NULL;
    cJSON *aver_window = NULL;
    cJSON *req_mbs_arp = NULL;
    OpenAPI_arp_t *req_mbs_arp_local_nonprim = NULL;
    _5qi = cJSON_GetObjectItemCaseSensitive(mbs_qo_s_reqJSON, "5qi");
    if (!_5qi) {
        ogs_error("OpenAPI_mbs_qo_s_req_parseFromJSON() failed [_5qi]");
        goto end;
    }
    if (!cJSON_IsNumber(_5qi)) {
        ogs_error("OpenAPI_mbs_qo_s_req_parseFromJSON() failed [_5qi]");
        goto end;
    }

    guar_bit_rate = cJSON_GetObjectItemCaseSensitive(mbs_qo_s_reqJSON, "guarBitRate");
    if (guar_bit_rate) {
    if (!cJSON_IsString(guar_bit_rate) && !cJSON_IsNull(guar_bit_rate)) {
        ogs_error("OpenAPI_mbs_qo_s_req_parseFromJSON() failed [guar_bit_rate]");
        goto end;
    }
    }

    max_bit_rate = cJSON_GetObjectItemCaseSensitive(mbs_qo_s_reqJSON, "maxBitRate");
    if (max_bit_rate) {
    if (!cJSON_IsString(max_bit_rate) && !cJSON_IsNull(max_bit_rate)) {
        ogs_error("OpenAPI_mbs_qo_s_req_parseFromJSON() failed [max_bit_rate]");
        goto end;
    }
    }

    aver_window = cJSON_GetObjectItemCaseSensitive(mbs_qo_s_reqJSON, "averWindow");
    if (aver_window) {
    if (!cJSON_IsNumber(aver_window)) {
        ogs_error("OpenAPI_mbs_qo_s_req_parseFromJSON() failed [aver_window]");
        goto end;
    }
    }

    req_mbs_arp = cJSON_GetObjectItemCaseSensitive(mbs_qo_s_reqJSON, "reqMbsArp");
    if (req_mbs_arp) {
    req_mbs_arp_local_nonprim = OpenAPI_arp_parseFromJSON(req_mbs_arp);
    if (!req_mbs_arp_local_nonprim) {
        ogs_error("OpenAPI_arp_parseFromJSON failed [req_mbs_arp]");
        goto end;
    }
    }

    mbs_qo_s_req_local_var = OpenAPI_mbs_qo_s_req_create (
        
        _5qi->valuedouble,
        guar_bit_rate && !cJSON_IsNull(guar_bit_rate) ? ogs_strdup(guar_bit_rate->valuestring) : NULL,
        max_bit_rate && !cJSON_IsNull(max_bit_rate) ? ogs_strdup(max_bit_rate->valuestring) : NULL,
        aver_window ? true : false,
        aver_window ? aver_window->valuedouble : 0,
        req_mbs_arp ? req_mbs_arp_local_nonprim : NULL
    );

    return mbs_qo_s_req_local_var;
end:
    if (req_mbs_arp_local_nonprim) {
        OpenAPI_arp_free(req_mbs_arp_local_nonprim);
        req_mbs_arp_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_mbs_qo_s_req_t *OpenAPI_mbs_qo_s_req_copy(OpenAPI_mbs_qo_s_req_t *dst, OpenAPI_mbs_qo_s_req_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_qo_s_req_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_qo_s_req_convertToJSON() failed");
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

    OpenAPI_mbs_qo_s_req_free(dst);
    dst = OpenAPI_mbs_qo_s_req_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

