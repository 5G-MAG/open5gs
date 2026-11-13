
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "problem_details_extension.h"

OpenAPI_problem_details_extension_t *OpenAPI_problem_details_extension_create(
    OpenAPI_acceptable_mbs_serv_info_t *acc_mbs_service_info
)
{
    OpenAPI_problem_details_extension_t *problem_details_extension_local_var = ogs_malloc(sizeof(OpenAPI_problem_details_extension_t));
    ogs_assert(problem_details_extension_local_var);

    problem_details_extension_local_var->acc_mbs_service_info = acc_mbs_service_info;

    return problem_details_extension_local_var;
}

void OpenAPI_problem_details_extension_free(OpenAPI_problem_details_extension_t *problem_details_extension)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == problem_details_extension) {
        return;
    }
    if (problem_details_extension->acc_mbs_service_info) {
        OpenAPI_acceptable_mbs_serv_info_free(problem_details_extension->acc_mbs_service_info);
        problem_details_extension->acc_mbs_service_info = NULL;
    }
    ogs_free(problem_details_extension);
}

cJSON *OpenAPI_problem_details_extension_convertToJSON(OpenAPI_problem_details_extension_t *problem_details_extension)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (problem_details_extension == NULL) {
        ogs_error("OpenAPI_problem_details_extension_convertToJSON() failed [ProblemDetailsExtension]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (problem_details_extension->acc_mbs_service_info) {
    cJSON *acc_mbs_service_info_local_JSON = OpenAPI_acceptable_mbs_serv_info_convertToJSON(problem_details_extension->acc_mbs_service_info);
    if (acc_mbs_service_info_local_JSON == NULL) {
        ogs_error("OpenAPI_problem_details_extension_convertToJSON() failed [acc_mbs_service_info]");
        goto end;
    }
    cJSON_AddItemToObject(item, "accMbsServiceInfo", acc_mbs_service_info_local_JSON);
    if (item->child == NULL) {
        ogs_error("OpenAPI_problem_details_extension_convertToJSON() failed [acc_mbs_service_info]");
        goto end;
    }
    }

end:
    return item;
}

OpenAPI_problem_details_extension_t *OpenAPI_problem_details_extension_parseFromJSON(cJSON *problem_details_extensionJSON)
{
    OpenAPI_problem_details_extension_t *problem_details_extension_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *acc_mbs_service_info = NULL;
    OpenAPI_acceptable_mbs_serv_info_t *acc_mbs_service_info_local_nonprim = NULL;
    acc_mbs_service_info = cJSON_GetObjectItemCaseSensitive(problem_details_extensionJSON, "accMbsServiceInfo");
    if (acc_mbs_service_info) {
    acc_mbs_service_info_local_nonprim = OpenAPI_acceptable_mbs_serv_info_parseFromJSON(acc_mbs_service_info);
    if (!acc_mbs_service_info_local_nonprim) {
        ogs_error("OpenAPI_acceptable_mbs_serv_info_parseFromJSON failed [acc_mbs_service_info]");
        goto end;
    }
    }

    problem_details_extension_local_var = OpenAPI_problem_details_extension_create (
        acc_mbs_service_info ? acc_mbs_service_info_local_nonprim : NULL
    );

    return problem_details_extension_local_var;
end:
    if (acc_mbs_service_info_local_nonprim) {
        OpenAPI_acceptable_mbs_serv_info_free(acc_mbs_service_info_local_nonprim);
        acc_mbs_service_info_local_nonprim = NULL;
    }
    return NULL;
}

OpenAPI_problem_details_extension_t *OpenAPI_problem_details_extension_copy(OpenAPI_problem_details_extension_t *dst, OpenAPI_problem_details_extension_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_problem_details_extension_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_problem_details_extension_convertToJSON() failed");
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

    OpenAPI_problem_details_extension_free(dst);
    dst = OpenAPI_problem_details_extension_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

