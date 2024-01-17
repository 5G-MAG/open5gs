
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "create_req_data.h"

OpenAPI_create_req_data_t *OpenAPI_create_req_data_create(
    OpenAPI_ext_mbs_session_t *mbs_session
)
{
    OpenAPI_create_req_data_t *create_req_data_local_var = ogs_malloc(sizeof(OpenAPI_create_req_data_t));
    ogs_assert(create_req_data_local_var);

    create_req_data_local_var->mbs_session = mbs_session;

    return create_req_data_local_var;
}

void OpenAPI_create_req_data_free(OpenAPI_create_req_data_t *create_req_data)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == create_req_data) {
        return;
    }
    if (create_req_data->mbs_session) {
        OpenAPI_ext_mbs_session_free(create_req_data->mbs_session);
        create_req_data->mbs_session = NULL;
    }
    ogs_free(create_req_data);
}

cJSON *OpenAPI_create_req_data_convertToJSON(OpenAPI_create_req_data_t *create_req_data)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (create_req_data == NULL) {
        ogs_error("OpenAPI_create_req_data_convertToJSON() failed [CreateReqData]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!create_req_data->mbs_session) {
        ogs_error("OpenAPI_create_req_data_convertToJSON() failed [mbs_session]");
        return NULL;
    }
    cJSON *mbs_session_local_JSON = OpenAPI_ext_mbs_session_convertToJSON(create_req_data->mbs_session);
    if (mbs_session_local_JSON == NULL) {
        ogs_error("OpenAPI_create_req_data_convertToJSON() failed [mbs_session]");
        goto end;
    }
    cJSON_AddItemToObject(item, "mbsSession", mbs_session_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_create_req_data_convertToJSON() failed [mbs_session]");
        goto end;
    }

end:
    return item;
}

OpenAPI_create_req_data_t *OpenAPI_create_req_data_parseFromJSON(cJSON *create_req_dataJSON)
{
    OpenAPI_create_req_data_t *create_req_data_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *mbs_session = NULL;
    OpenAPI_ext_mbs_session_t *mbs_session_local_nonprim = NULL;
    mbs_session = cJSON_GetObjectItemCaseSensitive(create_req_dataJSON, "mbsSession");
    if (!mbs_session) {
        ogs_error("OpenAPI_create_req_data_parseFromJSON() failed [mbs_session]");
        goto end;
    }
    mbs_session_local_nonprim = OpenAPI_ext_mbs_session_parseFromJSON(mbs_session);
    if (!mbs_session_local_nonprim) {
        ogs_error("OpenAPI_ext_mbs_session_parseFromJSON failed [mbs_session]");
        goto end;
    }

    create_req_data_local_var = OpenAPI_create_req_data_create (
        mbs_session_local_nonprim
    );

    return create_req_data_local_var;
end:
    if (mbs_session_local_nonprim) {
        OpenAPI_ext_mbs_session_free(mbs_session_local_nonprim);
        mbs_session_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_create_req_data_t *OpenAPI_create_req_data_copy(OpenAPI_create_req_data_t *dst, OpenAPI_create_req_data_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_create_req_data_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_create_req_data_convertToJSON() failed");
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

    OpenAPI_create_req_data_free(dst);
    dst = OpenAPI_create_req_data_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

