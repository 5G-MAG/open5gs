/*
 * tunnel_address.h
 *
 * Tunnel address
 */

#ifndef _OpenAPI_tunnel_address_H_
#define _OpenAPI_tunnel_address_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_tunnel_address_s OpenAPI_tunnel_address_t;
typedef struct OpenAPI_tunnel_address_s {
    char *ipv4_addr;
    char *ipv6_addr;
    int port_number;
} OpenAPI_tunnel_address_t;

OpenAPI_tunnel_address_t *OpenAPI_tunnel_address_create(
    char *ipv4_addr,
    char *ipv6_addr,
    int port_number
);
void OpenAPI_tunnel_address_free(OpenAPI_tunnel_address_t *tunnel_address);
OpenAPI_tunnel_address_t *OpenAPI_tunnel_address_parseFromJSON(cJSON *tunnel_addressJSON);
cJSON *OpenAPI_tunnel_address_convertToJSON(OpenAPI_tunnel_address_t *tunnel_address);
OpenAPI_tunnel_address_t *OpenAPI_tunnel_address_copy(OpenAPI_tunnel_address_t *dst, OpenAPI_tunnel_address_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_tunnel_address_H_ */

