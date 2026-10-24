/*
 * Copyright (C) 2019-2022 by Sukchan Lee <acetcom@gmail.com>
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

#include "context.h"

/*
 * The IPv6 prefixes are kept by length and masked address: the key is the length byte and the bytes of the prefix, so that
 * the longest prefix that contains an address is found by looking up its masks from the longest to the shortest.
 */
static int ipv6prefix_key_len(uint8_t len)
{
    return 1 + ((len + 7) >> 3);
}

static void ipv6prefix_mask(uint8_t *addr6, uint8_t len)
{
    int i;

    for (i = 0; i < OGS_IPV6_LEN; i++) {
        int bits = len - 8 * i;

        if (bits >= 8)
            continue;
        addr6[i] = bits <= 0 ? 0 : (addr6[i] & (uint8_t)(0xff << (8 - bits)));
    }
}

static bsf_context_t self;

/*
 * The bindings of one UE address form a chain (the head is what the hash holds): the key lives in the head, so it is given to the
 * hash again when the head changes.
 */
typedef struct chain_kind_s {
    ogs_hash_t *(*hash)(void);
    bsf_sess_t **(*next)(bsf_sess_t *sess);
    const void *(*key)(bsf_sess_t *sess);
    int (*key_len)(bsf_sess_t *sess);
} chain_kind_t;

static ogs_hash_t *v4_hash(void) { return self.ipv4addr_hash; }
static bsf_sess_t **v4_next(bsf_sess_t *s) { return &s->ipv4_next; }
static const void *v4_key(bsf_sess_t *s) { return &s->ipv4addr; }
static int v4_key_len(bsf_sess_t *s) { return sizeof(s->ipv4addr); }
static ogs_hash_t *v6_hash(void) { return self.ipv6prefix_hash; }
static bsf_sess_t **v6_next(bsf_sess_t *s) { return &s->ipv6_next; }
static const void *v6_key(bsf_sess_t *s) { return &s->ipv6prefix; }
static int v6_key_len(bsf_sess_t *s) { return ipv6prefix_key_len(s->ipv6prefix.len); }
static ogs_hash_t *mac_hash(void) { return self.mac_hash; }
static bsf_sess_t **mac_next(bsf_sess_t *s) { return &s->mac_next; }
static const void *mac_key(bsf_sess_t *s) { return s->mac; }
static int mac_key_len(bsf_sess_t *s) { return sizeof(s->mac); }

static const chain_kind_t chain_v4 = { v4_hash, v4_next, v4_key, v4_key_len };
static const chain_kind_t chain_v6 = { v6_hash, v6_next, v6_key, v6_key_len };
static const chain_kind_t chain_mac = { mac_hash, mac_next, mac_key, mac_key_len };

static void chain_add(const chain_kind_t *kind, bsf_sess_t *sess)
{
    bsf_sess_t *head = ogs_hash_get(kind->hash(), kind->key(sess), kind->key_len(sess));

    *kind->next(sess) = head;
    /* the hash keeps the key pointer of the entry it was given first: the entry is made again with the key of the new head */
    if (head)
        ogs_hash_set(kind->hash(), kind->key(head), kind->key_len(head), NULL);
    ogs_hash_set(kind->hash(), kind->key(sess), kind->key_len(sess), sess);
}

static void chain_del(const chain_kind_t *kind, bsf_sess_t *sess)
{
    bsf_sess_t *head = ogs_hash_get(kind->hash(), kind->key(sess), kind->key_len(sess));
    bsf_sess_t *cur = head, *prev = NULL;

    while (cur && cur != sess) {
        prev = cur;
        cur = *kind->next(cur);
    }
    if (!cur) return;

    if (prev) {
        *kind->next(prev) = *kind->next(cur);
    } else {
        bsf_sess_t *next = *kind->next(cur);

        ogs_hash_set(kind->hash(), kind->key(cur), kind->key_len(cur), NULL);
        if (next)
            ogs_hash_set(kind->hash(), kind->key(next), kind->key_len(next), next);
    }
    *kind->next(cur) = NULL;
}

int __bsf_log_domain;

static OGS_POOL(bsf_sess_pool, bsf_sess_t);

static int context_initialized = 0;

