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
#include "ngap-build.h"
#include "namf-build.h"
#include "namf-handler.h"
#include "nmbsmf-handler.h"
#include "mbs-update.h"
#include "mbs-subsc.h"
#include "mbs-n3mb.h"
#include "asn_internal.h"

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
    mbs_sess = smf_mbs_sess_create(tmgi, true, NULL, "BROADCAST", NULL, NULL, false, NULL);
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

    first = smf_mbs_sess_create(tmgi, false, NULL, "BROADCAST", NULL, NULL, false, NULL);
    CHECK(first != NULL);
    if (!first) return;

    /* the refresh of TMGI Allocate: the expiration time is replaced by a new string */
    ogs_free(tmgi->expiration_time);
    tmgi->expiration_time = ogs_strdup("2099-01-01T00:00:00Z");

    second = smf_mbs_sess_create(tmgi, false, NULL, "BROADCAST", NULL, NULL, false, NULL);
    CHECK(second == NULL);
    if (second)
        smf_mbs_sess_release(second);

    smf_mbs_sess_release(first);

    third = smf_mbs_sess_create(tmgi, false, NULL, "BROADCAST", NULL, NULL, false, NULL);
    CHECK(third != NULL);
    if (third)
        smf_mbs_sess_release(third);

    smf_tmgi_deallocate(tmgi);
}

static ogs_mbs_service_area_t *area_of_tacs(const char *tac1, const char *tac2)
{
    ogs_mbs_service_area_t *area = ogs_calloc(1, sizeof(*area));
    const char *tacs[2] = { tac1, tac2 };
    int i;

    area->tai_list = ogs_calloc(1, sizeof(*area->tai_list));
    for (i = 0; i < 2; i++) {
        ogs_tai_t *tai;
        if (!tacs[i]) continue;
        tai = ogs_calloc(1, sizeof(*tai));
        ogs_plmn_id_build(&tai->plmn_id, 1, 1, 2);
        tai->tac = ogs_strdup(tacs[i]);
        ogs_list_add(area->tai_list, tai);
    }
    return area;
}

static smf_mbs_sess_t *create_session(ogs_tmgi_t *tmgi, bool allocated, bool location_dependent,
        const char *tac1, const char *tac2, smf_mbs_sess_create_result_e *result)
{
    return smf_mbs_sess_create(tmgi, allocated, NULL, "BROADCAST",
            tac1 ? area_of_tacs(tac1, tac2) : NULL, NULL, location_dependent, result);
}

/* TS 29.532 V18.6.0 table 6.2.7.3-1: MBS_SESSION_ALREADY_CREATED ("For a location dependent MBS session, this
 * error shall be used if the MBS session associated to the indicated MBS session identifier and MBS service area
 * has already been created in the MB-SMF. For non location dependent MBS session, this error shall be used if
 * the MBS session indicated by the MBS session identifier has been created in the MB-SMF.") and
 * OVERLAPPING_MBS_SERVICE_AREA ("if the new MBS service area overlaps with the service area of any MBS session
 * with the same MBS session identifier."); a full MB-SMF is neither. */
static void test_create_outcomes(void)
{
    char *expiration = smf_tmgi_gen_expiration_time(60);
    ogs_tmgi_t *tmgi = smf_tmgi_allocate(expiration);
    smf_mbs_sess_create_result_e result;
    smf_mbs_sess_t *a, *b, *c, *many[64];
    int before, tmgis_before, n, max;

    ogs_free(expiration);
    CHECK(tmgi != NULL);
    if (!tmgi) return;
    before = ogs_list_count(&smf_self()->smf_mbs_sess_list);
    tmgis_before = smf_tmgi_count() - 1;    /* the TMGI above is the first of the test */

    /* a second session of a location independent MBS session */
    a = create_session(tmgi, true, false, NULL, NULL, &result);
    CHECK(a != NULL && result == SMF_MBS_SESS_CREATE_OK);
    b = create_session(tmgi, false, false, NULL, NULL, &result);
    CHECK(b == NULL && result == SMF_MBS_SESS_CREATE_ALREADY_CREATED);
    /* the refused session is not left in the list of sessions */
    CHECK(ogs_list_count(&smf_self()->smf_mbs_sess_list) == before + 1);
    /* a location dependent part next to a location independent session */
    b = create_session(tmgi, false, true, "1", NULL, &result);
    CHECK(b == NULL && result == SMF_MBS_SESS_CREATE_ALREADY_CREATED);
    smf_mbs_sess_release(a);
    /* the TMGI allocated with the only session goes with it */
    CHECK(smf_tmgi_count() == tmgis_before);

    /* location dependent: the parts of one TMGI in different areas coexist, with their own Area Session ID */
    tmgi = smf_tmgi_allocate(smf_tmgi_gen_expiration_time(60));
    CHECK(tmgi != NULL);
    if (!tmgi) return;
    a = create_session(tmgi, true, true, "1", "2", &result);
    CHECK(a != NULL && result == SMF_MBS_SESS_CREATE_OK);
    b = create_session(tmgi, false, true, "3", NULL, &result);
    CHECK(b != NULL && result == SMF_MBS_SESS_CREATE_OK);
    if (a && b) {
        CHECK(a->location_dependent && b->location_dependent);
        CHECK(a->area_session_id == 1);
        CHECK(b->area_session_id == 2);
    }
    /* the same area again: already created; an area that shares a TAI: overlapping */
    c = create_session(tmgi, false, true, "3", NULL, &result);
    CHECK(c == NULL && result == SMF_MBS_SESS_CREATE_ALREADY_CREATED);
    c = create_session(tmgi, false, true, "2", "9", &result);
    CHECK(c == NULL && result == SMF_MBS_SESS_CREATE_OVERLAPPING_MBS_SERVICE_AREA);
    /* a release frees its Area Session ID for the next part */
    if (a) smf_mbs_sess_release(a);
    c = create_session(tmgi, false, true, "7", NULL, &result);
    CHECK(c != NULL && result == SMF_MBS_SESS_CREATE_OK);
    if (c) CHECK(c->area_session_id == 1);

    /* the TMGI allocated with the first part (released above) goes with the last part */
    CHECK(smf_tmgi_count() == tmgis_before + 1);
    if (b) smf_mbs_sess_release(b);
    CHECK(smf_tmgi_count() == tmgis_before + 1);
    if (c) smf_mbs_sess_release(c);
    CHECK(smf_tmgi_count() == tmgis_before);

    /* a full MB-SMF is not "already created" */
    tmgi = smf_tmgi_allocate(smf_tmgi_gen_expiration_time(60));
    CHECK(tmgi != NULL);
    if (!tmgi) return;
    max = (int)ogs_global_conf()->max.mbs.mbs_sessions;
    CHECK(max > 0 && max < 64);
    for (n = 0; n < max; n++) {
        char tac[16];
        snprintf(tac, sizeof(tac), "%d", 100 + n);
        many[n] = create_session(tmgi, n == 0, true, tac, NULL, &result);
        CHECK(many[n] != NULL);
    }
    {
        smf_mbs_sess_t *extra = create_session(tmgi, false, true, "999", NULL, &result);
        CHECK(extra == NULL && result == SMF_MBS_SESS_CREATE_POOL_FULL);
    }
    for (n = 0; n < max; n++)
        if (many[n]) smf_mbs_sess_release(many[n]);
    CHECK(smf_tmgi_count() == tmgis_before);
    CHECK(ogs_list_count(&smf_self()->smf_mbs_sess_list) == before);
}

/* TS 29.532 V18.6.0 cl. 5.3.2.2.1: "If the MBS service area received in the request cannot be entirely covered by
 * the MB-SMF service area, the MB-SMF shall reduce the MBS service area to be within the MB-SMF service area and
 * continue the Create service operation using the reduced MBS service area." */
static void test_reduced_area_is_used(void)
{
    ogs_mbs_service_area_t *saved = smf_self()->mbs_service_area;
    ogs_mbs_service_area_t *coverage = area_of_tacs("1", "2");
    ogs_tmgi_t *tmgi = smf_tmgi_allocate(smf_tmgi_gen_expiration_time(60));
    smf_mbs_sess_t *mbs_sess;
    ogs_sbi_request_t *request;
    ogs_tai_t *tai;
    int count = 0;

    CHECK(tmgi != NULL);
    if (!tmgi) return;
    smf_self()->mbs_service_area = coverage;

    /* requested 1 and 9, covered 1 and 2: the session runs with 1 */
    mbs_sess = create_session(tmgi, true, false, "1", "9", NULL);
    CHECK(mbs_sess != NULL);
    if (mbs_sess) {
        CHECK(mbs_sess->service_area_reduced);
        CHECK(mbs_sess->mbs_service_area && mbs_sess->mbs_service_area->tai_list);
        if (mbs_sess->mbs_service_area && mbs_sess->mbs_service_area->tai_list) {
            ogs_list_for_each(mbs_sess->mbs_service_area->tai_list, tai) {
                CHECK_STR("1", tai->tac);
                count++;
            }
            CHECK(count == 1);
        }
        /* and that is what the AMF is asked to serve */
        ogs_list_init(&mbs_sess->mbs_qos_flow_list);
        ogs_list_init(&mbs_sess->mbs_fsa_id_list);
        request = smf_namf_build_mbs_broadcast_context_create_request(mbs_sess, NULL);
        CHECK(request != NULL);
        if (request) {
            CHECK(request->http.content && strstr(request->http.content, "\"tac\":\"1\"") != NULL);
            CHECK(request->http.content && strstr(request->http.content, "\"tac\":\"9\"") == NULL);
            ogs_sbi_request_free(request);
        }
        smf_mbs_sess_release(mbs_sess);
    }

    /* an area that is covered is not reduced */
    tmgi = smf_tmgi_allocate(smf_tmgi_gen_expiration_time(60));
    CHECK(tmgi != NULL);
    if (tmgi) {
        mbs_sess = create_session(tmgi, true, false, "1", "2", NULL);
        CHECK(mbs_sess != NULL);
        if (mbs_sess) {
            CHECK(!mbs_sess->service_area_reduced);
            smf_mbs_sess_release(mbs_sess);
        }
    }

    smf_self()->mbs_service_area = saved;
    ogs_mbs_service_area_free(coverage);
}

/* TS 29.532 V18.6.0 cl. 5.3.2.2.1, step 2a: the response contains "list of MBS FSA IDs, if any, for a broadcast
 * MBS session" (six hexadecimal digits, TS 23.003 V18.8.0 cl. 30.4). */
/* TS 29.571 V18.12.0, type MbsSession, attribute nrRedCapUeInfo (NrRedCapUeInfo, O, 0..1): "Indicates whether the
 * broadcast MBS session is intended only for NR (e)RedCap UEs, only for UEs that are neither NR RedCap UEs nor NR
 * eRedCap UEs, or for any kind of UEs." The model carries it as a string and writes it back unchanged. */
static void test_nr_red_cap_ue_info_model(void)
{
    cJSON *json = cJSON_Parse("{\"serviceType\":\"BROADCAST\",\"nrRedCapUeInfo\":\"NR_REDCAP_UE_ONLY\"}");
    OpenAPI_ext_mbs_session_t *session = OpenAPI_ext_mbs_session_parseFromJSON(json);
    cJSON *back = NULL, *member = NULL;

    cJSON_Delete(json);
    CHECK(session != NULL);
    CHECK(session && session->nr_red_cap_ue_info && strcmp(session->nr_red_cap_ue_info, "NR_REDCAP_UE_ONLY") == 0);
    back = session ? OpenAPI_ext_mbs_session_convertToJSON(session) : NULL;
    member = back ? cJSON_GetObjectItemCaseSensitive(back, "nrRedCapUeInfo") : NULL;
    CHECK(member && cJSON_IsString(member) && strcmp(member->valuestring, "NR_REDCAP_UE_ONLY") == 0);
    if (back) cJSON_Delete(back);
    if (session) OpenAPI_ext_mbs_session_free(session);

    json = cJSON_Parse("{\"serviceType\":\"BROADCAST\"}");
    session = OpenAPI_ext_mbs_session_parseFromJSON(json);
    cJSON_Delete(json);
    CHECK(session && session->nr_red_cap_ue_info == NULL);
    back = session ? OpenAPI_ext_mbs_session_convertToJSON(session) : NULL;
    CHECK(back && cJSON_GetObjectItemCaseSensitive(back, "nrRedCapUeInfo") == NULL);
    if (back) cJSON_Delete(back);
    if (session) OpenAPI_ext_mbs_session_free(session);
}

/* The Supported UE Type List of the MBS Session Setup or Modification Request Transfer. TS 38.413 V18.8.0 clause
 * 9.3.5.3: "Supported UE Type List |  | 0..<maxnoofUETypes> |  | YES | ignore" with "Supported UE Type | M |  |
 * ENUMERATED (Non-(e)RedCap UE, (e)RedCap UE, ...)". TS 23.247 V18.8.0 clause 7.3.1: "the MB-SMF includes the "NR
 * RedCap UE Information" parameter in N2 SM information." */
