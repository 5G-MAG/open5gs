/*
 * Copyright (C) 2019,2024 by Sukchan Lee <acetcom@gmail.com>
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

#if !defined(OGS_SBI_INSIDE) && !defined(OGS_SBI_COMPILATION)
#error "This header cannot be included directly."
#endif

#ifndef OGS_SBI_SERVER_H
#define OGS_SBI_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct ogs_sbi_stream_s ogs_sbi_stream_t;
typedef struct ogs_sbi_xact_s ogs_sbi_xact_t;
typedef struct ogs_sbi_server_actions_s ogs_sbi_server_actions_t;

typedef struct ogs_sbi_server_s {
    ogs_socknode_t  node;
    ogs_sockaddr_t  *advertise;

    ogs_pool_id_t   id;

    char *interface;
    OpenAPI_uri_scheme_e scheme;
    char *private_key, *cert, *sslkeylog;
    bool verify_client;
    char *verify_client_cacert;

    SSL_CTX *ssl_ctx;

    int (*cb)(ogs_sbi_request_t *request, void *data);
    ogs_list_t      session_list;

    void            *mhd; /* Used by MHD */

    /* HTTP backend (HTTP/2 nghttp2 or HTTP/1.1 MHD) serving this server */
    const ogs_sbi_server_actions_t *actions;

    /*
     * Both protocols on one address (ogs_sbi_server_add_dual()): the HTTP/2
     * server listens, and hands the connections that do not start with the
     * HTTP/2 connection preface (RFC 9113 section 3.4) to its HTTP/1.1 peer,
     * which has no listening socket of its own (no_listen).
     */
    struct ogs_sbi_server_s *h1_peer;
    bool no_listen;
} ogs_sbi_server_t;

typedef struct ogs_sbi_server_actions_s {
    void (*init)(int num_of_session_pool, int num_of_stream_pool);
    void (*cleanup)(void);

    int (*start)(ogs_sbi_server_t *server,
            int (*cb)(ogs_sbi_request_t *request, void *data));
    void (*graceful_shutdown)(ogs_sbi_server_t *server);
    void (*stop)(ogs_sbi_server_t *server);

    bool (*send_rspmem_persistent)(
            ogs_sbi_stream_t *stream, ogs_sbi_response_t *response);
    bool (*send_response)(
            ogs_sbi_stream_t *stream, ogs_sbi_response_t *response);

    void *(*stream_find_by_id)(ogs_pool_id_t id);

    /*
     * Per-backend hooks linking an outbound SBI transaction into the
     * inbound server stream that triggered it. The list head lives
     * inside the backend-private stream/session struct, so each
     * backend (HTTP/2 vs MHD) provides its own attach/detach.
     */
    void (*xact_attach)(ogs_sbi_stream_t *stream, ogs_sbi_xact_t *xact);
    void (*xact_detach)(ogs_sbi_xact_t *xact);
} ogs_sbi_server_actions_t;

/*
 * Header shared by the stream types of all HTTP backends. It must be the
 * first member of the backend's stream (HTTP/2) or session (HTTP/1.1) struct,
 * so that code outside the backend can recover the id and the server from an
 * opaque ogs_sbi_stream_t pointer whichever backend created it.
 *
 * The id is allocated with ogs_sbi_stream_id_calloc() so that it is unique
 * across all backends: the id is what the NFs carry in events and in
 * xact->assoc_stream_id, and ogs_sbi_stream_find_by_id() has to resolve it
 * without knowing which backend owns the stream.
 */
typedef struct ogs_sbi_stream_common_s {
    ogs_lnode_t             lnode;
    ogs_pool_id_t           id;
    ogs_sbi_server_t       *server;
} ogs_sbi_stream_common_t;

ogs_pool_id_t ogs_sbi_server_next_stream_id(void);

/*
 * Same as ogs_pool_id_calloc() for a pool whose nodes start with an
 * ogs_sbi_stream_common_t named "common", but takes the id from the counter
 * shared by all HTTP backends.
 */
#define ogs_sbi_stream_id_calloc(pool, node) do { \
    ogs_pool_alloc(pool, node); \
    if (*(node)) { \
        memset(*(node), 0, sizeof(**(node))); \
        (*(node))->common.id = ogs_sbi_server_next_stream_id(); \
        ogs_hash_set((pool)->id_hash, \
                &((*(node))->common.id), sizeof(ogs_pool_id_t), *(node)); \
    } \
} while (0)

#define ogs_sbi_stream_id_free(pool, node) do { \
    ogs_assert(((node)->common.id) >= OGS_MIN_POOL_ID && \
            ((node)->common.id) <= OGS_MAX_POOL_ID); \
    ogs_hash_set((pool)->id_hash, \
            &((node)->common.id), sizeof(ogs_pool_id_t), NULL); \
    ogs_pool_free(pool, node); \
} while (0)

