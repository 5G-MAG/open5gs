/*
 * Unit tests of the MB-SMF (src/smf) that need neither a peer nor a running SMF: functions of the MBS
 * handling that take their input as arguments.
 *
 * This file is self-contained (its own main): it runs on its own with `meson test smf-mbs`.
 */

#include "ogs-core.h"
#include "ogs-app.h"
#include "ogs-pfcp.h"
#include "ogs-gtp.h"
#include "ogs-sbi.h"
#include "context.h"
#include "n4mb-build.h"
#include "namf-build.h"

static int checks, failures;

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_STR(expected, actual) do { \
    const char *a_ = (actual); \
    checks++; \
    if (!a_ || strcmp((expected), a_) != 0) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: expected [%s] got [%s]\n", \
                __FILE__, __LINE__, (expected), a_ ? a_ : "(null)"); \
    } \
} while (0)

/* TS 23.003 V18.8.0 clause 30.2: "MBS Service ID uniquely identifies an MBS service within a PLMN." */
static void test_pick_mbs_service_id(void)
{
    ogs_list_t list;
    ogs_tmgi_t a, b, other_plmn;
    ogs_plmn_id_t plmn, plmn2;
    char *id;

    ogs_list_init(&list);
    ogs_plmn_id_build(&plmn, 1, 1, 2);
    ogs_plmn_id_build(&plmn2, 1, 2, 2);

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memset(&other_plmn, 0, sizeof(other_plmn));

    /* an empty pool gives the starting value */
    id = smf_tmgi_pick_mbs_service_id(&list, &plmn, 0x00ABCD);
    CHECK_STR("00ABCD", id);
    ogs_free(id);

    /* a taken value is skipped, the next free one is used */
    a.mbs_service_id = (char *)"00ABCD";
    memcpy(&a.plmn_id, &plmn, sizeof(plmn));
    ogs_list_add(&list, &a);
    id = smf_tmgi_pick_mbs_service_id(&list, &plmn, 0x00ABCD);
    CHECK_STR("00ABCE", id);
    ogs_free(id);

    b.mbs_service_id = (char *)"00ABCE";
    memcpy(&b.plmn_id, &plmn, sizeof(plmn));
    ogs_list_add(&list, &b);
    id = smf_tmgi_pick_mbs_service_id(&list, &plmn, 0x00ABCD);
    CHECK_STR("00ABCF", id);
    ogs_free(id);

    /* the same identifier under another PLMN is a different TMGI */
    other_plmn.mbs_service_id = (char *)"00ABCF";
    memcpy(&other_plmn.plmn_id, &plmn2, sizeof(plmn2));
    ogs_list_add(&list, &other_plmn);
    id = smf_tmgi_pick_mbs_service_id(&list, &plmn, 0x00ABCD);
    CHECK_STR("00ABCF", id);
    ogs_free(id);

    /* a TMGI that has no identifier yet does not take one */
    {
        ogs_tmgi_t fresh;
        memset(&fresh, 0, sizeof(fresh));
        ogs_list_add(&list, &fresh);
        id = smf_tmgi_pick_mbs_service_id(&list, &plmn, 0x00ABCD);
        CHECK_STR("00ABCF", id);
        ogs_free(id);
        ogs_list_remove(&list, &fresh);
    }

    /* the values wrap at FFFFFF */
    ogs_list_remove(&list, &a);
    ogs_list_remove(&list, &b);
    ogs_list_remove(&list, &other_plmn);
    a.mbs_service_id = (char *)"FFFFFF";
    ogs_list_add(&list, &a);
    id = smf_tmgi_pick_mbs_service_id(&list, &plmn, 0xFFFFFF);
    CHECK_STR("000000", id);
    ogs_free(id);

    /* case of the stored digits does not matter */
    a.mbs_service_id = (char *)"00abcd";
    id = smf_tmgi_pick_mbs_service_id(&list, &plmn, 0x00ABCD);
    CHECK_STR("00ABCE", id);
    ogs_free(id);

    ogs_list_remove(&list, &a);
}


/* A configuration that gives the SMF context what the MBS functions read: two tracking areas of two
 * PLMNs (the first listed is 001/01), one slice. No socket is opened. */
static const char *test_config =
    "logger:\n"
    "  level: error\n"
    "global:\n"
    "  max:\n"
    "    ue: 1024\n"
    "smf:\n"
    "  sbi:\n"
    "    server:\n"
    "      - address: 127.0.0.4\n"
    "        port: 7777\n"
    "  pfcp:\n"
    "    server:\n"
    "      - address: 127.0.0.4\n"
    "    client:\n"
    "      upf:\n"
    "        - address: 127.0.0.7\n"
    "  gtpc:\n"
    "    server:\n"
    "      - address: 127.0.0.4\n"
    "  gtpu:\n"
    "    server:\n"
    "      - address: 127.0.0.4\n"
    "  session:\n"
    "    - subnet: 10.45.0.0/16\n"
    "      gateway: 10.45.0.1\n"
    "  dns:\n"
    "    - 8.8.8.8\n"
    "  info:\n"
    "    - s_nssai:\n"
    "        - sst: 1\n"
    "          sd: 1\n"
    "          dnn:\n"
    "            - internet\n"
    "      tai:\n"
    "        - plmn_id:\n"
    "            mcc: 001\n"
    "            mnc: 01\n"
    "          tac: 1\n"
    "        - plmn_id:\n"
    "            mcc: 002\n"
    "            mnc: 02\n"
    "          tac: 2\n";

static int smf_context_setup(void)
{
    char dir[] = "/tmp/smf-mbs-test-XXXXXX";
    char path[64];
    FILE *fp;
    const char *argv[] = { "smf-mbs", "-c", path, NULL };

    if (!mkdtemp(dir)) return OGS_ERROR;
    snprintf(path, sizeof(path), "%s/smf.yaml", dir);
    fp = fopen(path, "w");
    if (!fp) return OGS_ERROR;
    fputs(test_config, fp);
    fclose(fp);

    if (ogs_app_initialize(NULL, path, argv) != OGS_OK) return OGS_ERROR;
    unlink(path);
    rmdir(dir);

    if (ogs_app_parse_local_conf("smf") != OGS_OK) return OGS_ERROR;
    ogs_gtp_context_init(ogs_app()->pool.nf * OGS_MAX_NUM_OF_GTPU_RESOURCE);
    ogs_pfcp_context_init();
    ogs_sbi_context_init(OpenAPI_nf_type_SMF);
    smf_context_init();
    if (ogs_gtp_context_parse_config("smf", "upf") != OGS_OK) return OGS_ERROR;
    if (ogs_pfcp_context_parse_config("smf", "upf") != OGS_OK) return OGS_ERROR;
    if (ogs_sbi_context_parse_config("smf", "nrf", "scp") != OGS_OK) return OGS_ERROR;
    if (smf_context_parse_config() != OGS_OK) return OGS_ERROR;

    /* the local PFCP address is learned when the socket is opened; here it is taken from the configuration */
    {
        ogs_socknode_t *node = ogs_list_first(&ogs_pfcp_self()->pfcp_list);
        if (!node) return OGS_ERROR;
        ogs_pfcp_self()->pfcp_addr = node->addr;
    }
    return OGS_OK;
}

/* TS 23.003 V18.8.0 clause 30.2, item 3: "The MNC identifies the PLMN which the MB-SMF belongs to". */
static void test_tmgi_plmn(void)
{
    ogs_plmn_id_t plmn;
    ogs_tmgi_t *tmgi;
    char *expiration = smf_tmgi_gen_expiration_time(60);

    /* no serving PLMN: the first listed tracking area, not the last */
    CHECK(smf_tmgi_own_plmn(&plmn));
    CHECK(ogs_plmn_id_mcc(&plmn) == 1);
    CHECK(ogs_plmn_id_mnc(&plmn) == 1);

    tmgi = smf_tmgi_allocate(expiration);
    CHECK(tmgi != NULL);
    if (tmgi) {
        CHECK(ogs_plmn_id_mcc(&tmgi->plmn_id) == 1);
        CHECK(ogs_plmn_id_mnc(&tmgi->plmn_id) == 1);
        CHECK(tmgi->mbs_service_id && strlen(tmgi->mbs_service_id) == 6);
        smf_tmgi_deallocate(tmgi);
    }

    /* a serving PLMN of the configuration takes precedence */
    ogs_plmn_id_build(&ogs_local_conf()->serving_plmn_id[0], 310, 14, 3);
    ogs_local_conf()->num_of_serving_plmn_id = 1;
    CHECK(smf_tmgi_own_plmn(&plmn));
    CHECK(ogs_plmn_id_mcc(&plmn) == 310);
    CHECK(ogs_plmn_id_mnc(&plmn) == 14);
    CHECK(ogs_plmn_id_mnc_len(&plmn) == 3);
    ogs_local_conf()->num_of_serving_plmn_id = 0;

    /* no PLMN anywhere: no TMGI with an all-zero PLMN */
    {
        ogs_sbi_nf_instance_t *self_nf = ogs_sbi_self()->nf_instance;
        ogs_sbi_nf_info_t *nf_info, *next;
        ogs_list_t saved;
        int before = smf_tmgi_count();

        memcpy(&saved, &self_nf->nf_info_list, sizeof(saved));
        memset(&self_nf->nf_info_list, 0, sizeof(self_nf->nf_info_list));
        ogs_list_init(&self_nf->nf_info_list);
        CHECK(!smf_tmgi_own_plmn(&plmn));
        CHECK(smf_tmgi_allocate(expiration) == NULL);
        CHECK(smf_tmgi_count() == before);
        memcpy(&self_nf->nf_info_list, &saved, sizeof(saved));
        (void)nf_info; (void)next;
    }

    ogs_free(expiration);
}

/* An MBS session of the SMF context for the builders: a broadcast session with a freshly allocated TMGI,
 * bound to the PFCP peer of the configuration. */
static smf_mbs_sess_t *new_broadcast_session(void)
{
    char *expiration = smf_tmgi_gen_expiration_time(60);
    ogs_tmgi_t *tmgi = smf_tmgi_allocate(expiration);
    smf_mbs_sess_t *mbs_sess;

    ogs_free(expiration);
    if (!tmgi) return NULL;
    mbs_sess = smf_mbs_sess_create(tmgi, true, NULL, "BROADCAST", NULL, NULL);
    if (!mbs_sess) return NULL;
    ogs_list_init(&mbs_sess->mbs_qos_flow_list);
    ogs_list_init(&mbs_sess->mbs_fsa_id_list);
    {
        /* the peer's address is learned at association; here it is taken from the configuration */
        ogs_pfcp_node_t *node = NULL;
        ogs_list_for_each(&ogs_pfcp_self()->pfcp_peer_list, node)
            memcpy(&node->addr, node->sa_list, sizeof(node->addr));
    }
    smf_mbs_sess_create_mbs_data_forwarding(mbs_sess);
    return mbs_sess;
}

/* Parses the N4mb Session Establishment Request built for mbs_sess. The parsed IEs point into the packet,
 * so both are kept until the next call. */
static ogs_pfcp_message_t *build_and_parse_establishment(smf_mbs_sess_t *mbs_sess)
{
    static ogs_pfcp_message_t *message;
    static ogs_pkbuf_t *pkbuf;

    if (message) { ogs_pfcp_message_free(message); message = NULL; }
    if (pkbuf) { ogs_pkbuf_free(pkbuf); pkbuf = NULL; }

    pkbuf = smf_n4mb_build_session_establishment_request(mbs_sess, (ogs_pfcp_xact_t *)&message);
    if (!pkbuf) return NULL;
    {
        /* the builder returns the body; the transaction layer adds the header, so it is added here */
        ogs_pfcp_header_t h;
        memset(&h, 0, sizeof(h));
        h.version = 1;
        h.seid_presence = 1;
        h.type = OGS_PFCP_SESSION_ESTABLISHMENT_REQUEST_TYPE;
        h.length = htobe16(pkbuf->len + OGS_PFCP_HEADER_LEN - 4);
        ogs_pkbuf_push(pkbuf, OGS_PFCP_HEADER_LEN);
        memcpy(pkbuf->data, &h, OGS_PFCP_HEADER_LEN);
    }
    message = ogs_pfcp_parse_msg(pkbuf);
    return message;
}

/* TS 23.003 V18.8.0 clause 12.7.1: "The NID shall consist of 11 hexadecimal digits"; TS 29.244 V18.10.0
 * clause 8.2.206: the NID field of the MBS Session Identifier. */
static void test_nid(void)
{
    uint8_t octets[6];
    smf_mbs_sess_t *mbs_sess;
    ogs_pfcp_message_t *msg;
    ogs_tlv_octet_t *id;
    static const uint8_t expected[6] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xA0 };

    CHECK(smf_nid_is_valid("0123456789A"));
    CHECK(smf_nid_is_valid("abcdefABCDE"));
    CHECK(!smf_nid_is_valid(NULL));
    CHECK(!smf_nid_is_valid(""));
    CHECK(!smf_nid_is_valid("0123456789"));
    CHECK(!smf_nid_is_valid("0123456789AB"));
    CHECK(!smf_nid_is_valid("0123456789G"));

    CHECK(smf_n4mb_nid_to_octets("0123456789A", octets));
    CHECK(memcmp(octets, expected, 6) == 0);
    CHECK(smf_n4mb_nid_to_octets("fFfFfFfFfFf", octets));
    {
        static const uint8_t ff[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0 };
        CHECK(memcmp(octets, ff, 6) == 0);
    }
    CHECK(!smf_n4mb_nid_to_octets("xyz", octets));

    /* without a NID the identifier carries the TMGI only */
    mbs_sess = new_broadcast_session();
    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    msg = build_and_parse_establishment(mbs_sess);
    CHECK(msg != NULL);
    if (msg) {
        id = &msg->pfcp_session_establishment_request.mbs_session_n4mb_control_information.mbs_session_identifier;
        CHECK(id->presence);
        CHECK(id->len == 7);
        CHECK(((uint8_t *)id->data)[0] == 0x01);
    }

    /* with a NID: NIDI set (bit 3) and the six octets after the TMGI */
    mbs_sess->mbs_session_id.nid = ogs_strdup("0123456789A");
    msg = build_and_parse_establishment(mbs_sess);
    CHECK(msg != NULL);
    if (msg) {
        id = &msg->pfcp_session_establishment_request.mbs_session_n4mb_control_information.mbs_session_identifier;
        CHECK(id->presence);
        CHECK(id->len == 13);
        CHECK(((uint8_t *)id->data)[0] == 0x05);
        CHECK(memcmp((uint8_t *)id->data + 7, expected, 6) == 0);
    }

    /* a NID that is not 11 hexadecimal digits is not sent */
    ogs_free(mbs_sess->mbs_session_id.nid);
    mbs_sess->mbs_session_id.nid = ogs_strdup("12345");
    CHECK(build_and_parse_establishment(mbs_sess) == NULL);

    /* the Namf_MBSBroadcast ContextCreate request carries the same NID in mbsSessionId (TS 29.518 V18.14.0
     * table 6.5.6.2.2-1) */
    ogs_free(mbs_sess->mbs_session_id.nid);
    mbs_sess->mbs_session_id.nid = NULL;
    {
        ogs_sbi_request_t *request = smf_namf_build_mbs_broadcast_context_create_request(mbs_sess, NULL);
        CHECK(request != NULL);
        if (request) {
            CHECK(request->http.content && strstr(request->http.content, "\"nid\"") == NULL);
            ogs_sbi_request_free(request);
        }
        mbs_sess->mbs_session_id.nid = ogs_strdup("0123456789A");
        request = smf_namf_build_mbs_broadcast_context_create_request(mbs_sess, NULL);
        CHECK(request != NULL);
        if (request) {
            CHECK(request->http.content && strstr(request->http.content, "\"nid\":\"0123456789A\"") != NULL);
            ogs_sbi_request_free(request);
        }
    }

    smf_mbs_sess_release(mbs_sess);
}

