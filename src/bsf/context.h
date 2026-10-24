/*
 * Copyright (C) 2019-2023 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef BSF_CONTEXT_H
#define BSF_CONTEXT_H

#include "ogs-sbi.h"
#include "ogs-app.h"

#include "bsf-sm.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int __bsf_log_domain;

#undef OGS_LOG_DOMAIN
#define OGS_LOG_DOMAIN __bsf_log_domain

typedef struct bsf_context_s {
    ogs_hash_t          *ipv4addr_hash;
    ogs_hash_t          *ipv6prefix_hash;
    ogs_hash_t          *mac_hash;
    ogs_list_t          secondary_list;

    ogs_list_t          sess_list;
} bsf_context_t;

typedef struct bsf_sess_s {
    ogs_sbi_object_t sbi;

    char *binding_id;

    char *supi;
    char *gpsi;
    char *ip_domain;

    char *ipv4addr_string;
    char *ipv6prefix_string;

    OpenAPI_list_t *ipv4_frame_route_list;
    OpenAPI_list_t *ipv6_frame_route_list;

    uint32_t ipv4addr;
    struct {
        uint8_t len;
        uint8_t addr6[OGS_IPV6_LEN];
    } ipv6prefix;

    ogs_s_nssai_t s_nssai;
    char *dnn;

    /* PCF address information */
    char *pcf_fqdn;

    int num_of_pcf_ip;
    struct {
        char *addr;
        char *addr6;
        bool is_port;
        int port;
    } pcf_ip[OGS_SBI_MAX_NUM_OF_IP_ADDRESS];

    /* the PcfBinding as the PCF registered it (TS 29.521 V19.7.0 clause 4.2.4.2) */
    OpenAPI_pcf_binding_t *registered;

    /*
     * Bindings of the same UE address that differ in ipDomain, supi, dnn or slice are kept in a chain per address, so a query
     * can find that more than one binding matches (TS 29.521 V19.7.0 table 5.3.2.3.2-3).
     */
    struct bsf_sess_s *ipv4_next;
    struct bsf_sess_s *ipv6_next;
    struct bsf_sess_s *mac_next;

    /* The MAC address of the UE (macAddr48), a key like the IP addresses */
    bool mac_present;
    uint8_t mac[6];

    /* A binding with addresses beyond the first (addIpv6Prefixes, addMacAddrs, framed routes) is in the secondary list */
    bool in_secondary_list;
    ogs_lnode_t secondary_node;

    /* SBI Features */
    uint64_t management_features;

} bsf_sess_t;

void bsf_context_init(void);
void bsf_context_final(void);
bsf_context_t *bsf_self(void);

int bsf_context_parse_config(void);

bsf_sess_t *bsf_sess_add_by_ip_address(
            char *ipv4addr_string, char *ipv6prefix_string);
void bsf_sess_remove(bsf_sess_t *sess);
void bsf_sess_remove_all(void);

bool bsf_sess_set_ipv4addr(bsf_sess_t *sess, char *ipv4addr);
bool bsf_sess_set_ipv6prefix(bsf_sess_t *sess, char *ipv6prefix);

bsf_sess_t *bsf_sess_find(uint32_t index);
bsf_sess_t *bsf_sess_find_by_snssai_and_dnn(ogs_s_nssai_t *s_nssai, char *dnn);
bsf_sess_t *bsf_sess_find_by_binding_id(char *binding_id);
/* The query of the retrieval of PCF bindings (TS 29.521 V19.7.0 table 5.3.2.3.2-1); the UE address is exactly one of the three */
typedef struct bsf_query_s {
    const char *ipv4addr;
    const char *ipv6prefix;
    const char *mac_addr48;
    const char *dnn;
    const char *supi;
    const char *gpsi;
    const char *ip_domain;
    bool snssai_presence;
    ogs_s_nssai_t s_nssai;
} bsf_query_t;

/*
 * The number of bindings that match the query (the binding of the longest matching prefix or framed route when the address is
 * not registered as such), and the first of them. *invalid is set when an address of the query cannot be parsed.
 */
int bsf_sess_query(const bsf_query_t *query, bsf_sess_t **found, bool *invalid);

/*
 * The binding that a registration replaces: the same UE address (IPv4 address, IPv6 prefix or MAC address) and the same ipDomain.
 * NULL if there is none.
 */
bsf_sess_t *bsf_sess_find_for_registration(
        const char *ipv4addr, const char *ipv6prefix, const char *mac_addr48,
        const char *ip_domain, bool *invalid);

/* Take the MAC address and the additional addresses of the registered binding into the indexes */
bool bsf_sess_index_registered(bsf_sess_t *sess);

int get_sess_load(void);

#ifdef __cplusplus
}
#endif

#endif /* BSF_CONTEXT_H */