void bsf_context_init(void)
{
    ogs_assert(context_initialized == 0);

    /* Initialize BSF context */
    memset(&self, 0, sizeof(bsf_context_t));

    ogs_log_install_domain(&__bsf_log_domain, "bsf", ogs_core()->log.level);

    ogs_pool_init(&bsf_sess_pool, ogs_app()->pool.sess);

    self.ipv4addr_hash = ogs_hash_make();
    ogs_assert(self.ipv4addr_hash);
    self.ipv6prefix_hash = ogs_hash_make();
    ogs_assert(self.ipv6prefix_hash);
    self.mac_hash = ogs_hash_make();
    ogs_assert(self.mac_hash);
    ogs_list_init(&self.secondary_list);

    context_initialized = 1;
}

void bsf_context_final(void)
{
    ogs_assert(context_initialized == 1);

    bsf_sess_remove_all();

    ogs_assert(self.ipv4addr_hash);
    ogs_hash_destroy(self.ipv4addr_hash);
    ogs_assert(self.ipv6prefix_hash);
    ogs_hash_destroy(self.ipv6prefix_hash);
    ogs_assert(self.mac_hash);
    ogs_hash_destroy(self.mac_hash);

    ogs_pool_final(&bsf_sess_pool);

    context_initialized = 0;
}

bsf_context_t *bsf_self(void)
{
    return &self;
}

static int bsf_context_prepare(void)
{
    return OGS_OK;
}

static int bsf_context_validation(void)
{
    return OGS_OK;
}

int bsf_context_parse_config(void)
{
    int rv;
    yaml_document_t *document = NULL;
    ogs_yaml_iter_t root_iter;
    int idx = 0;

    document = ogs_app()->document;
    ogs_assert(document);

    rv = bsf_context_prepare();
    if (rv != OGS_OK) return rv;

    ogs_yaml_iter_init(&root_iter, document);
    while (ogs_yaml_iter_next(&root_iter)) {
        const char *root_key = ogs_yaml_iter_key(&root_iter);
        ogs_assert(root_key);
        if ((!strcmp(root_key, "bsf")) &&
            (idx++ == ogs_app()->config_section_id)) {
            ogs_yaml_iter_t bsf_iter;
            ogs_yaml_iter_recurse(&root_iter, &bsf_iter);
            while (ogs_yaml_iter_next(&bsf_iter)) {
                const char *bsf_key = ogs_yaml_iter_key(&bsf_iter);
                ogs_assert(bsf_key);
                if (!strcmp(bsf_key, "default")) {
                    /* handle config in sbi library */
                } else if (!strcmp(bsf_key, "sbi")) {
                    /* handle config in sbi library */
                } else if (!strcmp(bsf_key, "nrf")) {
                    /* handle config in sbi library */
                } else if (!strcmp(bsf_key, "scp")) {
                    /* handle config in sbi library */
                } else if (!strcmp(bsf_key, "service_name")) {
                    /* handle config in sbi library */
                } else if (!strcmp(bsf_key, "discovery")) {
                    /* handle config in sbi library */
                } else
                    ogs_warn("unknown key `%s`", bsf_key);
            }
        }
    }

    rv = bsf_context_validation();
    if (rv != OGS_OK) return rv;

    return OGS_OK;
}

bsf_sess_t *bsf_sess_add_by_ip_address(
            char *ipv4addr_string, char *ipv6prefix_string)
{
    bsf_sess_t *sess = NULL;

    ogs_pool_alloc(&bsf_sess_pool, &sess);
    if (!sess) {
        ogs_error("Maximum number of session[%lld] reached",
            (long long)ogs_app()->pool.sess);
        return NULL;
    }
    memset(sess, 0, sizeof *sess);

    if (ipv4addr_string &&
        bsf_sess_set_ipv4addr(sess, ipv4addr_string) == false) {
        ogs_error("bsf_sess_set_ipv4addr[%s] failed", ipv4addr_string);
        ogs_pool_free(&bsf_sess_pool, sess);
        return NULL;
    }
    if (ipv6prefix_string &&
        bsf_sess_set_ipv6prefix(sess, ipv6prefix_string) == false) {
        ogs_error("bsf_sess_set_ipv6prefix[%s] failed", ipv6prefix_string);
        if (sess->ipv4addr_string) {
            chain_del(&chain_v4, sess);
            ogs_free(sess->ipv4addr_string);
        }
        ogs_pool_free(&bsf_sess_pool, sess);
        return NULL;
    }

    /* SBI Features */
    OGS_SBI_FEATURES_SET(sess->management_features,
            OGS_SBI_NBSF_MANAGEMENT_BINDING_UPDATE);

    sess->binding_id = ogs_msprintf("%d",
            (int)ogs_pool_index(&bsf_sess_pool, sess));
    ogs_assert(sess->binding_id);

    ogs_list_add(&self.sess_list, sess);

    return sess;
}

