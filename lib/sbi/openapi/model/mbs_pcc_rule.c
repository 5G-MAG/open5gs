
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_pcc_rule.h"

OpenAPI_mbs_pcc_rule_t *OpenAPI_mbs_pcc_rule_create(
    char *mbs_pcc_rule_id,
    OpenAPI_list_t *mbs_dl_ip_flow_info,
    bool is_precedence,
    int precedence,
    OpenAPI_list_t *ref_mbs_qos_dec,
    bool is_ref_mbs_chg_data_null,
    OpenAPI_list_t *ref_mbs_chg_data
)
{
    OpenAPI_mbs_pcc_rule_t *mbs_pcc_rule_local_var = ogs_malloc(sizeof(OpenAPI_mbs_pcc_rule_t));
    ogs_assert(mbs_pcc_rule_local_var);

    mbs_pcc_rule_local_var->mbs_pcc_rule_id = mbs_pcc_rule_id;
    mbs_pcc_rule_local_var->mbs_dl_ip_flow_info = mbs_dl_ip_flow_info;
    mbs_pcc_rule_local_var->is_precedence = is_precedence;
    mbs_pcc_rule_local_var->precedence = precedence;
    mbs_pcc_rule_local_var->ref_mbs_qos_dec = ref_mbs_qos_dec;
    mbs_pcc_rule_local_var->is_ref_mbs_chg_data_null = is_ref_mbs_chg_data_null;
    mbs_pcc_rule_local_var->ref_mbs_chg_data = ref_mbs_chg_data;

    return mbs_pcc_rule_local_var;
}

void OpenAPI_mbs_pcc_rule_free(OpenAPI_mbs_pcc_rule_t *mbs_pcc_rule)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_pcc_rule) {
        return;
    }
    if (mbs_pcc_rule->mbs_pcc_rule_id) {
        ogs_free(mbs_pcc_rule->mbs_pcc_rule_id);
        mbs_pcc_rule->mbs_pcc_rule_id = NULL;
    }
    if (mbs_pcc_rule->mbs_dl_ip_flow_info) {
        OpenAPI_list_for_each(mbs_pcc_rule->mbs_dl_ip_flow_info, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_pcc_rule->mbs_dl_ip_flow_info);
        mbs_pcc_rule->mbs_dl_ip_flow_info = NULL;
    }
    if (mbs_pcc_rule->ref_mbs_qos_dec) {
        OpenAPI_list_for_each(mbs_pcc_rule->ref_mbs_qos_dec, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_pcc_rule->ref_mbs_qos_dec);
        mbs_pcc_rule->ref_mbs_qos_dec = NULL;
    }
    if (mbs_pcc_rule->ref_mbs_chg_data) {
        OpenAPI_list_for_each(mbs_pcc_rule->ref_mbs_chg_data, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_pcc_rule->ref_mbs_chg_data);
        mbs_pcc_rule->ref_mbs_chg_data = NULL;
    }
    ogs_free(mbs_pcc_rule);
}