/* TS 29.532 V18.6.0 cl. 5.3.2.2.1: DNN and S-NSSAI of the Create request; TS 29.518 V18.14.0 table 6.5.6.2.2-1
 * (snssai) and NOTE 2: the S-NSSAI received, else a pre-configured default. */
static void test_dnn_snssai(void)
{
    smf_mbs_sess_t *mbs_sess = new_broadcast_session();
    ogs_pfcp_message_t *msg;
    ogs_sbi_request_t *request;
    OpenAPI_snssai_t *snssai;

    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;

    /* none given: neither IE in the N4mb request; the ContextCreate takes the configured slice (1, sd 000001) */
    msg = build_and_parse_establishment(mbs_sess);
    CHECK(msg != NULL);
    if (msg) {
        CHECK(!msg->pfcp_session_establishment_request.apn_dnn.presence);
        CHECK(!msg->pfcp_session_establishment_request.s_nssai.presence);
    }
    CHECK(smf_mbs_sess_build_snssai(mbs_sess) == NULL);
    request = smf_namf_build_mbs_broadcast_context_create_request(mbs_sess, NULL);
    CHECK(request != NULL);
    if (request) {
        CHECK(request->http.content && strstr(request->http.content, "\"snssai\":{\"sst\":1,\"sd\":\"000001\"}"));
        ogs_sbi_request_free(request);
    }

    /* given: both are sent to the MB-UPF and the S-NSSAI to the AMF, in place of the configured one */
    mbs_sess->dnn = ogs_strdup("mbs");
    mbs_sess->has_s_nssai = true;
    mbs_sess->s_nssai.sst = 3;
    mbs_sess->s_nssai.sd = ogs_s_nssai_sd_from_string("0000AB");
    msg = build_and_parse_establishment(mbs_sess);
    CHECK(msg != NULL);
    if (msg) {
        ogs_tlv_octet_t *dnn = &msg->pfcp_session_establishment_request.apn_dnn;
        ogs_tlv_octet_t *nssai = &msg->pfcp_session_establishment_request.s_nssai;
        CHECK(dnn->presence);
        CHECK(dnn->len == 4 && memcmp(dnn->data, "\x03mbs", 4) == 0);
        CHECK(nssai->presence);
        CHECK(nssai->len == 4 && ((uint8_t *)nssai->data)[0] == 3);
    }
    snssai = smf_mbs_sess_build_snssai(mbs_sess);
    CHECK(snssai != NULL);
    if (snssai) {
        CHECK(snssai->sst == 3);
        CHECK_STR("0000ab", snssai->sd);
        OpenAPI_snssai_free(snssai);
    }
    request = smf_namf_build_mbs_broadcast_context_create_request(mbs_sess, NULL);
    CHECK(request != NULL);
    if (request) {
        CHECK(request->http.content && strstr(request->http.content, "\"snssai\":{\"sst\":3,\"sd\":\"0000ab\"}"));
        ogs_sbi_request_free(request);
    }

    /* an S-NSSAI without SD */
    mbs_sess->s_nssai.sd = ogs_s_nssai_sd_from_string(NULL);
    snssai = smf_mbs_sess_build_snssai(mbs_sess);
    CHECK(snssai != NULL);
    if (snssai) {
        CHECK(snssai->sst == 3);
        CHECK(snssai->sd == NULL);
        OpenAPI_snssai_free(snssai);
    }

    smf_mbs_sess_release(mbs_sess);
}

