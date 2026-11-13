
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_report.h"

OpenAPI_mbs_report_t *OpenAPI_mbs_report_create(
    OpenAPI_list_t *mbs_pcc_rule_ids,
    OpenAPI_mbs_pcc_rule_status_e mbs_pcc_rule_status,
    OpenAPI_mbs_failure_code_e failure_code
)
{
    OpenAPI_mbs_report_t *mbs_report_local_var = ogs_malloc(sizeof(OpenAPI_mbs_report_t));
    ogs_assert(mbs_report_local_var);

    mbs_report_local_var->mbs_pcc_rule_ids = mbs_pcc_rule_ids;
    mbs_report_local_var->mbs_pcc_rule_status = mbs_pcc_rule_status;
    mbs_report_local_var->failure_code = failure_code;

    return mbs_report_local_var;
}

void OpenAPI_mbs_report_free(OpenAPI_mbs_report_t *mbs_report)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_report) {
        return;
    }
    if (mbs_report->mbs_pcc_rule_ids) {
        OpenAPI_list_for_each(mbs_report->mbs_pcc_rule_ids, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_report->mbs_pcc_rule_ids);
        mbs_report->mbs_pcc_rule_ids = NULL;
    }
    ogs_free(mbs_report);
}

cJSON *OpenAPI_mbs_report_convertToJSON(OpenAPI_mbs_report_t *mbs_report)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_report == NULL) {
        ogs_error("OpenAPI_mbs_report_convertToJSON() failed [MbsReport]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_report->mbs_pcc_rule_ids) {
    cJSON *mbs_pcc_rule_idsList = cJSON_AddArrayToObject(item, "mbsPccRuleIds");
    if (mbs_pcc_rule_idsList == NULL) {
        ogs_error("OpenAPI_mbs_report_convertToJSON() failed [mbs_pcc_rule_ids]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_report->mbs_pcc_rule_ids, node) {
        if (cJSON_AddStringToObject(mbs_pcc_rule_idsList, "", (char*)node->data) == NULL) {
            ogs_error("OpenAPI_mbs_report_convertToJSON() failed [mbs_pcc_rule_ids]");
            goto end;
        }
    }
    }

    if (mbs_report->mbs_pcc_rule_status != OpenAPI_mbs_pcc_rule_status_NULL) {
    if (cJSON_AddStringToObject(item, "mbsPccRuleStatus", OpenAPI_mbs_pcc_rule_status_ToString(mbs_report->mbs_pcc_rule_status)) == NULL) {
        ogs_error("OpenAPI_mbs_report_convertToJSON() failed [mbs_pcc_rule_status]");
        goto end;
    }
    }

    if (mbs_report->failure_code != OpenAPI_mbs_failure_code_NULL) {
    if (cJSON_AddStringToObject(item, "failureCode", OpenAPI_mbs_failure_code_ToString(mbs_report->failure_code)) == NULL) {
        ogs_error("OpenAPI_mbs_report_convertToJSON() failed [failure_code]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_report_t *OpenAPI_mbs_report_parseFromJSON(cJSON *mbs_reportJSON)
{
    OpenAPI_mbs_report_t *mbs_report_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_pcc_rule_ids = NULL;
    OpenAPI_list_t *mbs_pcc_rule_idsList = NULL;
    cJSON *mbs_pcc_rule_status = NULL;
    OpenAPI_mbs_pcc_rule_status_e mbs_pcc_rule_statusVariable = 0;
    cJSON *failure_code = NULL;
    OpenAPI_mbs_failure_code_e failure_codeVariable = 0;
    mbs_pcc_rule_ids = cJSON_GetObjectItemCaseSensitive(mbs_reportJSON, "mbsPccRuleIds");
    if (mbs_pcc_rule_ids) {
        cJSON *mbs_pcc_rule_ids_local = NULL;
        if (!cJSON_IsArray(mbs_pcc_rule_ids)) {
            ogs_error("OpenAPI_mbs_report_parseFromJSON() failed [mbs_pcc_rule_ids]");
            goto end;
        }

        mbs_pcc_rule_idsList = OpenAPI_list_create();

        cJSON_ArrayForEach(mbs_pcc_rule_ids_local, mbs_pcc_rule_ids) {
            double *localDouble = NULL;
            int *localInt = NULL;
            if (!cJSON_IsString(mbs_pcc_rule_ids_local)) {
                ogs_error("OpenAPI_mbs_report_parseFromJSON() failed [mbs_pcc_rule_ids]");
                goto end;
            }
            OpenAPI_list_add(mbs_pcc_rule_idsList, ogs_strdup(mbs_pcc_rule_ids_local->valuestring));
        }
    }

    mbs_pcc_rule_status = cJSON_GetObjectItemCaseSensitive(mbs_reportJSON, "mbsPccRuleStatus");
    if (mbs_pcc_rule_status) {
    if (!cJSON_IsString(mbs_pcc_rule_status)) {
        ogs_error("OpenAPI_mbs_report_parseFromJSON() failed [mbs_pcc_rule_status]");
        goto end;
    }
    mbs_pcc_rule_statusVariable = OpenAPI_mbs_pcc_rule_status_FromString(mbs_pcc_rule_status->valuestring);
    }

    failure_code = cJSON_GetObjectItemCaseSensitive(mbs_reportJSON, "failureCode");
    if (failure_code) {
    if (!cJSON_IsString(failure_code)) {
        ogs_error("OpenAPI_mbs_report_parseFromJSON() failed [failure_code]");
        goto end;
    }
    failure_codeVariable = OpenAPI_mbs_failure_code_FromString(failure_code->valuestring);
    }

    mbs_report_local_var = OpenAPI_mbs_report_create (
        mbs_pcc_rule_ids ? mbs_pcc_rule_idsList : NULL,
        mbs_pcc_rule_status ? mbs_pcc_rule_statusVariable : 0,
        failure_code ? failure_codeVariable : 0
    );

    return mbs_report_local_var;
end:
    if (mbs_pcc_rule_idsList) {
        OpenAPI_list_for_each(mbs_pcc_rule_idsList, node) {
            ogs_free(node->data);
        }
        OpenAPI_list_free(mbs_pcc_rule_idsList);
        mbs_pcc_rule_idsList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_report_t *OpenAPI_mbs_report_copy(OpenAPI_mbs_report_t *dst, OpenAPI_mbs_report_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_report_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_report_convertToJSON() failed");
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

    OpenAPI_mbs_report_free(dst);
    dst = OpenAPI_mbs_report_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

