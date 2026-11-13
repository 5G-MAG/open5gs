/*
 * associated_session_id.h
 *
 * an associated Session Id used in MOCN
 */

#ifndef _OpenAPI_associated_session_id_H_
#define _OpenAPI_associated_session_id_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_associated_session_id_s OpenAPI_associated_session_id_t;
#include "ip_addr.h"
#include "ssm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_associated_session_id_s {
    struct OpenAPI_ip_addr_s *source_ip_addr;
    struct OpenAPI_ip_addr_s *dest_ip_addr;
};

OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_create(
    OpenAPI_ip_addr_t *source_ip_addr,
    OpenAPI_ip_addr_t *dest_ip_addr
);
void OpenAPI_associated_session_id_free(OpenAPI_associated_session_id_t *associated_session_id);
OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_parseFromJSON(cJSON *associated_session_idJSON);
cJSON *OpenAPI_associated_session_id_convertToJSON(OpenAPI_associated_session_id_t *associated_session_id);
OpenAPI_associated_session_id_t *OpenAPI_associated_session_id_copy(OpenAPI_associated_session_id_t *dst, OpenAPI_associated_session_id_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_associated_session_id_H_ */

