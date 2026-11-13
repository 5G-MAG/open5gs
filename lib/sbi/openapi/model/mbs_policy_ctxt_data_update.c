
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_policy_ctxt_data_update.h"

OpenAPI_mbs_policy_ctxt_data_update_t *OpenAPI_mbs_policy_ctxt_data_update_create(
    OpenAPI_mbs_service_info_t *mbs_serv_info,
    OpenAPI_list_t *mbs_pcrts,
    OpenAPI_mbs_error_report_t *mbs_error_report
)
{
    OpenAPI_mbs_policy_ctxt_data_update_t *mbs_policy_ctxt_data_update_local_var = ogs_malloc(sizeof(OpenAPI_mbs_policy_ctxt_data_update_t));
    ogs_assert(mbs_policy_ctxt_data_update_local_var);

    mbs_policy_ctxt_data_update_local_var->mbs_serv_info = mbs_serv_info;
    mbs_policy_ctxt_data_update_local_var->mbs_pcrts = mbs_pcrts;
    mbs_policy_ctxt_data_update_local_var->mbs_error_report = mbs_error_report;

    return mbs_policy_ctxt_data_update_local_var;
}

void OpenAPI_mbs_policy_ctxt_data_update_free(OpenAPI_mbs_policy_ctxt_data_update_t *mbs_policy_ctxt_data_update)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_policy_ctxt_data_update) {
        return;
    }
    if (mbs_policy_ctxt_data_update->mbs_serv_info) {
        OpenAPI_mbs_service_info_free(mbs_policy_ctxt_data_update->mbs_serv_info);
        mbs_policy_ctxt_data_update->mbs_serv_info = NULL;
    }
    if (mbs_policy_ctxt_data_update->mbs_pcrts) {
        OpenAPI_list_free(mbs_policy_ctxt_data_update->mbs_pcrts);
        mbs_policy_ctxt_data_update->mbs_pcrts = NULL;
    }
    if (mbs_policy_ctxt_data_update->mbs_error_report) {
        OpenAPI_mbs_error_report_free(mbs_policy_ctxt_data_update->mbs_error_report);
        mbs_policy_ctxt_data_update->mbs_error_report = NULL;
    }
    ogs_free(mbs_policy_ctxt_data_update);
}