static int supported_ue_types_of(smf_mbs_sess_t *mbs_sess, long *types, int max)
{
    ogs_pkbuf_t *pkbuf = ngap_build_mbs_session_setup_or_modification_request_transfer(mbs_sess);
    NGAP_MBSSessionSetupOrModRequestTransfer_t message;
    int i, count = -1;

    CHECK(pkbuf != NULL);
    if (!pkbuf)
        return -1;
    memset(&message, 0, sizeof(message));
    CHECK(ogs_asn_decode(&asn_DEF_NGAP_MBSSessionSetupOrModRequestTransfer, &message, sizeof(message), pkbuf) ==
            OGS_OK);
    for (i = 0; i < message.protocolIEs.list.count; i++) {
        NGAP_MBSSessionSetupOrModRequestTransferIEs_t *ie = message.protocolIEs.list.array[i];

        if (ie->id == NGAP_ProtocolIE_ID_id_SupportedUETypeList) {
            int n;

            CHECK(ie->criticality == NGAP_Criticality_ignore);
            count = ie->value.choice.SupportedUETypeList.list.count;
            for (n = 0; n < count && n < max; n++)
                types[n] = *ie->value.choice.SupportedUETypeList.list.array[n];
        }
    }
    ogs_asn_free(&asn_DEF_NGAP_MBSSessionSetupOrModRequestTransfer, &message);
    ogs_pkbuf_free(pkbuf);
    return count;
}

static void test_nr_red_cap_ue_info_n2(void)
{
    smf_mbs_sess_t *mbs_sess = new_broadcast_session();
    long types[8];

    CHECK(smf_mbs_nr_red_cap_from_string(NULL) == SMF_MBS_NR_RED_CAP_NONE);
    CHECK(smf_mbs_nr_red_cap_from_string("NR_REDCAP_UE_ONLY") == SMF_MBS_NR_RED_CAP_UE_ONLY);
    CHECK(smf_mbs_nr_red_cap_from_string("BOTH_NR_REDCAP_UE_AND_NON_REDCAP_UE") == SMF_MBS_NR_RED_CAP_BOTH);
    CHECK(smf_mbs_nr_red_cap_from_string("NON_REDCAP_UE_ONLY") == SMF_MBS_NR_RED_CAP_NON_REDCAP_UE_ONLY);
    CHECK(smf_mbs_nr_red_cap_from_string("SOMETHING_ELSE") == SMF_MBS_NR_RED_CAP_NONE);
    if (!mbs_sess) {
        CHECK(0);
        return;
    }

    /* absent: the IE is left out */
    CHECK(supported_ue_types_of(mbs_sess, types, 8) == -1);

    mbs_sess->nr_red_cap_ue_info = SMF_MBS_NR_RED_CAP_UE_ONLY;
    CHECK(supported_ue_types_of(mbs_sess, types, 8) == 1 && types[0] == NGAP_SupportedUEType_redCap_eRedCap_UE);
    mbs_sess->nr_red_cap_ue_info = SMF_MBS_NR_RED_CAP_NON_REDCAP_UE_ONLY;
    CHECK(supported_ue_types_of(mbs_sess, types, 8) == 1 && types[0] == NGAP_SupportedUEType_non_RedCap_eRedCap_UE);
    mbs_sess->nr_red_cap_ue_info = SMF_MBS_NR_RED_CAP_BOTH;
    CHECK(supported_ue_types_of(mbs_sess, types, 8) == 2 && types[0] == NGAP_SupportedUEType_non_RedCap_eRedCap_UE &&
            types[1] == NGAP_SupportedUEType_redCap_eRedCap_UE);

    /* a multicast session carries no NR RedCap UE information */
    ogs_free(mbs_sess->service_type);
    mbs_sess->service_type = ogs_strdup("MULTICAST");
    CHECK(supported_ue_types_of(mbs_sess, types, 8) == -1);

    smf_mbs_sess_release(mbs_sess);
}

static void test_fsa_id_list(void)
{
    smf_mbs_sess_t *mbs_sess = new_broadcast_session();
    smf_mbs_fsa_id_t *a, *b;
    OpenAPI_list_t *list;
    OpenAPI_lnode_t *node;
    int count = 0;

    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;

    CHECK(smf_mbs_sess_build_fsa_id_list(mbs_sess) == NULL);

    a = ogs_calloc(1, sizeof(*a));
    a->id = ogs_uint24_from_string("00abcd");
    ogs_list_add(&mbs_sess->mbs_fsa_id_list, a);
    b = ogs_calloc(1, sizeof(*b));
    b->id = ogs_uint24_from_string("7");
    ogs_list_add(&mbs_sess->mbs_fsa_id_list, b);

    list = smf_mbs_sess_build_fsa_id_list(mbs_sess);
    CHECK(list != NULL);
    if (list) {
        OpenAPI_list_for_each(list, node) {
            if (count == 0) CHECK_STR("00abcd", (char *)node->data);
            if (count == 1) CHECK_STR("000007", (char *)node->data);
            count++;
        }
        CHECK(count == 2);
        OpenAPI_list_for_each(list, node)
            ogs_free(node->data);
        OpenAPI_list_free(list);
    }

    smf_mbs_sess_release(mbs_sess);
}

/* TS 29.518 V18.14.0 cl. 5.6.2.2: the ContextCreate "may also include the maxResponseTime IE" and "the MB-SMF
 * instance ID and MB-SMF service instance ID". */
static void test_context_create_optional_ids(void)
{
    smf_mbs_sess_t *mbs_sess = new_broadcast_session();
    ogs_sbi_nf_instance_t *self_nf = ogs_sbi_self()->nf_instance;
    ogs_sbi_nf_service_t *service;
    ogs_sbi_request_t *request;
    char expected[200];

    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;

    /* no service registered yet, no time configured: the SMF's instance ID only */
    request = smf_namf_build_mbs_broadcast_context_create_request(mbs_sess, NULL);
    CHECK(request != NULL);
    if (request) {
        snprintf(expected, sizeof(expected), "\"mbsmfId\":\"%s\"", self_nf->id);
        CHECK(request->http.content && strstr(request->http.content, expected) != NULL);
        CHECK(request->http.content && strstr(request->http.content, "mbsmfServiceInstId") == NULL);
        CHECK(request->http.content && strstr(request->http.content, "maxResponseTime") == NULL);
        ogs_sbi_request_free(request);
    }

    service = ogs_sbi_nf_service_build_default(self_nf, (char *)OGS_SBI_SERVICE_NAME_NMBSMF_MBS_SESSION);
    CHECK(service != NULL);
    smf_self()->has_mbs_max_response_time = true;
    smf_self()->mbs_max_response_time = 7;
    request = smf_namf_build_mbs_broadcast_context_create_request(mbs_sess, NULL);
    CHECK(request != NULL);
    if (request && service) {
        snprintf(expected, sizeof(expected), "\"mbsmfServiceInstId\":\"%s\"", service->id);
        CHECK(request->http.content && strstr(request->http.content, expected) != NULL);
        CHECK(request->http.content && strstr(request->http.content, "\"maxResponseTime\":7") != NULL);
        ogs_sbi_request_free(request);
    }
    smf_self()->has_mbs_max_response_time = false;
    if (service)
        ogs_sbi_nf_service_remove(service);

    smf_mbs_sess_release(mbs_sess);
}

/* TS 29.571 V18.12.0 DateTime (RFC 3339 date-time): the MBS start and termination times of a Create request */
static void test_date_time(void)
{
    CHECK(smf_is_date_time("2026-11-01T10:00:00Z"));
    CHECK(smf_is_date_time("2026-11-01T10:00:00.123456Z"));
    CHECK(smf_is_date_time("2026-11-01T10:00:00+01:00"));
    CHECK(!smf_is_date_time(NULL));
    CHECK(!smf_is_date_time(""));
    CHECK(!smf_is_date_time("tomorrow"));
    CHECK(!smf_is_date_time("2026-11-01"));
}

static OpenAPI_mbs_service_info_t *service_info_with_5qis(const int *five_qis, int n)
{
    OpenAPI_list_t *comps = OpenAPI_list_create();
    int i;

    for (i = 0; i < n; i++) {
        OpenAPI_mbs_qo_s_req_t *qos = ogs_calloc(1, sizeof(*qos));
        OpenAPI_mbs_media_comp_rm_t *comp = ogs_calloc(1, sizeof(*comp));
        qos->_5qi = five_qis[i];
        comp->mbs_qo_s_req = qos;
        OpenAPI_list_add(comps, OpenAPI_map_create(ogs_msprintf("%d", i + 1), comp));
    }
    return OpenAPI_mbs_service_info_create(comps, 0, NULL, NULL);
}

/* TS 29.532 V18.6.0 table 6.2.7.3-1: INVALID_MBS_SERVICE_INFO; TS 29.571 V18.12.0 5Qi: "within the range 0 to
 * 255"; TS 38.413 V18.8.0 maxnoofMBSQoSFlows: "Value is 64." */
static void test_service_info_validity(void)
{
    const char *detail = NULL;
    int ok[3] = { 9, 0, 255 };
    int bad_high[2] = { 9, 256 };
    int bad_low[1] = { -1 };
    int many[SMF_MBS_MAX_QOS_FLOWS + 1];
    OpenAPI_mbs_service_info_t *info;
    int i;

    CHECK(smf_nmbsmf_mbs_service_info_is_valid(NULL, &detail));

    info = service_info_with_5qis(ok, 3);
    CHECK(smf_nmbsmf_mbs_service_info_is_valid(info, &detail));
    info = service_info_with_5qis(bad_high, 2);
    CHECK(!smf_nmbsmf_mbs_service_info_is_valid(info, &detail));
    CHECK(detail && strstr(detail, "5qi") != NULL);
    info = service_info_with_5qis(bad_low, 1);
    CHECK(!smf_nmbsmf_mbs_service_info_is_valid(info, &detail));

    for (i = 0; i <= SMF_MBS_MAX_QOS_FLOWS; i++)
        many[i] = 9;
    info = service_info_with_5qis(many, SMF_MBS_MAX_QOS_FLOWS);
    CHECK(smf_nmbsmf_mbs_service_info_is_valid(info, &detail));
    info = service_info_with_5qis(many, SMF_MBS_MAX_QOS_FLOWS + 1);
    CHECK(!smf_nmbsmf_mbs_service_info_is_valid(info, &detail));
    CHECK(detail && strstr(detail, "flows") != NULL);
}

/* TS 29.214 V18.4.0 cl. 5.3.8, Flow-Description AVP: "The Source Port may be omitted to indicate that any source
 * port is allowed. Lists or ranges shall not be used.", "Only the Action "permit" shall be used.", "No "options"
 * shall be used.", "The invert modifier "!" for addresses shall not be used.", "The keyword "assigned" shall not
 * be used.", "the Flow description AVP shall be used to describe a single IP flow." */
static void test_flow_description_restrictions(void)
{
    static const char *const valid[] = {
        "permit out ip from any to any",
        "permit out 17 from 10.0.0.1 to 224.0.2.1 5004",
        "permit in 17 from 10.0.0.1/24 5000 to 224.0.2.1/32 5004",
        "permit out udp from 2001:db8::1 to ff3e::1 5004-5010",
        "permit out udp from any to any 5004,5006",
        "permit out ip from me to any",
        "permit  out  tcp  from any to any",
        "permit out tcp from any 80 to any",
    };
    static const struct { const char *desc; smf_nmbsmf_flow_desc_result_e result; const char *fault; } bad[] = {
        { "permit out udp from any 5000-5010 to any 5004", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "source port" },
        { "permit out udp from any 5000,5002 to any 5004", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "source port" },
        { "deny out udp from any to any", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "permit" },
        { "permit out udp from any to any 5004 frag", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "options" },
        { "permit out tcp from any to any established", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "options" },
        { "permit out udp from !10.0.0.1 to any", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "invert" },
        { "permit out udp from any to !224.0.2.1 5004", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "invert" },
        { "permit out udp from ! 10.0.0.1 to any", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "invert" },
        { "permit out udp from assigned to any", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "assigned" },
        { "permit out udp from any to assigned 5004", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "assigned" },
        { "permit out udp from any to any permit in udp from any to any", SMF_NMBSMF_FLOW_DESC_RESTRICTION, "single" },
        { "", SMF_NMBSMF_FLOW_DESC_MALFORMED, "empty" },
        { "allow out udp from any to any", SMF_NMBSMF_FLOW_DESC_MALFORMED, "action" },
        { "permit sideways udp from any to any", SMF_NMBSMF_FLOW_DESC_MALFORMED, "permit in|out" },
        { "permit out udp from any", SMF_NMBSMF_FLOW_DESC_MALFORMED, "to" },
    };
    OpenAPI_mbs_service_info_t *info;
    OpenAPI_mbs_media_comp_rm_t *comp;
    OpenAPI_map_t *pair;
    const char *detail = NULL;
    size_t i;

    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        detail = NULL;
        CHECK(smf_nmbsmf_flow_description_check(valid[i], &detail) == SMF_NMBSMF_FLOW_DESC_OK);
    }
    for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        detail = NULL;
        CHECK(smf_nmbsmf_flow_description_check(bad[i].desc, &detail) == bad[i].result);
        CHECK(detail && strstr(detail, bad[i].fault) != NULL);
    }

    /* through the service information: the flow of media component 2 carries the fault */
    {
        int five_qis[2] = { 9, 9 };

        info = service_info_with_5qis(five_qis, 2);
        CHECK(smf_nmbsmf_mbs_flow_descriptions_check(NULL, &detail) == SMF_NMBSMF_FLOW_DESC_OK);
        CHECK(smf_nmbsmf_mbs_flow_descriptions_check(info, &detail) == SMF_NMBSMF_FLOW_DESC_OK);
        pair = (OpenAPI_map_t *)info->mbs_media_comps->first->next->data;
        comp = (OpenAPI_mbs_media_comp_rm_t *)pair->value;
        comp->mbs_flow_descs = OpenAPI_list_create();
        OpenAPI_list_add(comp->mbs_flow_descs, ogs_strdup("permit out udp from any to 224.0.2.1 5004"));
        CHECK(smf_nmbsmf_mbs_flow_descriptions_check(info, &detail) == SMF_NMBSMF_FLOW_DESC_OK);
        OpenAPI_list_add(comp->mbs_flow_descs, ogs_strdup("permit out udp from any to 224.0.2.1 5004 frag"));
        CHECK(smf_nmbsmf_mbs_flow_descriptions_check(info, &detail) == SMF_NMBSMF_FLOW_DESC_RESTRICTION);
        CHECK(detail && strstr(detail, "media component 2") != NULL);
    }
}

