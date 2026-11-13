
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_error_report.h"

OpenAPI_mbs_error_report_t *OpenAPI_mbs_error_report_create(
    OpenAPI_list_t *mbs_reports
)
{
    OpenAPI_mbs_error_report_t *mbs_error_report_local_var = ogs_malloc(sizeof(OpenAPI_mbs_error_report_t));
    ogs_assert(mbs_error_report_local_var);

    mbs_error_report_local_var->mbs_reports = mbs_reports;

    return mbs_error_report_local_var;
}

void OpenAPI_mbs_error_report_free(OpenAPI_mbs_error_report_t *mbs_error_report)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_error_report) {
        return;
    }
    if (mbs_error_report->mbs_reports) {
        OpenAPI_list_for_each(mbs_error_report->mbs_reports, node) {
            OpenAPI_mbs_report_free(node->data);
        }
        OpenAPI_list_free(mbs_error_report->mbs_reports);
        mbs_error_report->mbs_reports = NULL;
    }
    ogs_free(mbs_error_report);
}

cJSON *OpenAPI_mbs_error_report_convertToJSON(OpenAPI_mbs_error_report_t *mbs_error_report)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_error_report == NULL) {
        ogs_error("OpenAPI_mbs_error_report_convertToJSON() failed [MbsErrorReport]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_error_report->mbs_reports) {
    cJSON *mbs_reportsList = cJSON_AddArrayToObject(item, "mbsReports");
    if (mbs_reportsList == NULL) {
        ogs_error("OpenAPI_mbs_error_report_convertToJSON() failed [mbs_reports]");
        goto end;
    }
    OpenAPI_list_for_each(mbs_error_report->mbs_reports, node) {
        cJSON *itemLocal = OpenAPI_mbs_report_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_mbs_error_report_convertToJSON() failed [mbs_reports]");
            goto end;
        }
        cJSON_AddItemToArray(mbs_reportsList, itemLocal);
    }
    }

end:
    return item;
}

OpenAPI_mbs_error_report_t *OpenAPI_mbs_error_report_parseFromJSON(cJSON *mbs_error_reportJSON)
{
    OpenAPI_mbs_error_report_t *mbs_error_report_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_reports = NULL;
    OpenAPI_list_t *mbs_reportsList = NULL;
    mbs_reports = cJSON_GetObjectItemCaseSensitive(mbs_error_reportJSON, "mbsReports");
    if (mbs_reports) {
        cJSON *mbs_reports_local = NULL;
        if (!cJSON_IsArray(mbs_reports)) {
            ogs_error("OpenAPI_mbs_error_report_parseFromJSON() failed [mbs_reports]");
            goto end;
        }

        mbs_reportsList = OpenAPI_list_create();

        cJSON_ArrayForEach(mbs_reports_local, mbs_reports) {
            if (!cJSON_IsObject(mbs_reports_local)) {
                ogs_error("OpenAPI_mbs_error_report_parseFromJSON() failed [mbs_reports]");
                goto end;
            }
            OpenAPI_mbs_report_t *mbs_reportsItem = OpenAPI_mbs_report_parseFromJSON(mbs_reports_local);
            if (!mbs_reportsItem) {
                ogs_error("No mbs_reportsItem");
                goto end;
            }
            OpenAPI_list_add(mbs_reportsList, mbs_reportsItem);
        }
    }

    mbs_error_report_local_var = OpenAPI_mbs_error_report_create (
        mbs_reports ? mbs_reportsList : NULL
    );

    return mbs_error_report_local_var;
end:
    if (mbs_reportsList) {
        OpenAPI_list_for_each(mbs_reportsList, node) {
            OpenAPI_mbs_report_free(node->data);
        }
        OpenAPI_list_free(mbs_reportsList);
        mbs_reportsList = NULL;
    }
    return NULL;
}

OpenAPI_mbs_error_report_t *OpenAPI_mbs_error_report_copy(OpenAPI_mbs_error_report_t *dst, OpenAPI_mbs_error_report_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_error_report_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_error_report_convertToJSON() failed");
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

    OpenAPI_mbs_error_report_free(dst);
    dst = OpenAPI_mbs_error_report_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

