/*
 * mbs_session_event.h
 *
 * MBS session event
 */

#ifndef _OpenAPI_mbs_session_event_H_
#define _OpenAPI_mbs_session_event_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_session_event_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_session_event_s OpenAPI_mbs_session_event_t;
typedef struct OpenAPI_mbs_session_event_s {
    OpenAPI_mbs_session_event_type_e event_type;
} OpenAPI_mbs_session_event_t;

OpenAPI_mbs_session_event_t *OpenAPI_mbs_session_event_create(
    OpenAPI_mbs_session_event_type_e event_type
);
void OpenAPI_mbs_session_event_free(OpenAPI_mbs_session_event_t *mbs_session_event);
OpenAPI_mbs_session_event_t *OpenAPI_mbs_session_event_parseFromJSON(cJSON *mbs_session_eventJSON);
cJSON *OpenAPI_mbs_session_event_convertToJSON(OpenAPI_mbs_session_event_t *mbs_session_event);
OpenAPI_mbs_session_event_t *OpenAPI_mbs_session_event_copy(OpenAPI_mbs_session_event_t *dst, OpenAPI_mbs_session_event_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_session_event_H_ */