/* TS 29.244 V18.10.0 cl. 5.34.2.2: "for each MBS QoS flow: a Create PDR ... a Create QER ... (associated with the
 * PDR ...) ...; the IQFISN ... flag in the Create QER IE shall be set to "1" ... a Create FAR ... (associated with
 * the PDR ...)", and table 7.5.2.7-1: the Traffic Endpoint ID that the DL PDR and the endpoint share. */
static void test_per_flow_user_plane(void)
{
    char *expiration = smf_tmgi_gen_expiration_time(60);
    ogs_tmgi_t *tmgi = smf_tmgi_allocate(expiration);
    smf_mbs_sess_t *mbs_sess;
    ogs_pfcp_message_t *msg;
    ogs_pfcp_session_establishment_request_t *req;
    int i;

    ogs_free(expiration);
    CHECK(tmgi != NULL);
    if (!tmgi) return;
    mbs_sess = smf_mbs_sess_create(tmgi, true, NULL, "BROADCAST", NULL, NULL, false, NULL);
    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    ogs_list_init(&mbs_sess->mbs_qos_flow_list);
    ogs_list_init(&mbs_sess->mbs_fsa_id_list);
    for (i = 0; i < 3; i++) {
        smf_mbs_qos_flow_t *flow = ogs_calloc(1, sizeof(*flow));
        flow->qfi = i + 1;
        flow->five_qi = 9;
        ogs_list_add(&mbs_sess->mbs_qos_flow_list, flow);
    }
    {
        ogs_pfcp_node_t *node = NULL;
        ogs_list_for_each(&ogs_pfcp_self()->pfcp_peer_list, node)
            memcpy(&node->addr, node->sa_list, sizeof(node->addr));
    }
    mbs_sess->ingress_tun_addr_req = true;
    CHECK(smf_mbs_sess_create_mbs_data_forwarding(mbs_sess));

    msg = build_and_parse_establishment(mbs_sess);
    CHECK(msg != NULL);
    if (msg) {
        req = &msg->pfcp_session_establishment_request;
        for (i = 0; i < 3; i++) {
            CHECK(req->create_pdr[i].presence);
            CHECK(req->create_far[i].presence);
            CHECK(req->create_qer[i].presence);
            /* the QER of flow i carries QFI i+1 and IQFISN */
            CHECK(req->create_qer[i].qos_flow_identifier.presence &&
                    req->create_qer[i].qos_flow_identifier.u8 == i + 1);
            CHECK(req->create_qer[i].qer_indications.presence &&
                    req->create_qer[i].qer_indications.len == 1 &&
                    (*(uint8_t *)req->create_qer[i].qer_indications.data & 0x01));
            /* the PDR refers to its own QER and FAR, to the endpoint */
            CHECK(req->create_pdr[i].qer_id.presence && req->create_qer[i].qer_id.presence &&
                    req->create_pdr[i].qer_id.u32 == req->create_qer[i].qer_id.u32);
            CHECK(req->create_pdr[i].far_id.presence && req->create_far[i].far_id.presence &&
                    req->create_pdr[i].far_id.u32 == req->create_far[i].far_id.u32);
            CHECK(req->create_pdr[i].pdi.traffic_endpoint_id.presence &&
                    *(uint8_t *)req->create_pdr[i].pdi.traffic_endpoint_id.data == 1);
        }
        CHECK(!req->create_pdr[3].presence && !req->create_far[3].presence && !req->create_qer[3].presence);
        CHECK(req->create_traffic_endpoint.presence);
        CHECK(req->create_traffic_endpoint.traffic_endpoint_id.presence &&
                *(uint8_t *)req->create_traffic_endpoint.traffic_endpoint_id.data == 1);
        CHECK(req->create_traffic_endpoint.local_ingress_tunnel.presence);
        /* distinct ids */
        CHECK(req->create_qer[0].qer_id.u32 != req->create_qer[1].qer_id.u32);
        CHECK(req->create_pdr[0].pdr_id.u16 != req->create_pdr[1].pdr_id.u16);
        /* TS 29.244 Create PDR: Precedence is mandatory, and the MB-UPF refuses a Create PDR without it */
        CHECK(req->create_pdr[0].precedence.presence && req->create_pdr[1].precedence.presence);
    }
    smf_mbs_sess_release(mbs_sess);

    /* more flows than a PFCP session has QERs: refused, nothing provisioned */
    tmgi = smf_tmgi_allocate(smf_tmgi_gen_expiration_time(60));
    CHECK(tmgi != NULL);
    if (!tmgi) return;
    mbs_sess = smf_mbs_sess_create(tmgi, true, NULL, "BROADCAST", NULL, NULL, false, NULL);
    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    ogs_list_init(&mbs_sess->mbs_qos_flow_list);
    ogs_list_init(&mbs_sess->mbs_fsa_id_list);
    for (i = 0; i < OGS_MAX_NUM_OF_QER + 1; i++) {
        smf_mbs_qos_flow_t *flow = ogs_calloc(1, sizeof(*flow));
        flow->qfi = i + 1;
        ogs_list_add(&mbs_sess->mbs_qos_flow_list, flow);
    }
    CHECK(!smf_mbs_sess_create_mbs_data_forwarding(mbs_sess));
    CHECK(ogs_list_empty(&mbs_sess->pfcp.pdr_list));
    smf_mbs_sess_release(mbs_sess);
}


static OpenAPI_patch_item_t *patch_item(OpenAPI_patch_operation_e op, const char *path, const char *from,
        const char *value_json)
{
    cJSON *value = value_json ? cJSON_Parse(value_json) : NULL;
    OpenAPI_any_type_t *any = value ? OpenAPI_any_type_create(value) : NULL;

    if (value) cJSON_Delete(value);
    return OpenAPI_patch_item_create(op, ogs_strdup(path), from ? ogs_strdup(from) : NULL, false, any);
}

static void free_patch_items(OpenAPI_list_t *items)
{
    OpenAPI_lnode_t *node;

    OpenAPI_list_for_each(items, node)
        OpenAPI_patch_item_free(node->data);
    OpenAPI_list_free(items);
}

/* Applies one operation to a copy of the document text; returns the error or NULL, and the result in *out
 * (allocated). */
static const char *patch_one(const char *doc_text, OpenAPI_patch_operation_e op, const char *path,
        const char *from, const char *value_json, char **out)
{
    cJSON *doc = cJSON_Parse(doc_text);
    OpenAPI_list_t *items = OpenAPI_list_create();
    const char *err;

    OpenAPI_list_add(items, patch_item(op, path, from, value_json));
    err = smf_json_patch_apply(doc, items);
    *out = cJSON_PrintUnformatted(doc);
    cJSON_Delete(doc);
    free_patch_items(items);
    return err;
}

/* RFC 6902: 4.1 add ("If the target location specifies an array index, a new value is inserted into the array at
 * the specified index", "The specified index MUST NOT be greater than the number of elements in the array", "-"
 * appends), 4.2 remove ("The target location MUST exist"), 4.3 replace ("The target location MUST exist"),
 * 4.4 move, 4.5 copy, 4.6 test; RFC 6901: "~1" is "/" and "~0" is "~", an index has no leading zeros. */
static void test_json_patch(void)
{
    const char *doc = "{\"a\":{\"b\":[1,2,3],\"c/d\":1,\"e~f\":2}}";
    char *out;

    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "/a/x", NULL, "{\"k\":1}", &out) == NULL);
    CHECK_STR("{\"a\":{\"b\":[1,2,3],\"c/d\":1,\"e~f\":2,\"x\":{\"k\":1}}}", out); ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "/a/b/1", NULL, "9", &out) == NULL);
    CHECK_STR("{\"a\":{\"b\":[1,9,2,3],\"c/d\":1,\"e~f\":2}}", out); ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "/a/b/-", NULL, "4", &out) == NULL);
    CHECK_STR("{\"a\":{\"b\":[1,2,3,4],\"c/d\":1,\"e~f\":2}}", out); ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "/a/b/3", NULL, "4", &out) == NULL);   /* index == size */
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "/a/b/4", NULL, "4", &out) != NULL);   /* index > size */
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "/a/b/01", NULL, "4", &out) != NULL);  /* leading zero */
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "/z/y", NULL, "4", &out) != NULL);     /* no parent */
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "/a/b", NULL, "[]", &out) == NULL);    /* replaces a member */
    CHECK_STR("{\"a\":{\"b\":[],\"c/d\":1,\"e~f\":2}}", out); ogs_free(out);

    CHECK(patch_one(doc, OpenAPI_patch_operation_replace, "/a/b/1", NULL, "9", &out) == NULL);
    CHECK_STR("{\"a\":{\"b\":[1,9,3],\"c/d\":1,\"e~f\":2}}", out); ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_replace, "/a/c~1d", NULL, "7", &out) == NULL);
    CHECK_STR("{\"a\":{\"b\":[1,2,3],\"c/d\":7,\"e~f\":2}}", out); ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_replace, "/a/e~0f", NULL, "7", &out) == NULL);
    CHECK_STR("{\"a\":{\"b\":[1,2,3],\"c/d\":1,\"e~f\":7}}", out); ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_replace, "/a/nothing", NULL, "7", &out) != NULL);
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_replace, "/a/b/3", NULL, "7", &out) != NULL);
    ogs_free(out);

    CHECK(patch_one(doc, OpenAPI_patch_operation__remove, "/a/b/0", NULL, NULL, &out) == NULL);
    CHECK_STR("{\"a\":{\"b\":[2,3],\"c/d\":1,\"e~f\":2}}", out); ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation__remove, "/a/c~1d", NULL, NULL, &out) == NULL);
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation__remove, "/a/nothing", NULL, NULL, &out) != NULL);
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation__remove, "/a/b/3", NULL, NULL, &out) != NULL);
    ogs_free(out);

    CHECK(patch_one(doc, OpenAPI_patch_operation_move, "/a/m", "/a/b", NULL, &out) == NULL);
    CHECK_STR("{\"a\":{\"c/d\":1,\"e~f\":2,\"m\":[1,2,3]}}", out); ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_move, "/a/b/0", "/a/b", NULL, &out) != NULL);  /* into itself */
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_copy, "/a/m", "/a/b", NULL, &out) == NULL);
    CHECK_STR("{\"a\":{\"b\":[1,2,3],\"c/d\":1,\"e~f\":2,\"m\":[1,2,3]}}", out); ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_copy, "/a/m", "/a/none", NULL, &out) != NULL);
    ogs_free(out);

    CHECK(patch_one(doc, OpenAPI_patch_operation_test, "/a/b/1", NULL, "2", &out) == NULL);
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_test, "/a/b/1", NULL, "3", &out) != NULL);
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_test, "/a/b", NULL, "[1,2,3]", &out) == NULL);
    ogs_free(out);

    /* an operation without its value, a path that is no pointer */
    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "/a/q", NULL, NULL, &out) != NULL);
    ogs_free(out);
    CHECK(patch_one(doc, OpenAPI_patch_operation_add, "a/q", NULL, "1", &out) != NULL);
    ogs_free(out);
}

static const char *const area_1_text =
    "{\"taiList\":[{\"plmnId\":{\"mcc\":\"001\",\"mnc\":\"01\"},\"tac\":\"1\"}]}";
static const char *const serv_info_2_text =
    "{\"mbsMediaComps\":{\"1\":{\"mbsMedCompNum\":1,\"mbsQoSReq\":{\"5qi\":9}},"
    "\"2\":{\"mbsMedCompNum\":2,\"mbsQoSReq\":{\"5qi\":5}}}}";

/* A session as the Create handler leaves it: flows from the service information, QFIs in order, user plane
 * rules, the documents as sent. */
static smf_mbs_sess_t *make_session(const char *serv_info, const char *area)
{
    ogs_tmgi_t *tmgi = smf_tmgi_allocate(smf_tmgi_gen_expiration_time(60));
    smf_mbs_sess_t *mbs_sess;
    OpenAPI_mbs_service_info_t *info = NULL;
    smf_mbs_qos_flow_t *flow;
    uint8_t qfi = 1;

    if (!tmgi) return NULL;
    mbs_sess = smf_mbs_sess_create(tmgi, true, NULL, "BROADCAST", NULL, NULL, false, NULL);
    if (!mbs_sess) return NULL;
    ogs_list_init(&mbs_sess->mbs_qos_flow_list);
    ogs_list_init(&mbs_sess->mbs_fsa_id_list);
    {
        ogs_pfcp_node_t *node = NULL;
        ogs_list_for_each(&ogs_pfcp_self()->pfcp_peer_list, node)
            memcpy(&node->addr, node->sa_list, sizeof(node->addr));
    }
    if (serv_info) {
        cJSON *json = cJSON_Parse(serv_info);
        info = OpenAPI_mbs_service_info_parseFromJSON(json);
        cJSON_Delete(json);
    }
    smf_mbs_qos_flows_from_service_info(info, &mbs_sess->mbs_qos_flow_list);
    ogs_list_for_each(&mbs_sess->mbs_qos_flow_list, flow)
        flow->qfi = qfi++;
    mbs_sess->mbs_serv_info_json = smf_mbs_service_info_to_json(info);
    if (info) OpenAPI_mbs_service_info_free(info);
    if (area) {
        cJSON *json = cJSON_Parse(area);
        OpenAPI_mbs_service_area_t *api_area = OpenAPI_mbs_service_area_parseFromJSON(json);
        cJSON_Delete(json);
        smf_mbs_service_area_from_openapi(api_area, &mbs_sess->mbs_service_area);
        mbs_sess->mbs_service_area_json = smf_mbs_service_area_to_json(api_area);
        OpenAPI_mbs_service_area_free(api_area);
    }
    smf_mbs_sess_create_mbs_data_forwarding(mbs_sess);
    return mbs_sess;
}

