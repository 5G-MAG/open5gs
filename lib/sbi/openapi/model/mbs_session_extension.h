/*
 * mbs_session_extension.h
 *
 * MB-SMF API specific extensions to the MbsSession common data type
 */

#ifndef _OpenAPI_mbs_session_extension_H_
#define _OpenAPI_mbs_session_extension_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_session_extension_s OpenAPI_mbs_session_extension_t;
#include "mbs_security_context.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_session_extension_s {
    struct OpenAPI_mbs_security_context_s *mbs_security_context;
    bool is_contact_pcf_ind;
    int contact_pcf_ind;
    bool is_area_session_policy_id;
    int area_session_policy_id;
};

OpenAPI_mbs_session_extension_t *OpenAPI_mbs_session_extension_create(
    OpenAPI_mbs_security_context_t *mbs_security_context,
    bool is_contact_pcf_ind,
    int contact_pcf_ind,
    bool is_area_session_policy_id,
    int area_session_policy_id
);
void OpenAPI_mbs_session_extension_free(OpenAPI_mbs_session_extension_t *mbs_session_extension);
OpenAPI_mbs_session_extension_t *OpenAPI_mbs_session_extension_parseFromJSON(cJSON *mbs_session_extensionJSON);
cJSON *OpenAPI_mbs_session_extension_convertToJSON(OpenAPI_mbs_session_extension_t *mbs_session_extension);
OpenAPI_mbs_session_extension_t *OpenAPI_mbs_session_extension_copy(OpenAPI_mbs_session_extension_t *dst, OpenAPI_mbs_session_extension_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_session_extension_H_ */

