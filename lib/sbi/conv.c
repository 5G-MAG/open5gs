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

#include "ogs-sbi.h"
#include "yuarel.h"

static int parse_scheme_output(
        char *_protection_scheme_id, char *_scheme_output,
        ogs_datum_t *ecckey, ogs_datum_t *cipher_text, uint8_t *mactag)
{
    uint8_t protection_scheme_id;
    uint8_t scheme_output_size;
    uint8_t *scheme_output = NULL;
    uint8_t *p = NULL;

    ogs_assert(_protection_scheme_id);
    ogs_assert(_scheme_output);
    ogs_assert(ecckey);
    ogs_assert(mactag);
    ogs_assert(cipher_text);

    scheme_output_size = strlen(_scheme_output)/2;
    if (scheme_output_size <= ((OGS_ECCKEY_LEN+1) + OGS_MACTAG_LEN)) {
        ogs_error("Not enought length [%d]", (int)strlen(_scheme_output));
        return OGS_ERROR;
    }

    scheme_output = ogs_calloc(1, scheme_output_size);
    ogs_assert(scheme_output);

    ogs_ascii_to_hex(_scheme_output, strlen(_scheme_output),
            scheme_output, scheme_output_size);

    protection_scheme_id = atoi(_protection_scheme_id);
    if (protection_scheme_id == OGS_PROTECTION_SCHEME_PROFILE_A) {
        ecckey->size = OGS_ECCKEY_LEN;
    } else if (protection_scheme_id == OGS_PROTECTION_SCHEME_PROFILE_B) {
        ecckey->size = OGS_ECCKEY_LEN+1;
    } else {
        ogs_free(scheme_output);

        ogs_fatal("Invalid protection scheme id [%s]", _protection_scheme_id);
        ogs_assert_if_reached();

        return OGS_ERROR;
    }

    cipher_text->size = OGS_MSIN_LEN;

    p = scheme_output;
    ecckey->data = ogs_memdup(p, ecckey->size);
    ogs_assert(ecckey->data);

    p += ecckey->size;
    cipher_text->data = ogs_memdup(p, cipher_text->size);
    ogs_assert(cipher_text->data);

    p += cipher_text->size;
    memcpy(mactag, p, OGS_MACTAG_LEN);

    ogs_free(scheme_output);

    return OGS_OK;
}

char *ogs_supi_from_suci(char *suci)
{
#define MAX_SUCI_TOKEN 16
    char *array[MAX_SUCI_TOKEN];
    char *p, *tmp;
    int i;
    char *supi = NULL;

    ogs_assert(suci);
    tmp = ogs_strdup(suci);
    if (!tmp) {
        ogs_error("ogs_strdup() failed");
        return NULL;
    }

    /* Clang scan-build SA: Branch condition evaluates to a garbage value: If array "array" is not fully populated
     * in the while loop below then later access in the following switch-case may check uninitialized values.
     * Initialize "array" to NULL pointers to fix the issue. */
    for (i = 0; i < MAX_SUCI_TOKEN; i++) {
        array[i] = NULL;
    }

    p = tmp;
    i = 0;
    while((i < MAX_SUCI_TOKEN) && (array[i++] = strsep(&p, "-"))) {
        /* Empty Body */
    }

    SWITCH(array[0])
    CASE("suci")
        SWITCH(array[1])
        CASE("0")   /* SUPI format : IMSI */
            if (array[2] && array[3] && array[5] && array[6] && array[7]) {
                uint8_t protection_scheme_id = atoi(array[5]);
                uint8_t home_network_pki_value = atoi(array[6]);

                if (protection_scheme_id == OGS_PROTECTION_SCHEME_NULL) {
                    supi = ogs_msprintf("imsi-%s%s%s",
                            array[2], array[3], array[7]);
                } else if (protection_scheme_id ==
                            OGS_PROTECTION_SCHEME_PROFILE_A ||
                        protection_scheme_id ==
                            OGS_PROTECTION_SCHEME_PROFILE_B) {

                    ogs_datum_t pubkey;
                    ogs_datum_t cipher_text;
                    ogs_datum_t plain_text;
                    char *plain_bcd;
                    uint8_t mactag1[OGS_MACTAG_LEN], mactag2[OGS_MACTAG_LEN];

                    uint8_t z[OGS_ECCKEY_LEN];

                    uint8_t ek[OGS_KEY_LEN];
                    uint8_t icb[OGS_IVEC_LEN];
                    uint8_t mk[OGS_SHA256_DIGEST_SIZE];

                    if (home_network_pki_value <
                            OGS_HOME_NETWORK_PKI_VALUE_MIN ||
                        home_network_pki_value >
                            OGS_HOME_NETWORK_PKI_VALUE_MAX) {
                        ogs_error("Invalid HNET PKI Value [%s]", array[6]);
                        break;
                    }

                    if (!ogs_sbi_self()->hnet[home_network_pki_value].avail) {
                        ogs_error("HNET PKI Value Not Avaiable [%s]", array[6]);
                        break;
                    }

                    if (ogs_sbi_self()->hnet[home_network_pki_value].scheme
                            != protection_scheme_id) {
                        ogs_error("Scheme Not Matched [%d != %d]",
                            ogs_sbi_self()->hnet[home_network_pki_value].scheme,
                            protection_scheme_id);
                        break;
                    }

                    if (parse_scheme_output(
                            array[5], array[7],
                            &pubkey, &cipher_text, mactag1) != OGS_OK) {
                        ogs_error("parse_scheme_output[%s] failed", array[7]);
                        break;
                    }

                    if (protection_scheme_id ==
                            OGS_PROTECTION_SCHEME_PROFILE_A) {
                        curve25519_donna(z,
                            ogs_sbi_self()->hnet[home_network_pki_value].key,
                            pubkey.data);
                    } else if (protection_scheme_id ==
                            OGS_PROTECTION_SCHEME_PROFILE_B) {
                        if (ecdh_shared_secret(
                                pubkey.data,
                                ogs_sbi_self()->
                                    hnet[home_network_pki_value].key,
                                z) != 1) {
                            ogs_error("ecdh_shared_secret() failed");
                            ogs_log_hexdump(OGS_LOG_ERROR,
                                    pubkey.data, OGS_ECCKEY_LEN);
                            ogs_log_hexdump(OGS_LOG_ERROR,
                                ogs_sbi_self()->
                                    hnet[home_network_pki_value].key,
                                    OGS_ECCKEY_LEN);
                            goto cleanup;
                        }
                    } else
                        ogs_assert_if_reached();

                    ogs_kdf_ansi_x963(
                        z, OGS_ECCKEY_LEN, pubkey.data, pubkey.size,
                        ek, icb, mk);

                    ogs_hmac_sha256(
                            mk, OGS_SHA256_DIGEST_SIZE,
                            cipher_text.data, cipher_text.size,
                            mactag2, OGS_MACTAG_LEN);

                    if (memcmp(mactag1, mactag2, OGS_MACTAG_LEN) != 0) {
                        ogs_error("MAC-tag not matched");
                        ogs_log_hexdump(OGS_LOG_ERROR, mactag1, OGS_MACTAG_LEN);
                        ogs_log_hexdump(OGS_LOG_ERROR, mactag2, OGS_MACTAG_LEN);
                        goto cleanup;
                    }

                    plain_text.size = cipher_text.size;
                    plain_text.data = ogs_calloc(1, plain_text.size);
                    ogs_assert(plain_text.data);

                    ogs_aes_ctr128_encrypt(
                            ek, icb, cipher_text.data, cipher_text.size,
                            plain_text.data);

                    plain_bcd = ogs_calloc(1, plain_text.size*2+1);
                    ogs_assert(plain_bcd);

                    ogs_buffer_to_bcd(
                        plain_text.data, plain_text.size, plain_bcd);

                    supi = ogs_msprintf("imsi-%s%s%s",
                            array[2], array[3], plain_bcd);
                    ogs_assert(supi);

                    if (plain_text.data)
                        ogs_free(plain_text.data);
                    ogs_free(plain_bcd);
cleanup:
                    if (pubkey.data)
                        ogs_free(pubkey.data);
                    if (cipher_text.data)
                        ogs_free(cipher_text.data);
                } else {
                    ogs_error("Invalid Protection Scheme [%s]", array[5]);
                }
            }
            break;
        DEFAULT
            ogs_error("Not implemented [%s]", array[1]);
            break;
        END
        break;
    DEFAULT
        ogs_error("Not implemented [%s]", array[0]);
        break;
    END

    ogs_free(tmp);
    return supi;
}

char *ogs_supi_from_supi_or_suci(char *supi_or_suci)
{
    char *type = NULL;
    char *supi = NULL;

    ogs_assert(supi_or_suci);
    type = ogs_id_get_type(supi_or_suci);
    if (!type) {
        ogs_error("ogs_id_get_type[%s] failed", supi_or_suci);
        goto cleanup;
    }
    SWITCH(type)
    CASE("imsi")
        supi = ogs_strdup(supi_or_suci);
        ogs_expect(supi);
        break;
    CASE("suci")
        supi = ogs_supi_from_suci(supi_or_suci);
        ogs_expect(supi);
        break;
    DEFAULT
        ogs_error("Not implemented [%s]", type);
        break;
    END

cleanup:
    if (type)
        ogs_free(type);
    return supi;
}

