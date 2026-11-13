
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ran_failure.h"

OpenAPI_ran_failure_t *OpenAPI_ran_failure_create(
    OpenAPI_global_ran_node_id_t *ran_id,
    OpenAPI_ng_ap_cause_t *ran_failure_cause,
    OpenAPI_ran_failure_indication_e ran_failure_indication
)
{
    OpenAPI_ran_failure_t *ran_failure_local_var = ogs_malloc(sizeof(OpenAPI_ran_failure_t));
    ogs_assert(ran_failure_local_var);

    ran_failure_local_var->ran_id = ran_id;
    ran_failure_local_var->ran_failure_cause = ran_failure_cause;
    ran_failure_local_var->ran_failure_indication = ran_failure_indication;

    return ran_failure_local_var;
}

void OpenAPI_ran_failure_free(OpenAPI_ran_failure_t *ran_failure)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == ran_failure) {
        return;
    }
    if (ran_failure->ran_id) {
        OpenAPI_global_ran_node_id_free(ran_failure->ran_id);
        ran_failure->ran_id = NULL;
    }
    if (ran_failure->ran_failure_cause) {
        OpenAPI_ng_ap_cause_free(ran_failure->ran_failure_cause);
        ran_failure->ran_failure_cause = NULL;
    }
    ogs_free(ran_failure);
}

cJSON *OpenAPI_ran_failure_convertToJSON(OpenAPI_ran_failure_t *ran_failure)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (ran_failure == NULL) {
        ogs_error("OpenAPI_ran_failure_convertToJSON() failed [RanFailure]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!ran_failure->ran_id) {
        ogs_error("OpenAPI_ran_failure_convertToJSON() failed [ran_id]");
        return NULL;
    }
    cJSON *ran_id_local_JSON = OpenAPI_global_ran_node_id_convertToJSON(ran_failure->ran_id);
    if (ran_id_local_JSON == NULL) {
        ogs_error("OpenAPI_ran_failure_convertToJSON() failed [ran_id]");
        goto end;
    }
    cJSON_AddItemToObject(item, "ranId", ran_id_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ran_failure_convertToJSON() failed [ran_id]");
        goto end;
    }

    if (ran_failure->ran_failure_cause) {
    cJSON *ran_failure_cause_local_JSON = OpenAPI_ng_ap_cause_convertToJSON(ran_failure->ran_failure_cause);
    if (ran_failure_cause_local_JSON == NULL) {
        ogs_error("OpenAPI_ran_failure_convertToJSON() failed [ran_failure_cause]");
        goto end;
    }
    cJSON_AddItemToObject(item, "ranFailureCause", ran_failure_cause_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_ran_failure_convertToJSON() failed [ran_failure_cause]");
        goto end;
    }
    }

    if (ran_failure->ran_failure_indication != OpenAPI_ran_failure_indication_NULL) {
    if (cJSON_AddStringToObject(item, "ranFailureIndication", OpenAPI_ran_failure_indication_ToString(ran_failure->ran_failure_indication)) == NULL) {
        ogs_error("OpenAPI_ran_failure_convertToJSON() failed [ran_failure_indication]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_ran_failure_t *OpenAPI_ran_failure_parseFromJSON(cJSON *ran_failureJSON)
{
    OpenAPI_ran_failure_t *ran_failure_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *ran_id = NULL;
    OpenAPI_global_ran_node_id_t *ran_id_local_nonprim = NULL;
    cJSON *ran_failure_cause = NULL;
    OpenAPI_ng_ap_cause_t *ran_failure_cause_local_nonprim = NULL;
    cJSON *ran_failure_indication = NULL;
    OpenAPI_ran_failure_indication_e ran_failure_indicationVariable = 0;
    ran_id = cJSON_GetObjectItemCaseSensitive(ran_failureJSON, "ranId");
    if (!ran_id) {
        ogs_error("OpenAPI_ran_failure_parseFromJSON() failed [ran_id]");
        goto end;
    }
    ran_id_local_nonprim = OpenAPI_global_ran_node_id_parseFromJSON(ran_id);
    if (!ran_id_local_nonprim) {
        ogs_error("OpenAPI_global_ran_node_id_parseFromJSON failed [ran_id]");
        goto end;
    }

    ran_failure_cause = cJSON_GetObjectItemCaseSensitive(ran_failureJSON, "ranFailureCause");
    if (ran_failure_cause) {
    ran_failure_cause_local_nonprim = OpenAPI_ng_ap_cause_parseFromJSON(ran_failure_cause);
    if (!ran_failure_cause_local_nonprim) {
        ogs_error("OpenAPI_ng_ap_cause_parseFromJSON failed [ran_failure_cause]");
        goto end;
    }
    }

    ran_failure_indication = cJSON_GetObjectItemCaseSensitive(ran_failureJSON, "ranFailureIndication");
    if (ran_failure_indication) {
    if (!cJSON_IsString(ran_failure_indication)) {
        ogs_error("OpenAPI_ran_failure_parseFromJSON() failed [ran_failure_indication]");
        goto end;
    }
    ran_failure_indicationVariable = OpenAPI_ran_failure_indication_FromString(ran_failure_indication->valuestring);
    }

    ran_failure_local_var = OpenAPI_ran_failure_create (
        ran_id_local_nonprim,
        ran_failure_cause ? ran_failure_cause_local_nonprim : NULL,
        ran_failure_indication ? ran_failure_indicationVariable : 0
    );

    return ran_failure_local_var;
end:
    if (ran_id_local_nonprim) {
        OpenAPI_global_ran_node_id_free(ran_id_local_nonprim);
        ran_id_local_nonprim = NULL;
    }
    if (ran_failure_cause_local_nonprim) {
        OpenAPI_ng_ap_cause_free(ran_failure_cause_local_nonprim);
        ran_failure_cause_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_ran_failure_t *OpenAPI_ran_failure_copy(OpenAPI_ran_failure_t *dst, OpenAPI_ran_failure_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_ran_failure_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_ran_failure_convertToJSON() failed");
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

    OpenAPI_ran_failure_free(dst);
    dst = OpenAPI_ran_failure_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

