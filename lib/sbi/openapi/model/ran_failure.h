/*
 * ran_failure.h
 *
 * Description of an MBS related N2 procedure failure
 */

#ifndef _OpenAPI_ran_failure_H_
#define _OpenAPI_ran_failure_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_ran_failure_s OpenAPI_ran_failure_t;
#include "global_ran_node_id.h"
#include "ng_ap_cause.h"
#include "ran_failure_indication.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_ran_failure_s {
    struct OpenAPI_global_ran_node_id_s *ran_id;
    struct OpenAPI_ng_ap_cause_s *ran_failure_cause;
    OpenAPI_ran_failure_indication_e ran_failure_indication;
};

OpenAPI_ran_failure_t *OpenAPI_ran_failure_create(
    OpenAPI_global_ran_node_id_t *ran_id,
    OpenAPI_ng_ap_cause_t *ran_failure_cause,
    OpenAPI_ran_failure_indication_e ran_failure_indication
);
void OpenAPI_ran_failure_free(OpenAPI_ran_failure_t *ran_failure);
OpenAPI_ran_failure_t *OpenAPI_ran_failure_parseFromJSON(cJSON *ran_failureJSON);
cJSON *OpenAPI_ran_failure_convertToJSON(OpenAPI_ran_failure_t *ran_failure);
OpenAPI_ran_failure_t *OpenAPI_ran_failure_copy(OpenAPI_ran_failure_t *dst, OpenAPI_ran_failure_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_ran_failure_H_ */

