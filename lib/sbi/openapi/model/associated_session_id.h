/*
 * associated_session_id.h
 *
 * An associated Session Id used in MOCN
 */

#ifndef _OpenAPI_associated_session_id_H_
#define _OpenAPI_associated_session_id_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "ssm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TS 29.571 V18.12.0 cl. 5.9.4.21.1: AssociatedSessionId is an anyOf of an Ssm and a string; this is the
 * representation of whichever of the two the value is (exactly one member is set). Hand-written like the other
 * Release 18 additions to this model tree, not generated. */
typedef struct OpenAPI_associated_session_id_s OpenAPI_associated_session_id_t;
typedef struct OpenAPI_associated_session_id_s {
    struct OpenAPI_ssm_s *ssm;
    char *string;
} OpenAPI_associated_session_id_t;

OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_create(
    OpenAPI_ssm_t *ssm,
    char *string
);
void OpenAPI_associated_session_id_free(OpenAPI_associated_session_id_t *associated_session_id);
OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_parseFromJSON(cJSON *associated_session_idJSON);
cJSON *OpenAPI_associated_session_id_convertToJSON(OpenAPI_associated_session_id_t *associated_session_id);
OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_copy(OpenAPI_associated_session_id_t *dst, OpenAPI_associated_session_id_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_associated_session_id_H_ */