char *ogs_uridup(
        OpenAPI_uri_scheme_e scheme,
        char *fqdn, ogs_sockaddr_t *addr, ogs_sockaddr_t *addr6, uint16_t port,
        ogs_sbi_header_t *h)
{
    char buf[OGS_ADDRSTRLEN];
    char uri[OGS_HUGE_LEN];
    char *p, *last;
    int i;

    ogs_assert(scheme);
    ogs_assert(fqdn || addr || addr6);

    p = uri;
    last = uri + OGS_HUGE_LEN;

    /* HTTP scheme is selected based on TLS information */
    if (scheme == OpenAPI_uri_scheme_https)
        p = ogs_slprintf(p, last, "https://");
    else if (scheme == OpenAPI_uri_scheme_http)
        p = ogs_slprintf(p, last, "http://");
    else {
        ogs_fatal("Invalid scheme [%d]", scheme);
        ogs_assert_if_reached();
    }

    /* Hostname/IP address */
    if (fqdn)
        p = ogs_slprintf(p, last, "%s", fqdn);
    else if (addr6) {
        p = ogs_slprintf(p, last, "[%s]", OGS_ADDR(addr6, buf));
    } else if (addr) {
        p = ogs_slprintf(p, last, "%s", OGS_ADDR(addr, buf));
    } else
        ogs_assert_if_reached();

    /* Port number */
    if (port)
        p = ogs_slprintf(p, last, ":%d", port);

    /* API */
    if (h) {
        ogs_assert(h->service.name);
        p = ogs_slprintf(p, last, "/%s", h->service.name);
        ogs_assert(h->api.version);
        p = ogs_slprintf(p, last, "/%s", h->api.version);

        /* Resource */
        ogs_assert(h->resource.component[0]);
        for (i = 0; i < OGS_SBI_MAX_NUM_OF_RESOURCE_COMPONENT &&
                            h->resource.component[i]; i++)
            p = ogs_slprintf(p, last, "/%s", h->resource.component[i]);
    }

    return ogs_strdup(uri);
}

char *ogs_sbi_server_uri(ogs_sbi_server_t *server, ogs_sbi_header_t *h)
{
    ogs_sockaddr_t *advertise = NULL;

    ogs_assert(server);

    advertise = server->advertise;
    if (!advertise)
        advertise = server->node.addr;
    ogs_assert(advertise);

    return ogs_sbi_sockaddr_uri(server->scheme, advertise, h);
}

uint16_t ogs_sbi_uri_port_from_scheme_and_addr(
        OpenAPI_uri_scheme_e scheme, ogs_sockaddr_t *addr)
{
    uint16_t port = 0;

    ogs_assert(scheme);
    ogs_assert(addr);

    if (scheme == OpenAPI_uri_scheme_https &&
        OGS_PORT(addr) == OGS_SBI_HTTPS_PORT) {
        /* No Port in URI */
    } else if (scheme == OpenAPI_uri_scheme_http &&
        OGS_PORT(addr) == OGS_SBI_HTTP_PORT) {
        /* No Port in URI */
    } else {
        port = OGS_PORT(addr);
    }

    return port;
}

char *ogs_sbi_sockaddr_uri(
        OpenAPI_uri_scheme_e scheme,
        ogs_sockaddr_t *sa_list, ogs_sbi_header_t *h)
{
    int rv;
    char *hostname = NULL;
    ogs_sockaddr_t *addr = NULL, *addr6 = NULL;
    uint16_t port = 0;
    char *uri = NULL;

    ogs_assert(scheme);
    ogs_assert(sa_list);

    hostname = ogs_gethostname(sa_list);

    rv = ogs_copyaddrinfo(&addr, sa_list);
    ogs_assert(rv == OGS_OK);
    rv = ogs_copyaddrinfo(&addr6, addr);
    ogs_assert(rv == OGS_OK);

    rv = ogs_filteraddrinfo(&addr, AF_INET);
    ogs_assert(rv == OGS_OK);
    rv = ogs_filteraddrinfo(&addr6, AF_INET6);
    ogs_assert(rv == OGS_OK);

    if (addr6)
        port = ogs_sbi_uri_port_from_scheme_and_addr(scheme, addr6);
    else if (addr)
        port = ogs_sbi_uri_port_from_scheme_and_addr(scheme, addr);

    uri = ogs_uridup(scheme, hostname, addr, addr6, port, h);

    ogs_freeaddrinfo(addr);
    ogs_freeaddrinfo(addr6);

    return uri;
}

char *ogs_sbi_client_uri(ogs_sbi_client_t *client, ogs_sbi_header_t *h)
{
    uint16_t port = 0;

    ogs_assert(client);

    if (client->fqdn) {
        port = client->fqdn_port;
    } else {
        if (client->addr6) {
            port = ogs_sbi_uri_port_from_scheme_and_addr(
                    client->scheme, client->addr6);
        } else if (client->addr) {
            port = ogs_sbi_uri_port_from_scheme_and_addr(
                    client->scheme, client->addr);
        }
    }

    return ogs_uridup(
            client->scheme, client->fqdn, client->addr, client->addr6, port, h);
}

char *ogs_sbi_client_apiroot(ogs_sbi_client_t *client)
{
    return ogs_sbi_client_uri(client, NULL);
}

/**
 * Returns a url-decoded version of str
 * IMPORTANT: be sure to free() the returned string after use
 * Thanks Geek Hideout!
 * http://www.geekhideout.com/urlcode.shtml
 */
char *ogs_sbi_url_encode(const char *str)
{
    if (str != NULL) {
        char *pstr = (char *)str;
        char *buf = ogs_malloc(strlen(str) * 3 + 1);
        char *pbuf = buf;
        ogs_assert(buf);
        while (*pstr) {

            if (*pstr == '"' ||
                *pstr == '(' ||
                *pstr == ')' ||
                *pstr == ',' ||
                *pstr == '/' ||
                *pstr == ':' ||
                *pstr == ';' ||
                *pstr == '<' ||
                *pstr == '=' ||
                *pstr == '>' ||
                *pstr == '?' ||
                *pstr == '@' ||
                *pstr == '[' ||
                *pstr == '\\' ||
                *pstr == ']' ||
                *pstr == '{' ||
                *pstr == '}') {
                *pbuf++ = '%';
                *pbuf++ = ogs_to_hex(*pstr >> 4);
                *pbuf++ = ogs_to_hex(*pstr & 15);
            } else
                *pbuf++ = *pstr;

            pstr++;
        }
        *pbuf = '\0';
        return buf;
    } else {
        return NULL;
    }
}

char *ogs_sbi_url_decode(const char *str)
{
    if (str != NULL) {
        char *pstr = (char *)str;
        char *buf = ogs_malloc(strlen(str) + 1);
        char *pbuf = buf;
        ogs_assert(buf);
        while (*pstr) {
            if (*pstr == '%') {
                if (pstr[1] && pstr[2]) {
                    *pbuf++ = ogs_from_hex(pstr[1]) << 4 |
                                ogs_from_hex(pstr[2]);
                    pstr += 2;
                }
            } else if (*pstr == '+') {
                *pbuf++ = ' ';
            } else {
                *pbuf++ = * pstr;
            }
            pstr++;
        }
        *pbuf = '\0';
        return buf;
    } else {
        return NULL;
    }
}

char *ogs_sbi_parse_uri(char *uri, const char *delim, char **saveptr)
{
    char *item = NULL;

    item = ogs_sbi_url_decode(ogs_strtok_r(uri, delim, saveptr));
    if (!item) {
        return NULL;
    }

    return item;
}

bool ogs_sbi_getaddr_from_uri(
        OpenAPI_uri_scheme_e *scheme,
        char **fqdn, uint16_t *fqdn_port,
        ogs_sockaddr_t **addr, ogs_sockaddr_t **addr6,
        char *uri)
{
    int rv;
    ogs_sockaddr_t tmp;
    struct yuarel yuarel;
    char *p = NULL;
    int port = 0;

    ogs_assert(fqdn);
    ogs_assert(fqdn_port);
    ogs_assert(addr);
    ogs_assert(addr6);
    ogs_assert(uri);

    p = ogs_strdup(uri);

    rv = yuarel_parse(&yuarel, p);
    if (rv != OGS_OK) {
        ogs_free(p);
        ogs_error("yuarel_parse() failed [%s]", uri);
        return false;
    }

    if (!yuarel.scheme) {
        ogs_error("No http.scheme found [%s]", uri);
        ogs_free(p);
        return false;
    }

    if (strcmp(yuarel.scheme, "https") == 0) {
        *scheme = OpenAPI_uri_scheme_https;
    } else if (strcmp(yuarel.scheme, "http") == 0) {
        *scheme = OpenAPI_uri_scheme_http;
    } else {
        ogs_error("Invalid http.scheme [%s:%s]", yuarel.scheme, uri);
        ogs_free(p);
        return false;
    }

    if (!yuarel.host) {
        ogs_error("No http.host found [%s]", uri);
        ogs_free(p);
        return false;
    }

    if (yuarel.port) port = yuarel.port;

    if (ogs_inet_pton(AF_INET, yuarel.host, &tmp) == OGS_OK ||
        ogs_inet_pton(AF_INET6, yuarel.host, &tmp) == OGS_OK) {

        *fqdn = NULL;
        *fqdn_port = 0;

        rv = ogs_getaddrinfo(addr, AF_UNSPEC, yuarel.host, port, 0);
        if (rv != OGS_OK) {
            ogs_error("ogs_getaddrinfo() failed [%s]", uri);
            ogs_free(p);
            return false;
        }

        rv = ogs_copyaddrinfo(addr6, *addr);
        ogs_assert(rv == OGS_OK);

        rv = ogs_filteraddrinfo(addr, AF_INET);
        ogs_assert(rv == OGS_OK);
        rv = ogs_filteraddrinfo(addr6, AF_INET6);
        ogs_assert(rv == OGS_OK);

    } else {

        *addr = NULL;
        *addr6 = NULL;

        *fqdn = ogs_strdup(yuarel.host);
        ogs_assert(*fqdn);
        *fqdn_port = port;

    }

    ogs_free(p);
    return true;
}