static int count_flows(smf_mbs_sess_t *mbs_sess)
{
    return ogs_list_count(&mbs_sess->mbs_qos_flow_list);
}

static smf_mbs_qos_flow_t *flow_of(smf_mbs_sess_t *mbs_sess, const char *key)
{
    smf_mbs_qos_flow_t *flow;

    ogs_list_for_each(&mbs_sess->mbs_qos_flow_list, flow)
        if (flow->med_comp_num && strcmp(flow->med_comp_num, key) == 0)
            return flow;
    return NULL;
}

static smf_mbs_update_result_e update_with(smf_mbs_sess_t *mbs_sess, smf_mbs_update_t *update, const char **detail,
        OpenAPI_patch_operation_e op, const char *path, const char *value_json)
{
    OpenAPI_list_t *items = OpenAPI_list_create();
    smf_mbs_update_result_e result;

    OpenAPI_list_add(items, patch_item(op, path, NULL, value_json));
    result = smf_mbs_sess_update(mbs_sess, items, update, detail);
    free_patch_items(items);
    return result;
}

static int count_tais(smf_mbs_sess_t *mbs_sess)
{
    ogs_tai_t *tai;
    int n = 0;

    if (!mbs_sess->mbs_service_area || !mbs_sess->mbs_service_area->tai_list)
        return 0;
    ogs_list_for_each(mbs_sess->mbs_service_area->tai_list, tai)
        n++;
    return n;
}

/* Parses the N4mb Session Modification Request built for the change of the flows of mbs_sess. */
static ogs_pfcp_message_t *build_and_parse_modification_of(smf_mbs_sess_t *mbs_sess, const smf_n4mb_modification_t *mod)
{
    static ogs_pfcp_message_t *message;
    static ogs_pkbuf_t *pkbuf;
    ogs_pfcp_header_t h;

    if (message) { ogs_pfcp_message_free(message); message = NULL; }
    if (pkbuf) { ogs_pkbuf_free(pkbuf); pkbuf = NULL; }

    pkbuf = smf_n4mb_build_session_modification_request(mbs_sess, mod);
    if (!pkbuf) return NULL;
    memset(&h, 0, sizeof(h));
    h.version = 1;
    h.seid_presence = 1;
    h.type = OGS_PFCP_SESSION_MODIFICATION_REQUEST_TYPE;
    h.length = htobe16(pkbuf->len + OGS_PFCP_HEADER_LEN - 4);
    ogs_pkbuf_push(pkbuf, OGS_PFCP_HEADER_LEN);
    memcpy(pkbuf->data, &h, OGS_PFCP_HEADER_LEN);
    message = ogs_pfcp_parse_msg(pkbuf);
    return message;
}

static ogs_pfcp_message_t *build_and_parse_modification(smf_mbs_sess_t *mbs_sess, const smf_mbs_flow_change_t *flows)
{
    smf_n4mb_modification_t mod;

    memset(&mod, 0, sizeof(mod));
    mod.flows = flows;
    return build_and_parse_modification_of(mbs_sess, &mod);
}

/* TS 29.532 V18.6.0 cl. 5.3.2.3.1: "The following parameters may be modified: ... MBS Service Area and/or; MBS
 * service information." and for a broadcast MBS session "list of MBS FSA IDs", and "the NF service consumer shall
 * include the corresponding "contactPcfInd" attribute within the set of requested modifications."; TS 23.247
 * V18.8.0 cl. 7.1.1.6 step 4: "This may lead to addition of new MBS QoS Flow(s), removal of existing MBS QoS
 * Flow(s) or update of existing MBS QoS Flow(s)." */
static void test_update(void)
{
    smf_mbs_sess_t *mbs_sess = make_session(serv_info_2_text, area_1_text);
    smf_mbs_update_t update;
    const char *detail = NULL;
    smf_mbs_qos_flow_t *flow;
    int pdrs, qfi2;

    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    CHECK(count_flows(mbs_sess) == 2);
    CHECK(flow_of(mbs_sess, "1") && flow_of(mbs_sess, "1")->qfi == 1 && flow_of(mbs_sess, "1")->pdr);
    CHECK(flow_of(mbs_sess, "2") && flow_of(mbs_sess, "2")->qfi == 2 && flow_of(mbs_sess, "2")->five_qi == 5);

    /* the service area grows by one TAI: the area changes, the flows do not */
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/mbsServiceArea/taiList/-",
            "{\"plmnId\":{\"mcc\":\"001\",\"mnc\":\"01\"},\"tac\":\"2\"}") == SMF_MBS_UPDATE_OK);
    CHECK(update.area_in_request && update.area_changed && !update.area_reduced);
    CHECK(!update.flow_change.num_added && !update.flow_change.num_removed && !update.n2_info_changed);
    CHECK(count_tais(mbs_sess) == 2);

    /* the same request again leaves an area that is the same */
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_replace, "/mbsServiceArea/taiList/1/tac",
            "\"2\"") == SMF_MBS_UPDATE_OK);
    CHECK(update.area_in_request && !update.area_changed);

    /* the 5QI of a flow changes: same flow, same rules, other N2 information */
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_replace,
            "/mbsServInfo/mbsMediaComps/2/mbsQoSReq/5qi", "7") == SMF_MBS_UPDATE_OK);
    CHECK(update.service_info_changed && update.flow_change.qos_changed && update.n2_info_changed);
    CHECK(!update.flow_change.num_added && !update.flow_change.num_removed);
    CHECK(flow_of(mbs_sess, "2") && flow_of(mbs_sess, "2")->five_qi == 7 && flow_of(mbs_sess, "2")->qfi == 2);

    /* a media component comes: the lowest free QFI, a PDR, a FAR and a QER */
    pdrs = ogs_list_count(&mbs_sess->pfcp.pdr_list);
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/mbsServInfo/mbsMediaComps/3",
            "{\"mbsMedCompNum\":3,\"mbsQoSReq\":{\"5qi\":9}}") == SMF_MBS_UPDATE_OK);
    CHECK(update.flow_change.num_added == 1 && update.flow_change.num_removed == 0 && update.n2_info_changed);
    CHECK(flow_of(mbs_sess, "3") && flow_of(mbs_sess, "3")->qfi == 3);
    CHECK(ogs_list_count(&mbs_sess->pfcp.pdr_list) == pdrs + 1);
    /* the MB-UPF is told with a Create PDR, FAR and QER of the new flow, as at the establishment */
    {
        ogs_pfcp_message_t *msg = build_and_parse_modification(mbs_sess, &update.flow_change);
        ogs_pfcp_session_modification_request_t *req;

        CHECK(msg != NULL);
        if (msg) {
            req = &msg->pfcp_session_modification_request;
            CHECK(req->create_pdr[0].presence && !req->create_pdr[1].presence);
            CHECK(req->create_far[0].presence && !req->create_far[1].presence);
            CHECK(req->create_qer[0].presence && !req->create_qer[1].presence);
            CHECK(!req->remove_pdr[0].presence && !req->remove_far[0].presence && !req->remove_qer[0].presence);
            CHECK(req->create_qer[0].qos_flow_identifier.presence && req->create_qer[0].qos_flow_identifier.u8 == 3);
            CHECK(req->create_qer[0].qer_indications.presence && req->create_qer[0].qer_indications.len == 1 &&
                    (*(uint8_t *)req->create_qer[0].qer_indications.data & 0x01));
            CHECK(req->create_pdr[0].pdr_id.presence && req->create_pdr[0].pdr_id.u16 == update.flow_change.added[0]->id);
            CHECK(req->create_pdr[0].precedence.presence);
            CHECK(req->create_pdr[0].far_id.presence && req->create_pdr[0].far_id.u32 == req->create_far[0].far_id.u32);
            CHECK(req->create_pdr[0].qer_id.presence && req->create_pdr[0].qer_id.u32 == req->create_qer[0].qer_id.u32);
            CHECK(req->create_far[0].apply_action.presence);
        }
    }
    if (update.flow_change.num_added == 1) {
        ogs_pfcp_pdr_t *pdr = update.flow_change.added[0];
        CHECK(pdr == flow_of(mbs_sess, "3")->pdr && pdr->far && pdr->qer && pdr->qer->qfi == 3);
        CHECK(pdr->precedence > flow_of(mbs_sess, "1")->pdr->precedence);
    }

    /* a media component goes: its rules go with it, the others keep theirs and their QFI */
    qfi2 = flow_of(mbs_sess, "2")->qfi;
    {
        uint16_t pdr_id = flow_of(mbs_sess, "1")->pdr->id;
        uint32_t qer_id = flow_of(mbs_sess, "1")->pdr->qer->id;
        uint32_t far_id = flow_of(mbs_sess, "1")->pdr->far->id;

        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation__remove,
                "/mbsServInfo/mbsMediaComps/1", NULL) == SMF_MBS_UPDATE_OK);
        CHECK(update.flow_change.num_removed == 1 && update.flow_change.num_added == 0);
        CHECK(update.flow_change.removed[0].pdr_id == pdr_id && update.flow_change.removed[0].qer_id == qer_id &&
                update.flow_change.removed[0].far_id == far_id);
        /* and a Remove PDR, FAR and QER of the flow that went, nothing created */
        {
            ogs_pfcp_message_t *msg = build_and_parse_modification(mbs_sess, &update.flow_change);

            CHECK(msg != NULL);
            if (msg) {
                ogs_pfcp_session_modification_request_t *req = &msg->pfcp_session_modification_request;

                CHECK(req->remove_pdr[0].presence && req->remove_pdr[0].pdr_id.u16 == pdr_id && !req->remove_pdr[1].presence);
                CHECK(req->remove_far[0].presence && req->remove_far[0].far_id.u32 == far_id && !req->remove_far[1].presence);
                CHECK(req->remove_qer[0].presence && req->remove_qer[0].qer_id.u32 == qer_id && !req->remove_qer[1].presence);
                CHECK(!req->create_pdr[0].presence && !req->create_far[0].presence && !req->create_qer[0].presence);
            }
        }
    }
    CHECK(count_flows(mbs_sess) == 2);
    CHECK(flow_of(mbs_sess, "1") == NULL && flow_of(mbs_sess, "2")->qfi == qfi2 && flow_of(mbs_sess, "3")->qfi == 3);
    CHECK(ogs_list_count(&mbs_sess->pfcp.pdr_list) == pdrs);
    /* a media component takes the QFI that is free again */
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/mbsServInfo/mbsMediaComps/4",
            "{\"mbsMedCompNum\":4,\"mbsQoSReq\":{\"5qi\":9}}") == SMF_MBS_UPDATE_OK);
    CHECK(flow_of(mbs_sess, "4") && flow_of(mbs_sess, "4")->qfi == 1);
    /* every PDR has its own FAR and QER, none is shared */
    ogs_list_for_each(&mbs_sess->mbs_qos_flow_list, flow) {
        CHECK(flow->pdr && flow->pdr->far && flow->pdr->qer && flow->pdr->qer->qfi == flow->qfi);
    }

    /* an update that does not fit leaves everything as it was: five flows are more than the QERs of a session */
    {
        int before = count_flows(mbs_sess);
        int rules = ogs_list_count(&mbs_sess->pfcp.qer_list);
        char *info_before = ogs_strdup(mbs_sess->mbs_serv_info_json);

        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/mbsServInfo/mbsMediaComps/5",
                "{\"mbsMedCompNum\":5,\"mbsQoSReq\":{\"5qi\":9}}") == SMF_MBS_UPDATE_OK);
        CHECK(count_flows(mbs_sess) == before + 1);
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/mbsServInfo/mbsMediaComps/6",
                "{\"mbsMedCompNum\":6,\"mbsQoSReq\":{\"5qi\":9}}") == SMF_MBS_UPDATE_INSUFFICIENT_RESOURCES);
        CHECK(count_flows(mbs_sess) == before + 1);
        CHECK(ogs_list_count(&mbs_sess->pfcp.qer_list) == rules + 1);
        CHECK(mbs_sess->mbs_serv_info_json && strstr(mbs_sess->mbs_serv_info_json, "\"6\"") == NULL);
        ogs_free(info_before);
    }

    /* what cannot be applied or is erroneous changes nothing */
    {
        int flows = count_flows(mbs_sess), tais = count_tais(mbs_sess);

        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_replace,
                "/mbsServInfo/mbsMediaComps/99/mbsQoSReq/5qi", "7") == SMF_MBS_UPDATE_PATCH_FAILED);
        CHECK(detail != NULL);
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_replace,
                "/mbsServInfo/mbsMediaComps/2/mbsQoSReq/5qi", "300") == SMF_MBS_UPDATE_INVALID_MBS_SERVICE_INFO);
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add,
                "/mbsServInfo/mbsMediaComps/2/mbsFlowDescs",
                "[\"permit out udp from any 1-9 to any 5004\"]") ==
                SMF_MBS_UPDATE_FILTER_RESTRICTIONS_NOT_RESPECTED);
        CHECK(detail && strstr(detail, "source port") != NULL);
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add,
                "/mbsServInfo/mbsMediaComps/2/mbsFlowDescs",
                "[\"permit out udp from any to any 5004 frag\"]") ==
                SMF_MBS_UPDATE_FILTER_RESTRICTIONS_NOT_RESPECTED);
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add,
                "/mbsServInfo/mbsMediaComps/2/mbsFlowDescs", "[\"nonsense\"]") ==
                SMF_MBS_UPDATE_INVALID_MBS_SERVICE_INFO);
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_replace, "/mbsServiceArea",
                "{\"taiList\":[{\"tac\":\"1\"}]}") == SMF_MBS_UPDATE_ERROR_INPUT_PARAMETERS);
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/mbsFsaIdList",
                "[\"xyz\"]") == SMF_MBS_UPDATE_ERROR_INPUT_PARAMETERS);
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/contactPcfInd",
                "\"yes\"") == SMF_MBS_UPDATE_ERROR_INPUT_PARAMETERS);
        CHECK(count_flows(mbs_sess) == flows && count_tais(mbs_sess) == tais);
        CHECK(flow_of(mbs_sess, "2")->five_qi == 7);
        CHECK(ogs_list_empty(&mbs_sess->mbs_fsa_id_list) && !mbs_sess->has_contact_pcf_ind);
    }

    /* a valid flow description is accepted */
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add,
            "/mbsServInfo/mbsMediaComps/2/mbsFlowDescs", "[\"permit out udp from any to 224.0.2.1 5004\"]") ==
            SMF_MBS_UPDATE_OK);

    /* the FSA IDs and contactPcfInd */
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/mbsFsaIdList",
            "[\"00abcd\",\"000007\"]") == SMF_MBS_UPDATE_OK);
    CHECK(update.fsa_changed && update.n2_info_changed && ogs_list_count(&mbs_sess->mbs_fsa_id_list) == 2);
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_replace, "/mbsFsaIdList",
            "[\"00ABCD\",\"000007\"]") == SMF_MBS_UPDATE_OK);
    CHECK(!update.fsa_changed);
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/mbsFsaIdList/-",
            "\"000008\"") == SMF_MBS_UPDATE_OK);
    CHECK(update.fsa_changed && ogs_list_count(&mbs_sess->mbs_fsa_id_list) == 3);
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation__remove, "/mbsFsaIdList",
            NULL) == SMF_MBS_UPDATE_OK);
    CHECK(update.fsa_changed && ogs_list_empty(&mbs_sess->mbs_fsa_id_list));
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/contactPcfInd",
            "true") == SMF_MBS_UPDATE_OK);
    CHECK(update.contact_pcf_ind_changed && !update.n2_info_changed && mbs_sess->has_contact_pcf_ind &&
            mbs_sess->contact_pcf_ind);
    CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_replace, "/contactPcfInd",
            "true") == SMF_MBS_UPDATE_OK);
    CHECK(!update.contact_pcf_ind_changed);

    smf_mbs_sess_release(mbs_sess);
}

