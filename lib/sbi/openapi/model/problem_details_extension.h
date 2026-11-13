/*
 * problem_details_extension.h
 *
 * Extensions to ProblemDetails
 */

#ifndef _OpenAPI_problem_details_extension_H_
#define _OpenAPI_problem_details_extension_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_problem_details_extension_s OpenAPI_problem_details_extension_t;
#include "acceptable_mbs_serv_info.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_problem_details_extension_s {
    struct OpenAPI_acceptable_mbs_serv_info_s *acc_mbs_service_info;
};

OpenAPI_problem_details_extension_t *OpenAPI_problem_details_extension_create(
    OpenAPI_acceptable_mbs_serv_info_t *acc_mbs_service_info
);
void OpenAPI_problem_details_extension_free(OpenAPI_problem_details_extension_t *problem_details_extension);
OpenAPI_problem_details_extension_t *OpenAPI_problem_details_extension_parseFromJSON(cJSON *problem_details_extensionJSON);
cJSON *OpenAPI_problem_details_extension_convertToJSON(OpenAPI_problem_details_extension_t *problem_details_extension);
OpenAPI_problem_details_extension_t *OpenAPI_problem_details_extension_copy(OpenAPI_problem_details_extension_t *dst, OpenAPI_problem_details_extension_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_problem_details_extension_H_ */

