#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "associated_session_id.h"

OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_create(
    OpenAPI_ssm_t *ssm,
    char *string
)
{
    OpenAPI_associated_session_id_t *associated_session_id_local_var =
        ogs_malloc(sizeof(OpenAPI_associated_session_id_t));
    ogs_assert(associated_session_id_local_var);

    associated_session_id_local_var->ssm = ssm;
    associated_session_id_local_var->string = string;

    return associated_session_id_local_var;
}

void OpenAPI_associated_session_id_free(OpenAPI_associated_session_id_t *associated_session_id)
{
    if (NULL == associated_session_id) {
        return;
    }
    if (associated_session_id->ssm) {
        OpenAPI_ssm_free(associated_session_id->ssm);
        associated_session_id->ssm = NULL;
    }
    if (associated_session_id->string) {
        ogs_free(associated_session_id->string);
        associated_session_id->string = NULL;
    }
    ogs_free(associated_session_id);
}

cJSON *OpenAPI_associated_session_id_convertToJSON(OpenAPI_associated_session_id_t *associated_session_id)
{
    if (associated_session_id == NULL) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed [AssociatedSessionId]");
        return NULL;
    }

    if (associated_session_id->ssm)
        return OpenAPI_ssm_convertToJSON(associated_session_id->ssm);
    if (associated_session_id->string)
        return cJSON_CreateString(associated_session_id->string);

    ogs_error("OpenAPI_associated_session_id_convertToJSON() failed [neither ssm nor string]");
    return NULL;
}

OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_parseFromJSON(cJSON *associated_session_idJSON)
{
    OpenAPI_ssm_t *ssm = NULL;

    if (cJSON_IsString(associated_session_idJSON)) {
        return OpenAPI_associated_session_id_create(NULL, ogs_strdup(associated_session_idJSON->valuestring));
    }
    if (cJSON_IsObject(associated_session_idJSON)) {
        ssm = OpenAPI_ssm_parseFromJSON(associated_session_idJSON);
        if (!ssm) {
            ogs_error("OpenAPI_ssm_parseFromJSON failed [associated_session_id]");
            return NULL;
        }
        return OpenAPI_associated_session_id_create(ssm, NULL);
    }

    ogs_error("OpenAPI_associated_session_id_parseFromJSON() failed [neither a string nor an object]");
    return NULL;
}

OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_copy(
        OpenAPI_associated_session_id_t *dst, OpenAPI_associated_session_id_t *src)
{
    cJSON *item = NULL;

    ogs_assert(src);
    item = OpenAPI_associated_session_id_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_associated_session_id_convertToJSON() failed");
        return NULL;
    }

    OpenAPI_associated_session_id_free(dst);
    dst = OpenAPI_associated_session_id_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}
