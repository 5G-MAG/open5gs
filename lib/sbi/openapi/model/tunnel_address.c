
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "tunnel_address.h"

OpenAPI_tunnel_address_t *OpenAPI_tunnel_address_create(
    char *ipv4_addr,
    char *ipv6_addr,
    int port_number
)
{
    OpenAPI_tunnel_address_t *tunnel_address_local_var = ogs_malloc(sizeof(OpenAPI_tunnel_address_t));
    ogs_assert(tunnel_address_local_var);

    tunnel_address_local_var->ipv4_addr = ipv4_addr;
    tunnel_address_local_var->ipv6_addr = ipv6_addr;
    tunnel_address_local_var->port_number = port_number;

    return tunnel_address_local_var;
}

void OpenAPI_tunnel_address_free(OpenAPI_tunnel_address_t *tunnel_address)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == tunnel_address) {
        return;
    }
    if (tunnel_address->ipv4_addr) {
        ogs_free(tunnel_address->ipv4_addr);
        tunnel_address->ipv4_addr = NULL;
    }
    if (tunnel_address->ipv6_addr) {
        ogs_free(tunnel_address->ipv6_addr);
        tunnel_address->ipv6_addr = NULL;
    }
    ogs_free(tunnel_address);
}

cJSON *OpenAPI_tunnel_address_convertToJSON(OpenAPI_tunnel_address_t *tunnel_address)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (tunnel_address == NULL) {
        ogs_error("OpenAPI_tunnel_address_convertToJSON() failed [TunnelAddress]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (tunnel_address->ipv4_addr) {
    if (cJSON_AddStringToObject(item, "ipv4Addr", tunnel_address->ipv4_addr) == NULL) {
        ogs_error("OpenAPI_tunnel_address_convertToJSON() failed [ipv4_addr]");
        goto end;
    }
    }

    if (tunnel_address->ipv6_addr) {
    if (cJSON_AddStringToObject(item, "ipv6Addr", tunnel_address->ipv6_addr) == NULL) {
        ogs_error("OpenAPI_tunnel_address_convertToJSON() failed [ipv6_addr]");
        goto end;
    }
    }

    if (cJSON_AddNumberToObject(item, "portNumber", tunnel_address->port_number) == NULL) {
        ogs_error("OpenAPI_tunnel_address_convertToJSON() failed [port_number]");
        goto end;
    }

end:
    return item;
}

OpenAPI_tunnel_address_t *OpenAPI_tunnel_address_parseFromJSON(cJSON *tunnel_addressJSON)
{
    OpenAPI_tunnel_address_t *tunnel_address_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *ipv4_addr = NULL;
    cJSON *ipv6_addr = NULL;
    cJSON *port_number = NULL;
    ipv4_addr = cJSON_GetObjectItemCaseSensitive(tunnel_addressJSON, "ipv4Addr");
    if (ipv4_addr) {
    if (!cJSON_IsString(ipv4_addr) && !cJSON_IsNull(ipv4_addr)) {
        ogs_error("OpenAPI_tunnel_address_parseFromJSON() failed [ipv4_addr]");
        goto end;
    }
    }

    ipv6_addr = cJSON_GetObjectItemCaseSensitive(tunnel_addressJSON, "ipv6Addr");
    if (ipv6_addr) {
    if (!cJSON_IsString(ipv6_addr) && !cJSON_IsNull(ipv6_addr)) {
        ogs_error("OpenAPI_tunnel_address_parseFromJSON() failed [ipv6_addr]");
        goto end;
    }
    }

    port_number = cJSON_GetObjectItemCaseSensitive(tunnel_addressJSON, "portNumber");
    if (!port_number) {
        ogs_error("OpenAPI_tunnel_address_parseFromJSON() failed [port_number]");
        goto end;
    }
    if (!cJSON_IsNumber(port_number)) {
        ogs_error("OpenAPI_tunnel_address_parseFromJSON() failed [port_number]");
        goto end;
    }

    tunnel_address_local_var = OpenAPI_tunnel_address_create (
        ipv4_addr && !cJSON_IsNull(ipv4_addr) ? ogs_strdup(ipv4_addr->valuestring) : NULL,
        ipv6_addr && !cJSON_IsNull(ipv6_addr) ? ogs_strdup(ipv6_addr->valuestring) : NULL,
        
        port_number->valuedouble
    );

    return tunnel_address_local_var;
end:
    return NULL;
}

OpenAPI_tunnel_address_t *OpenAPI_tunnel_address_copy(OpenAPI_tunnel_address_t *dst, OpenAPI_tunnel_address_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_tunnel_address_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_tunnel_address_convertToJSON() failed");
        return NULL;
    }

    content = cJSON_Print(item);
    cJSON_Delete(item);

    if (!content) {
        ogs_error("cJSON_Print() failed");
        return NULL;
    }

    item = cJSON_Parse(content);
    ogs_free(content);
    if (!item) {
        ogs_error("cJSON_Parse() failed");
        return NULL;
    }

    OpenAPI_tunnel_address_free(dst);
    dst = OpenAPI_tunnel_address_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