/* TS 29.532 V18.6.0 cl. 5.3.2.3.1 step 2b: "If the MBS service area received in the request cannot be entirely
 * covered by the MB-SMF service area, the MB-SMF shall reduce the MBS service area to be within the MB-SMF service
 * area and continue the Update service operation using the reduced MBS service area." */
static void test_update_reduced_area(void)
{
    ogs_mbs_service_area_t *saved = smf_self()->mbs_service_area;
    ogs_mbs_service_area_t *coverage = area_of_tacs("1", "2");
    smf_mbs_sess_t *mbs_sess;
    smf_mbs_update_t update;
    const char *detail = NULL;

    smf_self()->mbs_service_area = coverage;
    mbs_sess = make_session(NULL, area_1_text);
    CHECK(mbs_sess != NULL);
    if (mbs_sess) {
        /* 1 and 9 requested, 1 and 2 covered: the session runs with 1 and the response reports it */
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_replace, "/mbsServiceArea",
                "{\"taiList\":[{\"plmnId\":{\"mcc\":\"001\",\"mnc\":\"01\"},\"tac\":\"1\"},"
                "{\"plmnId\":{\"mcc\":\"001\",\"mnc\":\"01\"},\"tac\":\"9\"}]}") == SMF_MBS_UPDATE_OK);
        CHECK(update.area_in_request && update.area_changed && update.area_reduced);
        CHECK(mbs_sess->service_area_reduced && count_tais(mbs_sess) == 1);
        CHECK(mbs_sess->mbs_service_area_json && strstr(mbs_sess->mbs_service_area_json, "\"9\"") != NULL);

        /* a request that does not touch the area does not report one, and keeps the reduced area */
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_add, "/contactPcfInd",
                "false") == SMF_MBS_UPDATE_OK);
        CHECK(!update.area_in_request && !update.area_reduced && !update.area_changed);
        CHECK(count_tais(mbs_sess) == 1);

        /* a covered area is not reduced */
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation_replace, "/mbsServiceArea",
                "{\"taiList\":[{\"plmnId\":{\"mcc\":\"001\",\"mnc\":\"01\"},\"tac\":\"2\"}]}") == SMF_MBS_UPDATE_OK);
        CHECK(update.area_changed && !update.area_reduced && !mbs_sess->service_area_reduced);
        CHECK(count_tais(mbs_sess) == 1);

        /* no area any more */
        CHECK(update_with(mbs_sess, &update, &detail, OpenAPI_patch_operation__remove, "/mbsServiceArea",
                NULL) == SMF_MBS_UPDATE_OK);
        CHECK(update.area_changed && mbs_sess->mbs_service_area == NULL && mbs_sess->mbs_service_area_json == NULL);
        smf_mbs_sess_release(mbs_sess);
    }
    smf_self()->mbs_service_area = saved;
    ogs_mbs_service_area_free(coverage);
}

/* TS 23.247 V18.8.0 cl. 7.3.2 step 3: "MB-SMF sends Namf_MBSBroadcast_ContextRelease request (TMGI) to the AMF(s)
 * that has been involved in the MBS Session."; the notification URI of a ContextCreate ends with the id of the
 * AMF entry, so that a ContextStatusNotify tells which AMF sent it. */
static void test_amf_entries(void)
{
    smf_mbs_sess_t *mbs_sess = make_session(NULL, area_1_text);
    smf_mbs_amf_t *amf1, *amf2;
    ogs_sbi_request_t *request;
    char expected[64];

    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    amf1 = smf_mbs_sess_amf_add(mbs_sess);
    amf2 = smf_mbs_sess_amf_add(mbs_sess);

    request = smf_namf_build_mbs_broadcast_context_create_request(mbs_sess, amf2);
    CHECK(request != NULL);
    if (request) {
        snprintf(expected, sizeof(expected), "/mbs-context-status-notify/%u\"", amf2->id);
        CHECK(request->http.content && strstr(request->http.content, expected) != NULL);
        ogs_sbi_request_free(request);
    }

    /* the request that releases one AMF's context addresses that context */
    amf1->mbs_context_ref = ogs_strdup("11");
    amf2->mbs_context_ref = ogs_strdup("22");
    request = smf_namf_build_mbs_broadcast_context_delete_request(mbs_sess, amf2);
    CHECK(request != NULL);
    if (request) {
        CHECK(request->h.method && strcmp(request->h.method, OGS_SBI_HTTP_METHOD_DELETE) == 0);
        CHECK(request->h.resource.component[0] && strcmp(request->h.resource.component[0], "mbs-contexts") == 0);
        CHECK(request->h.resource.component[1] && strcmp(request->h.resource.component[1], "22") == 0);
        ogs_sbi_request_free(request);
    }

    smf_mbs_sess_release(mbs_sess);     /* the entries go with the session */
}

/* TS 29.518 V18.14.0 cl. 5.6.2.3: the ContextUpdate "shall send a POST request targeting the individual Broadcast
 * MBS session context resource", with "N2 MBS Session Management container", "updated MBS service area, for a
 * Local broadcast MBS session; updated list of Area Session ID and related MBS service area, for a Location
 * dependent broadcast MBS session" and "the n2MbsInfoChangeInd IE set to "true" or "false""; the resource is
 * mbs-contexts/{mbsContextRef}/update (table 6.5.3.2.4.1-1, POST). */
static void test_context_update_request(void)
{
    smf_mbs_sess_t *mbs_sess = make_session(serv_info_2_text, area_1_text);
    smf_mbs_amf_t *amf;
    smf_namf_context_update_param_t param;
    ogs_sbi_request_t *request;
    cJSON *json;
    const char *content;

    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    amf = smf_mbs_sess_amf_add(mbs_sess);
    amf->mbs_context_ref = ogs_strdup("5");

    memset(&param, 0, sizeof(param));
    param.amf = amf;
    param.service_area = true;
    param.n2_info_changed = false;
    request = smf_namf_build_mbs_broadcast_context_update_request(mbs_sess, &param);
    CHECK(request != NULL);
    if (request) {
        CHECK(request->h.method && strcmp(request->h.method, OGS_SBI_HTTP_METHOD_POST) == 0);
        CHECK(request->h.service.name && strcmp(request->h.service.name, "namf-mbs-bc") == 0);
        CHECK(request->h.resource.component[0] && strcmp(request->h.resource.component[0], "mbs-contexts") == 0);
        CHECK(request->h.resource.component[1] && strcmp(request->h.resource.component[1], "5") == 0);
        CHECK(request->h.resource.component[2] && strcmp(request->h.resource.component[2], "update") == 0);
        content = request->http.content;
        CHECK(content != NULL);
        if (content) {
            /* the JSON part, then the N2 container as the part n2MbsSmInfo refers to */
            const char *brace = strchr(content, '{');
            const char *end = brace ? strstr(brace, "\r\n--") : NULL;
            char *text = (brace && end) ? ogs_strndup(brace, end - brace) : NULL;

            CHECK(text != NULL);
            json = text ? cJSON_Parse(text) : NULL;
            CHECK(json != NULL);
            if (json) {
                CHECK(cJSON_GetObjectItemCaseSensitive(json, "mbsServiceArea") != NULL);
                CHECK(cJSON_GetObjectItemCaseSensitive(json, "mbsServiceAreaInfoList") == NULL);
                CHECK(cJSON_GetObjectItemCaseSensitive(json, "n2MbsSmInfo") != NULL);
                CHECK(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(json, "n2MbsInfoChangeInd")));
                CHECK(cJSON_GetObjectItemCaseSensitive(json, "notifyUri") == NULL);
                cJSON_Delete(json);
            }
            if (text) ogs_free(text);
            CHECK(strstr(content, "Content-Id: ngap-sm") != NULL || strstr(content, "Content-ID: ngap-sm") != NULL);
        }
        ogs_sbi_request_free(request);
    }

    /* the area is left out when it did not change, the change indication is true */
    param.service_area = false;
    param.n2_info_changed = true;
    request = smf_namf_build_mbs_broadcast_context_update_request(mbs_sess, &param);
    CHECK(request != NULL);
    if (request) {
        CHECK(request->http.content && strstr(request->http.content, "mbsServiceArea") == NULL);
        CHECK(request->http.content && strstr(request->http.content, "\"n2MbsInfoChangeInd\":true") != NULL);
        ogs_sbi_request_free(request);
    }

    /* a location dependent session sends the list of Area Session ID and area */
    mbs_sess->location_dependent = true;
    mbs_sess->area_session_id = 3;
    param.service_area = true;
    request = smf_namf_build_mbs_broadcast_context_update_request(mbs_sess, &param);
    CHECK(request != NULL);
    if (request) {
        CHECK(request->http.content && strstr(request->http.content, "\"mbsServiceAreaInfoList\"") != NULL);
        CHECK(request->http.content && strstr(request->http.content, "\"areaSessionId\":3") != NULL);
        ogs_sbi_request_free(request);
    }

    /* TS 29.518 cl. 5.6.2.3 steps 2a and 2b: "200 OK" and "204 No Content" are both success */
    {
        ogs_sbi_message_t answer;

        memset(&answer, 0, sizeof(answer));
        answer.res_status = OGS_SBI_HTTP_STATUS_NO_CONTENT;
        CHECK(smf_namf_handle_mbs_broadcast_context_update_response(mbs_sess, amf, &answer));
        answer.res_status = OGS_SBI_HTTP_STATUS_OK;
        CHECK(smf_namf_handle_mbs_broadcast_context_update_response(mbs_sess, amf, &answer));
        answer.res_status = OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR;
        CHECK(!smf_namf_handle_mbs_broadcast_context_update_response(mbs_sess, amf, &answer));
        /* a failed update leaves the context in place */
        CHECK(smf_mbs_sess_amf_find(mbs_sess, amf->id) == amf && amf->mbs_context_ref != NULL);
    }

    smf_mbs_sess_release(mbs_sess);
}

/* TS 29.244 V18.10.0 cl. 8.2.209 Local Ingress Tunnel: with the CH bit set the UDP port and the address fields are
 * not present; the spare bits of octet 5 are "0": octet 5 of a CHOOSE for IPv4 is 0x05, for IPv6 0x06. */
static void test_local_ingress_tunnel_choose(void)
{
    ogs_pfcp_local_ingress_tunnel_t tunnel;
    int len = 0;

    memset(&tunnel, 0xff, sizeof(tunnel));
    CHECK(ogs_pfcp_proto_choice_to_local_ingress_tunnel(true, false, &tunnel, &len) == OGS_OK);
    CHECK(len == 1 && tunnel.flags == 0x05);

    memset(&tunnel, 0xff, sizeof(tunnel));
    CHECK(ogs_pfcp_proto_choice_to_local_ingress_tunnel(false, true, &tunnel, &len) == OGS_OK);
    CHECK(len == 1 && tunnel.flags == 0x06);
}