void bsf_sess_remove(bsf_sess_t *sess)
{
    int i;

    ogs_assert(sess);

    ogs_list_remove(&self.sess_list, sess);

    /* Free SBI object memory */
    ogs_sbi_object_free(&sess->sbi);

    ogs_assert(sess->binding_id);
    ogs_free(sess->binding_id);

    if (sess->supi)
        ogs_free(sess->supi);
    if (sess->gpsi)
        ogs_free(sess->gpsi);
    if (sess->ip_domain)
        ogs_free(sess->ip_domain);

    if (sess->ipv4addr_string) {
        chain_del(&chain_v4, sess);
        ogs_free(sess->ipv4addr_string);
    }
    if (sess->ipv6prefix_string) {
        chain_del(&chain_v6, sess);
        ogs_free(sess->ipv6prefix_string);
    }
    if (sess->mac_present)
        chain_del(&chain_mac, sess);
    if (sess->in_secondary_list)
        ogs_list_remove(&self.secondary_list, &sess->secondary_node);

    OpenAPI_clear_and_free_string_list(sess->ipv4_frame_route_list);
    OpenAPI_clear_and_free_string_list(sess->ipv6_frame_route_list);

    if (sess->registered)
        OpenAPI_pcf_binding_free(sess->registered);

    ogs_assert(sess->dnn);
    ogs_free(sess->dnn);

    if (sess->pcf_fqdn)
        ogs_free(sess->pcf_fqdn);

    for (i = 0; i < sess->num_of_pcf_ip; i++) {
        if (sess->pcf_ip[i].addr)
            ogs_free(sess->pcf_ip[i].addr);
        if (sess->pcf_ip[i].addr6)
            ogs_free(sess->pcf_ip[i].addr6);
    }
    sess->num_of_pcf_ip = 0;

    ogs_pool_free(&bsf_sess_pool, sess);
}

void bsf_sess_remove_all(void)
{
    bsf_sess_t *sess = NULL, *next_sess = NULL;

    ogs_list_for_each_safe(&self.sess_list, next_sess, sess)
        bsf_sess_remove(sess);
}

bool bsf_sess_set_ipv4addr(bsf_sess_t *sess, char *ipv4addr_string)
{
    int rv;

    ogs_assert(sess);
    ogs_assert(ipv4addr_string);

    if (sess->ipv4addr_string) {
        chain_del(&chain_v4, sess);
        ogs_free(sess->ipv4addr_string);
        sess->ipv4addr_string = NULL;
    }
    rv = ogs_ipv4_from_string(&sess->ipv4addr, ipv4addr_string);
    if (rv != OGS_OK) {
        ogs_error("ogs_ipv4_from_string() failed");
        return false;
    }

    sess->ipv4addr_string = ogs_strdup(ipv4addr_string);
    if (!sess->ipv4addr_string) {
        ogs_error("ogs_strdup() failed");
        return false;
    }

    chain_add(&chain_v4, sess);

    return true;
}

bool bsf_sess_set_ipv6prefix(bsf_sess_t *sess, char *ipv6prefix_string)
{
    int rv;

    ogs_assert(sess);
    ogs_assert(ipv6prefix_string);

    if (sess->ipv6prefix_string) {
        chain_del(&chain_v6, sess);
        ogs_free(sess->ipv6prefix_string);
        sess->ipv6prefix_string = NULL;
    }
    rv = ogs_ipv6prefix_from_string(
            sess->ipv6prefix.addr6, &sess->ipv6prefix.len, ipv6prefix_string);
    if (rv != OGS_OK) {
        ogs_error("ogs_ipv6prefix_from_string() failed");
        return false;
    }

    /* TS 29.521 V19.7.0 table 5.3.2.3.1-2: ipv6Prefix is the prefix of the UE, of any length */
    if (sess->ipv6prefix.len < 1 || sess->ipv6prefix.len > OGS_IPV6_128_PREFIX_LEN) {
        ogs_error("Unsupported IPv6 prefix length [%d]", sess->ipv6prefix.len);
        return false;
    }
    ipv6prefix_mask(sess->ipv6prefix.addr6, sess->ipv6prefix.len);

    sess->ipv6prefix_string = ogs_strdup(ipv6prefix_string);
    if (!sess->ipv6prefix_string) {
        ogs_error("ogs_strdup() failed");
        return false;
    }

    chain_add(&chain_v6, sess);

    return true;
}