bool ogs_sbi_getpath_from_uri(char **path, char *uri)
{
    int rv;
    struct yuarel yuarel;
    char *p = NULL;

    ogs_assert(uri);

    p = ogs_strdup(uri);

    rv = yuarel_parse(&yuarel, p);
    if (rv != OGS_OK) {
        ogs_error("yuarel_parse() failed [%s]", uri);
        ogs_free(p);
        return false;
    }

    if (!yuarel.scheme) {
        ogs_error("No http.scheme found [%s]", uri);
        ogs_free(p);
        return false;
    }

    if (strcmp(yuarel.scheme, "https") == 0) {

    } else if (strcmp(yuarel.scheme, "http") == 0) {

    } else {
        ogs_error("Invalid http.scheme [%s:%s]", yuarel.scheme, uri);
        ogs_free(p);
        return false;
    }

    if (!yuarel.host) {
        ogs_error("No http.host found [%s]", uri);
        ogs_free(p);
        return false;
    }

    if (!yuarel.path) {
        ogs_error("No http.path found [%s]", uri);
        ogs_free(p);
        return false;
    }

    *path = ogs_strdup(yuarel.path);
    ogs_assert(*path);

    ogs_free(p);
    return true;
}

char *ogs_sbi_client_resolve(
        OpenAPI_uri_scheme_e scheme,
        char *fqdn, uint16_t fqdn_port,
        const char **resolve, int num_of_resolve)
{
    int i;
    uint16_t port;
    char *result = NULL;

    ogs_assert(scheme);
    ogs_assert(fqdn);
    ogs_assert(resolve);
    ogs_assert(resolve[0]);
    ogs_assert(num_of_resolve);

    port = fqdn_port;
    if (!port) {
        if (scheme == OpenAPI_uri_scheme_https)
            port = OGS_SBI_HTTPS_PORT;
        else if (scheme == OpenAPI_uri_scheme_http)
            port = OGS_SBI_HTTP_PORT;
        else
            ogs_assert_if_reached();
    }

    result = ogs_msprintf("%s:%d:%s", fqdn, port, resolve[0]);
    if (!result) {
        ogs_error("ogs_msprintf() failed");
        return NULL;
    }

    for (i = 1; i < num_of_resolve; i++) {
        ogs_assert(resolve[i]);
        result = ogs_mstrcatf(result, ",%s", resolve[i]);
        if (!result) {
            ogs_error("ogs_mstrcatf() failed");
            ogs_free(result);
            return NULL;
        }
    }

    return result;
}

char *ogs_sbi_bitrate_to_string(uint64_t bitrate, int unit)
{
    if (unit == OGS_SBI_BITRATE_KBPS) {
        return ogs_msprintf("%lld Kbps",
                (long long)bitrate / 1000);
    } else if (unit == OGS_SBI_BITRATE_MBPS) {
        return ogs_msprintf("%lld Mbps",
                (long long)bitrate / 1000 / 1000);
    } else if (unit == OGS_SBI_BITRATE_GBPS) {
        return ogs_msprintf("%lld Gbps",
                (long long)bitrate / 1000 / 1000 / 1000);
    } else if (unit == OGS_SBI_BITRATE_TBPS) {
        return ogs_msprintf("%lld Tbps",
                (long long)bitrate / 1000 / 1000 / 1000 / 1000);
    }

    return ogs_msprintf("%lld bps", (long long)bitrate);
}

uint64_t ogs_sbi_bitrate_from_string(char *str)
{
    char *unit = NULL;
    double bitrate = 0;
    ogs_assert(str);
    uint64_t mul = 1;

    unit = strrchr(str, ' ');
    bitrate = atof(str);

    if (!unit) {
        ogs_error("No Unit [%s]", str);
        return bitrate;
    }

    SWITCH(unit+1)
    CASE("Kbps")
        mul = 1000ul;
        break;
    CASE("Mbps")
        mul = 1000ul * 1000ul;
        break;
    CASE("Gbps")
        mul = 1000ul * 1000ul * 1000ul;
        break;
    CASE("Tbps")
        mul = 1000ul * 1000ul * 1000ul * 1000ul;
        break;
    DEFAULT
    END

    if (bitrate >= (INT64_MAX / mul))
        bitrate = INT64_MAX;
    else
        bitrate *= mul;

    return (uint64_t) bitrate;
}

#define MAX_TIMESTR_LEN 128

int ogs_strftimezone(char *str, size_t size, int tm_gmtoff)
{
    uint8_t off_sign;
    int off;
    int len;

    ogs_assert(str);
    ogs_assert(size);

    off_sign = '+';
    off = tm_gmtoff;
    if (tm_gmtoff < 0) {
        off_sign = '-';
        off = -off;
    }

    len = ogs_snprintf(str, size, "%c%02d:%02d",
            off_sign, off / 3600, (off % 3600) / 60);
    if (len != 6) {
        ogs_fatal("Unknown tm_gmtoff[%d:%d], len[%d], str[%s]",
                tm_gmtoff, off, len, str);
        ogs_assert_if_reached();
    }

    return len;
}

#define USE_MILLISECONDS_IN_RFC3339 0

char *ogs_sbi_localtime_string(ogs_time_t timestamp)
{
    struct tm tm;

    char datetime[MAX_TIMESTR_LEN];
    char timezone[MAX_TIMESTR_LEN];
    int len;

    ogs_localtime(ogs_time_sec(timestamp), &tm);
    ogs_strftime(datetime, sizeof datetime, "%Y-%m-%dT%H:%M:%S", &tm);

    len = ogs_strftimezone(timezone, MAX_TIMESTR_LEN, tm.tm_gmtoff);
    ogs_assert(len == 6);

#if USE_MILLISECONDS_IN_RFC3339
    return ogs_msprintf("%s.%03lld%s",
            datetime, (long long)ogs_time_msec(timestamp), timezone);
#else
    return ogs_msprintf("%s.%06lld%s",
            datetime, (long long)ogs_time_usec(timestamp), timezone);
#endif
}

char *ogs_sbi_gmtime_string(ogs_time_t timestamp)
{
    struct tm tm;

    char datetime[MAX_TIMESTR_LEN];

    ogs_gmtime(ogs_time_sec(timestamp), &tm);
    ogs_strftime(datetime, sizeof datetime, "%Y-%m-%dT%H:%M:%S", &tm);

#if USE_MILLISECONDS_IN_RFC3339
    return ogs_msprintf("%s.%03lldZ",
            datetime, (long long)ogs_time_msec(timestamp));
#else
    return ogs_msprintf("%s.%06lldZ",
            datetime, (long long)ogs_time_usec(timestamp));
#endif
}

char *ogs_sbi_timezone_string(int tm_gmtoff)
{
    char timezone[MAX_TIMESTR_LEN];
    int len;

    len = ogs_strftimezone(timezone, MAX_TIMESTR_LEN, tm_gmtoff);
    ogs_assert(len == 6);

    return ogs_msprintf("%s", timezone);
}

bool ogs_sbi_time_from_string(ogs_time_t *timestamp, char *str)
{
    int rv, i, j, k;
    struct tm tm;
    bool is_subsecs, is_time, timezone_found;
    char seconds[MAX_TIMESTR_LEN];
    char subsecs[MAX_TIMESTR_LEN];
    ogs_time_t usecs;

    ogs_assert(str);
    ogs_assert(timestamp);

    memset(seconds, 0, sizeof seconds);
    memset(subsecs, 0, sizeof subsecs);

    is_subsecs = false;
    is_time = false;
    timezone_found = false;
    i = 0; j = 0, k = 0;
    while(str[i]) {
        if (is_subsecs == false && str[i] == '.')
            is_subsecs = true;
        else if (is_subsecs == false && str[i] == 'T')
            is_time = true;
        else if (is_subsecs == true && (str[i] < '0' || str[i] > '9'))
            is_subsecs = false;

        if (is_time == true && (str[i] == '+' || str[i] == '-'))
            timezone_found = true;

        if (is_subsecs == false) {
            if (str[i] == ':' && i >= 3 &&
                    (str[i-3] == '+' || str[i-3] == '-')) {
                /* remove ':' character in timezone string range */
            } else {
                if (j >= MAX_TIMESTR_LEN - 1) {
                    ogs_error("Too long time string [%d]", (int)strlen(str));
                    return false;
                }
                seconds[j++] = str[i];
            }
        } else {
            if (k >= MAX_TIMESTR_LEN - 1) {
                ogs_error("Too long time string [%d]", (int)strlen(str));
                return false;
            }
            subsecs[k++] = str[i];
        }

        i++;
    }

    memset(&tm, 0, sizeof(tm));
    if (timezone_found == true)
        ogs_strptime(seconds, "%Y-%m-%dT%H:%M:%S%z", &tm);
    else
        ogs_strptime(seconds, "%Y-%m-%dT%H:%M:%S", &tm);
#if USE_MATH
    usecs = (ogs_time_t)floor(atof(subsecs) * 1000000.0 + 0.5);
#else
    usecs = (ogs_time_t)((atof(subsecs) * 10000000 + 5) / 10);
#endif

    rv = ogs_time_from_gmt(timestamp, &tm, usecs);
    if (rv != OGS_OK) {
        ogs_error("Cannot convert time [%s]", str);
        return false;
    }

    return true;
}

