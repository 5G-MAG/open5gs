
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "update_rsp_data.h"

OpenAPI_update_rsp_data_t *OpenAPI_update_rsp_data_create(
    OpenAPI_ext_mbs_session_t *mbs_session
)
{
    OpenAPI_update_rsp_data_t *update_rsp_data_local_var = ogs_malloc(sizeof(OpenAPI_update_rsp_data_t));
    ogs_assert(update_rsp_data_local_var);

    update_rsp_data_local_var->mbs_session = mbs_session;

    return update_rsp_data_local_var;
}

void OpenAPI_update_rsp_data_free(OpenAPI_update_rsp_data_t *update_rsp_data)
{
    if (NULL == update_rsp_data) {
        return;
    }
    if (update_rsp_data->mbs_session) {
        OpenAPI_ext_mbs_session_free(update_rsp_data->mbs_session);
        update_rsp_data->mbs_session = NULL;
    }
    ogs_free(update_rsp_data);
}

cJSON *OpenAPI_update_rsp_data_convertToJSON(OpenAPI_update_rsp_data_t *update_rsp_data)
{
    cJSON *item = NULL;

    if (update_rsp_data == NULL) {
        ogs_error("OpenAPI_update_rsp_data_convertToJSON() failed [UpdateRspData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!update_rsp_data->mbs_session) {
        ogs_error("OpenAPI_update_rsp_data_convertToJSON() failed [mbs_session]");
        return NULL;
    }
    cJSON *mbs_session_local_JSON = OpenAPI_ext_mbs_session_convertToJSON(update_rsp_data->mbs_session);
    if (mbs_session_local_JSON == NULL) {
        ogs_error("OpenAPI_update_rsp_data_convertToJSON() failed [mbs_session]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSession", mbs_session_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_update_rsp_data_convertToJSON() failed [mbs_session]");
        goto end;
    }

end:
    return item;
}

OpenAPI_update_rsp_data_t *OpenAPI_update_rsp_data_parseFromJSON(cJSON *update_rsp_dataJSON)
{
    OpenAPI_update_rsp_data_t *update_rsp_data_local_var = NULL;
    cJSON *mbs_session = NULL;
    OpenAPI_ext_mbs_session_t *mbs_session_local_nonprim = NULL;
    mbs_session = cJSON_GetObjectItemCaseSensitive(update_rsp_dataJSON, "mbsSession");
    if (!mbs_session) {
        ogs_error("OpenAPI_update_rsp_data_parseFromJSON() failed [mbs_session]");
        goto end;
    }
    mbs_session_local_nonprim = OpenAPI_ext_mbs_session_parseFromJSON(mbs_session);
    if (!mbs_session_local_nonprim) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON failed [mbs_session]");
        goto end;
    }

    update_rsp_data_local_var = OpenAPI_update_rsp_data_create (
        mbs_session_local_nonprim
    );

    return update_rsp_data_local_var;
end:
    if (mbs_session_local_nonprim) {
        OpenAPI_ext_mbs_session_free(mbs_session_local_nonprim);
        mbs_session_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_update_rsp_data_t *OpenAPI_update_rsp_data_copy(OpenAPI_update_rsp_data_t *dst, OpenAPI_update_rsp_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_update_rsp_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_update_rsp_data_convertToJSON() failed");
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

    OpenAPI_update_rsp_data_free(dst);
    dst = OpenAPI_update_rsp_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}