bsf_sess_t *bsf_sess_find(uint32_t index)
{
    return ogs_pool_find(&bsf_sess_pool, index);
}

bsf_sess_t *bsf_sess_find_by_snssai_and_dnn(ogs_s_nssai_t *s_nssai, char *dnn)
{
    bsf_sess_t *sess = NULL;

    ogs_assert(s_nssai);
    ogs_assert(dnn);

    ogs_list_for_each(&self.sess_list, sess)
        if (sess->s_nssai.sst == s_nssai->sst &&
            sess->dnn && ogs_strcasecmp(sess->dnn, dnn) == 0)
            return sess;

    return NULL;
}

bsf_sess_t *bsf_sess_find_by_binding_id(char *binding_id)
{
    ogs_assert(binding_id);
    return bsf_sess_find(atoll(binding_id));
}

/* ---- the UE addresses of a query ---- */

static bool mac_from_string(uint8_t mac[6], const char *text)
{
    unsigned int v[6];
    char sep = 0, tail = 0;
    int n = 0;

    if (!text) return false;
    /* MacAddr48: six pairs of hexadecimal digits separated by hyphens (TS 29.571 clause 5.2.2), colons are accepted as well */
    n = sscanf(text, "%2x%c%2x%c%2x%c%2x%c%2x%c%2x%c", &v[0], &sep, &v[1], &sep, &v[2], &sep, &v[3], &sep, &v[4], &sep, &v[5], &tail);
    if (n != 11 || strlen(text) != 17) return false;
    for (n = 0; n < 6; n++) mac[n] = (uint8_t)v[n];
    return true;
}

/* whether the binding passes the narrowing keys of the query (dnn, snssai, supi, gpsi, ipDomain) */
static bool passes_keys(const bsf_query_t *q, const bsf_sess_t *sess)
{
    if (q->dnn && (!sess->dnn || ogs_strcasecmp(q->dnn, sess->dnn))) return false;
    if (q->supi && (!sess->supi || strcmp(q->supi, sess->supi))) return false;
    if (q->gpsi && (!sess->gpsi || strcmp(q->gpsi, sess->gpsi))) return false;
    if (q->ip_domain && (!sess->ip_domain || strcmp(q->ip_domain, sess->ip_domain))) return false;
    if (q->snssai_presence && (q->s_nssai.sst != sess->s_nssai.sst || q->s_nssai.sd.v != sess->s_nssai.sd.v)) return false;
    return true;
}

/* The chain of a head, counted with the keys of the query; *first is the first one that passes */
static int count_chain(const chain_kind_t *kind, bsf_sess_t *head, const bsf_query_t *q, bsf_sess_t **first)
{
    int count = 0;
    bsf_sess_t *cur;

    for (cur = head; cur; cur = *kind->next(cur)) {
        if (!passes_keys(q, cur)) continue;
        if (!count && first) *first = cur;
        count++;
    }
    return count;
}

/* whether address is inside the IPv4 network "a.b.c.d/len" (a framed route); *plen is its length */
static bool ipv4_in_route(uint32_t address, const char *route, int *plen)
{
    char *copy = ogs_strdup(route), *slash;
    uint32_t net;
    int len = 32;
    bool in = false;

    if (!copy) return false;
    slash = strchr(copy, '/');
    if (slash) { *slash = '\0'; len = atoi(slash + 1); }
    if (len >= 0 && len <= 32 && ogs_ipv4_from_string(&net, copy) == OGS_OK) {
        uint32_t mask = len == 0 ? 0 : htonl(0xffffffffu << (32 - len));

        in = (address & mask) == (net & mask);
        if (in && plen) *plen = len;
    }
    ogs_free(copy);
    return in;
}

