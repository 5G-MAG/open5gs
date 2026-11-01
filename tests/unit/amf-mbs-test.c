/*
 * Unit tests of the AMF (src/amf) for the Namf_MBSBroadcast messages that carry N2 MBS Session Management
 * containers (ContextCreate and ContextUpdate responses, ContextStatusNotify requests). They need neither a peer
 * nor a running AMF: the functions take their input as arguments.
 *
 * This file is self-contained (its own main): it runs on its own with `meson test amf-mbs`.
 */

#include "ogs-core.h"
#include "ogs-app.h"
#include "ogs-sbi.h"
#include "context.h"
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

/* the content of a built request, for substring checks */
static const char *body_of(ogs_sbi_request_t *request)
{
    return request->http.content ? request->http.content : "";
}

/* amf_gnb_t is large: on the heap */
static amf_gnb_t *make_gnb(uint32_t gnb_id, uint8_t bit_length)
{
    amf_gnb_t *gnb = ogs_calloc(1, sizeof(*gnb));

    ogs_plmn_id_build(&gnb->plmn_id, 999, 70, 2);
    gnb->gnb_id = gnb_id;
    gnb->gnb_id_bit_length = bit_length;
    /* the gNB ID as the bit string of NG Setup carries it: left aligned in three octets */
    gnb->gnb_id_buf[0] = (gnb_id << (24 - bit_length)) >> 16;
    gnb->gnb_id_buf[1] = (gnb_id << (24 - bit_length)) >> 8;
    gnb->gnb_id_buf[2] = (gnb_id << (24 - bit_length));
    return gnb;
}

/* TS 29.518 V18.14.0 cl. 5.6.2.5: "for each N2 MBS Session Management container sent towards the MB-SMF, the AMF
 * shall insert the identifier of the NG-RAN node that generated it in the corresponding entry of the
 * n2MbsSmInfoList attribute." */
static void test_n2_info_list(void)
{
    static const uint8_t t1[] = { 0x0a, 0x0b }, t2[] = { 0x0c };
    amf_gnb_t *gnb1 = make_gnb(0x100, 22), *gnb2 = make_gnb(0x200, 22);
    ogs_list_t infos;
    ogs_sbi_message_t message;
    OpenAPI_list_t *list;
    OpenAPI_n2_mbs_sm_info_t *e0, *e1;

    ogs_list_init(&infos);
    ogs_list_add(&infos, amf_mbs_n2_info_create(gnb1, OpenAPI_ngap_ie_type_MBS_SES_RSP, t1, sizeof(t1)));
    ogs_list_add(&infos, amf_mbs_n2_info_create(gnb2, OpenAPI_ngap_ie_type_MBS_SES_FAIL, t2, sizeof(t2)));
    CHECK(ogs_list_count(&infos) == 2);

    memset(&message, 0, sizeof(message));
    list = amf_namf_build_n2_mbs_sm_info_list(&message, &infos);
    CHECK(list != NULL);
    CHECK(message.num_of_part == 2);
    if (list && list->count == 2 && message.num_of_part == 2) {
        e0 = list->first->data;
        e1 = list->first->next->data;

        CHECK(e0->ngap_ie_type == OpenAPI_ngap_ie_type_MBS_SES_RSP);
        CHECK(e1->ngap_ie_type == OpenAPI_ngap_ie_type_MBS_SES_FAIL);

        /* each container references its own binary part */
        CHECK_STR(message.part[0].content_id, e0->ngap_data->content_id);
        CHECK_STR(message.part[1].content_id, e1->ngap_data->content_id);
        CHECK(strcmp(message.part[0].content_id, message.part[1].content_id) != 0);
        CHECK(message.part[0].pkbuf->len == 2 && message.part[0].pkbuf->data[1] == 0x0b);
        CHECK(message.part[1].pkbuf->len == 1 && message.part[1].pkbuf->data[0] == 0x0c);

        /* the identity of the node that generated it */
        CHECK(e0->rand_id && e0->rand_id->g_nb_id);
        if (e0->rand_id && e0->rand_id->g_nb_id) {
            CHECK(e0->rand_id->g_nb_id->bit_length == 22);
            CHECK_STR("000400", e0->rand_id->g_nb_id->g_nb_value);
            CHECK_STR("999", e0->rand_id->plmn_id->mcc);
            CHECK_STR("70", e0->rand_id->plmn_id->mnc);
        }
        CHECK(e1->rand_id && e1->rand_id->g_nb_id);
        if (e1->rand_id && e1->rand_id->g_nb_id)
            CHECK_STR("000800", e1->rand_id->g_nb_id->g_nb_value);
    }

    amf_namf_free_n2_mbs_sm_info_list(list);
    amf_mbs_n2_info_remove_all(&infos);
    CHECK(ogs_list_count(&infos) == 0);
    ogs_free(gnb1);
    ogs_free(gnb2);
}

