
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_session_event.h"

OpenAPI_mbs_session_event_t *OpenAPI_mbs_session_event_create(
    OpenAPI_mbs_session_event_type_e event_type
)
{
    OpenAPI_mbs_session_event_t *mbs_session_event_local_var = ogs_malloc(sizeof(OpenAPI_mbs_session_event_t));
    ogs_assert(mbs_session_event_local_var);

    mbs_session_event_local_var->event_type = event_type;

    return mbs_session_event_local_var;
}

void OpenAPI_mbs_session_event_free(OpenAPI_mbs_session_event_t *mbs_session_event)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == mbs_session_event) {
        return;
    }
    ogs_free(mbs_session_event);
}

cJSON *OpenAPI_mbs_session_event_convertToJSON(OpenAPI_mbs_session_event_t *mbs_session_event)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (mbs_session_event == NULL) {
        ogs_error("OpenAPI_mbs_session_event_convertToJSON() failed [MbsSessionEvent]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (mbs_session_event->event_type == OpenAPI_mbs_session_event_type_NULL) {
        ogs_error("OpenAPI_mbs_session_event_convertToJSON() failed [event_type]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "eventType", OpenAPI_mbs_session_event_type_ToString(mbs_session_event->event_type)) == NULL) {
        ogs_error("OpenAPI_mbs_session_event_convertToJSON() failed [event_type]");
        goto end;
    }

end:
    return item;
}

OpenAPI_mbs_session_event_t *OpenAPI_mbs_session_event_parseFromJSON(cJSON *mbs_session_eventJSON)
{
    OpenAPI_mbs_session_event_t *mbs_session_event_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *event_type = NULL;
    OpenAPI_mbs_session_event_type_e event_typeVariable = 0;
    event_type = cJSON_GetObjectItemCaseSensitive(mbs_session_eventJSON, "eventType");
    if (!event_type) {
        ogs_error("OpenAPI_mbs_session_event_parseFromJSON() failed [event_type]");
        goto end;
    }
    if (!cJSON_IsString(event_type)) {
        ogs_error("OpenAPI_mbs_session_event_parseFromJSON() failed [event_type]");
        goto end;
    }
    event_typeVariable = OpenAPI_mbs_session_event_type_FromString(event_type->valuestring);

    mbs_session_event_local_var = OpenAPI_mbs_session_event_create (
        event_typeVariable
    );

    return mbs_session_event_local_var;
end:
    return NULL;
}

OpenAPI_mbs_session_event_t *OpenAPI_mbs_session_event_copy(OpenAPI_mbs_session_event_t *dst, OpenAPI_mbs_session_event_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_mbs_session_event_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_mbs_session_event_convertToJSON() failed");
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

    OpenAPI_mbs_session_event_free(dst);
    dst = OpenAPI_mbs_session_event_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