static smf_mbs_sess_t *session_with_ssm(const char *dest, const char *src)
{
    ogs_tmgi_t *tmgi = smf_tmgi_allocate(smf_tmgi_gen_expiration_time(60));
    ogs_ssm_t *ssm = ogs_calloc(1, sizeof(*ssm));
    smf_mbs_sess_t *mbs_sess;
    struct in_addr a4;
    struct in6_addr a6;

    if (!tmgi) return NULL;
    if (inet_pton(AF_INET, dest, &a4) == 1) {
        ssm->dest_ip_addr.ipv4 = 1;
        ssm->dest_ip_addr.len = OGS_IPV4_LEN;
        ssm->dest_ip_addr.addr = a4.s_addr;
    } else {
        inet_pton(AF_INET6, dest, &a6);
        ssm->dest_ip_addr.ipv6 = 1;
        ssm->dest_ip_addr.len = OGS_IPV6_LEN;
        memcpy(ssm->dest_ip_addr.addr6, &a6, OGS_IPV6_LEN);
    }
    if (inet_pton(AF_INET, src, &a4) == 1) {
        ssm->src_ip_addr.ipv4 = 1;
        ssm->src_ip_addr.len = OGS_IPV4_LEN;
        ssm->src_ip_addr.addr = a4.s_addr;
    } else {
        inet_pton(AF_INET6, src, &a6);
        ssm->src_ip_addr.ipv6 = 1;
        ssm->src_ip_addr.len = OGS_IPV6_LEN;
        memcpy(ssm->src_ip_addr.addr6, &a6, OGS_IPV6_LEN);
    }
    mbs_sess = smf_mbs_sess_create(tmgi, true, ssm, "BROADCAST", NULL, NULL, false, NULL);
    if (!mbs_sess) return NULL;
    ogs_list_init(&mbs_sess->mbs_qos_flow_list);
    ogs_list_init(&mbs_sess->mbs_fsa_id_list);
    {
        ogs_pfcp_node_t *node = NULL;
        ogs_list_for_each(&ogs_pfcp_self()->pfcp_peer_list, node)
            memcpy(&node->addr, node->sa_list, sizeof(node->addr));
    }
    smf_mbs_sess_create_mbs_data_forwarding(mbs_sess);
    return mbs_sess;
}

/* TS 29.244 V18.10.0 figures 8.2.137-1 (IP Multicast Address) and 8.2.138-1 (Source IP Address): octet 5 holds the
 * flags (IP Multicast Address: V6 bit 1, V4 bit 2; Source IP Address: V6 bit 1, V4 bit 2, MPL bit 3), and the
 * address fields follow it with no gap; an IPv6 address is 16 octets starting at octet 6. */
static void test_ip_multicast_addressing_info_layout(void)
{
    static const uint8_t v6_group[16] = { 0xff, 0x3e, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x12, 0x34 };
    static const uint8_t v6_source[16] = { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01 };
    smf_mbs_sess_t *mbs_sess;
    ogs_pfcp_message_t *msg;
    ogs_tlv_octet_t *group, *source;

    /* IPv6 group, IPv6 source */
    mbs_sess = session_with_ssm("ff3e::1234", "2001:db8::1");
    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    msg = build_and_parse_establishment(mbs_sess);
    CHECK(msg != NULL);
    if (msg) {
        group = &msg->pfcp_session_establishment_request.create_pdr[0].pdi.ip_multicast_addressing_info.ip_multicast_address;
        source = &msg->pfcp_session_establishment_request.create_pdr[0].pdi.ip_multicast_addressing_info.source_ip_address;
        CHECK(group->presence && group->len == 17);
        CHECK(group->len == 17 && ((uint8_t *)group->data)[0] == 0x01 && memcmp((uint8_t *)group->data + 1, v6_group, 16) == 0);
        CHECK(source->presence && source->len == 17);
        CHECK(source->len == 17 && ((uint8_t *)source->data)[0] == 0x01 && memcmp((uint8_t *)source->data + 1, v6_source, 16) == 0);
    }
    smf_mbs_sess_release(mbs_sess);

    /* IPv4 group, IPv4 source */
    mbs_sess = session_with_ssm("232.1.2.3", "10.0.0.9");
    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    msg = build_and_parse_establishment(mbs_sess);
    CHECK(msg != NULL);
    if (msg) {
        static const uint8_t g4[4] = { 232, 1, 2, 3 }, s4[4] = { 10, 0, 0, 9 };

        group = &msg->pfcp_session_establishment_request.create_pdr[0].pdi.ip_multicast_addressing_info.ip_multicast_address;
        source = &msg->pfcp_session_establishment_request.create_pdr[0].pdi.ip_multicast_addressing_info.source_ip_address;
        CHECK(group->len == 5 && ((uint8_t *)group->data)[0] == 0x02 && memcmp((uint8_t *)group->data + 1, g4, 4) == 0);
        CHECK(source->len == 5 && ((uint8_t *)source->data)[0] == 0x02 && memcmp((uint8_t *)source->data + 1, s4, 4) == 0);
    }
    smf_mbs_sess_release(mbs_sess);

    /* the encoders on their own: a range, any, a prefix length, and flags that are not valid */
    {
        ogs_pfcp_ip_multicast_address_t a;
        ogs_pfcp_source_ip_address_t b;
        uint8_t buf[OGS_PFCP_IP_MULTICAST_ADDRESS_MAX_LEN];
        uint8_t sbuf[OGS_PFCP_SOURCE_IP_ADDRESS_MAX_LEN];

        memset(&a, 0, sizeof(a));
        a.a = 1;
        CHECK(ogs_pfcp_build_ip_multicast_address(buf, &a) == 1 && buf[0] == 0x08);

        memset(&a, 0, sizeof(a));
        a.v4 = 1;
        a.r = 1;
        a.s_ipv4_addr = htonl(0xe8010203);
        a.e_ipv4_addr = htonl(0xe80102ff);
        CHECK(ogs_pfcp_build_ip_multicast_address(buf, &a) == 9 && buf[0] == 0x06 && buf[4] == 0x03 && buf[8] == 0xff);

        memset(&a, 0, sizeof(a));
        a.v6 = 1;
        a.r = 1;
        memcpy(a.s_ipv6_addr, v6_group, 16);
        memcpy(a.e_ipv6_addr, v6_source, 16);
        CHECK(ogs_pfcp_build_ip_multicast_address(buf, &a) == 33 && buf[0] == 0x05 &&
                memcmp(buf + 1, v6_group, 16) == 0 && memcmp(buf + 17, v6_source, 16) == 0);

        memset(&a, 0, sizeof(a));
        CHECK(ogs_pfcp_build_ip_multicast_address(buf, &a) == -1);      /* no address kind */
        a.v4 = 1;
        a.v6 = 1;
        CHECK(ogs_pfcp_build_ip_multicast_address(buf, &a) == -1);      /* two */

        memset(&b, 0, sizeof(b));
        b.v4 = 1;
        b.v6 = 1;
        b.mpl = 1;
        b.ipv4_addr = htonl(0x0a000009);
        memcpy(b.ipv6_addr, v6_source, 16);
        b.mask_prefix_len = 64;
        CHECK(ogs_pfcp_build_source_ip_address(sbuf, &b) == 22 && sbuf[0] == 0x07 && sbuf[4] == 0x09 &&
                memcmp(sbuf + 5, v6_source, 16) == 0 && sbuf[21] == 64);
        memset(&b, 0, sizeof(b));
        CHECK(ogs_pfcp_build_source_ip_address(sbuf, &b) == -1);
    }
}

static void free_event_list(OpenAPI_list_t *list)
{
    OpenAPI_lnode_t *node;

    OpenAPI_list_for_each(list, node)
        OpenAPI_mbs_session_event_free(node->data);
    OpenAPI_list_free(list);
}

/* TS 29.532 V18.6.0 cl. 5.3.2.6.2 / 5.3.2.8.1 and TS 29.571 V18.12.0 5.9.4.7 to 5.9.4.10: a status subscription of
 * an MBS session, the events it can be about, and the StatusNotify that reports one. */
static void test_status_subscription(void)
{
    smf_mbs_sess_t *mbs_sess = make_session(NULL, area_1_text);
    smf_mbs_sess_t *other = make_session(NULL, area_1_text);
    smf_mbs_subsc_t *subsc, *dead, *gone;
    OpenAPI_list_t *list;
    bool unsupported;
    uint32_t events;
    ogs_sbi_request_t *request;
    cJSON *json = NULL;
    int before = ogs_list_count(&smf_self()->smf_mbs_subsc_list);

    CHECK(mbs_sess != NULL && other != NULL);
    if (!mbs_sess || !other) return;

    /* the events: the two this MB-SMF reports; MBS_REL_TMGI_EXPIRY is not reported */
    list = OpenAPI_list_create();
    OpenAPI_list_add(list, OpenAPI_mbs_session_event_create(OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS));
    events = smf_mbs_subsc_events_from_list(list, &unsupported);
    CHECK(events == SMF_MBS_EVENT(OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS) && !unsupported);
    OpenAPI_list_add(list, OpenAPI_mbs_session_event_create(OpenAPI_mbs_session_event_type_MBS_REL_TMGI_EXPIRY));
    events = smf_mbs_subsc_events_from_list(list, &unsupported);
    CHECK(events == SMF_MBS_EVENT(OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS) && unsupported);
    {
        OpenAPI_list_t *only_expiry = OpenAPI_list_create();

        OpenAPI_list_add(only_expiry, OpenAPI_mbs_session_event_create(OpenAPI_mbs_session_event_type_MBS_REL_TMGI_EXPIRY));
        CHECK(smf_mbs_subsc_events_from_list(only_expiry, &unsupported) == 0 && unsupported);
        free_event_list(only_expiry);
    }
    CHECK(smf_mbs_subsc_events_from_list(NULL, &unsupported) == 0);
    free_event_list(list);

    /* a subscription, found by its id, with its notification URI */
    subsc = smf_mbs_subsc_add(mbs_sess, SMF_MBS_EVENT(OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS),
            "http://127.0.91.50:9000/notify", "corr-1", NULL, NULL);
    CHECK(subsc != NULL);
    if (!subsc) return;
    CHECK(smf_mbs_subsc_find_by_id(subsc->id) == subsc && smf_mbs_subsc_find_by_id("nothing") == NULL);
    CHECK(subsc->client != NULL && subsc->notify_uri && strcmp(subsc->notify_uri, "http://127.0.91.50:9000/notify") == 0);
    CHECK(!smf_mbs_subsc_is_expired(subsc));
    CHECK(smf_mbs_subsc_add(mbs_sess, SMF_MBS_EVENT(1), "not a uri", NULL, NULL, NULL) == NULL);

    /* the expiry: past it the subscription is invalid */
    dead = smf_mbs_subsc_add(mbs_sess, SMF_MBS_EVENT(OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS),
            "http://127.0.91.50:9000/notify", NULL, "2020-01-01T00:00:00Z", NULL);
    CHECK(dead != NULL && smf_mbs_subsc_is_expired(dead));
    gone = smf_mbs_subsc_add(other, SMF_MBS_EVENT(OpenAPI_mbs_session_event_type_INGRESS_TUNNEL_ADD_CHANGE),
            "http://127.0.91.50:9000/other", NULL, "2099-01-01T00:00:00Z", NULL);
    CHECK(gone != NULL && !smf_mbs_subsc_is_expired(gone));

    /* the StatusNotify of a delivery status event: the report, the status, the correlation id */
    request = smf_mbs_subsc_build_status_notify(subsc, mbs_sess,
            OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS, OpenAPI_broadcast_delivery_status_STARTED);
    CHECK(request != NULL);
    if (request) {
        CHECK(request->h.method && strcmp(request->h.method, OGS_SBI_HTTP_METHOD_POST) == 0);
        CHECK(request->h.uri && strcmp(request->h.uri, "http://127.0.91.50:9000/notify") == 0);
        json = request->http.content ? cJSON_Parse(request->http.content) : NULL;
        CHECK(json != NULL);
        if (json) {
            cJSON *list_json = cJSON_GetObjectItemCaseSensitive(json, "eventList");
            cJSON *reports = list_json ? cJSON_GetObjectItemCaseSensitive(list_json, "eventReportList") : NULL;
            cJSON *report = reports ? cJSON_GetArrayItem(reports, 0) : NULL;

            CHECK(list_json && cJSON_GetObjectItemCaseSensitive(list_json, "notifyCorrelationId") &&
                    strcmp(cJSON_GetObjectItemCaseSensitive(list_json, "notifyCorrelationId")->valuestring, "corr-1") == 0);
            CHECK(report && cJSON_GetArraySize(reports) == 1);
            if (report) {
                cJSON *type = cJSON_GetObjectItemCaseSensitive(report, "eventType");
                cJSON *status = cJSON_GetObjectItemCaseSensitive(report, "broadcastDelStatus");

                CHECK(type && strcmp(type->valuestring, "BROADCAST_DELIVERY_STATUS") == 0);
                CHECK(status && strcmp(status->valuestring, "STARTED") == 0);
                CHECK(cJSON_GetObjectItemCaseSensitive(report, "timeStamp") != NULL);
                CHECK(cJSON_GetObjectItemCaseSensitive(report, "ingressTunAddrInfo") == NULL);
            }
            cJSON_Delete(json);
        }
        ogs_sbi_request_free(request);
    }

    /* an event that was not subscribed, an expired subscription: no request */
    CHECK(smf_mbs_subsc_build_status_notify(subsc, mbs_sess,
            OpenAPI_mbs_session_event_type_INGRESS_TUNNEL_ADD_CHANGE, OpenAPI_broadcast_delivery_status_NULL) == NULL);
    CHECK(smf_mbs_subsc_build_status_notify(dead, mbs_sess,
            OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS, OpenAPI_broadcast_delivery_status_STARTED) == NULL);

    /* the ingress tunnel event carries the address; with no address there is nothing to report */
    CHECK(smf_mbs_subsc_build_status_notify(gone, other,
            OpenAPI_mbs_session_event_type_INGRESS_TUNNEL_ADD_CHANGE, OpenAPI_broadcast_delivery_status_NULL) == NULL);
    ogs_addaddrinfo(&other->ingress_tun_addr, AF_INET, "127.0.91.7", 50000, 0);
    request = smf_mbs_subsc_build_status_notify(gone, other,
            OpenAPI_mbs_session_event_type_INGRESS_TUNNEL_ADD_CHANGE, OpenAPI_broadcast_delivery_status_NULL);
    CHECK(request != NULL);
    if (request) {
        CHECK(request->http.content && strstr(request->http.content, "\"INGRESS_TUNNEL_ADD_CHANGE\"") != NULL);
        CHECK(request->http.content && strstr(request->http.content, "\"ipv4Addr\":\"127.0.91.7\"") != NULL);
        CHECK(request->http.content && strstr(request->http.content, "\"portNumber\":50000") != NULL);
        CHECK(request->http.content && strstr(request->http.content, "broadcastDelStatus") == NULL);
        ogs_sbi_request_free(request);
    }

    /* the answer that describes a subscription: the session, the events, the expiry; the write-only attributes are not
     * in it, mbsSessionSubscUri only when asked for */
    {
        OpenAPI_mbs_session_subscription_t *answer = smf_mbs_subsc_build(gone, other, "http://x/subscriptions/9");
        cJSON *j = answer ? OpenAPI_mbs_session_subscription_convertToJSON(answer) : NULL;

        CHECK(answer != NULL && j != NULL);
        if (j) {
            CHECK(cJSON_GetObjectItemCaseSensitive(j, "mbsSessionId") != NULL);
            CHECK(cJSON_GetObjectItemCaseSensitive(j, "eventList") != NULL);
            CHECK(cJSON_GetObjectItemCaseSensitive(j, "expiryTime") != NULL);
            CHECK(cJSON_GetObjectItemCaseSensitive(j, "notifyUri") == NULL);
            CHECK(cJSON_GetObjectItemCaseSensitive(j, "mbsSessionSubscUri") != NULL);
            cJSON_Delete(j);
        }
        if (answer) OpenAPI_mbs_session_subscription_free(answer);
    }

    /* the subscriptions of a session end with it, the others stay */
    smf_mbs_sess_release(mbs_sess);
    CHECK(smf_mbs_subsc_find_by_id(subsc->id) == NULL || 1);   /* subsc is freed: not dereferenced */
    CHECK(ogs_list_count(&smf_self()->smf_mbs_subsc_list) == before + 1);
    smf_mbs_sess_release(other);
    CHECK(ogs_list_count(&smf_self()->smf_mbs_subsc_list) == before);
}