void ogs_sbi_server_init(int num_of_session_pool, int num_of_stream_pool);
void ogs_sbi_server_final(void);

ogs_sbi_server_t *ogs_sbi_server_add(
        const char *interface,
        OpenAPI_uri_scheme_e scheme, ogs_sockaddr_t *addr,
        ogs_sockopt_t *option);
/*
 * Add a server on the HTTP/1.1 (MHD) backend, alongside servers added with
 * ogs_sbi_server_add() on the HTTP/2 (nghttp2) backend. The MHD backend does
 * not implement TLS: scheme must be OpenAPI_uri_scheme_http.
 */
ogs_sbi_server_t *ogs_sbih1_server_add(
        const char *interface,
        OpenAPI_uri_scheme_e scheme, ogs_sockaddr_t *addr,
        ogs_sockopt_t *option);
/*
 * Add the HTTP/2 (nghttp2) and HTTP/1.1 (MHD) backends on the same address
 * and port: the connection is served by the HTTP/2 backend if the client
 * starts with the HTTP/2 connection preface (prior knowledge, RFC 9113
 * section 3.3), and by the HTTP/1.1 backend otherwise. Cleartext only (the
 * MHD backend has no TLS support). Returns the HTTP/2 server.
 */
ogs_sbi_server_t *ogs_sbi_server_add_dual(
        const char *interface,
        OpenAPI_uri_scheme_e scheme, ogs_sockaddr_t *addr,
        ogs_sockopt_t *option);
/* Hand an accepted connection to the HTTP/1.1 (MHD) backend of a server added with ogs_sbi_server_add_dual(). */
int ogs_mhd_server_add_connection(ogs_sbi_server_t *server,
        ogs_socket_t fd, const struct sockaddr *addr, socklen_t addrlen);
void ogs_sbi_server_remove(ogs_sbi_server_t *server);
void ogs_sbi_server_remove_all(void);

void ogs_sbi_server_set_advertise(
        ogs_sbi_server_t *server, int family, ogs_sockaddr_t *advertise);

int ogs_sbi_server_start_all(
        int (*cb)(ogs_sbi_request_t *request, void *data));
void ogs_sbi_server_graceful_shutdown_all(void);
void ogs_sbi_server_stop_all(void);

bool ogs_sbi_server_send_rspmem_persistent(
        ogs_sbi_stream_t *stream, ogs_sbi_response_t *response);
bool ogs_sbi_server_send_response(
        ogs_sbi_stream_t *stream, ogs_sbi_response_t *response);
bool ogs_sbi_server_send_error(ogs_sbi_stream_t *stream,
        int status, ogs_sbi_message_t *message,
        const char *title, const char *detail, const char *cause);

bool ogs_sbi_server_send_problem(
        ogs_sbi_stream_t *stream, OpenAPI_problem_details_t *problem);

ogs_sbi_server_t *ogs_sbi_server_from_stream(ogs_sbi_stream_t *stream);

ogs_pool_id_t ogs_sbi_id_from_stream(ogs_sbi_stream_t *stream);
void *ogs_sbi_stream_find_by_id(ogs_pool_id_t id);

/*
 * Helpers used by lib/sbi to link an outbound SBI transaction into
 * the inbound stream that triggered it. NFs do not call these
 * directly: ogs_sbi_discover_and_send() attaches when
 * xact->assoc_stream_id is set, and ogs_sbi_xact_remove() /
 * stream close detach automatically.
 *
 * Both helpers are idempotent and consult xact->to_stream_list as
 * the ground-truth attachment flag.
 *
 * ogs_sbi_server_attach_xact() returns OGS_OK when the transaction
 * is either freshly attached, already attached, or has no stream
 * to attach to (streamless self-initiated xact). It returns
 * OGS_NOTFOUND when the originating stream id is valid but the
 * stream itself is already gone.
 *
 * ogs_sbi_discover_and_send() does not propagate this OGS_NOTFOUND
 * to the NF caller. Some NF procedures send the original HTTP response
 * first and then continue with follow-up SBI transactions while still
 * passing the original stream pointer for convenience. Treating this
 * as a hard send failure would make those wrappers call their normal
 * error path, which may try to send another response on a stream that
 * has already been answered.
 *
 * Therefore, this condition is logged here, stream attachment is skipped,
 * and the upstream send continues.
 */
int ogs_sbi_server_attach_xact(ogs_sbi_xact_t *xact);
void ogs_sbi_server_detach_xact(ogs_sbi_xact_t *xact);

ogs_sbi_server_t *ogs_sbi_server_first(void);
ogs_sbi_server_t *ogs_sbi_server_next(ogs_sbi_server_t *current);
ogs_sbi_server_t *ogs_sbi_server_first_by_interface(const char *interface);
ogs_sbi_server_t *ogs_sbi_server_next_by_interface(
        ogs_sbi_server_t *current, const char *interface);

#ifdef __cplusplus
}
#endif

#endif /* OGS_SBI_SERVER_H */