int ogs_sbi_rfc7231_string(char *date_str, ogs_time_t time)
{
    const char ogs_month_snames[12][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
        "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const char ogs_day_snames[7][4] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };

    struct tm gmt;
    const char *s;
    int real_year;

    ogs_time_t sec = ogs_time_sec(time);
    ogs_time_t msec = ogs_time_msec(time);

    ogs_assert(date_str);

    ogs_gmtime(sec, &gmt);

    /* example: "Sun, 04 Aug 2019 08:49:37.845 GMT" */
    /*           123456789012345678901234567890123  */

    s = &ogs_day_snames[gmt.tm_wday][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ',';
    *date_str++ = ' ';
    *date_str++ = gmt.tm_mday / 10 + '0';
    *date_str++ = gmt.tm_mday % 10 + '0';
    *date_str++ = ' ';
    s = &ogs_month_snames[gmt.tm_mon][0];
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = *s++;
    *date_str++ = ' ';
    real_year = 1900 + gmt.tm_year;
    /* This routine isn't y10k ready. */
    *date_str++ = real_year / 1000 + '0';
    *date_str++ = real_year % 1000 / 100 + '0';
    *date_str++ = real_year % 100 / 10 + '0';
    *date_str++ = real_year % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = gmt.tm_hour / 10 + '0';
    *date_str++ = gmt.tm_hour % 10 + '0';
    *date_str++ = ':';
    *date_str++ = gmt.tm_min / 10 + '0';
    *date_str++ = gmt.tm_min % 10 + '0';
    *date_str++ = ':';
    *date_str++ = gmt.tm_sec / 10 + '0';
    *date_str++ = gmt.tm_sec % 10 + '0';
    *date_str++ = '.';
    *date_str++ = msec / 100 + '0';
    *date_str++ = msec % 100 / 10 + '0';
    *date_str++ = msec % 10 + '0';
    *date_str++ = ' ';
    *date_str++ = 'G';
    *date_str++ = 'M';
    *date_str++ = 'T';
    *date_str++ = 0;

    return OGS_OK;
}

char *ogs_sbi_s_nssai_to_json(ogs_s_nssai_t *s_nssai)
{
    cJSON *item = NULL;
    OpenAPI_snssai_t sNSSAI;

    char *v = NULL;

    ogs_assert(s_nssai);

    sNSSAI.sst = s_nssai->sst;
    sNSSAI.sd = ogs_s_nssai_sd_to_string(s_nssai->sd);

    item = OpenAPI_snssai_convertToJSON(&sNSSAI);
    if (!item) {
        ogs_error("OpenAPI_snssai_convertToJSON() failed");
        return NULL;
    }
    if (sNSSAI.sd) ogs_free(sNSSAI.sd);

    v = cJSON_PrintUnformatted(item);
    ogs_expect(v);
    cJSON_Delete(item);

    return v;
}

bool ogs_sbi_s_nssai_from_json(ogs_s_nssai_t *s_nssai, char *str)
{
    bool rc = false;

    cJSON *item = NULL;
    OpenAPI_snssai_t *sNSSAI = NULL;

    ogs_assert(s_nssai);
    ogs_assert(str);

    item = cJSON_Parse(str);
    if (item) {
        sNSSAI = OpenAPI_snssai_parseFromJSON(item);
        if (sNSSAI) {
            s_nssai->sst = sNSSAI->sst;
            s_nssai->sd = ogs_s_nssai_sd_from_string(sNSSAI->sd);
            OpenAPI_snssai_free(sNSSAI);
            rc = true;
        }
        cJSON_Delete(item);
    }

    return rc;
}

char *ogs_sbi_s_nssai_to_string(ogs_s_nssai_t *s_nssai)
{
    ogs_assert(s_nssai);

    if (s_nssai->sd.v != OGS_S_NSSAI_NO_SD_VALUE) {
        return ogs_msprintf("%d-%06x", s_nssai->sst, s_nssai->sd.v);
    } else {
        return ogs_msprintf("%d", s_nssai->sst);
    }
}

bool ogs_sbi_s_nssai_from_string(ogs_s_nssai_t *s_nssai, char *str)
{
    bool rc = false;
    char *token, *p, *tofree;
    char *sst = NULL;
    char *sd = NULL;

    ogs_assert(s_nssai);
    ogs_assert(str);

    tofree = p = ogs_strdup(str);
    if (!p) {
        ogs_error("ogs_strdup[%s] failed", str);
        goto cleanup;
    }

    token = strsep(&p, "-");
    if (!token) {
        ogs_error("strsep[%s] failed", str);
        goto cleanup;
    }

    sst = ogs_strdup(token);
    if (!sst) {
        ogs_error("ogs_strdup[%s:%s] failed", str, token);
        goto cleanup;
    }

    s_nssai->sst = atoi(sst);
    s_nssai->sd.v = OGS_S_NSSAI_NO_SD_VALUE;

    if (p) {
        sd = ogs_strdup(p);
        if (!sd) {
            ogs_error("ogs_strdup[%s:%s] failed", str, token);
            goto cleanup;
        }
        s_nssai->sd = ogs_uint24_from_string_hexadecimal(sd);
    }

    rc = true;

cleanup:
    if (tofree)
        ogs_free(tofree);
    if (sst)
        ogs_free(sst);
    if (sd)
        ogs_free(sd);

    return rc;
}

OpenAPI_plmn_id_t *ogs_sbi_build_plmn_id(ogs_plmn_id_t *plmn_id)
{
    OpenAPI_plmn_id_t *PlmnId = NULL;

    ogs_assert(plmn_id);

    PlmnId = ogs_calloc(1, sizeof(*PlmnId));
    if (!PlmnId) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    PlmnId->mcc = ogs_plmn_id_mcc_string(plmn_id);
    if (!PlmnId->mcc) {
        ogs_error("ogs_plmn_id_mcc_string() failed");
        ogs_sbi_free_plmn_id(PlmnId);
        return NULL;
    }
    PlmnId->mnc = ogs_plmn_id_mnc_string(plmn_id);
    if (!PlmnId->mnc) {
        ogs_error("ogs_plmn_id_mnc_string() failed");
        ogs_sbi_free_plmn_id(PlmnId);
        return NULL;
    }

    return PlmnId;
}

bool ogs_sbi_parse_plmn_id(
        ogs_plmn_id_t *plmn_id, OpenAPI_plmn_id_t *PlmnId)
{
    ogs_assert(plmn_id);
    ogs_assert(PlmnId);
    ogs_assert(PlmnId->mcc);
    ogs_assert(PlmnId->mnc);

    ogs_plmn_id_build(plmn_id,
            atoi(PlmnId->mcc), atoi(PlmnId->mnc), strlen(PlmnId->mnc));

    return true;
}

void ogs_sbi_free_plmn_id(OpenAPI_plmn_id_t *PlmnId)
{
    ogs_assert(PlmnId);

    if (PlmnId->mcc)
        ogs_free(PlmnId->mcc);
    if (PlmnId->mnc)
        ogs_free(PlmnId->mnc);

    ogs_free(PlmnId);
}

OpenAPI_list_t *ogs_sbi_build_plmn_list(
        ogs_plmn_id_t *plmn_list, int num_of_plmn_list)
{
    OpenAPI_list_t *PlmnList = NULL;
    OpenAPI_plmn_id_t *PlmnId = NULL;
    int i;

    ogs_assert(plmn_list);
    ogs_assert(num_of_plmn_list);

    PlmnList = OpenAPI_list_create();
    ogs_assert(PlmnList);

    for (i = 0; i < num_of_plmn_list; i++) {
        PlmnId = ogs_sbi_build_plmn_id(plmn_list + i);
        ogs_assert(PlmnId);

        OpenAPI_list_add(PlmnList, PlmnId);
    }

    return PlmnList;
}

int ogs_sbi_parse_plmn_list(
        ogs_plmn_id_t *plmn_list, OpenAPI_list_t *PlmnList)
{
    OpenAPI_plmn_id_t *PlmnId = NULL;
    OpenAPI_lnode_t *node = NULL;
    int num_of_plmn_list = 0;

    ogs_assert(plmn_list);
    ogs_assert(PlmnList);

    num_of_plmn_list = 0;
    OpenAPI_list_for_each(PlmnList, node) {
        PlmnId = node->data;
        if (PlmnId) {
            ogs_assert(PlmnId->mcc);
            ogs_assert(PlmnId->mnc);

            if (num_of_plmn_list >= OGS_MAX_NUM_OF_PLMN) {
                ogs_warn("Exceeded maximum PLMN list size (%d)",
                        OGS_MAX_NUM_OF_PLMN);
                break;
            }

            ogs_plmn_id_build(plmn_list + num_of_plmn_list,
                    atoi(PlmnId->mcc), atoi(PlmnId->mnc), strlen(PlmnId->mnc));

            num_of_plmn_list++;
        }
    }

    return num_of_plmn_list;
}

void ogs_sbi_free_plmn_list(OpenAPI_list_t *PlmnList)
{
    OpenAPI_plmn_id_t *PlmnId = NULL;
    OpenAPI_lnode_t *node = NULL;

    ogs_assert(PlmnList);

    OpenAPI_list_for_each(PlmnList, node) {
        PlmnId = node->data;
        if (PlmnId)
            ogs_sbi_free_plmn_id(PlmnId);
    }
    OpenAPI_list_free(PlmnList);
}

/**
 * Compares an ogs_plmn_id_t structure with an OpenAPI_plmn_id_t structure.
 *
 * @param plmn_list The PLMN-ID in ogs_plmn_id_t format.
 * @param PlmnList The PLMN-ID in OpenAPI_plmn_id_t format.
 * @return true if the PLMN-IDs are equal; otherwise, false.
 */
bool ogs_sbi_compare_plmn_list(
        ogs_plmn_id_t *plmn_id, OpenAPI_plmn_id_t *PlmnId)
{
    ogs_plmn_id_t temp_plmn_id;

    ogs_assert(plmn_id);
    ogs_assert(PlmnId);
    ogs_assert(PlmnId->mcc);
    ogs_assert(PlmnId->mnc);

    /* Convert OpenAPI_plmn_id_t to ogs_plmn_id_t */
    ogs_sbi_parse_plmn_id(&temp_plmn_id, PlmnId);

    /* Compare MCC and MNC values */
    if (ogs_plmn_id_mcc(plmn_id) == ogs_plmn_id_mcc(&temp_plmn_id) &&
        ogs_plmn_id_mnc(plmn_id) == ogs_plmn_id_mnc(&temp_plmn_id)) {
        return true;
    }

    return false;
}

OpenAPI_plmn_id_nid_t *ogs_sbi_build_plmn_id_nid(ogs_plmn_id_t *plmn_id)
{
    OpenAPI_plmn_id_nid_t *PlmnIdNid = NULL;

    ogs_assert(plmn_id);

    PlmnIdNid = ogs_calloc(1, sizeof(*PlmnIdNid));
    if (!PlmnIdNid) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    PlmnIdNid->mcc = ogs_plmn_id_mcc_string(plmn_id);
    if (!PlmnIdNid->mcc) {
        ogs_error("ogs_plmn_id_mcc_string() failed");
        ogs_sbi_free_plmn_id_nid(PlmnIdNid);
        return NULL;
    }
    PlmnIdNid->mnc = ogs_plmn_id_mnc_string(plmn_id);
    if (!PlmnIdNid->mnc) {
        ogs_error("ogs_plmn_id_mnc_string() failed");
        ogs_sbi_free_plmn_id_nid(PlmnIdNid);
        return NULL;
    }

    return PlmnIdNid;
}

bool ogs_sbi_parse_plmn_id_nid(
        ogs_plmn_id_t *plmn_id, OpenAPI_plmn_id_nid_t *PlmnIdNid)
{
    ogs_assert(plmn_id);
    ogs_assert(PlmnIdNid);
    ogs_assert(PlmnIdNid->mcc);
    ogs_assert(PlmnIdNid->mnc);

    ogs_plmn_id_build(plmn_id,
            atoi(PlmnIdNid->mcc), atoi(PlmnIdNid->mnc), strlen(PlmnIdNid->mnc));

    return true;
}

void ogs_sbi_free_plmn_id_nid(OpenAPI_plmn_id_nid_t *PlmnIdNid)
{
    ogs_assert(PlmnIdNid);

    if (PlmnIdNid->mcc)
        ogs_free(PlmnIdNid->mcc);
    if (PlmnIdNid->mnc)
        ogs_free(PlmnIdNid->mnc);
    if (PlmnIdNid->nid)
        ogs_free(PlmnIdNid->nid);

    ogs_free(PlmnIdNid);
}

OpenAPI_guami_t *ogs_sbi_build_guami(ogs_guami_t *guami)
{
    OpenAPI_guami_t *Guami = NULL;

    ogs_assert(guami);

    Guami = ogs_calloc(1, sizeof(*Guami));
    if (!Guami) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    Guami->plmn_id = ogs_sbi_build_plmn_id_nid(&guami->plmn_id);
    if (!Guami->plmn_id) {
        ogs_error("ogs_sbi_build_plmn_id_nid() failed");
        ogs_sbi_free_guami(Guami);
        return NULL;
    }
    Guami->amf_id = ogs_amf_id_to_string(&guami->amf_id);
    if (!Guami->amf_id) {
        ogs_error("ogs_amf_id_to_string() failed");
        ogs_sbi_free_guami(Guami);
        return NULL;
    }

    return Guami;
}

bool ogs_sbi_parse_guami(ogs_guami_t *guami, OpenAPI_guami_t *Guami)
{
    ogs_assert(guami);
    ogs_assert(Guami);
    ogs_assert(Guami->amf_id);
    ogs_assert(Guami->plmn_id);

    ogs_amf_id_from_string(&guami->amf_id, Guami->amf_id);
    ogs_sbi_parse_plmn_id_nid(&guami->plmn_id, Guami->plmn_id);

    return true;
}

void ogs_sbi_free_guami(OpenAPI_guami_t *Guami)
{
    ogs_assert(Guami);

    if (Guami->plmn_id)
        ogs_sbi_free_plmn_id_nid(Guami->plmn_id);
    if (Guami->amf_id)
        ogs_free(Guami->amf_id);
    ogs_free(Guami);
}

OpenAPI_nr_location_t *ogs_sbi_build_nr_location(
    ogs_5gs_tai_t *tai, ogs_nr_cgi_t *nr_cgi)
{
    OpenAPI_nr_location_t *NrLocation = NULL;
    OpenAPI_tai_t *Tai = NULL;
    OpenAPI_ncgi_t *Ncgi = NULL;

    ogs_assert(tai);
    ogs_assert(nr_cgi);

    NrLocation = ogs_calloc(1, sizeof(*NrLocation));
    if (!NrLocation) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    NrLocation->tai = Tai = ogs_calloc(1, sizeof(*Tai));
    if (!Tai) {
        ogs_error("ogs_calloc() failed");
        ogs_sbi_free_nr_location(NrLocation);
        return NULL;
    }
    Tai->plmn_id = ogs_sbi_build_plmn_id(&tai->plmn_id);
    if (!Tai->plmn_id) {
        ogs_error("ogs_sbi_build_plmn_id() failed");
        ogs_sbi_free_nr_location(NrLocation);
        return NULL;
    }
    Tai->tac = ogs_uint24_to_0string(tai->tac);
    if (!Tai->tac) {
        ogs_error("ogs_uint24_to_0string() failed");
        ogs_sbi_free_nr_location(NrLocation);
        return NULL;
    }

    NrLocation->ncgi = Ncgi = ogs_calloc(1, sizeof(*Ncgi));
    if (!Ncgi) {
        ogs_error("ogs_calloc() failed");
        ogs_sbi_free_nr_location(NrLocation);
        return NULL;
    }
    Ncgi->plmn_id = ogs_sbi_build_plmn_id(&nr_cgi->plmn_id);
    if (!Ncgi->plmn_id) {
        ogs_error("ogs_sbi_build_plmn_id() failed");
        ogs_sbi_free_nr_location(NrLocation);
        return NULL;
    }
    Ncgi->nr_cell_id = ogs_uint36_to_0string(nr_cgi->cell_id);
    if (!Ncgi->nr_cell_id) {
        ogs_error("ogs_uint36_to_0string() failed");
        ogs_sbi_free_nr_location(NrLocation);
        return NULL;
    }

    return NrLocation;
}

bool ogs_sbi_parse_nr_location(ogs_5gs_tai_t *tai, ogs_nr_cgi_t *nr_cgi,
        OpenAPI_nr_location_t *NrLocation)
{
    OpenAPI_tai_t *Tai = NULL;
    OpenAPI_ncgi_t *Ncgi = NULL;

    ogs_assert(tai);
    ogs_assert(nr_cgi);
    ogs_assert(NrLocation);

    Tai = NrLocation->tai;
    if (Tai) {
        if (Tai->plmn_id)
            ogs_sbi_parse_plmn_id(&tai->plmn_id, Tai->plmn_id);
        if (Tai->tac)
            tai->tac = ogs_uint24_from_string_hexadecimal(Tai->tac);
    }

    Ncgi = NrLocation->ncgi;
    if (Ncgi) {
        if (Ncgi->plmn_id)
            ogs_sbi_parse_plmn_id(&nr_cgi->plmn_id, Ncgi->plmn_id);
        if (Ncgi->nr_cell_id)
            nr_cgi->cell_id = ogs_uint64_from_string_hexadecimal(
                    Ncgi->nr_cell_id);

    }

    return true;
}

void ogs_sbi_free_nr_location(OpenAPI_nr_location_t *NrLocation)
{
    OpenAPI_tai_t *Tai = NULL;
    OpenAPI_ncgi_t *Ncgi = NULL;

    ogs_assert(NrLocation);

    Tai = NrLocation->tai;
    if (Tai) {
        if (Tai->plmn_id)
            ogs_sbi_free_plmn_id(Tai->plmn_id);
        if (Tai->tac)
            ogs_free(Tai->tac);
        ogs_free(Tai);
    }

    Ncgi = NrLocation->ncgi;
    if (Ncgi) {
        if (Ncgi->plmn_id)
            ogs_sbi_free_plmn_id(Ncgi->plmn_id);
        if (Ncgi->nr_cell_id)
            ogs_free(Ncgi->nr_cell_id);
        ogs_free(Ncgi);
    }

    ogs_free(NrLocation);
}

/* The model of a ProtocolDescription or of an array of MpxMediaInfo from its JSON text (the text of a rule is built from a valid model); NULL if it does not parse */
static OpenAPI_protocol_description_t *parse_proto_desc(const char *json)
{
    OpenAPI_protocol_description_t *desc = NULL;
    cJSON *item = cJSON_Parse(json);

    if (!item)
        return NULL;
    desc = OpenAPI_protocol_description_parseFromJSON(item);
    cJSON_Delete(item);
    return desc;
}

static OpenAPI_list_t *parse_mpx_infos(const char *json)
{
    OpenAPI_list_t *list = NULL;
    cJSON *array = cJSON_Parse(json), *item = NULL;

    if (!array)
        return NULL;
    list = OpenAPI_list_create();
    ogs_assert(list);
    cJSON_ArrayForEach(item, array) {
        OpenAPI_mpx_media_info_t *info = OpenAPI_mpx_media_info_parseFromJSON(item);

        if (info)
            OpenAPI_list_add(list, info);
    }
    cJSON_Delete(array);
    if (list->count == 0) {
        OpenAPI_list_free(list);
        return NULL;
    }
    return list;
}

static void free_mpx_infos(OpenAPI_list_t *list)
{
    OpenAPI_lnode_t *node = NULL;

    if (!list)
        return;
    OpenAPI_list_for_each(list, node) {
        if (node->data)
            OpenAPI_mpx_media_info_free(node->data);
    }
    OpenAPI_list_free(list);
}

OpenAPI_pcc_rule_t *ogs_sbi_build_pcc_rule(
        ogs_pcc_rule_t *pcc_rule, int flow_presence)
{
    OpenAPI_pcc_rule_t *PccRule = NULL;
    OpenAPI_list_t *FlowInformationList = NULL;
    OpenAPI_flow_information_t *FlowInformation = NULL;

    int i;

    ogs_assert(pcc_rule);

    PccRule = ogs_calloc(1, sizeof(*PccRule));
    ogs_assert(PccRule);

    /*
     * At this point, only 1 QosData is used for PccRule.
     * Therefore, QoS ID uses the same value as PCC Rule ID.
     */
    PccRule->pcc_rule_id = pcc_rule->id;

    PccRule->ref_qos_data = OpenAPI_list_create();
    ogs_assert(PccRule->ref_qos_data);

    OpenAPI_list_add(PccRule->ref_qos_data, PccRule->pcc_rule_id);

    /* the Alternative QoS parameter sets, highest priority first (TS 29.512 V19.7.0 clause 4.2.6.2.1, "refAltQosParams") */
    if (pcc_rule->alt_qos.num_of_set) {
        PccRule->ref_alt_qos_params = OpenAPI_list_create();
        ogs_assert(PccRule->ref_alt_qos_params);
        for (i = 0; i < pcc_rule->alt_qos.num_of_set; i++)
            OpenAPI_list_add(PccRule->ref_alt_qos_params, pcc_rule->alt_qos.set[i].id);
    }

    PccRule->is_precedence = true;
    PccRule->precedence = pcc_rule->precedence;

    /* the Protocol Description for the PDU Set identification and the dynamically changing traffic characteristics (TS 29.512 V19.7.0 clause 4.2.6.10) */
    if (pcc_rule->proto_desc_dl[0])
        PccRule->proto_desc_dl = parse_proto_desc(pcc_rule->proto_desc_dl);
    if (pcc_rule->proto_desc_ul[0])
        PccRule->proto_desc_ul = parse_proto_desc(pcc_rule->proto_desc_ul);
    if (pcc_rule->exp_tran_ind) {
        PccRule->is_exp_tran_ind = true;
        PccRule->exp_tran_ind = true;
    }

    if (flow_presence == 1) {
        FlowInformationList = OpenAPI_list_create();
        ogs_assert(FlowInformationList);

        for (i = 0; i < pcc_rule->num_of_flow; i++) {
            ogs_flow_t *flow = &pcc_rule->flow[i];
            ogs_assert(flow);

            FlowInformation = ogs_calloc(1, sizeof(*FlowInformation));
            ogs_assert(FlowInformation);

            if (flow->direction == OGS_FLOW_UPLINK_ONLY)
                FlowInformation->flow_direction =
                    OpenAPI_flow_direction_UPLINK;
            else if (flow->direction == OGS_FLOW_DOWNLINK_ONLY)
                FlowInformation->flow_direction =
                    OpenAPI_flow_direction_DOWNLINK;
            else if (flow->direction == OGS_FLOW_BIDIRECTIONAL)
                FlowInformation->flow_direction =
                    OpenAPI_flow_direction_BIDIRECTIONAL;
            else {
                ogs_fatal("Unsupported direction [%d]", flow->direction);
                ogs_assert_if_reached();
            }

            ogs_assert(flow->description);
            FlowInformation->flow_description = flow->description;
            /* the (S)RTP Multiplexed Media Identification Information of the flow (clause 4.2.6.10.5) */
            if (pcc_rule->mpx[i].ul[0])
                FlowInformation->mpx_media_ul_infos = parse_mpx_infos(pcc_rule->mpx[i].ul);
            if (pcc_rule->mpx[i].dl[0])
                FlowInformation->mpx_media_dl_infos = parse_mpx_infos(pcc_rule->mpx[i].dl);

            OpenAPI_list_add(FlowInformationList, FlowInformation);
        }

        if (FlowInformationList->count)
            PccRule->flow_infos = FlowInformationList;
        else
            OpenAPI_list_free(FlowInformationList);
    }

    return PccRule;
}

void ogs_sbi_free_pcc_rule(OpenAPI_pcc_rule_t *PccRule)
{
    OpenAPI_flow_information_t *FlowInformation = NULL;
    OpenAPI_lnode_t *node = NULL;

    ogs_assert(PccRule);

    if (PccRule->ref_qos_data)
        OpenAPI_list_free(PccRule->ref_qos_data);
    if (PccRule->ref_alt_qos_params)
        OpenAPI_list_free(PccRule->ref_alt_qos_params);
    if (PccRule->ref_tc_data)
        OpenAPI_list_free(PccRule->ref_tc_data);
    if (PccRule->ref_um_data)
        OpenAPI_list_free(PccRule->ref_um_data);
    if (PccRule->ref_chg_data)
        OpenAPI_list_free(PccRule->ref_chg_data);
    if (PccRule->ref_qos_mon)
        OpenAPI_list_free(PccRule->ref_qos_mon);
    if (PccRule->flow_infos) {
        OpenAPI_list_for_each(PccRule->flow_infos, node) {
            FlowInformation = node->data;
            if (FlowInformation) {
                free_mpx_infos(FlowInformation->mpx_media_ul_infos);
                free_mpx_infos(FlowInformation->mpx_media_dl_infos);
                ogs_free(FlowInformation);
            }
        }
        OpenAPI_list_free(PccRule->flow_infos);
    }
    if (PccRule->proto_desc_dl)
        OpenAPI_protocol_description_free(PccRule->proto_desc_dl);
    if (PccRule->proto_desc_ul)
        OpenAPI_protocol_description_free(PccRule->proto_desc_ul);
    ogs_free(PccRule);
}

OpenAPI_flow_status_e ogs_sbi_flow_status_from_pcc_rule(int flow_status)
{
    switch (flow_status) {
    case 0: return OpenAPI_flow_status_ENABLED_UPLINK;
    case 1: return OpenAPI_flow_status_ENABLED_DOWNLINK;
    case 3: return OpenAPI_flow_status_DISABLED;
    case 4: return OpenAPI_flow_status_REMOVED;
    case 2:
    default: return OpenAPI_flow_status_ENABLED;
    }
}

int ogs_sbi_flow_status_to_pcc_rule(OpenAPI_flow_status_e flow_status)
{
    switch (flow_status) {
    case OpenAPI_flow_status_ENABLED_UPLINK: return 0;
    case OpenAPI_flow_status_ENABLED_DOWNLINK: return 1;
    case OpenAPI_flow_status_DISABLED: return 3;
    case OpenAPI_flow_status_REMOVED: return 4;
    case OpenAPI_flow_status_ENABLED:
    default: return 2;
    }
}

void ogs_sbi_decision_add_traffic_control(
        OpenAPI_sm_policy_decision_t *decision, OpenAPI_pcc_rule_t *pcc_rule_model, ogs_pcc_rule_t *pcc_rule)
{
    OpenAPI_traffic_control_data_t *TcData = NULL;
    OpenAPI_map_t *TcMap = NULL;

    ogs_assert(decision);
    ogs_assert(pcc_rule_model);
    ogs_assert(pcc_rule);

    TcData = ogs_calloc(1, sizeof(*TcData));
    ogs_assert(TcData);
    /* only 1 TrafficControlData is used for a PccRule, so its id is the id of the rule */
    TcData->tc_id = pcc_rule->id;
    TcData->flow_status = ogs_sbi_flow_status_from_pcc_rule(pcc_rule->flow_status);
    /* l4sInd: the explicit indication of ECN marking for L4S support (TS 29.512 V19.7.0 clause 4.2.6.2.21) */
    /* datBurstSizeInd and timetoNextBurstInd: the PSA UPF detects and marks them with the protoDescDl of the rule (clauses 4.2.6.10.6 and 4.2.6.10.9) */
    if (pcc_rule->dat_burst_size_ind) {
        TcData->is_dat_burst_size_ind = true;
        TcData->dat_burst_size_ind = true;
    }
    if (pcc_rule->time_to_next_burst_ind) {
        TcData->is_timeto_next_burst_ind = true;
        TcData->timeto_next_burst_ind = true;
    }
    switch (pcc_rule->l4s_ind) {
    case OGS_PCC_RULE_L4S_UL: TcData->l4s_ind = OpenAPI_uplink_downlink_support_UL; break;
    case OGS_PCC_RULE_L4S_DL: TcData->l4s_ind = OpenAPI_uplink_downlink_support_DL; break;
    case OGS_PCC_RULE_L4S_UL_DL: TcData->l4s_ind = OpenAPI_uplink_downlink_support_UL_DL; break;
    default: break;
    }

    if (!decision->traff_cont_decs) {
        decision->traff_cont_decs = OpenAPI_list_create();
        ogs_assert(decision->traff_cont_decs);
    }
    TcMap = OpenAPI_map_create(TcData->tc_id, TcData);
    ogs_assert(TcMap);
    OpenAPI_list_add(decision->traff_cont_decs, TcMap);

    if (!pcc_rule_model->ref_tc_data) {
        pcc_rule_model->ref_tc_data = OpenAPI_list_create();
        ogs_assert(pcc_rule_model->ref_tc_data);
    }
    OpenAPI_list_add(pcc_rule_model->ref_tc_data, pcc_rule->id);
}

static void free_map_list(OpenAPI_list_t *list, void (*free_value)(void *), bool free_key)
{
    OpenAPI_lnode_t *node = NULL;

    if (!list) return;
    OpenAPI_list_for_each(list, node) {
        OpenAPI_map_t *map = node->data;

        if (map) {
            if (map->value) free_value(map->value);
            if (free_key && map->key) ogs_free(map->key);
            ogs_free(map);
        }
    }
    OpenAPI_list_free(list);
}

static void free_plain(void *value) { ogs_free(value); }

static void free_charging_data(void *value)
{
    OpenAPI_charging_data_t *Chg = value;

    if (Chg->sponsor_id) ogs_free(Chg->sponsor_id);
    if (Chg->app_svc_prov_id) ogs_free(Chg->app_svc_prov_id);
    ogs_free(Chg);
}

static void free_usage_monitoring_data(void *value)
{
    OpenAPI_usage_monitoring_data_t *Um = value;

    if (Um->um_id) ogs_free(Um->um_id);
    ogs_free(Um);
}

static void free_qos_monitoring_data(void *value)
{
    OpenAPI_qos_monitoring_data_t *Qm = value;

    if (Qm->req_qos_mon_params) OpenAPI_list_free(Qm->req_qos_mon_params);
    if (Qm->rep_freqs) OpenAPI_list_free(Qm->rep_freqs);
    if (Qm->qm_id) ogs_free(Qm->qm_id);
    ogs_free(Qm);
}

void ogs_sbi_decision_free_traffic_control(OpenAPI_sm_policy_decision_t *decision)
{
    ogs_assert(decision);

    free_map_list(decision->traff_cont_decs, free_plain, false);
    decision->traff_cont_decs = NULL;
    /* the keys of the usage monitoring and charging entries are the ids of their data: owned by the data */
    free_map_list(decision->um_decs, free_usage_monitoring_data, false);
    decision->um_decs = NULL;
    free_map_list(decision->chg_decs, free_charging_data, false);
    decision->chg_decs = NULL;
    free_map_list(decision->qos_mon_decs, free_qos_monitoring_data, false);
    decision->qos_mon_decs = NULL;
}

void ogs_sbi_decision_add_usage_monitoring(
        OpenAPI_sm_policy_decision_t *decision, OpenAPI_pcc_rule_t *pcc_rule_model, const char *um_id,
        long volume, long volume_uplink, long volume_downlink, int time)
{
    OpenAPI_usage_monitoring_data_t *Um = NULL;
    OpenAPI_map_t *UmMap = NULL;
    OpenAPI_lnode_t *node = NULL;
    bool known = false;

    ogs_assert(decision);
    ogs_assert(pcc_rule_model);
    ogs_assert(um_id);

    if (!decision->um_decs) {
        decision->um_decs = OpenAPI_list_create();
        ogs_assert(decision->um_decs);
    }
    OpenAPI_list_for_each(decision->um_decs, node) {
        OpenAPI_map_t *Map = node->data;

        if (Map && Map->key && !strcmp(Map->key, um_id))
            known = true;
    }
    if (!known) {
        Um = ogs_calloc(1, sizeof(*Um));
        ogs_assert(Um);
        Um->um_id = ogs_strdup(um_id);
        ogs_assert(Um->um_id);
        if (volume) { Um->is_volume_threshold = true; Um->volume_threshold = volume; }
        if (volume_uplink) { Um->is_volume_threshold_uplink = true; Um->volume_threshold_uplink = volume_uplink; }
        if (volume_downlink) { Um->is_volume_threshold_downlink = true; Um->volume_threshold_downlink = volume_downlink; }
        if (time) { Um->is_time_threshold = true; Um->time_threshold = time; }
        UmMap = OpenAPI_map_create(Um->um_id, Um);
        ogs_assert(UmMap);
        OpenAPI_list_add(decision->um_decs, UmMap);
    }

    if (!pcc_rule_model->ref_um_data) {
        pcc_rule_model->ref_um_data = OpenAPI_list_create();
        ogs_assert(pcc_rule_model->ref_um_data);
    }
    OpenAPI_list_add(pcc_rule_model->ref_um_data, (void *)um_id);
}

void ogs_sbi_decision_end_usage_monitoring(
        OpenAPI_sm_policy_decision_t *decision, const char *um_id)
{
    OpenAPI_map_t *UmMap = NULL;

    ogs_assert(decision);
    ogs_assert(um_id);

    if (!decision->um_decs) {
        decision->um_decs = OpenAPI_list_create();
        ogs_assert(decision->um_decs);
    }
    UmMap = OpenAPI_map_create((char *)um_id, NULL);
    ogs_assert(UmMap);
    OpenAPI_list_add(decision->um_decs, UmMap);
}

void ogs_sbi_decision_add_sponsor(
        OpenAPI_sm_policy_decision_t *decision, OpenAPI_pcc_rule_t *pcc_rule_model, const char *chg_id,
        const char *sponsor_id, const char *asp_id)
{
    OpenAPI_charging_data_t *Chg = NULL;
    OpenAPI_map_t *ChgMap = NULL;
    OpenAPI_lnode_t *node = NULL;
    bool known = false;

    ogs_assert(decision);
    ogs_assert(pcc_rule_model);
    ogs_assert(chg_id);

    if (!decision->chg_decs) {
        decision->chg_decs = OpenAPI_list_create();
        ogs_assert(decision->chg_decs);
    }
    OpenAPI_list_for_each(decision->chg_decs, node) {
        OpenAPI_map_t *Map = node->data;

        if (Map && Map->key && !strcmp(Map->key, chg_id))
            known = true;
    }
    if (!known) {
        Chg = ogs_calloc(1, sizeof(*Chg));
        ogs_assert(Chg);
        Chg->chg_id = (char *)chg_id; /* the id is the one of the caller, which outlives the decision */
        Chg->metering_method = OpenAPI_metering_method_VOLUME;
        Chg->reporting_level = OpenAPI_reporting_level_SPON_CON_LEVEL;
        if (sponsor_id) Chg->sponsor_id = ogs_strdup(sponsor_id);
        if (asp_id) Chg->app_svc_prov_id = ogs_strdup(asp_id);
        ChgMap = OpenAPI_map_create((char *)chg_id, Chg);
        ogs_assert(ChgMap);
        OpenAPI_list_add(decision->chg_decs, ChgMap);
    }

    if (!pcc_rule_model->ref_chg_data) {
        pcc_rule_model->ref_chg_data = OpenAPI_list_create();
        ogs_assert(pcc_rule_model->ref_chg_data);
    }
    OpenAPI_list_add(pcc_rule_model->ref_chg_data, (void *)chg_id);
}

void ogs_sbi_decision_add_qos_monitoring(
        OpenAPI_sm_policy_decision_t *decision, OpenAPI_pcc_rule_t *pcc_rule_model, const char *qm_id,
        bool dl, bool ul, bool rt, bool event_triggered, bool periodic,
        int thr_dl, int thr_ul, int thr_rt, int wait_time, int rep_period)
{
    OpenAPI_qos_monitoring_data_t *Qm = NULL;
    OpenAPI_map_t *QmMap = NULL;
    OpenAPI_lnode_t *node = NULL;
    bool known = false;

    ogs_assert(decision);
    ogs_assert(pcc_rule_model);
    ogs_assert(qm_id);

    if (!decision->qos_mon_decs) {
        decision->qos_mon_decs = OpenAPI_list_create();
        ogs_assert(decision->qos_mon_decs);
    }
    OpenAPI_list_for_each(decision->qos_mon_decs, node) {
        OpenAPI_map_t *Map = node->data;

        if (Map && Map->key && !strcmp(Map->key, qm_id))
            known = true;
    }
    if (!known) {
        Qm = ogs_calloc(1, sizeof(*Qm));
        ogs_assert(Qm);
        Qm->qm_id = ogs_strdup(qm_id);
        ogs_assert(Qm->qm_id);
        Qm->req_qos_mon_params = OpenAPI_list_create();
        ogs_assert(Qm->req_qos_mon_params);
        if (dl) OpenAPI_list_add(Qm->req_qos_mon_params, (void *)OpenAPI_requested_qos_monitoring_parameter_DOWNLINK);
        if (ul) OpenAPI_list_add(Qm->req_qos_mon_params, (void *)OpenAPI_requested_qos_monitoring_parameter_UPLINK);
        if (rt) OpenAPI_list_add(Qm->req_qos_mon_params, (void *)OpenAPI_requested_qos_monitoring_parameter_ROUND_TRIP);
        Qm->rep_freqs = OpenAPI_list_create();
        ogs_assert(Qm->rep_freqs);
        if (event_triggered) OpenAPI_list_add(Qm->rep_freqs, (void *)OpenAPI_reporting_frequency_EVENT_TRIGGERED);
        if (periodic) OpenAPI_list_add(Qm->rep_freqs, (void *)OpenAPI_reporting_frequency_PERIODIC);
        if (thr_dl >= 0) { Qm->is_rep_thresh_dl = true; Qm->rep_thresh_dl = thr_dl; }
        if (thr_ul >= 0) { Qm->is_rep_thresh_ul = true; Qm->rep_thresh_ul = thr_ul; }
        if (thr_rt >= 0) { Qm->is_rep_thresh_rp = true; Qm->rep_thresh_rp = thr_rt; }
        if (wait_time >= 0) { Qm->is_wait_time = true; Qm->wait_time = wait_time; }
        if (rep_period >= 0) { Qm->is_rep_period = true; Qm->rep_period = rep_period; }
        QmMap = OpenAPI_map_create(Qm->qm_id, Qm);
        ogs_assert(QmMap);
        OpenAPI_list_add(decision->qos_mon_decs, QmMap);
    }

    if (!pcc_rule_model->ref_qos_mon) {
        pcc_rule_model->ref_qos_mon = OpenAPI_list_create();
        ogs_assert(pcc_rule_model->ref_qos_mon);
    }
    OpenAPI_list_add(pcc_rule_model->ref_qos_mon, (void *)qm_id);
}

void ogs_sbi_decision_add_qos_monitoring_trigger(OpenAPI_sm_policy_decision_t *decision)
{
    OpenAPI_lnode_t *node = NULL;

    ogs_assert(decision);
    if (!decision->policy_ctrl_req_triggers) {
        decision->policy_ctrl_req_triggers = OpenAPI_list_create();
        ogs_assert(decision->policy_ctrl_req_triggers);
    }
    OpenAPI_list_for_each(decision->policy_ctrl_req_triggers, node)
        if ((OpenAPI_policy_control_request_trigger_e)(intptr_t)node->data ==
                OpenAPI_policy_control_request_trigger_QOS_MONITORING)
            return;
    OpenAPI_list_add(decision->policy_ctrl_req_triggers,
            (void *)OpenAPI_policy_control_request_trigger_QOS_MONITORING);
}

void ogs_sbi_decision_add_l4s_trigger(OpenAPI_sm_policy_decision_t *decision)
{
    OpenAPI_lnode_t *node = NULL;

    ogs_assert(decision);
    if (!decision->policy_ctrl_req_triggers) {
        decision->policy_ctrl_req_triggers = OpenAPI_list_create();
        ogs_assert(decision->policy_ctrl_req_triggers);
    }
    OpenAPI_list_for_each(decision->policy_ctrl_req_triggers, node)
        if ((OpenAPI_policy_control_request_trigger_e)(intptr_t)node->data ==
                OpenAPI_policy_control_request_trigger_L4S_SUPP)
            return;
    OpenAPI_list_add(decision->policy_ctrl_req_triggers,
            (void *)OpenAPI_policy_control_request_trigger_L4S_SUPP);
}

void ogs_sbi_decision_add_us_re_trigger(OpenAPI_sm_policy_decision_t *decision)
{
    OpenAPI_lnode_t *node = NULL;

    ogs_assert(decision);
    if (!decision->policy_ctrl_req_triggers) {
        decision->policy_ctrl_req_triggers = OpenAPI_list_create();
        ogs_assert(decision->policy_ctrl_req_triggers);
    }
    OpenAPI_list_for_each(decision->policy_ctrl_req_triggers, node)
        if ((OpenAPI_policy_control_request_trigger_e)(intptr_t)node->data ==
                OpenAPI_policy_control_request_trigger_US_RE)
            return;
    OpenAPI_list_add(decision->policy_ctrl_req_triggers,
            (void *)OpenAPI_policy_control_request_trigger_US_RE);
}

static OpenAPI_pdu_set_qos_para_rm_t *build_pdu_set_qos(const ogs_pdu_set_qos_t *pdu_set)
{
    OpenAPI_pdu_set_qos_para_rm_t *Para = NULL;

    if (!pdu_set->presence)
        return NULL;

    Para = ogs_calloc(1, sizeof(*Para));
    ogs_assert(Para);
    if (pdu_set->delay_budget_presence) {
        Para->is_pdu_set_delay_budget = true;
        Para->pdu_set_delay_budget = pdu_set->delay_budget;
    }
    if (pdu_set->error_rate[0]) {
        Para->pdu_set_err_rate = ogs_strdup(pdu_set->error_rate);
        ogs_assert(Para->pdu_set_err_rate);
    }
    if (pdu_set->handling_info == OGS_PDU_SET_ALL_PDUS_NEEDED)
        Para->pdu_set_handling_info = OpenAPI_pdu_set_handling_info_ALL_PDUS_NEEDED;
    else if (pdu_set->handling_info == OGS_PDU_SET_ALL_PDUS_NOT_NEEDED)
        Para->pdu_set_handling_info = OpenAPI_pdu_set_handling_info_ALL_PDUS_NOT_NEEDED;
    return Para;
}

static void free_pdu_set_qos(OpenAPI_pdu_set_qos_para_rm_t *Para)
{
    if (!Para)
        return;
    if (Para->pdu_set_err_rate)
        ogs_free(Para->pdu_set_err_rate);
    ogs_free(Para);
}

OpenAPI_qos_data_t *ogs_sbi_build_alt_qos_data(ogs_pcc_rule_t *pcc_rule, int index)
{
    OpenAPI_qos_data_t *QosData = NULL;

    ogs_assert(pcc_rule);
    ogs_assert(index >= 0 && index < pcc_rule->alt_qos.num_of_set);

    QosData = ogs_calloc(1, sizeof(*QosData));
    ogs_assert(QosData);

    /* TS 29.512 V19.7.0 clause 4.2.3.30 of TS 29.514 V19.8.0 and clause 4.2.6.2.1: qosId, packetDelayBudget, packetErrorRate, gbrUl and gbrDl of the set */
    QosData->qos_id = pcc_rule->alt_qos.set[index].id;
    if (pcc_rule->alt_qos.set[index].pdb_presence) {
        QosData->is_packet_delay_budget = true;
        QosData->packet_delay_budget = pcc_rule->alt_qos.set[index].pdb;
    }
    if (pcc_rule->alt_qos.set[index].per[0]) {
        QosData->packet_error_rate = ogs_strdup(pcc_rule->alt_qos.set[index].per);
        ogs_assert(QosData->packet_error_rate);
    }
    if (pcc_rule->alt_qos.set[index].gbr.uplink)
        QosData->gbr_ul = ogs_sbi_bitrate_to_string(pcc_rule->alt_qos.set[index].gbr.uplink, OGS_SBI_BITRATE_BPS);
    if (pcc_rule->alt_qos.set[index].gbr.downlink)
        QosData->gbr_dl = ogs_sbi_bitrate_to_string(pcc_rule->alt_qos.set[index].gbr.downlink, OGS_SBI_BITRATE_BPS);
    QosData->pdu_set_qos_dl = build_pdu_set_qos(&pcc_rule->alt_qos.set[index].pdu_set_dl);
    QosData->pdu_set_qos_ul = build_pdu_set_qos(&pcc_rule->alt_qos.set[index].pdu_set_ul);

    return QosData;
}

OpenAPI_qos_data_t *ogs_sbi_build_qos_data(ogs_pcc_rule_t *pcc_rule)
{
    OpenAPI_qos_data_t *QosData = NULL;

    ogs_assert(pcc_rule);

    QosData = ogs_calloc(1, sizeof(*QosData));
    ogs_assert(QosData);

    /*
     * At this point, only 1 QosData is used for PccRule.
     * Therefore, QoS ID uses the same value as PCC Rule ID.
     */
    QosData->qos_id = pcc_rule->id;

    QosData->is__5qi = true;
    QosData->_5qi = pcc_rule->qos.index;
    QosData->is_priority_level = true;
    QosData->priority_level = pcc_rule->qos.arp.priority_level;

    QosData->arp = ogs_calloc(1, sizeof(OpenAPI_arp_t));
    ogs_assert(QosData->arp);

    if (pcc_rule->qos.arp.pre_emption_capability ==
            OGS_5GC_PRE_EMPTION_ENABLED)
        QosData->arp->preempt_cap =
            OpenAPI_preemption_capability_MAY_PREEMPT;
    else if (pcc_rule->qos.arp.pre_emption_capability ==
            OGS_5GC_PRE_EMPTION_DISABLED)
        QosData->arp->preempt_cap =
            OpenAPI_preemption_capability_NOT_PREEMPT;
    ogs_assert(pcc_rule->qos.arp.pre_emption_capability);

    if (pcc_rule->qos.arp.pre_emption_vulnerability ==
            OGS_5GC_PRE_EMPTION_ENABLED)
        QosData->arp->preempt_vuln =
            OpenAPI_preemption_vulnerability_PREEMPTABLE;
    else if (pcc_rule->qos.arp.pre_emption_vulnerability ==
            OGS_5GC_PRE_EMPTION_DISABLED)
        QosData->arp->preempt_vuln =
            OpenAPI_preemption_vulnerability_NOT_PREEMPTABLE;
    ogs_assert(pcc_rule->qos.arp.pre_emption_vulnerability);
    QosData->arp->priority_level = pcc_rule->qos.arp.priority_level;

    if (pcc_rule->qos.mbr.uplink)
        QosData->maxbr_ul = ogs_sbi_bitrate_to_string(
                pcc_rule->qos.mbr.uplink, OGS_SBI_BITRATE_BPS);
    if (pcc_rule->qos.mbr.downlink)
        QosData->maxbr_dl = ogs_sbi_bitrate_to_string(
                pcc_rule->qos.mbr.downlink, OGS_SBI_BITRATE_BPS);

    if (pcc_rule->qos.gbr.uplink)
        QosData->gbr_ul = ogs_sbi_bitrate_to_string(
                pcc_rule->qos.gbr.uplink, OGS_SBI_BITRATE_BPS);
    if (pcc_rule->qos.gbr.downlink)
        QosData->gbr_dl = ogs_sbi_bitrate_to_string(
                pcc_rule->qos.gbr.downlink, OGS_SBI_BITRATE_BPS);

    /* TS 29.512 V19.7.0 clause 4.2.6.6.2: the authorized PDU Set QoS parameters of the rule */
    QosData->pdu_set_qos_dl = build_pdu_set_qos(&pcc_rule->pdu_set_dl);
    QosData->pdu_set_qos_ul = build_pdu_set_qos(&pcc_rule->pdu_set_ul);

    return QosData;
}

void ogs_sbi_decision_add_qos_data(OpenAPI_list_t *QosDecisionList, ogs_pcc_rule_t *pcc_rule)
{
    OpenAPI_qos_data_t *QosData = NULL;
    int i;

    ogs_assert(QosDecisionList);
    ogs_assert(pcc_rule);

    QosData = ogs_sbi_build_qos_data(pcc_rule);
    ogs_assert(QosData);
    ogs_assert(QosData->qos_id);
    OpenAPI_list_add(QosDecisionList, OpenAPI_map_create(QosData->qos_id, QosData));

    /* the QosData that refAltQosParams of the rule refer to (TS 29.512 V19.7.0 clause 4.2.6.2.1) */
    for (i = 0; i < pcc_rule->alt_qos.num_of_set; i++) {
        QosData = ogs_sbi_build_alt_qos_data(pcc_rule, i);
        OpenAPI_list_add(QosDecisionList, OpenAPI_map_create(QosData->qos_id, QosData));
    }
}

void ogs_sbi_free_qos_data(OpenAPI_qos_data_t *QosData)
{
    ogs_assert(QosData);

    if (QosData->arp) ogs_free(QosData->arp);
    if (QosData->maxbr_ul) ogs_free(QosData->maxbr_ul);
    if (QosData->maxbr_dl) ogs_free(QosData->maxbr_dl);
    if (QosData->gbr_ul) ogs_free(QosData->gbr_ul);
    if (QosData->gbr_dl) ogs_free(QosData->gbr_dl);
    if (QosData->packet_error_rate) ogs_free(QosData->packet_error_rate);
    free_pdu_set_qos(QosData->pdu_set_qos_dl);
    free_pdu_set_qos(QosData->pdu_set_qos_ul);

    ogs_free(QosData);
}