/* The container of an NG-RAN: MBS Session Setup or Modification Response Transfer with a shared unicast tunnel
 * (TS 38.413 V18.8.0 clause 9.3.5.5, "MBS Session TNL Information NG-RAN | O"), or without; and the Failure
 * Transfer (clause 9.3.5.6, "Cause | M"). */
static ogs_pkbuf_t *encode_response_transfer(bool with_tunnel, const char *addr, uint32_t teid)
{
    NGAP_MBSSessionSetupOrModResponseTransfer_t transfer;
    ogs_pkbuf_t *pkbuf;

    memset(&transfer, 0, sizeof(transfer));
    if (with_tunnel) {
        NGAP_MBS_SessionTNLInfoNGRAN_t *tnl = CALLOC(1, sizeof(*tnl));
        NGAP_UPTransportLayerInformation_t *up = CALLOC(1, sizeof(*up));
        NGAP_GTPTunnel_t *gtp = CALLOC(1, sizeof(*gtp));
        ogs_ip_t ip;
        struct in_addr a4;

        memset(&ip, 0, sizeof(ip));
        inet_pton(AF_INET, addr, &a4);
        ip.ipv4 = 1;
        ip.addr = a4.s_addr;
        ip.len = OGS_IPV4_LEN;
        ogs_asn_ip_to_BIT_STRING(&ip, &gtp->transportLayerAddress);
        ogs_asn_uint32_to_OCTET_STRING(teid, &gtp->gTP_TEID);
        up->present = NGAP_UPTransportLayerInformation_PR_gTPTunnel;
        up->choice.gTPTunnel = gtp;
        tnl->present = NGAP_MBS_SessionTNLInfoNGRAN_PR_locationindependent;
        tnl->choice.locationindependent = up;
        transfer.mBS_SessionTNLInfoNGRAN = tnl;
    }
    /* the encoder frees what it encoded */
    pkbuf = ogs_asn_encode(&asn_DEF_NGAP_MBSSessionSetupOrModResponseTransfer, &transfer);
    return pkbuf;
}

static ogs_pkbuf_t *encode_failure_transfer(long radio_network_cause)
{
    NGAP_MBSSessionSetupOrModFailureTransfer_t transfer;
    ogs_pkbuf_t *pkbuf;

    memset(&transfer, 0, sizeof(transfer));
    transfer.cause.present = NGAP_Cause_PR_radioNetwork;
    transfer.cause.choice.radioNetwork = radio_network_cause;
    pkbuf = ogs_asn_encode(&asn_DEF_NGAP_MBSSessionSetupOrModFailureTransfer, &transfer);
    return pkbuf;
}

static const char *const ran_a_text = "{\"plmnId\":{\"mcc\":\"001\",\"mnc\":\"01\"},\"gNbId\":{\"bitLength\":24,\"gNBValue\":\"000001\"}}";

static OpenAPI_n2_mbs_sm_info_t *n2_info(OpenAPI_ngap_ie_type_e type, const char *content_id, const char *ran_text)
{
    cJSON *json = ran_text ? cJSON_Parse(ran_text) : NULL;
    OpenAPI_global_ran_node_id_t *ran = json ? OpenAPI_global_ran_node_id_parseFromJSON(json) : NULL;

    if (json) cJSON_Delete(json);
    return OpenAPI_n2_mbs_sm_info_create(type,
            content_id ? OpenAPI_ref_to_binary_data_create(ogs_strdup((char *)content_id)) : NULL, ran);
}

/* TS 29.518 V18.14.0 cl. 5.6.2.2 and table 6.5.6.2.3-1; TS 23.247 V18.8.0 cl. 7.3.1 step 7: "The MB-SMF stores the
 * AMF(s) which responds success in the MBS Session Context as the downstream nodes." and, for unicast N3mb transport,
 * the DL tunnel with its NG-RAN ID; cl. 7.3.5: the session has started once every AMF completed the operation. */
static void test_amf_answer(void)
{
    smf_mbs_sess_t *mbs_sess = make_session(NULL, area_1_text);
    smf_mbs_amf_t *amf1, *amf2;
    ogs_sbi_message_t answer;
    OpenAPI_list_t *list;
    OpenAPI_lnode_t *node;
    smf_mbs_dl_tunnel_t *tunnel;
    ogs_pkbuf_t *rsp_a, *rsp_none, *fail;
    int changes;
    char *ran_a;
    OpenAPI_global_ran_node_id_t *ran;
    smf_mbs_tunnel_changes_t ch;

    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    amf1 = smf_mbs_sess_amf_add(mbs_sess);
    amf2 = smf_mbs_sess_amf_add(mbs_sess);
    amf1->mbs_context_ref = ogs_strdup("1");
    amf2->mbs_context_ref = ogs_strdup("2");

    /* completion: the session has started when every AMF with a context completed the start */
    memset(&answer, 0, sizeof(answer));
    CHECK(smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_MBS_SESSION_START_INCOMPLETE, NULL,
            &answer, NULL) == 0);
    CHECK(!amf1->start_complete && !mbs_sess->started);
    smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_MBS_SESSION_START_COMPLETE, NULL, &answer, NULL);
    CHECK(amf1->start_complete && !amf2->start_complete && !mbs_sess->started);
    smf_mbs_sess_take_amf_answer(mbs_sess, NULL, OpenAPI_operation_status_MBS_SESSION_START_COMPLETE, NULL, &answer, NULL);
    CHECK(!mbs_sess->started);      /* a notification of an unknown AMF completes nothing */
    smf_mbs_sess_take_amf_answer(mbs_sess, amf2, OpenAPI_operation_status_MBS_SESSION_START_COMPLETE, NULL, &answer, NULL);
    CHECK(amf2->start_complete && mbs_sess->started);

    /* an NG-RAN's unicast tunnel, from the container of its response */
    rsp_a = encode_response_transfer(true, "10.20.30.40", 0xabcd0001);
    rsp_none = encode_response_transfer(false, NULL, 0);
    fail = encode_failure_transfer(NGAP_CauseRadioNetwork_unspecified);
    CHECK(rsp_a && rsp_none && fail);
    if (!rsp_a || !rsp_none || !fail) return;
    {
        bool has_tunnel = false;
        ogs_ip_t addr;
        uint32_t teid = 0;
        int group = 0;
        long value = 0;

        CHECK(smf_mbs_n3mb_parse_response_transfer(rsp_a, false, 0, &has_tunnel, &addr, &teid) && has_tunnel &&
                addr.ipv4 && teid == 0xabcd0001);
        CHECK(smf_mbs_n3mb_parse_response_transfer(rsp_none, false, 0, &has_tunnel, &addr, &teid) && !has_tunnel);
        CHECK(smf_mbs_n3mb_parse_failure_transfer(fail, &group, &value) && group == NGAP_Cause_PR_radioNetwork &&
                value == NGAP_CauseRadioNetwork_unspecified);
    }

    memset(&answer, 0, sizeof(answer));
    answer.part[0].pkbuf = rsp_a;
    answer.part[0].content_id = (char *)"n2-a";
    answer.part[1].pkbuf = rsp_none;
    answer.part[1].content_id = (char *)"n2-none";
    answer.part[2].pkbuf = fail;
    answer.part[2].content_id = (char *)"n2-fail";
    answer.num_of_part = 3;

    ran = OpenAPI_global_ran_node_id_parseFromJSON(cJSON_Parse(ran_a_text));
    ran_a = smf_mbs_ran_id_text(ran);
    OpenAPI_global_ran_node_id_free(ran);

    list = OpenAPI_list_create();
    OpenAPI_list_add(list, n2_info(OpenAPI_ngap_ie_type_MBS_SES_RSP, "n2-a", ran_a_text));
    changes = smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_NULL, list, &answer, &ch);
    CHECK(changes == 1);
    tunnel = smf_mbs_sess_dl_tunnel_find(mbs_sess, ran_a);
    CHECK(tunnel != NULL && tunnel->teid == 0xabcd0001 && tunnel->addr.ipv4 && tunnel->unicast_params_id == 1);
    /* the MB-UPF is to be given it, under the tunnel's ID */
    CHECK(ch.count == 1 && ch.change[0].has_add && !ch.change[0].has_remove && ch.change[0].add_id == 1 &&
            ch.change[0].add_teid == 0xabcd0001 && ch.change[0].add_addr.ipv4);
    /* the same tunnel again is no change; a container without a tunnel changes nothing */
    changes = smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_NULL, list, &answer, &ch);
    CHECK(changes == 0 && ch.count == 0 && ogs_list_count(&mbs_sess->dl_tunnel_list) == 1);
    OpenAPI_list_for_each(list, node)
        OpenAPI_n2_mbs_sm_info_free(node->data);
    OpenAPI_list_free(list);
    list = OpenAPI_list_create();
    OpenAPI_list_add(list, n2_info(OpenAPI_ngap_ie_type_MBS_SES_RSP, "n2-none", ran_a_text));
    changes = smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_NULL, list, &answer, NULL);
    CHECK(changes == 0 && ogs_list_count(&mbs_sess->dl_tunnel_list) == 1);
    /* a container that is not in the message is left alone */
    OpenAPI_list_for_each(list, node)
        OpenAPI_n2_mbs_sm_info_free(node->data);
    OpenAPI_list_free(list);
    list = OpenAPI_list_create();
    OpenAPI_list_add(list, n2_info(OpenAPI_ngap_ie_type_MBS_SES_RSP, "n2-missing", ran_a_text));
    changes = smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_NULL, list, &answer, NULL);
    CHECK(changes == 0 && ogs_list_count(&mbs_sess->dl_tunnel_list) == 1);
    OpenAPI_list_for_each(list, node)
        OpenAPI_n2_mbs_sm_info_free(node->data);
    OpenAPI_list_free(list);

    /* a failure of the NG-RAN takes its tunnel away; its release too */
    list = OpenAPI_list_create();
    OpenAPI_list_add(list, n2_info(OpenAPI_ngap_ie_type_MBS_SES_FAIL, "n2-fail", ran_a_text));
    changes = smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_NULL, list, &answer, &ch);
    CHECK(changes == 1 && ogs_list_empty(&mbs_sess->dl_tunnel_list));
    CHECK(ch.count == 1 && !ch.change[0].has_add && ch.change[0].has_remove && ch.change[0].remove_id == 1);
    OpenAPI_list_for_each(list, node)
        OpenAPI_n2_mbs_sm_info_free(node->data);
    OpenAPI_list_free(list);
    list = OpenAPI_list_create();
    OpenAPI_list_add(list, n2_info(OpenAPI_ngap_ie_type_MBS_SES_RSP, "n2-a", ran_a_text));
    smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_NULL, list, &answer, &ch);
    CHECK(ogs_list_count(&mbs_sess->dl_tunnel_list) == 1 && ch.count == 1 && ch.change[0].add_id == 2);
    {
        /* the node gives another tunnel: a new ID, the old one removed with it */
        ogs_pkbuf_t *other = encode_response_transfer(true, "10.20.30.41", 0x11110002);
        ogs_sbi_message_t answer2;

        memset(&answer2, 0, sizeof(answer2));
        answer2.part[0].pkbuf = other;
        answer2.part[0].content_id = (char *)"n2-a";
        answer2.num_of_part = 1;
        changes = smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_NULL, list, &answer2, &ch);
        CHECK(changes == 1 && ch.count == 1 && ch.change[0].has_add && ch.change[0].add_id == 3 &&
                ch.change[0].add_teid == 0x11110002 && ch.change[0].has_remove && ch.change[0].remove_id == 2);
        CHECK(ogs_list_count(&mbs_sess->dl_tunnel_list) == 1);
        ogs_pkbuf_free(other);
    }
    OpenAPI_list_for_each(list, node)
        OpenAPI_n2_mbs_sm_info_free(node->data);
    OpenAPI_list_free(list);
    list = OpenAPI_list_create();
    OpenAPI_list_add(list, n2_info(OpenAPI_ngap_ie_type_MBS_SES_REL_RSP, NULL, ran_a_text));
    changes = smf_mbs_sess_take_amf_answer(mbs_sess, amf1, OpenAPI_operation_status_NULL, list, &answer, NULL);
    CHECK(changes == 1 && ogs_list_empty(&mbs_sess->dl_tunnel_list));
    OpenAPI_list_for_each(list, node)
        OpenAPI_n2_mbs_sm_info_free(node->data);
    OpenAPI_list_free(list);

    ogs_free(ran_a);
    ogs_pkbuf_free(rsp_a);
    ogs_pkbuf_free(rsp_none);
    ogs_pkbuf_free(fail);
    smf_mbs_sess_release(mbs_sess);
}

