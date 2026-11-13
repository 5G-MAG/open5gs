
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_charging_data.h"

OpenAPI_mbs_charging_data_t *OpenAPI_mbs_charging_data_create(
    char *mbs_chg_id,
    bool is_mbs_rating_grp,
    int mbs_rating_grp,
    OpenAPI_metering_method_e metering_method
)
{
    OpenAPI_mbs_charging_data_t *mbs_charging_data_local_var = ogs_malloc(sizeof(OpenAPI_mbs_charging_data_t));
    ogs_assert(mbs_charging_data_local_var);

    mbs_charging_data_local_var->mbs_chg_id = mbs_chg_id;
    mbs_charging_data_local_var->is_mbs_rating_grp = is_mbs_rating_grp;
    mbs_charging_data_local_var->mbs_rating_grp = mbs_rating_grp;
    mbs_charging_data_local_var->metering_method = metering_method;

    return mbs_charging_data_local_var;
}

void OpenAPI_mbs_charging_data_free(OpenAPI_mbs_charging_data_t *mbs_charging_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_charging_data) {
        return;
    }
    if (mbs_charging_data->mbs_chg_id) {
        ogs_free(mbs_charging_data->mbs_chg_id);
        mbs_charging_data->mbs_chg_id = NULL;
    }
    ogs_free(mbs_charging_data);
}

cJSON *OpenAPI_mbs_charging_data_convertToJSON(OpenAPI_mbs_charging_data_t *mbs_charging_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_charging_data == NULL) {
        ogs_error("OpenAPI_mbs_charging_data_convertToJSON() failed [MbsChargingData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!mbs_charging_data->mbs_chg_id) {
        ogs_error("OpenAPI_mbs_charging_data_convertToJSON() failed [mbs_chg_id]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "mbsChgId", mbs_charging_data->mbs_chg_id) == NULL) {
        ogs_error("OpenAPI_mbs_charging_data_convertToJSON() failed [mbs_chg_id]");
        goto end;
    }

    if (mbs_charging_data->is_mbs_rating_grp) {
    if (cJSON_AddNumberToObject(item, "mbsRatingGrp", mbs_charging_data->mbs_rating_grp) == NULL) {
        ogs_error("OpenAPI_mbs_charging_data_convertToJSON() failed [mbs_rating_grp]");
        goto end;
    }
    }

    if (mbs_charging_data->metering_method != OpenAPI_metering_method_NULL) {
    if (cJSON_AddStringToObject(item, "meteringMethod", OpenAPI_metering_method_ToString(mbs_charging_data->metering_method)) == NULL) {
        ogs_error("OpenAPI_mbs_charging_data_convertToJSON() failed [metering_method]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_mbs_charging_data_t *OpenAPI_mbs_charging_data_parseFromJSON(cJSON *mbs_charging_dataJSON)
{
    OpenAPI_mbs_charging_data_t *mbs_charging_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_chg_id = NULL;
    cJSON *mbs_rating_grp = NULL;
    cJSON *metering_method = NULL;
    OpenAPI_metering_method_e metering_methodVariable = 0;
    mbs_chg_id = cJSON_GetObjectItemCaseSensitive(mbs_charging_dataJSON, "mbsChgId");
    if (!mbs_chg_id) {
        ogs_error("OpenAPI_mbs_charging_data_parseFromJSON() failed [mbs_chg_id]");
        goto end;
    }
    if (!cJSON_IsString(mbs_chg_id)) {
        ogs_error("OpenAPI_mbs_charging_data_parseFromJSON() failed [mbs_chg_id]");
        goto end;
    }

    mbs_rating_grp = cJSON_GetObjectItemCaseSensitive(mbs_charging_dataJSON, "mbsRatingGrp");
    if (mbs_rating_grp) {
    if (!cJSON_IsNumber(mbs_rating_grp)) {
        ogs_error("OpenAPI_mbs_charging_data_parseFromJSON() failed [mbs_rating_grp]");
        goto end;
    }
    }

    metering_method = cJSON_GetObjectItemCaseSensitive(mbs_charging_dataJSON, "meteringMethod");
    if (metering_method) {
    if (!cJSON_IsString(metering_method)) {
        ogs_error("OpenAPI_mbs_charging_data_parseFromJSON() failed [metering_method]");
        goto end;
    }
    metering_methodVariable = OpenAPI_metering_method_FromString(metering_method->valuestring);
    }

    mbs_charging_data_local_var = OpenAPI_mbs_charging_data_create (
        ogs_strdup(mbs_chg_id->valuestring),
        mbs_rating_grp ? true : false,
        mbs_rating_grp ? mbs_rating_grp->valuedouble : 0,
        metering_method ? metering_methodVariable : 0
    );

    return mbs_charging_data_local_var;
end:
    return NULL;
}

OpenAPI_mbs_charging_data_t *OpenAPI_mbs_charging_data_copy(OpenAPI_mbs_charging_data_t *dst, OpenAPI_mbs_charging_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_charging_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_charging_data_convertToJSON() failed");
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

    OpenAPI_mbs_charging_data_free(dst);
    dst = OpenAPI_mbs_charging_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