cJSON *OpenAPI_mbs_policy_ctxt_data_update_convertToJSON(OpenAPI_mbs_policy_ctxt_data_update_t *mbs_policy_ctxt_data_update)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_policy_ctxt_data_update == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_update_convertToJSON() failed [MbsPolicyCtxtDataUpdate]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_policy_ctxt_data_update->mbs_serv_info) {
    cJSON *mbs_serv_info_local_JSON = OpenAPI_mbs_service_info_convertToJSON(mbs_policy_ctxt_data_update->mbs_serv_info);
    if (mbs_serv_info_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_update_convertToJSON() failed [mbs_serv_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsServInfo", mbs_serv_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_update_convertToJSON() failed [mbs_serv_info]");
        goto end;
    }
    }

    if (mbs_policy_ctxt_data_update->mbs_pcrts != OpenAPI_mbs_pcrt_NULL) {
    cJSON *mbs_pcrtsList = cJSON_AddArrayToObject(item, "mbsPcrts");
    if (mbs_pcrtsList == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_update_convertToJSON() failed [mbs_pcrts]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_policy_ctxt_data_update->mbs_pcrts, node) {
        if (cJSON_AddStringToObject(mbs_pcrtsList, "", OpenAPI_mbs_pcrt_ToString((intptr_t)node->data)) == NULL) {
            ogs_error("OpenAPI_mbs_policy_ctxt_data_update_convertToJSON() failed [mbs_pcrts]");
            goto end;
        }
    }
    }

    if (mbs_policy_ctxt_data_update->mbs_error_report) {
    cJSON *mbs_error_report_local_JSON = OpenAPI_mbs_error_report_convertToJSON(mbs_policy_ctxt_data_update->mbs_error_report);
    if (mbs_error_report_local_JSON == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_update_convertToJSON() failed [mbs_error_report]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsErrorReport", mbs_error_report_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_update_convertToJSON() failed [mbs_error_report]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_policy_ctxt_data_update_t *OpenAPI_mbs_policy_ctxt_data_update_parseFromJSON(cJSON *mbs_policy_ctxt_data_updateJSON)
{
    OpenAPI_mbs_policy_ctxt_data_update_t *mbs_policy_ctxt_data_update_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_serv_info = NULL;
    OpenAPI_mbs_service_info_t *mbs_serv_info_local_nonprim = NULL;
    cJSON *mbs_pcrts = NULL;
    OpenAPI_list_t *mbs_pcrtsList = NULL;
    cJSON *mbs_error_report = NULL;
    OpenAPI_mbs_error_report_t *mbs_error_report_local_nonprim = NULL;
    mbs_serv_info = cJSON_GetObjectItemCaseSensitive(mbs_policy_ctxt_data_updateJSON, "mbsServInfo");
    if (mbs_serv_info) {
    mbs_serv_info_local_nonprim = OpenAPI_mbs_service_info_parseFromJSON(mbs_serv_info);
    if (!mbs_serv_info_local_nonprim) {
        ogs_error("OpenAPI_mbs_service_info_parseFromJSON failed [mbs_serv_info]");
        goto end;
    }
    }

    mbs_pcrts = cJSON_GetObjectItemCaseSensitive(mbs_policy_ctxt_data_updateJSON, "mbsPcrts");
    if (mbs_pcrts) {
        cJSON *mbs_pcrts_local = NULL;
        if (!cJSON_IsArray(mbs_pcrts)) {
            ogs_error("OpenAPI_mbs_policy_ctxt_data_update_parseFromJSON() failed [mbs_pcrts]");
            goto end;
        }

        mbs_pcrtsList = OpenAPI_list_create();

        cJSON_ArrayForEach(mbs_pcrts_local, mbs_pcrts) {
            OpenAPI_mbs_pcrt_e localEnum = OpenAPI_mbs_pcrt_NULL;
            if (!cJSON_IsString(mbs_pcrts_local)) {
                ogs_error("OpenAPI_mbs_policy_ctxt_data_update_parseFromJSON() failed [mbs_pcrts]");
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
            ogs_error("OpenAPI_mbs_policy_ctxt_data_update_parseFromJSON() failed: Expected mbs_pcrtsList to not be empty (after ignoring unsupported enum values).");
            goto end;
        }
    }

    mbs_error_report = cJSON_GetObjectItemCaseSensitive(mbs_policy_ctxt_data_updateJSON, "mbsErrorReport");
    if (mbs_error_report) {
    mbs_error_report_local_nonprim = OpenAPI_mbs_error_report_parseFromJSON(mbs_error_report);
    if (!mbs_error_report_local_nonprim) {
        ogs_error("OpenAPI_mbs_error_report_parseFromJSON failed [mbs_error_report]");
        goto end;
    }
    }

    mbs_policy_ctxt_data_update_local_var = OpenAPI_mbs_policy_ctxt_data_update_create (
        mbs_serv_info ? mbs_serv_info_local_nonprim : NULL,
        mbs_pcrts ? mbs_pcrtsList : NULL,
        mbs_error_report ? mbs_error_report_local_nonprim : NULL
    );

    return mbs_policy_ctxt_data_update_local_var;
end:
    if (mbs_serv_info_local_nonprim) {
        OpenAPI_mbs_service_info_free(mbs_serv_info_local_nonprim);
        mbs_serv_info_local_nonprim = NULL;
    }
    if (mbs_pcrtsList) {
        OpenAPI_list_free(mbs_pcrtsList);
        mbs_pcrtsList = NULL;
    }
    if (mbs_error_report_local_nonprim) {
        OpenAPI_mbs_error_report_free(mbs_error_report_local_nonprim);
        mbs_error_report_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_mbs_policy_ctxt_data_update_t *OpenAPI_mbs_policy_ctxt_data_update_copy(OpenAPI_mbs_policy_ctxt_data_update_t *dst, OpenAPI_mbs_policy_ctxt_data_update_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_policy_ctxt_data_update_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_policy_ctxt_data_update_convertToJSON() failed");
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

    OpenAPI_mbs_policy_ctxt_data_update_free(dst);
    dst = OpenAPI_mbs_policy_ctxt_data_update_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