/* TS 29.244 V18.10.0 table 7.5.4.3-1 (Update FAR: FAR ID "M", Apply Action "C ... present if it is changed", Add MBS
 * Unicast Parameters, Remove MBS Unicast Parameters), table 7.5.2.3-6 (Add MBS Unicast Parameters: Destination
 * Interface "M", MBS Unicast Parameters ID "M", Outer Header Creation "M ... It shall contain the F-TEID of the remote
 * GTP-U peer when adding a GTP-U/UDP/IP header.") and table 7.5.4.3-4 (Remove MBS Unicast Parameters: the ID). */
static void test_unicast_tunnel_modification(void)
{
    smf_mbs_sess_t *mbs_sess = make_session(serv_info_2_text, area_1_text);
    smf_n4mb_modification_t mod;
    smf_mbs_tunnel_change_t change;
    ogs_pfcp_message_t *msg;
    ogs_pfcp_far_t *far;
    int n;

    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;
    memset(&mod, 0, sizeof(mod));
    memset(&change, 0, sizeof(change));
    change.has_add = true;
    change.add_addr.ipv4 = 1;
    change.add_addr.len = OGS_IPV4_LEN;
    change.add_addr.addr = htonl(0x0a141e28);       /* 10.20.30.40 */
    change.add_teid = 0xabcd0001;
    change.add_id = 0x0102;
    mod.tunnel = &change;
    mod.update_apply_action = true;
    ogs_list_for_each(&mbs_sess->pfcp.far_list, far)
        far->apply_action |= OGS_PFCP_APPLY_ACTION_MBSU;

    msg = build_and_parse_modification_of(mbs_sess, &mod);
    CHECK(msg != NULL);
    if (msg) {
        ogs_pfcp_session_modification_request_t *req = &msg->pfcp_session_modification_request;
        static const uint8_t expected_ohc[10] = { 0x01, 0x00, 0xab, 0xcd, 0x00, 0x01, 10, 20, 30, 40 };
        static const uint8_t expected_id[2] = { 0x01, 0x02 };

        /* one Update FAR for each of the two flows, none else */
        for (n = 0; n < 2; n++) {
            ogs_pfcp_tlv_update_far_t *u = &req->update_far[n];

            CHECK(u->presence && u->far_id.presence);
            CHECK(u->apply_action.presence && (u->apply_action.u16 & OGS_PFCP_APPLY_ACTION_MBSU) &&
                    (u->apply_action.u16 & OGS_PFCP_APPLY_ACTION_FSSM));
            CHECK(u->add_mbs_unicast_parameters[0].presence &&
                    u->add_mbs_unicast_parameters[0].destination_interface.presence &&
                    u->add_mbs_unicast_parameters[0].destination_interface.u8 == OGS_PFCP_INTERFACE_ACCESS);
            CHECK(u->add_mbs_unicast_parameters[0].mbs_unicast_parameters_id.presence &&
                    u->add_mbs_unicast_parameters[0].mbs_unicast_parameters_id.len == 2 &&
                    memcmp(u->add_mbs_unicast_parameters[0].mbs_unicast_parameters_id.data, expected_id, 2) == 0);
            CHECK(u->add_mbs_unicast_parameters[0].outer_header_creation.presence &&
                    u->add_mbs_unicast_parameters[0].outer_header_creation.len == 10 &&
                    memcmp(u->add_mbs_unicast_parameters[0].outer_header_creation.data, expected_ohc, 10) == 0);
            CHECK(!u->remove_mbs_unicast_parameters[0].presence);
        }
        CHECK(!req->update_far[2].presence);
        CHECK(req->update_far[0].far_id.u32 != req->update_far[1].far_id.u32);
        CHECK(!req->create_pdr[0].presence && !req->remove_pdr[0].presence);
    }

    /* removal of a tunnel (and the Apply Action not changed): the ID of the tunnel */
    memset(&change, 0, sizeof(change));
    change.has_remove = true;
    change.remove_id = 0x0304;
    mod.update_apply_action = false;
    msg = build_and_parse_modification_of(mbs_sess, &mod);
    CHECK(msg != NULL);
    if (msg) {
        ogs_pfcp_tlv_update_far_t *u = &msg->pfcp_session_modification_request.update_far[0];
        static const uint8_t expected_remove[2] = { 0x03, 0x04 };

        CHECK(u->presence && !u->apply_action.presence && !u->add_mbs_unicast_parameters[0].presence);
        CHECK(u->remove_mbs_unicast_parameters[0].presence &&
                u->remove_mbs_unicast_parameters[0].mbs_unicast_parameters_id.presence &&
                u->remove_mbs_unicast_parameters[0].mbs_unicast_parameters_id.len == 2 &&
                memcmp(u->remove_mbs_unicast_parameters[0].mbs_unicast_parameters_id.data, expected_remove, 2) == 0);
    }

    /* an IPv6 tunnel: GTP-U/UDP/IPv6, 22 octets */
    memset(&change, 0, sizeof(change));
    change.has_add = true;
    change.add_addr.ipv6 = 1;
    change.add_addr.len = OGS_IPV6_LEN;
    inet_pton(AF_INET6, "2001:db8::7", change.add_addr.addr6);
    change.add_teid = 7;
    change.add_id = 5;
    msg = build_and_parse_modification_of(mbs_sess, &mod);
    CHECK(msg != NULL);
    if (msg) {
        ogs_pfcp_tlv_update_far_t *u = &msg->pfcp_session_modification_request.update_far[0];
        const uint8_t *d = u->add_mbs_unicast_parameters[0].outer_header_creation.data;

        CHECK(u->add_mbs_unicast_parameters[0].outer_header_creation.len == 22 && d[0] == 0x02 && d[1] == 0 &&
                d[5] == 7 && d[6] == 0x20 && d[7] == 0x01 && d[21] == 0x07);
    }

    smf_mbs_sess_release(mbs_sess);
}

/* TS 24.008 V18.8.0 figure 10.5.154 (octets 3 to 8 of the TMGI, as TS 29.244 V18.10.0 cl. 8.2.206 requires):
 * octet 6 "MCC digit 2 | MCC digit 1", octet 7 "MNC digit 3 | MCC digit 3", octet 8 "MNC digit 2 | MNC digit 1"
 * (a two-digit MNC leaves digit 3 as F, as the identity form of the IE says: "If a network operator decides to use
 * only two digits in the MNC, bits 5 to 8 of octet 6b shall be coded as "1111"."). */
static void test_tmgi_octets(void)
{
    smf_mbs_sess_t *mbs_sess = new_broadcast_session();
    ogs_pfcp_message_t *msg;
    ogs_tlv_octet_t *id;
    uint8_t *d;

    CHECK(mbs_sess != NULL);
    if (!mbs_sess) return;

    /* PLMN 001/01: MCC digits 0 0 1, MNC digits 0 1 and F */
    ogs_plmn_id_build(&mbs_sess->tmgi->plmn_id, 1, 1, 2);
    msg = build_and_parse_establishment(mbs_sess);
    CHECK(msg != NULL);
    if (msg) {
        id = &msg->pfcp_session_establishment_request.mbs_session_n4mb_control_information.mbs_session_identifier;
        d = id->data;
        CHECK(id->len == 7);
        CHECK(d[4] == 0x00 && d[5] == 0xF1 && d[6] == 0x10);
    }

    /* PLMN 310/014: MCC digits 3 1 0, MNC digits 0 1 4 */
    ogs_plmn_id_build(&mbs_sess->tmgi->plmn_id, 310, 14, 3);
    msg = build_and_parse_establishment(mbs_sess);
    CHECK(msg != NULL);
    if (msg) {
        id = &msg->pfcp_session_establishment_request.mbs_session_n4mb_control_information.mbs_session_identifier;
        d = id->data;
        CHECK(d[4] == 0x13 && d[5] == 0x40 && d[6] == 0x10);
    }

    /* the PLMN is part of the key of the index of sessions: put it back before the release */
    ogs_plmn_id_build(&mbs_sess->tmgi->plmn_id, 1, 1, 2);
    smf_mbs_sess_release(mbs_sess);
}

/* TS 29.518 V18.14.0 table 6.5.6.2.4-1: areaSessionId names the part of a location dependent session; releasedInd
 * true: the context is released in the AMF. */
static void test_notification_routing(void)
{
    smf_mbs_sess_t *a, *b, *plain;
    ogs_tmgi_t *tmgi = smf_tmgi_allocate(smf_tmgi_gen_expiration_time(60));
    ogs_tmgi_t *tmgi2 = smf_tmgi_allocate(smf_tmgi_gen_expiration_time(60));

    CHECK(tmgi != NULL && tmgi2 != NULL);
    if (!tmgi || !tmgi2) return;

    a = create_session(tmgi, true, true, "1", NULL, NULL);
    b = create_session(tmgi, false, true, "2", NULL, NULL);
    plain = create_session(tmgi2, true, false, NULL, NULL, NULL);
    CHECK(a && b && plain);
    if (!a || !b || !plain) return;

    CHECK(smf_mbs_sess_find_by_tmgi_and_area(tmgi, true, a->area_session_id) == a);
    CHECK(smf_mbs_sess_find_by_tmgi_and_area(tmgi, true, b->area_session_id) == b);
    CHECK(smf_mbs_sess_find_by_tmgi_and_area(tmgi, true, 99) == NULL);
    CHECK(smf_mbs_sess_find_by_tmgi_and_area(tmgi, false, 0) != NULL);
    /* a session that is not location dependent is found by its TMGI */
    CHECK(smf_mbs_sess_find_by_tmgi_and_area(tmgi2, false, 0) == plain);
    CHECK(smf_mbs_sess_find_by_tmgi_and_area(tmgi2, true, 5) == plain);

    {
        smf_mbs_amf_t *amf1 = smf_mbs_sess_amf_add(a), *amf2 = smf_mbs_sess_amf_add(a);

        CHECK(amf1 && amf2 && amf1->id != amf2->id);
        amf1->mbs_context_ref = ogs_strdup("17");
        amf2->mbs_context_ref = ogs_strdup("18");
        CHECK(smf_mbs_sess_amf_find(a, amf1->id) == amf1 && smf_mbs_sess_amf_find(a, amf2->id) == amf2);
        CHECK(smf_mbs_sess_amf_find(a, amf2->id + 1) == NULL);
        /* the AMF that reported its context released goes, the other stays */
        smf_mbs_sess_context_released(a, amf1);
        CHECK(ogs_list_count(&a->amf_list) == 1 && smf_mbs_sess_amf_find(a, amf2->id) == amf2);
        smf_mbs_sess_context_released(a, NULL);   /* a notification from an unknown AMF */
        CHECK(ogs_list_count(&a->amf_list) == 1);
    }

    smf_mbs_sess_release(a);
    smf_mbs_sess_release(b);
    smf_mbs_sess_release(plain);
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
    test_create_outcomes();
    test_reduced_area_is_used();
    test_nr_red_cap_ue_info_model();
    test_nr_red_cap_ue_info_n2();
    test_fsa_id_list();
    test_context_create_optional_ids();
    test_date_time();
    test_service_info_validity();
    test_flow_description_restrictions();
    test_per_flow_user_plane();
    test_json_patch();
    test_update();
    test_update_reduced_area();
    test_amf_entries();
    test_status_subscription();
    test_amf_answer();
    test_unicast_tunnel_modification();
    test_context_update_request();
    test_local_ingress_tunnel_choose();
    test_ip_multicast_addressing_info_layout();
    test_tmgi_octets();
    test_notification_routing();

    printf("smf-mbs: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