/* whether the IPv6 prefix (addr6, len) lies inside the prefix "address/len" (a framed route or an additional prefix) */
static bool ipv6_in_route(const uint8_t *addr6, int len, const char *route, int *plen)
{
    uint8_t net[OGS_IPV6_LEN], masked[OGS_IPV6_LEN];
    uint8_t net_len = 0;

    if (ogs_ipv6prefix_from_string(net, &net_len, (char *)route) != OGS_OK || net_len < 1 || net_len > OGS_IPV6_128_PREFIX_LEN)
        return false;
    if (len < net_len) return false;
    memcpy(masked, addr6, OGS_IPV6_LEN);
    ipv6prefix_mask(masked, net_len);
    ipv6prefix_mask(net, net_len);
    if (memcmp(masked, net, OGS_IPV6_LEN)) return false;
    if (plen) *plen = net_len;
    return true;
}

/*
 * The bindings with more addresses than the first one are looked at when no binding is registered for the address of the query:
 * a framed route (TS 29.521 V19.7.0 table 5.3.2.3.2-1 note 4), an additional IPv6 prefix or an additional MAC address. The
 * longest route that contains the address wins; *best is the length it has.
 */
static int secondary_matches(const bsf_query_t *q, uint32_t ipv4, const uint8_t *addr6, int plen6, const uint8_t *mac, bsf_sess_t **found)
{
    bsf_sess_t *sess;
    int best = -1, count = 0;

    ogs_list_for_each_entry(&self.secondary_list, sess, secondary_node) {
        OpenAPI_lnode_t *node;
        int hit = -1, len;

        if (!passes_keys(q, sess)) continue;
        if (q->ipv4addr) {
            OpenAPI_list_for_each(sess->ipv4_frame_route_list, node)
                if (node->data && ipv4_in_route(ipv4, node->data, &len) && len > hit) hit = len;
        } else if (q->ipv6prefix) {
            OpenAPI_list_for_each(sess->ipv6_frame_route_list, node)
                if (node->data && ipv6_in_route(addr6, plen6, node->data, &len) && len > hit) hit = len;
            if (sess->registered)
                OpenAPI_list_for_each(sess->registered->add_ipv6_prefixes, node)
                    if (node->data && ipv6_in_route(addr6, plen6, node->data, &len) && len > hit) hit = len;
        } else if (q->mac_addr48 && sess->registered) {
            OpenAPI_list_for_each(sess->registered->add_mac_addrs, node) {
                uint8_t other[6];

                if (node->data && mac_from_string(other, node->data) && !memcmp(other, mac, 6)) hit = 48;
            }
        }
        if (hit < 0) continue;
        if (hit > best) {
            best = hit;
            count = 1;
            *found = sess;
        } else if (hit == best) {
            count++;
        }
    }
    return count;
}

int bsf_sess_query(const bsf_query_t *q, bsf_sess_t **found, bool *invalid)
{
    int count = 0;
    bsf_sess_t *first = NULL;

    ogs_assert(q);
    ogs_assert(found);
    *found = NULL;
    if (invalid) *invalid = false;

    if (q->ipv4addr) {
        uint32_t ipv4;

        if (ogs_ipv4_from_string(&ipv4, (char *)q->ipv4addr) != OGS_OK) {
            ogs_error("ogs_ipv4_from_string() failed");
            if (invalid) *invalid = true;
            return 0;
        }
        count = count_chain(&chain_v4, ogs_hash_get(self.ipv4addr_hash, &ipv4, sizeof(ipv4)), q, &first);
        if (!count)
            count = secondary_matches(q, ipv4, NULL, 0, NULL, &first);
    } else if (q->ipv6prefix) {
        struct { uint8_t len; uint8_t addr6[OGS_IPV6_LEN]; } want, key;
        int len;

        if (ogs_ipv6prefix_from_string(want.addr6, &want.len, (char *)q->ipv6prefix) != OGS_OK ||
                want.len < 1 || want.len > OGS_IPV6_128_PREFIX_LEN) {
            ogs_error("Invalid IPv6 prefix [%s]", q->ipv6prefix);
            if (invalid) *invalid = true;
            return 0;
        }
        /* TS 29.521 V19.7.0 clause 4.2.4.2: the longest prefix match; a length whose bindings fail the keys gives way to a shorter one */
        for (len = want.len; len >= 1 && !count; len--) {
            memset(&key, 0, sizeof(key));
            key.len = len;
            memcpy(key.addr6, want.addr6, OGS_IPV6_LEN);
            ipv6prefix_mask(key.addr6, len);
            count = count_chain(&chain_v6, ogs_hash_get(self.ipv6prefix_hash, &key, ipv6prefix_key_len(len)), q, &first);
        }
        if (!count)
            count = secondary_matches(q, 0, want.addr6, want.len, NULL, &first);
    } else if (q->mac_addr48) {
        uint8_t mac[6];

        if (!mac_from_string(mac, q->mac_addr48)) {
            ogs_error("Invalid MAC address [%s]", q->mac_addr48);
            if (invalid) *invalid = true;
            return 0;
        }
        count = count_chain(&chain_mac, ogs_hash_get(self.mac_hash, mac, sizeof(mac)), q, &first);
        if (!count)
            count = secondary_matches(q, 0, NULL, 0, mac, &first);
    }
    *found = first;
    return count;
}

