/*
 * ngran_failure_indication.h
 *
 * Indicates a NG-RAN failure event.
 */

#ifndef _OpenAPI_ngran_failure_indication_H_
#define _OpenAPI_ngran_failure_indication_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "ngran_failure_indication_any_of.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_ngran_failure_indication_s OpenAPI_ngran_failure_indication_t;
typedef struct OpenAPI_ngran_failure_indication_s {
} OpenAPI_ngran_failure_indication_t;

OpenAPI_ngran_failure_indication_t *OpenAPI_ngran_failure_indication_create(
);
void OpenAPI_ngran_failure_indication_free(OpenAPI_ngran_failure_indication_t *ngran_failure_indication);
OpenAPI_ngran_failure_indication_t *OpenAPI_ngran_failure_indication_parseFromJSON(cJSON *ngran_failure_indicationJSON);
cJSON *OpenAPI_ngran_failure_indication_convertToJSON(OpenAPI_ngran_failure_indication_t *ngran_failure_indication);
OpenAPI_ngran_failure_indication_t *OpenAPI_ngran_failure_indication_copy(OpenAPI_ngran_failure_indication_t *dst, OpenAPI_ngran_failure_indication_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_ngran_failure_indication_H_ */

