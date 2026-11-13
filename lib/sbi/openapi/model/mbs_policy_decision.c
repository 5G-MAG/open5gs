
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_policy_decision.h"

OpenAPI_mbs_policy_decision_t *OpenAPI_mbs_policy_decision_create(
    bool is_mbs_pcc_rules_null,
    OpenAPI_list_t* mbs_pcc_rules,
    OpenAPI_list_t* mbs_qos_decs,
    OpenAPI_list_t* mbs_qos_chars,
    char *auth_mbs_sess_ambr,
    bool is_mbs_pcrts_null,
    OpenAPI_list_t *mbs_pcrts,
    bool is_mbs_charging_data_null,
    OpenAPI_list_t* mbs_charging_data
)
{
    OpenAPI_mbs_policy_decision_t *mbs_policy_decision_local_var = ogs_malloc(sizeof(OpenAPI_mbs_policy_decision_t));
    ogs_assert(mbs_policy_decision_local_var);

    mbs_policy_decision_local_var->is_mbs_pcc_rules_null = is_mbs_pcc_rules_null;
    mbs_policy_decision_local_var->mbs_pcc_rules = mbs_pcc_rules;
    mbs_policy_decision_local_var->mbs_qos_decs = mbs_qos_decs;
    mbs_policy_decision_local_var->mbs_qos_chars = mbs_qos_chars;
    mbs_policy_decision_local_var->auth_mbs_sess_ambr = auth_mbs_sess_ambr;
    mbs_policy_decision_local_var->is_mbs_pcrts_null = is_mbs_pcrts_null;
    mbs_policy_decision_local_var->mbs_pcrts = mbs_pcrts;
    mbs_policy_decision_local_var->is_mbs_charging_data_null = is_mbs_charging_data_null;
    mbs_policy_decision_local_var->mbs_charging_data = mbs_charging_data;

    return mbs_policy_decision_local_var;
}

void OpenAPI_mbs_policy_decision_free(OpenAPI_mbs_policy_decision_t *mbs_policy_decision)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_policy_decision) {
        return;
    }
    if (mbs_policy_decision->mbs_pcc_rules) {
        OpenAPI_list_for_each(mbs_policy_decision->mbs_pcc_rules, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_pcc_rule_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_policy_decision->mbs_pcc_rules);
        mbs_policy_decision->mbs_pcc_rules = NULL;
    }
    if (mbs_policy_decision->mbs_qos_decs) {
        OpenAPI_list_for_each(mbs_policy_decision->mbs_qos_decs, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_qos_dec_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_policy_decision->mbs_qos_decs);
        mbs_policy_decision->mbs_qos_decs = NULL;
    }
    if (mbs_policy_decision->mbs_qos_chars) {
        OpenAPI_list_for_each(mbs_policy_decision->mbs_qos_chars, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_qos_char_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_policy_decision->mbs_qos_chars);
        mbs_policy_decision->mbs_qos_chars = NULL;
    }
    if (mbs_policy_decision->auth_mbs_sess_ambr) {
        ogs_free(mbs_policy_decision->auth_mbs_sess_ambr);
        mbs_policy_decision->auth_mbs_sess_ambr = NULL;
    }
    if (mbs_policy_decision->mbs_pcrts) {
        OpenAPI_list_free(mbs_policy_decision->mbs_pcrts);
        mbs_policy_decision->mbs_pcrts = NULL;
    }
    if (mbs_policy_decision->mbs_charging_data) {
        OpenAPI_list_for_each(mbs_policy_decision->mbs_charging_data, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_charging_data_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_policy_decision->mbs_charging_data);
        mbs_policy_decision->mbs_charging_data = NULL;
    }
    ogs_free(mbs_policy_decision);
}

