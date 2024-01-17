/*
 * ingress_tun_addr_info.h
 *
 * Ingress Tunnel Address Information
 */

#ifndef _OpenAPI_ingress_tun_addr_info_H_
#define _OpenAPI_ingress_tun_addr_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "tunnel_address.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_ingress_tun_addr_info_s OpenAPI_ingress_tun_addr_info_t;
typedef struct OpenAPI_ingress_tun_addr_info_s {
    OpenAPI_list_t *ingress_tun_addr;
} OpenAPI_ingress_tun_addr_info_t;

OpenAPI_ingress_tun_addr_info_t *OpenAPI_ingress_tun_addr_info_create(
    OpenAPI_list_t *ingress_tun_addr
);
void OpenAPI_ingress_tun_addr_info_free(OpenAPI_ingress_tun_addr_info_t *ingress_tun_addr_info);
OpenAPI_ingress_tun_addr_info_t *OpenAPI_ingress_tun_addr_info_parseFromJSON(cJSON *ingress_tun_addr_infoJSON);
cJSON *OpenAPI_ingress_tun_addr_info_convertToJSON(OpenAPI_ingress_tun_addr_info_t *ingress_tun_addr_info);
OpenAPI_ingress_tun_addr_info_t *OpenAPI_ingress_tun_addr_info_copy(OpenAPI_ingress_tun_addr_info_t *dst, OpenAPI_ingress_tun_addr_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_ingress_tun_addr_info_H_ */

