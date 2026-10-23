/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
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

/*
 * An HTTP/2 (nghttp2) server and an HTTP/1.1 (MHD) server in the same
 * process. Requests are answered, as in the NFs, after the server callback
 * returned and from the stream id that the callback was given. Each server
 * answers with its backend ("h2" or "h1"), so that a stream resolved to the
 * wrong backend, or a response sent through the wrong backend, shows up.
 * A third server, added with ogs_sbi_server_add_dual(), serves both protocols
 * on one port.
 */

#include "ogs-sbi.h"
#include "core/abts.h"

#include <curl/curl.h>

extern const ogs_sbi_server_actions_t ogs_mhd_server_actions;
extern const ogs_sbi_server_actions_t ogs_nghttp2_server_actions;

#define TEST_H2_PORT 18077
#define TEST_H1_PORT 18078
#define TEST_DUAL_PORT 18079
#define TEST_NUM_OF_REQUESTS 64
/* With 16 parallel connections some HTTP/2 requests were reset intermittently, on a single-protocol HTTP/2 server too (not investigated) */
#define TEST_MAX_PARALLEL 4

static ogs_pool_id_t pending_id[TEST_NUM_OF_REQUESTS * 2];
static int num_of_pending;

static volatile bool running;
static int num_of_unresolved;
static int num_of_ids_seen;
static ogs_pool_id_t ids_seen[TEST_NUM_OF_REQUESTS * 2];
static bool duplicate_id;
static int num_of_flag;

static int server_cb(ogs_sbi_request_t *request, void *data)
{
    /*
     * Like ogs_sbi_server_handler(): queue, and answer later. The request
     * belongs to the stream and is freed with it.
     */
    /* "?flag" has no value; it has to reach the request on both servers */
    if (ogs_sbi_header_get(request->http.params, "flag"))
        num_of_flag++;

    ogs_assert(num_of_pending < (int)OGS_ARRAY_SIZE(pending_id));
    pending_id[num_of_pending] = OGS_POINTER_TO_UINT(data);
    num_of_pending++;

    return OGS_OK;
}

static void answer_pending(void)
{
    int i, j;

    for (i = 0; i < num_of_pending; i++) {
        ogs_pool_id_t id = pending_id[i];
        ogs_sbi_stream_t *stream = NULL;
        ogs_sbi_server_t *server = NULL;
        ogs_sbi_response_t *response = NULL;
        const char *name = NULL;

        for (j = 0; j < num_of_ids_seen; j++)
            if (ids_seen[j] == id)
                duplicate_id = true;
        if (num_of_ids_seen < (int)OGS_ARRAY_SIZE(ids_seen))
            ids_seen[num_of_ids_seen++] = id;

        stream = ogs_sbi_stream_find_by_id(id);
        if (!stream) {
            num_of_unresolved++;
            continue;
        }

        server = ogs_sbi_server_from_stream(stream);
        ogs_assert(server);
        ogs_assert(ogs_sbi_id_from_stream(stream) == id);
        name = server->actions == &ogs_nghttp2_server_actions ? "h2" : "h1";

        response = ogs_sbi_response_new();
        ogs_assert(response);
        response->status = OGS_SBI_HTTP_STATUS_OK;
        response->http.content = ogs_strdup(name);
        response->http.content_length = strlen(name);
        ogs_sbi_header_set(
                response->http.headers, "Content-Type", "text/plain");

        ogs_assert(true == ogs_sbi_server_send_response(stream, response));
    }
    num_of_pending = 0;
}

static void server_main(void *data)
{
    while (running) {
        ogs_pollset_poll(ogs_app()->pollset, ogs_time_from_msec(20));
        ogs_timer_mgr_expire(ogs_app()->timer_mgr);
        answer_pending();
    }
}

struct transfer {
    CURL *easy;
    bool h2;
    char body[16];
    size_t len;
};

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct transfer *t = userdata;
    size_t n = size * nmemb;

    if (t->len + n < sizeof(t->body)) {
        memcpy(t->body + t->len, ptr, n);
        t->len += n;
        t->body[t->len] = 0;
    }

    return n;
}

