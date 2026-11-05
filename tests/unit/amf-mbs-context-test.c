/*
 * Unit tests of the life of the AMF's Broadcast MBS session contexts (src/amf/context.c): what a context holds is
 * freed when it is removed (ContextDelete, a ContextCreate that replaces it, the shutdown of the AMF), and nothing of
 * it is found in a context created later, for the same TMGI or another. Runs on its own with `meson test amf-mbs-context`.
 * A talloc report of the core library at the end shows what was not freed.
 */

#include <unistd.h>

#include "ogs-core.h"
#include "ogs-app.h"
#include "ogs-sbi.h"
#include "context.h"
#include "ngap-build.h"

static int checks, failures;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

static const char *test_config =
    "logger:\n"
    "  level: error\n"
    "global:\n"
    "  max:\n"
    "    ue: 64\n"
    "amf:\n"
    "  sbi:\n"
    "    server:\n"
    "      - address: 127.0.0.5\n"
    "        port: 7777\n"
    "    client:\n"
    "      nrf:\n"
    "        - uri: http://127.0.0.201:7777\n"
    "  ngap:\n"
    "    server:\n"
    "      - address: 127.0.0.5\n"
    "  guami:\n"
    "    - plmn_id:\n"
    "        mcc: 999\n"
    "        mnc: 70\n"
    "      amf_id:\n"
    "        region: 2\n"
    "        set: 1\n"
    "  tai:\n"
    "    - plmn_id:\n"
    "        mcc: 999\n"
    "        mnc: 70\n"
    "      tac: 1\n"
    "  plmn_support:\n"
    "    - plmn_id:\n"
    "        mcc: 999\n"
    "        mnc: 70\n"
    "      s_nssai:\n"
    "        - sst: 1\n"
    "  security:\n"
    "    integrity_order : [ NIA2, NIA1, NIA0 ]\n"
    "    ciphering_order : [ NEA0, NEA1, NEA2 ]\n"
    "  network_name:\n"
    "    full: Open5GS\n"
    "    short: Next\n"
    "  amf_name: open5gs-amf-test\n"
    "  time:\n"
    "    t3512:\n"
    "      value: 540\n";

static int amf_context_setup(void)
{
    char dir[] = "/tmp/amf-mbs-context-test-XXXXXX";
    char path[64];
    FILE *fp;
    const char *argv[] = { "amf-mbs-context", "-c", path, NULL };

    if (!mkdtemp(dir)) return OGS_ERROR;
    snprintf(path, sizeof(path), "%s/amf.yaml", dir);
    fp = fopen(path, "w");
    if (!fp) return OGS_ERROR;
    fputs(test_config, fp);
    fclose(fp);

    if (ogs_app_initialize(NULL, path, argv) != OGS_OK) return OGS_ERROR;
    unlink(path);
    rmdir(dir);

    if (ogs_app_parse_local_conf("amf") != OGS_OK) return OGS_ERROR;
    ogs_sbi_context_init(OpenAPI_nf_type_AMF);
    amf_context_init();
    if (ogs_sbi_context_parse_config("amf", "nrf", "scp") != OGS_OK) return OGS_ERROR;
    if (amf_context_parse_config() != OGS_OK) return OGS_ERROR;
    return OGS_OK;
}

static ogs_tmgi_t make_tmgi(const char *service_id)
{
    ogs_tmgi_t tmgi;

    memset(&tmgi, 0, sizeof(tmgi));
    tmgi.mbs_service_id = (char *)service_id;
    ogs_plmn_id_build(&tmgi.plmn_id, 999, 70, 2);
    return tmgi;
}

static amf_gnb_t *make_gnb(uint32_t gnb_id)
{
    amf_gnb_t *gnb = ogs_calloc(1, sizeof(*gnb));

    ogs_plmn_id_build(&gnb->plmn_id, 999, 70, 2);
    gnb->gnb_id = gnb_id;
    gnb->gnb_id_bit_length = 22;
    gnb->id = (ogs_pool_id_t)gnb_id;
    return gnb;
}

/* Gives the context everything the AMF stores in one over its life: the last container, the Area Session IDs and the
 * areas, a gNB record with a re-establishment time, an outstanding operation, containers waiting for a response, the
 * notification URI. */
static void fill(amf_mbs_context_t *ctx, amf_gnb_t *gnb)
{
    amf_mbs_area_item_t *item = ogs_calloc(1, sizeof(*item));
    ogs_pkbuf_t *container = ogs_pkbuf_alloc(NULL, 4);
    amf_mbs_gnb_t *rec;

    ogs_pkbuf_put_data(container, "\x01\x02\x03\x04", 4);
    CHECK(amf_mbs_context_store_n2_container(ctx, container));
    ogs_pkbuf_free(container);

    ctx->location_dependent = true;
    item->area_session_id = 5;
    item->area = ogs_calloc(1, sizeof(*item->area));
    ogs_list_add(&ctx->area_items, item);
    ctx->area_session_id = 5;
    ctx->mbs_service_area = ogs_calloc(1, sizeof(*ctx->mbs_service_area));
    ctx->notify_uri = ogs_strdup("http://127.0.0.30:9999/notify");

    amf_mbs_context_involve_gnb(ctx, gnb);
    rec = amf_mbs_context_find_involved_gnb(ctx, gnb);
    CHECK(rec != NULL);
    if (rec) {
        rec->reestablish_at = ogs_get_monotonic_time() + ogs_time_from_sec(60);
        rec->restoring = true;
        rec->release_sent = true;
    }
    ogs_timer_start(ctx->t_reestablish, ogs_time_from_sec(60));
    ogs_timer_start(ctx->t_response, ogs_time_from_sec(60));
    ctx->gnb_request_count = 3;
    ctx->gnb_response_count = 1;
    ctx->release_pending = true;
}

