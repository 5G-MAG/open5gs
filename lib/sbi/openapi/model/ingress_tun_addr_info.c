
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ingress_tun_addr_info.h"

OpenAPI_ingress_tun_addr_info_t *OpenAPI_ingress_tun_addr_info_create(
    OpenAPI_list_t *ingress_tun_addr
)
{
    OpenAPI_ingress_tun_addr_info_t *ingress_tun_addr_info_local_var = ogs_malloc(sizeof(OpenAPI_ingress_tun_addr_info_t));
    ogs_assert(ingress_tun_addr_info_local_var);

    ingress_tun_addr_info_local_var->ingress_tun_addr = ingress_tun_addr;

    return ingress_tun_addr_info_local_var;
}

void OpenAPI_ingress_tun_addr_info_free(OpenAPI_ingress_tun_addr_info_t *ingress_tun_addr_info)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == ingress_tun_addr_info) {
        return;
    }
    if (ingress_tun_addr_info->ingress_tun_addr) {
        OpenAPI_list_for_each(ingress_tun_addr_info->ingress_tun_addr, node) {
            OpenAPI_tunnel_address_free(node->data);
        }
        OpenAPI_list_free(ingress_tun_addr_info->ingress_tun_addr);
        ingress_tun_addr_info->ingress_tun_addr = NULL;
    }
    ogs_free(ingress_tun_addr_info);
}

cJSON *OpenAPI_ingress_tun_addr_info_convertToJSON(OpenAPI_ingress_tun_addr_info_t *ingress_tun_addr_info)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;

    if (ingress_tun_addr_info == NULL) {
        ogs_error("OpenAPI_ingress_tun_addr_info_convertToJSON() failed [IngressTunAddrInfo]");
        return NULL;
    }

    item = cJSON_CreateObject();
    if (!ingress_tun_addr_info->ingress_tun_addr) {
        ogs_error("OpenAPI_ingress_tun_addr_info_convertToJSON() failed [ingress_tun_addr]");
        return NULL;
    }
    cJSON *ingress_tun_addrList = cJSON_AddArrayToObject(item, "ingressTunAddr");
    if (ingress_tun_addrList == NULL) {
        ogs_error("OpenAPI_ingress_tun_addr_info_convertToJSON() failed [ingress_tun_addr]");
        goto end;
    }
    OpenAPI_list_for_each(ingress_tun_addr_info->ingress_tun_addr, node) {
        cJSON *itemLocal = OpenAPI_tunnel_address_convertToJSON(node->data);
        if (itemLocal == NULL) {
            ogs_error("OpenAPI_ingress_tun_addr_info_convertToJSON() failed [ingress_tun_addr]");
            goto end;
        }
        cJSON_AddItemToArray(ingress_tun_addrList, itemLocal);
    }

end:
    return item;
}

OpenAPI_ingress_tun_addr_info_t *OpenAPI_ingress_tun_addr_info_parseFromJSON(cJSON *ingress_tun_addr_infoJSON)
{
    OpenAPI_ingress_tun_addr_info_t *ingress_tun_addr_info_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *ingress_tun_addr = NULL;
    OpenAPI_list_t *ingress_tun_addrList = NULL;
    ingress_tun_addr = cJSON_GetObjectItemCaseSensitive(ingress_tun_addr_infoJSON, "ingressTunAddr");
    if (!ingress_tun_addr) {
        ogs_error("OpenAPI_ingress_tun_addr_info_parseFromJSON() failed [ingress_tun_addr]");
        goto end;
    }
        cJSON *ingress_tun_addr_local = NULL;
        if (!cJSON_IsArray(ingress_tun_addr)) {
            ogs_error("OpenAPI_ingress_tun_addr_info_parseFromJSON() failed [ingress_tun_addr]");
            goto end;
        }

        ingress_tun_addrList = OpenAPI_list_create();

        cJSON_ArrayForEach(ingress_tun_addr_local, ingress_tun_addr) {
            if (!cJSON_IsObject(ingress_tun_addr_local)) {
                ogs_error("OpenAPI_ingress_tun_addr_info_parseFromJSON() failed [ingress_tun_addr]");
                goto end;
            }
            OpenAPI_tunnel_address_t *ingress_tun_addrItem = OpenAPI_tunnel_address_parseFromJSON(ingress_tun_addr_local);
            if (!ingress_tun_addrItem) {
                ogs_error("No ingress_tun_addrItem");
                goto end;
            }
            OpenAPI_list_add(ingress_tun_addrList, ingress_tun_addrItem);
        }

    ingress_tun_addr_info_local_var = OpenAPI_ingress_tun_addr_info_create (
        ingress_tun_addrList
    );

    return ingress_tun_addr_info_local_var;
end:
    if (ingress_tun_addrList) {
        OpenAPI_list_for_each(ingress_tun_addrList, node) {
            OpenAPI_tunnel_address_free(node->data);
        }
        OpenAPI_list_free(ingress_tun_addrList);
        ingress_tun_addrList = NULL;
    }
    return NULL;
}

OpenAPI_ingress_tun_addr_info_t *OpenAPI_ingress_tun_addr_info_copy(OpenAPI_ingress_tun_addr_info_t *dst, OpenAPI_ingress_tun_addr_info_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_ingress_tun_addr_info_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_ingress_tun_addr_info_convertToJSON() failed");
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

    OpenAPI_ingress_tun_addr_info_free(dst);
    dst = OpenAPI_ingress_tun_addr_info_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