/* A TMGI refreshed while an MBS session uses it keeps its place in the index of sessions by TMGI: a second
 * session on the same TMGI is still found to collide with it, and releasing the session removes it from the
 * index. */
static void test_tmgi_index_after_refresh(void)
{
    char *expiration = smf_tmgi_gen_expiration_time(60);
    ogs_tmgi_t *tmgi = smf_tmgi_allocate(expiration);
    smf_mbs_sess_t *first, *second, *third;

    ogs_free(expiration);
    CHECK(tmgi != NULL);
    if (!tmgi) return;

    first = smf_mbs_sess_create(tmgi, false, NULL, "BROADCAST", NULL, NULL);
    CHECK(first != NULL);
    if (!first) return;

    /* the refresh of TMGI Allocate: the expiration time is replaced by a new string */
    ogs_free(tmgi->expiration_time);
    tmgi->expiration_time = ogs_strdup("2099-01-01T00:00:00Z");

    second = smf_mbs_sess_create(tmgi, false, NULL, "BROADCAST", NULL, NULL);
    CHECK(second == NULL);
    if (second)
        smf_mbs_sess_release(second);

    smf_mbs_sess_release(first);

    third = smf_mbs_sess_create(tmgi, false, NULL, "BROADCAST", NULL, NULL);
    CHECK(third != NULL);
    if (third)
        smf_mbs_sess_release(third);

    smf_tmgi_deallocate(tmgi);
}

int main(void)
{
    /* ogs_app_initialize() initialises the core library itself */
    if (smf_context_setup() != OGS_OK) {
        fprintf(stderr, "FAIL: cannot set up the SMF context\n");
        return 2;
    }

    test_pick_mbs_service_id();
    test_tmgi_plmn();
    test_nid();
    test_dnn_snssai();
    test_tmgi_index_after_refresh();

    printf("smf-mbs: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
