
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_status_event.h"

OpenAPI_context_status_event_t *OpenAPI_context_status_event_create(
    OpenAPI_context_status_event_type_e event_type,
    bool is_immediate_report_ind,
    int immediate_report_ind,
    OpenAPI_reporting_mode_e reporting_mode
)
{
    OpenAPI_context_status_event_t *context_status_event_local_var = ogs_malloc(sizeof(OpenAPI_context_status_event_t));
    ogs_assert(context_status_event_local_var);

    context_status_event_local_var->event_type = event_type;
    context_status_event_local_var->is_immediate_report_ind = is_immediate_report_ind;
    context_status_event_local_var->immediate_report_ind = immediate_report_ind;
    context_status_event_local_var->reporting_mode = reporting_mode;

    return context_status_event_local_var;
}

void OpenAPI_context_status_event_free(OpenAPI_context_status_event_t *context_status_event)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == context_status_event) {
        return;
    }
    ogs_free(context_status_event);
}

cJSON *OpenAPI_context_status_event_convertToJSON(OpenAPI_context_status_event_t *context_status_event)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (context_status_event == NULL) {
        ogs_error("OpenAPI_context_status_event_convertToJSON() failed [ContextStatusEvent]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (context_status_event->event_type == OpenAPI_context_status_event_type_NULL) {
        ogs_error("OpenAPI_context_status_event_convertToJSON() failed [event_type]");
        return NULL;
    }
    if (cJSON_AddStringToObject(item, "eventType", OpenAPI_context_status_event_type_ToString(context_status_event->event_type)) == NULL) {
        ogs_error("OpenAPI_context_status_event_convertToJSON() failed [event_type]");
        goto end;
    }

    if (context_status_event->is_immediate_report_ind) {
    if (cJSON_AddBoolToObject(item, "immediateReportInd", context_status_event->immediate_report_ind) == NULL) {
        ogs_error("OpenAPI_context_status_event_convertToJSON() failed [immediate_report_ind]");
        goto end;
    }
    }

    if (context_status_event->reporting_mode != OpenAPI_reporting_mode_NULL) {
    if (cJSON_AddStringToObject(item, "reportingMode", OpenAPI_reporting_mode_ToString(context_status_event->reporting_mode)) == NULL) {
        ogs_error("OpenAPI_context_status_event_convertToJSON() failed [reporting_mode]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_context_status_event_t *OpenAPI_context_status_event_parseFromJSON(cJSON *context_status_eventJSON)
{
    OpenAPI_context_status_event_t *context_status_event_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *event_type = NULL;
    OpenAPI_context_status_event_type_e event_typeVariable = 0;
    cJSON *immediate_report_ind = NULL;
    cJSON *reporting_mode = NULL;
    OpenAPI_reporting_mode_e reporting_modeVariable = 0;
    event_type = cJSON_GetObjectItemCaseSensitive(context_status_eventJSON, "eventType");
    if (!event_type) {
        ogs_error("OpenAPI_context_status_event_parseFromJSON() failed [event_type]");
        goto end;
    }
    if (!cJSON_IsString(event_type)) {
        ogs_error("OpenAPI_context_status_event_parseFromJSON() failed [event_type]");
        goto end;
    }
    event_typeVariable = OpenAPI_context_status_event_type_FromString(event_type->valuestring);

    immediate_report_ind = cJSON_GetObjectItemCaseSensitive(context_status_eventJSON, "immediateReportInd");
    if (immediate_report_ind) {
    if (!cJSON_IsBool(immediate_report_ind)) {
        ogs_error("OpenAPI_context_status_event_parseFromJSON() failed [immediate_report_ind]");
        goto end;
    }
    }

    reporting_mode = cJSON_GetObjectItemCaseSensitive(context_status_eventJSON, "reportingMode");
    if (reporting_mode) {
    if (!cJSON_IsString(reporting_mode)) {
        ogs_error("OpenAPI_context_status_event_parseFromJSON() failed [reporting_mode]");
        goto end;
    }
    reporting_modeVariable = OpenAPI_reporting_mode_FromString(reporting_mode->valuestring);
    }

    context_status_event_local_var = OpenAPI_context_status_event_create (
        event_typeVariable,
        immediate_report_ind ? true : false,
        immediate_report_ind ? immediate_report_ind->valueint : 0,
        reporting_mode ? reporting_modeVariable : 0
    );

    return context_status_event_local_var;
end:
    return NULL;
}

OpenAPI_context_status_event_t *OpenAPI_context_status_event_copy(OpenAPI_context_status_event_t *dst, OpenAPI_context_status_event_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_context_status_event_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_context_status_event_convertToJSON() failed");
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

    OpenAPI_context_status_event_free(dst);
    dst = OpenAPI_context_status_event_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