cJSON *OpenAPI_mbs_pcc_rule_convertToJSON(OpenAPI_mbs_pcc_rule_t *mbs_pcc_rule)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_pcc_rule == NULL) {
        ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [MbsPccRule]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_pcc_rule->mbs_pcc_rule_id) {
        ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [mbs_pcc_rule_id]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "mbsPccRuleId", mbs_pcc_rule->mbs_pcc_rule_id) == NULL) {
        ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [mbs_pcc_rule_id]");
        goto end;
    }

    if (mbs_pcc_rule->mbs_dl_ip_flow_info) {
    cJSON *mbs_dl_ip_flow_infoList = cJSON_AddArrayToObject(item, "mbsDlIpFlowInfo");
    if (mbs_dl_ip_flow_infoList == NULL) {
        ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [mbs_dl_ip_flow_info]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_pcc_rule->mbs_dl_ip_flow_info, node) {
        if (cJSON_AddStringToObject(mbs_dl_ip_flow_infoList, "", (char*)node->data) == NULL) {
            ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [mbs_dl_ip_flow_info]");
            goto end;
        }
    }
    }

    if (mbs_pcc_rule->is_precedence) {
    if (cJSON_AddNumberToObject(item, "precedence", mbs_pcc_rule->precedence) == NULL) {
        ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [precedence]");
        goto end;
    }
    }

    if (mbs_pcc_rule->ref_mbs_qos_dec) {
    cJSON *ref_mbs_qos_decList = cJSON_AddArrayToObject(item, "refMbsQosDec");
    if (ref_mbs_qos_decList == NULL) {
        ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [ref_mbs_qos_dec]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_pcc_rule->ref_mbs_qos_dec, node) {
        if (cJSON_AddStringToObject(ref_mbs_qos_decList, "", (char*)node->data) == NULL) {
            ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [ref_mbs_qos_dec]");
            goto end;
        }
    }
    }

    if (mbs_pcc_rule->ref_mbs_chg_data) {
    cJSON *ref_mbs_chg_dataList = cJSON_AddArrayToObject(item, "refMbsChgData");
    if (ref_mbs_chg_dataList == NULL) {
        ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [ref_mbs_chg_data]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_pcc_rule->ref_mbs_chg_data, node) {
        if (cJSON_AddStringToObject(ref_mbs_chg_dataList, "", (char*)node->data) == NULL) {
            ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [ref_mbs_chg_data]");
            goto end;
        }
    }
    } else if (mbs_pcc_rule->is_ref_mbs_chg_data_null) {
        if (cJSON_AddNullToObject(item, "refMbsChgData") == NULL) {
            ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed [ref_mbs_chg_data]");
            goto end;
        }
    }

end:
    return item;
}

