/*
 * multicast_transport_address_change_info.h
 *
 * Multicast Transport Address Change Information
 */

#ifndef _OpenAPI_multicast_transport_address_change_info_H_
#define _OpenAPI_multicast_transport_address_change_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "ssm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_multicast_transport_address_change_info_s OpenAPI_multicast_transport_address_change_info_t;
typedef struct OpenAPI_multicast_transport_address_change_info_s {
    struct OpenAPI_ssm_s *ll_ssm;
    int c_teid;
    bool is_area_session_id;
    int area_session_id;
} OpenAPI_multicast_transport_address_change_info_t;

OpenAPI_multicast_transport_address_change_info_t *OpenAPI_multicast_transport_address_change_info_create(
    OpenAPI_ssm_t *ll_ssm,
    int c_teid,
    bool is_area_session_id,
    int area_session_id
);
void OpenAPI_multicast_transport_address_change_info_free(OpenAPI_multicast_transport_address_change_info_t *multicast_transport_address_change_info);
OpenAPI_multicast_transport_address_change_info_t *OpenAPI_multicast_transport_address_change_info_parseFromJSON(cJSON *multicast_transport_address_change_infoJSON);
cJSON *OpenAPI_multicast_transport_address_change_info_convertToJSON(OpenAPI_multicast_transport_address_change_info_t *multicast_transport_address_change_info);
OpenAPI_multicast_transport_address_change_info_t *OpenAPI_multicast_transport_address_change_info_copy(OpenAPI_multicast_transport_address_change_info_t *dst, OpenAPI_multicast_transport_address_change_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_multicast_transport_address_change_info_H_ */