cJSON *OpenAPI_mbs_policy_decision_convertToJSON(OpenAPI_mbs_policy_decision_t *mbs_policy_decision)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_policy_decision == NULL) {
        ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [MbsPolicyDecision]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_policy_decision->mbs_pcc_rules) {
    cJSON *mbs_pcc_rules = cJSON_AddObjectToObject(item, "mbsPccRules");
    if (mbs_pcc_rules == NULL) {
        ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_pcc_rules]");
        goto end;
    }
    cJSON *localMapObject = mbs_pcc_rules;
    if (mbs_policy_decision->mbs_pcc_rules) {
        OpenAPI_list_for_each(mbs_policy_decision->mbs_pcc_rules, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            if (localKeyValue == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_pcc_rules]");
                goto end;
            }
            if (localKeyValue->key == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_pcc_rules]");
                goto end;
            }
            cJSON *itemLocal = localKeyValue->value ?
                OpenAPI_mbs_pcc_rule_convertToJSON(localKeyValue->value) :
                cJSON_CreateNull();
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [inner]");
                goto end;
            }
            cJSON_AddItemToObject(localMapObject, localKeyValue->key, itemLocal);
        }
    }
    } else if (mbs_policy_decision->is_mbs_pcc_rules_null) {
        if (cJSON_AddNullToObject(item, "mbsPccRules") == NULL) {
            ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_pcc_rules]");
            goto end;
        }
    }

    if (mbs_policy_decision->mbs_qos_decs) {
    cJSON *mbs_qos_decs = cJSON_AddObjectToObject(item, "mbsQosDecs");
    if (mbs_qos_decs == NULL) {
        ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_qos_decs]");
        goto end;
    }
    cJSON *localMapObject = mbs_qos_decs;
    if (mbs_policy_decision->mbs_qos_decs) {
        OpenAPI_list_for_each(mbs_policy_decision->mbs_qos_decs, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            if (localKeyValue == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_qos_decs]");
                goto end;
            }
            if (localKeyValue->key == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_qos_decs]");
                goto end;
            }
            cJSON *itemLocal = localKeyValue->value ?
                OpenAPI_mbs_qos_dec_convertToJSON(localKeyValue->value) :
                cJSON_CreateNull();
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [inner]");
                goto end;
            }
            cJSON_AddItemToObject(localMapObject, localKeyValue->key, itemLocal);
        }
    }
    }

    if (mbs_policy_decision->mbs_qos_chars) {
    cJSON *mbs_qos_chars = cJSON_AddObjectToObject(item, "mbsQosChars");
    if (mbs_qos_chars == NULL) {
        ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_qos_chars]");
        goto end;
    }
    cJSON *localMapObject = mbs_qos_chars;
    if (mbs_policy_decision->mbs_qos_chars) {
        OpenAPI_list_for_each(mbs_policy_decision->mbs_qos_chars, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            if (localKeyValue == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_qos_chars]");
                goto end;
            }
            if (localKeyValue->key == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_qos_chars]");
                goto end;
            }
            cJSON *itemLocal = localKeyValue->value ?
                OpenAPI_mbs_qos_char_convertToJSON(localKeyValue->value) :
                cJSON_CreateNull();
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [inner]");
                goto end;
            }
            cJSON_AddItemToObject(localMapObject, localKeyValue->key, itemLocal);
        }
    }
    }

    if (mbs_policy_decision->auth_mbs_sess_ambr) {
    if (cJSON_AddStringToObject(item, "authMbsSessAmbr", mbs_policy_decision->auth_mbs_sess_ambr) == NULL) {
        ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [auth_mbs_sess_ambr]");
        goto end;
    }
    }

    if (mbs_policy_decision->mbs_pcrts != OpenAPI_mbs_pcrt_NULL) {
    cJSON *mbs_pcrtsList = cJSON_AddArrayToObject(item, "mbsPcrts");
    if (mbs_pcrtsList == NULL) {
        ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_pcrts]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_policy_decision->mbs_pcrts, node) {
        if (cJSON_AddStringToObject(mbs_pcrtsList, "", OpenAPI_mbs_pcrt_ToString((intptr_t)node->data)) == NULL) {
            ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_pcrts]");
            goto end;
        }
    }
    } else if (mbs_policy_decision->is_mbs_pcrts_null) {
        if (cJSON_AddNullToObject(item, "mbsPcrts") == NULL) {
            ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_pcrts]");
            goto end;
        }
    }

    if (mbs_policy_decision->mbs_charging_data) {
    cJSON *mbs_charging_data = cJSON_AddObjectToObject(item, "mbsChargingData");
    if (mbs_charging_data == NULL) {
        ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_charging_data]");
        goto end;
    }
    cJSON *localMapObject = mbs_charging_data;
    if (mbs_policy_decision->mbs_charging_data) {
        OpenAPI_list_for_each(mbs_policy_decision->mbs_charging_data, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            if (localKeyValue == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_charging_data]");
                goto end;
            }
            if (localKeyValue->key == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_charging_data]");
                goto end;
            }
            cJSON *itemLocal = localKeyValue->value ?
                OpenAPI_mbs_charging_data_convertToJSON(localKeyValue->value) :
                cJSON_CreateNull();
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [inner]");
                goto end;
            }
            cJSON_AddItemToObject(localMapObject, localKeyValue->key, itemLocal);
        }
    }
    } else if (mbs_policy_decision->is_mbs_charging_data_null) {
        if (cJSON_AddNullToObject(item, "mbsChargingData") == NULL) {
            ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed [mbs_charging_data]");
            goto end;
        }
    }

