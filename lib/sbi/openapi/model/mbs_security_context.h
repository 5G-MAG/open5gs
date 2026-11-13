/*
 * mbs_security_context.h
 *
 * MBS security context consisting of MSK/MTK(s) and associated IDs
 */

#ifndef _OpenAPI_mbs_security_context_H_
#define _OpenAPI_mbs_security_context_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_security_context_s OpenAPI_mbs_security_context_t;
#include "mbs_key_info.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_security_context_s {
    OpenAPI_list_t* key_list;
};

OpenAPI_mbs_security_context_t *OpenAPI_mbs_security_context_create(
    OpenAPI_list_t* key_list
);
void OpenAPI_mbs_security_context_free(OpenAPI_mbs_security_context_t *mbs_security_context);
OpenAPI_mbs_security_context_t *OpenAPI_mbs_security_context_parseFromJSON(cJSON *mbs_security_contextJSON);
cJSON *OpenAPI_mbs_security_context_convertToJSON(OpenAPI_mbs_security_context_t *mbs_security_context);
OpenAPI_mbs_security_context_t *OpenAPI_mbs_security_context_copy(OpenAPI_mbs_security_context_t *dst, OpenAPI_mbs_security_context_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_security_context_H_ */

