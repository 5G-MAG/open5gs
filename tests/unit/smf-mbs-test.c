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
#include "nmbsmf-handler.h"

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
    test_fsa_id_list();
    test_context_create_optional_ids();
    test_date_time();
    test_service_info_validity();
    test_per_flow_user_plane();
    test_tmgi_octets();

    printf("smf-mbs: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