bsf_sess_t *bsf_sess_find_for_registration(
        const char *ipv4addr, const char *ipv6prefix, const char *mac_addr48,
        const char *ip_domain, bool *invalid)
{
    bsf_sess_t *head = NULL, *cur;
    const chain_kind_t *kind = NULL;

    if (invalid) *invalid = false;

    if (ipv4addr) {
        uint32_t ipv4;

        if (ogs_ipv4_from_string(&ipv4, (char *)ipv4addr) != OGS_OK) {
            if (invalid) *invalid = true;
            return NULL;
        }
        head = ogs_hash_get(self.ipv4addr_hash, &ipv4, sizeof(ipv4));
        kind = &chain_v4;
    } else if (ipv6prefix) {
        struct { uint8_t len; uint8_t addr6[OGS_IPV6_LEN]; } key;

        memset(&key, 0, sizeof(key));
        if (ogs_ipv6prefix_from_string(key.addr6, &key.len, (char *)ipv6prefix) != OGS_OK ||
                key.len < 1 || key.len > OGS_IPV6_128_PREFIX_LEN) {
            ogs_error("Invalid IPv6 prefix [%s]", ipv6prefix);
            if (invalid) *invalid = true;
            return NULL;
        }
        ipv6prefix_mask(key.addr6, key.len);
        head = ogs_hash_get(self.ipv6prefix_hash, &key, ipv6prefix_key_len(key.len));
        kind = &chain_v6;
    } else if (mac_addr48) {
        uint8_t mac[6];

        if (!mac_from_string(mac, mac_addr48)) {
            if (invalid) *invalid = true;
            return NULL;
        }
        head = ogs_hash_get(self.mac_hash, mac, sizeof(mac));
        kind = &chain_mac;
    }
    for (cur = head; kind && cur; cur = *kind->next(cur)) {
        /* the same address in the same IP domain is the same binding */
        if ((!ip_domain && !cur->ip_domain) ||
                (ip_domain && cur->ip_domain && !strcmp(ip_domain, cur->ip_domain)))
            return cur;
    }
    return NULL;
}

bool bsf_sess_index_registered(bsf_sess_t *sess)
{
    const OpenAPI_pcf_binding_t *reg;
    bool secondary;

    ogs_assert(sess);
    reg = sess->registered;

    if (sess->mac_present) {
        chain_del(&chain_mac, sess);
        sess->mac_present = false;
    }
    if (reg && reg->mac_addr48) {
        if (!mac_from_string(sess->mac, reg->mac_addr48)) {
            ogs_error("Invalid macAddr48 [%s]", reg->mac_addr48);
            return false;
        }
        sess->mac_present = true;
        chain_add(&chain_mac, sess);
    }

    secondary = (reg && ((reg->add_ipv6_prefixes && reg->add_ipv6_prefixes->count) ||
                         (reg->add_mac_addrs && reg->add_mac_addrs->count))) ||
                (sess->ipv4_frame_route_list && sess->ipv4_frame_route_list->count) ||
                (sess->ipv6_frame_route_list && sess->ipv6_frame_route_list->count);
    if (secondary && !sess->in_secondary_list) {
        ogs_list_add(&self.secondary_list, &sess->secondary_node);
        sess->in_secondary_list = true;
    } else if (!secondary && sess->in_secondary_list) {
        ogs_list_remove(&self.secondary_list, &sess->secondary_node);
        sess->in_secondary_list = false;
    }
    return true;
}

int get_sess_load(void)
{
    return (((ogs_pool_size(&bsf_sess_pool) -
            ogs_pool_avail(&bsf_sess_pool)) * 100) /
            ogs_pool_size(&bsf_sess_pool));
}