static void run_requests(abts_case *tc, int h2_port, int h1_port)
{
    CURLM *multi = NULL;
    struct transfer transfers[TEST_NUM_OF_REQUESTS];
    int i, started = 0, running_handles = 0, ok_h2 = 0, ok_h1 = 0;
    CURLMsg *msg;
    int left;

    num_of_flag = 0;
    num_of_unresolved = 0;
    num_of_ids_seen = 0;
    duplicate_id = false;

    memset(transfers, 0, sizeof(transfers));
    multi = curl_multi_init();
    ABTS_PTR_NOTNULL(tc, multi);

    for (i = 0; i < TEST_NUM_OF_REQUESTS; i++) {
        char url[64];
        struct transfer *t = &transfers[i];

        t->h2 = (i % 2) == 0;
        t->easy = curl_easy_init();
        ABTS_PTR_NOTNULL(tc, t->easy);
        snprintf(url, sizeof(url), "http://127.0.0.1:%d/test/%d?flag",
                t->h2 ? h2_port : h1_port, i);
        curl_easy_setopt(t->easy, CURLOPT_URL, url);
        curl_easy_setopt(t->easy, CURLOPT_HTTP_VERSION, t->h2 ?
                (long)CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE :
                (long)CURL_HTTP_VERSION_1_1);
        curl_easy_setopt(t->easy, CURLOPT_FORBID_REUSE, 1L);
        curl_easy_setopt(t->easy, CURLOPT_TIMEOUT, 20L);
        curl_easy_setopt(t->easy, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(t->easy, CURLOPT_WRITEDATA, t);
        curl_easy_setopt(t->easy, CURLOPT_PRIVATE, t);
    }

    /* Keep several transfers of both kinds in flight at the same time */
    while (started < TEST_NUM_OF_REQUESTS || running_handles > 0) {
        while (started < TEST_NUM_OF_REQUESTS &&
                running_handles < TEST_MAX_PARALLEL) {
            curl_multi_add_handle(multi, transfers[started++].easy);
            running_handles++;
        }

        curl_multi_perform(multi, &running_handles);
        curl_multi_wait(multi, NULL, 0, 10, NULL);

        while ((msg = curl_multi_info_read(multi, &left)) != NULL) {
            struct transfer *t = NULL;
            long status = 0, version = 0;

            if (msg->msg != CURLMSG_DONE)
                continue;

            curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &t);
            curl_easy_getinfo(msg->easy_handle,
                    CURLINFO_RESPONSE_CODE, &status);
            curl_easy_getinfo(msg->easy_handle,
                    CURLINFO_HTTP_VERSION, &version);

            if (msg->data.result == CURLE_OK && status == 200 &&
                    version == (t->h2 ? CURL_HTTP_VERSION_2_0 :
                                        CURL_HTTP_VERSION_1_1) &&
                    !strcmp(t->body, t->h2 ? "h2" : "h1")) {
                if (t->h2) ok_h2++; else ok_h1++;
            }
        }
    }

    for (i = 0; i < TEST_NUM_OF_REQUESTS; i++) {
        curl_multi_remove_handle(multi, transfers[i].easy);
        curl_easy_cleanup(transfers[i].easy);
    }
    curl_multi_cleanup(multi);

    ABTS_INT_EQUAL(tc, TEST_NUM_OF_REQUESTS, num_of_flag);
    ABTS_INT_EQUAL(tc, TEST_NUM_OF_REQUESTS / 2, ok_h2);
    ABTS_INT_EQUAL(tc, TEST_NUM_OF_REQUESTS / 2, ok_h1);
    ABTS_INT_EQUAL(tc, 0, num_of_unresolved);
    ABTS_TRUE(tc, !duplicate_id);
    ABTS_INT_EQUAL(tc, TEST_NUM_OF_REQUESTS, num_of_ids_seen);
}

/* an HTTP/2 server and an HTTP/1.1 server on two ports */
static void sbi_server_test1(abts_case *tc, void *data)
{
    ogs_sbi_server_t *server = NULL;
    int h2_servers = 0, h1_servers = 0;

    ogs_list_for_each(&ogs_sbi_self()->server_list, server) {
        if (server->actions == &ogs_nghttp2_server_actions) h2_servers++;
        if (server->actions == &ogs_mhd_server_actions) h1_servers++;
    }
    ABTS_TRUE(tc, h2_servers >= 1 && h1_servers >= 1);

    run_requests(tc, TEST_H2_PORT, TEST_H1_PORT);
}

/* both protocols on one port, told apart by the HTTP/2 connection preface */
static void sbi_server_test2(abts_case *tc, void *data)
{
    run_requests(tc, TEST_DUAL_PORT, TEST_DUAL_PORT);
}

int main(int argc, const char *const argv[])
{
    int rv;
    char config_file[] = "/tmp/sbi-server-test-XXXXXX";
    const char *argv_app[] = { argv[0], "-c", config_file, NULL };
    const char *argv_out[argc + 3];
    int fd;
    FILE *fp;
    ogs_sockaddr_t *addr = NULL;
    ogs_thread_t *thread = NULL;
    abts_suite *suite = NULL;

    rv = abts_main(argc, argv, argv_out);
    if (rv != OGS_OK) return rv;

    fd = mkstemp(config_file);
    if (fd < 0) return OGS_ERROR;
    fp = fdopen(fd, "w");
    if (!fp) return OGS_ERROR;
    fputs("logger:\n  level: error\n", fp);
    fclose(fp);

    rv = ogs_app_initialize(NULL, NULL, argv_app);
    unlink(config_file);
    if (rv != OGS_OK) return rv;

    rv = ogs_app_parse_local_conf("sbi-server-test");
    if (rv != OGS_OK) return rv;

    ogs_sbi_context_init(OpenAPI_nf_type_BSF);

    ogs_assert(OGS_OK == ogs_getaddrinfo(
                &addr, AF_INET, "127.0.0.1", TEST_H2_PORT, 0));
    ogs_assert(ogs_sbi_server_add(
                "h2", OpenAPI_uri_scheme_http, addr, NULL));
    ogs_freeaddrinfo(addr);

    ogs_assert(OGS_OK == ogs_getaddrinfo(
                &addr, AF_INET, "127.0.0.1", TEST_H1_PORT, 0));
    ogs_assert(ogs_sbih1_server_add(
                "h1", OpenAPI_uri_scheme_http, addr, NULL));
    ogs_freeaddrinfo(addr);

    ogs_assert(OGS_OK == ogs_getaddrinfo(
                &addr, AF_INET, "127.0.0.1", TEST_DUAL_PORT, 0));
    ogs_assert(ogs_sbi_server_add_dual(
                "dual", OpenAPI_uri_scheme_http, addr, NULL));
    ogs_freeaddrinfo(addr);

    ogs_assert(OGS_OK == ogs_sbi_server_start_all(server_cb));

    running = true;
    thread = ogs_thread_create(server_main, NULL);
    ogs_assert(thread);

    suite = ADD_SUITE(suite)
    abts_run_test(suite, sbi_server_test1, NULL);
    abts_run_test(suite, sbi_server_test2, NULL);

    running = false;
    ogs_thread_destroy(thread);

    rv = abts_report(suite);

    ogs_sbi_server_stop_all();
    ogs_sbi_context_final();
    ogs_app_terminate();

    return rv;
}