OpenAPI_mbs_pcc_rule_t *OpenAPI_mbs_pcc_rule_parseFromJSON(cJSON *mbs_pcc_ruleJSON)
{
    OpenAPI_mbs_pcc_rule_t *mbs_pcc_rule_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_pcc_rule_id = NULL;
    cJSON *mbs_dl_ip_flow_info = NULL;
    OpenAPI_list_t *mbs_dl_ip_flow_infoList = NULL;
    cJSON *precedence = NULL;
    cJSON *ref_mbs_qos_dec = NULL;
    OpenAPI_list_t *ref_mbs_qos_decList = NULL;
    cJSON *ref_mbs_chg_data = NULL;
    OpenAPI_list_t *ref_mbs_chg_dataList = NULL;
    mbs_pcc_rule_id = cJSON_GetObjectItemCaseSensitive(mbs_pcc_ruleJSON, "mbsPccRuleId");
    if (!mbs_pcc_rule_id) {
        ogs_error("OpenAPI_mbs_pcc_rule_parseFromJSON() failed [mbs_pcc_rule_id]");
        goto end;
    }
    if (!cJSON_IsString(mbs_pcc_rule_id)) {
        ogs_error("OpenAPI_mbs_pcc_rule_parseFromJSON() failed [mbs_pcc_rule_id]");
        goto end;
    }

    mbs_dl_ip_flow_info = cJSON_GetObjectItemCaseSensitive(mbs_pcc_ruleJSON, "mbsDlIpFlowInfo");
    if (mbs_dl_ip_flow_info) {
        cJSON *mbs_dl_ip_flow_info_local = NULL;
        if (!cJSON_IsArray(mbs_dl_ip_flow_info)) {
            ogs_error("OpenAPI_mbs_pcc_rule_parseFromJSON() failed [mbs_dl_ip_flow_info]");
            goto end;
        }

        mbs_dl_ip_flow_infoList = OpenAPI_list_create();

        cJSON_ArrayForEach(mbs_dl_ip_flow_info_local, mbs_dl_ip_flow_info) {
            double *localDouble = NULL;
            int *localInt = NULL;
            if (!cJSON_IsString(mbs_dl_ip_flow_info_local)) {
                ogs_error("OpenAPI_mbs_pcc_rule_parseFromJSON() failed [mbs_dl_ip_flow_info]");
                goto end;
            }
            OpenAPI_list_add(mbs_dl_ip_flow_infoList, ogs_strdup(mbs_dl_ip_flow_info_local->valuestring));
        }
    }

    precedence = cJSON_GetObjectItemCaseSensitive(mbs_pcc_ruleJSON, "precedence");
    if (precedence) {
    if (!cJSON_IsNumber(precedence)) {
        ogs_error("OpenAPI_mbs_pcc_rule_parseFromJSON() failed [precedence]");
        goto end;
    }
    }

    ref_mbs_qos_dec = cJSON_GetObjectItemCaseSensitive(mbs_pcc_ruleJSON, "refMbsQosDec");
    if (ref_mbs_qos_dec) {
        cJSON *ref_mbs_qos_dec_local = NULL;
        if (!cJSON_IsArray(ref_mbs_qos_dec)) {
            ogs_error("OpenAPI_mbs_pcc_rule_parseFromJSON() failed [ref_mbs_qos_dec]");
            goto end;
        }

        ref_mbs_qos_decList = OpenAPI_list_create();

        cJSON_ArrayForEach(ref_mbs_qos_dec_local, ref_mbs_qos_dec) {
            double *localDouble = NULL;
            int *localInt = NULL;
            if (!cJSON_IsString(ref_mbs_qos_dec_local)) {
                ogs_error("OpenAPI_mbs_pcc_rule_parseFromJSON() failed [ref_mbs_qos_dec]");
                goto end;
            }
            OpenAPI_list_add(ref_mbs_qos_decList, ogs_strdup(ref_mbs_qos_dec_local->valuestring));
        }
    }

    ref_mbs_chg_data = cJSON_GetObjectItemCaseSensitive(mbs_pcc_ruleJSON, "refMbsChgData");
    if (ref_mbs_chg_data) {
    if (!cJSON_IsNull(ref_mbs_chg_data)) {
        cJSON *ref_mbs_chg_data_local = NULL;
        if (!cJSON_IsArray(ref_mbs_chg_data)) {
            ogs_error("OpenAPI_mbs_pcc_rule_parseFromJSON() failed [ref_mbs_chg_data]");
            goto end;
        }

        ref_mbs_chg_dataList = OpenAPI_list_create();

        cJSON_ArrayForEach(ref_mbs_chg_data_local, ref_mbs_chg_data) {
            double *localDouble = NULL;
            int *localInt = NULL;
            if (!cJSON_IsString(ref_mbs_chg_data_local)) {
                ogs_error("OpenAPI_mbs_pcc_rule_parseFromJSON() failed [ref_mbs_chg_data]");
                goto end;
            }
            OpenAPI_list_add(ref_mbs_chg_dataList, ogs_strdup(ref_mbs_chg_data_local->valuestring));
        }
    }
    }

    mbs_pcc_rule_local_var = OpenAPI_mbs_pcc_rule_create (
        ogs_strdup(mbs_pcc_rule_id->valuestring),
        mbs_dl_ip_flow_info ? mbs_dl_ip_flow_infoList : NULL,
        precedence ? true : false,
        precedence ? precedence->valuedouble : 0,
        ref_mbs_qos_dec ? ref_mbs_qos_decList : NULL,
        ref_mbs_chg_data && cJSON_IsNull(ref_mbs_chg_data) ? true : false,
        ref_mbs_chg_data ? ref_mbs_chg_dataList : NULL
    );

    return mbs_pcc_rule_local_var;
end:
    if (mbs_dl_ip_flow_infoList) {
        OpenAPI_list_for_each(mbs_dl_ip_flow_infoList, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_dl_ip_flow_infoList);
        mbs_dl_ip_flow_infoList = NULL;
    }
    if (ref_mbs_qos_decList) {
        OpenAPI_list_for_each(ref_mbs_qos_decList, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(ref_mbs_qos_decList);
        ref_mbs_qos_decList = NULL;
    }
    if (ref_mbs_chg_dataList) {
        OpenAPI_list_for_each(ref_mbs_chg_dataList, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(ref_mbs_chg_dataList);
        ref_mbs_chg_dataList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_pcc_rule_t *OpenAPI_mbs_pcc_rule_copy(OpenAPI_mbs_pcc_rule_t *dst, OpenAPI_mbs_pcc_rule_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_pcc_rule_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_pcc_rule_convertToJSON() failed");
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

    OpenAPI_mbs_pcc_rule_free(dst);
    dst = OpenAPI_mbs_pcc_rule_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