/* A message has at most OGS_SBI_MAX_NUM_OF_PART binary parts: the rest of the containers are not carried. */
static void test_n2_info_list_limit(void)
{
    static const uint8_t t[] = { 1 };
    amf_gnb_t *gnb = make_gnb(0x100, 22);
    ogs_list_t infos;
    ogs_sbi_message_t message;
    OpenAPI_list_t *list;
    int i;

    ogs_list_init(&infos);
    for (i = 0; i < OGS_SBI_MAX_NUM_OF_PART + 2; i++)
        ogs_list_add(&infos, amf_mbs_n2_info_create(gnb, OpenAPI_ngap_ie_type_MBS_SES_RSP, t, sizeof(t)));

    memset(&message, 0, sizeof(message));
    list = amf_namf_build_n2_mbs_sm_info_list(&message, &infos);
    CHECK(list != NULL && list->count == OGS_SBI_MAX_NUM_OF_PART);
    CHECK(message.num_of_part == OGS_SBI_MAX_NUM_OF_PART);

    amf_namf_free_n2_mbs_sm_info_list(list);
    amf_mbs_n2_info_remove_all(&infos);

    /* nothing to carry: no list, no part */
    memset(&message, 0, sizeof(message));
    CHECK(amf_namf_build_n2_mbs_sm_info_list(&message, &infos) == NULL);
    CHECK(message.num_of_part == 0);
    ogs_free(gnb);
}

static amf_mbs_context_t make_context(void)
{
    amf_mbs_context_t ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.tmgi.mbs_service_id = (char *)"000001";
    ogs_plmn_id_build(&ctx.tmgi.plmn_id, 999, 70, 2);
    ctx.notify_uri = (char *)"http://127.0.0.30:9999/notify";
    return ctx;
}

/* TS 29.518 V18.14.0 Table 6.5.6.2.4-1: areaSessionId "shall be present if this is a Location dependent broadcast
 * MBS service", operationStatus "shall be present and indicate the completion of the MBS session start or update
 * operation ... a response has been received from all NG-RANs." */
static void test_status_notify(void)
{
    static const uint8_t t[] = { 0x0a, 0x0b };
    amf_gnb_t *gnb = make_gnb(0x100, 22);
    amf_mbs_context_t ctx = make_context();
    ogs_list_t infos;
    ogs_sbi_request_t *request;

    ogs_list_init(&infos);

    /* location independent, nothing to carry, no status: only the session ID */
    request = amf_namf_build_mbs_broadcast_context_status_notify(&ctx, &infos, OpenAPI_operation_status_NULL);
    CHECK(request != NULL);
    if (request) {
        CHECK_STR("http://127.0.0.30:9999/notify", request->h.uri);
        CHECK(strstr(body_of(request), "\"mbsServiceId\":\"000001\"") != NULL);
        CHECK(strstr(body_of(request), "areaSessionId") == NULL);
        CHECK(strstr(body_of(request), "operationStatus") == NULL);
        CHECK(strstr(body_of(request), "n2MbsSmInfoList") == NULL);
        ogs_sbi_request_free(request);
    }

    /* location dependent, one container, update complete */
    ctx.location_dependent = true;
    ctx.area_session_id = 13;
    ogs_list_add(&infos, amf_mbs_n2_info_create(gnb, OpenAPI_ngap_ie_type_MBS_SES_FAIL, t, sizeof(t)));
    request = amf_namf_build_mbs_broadcast_context_status_notify(
            &ctx, &infos, OpenAPI_operation_status_MBS_SESSION_UPDATE_COMPLETE);
    CHECK(request != NULL);
    if (request) {
        CHECK(strstr(body_of(request), "\"areaSessionId\":13") != NULL);
        CHECK(strstr(body_of(request), "\"operationStatus\":\"MBS_SESSION_UPDATE_COMPLETE\"") != NULL);
        CHECK(strstr(body_of(request), "\"ngapIeType\":\"MBS_SES_FAIL\"") != NULL);
        CHECK(strstr(body_of(request), "\"gNBValue\":\"000400\"") != NULL);
        CHECK(strstr(body_of(request), "Content-Id: ngap-sm-0") != NULL);
        ogs_sbi_request_free(request);
    }
    amf_mbs_n2_info_remove_all(&infos);

    /* the request has no target without the consumer's notifyUri */
    ctx.notify_uri = NULL;
    CHECK(amf_namf_build_mbs_broadcast_context_status_notify(
                &ctx, &infos, OpenAPI_operation_status_NULL) == NULL);
    ogs_free(gnb);
}

int main(int argc, const char *argv[])
{
    ogs_pkbuf_config_t config;

    ogs_core_initialize();
    ogs_pkbuf_default_init(&config);
    ogs_pkbuf_default_create(&config);
    ogs_log_install_domain(&__ogs_sbi_domain, "sbi", OGS_LOG_ERROR);
    ogs_log_install_domain(&__amf_log_domain, "amf", OGS_LOG_ERROR);
    ogs_sbi_message_init(32, 32);
    /* ogs_sbi_build_request() takes the maximum response time of a request from the configuration */
    ogs_local_conf()->time.message.duration = ogs_time_from_sec(10);

    test_n2_info_list();
    test_n2_info_list_limit();
    test_status_notify();

    ogs_sbi_message_final();
    ogs_pkbuf_default_destroy();
    ogs_core_terminate();

    printf("amf-mbs: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