end:
    return item;
}

OpenAPI_mbs_policy_decision_t *OpenAPI_mbs_policy_decision_parseFromJSON(cJSON *mbs_policy_decisionJSON)
{
    OpenAPI_mbs_policy_decision_t *mbs_policy_decision_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_pcc_rules = NULL;
    OpenAPI_list_t *mbs_pcc_rulesList = NULL;
    cJSON *mbs_qos_decs = NULL;
    OpenAPI_list_t *mbs_qos_decsList = NULL;
    cJSON *mbs_qos_chars = NULL;
    OpenAPI_list_t *mbs_qos_charsList = NULL;
    cJSON *auth_mbs_sess_ambr = NULL;
    cJSON *mbs_pcrts = NULL;
    OpenAPI_list_t *mbs_pcrtsList = NULL;
    cJSON *mbs_charging_data = NULL;
    OpenAPI_list_t *mbs_charging_dataList = NULL;
    mbs_pcc_rules = cJSON_GetObjectItemCaseSensitive(mbs_policy_decisionJSON, "mbsPccRules");
    if (mbs_pcc_rules) {
    if (!cJSON_IsNull(mbs_pcc_rules)) {
        cJSON *mbs_pcc_rules_local_map = NULL;
        if (!cJSON_IsObject(mbs_pcc_rules) && !cJSON_IsNull(mbs_pcc_rules)) {
            ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [mbs_pcc_rules]");
            goto end;
        }
        if (cJSON_IsObject(mbs_pcc_rules)) {
            mbs_pcc_rulesList = OpenAPI_list_create();
            OpenAPI_map_t *localMapKeyPair = NULL;
            cJSON_ArrayForEach(mbs_pcc_rules_local_map, mbs_pcc_rules) {
                cJSON *localMapObject = mbs_pcc_rules_local_map;
                if (cJSON_IsObject(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(
                        ogs_strdup(localMapObject->string), OpenAPI_mbs_pcc_rule_parseFromJSON(localMapObject));
                } else if (cJSON_IsNull(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(ogs_strdup(localMapObject->string), NULL);
                } else {
                    ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [inner]");
                    goto end;
                }
                OpenAPI_list_add(mbs_pcc_rulesList, localMapKeyPair);
            }
        }
    }
    }

    mbs_qos_decs = cJSON_GetObjectItemCaseSensitive(mbs_policy_decisionJSON, "mbsQosDecs");
    if (mbs_qos_decs) {
        cJSON *mbs_qos_decs_local_map = NULL;
        if (!cJSON_IsObject(mbs_qos_decs) && !cJSON_IsNull(mbs_qos_decs)) {
            ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [mbs_qos_decs]");
            goto end;
        }
        if (cJSON_IsObject(mbs_qos_decs)) {
            mbs_qos_decsList = OpenAPI_list_create();
            OpenAPI_map_t *localMapKeyPair = NULL;
            cJSON_ArrayForEach(mbs_qos_decs_local_map, mbs_qos_decs) {
                cJSON *localMapObject = mbs_qos_decs_local_map;
                if (cJSON_IsObject(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(
                        ogs_strdup(localMapObject->string), OpenAPI_mbs_qos_dec_parseFromJSON(localMapObject));
                } else if (cJSON_IsNull(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(ogs_strdup(localMapObject->string), NULL);
                } else {
                    ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [inner]");
                    goto end;
                }
                OpenAPI_list_add(mbs_qos_decsList, localMapKeyPair);
            }
        }
    }

    mbs_qos_chars = cJSON_GetObjectItemCaseSensitive(mbs_policy_decisionJSON, "mbsQosChars");
    if (mbs_qos_chars) {
        cJSON *mbs_qos_chars_local_map = NULL;
        if (!cJSON_IsObject(mbs_qos_chars) && !cJSON_IsNull(mbs_qos_chars)) {
            ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [mbs_qos_chars]");
            goto end;
        }
        if (cJSON_IsObject(mbs_qos_chars)) {
            mbs_qos_charsList = OpenAPI_list_create();
            OpenAPI_map_t *localMapKeyPair = NULL;
            cJSON_ArrayForEach(mbs_qos_chars_local_map, mbs_qos_chars) {
                cJSON *localMapObject = mbs_qos_chars_local_map;
                if (cJSON_IsObject(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(
                        ogs_strdup(localMapObject->string), OpenAPI_mbs_qos_char_parseFromJSON(localMapObject));
                } else if (cJSON_IsNull(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(ogs_strdup(localMapObject->string), NULL);
                } else {
                    ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [inner]");
                    goto end;
                }
                OpenAPI_list_add(mbs_qos_charsList, localMapKeyPair);
            }
        }
    }

    auth_mbs_sess_ambr = cJSON_GetObjectItemCaseSensitive(mbs_policy_decisionJSON, "authMbsSessAmbr");
    if (auth_mbs_sess_ambr) {
    if (!cJSON_IsString(auth_mbs_sess_ambr) && !cJSON_IsNull(auth_mbs_sess_ambr)) {
        ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [auth_mbs_sess_ambr]");
        goto end;
    }
    }

    mbs_pcrts = cJSON_GetObjectItemCaseSensitive(mbs_policy_decisionJSON, "mbsPcrts");
    if (mbs_pcrts) {
    if (!cJSON_IsNull(mbs_pcrts)) {
        cJSON *mbs_pcrts_local = NULL;
        if (!cJSON_IsArray(mbs_pcrts)) {
            ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [mbs_pcrts]");
            goto end;
        }

        mbs_pcrtsList = OpenAPI_list_create();

        cJSON_ArrayForEach(mbs_pcrts_local, mbs_pcrts) {
            OpenAPI_mbs_pcrt_e localEnum = OpenAPI_mbs_pcrt_NULL;
            if (!cJSON_IsString(mbs_pcrts_local)) {
                ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [mbs_pcrts]");
                goto end;
            }
            localEnum = OpenAPI_mbs_pcrt_FromString(mbs_pcrts_local->valuestring);
            if (!localEnum) {
                ogs_info("Enum value \"%s\" for field \"mbs_pcrts\" is not supported. Ignoring it ...",
                         mbs_pcrts_local->valuestring);
            } else {
                OpenAPI_list_add(mbs_pcrtsList, (void *)localEnum);
            }
        }
        if (mbs_pcrtsList->count == 0) {
            ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed: Expected mbs_pcrtsList to not be empty (after ignoring unsupported enum values).");
            goto end;
        }
    }
    }

    mbs_charging_data = cJSON_GetObjectItemCaseSensitive(mbs_policy_decisionJSON, "mbsChargingData");
    if (mbs_charging_data) {
    if (!cJSON_IsNull(mbs_charging_data)) {
        cJSON *mbs_charging_data_local_map = NULL;
        if (!cJSON_IsObject(mbs_charging_data) && !cJSON_IsNull(mbs_charging_data)) {
            ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [mbs_charging_data]");
            goto end;
        }
        if (cJSON_IsObject(mbs_charging_data)) {
            mbs_charging_dataList = OpenAPI_list_create();
            OpenAPI_map_t *localMapKeyPair = NULL;
            cJSON_ArrayForEach(mbs_charging_data_local_map, mbs_charging_data) {
                cJSON *localMapObject = mbs_charging_data_local_map;
                if (cJSON_IsObject(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(
                        ogs_strdup(localMapObject->string), OpenAPI_mbs_charging_data_parseFromJSON(localMapObject));
                } else if (cJSON_IsNull(localMapObject)) {
                    localMapKeyPair = OpenAPI_map_create(ogs_strdup(localMapObject->string), NULL);
                } else {
                    ogs_error("OpenAPI_mbs_policy_decision_parseFromJSON() failed [inner]");
                    goto end;
                }
                OpenAPI_list_add(mbs_charging_dataList, localMapKeyPair);
            }
        }
    }
    }

    mbs_policy_decision_local_var = OpenAPI_mbs_policy_decision_create (
        mbs_pcc_rules && cJSON_IsNull(mbs_pcc_rules) ? true : false,
        mbs_pcc_rules ? mbs_pcc_rulesList : NULL,
        mbs_qos_decs ? mbs_qos_decsList : NULL,
        mbs_qos_chars ? mbs_qos_charsList : NULL,
        auth_mbs_sess_ambr && !cJSON_IsNull(auth_mbs_sess_ambr) ? ogs_strdup(auth_mbs_sess_ambr->valuestring) : NULL,
        mbs_pcrts && cJSON_IsNull(mbs_pcrts) ? true : false,
        mbs_pcrts ? mbs_pcrtsList : NULL,
        mbs_charging_data && cJSON_IsNull(mbs_charging_data) ? true : false,
        mbs_charging_data ? mbs_charging_dataList : NULL
    );

    return mbs_policy_decision_local_var;
end:
    if (mbs_pcc_rulesList) {
        OpenAPI_list_for_each(mbs_pcc_rulesList, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_pcc_rule_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_pcc_rulesList);
        mbs_pcc_rulesList = NULL;
    }
    if (mbs_qos_decsList) {
        OpenAPI_list_for_each(mbs_qos_decsList, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_qos_dec_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_qos_decsList);
        mbs_qos_decsList = NULL;
    }
    if (mbs_qos_charsList) {
        OpenAPI_list_for_each(mbs_qos_charsList, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_qos_char_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_qos_charsList);
        mbs_qos_charsList = NULL;
    }
    if (mbs_pcrtsList) {
        OpenAPI_list_free(mbs_pcrtsList);
        mbs_pcrtsList = NULL;
    }
    if (mbs_charging_dataList) {
        OpenAPI_list_for_each(mbs_charging_dataList, node) {
            OpenAPI_map_t *localKeyValue = (OpenAPI_map_t*)node->data;
            ogs_free(localKeyValue->key);
            OpenAPI_mbs_charging_data_free(localKeyValue->value);
            OpenAPI_map_free(localKeyValue);
        }
        OpenAPI_list_free(mbs_charging_dataList);
        mbs_charging_dataList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_policy_decision_t *OpenAPI_mbs_policy_decision_copy(OpenAPI_mbs_policy_decision_t *dst, OpenAPI_mbs_policy_decision_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_policy_decision_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_policy_decision_convertToJSON() failed");
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

    OpenAPI_mbs_policy_decision_free(dst);
    dst = OpenAPI_mbs_policy_decision_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