/* The MBS Service ID is hexadecimal of either case (TS 29.571 Tmgi.mbsServiceId, pattern '^[A-Fa-f0-9]{6}$'): the
 * context of "00001A" is the one of "00001a", and not the one of another TMGI. */
static void test_find_by_tmgi_ignores_the_case_of_the_service_id(void)
{
    ogs_tmgi_t upper = make_tmgi("00001A"), lower = make_tmgi("00001a"), other = make_tmgi("00001B");
    amf_mbs_context_t *ctx = amf_mbs_context_create(&upper);

    CHECK(ctx != NULL);
    if (!ctx)
        return;
    CHECK(amf_mbs_context_find_by_tmgi(&lower) == ctx);
    CHECK(amf_mbs_context_find_by_tmgi(&upper) == ctx);
    CHECK(amf_mbs_context_find_by_tmgi(&other) == NULL);
    amf_mbs_context_remove(ctx);
}

/* A context created after another was removed is empty, whatever the earlier one held, for the same TMGI or another; and
 * the pool slot of the earlier one is reused (so a leftover would be found there). */
static void test_removed_context_leaves_nothing(void)
{
    ogs_tmgi_t tmgi = make_tmgi("000001");
    amf_gnb_t *gnb = make_gnb(0x100);
    amf_mbs_context_t *ctx = amf_mbs_context_create(&tmgi), *again;
    uint32_t index;
    char *ref;

    CHECK(ctx != NULL);
    if (!ctx)
        return;
    index = ctx->index;
    ref = ogs_strdup(ctx->mbs_context_ref);
    fill(ctx, gnb);
    CHECK(amf_mbs_context_find_by_tmgi(&tmgi) == ctx);

    /* ContextDelete (and the replacement of a context by a ContextCreate of the same TMGI) is this */
    amf_mbs_context_remove(ctx);
    CHECK(amf_mbs_context_find_by_tmgi(&tmgi) == NULL);
    CHECK(amf_mbs_context_find_by_ref(ref) == NULL);
    CHECK(amf_mbs_context_find_by_index(index) == NULL);
    ogs_free(ref);

    again = amf_mbs_context_create(&tmgi);
    CHECK(again != NULL);
    if (again) {
        CHECK(again->n2_mbs_sm_request == NULL);
        CHECK(ogs_list_empty(&again->area_items));
        CHECK(!again->location_dependent && again->area_session_id == 0);
        CHECK(again->mbs_service_area == NULL);
        CHECK(again->notify_uri == NULL && again->notify_client == NULL);
        CHECK(ogs_list_empty(&again->involved_gnb_list));
        CHECK(ogs_list_empty(&again->pending_n2_info_list));
        CHECK(again->gnb_request_count == 0 && again->gnb_response_count == 0);
        CHECK(!again->release_pending);
        CHECK(again->stream_id == OGS_INVALID_POOL_ID);
        CHECK(amf_mbs_context_earliest_reestablish(again) == 0);
        CHECK(again->t_reestablish != NULL && again->t_response != NULL);
        amf_mbs_context_remove(again);
    }

    ogs_free(gnb);
}

/* The shutdown of the AMF removes every context, with what each holds. */
static void test_final_frees_contexts(void)
{
    ogs_tmgi_t t1 = make_tmgi("000001"), t2 = make_tmgi("000002");
    amf_gnb_t *gnb = make_gnb(0x100);
    amf_mbs_context_t *a = amf_mbs_context_create(&t1), *b = amf_mbs_context_create(&t2);

    CHECK(a && b);
    if (a && b) {
        fill(a, gnb);
        fill(b, gnb);
        CHECK(ogs_list_count(&amf_self()->amf_mbs_context_list) == 2);
    }
    ogs_free(gnb);

    amf_context_final();
    CHECK(ogs_list_empty(&amf_self()->amf_mbs_context_list));
}

int main(void)
{
    if (amf_context_setup() != OGS_OK) {
        fprintf(stderr, "FAIL: cannot set up the AMF context\n");
        return 2;
    }

    test_removed_context_leaves_nothing();
    test_find_by_tmgi_ignores_the_case_of_the_service_id();
    test_final_frees_contexts();

    ogs_sbi_context_final();
    ogs_app_terminate();

    printf("amf-mbs-context: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
