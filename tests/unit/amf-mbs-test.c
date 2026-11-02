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
#include "namf-handler.h"
#include "ngap-build.h"

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
        CHECK(e0->ran_id && e0->ran_id->g_nb_id);
        if (e0->ran_id && e0->ran_id->g_nb_id) {
            CHECK(e0->ran_id->g_nb_id->bit_length == 22);
            CHECK_STR("000400", e0->ran_id->g_nb_id->g_nb_value);
            CHECK_STR("999", e0->ran_id->plmn_id->mcc);
            CHECK_STR("70", e0->ran_id->plmn_id->mnc);
        }
        CHECK(e1->ran_id && e1->ran_id->g_nb_id);
        if (e1->ran_id && e1->ran_id->g_nb_id)
            CHECK_STR("000800", e1->ran_id->g_nb_id->g_nb_value);
    }

    amf_namf_free_n2_mbs_sm_info_list(list);
    amf_mbs_n2_info_remove_all(&infos);
    CHECK(ogs_list_count(&infos) == 0);
    ogs_free(gnb1);
    ogs_free(gnb2);
}

/* TS 29.518 V18.14.0 table 6.5.6.2.7-1 (N2MbsSmInfo): "ranId | GlobalRanNodeId | M | 1 | This IE shall indicate the Global
 * RAN ID of the gNB that generated the N2 MBS Session Management related information, or of the gNB towards which the
 * N2 MBS Session Management related information is to be sent.", and TS29518_Namf_MBSBroadcast.yaml of that version:
 * "ranId:" under N2MbsSmInfo, "$ref: 'TS29571_CommonData.yaml#/components/schemas/GlobalRanNodeId'". The member name in
 * the JSON is ranId (not the randId of the Release 17 schema). */
static void test_n2_mbs_sm_info_ran_id_key(void)
{
    static const uint8_t t1[] = { 0x0a };
    amf_gnb_t *gnb = make_gnb(0x100, 22);
    ogs_list_t infos;
    ogs_sbi_message_t message;
    OpenAPI_list_t *list;
    OpenAPI_n2_mbs_sm_info_t *info = NULL;
    cJSON *json, *back;
    OpenAPI_n2_mbs_sm_info_t *parsed;

    ogs_list_init(&infos);
    ogs_list_add(&infos, amf_mbs_n2_info_create(gnb, OpenAPI_ngap_ie_type_MBS_SES_RSP, t1, sizeof(t1)));
    memset(&message, 0, sizeof(message));
    list = amf_namf_build_n2_mbs_sm_info_list(&message, &infos);
    CHECK(list && list->count == 1);
    if (list && list->count == 1) {
        info = list->first->data;
        json = OpenAPI_n2_mbs_sm_info_convertToJSON(info);
        CHECK(json != NULL);
        if (json) {
            back = cJSON_GetObjectItemCaseSensitive(json, "ranId");
            CHECK(back != NULL);
            CHECK(cJSON_GetObjectItemCaseSensitive(json, "randId") == NULL);
            CHECK(back && cJSON_GetObjectItemCaseSensitive(back, "gNbId") != NULL);
            cJSON_Delete(json);
        }
    }

    /* a body with ranId gives the identity; one with the old name gives none */
    json = cJSON_Parse("{\"ngapIeType\":\"MBS_SES_RSP\",\"ngapData\":{\"contentId\":\"c\"},"
            "\"ranId\":{\"plmnId\":{\"mcc\":\"999\",\"mnc\":\"70\"},"
            "\"gNbId\":{\"bitLength\":22,\"gNBValue\":\"000400\"}}}");
    parsed = OpenAPI_n2_mbs_sm_info_parseFromJSON(json);
    cJSON_Delete(json);
    CHECK(parsed && parsed->ran_id && parsed->ran_id->g_nb_id);
    if (parsed && parsed->ran_id && parsed->ran_id->g_nb_id) {
        CHECK_STR("000400", parsed->ran_id->g_nb_id->g_nb_value);
        CHECK(amf_namf_ran_id_is_gnb(gnb, parsed->ran_id));
    }
    if (parsed) OpenAPI_n2_mbs_sm_info_free(parsed);

    json = cJSON_Parse("{\"ngapIeType\":\"MBS_SES_RSP\",\"ngapData\":{\"contentId\":\"c\"},"
            "\"randId\":{\"plmnId\":{\"mcc\":\"999\",\"mnc\":\"70\"},"
            "\"gNbId\":{\"bitLength\":22,\"gNBValue\":\"000400\"}}}");
    parsed = OpenAPI_n2_mbs_sm_info_parseFromJSON(json);
    cJSON_Delete(json);
    CHECK(parsed && parsed->ran_id == NULL);
    if (parsed) OpenAPI_n2_mbs_sm_info_free(parsed);

    amf_namf_free_n2_mbs_sm_info_list(list);
    amf_mbs_n2_info_remove_all(&infos);
    ogs_free(gnb);
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
    request = amf_namf_build_mbs_broadcast_context_status_notify(&ctx, &infos, OpenAPI_operation_status_NULL, false, NULL);
    CHECK(request != NULL);
    if (request) {
        CHECK_STR("http://127.0.0.30:9999/notify", request->h.uri);
        CHECK(strstr(body_of(request), "\"mbsServiceId\":\"000001\"") != NULL);
        CHECK(strstr(body_of(request), "areaSessionId") == NULL);
        CHECK(strstr(body_of(request), "operationStatus") == NULL);
        CHECK(strstr(body_of(request), "n2MbsSmInfoList") == NULL);
        CHECK(strstr(body_of(request), "releasedInd") == NULL);
        ogs_sbi_request_free(request);
    }

    /* TS 29.518 V18.14.0 Table 6.5.6.2.4-1 releasedInd: present and "true" only when asked */
    request = amf_namf_build_mbs_broadcast_context_status_notify(
            &ctx, &infos, OpenAPI_operation_status_NULL, true, NULL);
    CHECK(request != NULL);
    if (request) {
        CHECK(strstr(body_of(request), "\"releasedInd\":true") != NULL);
        CHECK(strstr(body_of(request), "\"mbsServiceId\":\"000001\"") != NULL);
        ogs_sbi_request_free(request);
    }

    /* TS 29.518 V18.14.0 cl. 5.6.2.5: an NG-RAN that asked for the release is reported as an NG_RAN_EVENT with an
     * ngranFailureEvent NG_RAN_REQUIRED_RELEASE, with the gNB ID (Table 6.5.6.2.9-1: ngranId M) */
    request = amf_namf_build_mbs_broadcast_context_status_notify(
            &ctx, &infos, OpenAPI_operation_status_NULL, false, gnb);
    CHECK(request != NULL);
    if (request) {
        CHECK(strstr(body_of(request), "\"opEventType\":\"NG_RAN_EVENT\"") != NULL);
        CHECK(strstr(body_of(request), "\"ngranFailureIndication\":\"NG_RAN_REQUIRED_RELEASE\"") != NULL);
        CHECK(strstr(body_of(request), "\"gNBValue\":\"000400\"") != NULL);
        CHECK(strstr(body_of(request), "\"bitLength\":22") != NULL);
        CHECK(strstr(body_of(request), "releasedInd") == NULL);
        ogs_sbi_request_free(request);
    }

    /* no gNB ID known: no event rather than an event without its mandatory identity */
    {
        amf_gnb_t *unknown = make_gnb(0, 0);

        request = amf_namf_build_mbs_broadcast_context_status_notify(
                &ctx, &infos, OpenAPI_operation_status_NULL, false, unknown);
        CHECK(request != NULL);
        if (request) {
            CHECK(strstr(body_of(request), "operationEvents") == NULL);
            ogs_sbi_request_free(request);
        }
        ogs_free(unknown);
    }

    /* location dependent, one container, update complete */
    ctx.location_dependent = true;
    ctx.area_session_id = 13;
    ogs_list_add(&infos, amf_mbs_n2_info_create(gnb, OpenAPI_ngap_ie_type_MBS_SES_FAIL, t, sizeof(t)));
    request = amf_namf_build_mbs_broadcast_context_status_notify(
            &ctx, &infos, OpenAPI_operation_status_MBS_SESSION_UPDATE_COMPLETE, false, NULL);
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
                &ctx, &infos, OpenAPI_operation_status_NULL, false, NULL) == NULL);
    ogs_free(gnb);
}

/* Decodes an NGAP PDU built by the AMF; returns false when it does not decode. */
static bool decode_pdu(ogs_pkbuf_t *pkbuf, ogs_ngap_message_t *pdu)
{
    memset(pdu, 0, sizeof(*pdu));
    return pkbuf && ogs_ngap_decode(pdu, pkbuf) == OGS_OK;
}

/* TS 38.413 V18.8.0 clauses 9.2.16.11 and 9.2.16.12: BROADCAST SESSION TRANSPORT RESPONSE (MBS Session ID M, Broadcast
 * Transport Response Transfer M; successful outcome of the procedure of clause 8.17.5) and BROADCAST SESSION TRANSPORT
 * FAILURE (MBS Session ID M, Broadcast Transport Failure Transfer M, Cause M; unsuccessful outcome). */
static void test_transport_messages(void)
{
    static const uint8_t transfer[] = { 0x00, 0x01, 0x02 };
    amf_mbs_context_t ctx = make_context();
    ogs_ngap_message_t pdu;
    ogs_pkbuf_t *buf;
    int i, found;

    buf = ngap_build_broadcast_session_transport_response(&ctx.tmgi, transfer, sizeof(transfer));
    CHECK(decode_pdu(buf, &pdu));
    if (buf && pdu.present == NGAP_NGAP_PDU_PR_successfulOutcome) {
        NGAP_SuccessfulOutcome_t *outcome = pdu.choice.successfulOutcome;
        NGAP_BroadcastSessionTransportResponse_t *rsp;

        CHECK(outcome->procedureCode == NGAP_ProcedureCode_id_BroadcastSessionTransport);
        CHECK(outcome->value.present == NGAP_SuccessfulOutcome__value_PR_BroadcastSessionTransportResponse);
        rsp = &outcome->value.choice.BroadcastSessionTransportResponse;
        found = 0;
        for (i = 0; i < rsp->protocolIEs.list.count; i++) {
            NGAP_BroadcastSessionTransportResponseIEs_t *ie = rsp->protocolIEs.list.array[i];

            if (ie->id == NGAP_ProtocolIE_ID_id_MBS_SessionID && ie->criticality == NGAP_Criticality_reject)
                found |= 1;
            if (ie->id == NGAP_ProtocolIE_ID_id_BroadcastTransportResponseTransfer &&
                    ie->criticality == NGAP_Criticality_reject &&
                    ie->value.choice.OCTET_STRING_CONTAINING_BroadcastTransportResponseTransfer_.size ==
                        sizeof(transfer))
                found |= 2;
        }
        CHECK(found == 3 && rsp->protocolIEs.list.count == 2);
    } else
        CHECK(0);
    if (buf) ogs_ngap_free(&pdu);
    if (buf) ogs_pkbuf_free(buf);

    buf = ngap_build_broadcast_session_transport_failure(&ctx.tmgi, transfer, sizeof(transfer),
            NGAP_Cause_PR_radioNetwork, NGAP_CauseRadioNetwork_unknown_MBS_Session_ID);
    CHECK(decode_pdu(buf, &pdu));
    if (buf && pdu.present == NGAP_NGAP_PDU_PR_unsuccessfulOutcome) {
        NGAP_UnsuccessfulOutcome_t *outcome = pdu.choice.unsuccessfulOutcome;
        NGAP_BroadcastSessionTransportFailure_t *failure;

        CHECK(outcome->procedureCode == NGAP_ProcedureCode_id_BroadcastSessionTransport);
        CHECK(outcome->value.present == NGAP_UnsuccessfulOutcome__value_PR_BroadcastSessionTransportFailure);
        failure = &outcome->value.choice.BroadcastSessionTransportFailure;
        found = 0;
        for (i = 0; i < failure->protocolIEs.list.count; i++) {
            NGAP_BroadcastSessionTransportFailureIEs_t *ie = failure->protocolIEs.list.array[i];

            if (ie->id == NGAP_ProtocolIE_ID_id_MBS_SessionID)
                found |= 1;
            if (ie->id == NGAP_ProtocolIE_ID_id_BroadcastTransportFailureTransfer &&
                    ie->criticality == NGAP_Criticality_ignore)
                found |= 2;
            if (ie->id == NGAP_ProtocolIE_ID_id_Cause && ie->criticality == NGAP_Criticality_ignore &&
                    ie->value.choice.Cause.present == NGAP_Cause_PR_radioNetwork &&
                    ie->value.choice.Cause.choice.radioNetwork == NGAP_CauseRadioNetwork_unknown_MBS_Session_ID)
                found |= 4;
        }
        CHECK(found == 7 && failure->protocolIEs.list.count == 3);
    } else
        CHECK(0);
    if (buf) ogs_ngap_free(&pdu);
    if (buf) ogs_pkbuf_free(buf);

    /* the Failure Transfer the AMF builds itself decodes, with its Cause */
    buf = ngap_build_broadcast_transport_failure_transfer(&ctx.tmgi, NGAP_Cause_PR_transport,
            NGAP_CauseTransport_transport_resource_unavailable);
    CHECK(buf != NULL);
    if (buf) {
        NGAP_BroadcastTransportFailureTransfer_t ft;

        memset(&ft, 0, sizeof(ft));
        CHECK(ogs_asn_decode(&asn_DEF_NGAP_BroadcastTransportFailureTransfer, &ft, sizeof(ft), buf) == OGS_OK);
        CHECK(ft.cause.present == NGAP_Cause_PR_transport &&
                ft.cause.choice.transport == NGAP_CauseTransport_transport_resource_unavailable);
        ogs_asn_free(&asn_DEF_NGAP_BroadcastTransportFailureTransfer, &ft);
        ogs_pkbuf_free(buf);
    }
}

/* The Broadcast Session Transport Request Transfer goes to the MB-SMF as a container of the ContextStatusNotify of
 * TS 29.518 V18.14.0 clause 5.6.2.5 with the identity of the NG-RAN node, and the answers of the MB-SMF come back as
 * NGAP messages for that node: "the MB-SMF shall return a "200 OK" with the ContextStatusNotificationResponse
 * containing one or more N2 MBS Session Management containers encapsulating a Broadcast Session Transport Response
 * Transfer IE or a Broadcast Session Transport Failure Transfer IE." */
static void test_transport_relay(void)
{
    static const uint8_t request_transfer[] = { 0x01, 0x02, 0x03, 0x04 };
    amf_gnb_t *gnb = make_gnb(0x100, 22), *other = make_gnb(0x200, 22);
    amf_mbs_context_t ctx = make_context();
    ogs_list_t infos;
    ogs_sbi_request_t *request;
    ogs_sbi_message_t message;
    ogs_pkbuf_t *rsp_transfer, *fail_transfer, *garbage, *out[8];
    cJSON *json;
    int n, i;
    ogs_ngap_message_t pdu;

    /* the notification */
    ogs_list_init(&infos);
    ogs_list_add(&infos, amf_mbs_n2_info_create(gnb, OpenAPI_ngap_ie_type_BC_TRA_REQ, request_transfer,
                sizeof(request_transfer)));
    request = amf_namf_build_mbs_broadcast_context_status_notify(&ctx, &infos, OpenAPI_operation_status_NULL, false,
            NULL);
    CHECK(request != NULL);
    if (request) {
        CHECK(strstr(body_of(request), "\"ngapIeType\":\"BC_TRA_REQ\"") != NULL);
        CHECK(strstr(body_of(request), "\"gNBValue\":\"000400\"") != NULL);
        ogs_sbi_request_free(request);
    }
    amf_mbs_n2_info_remove_all(&infos);

    /* the answer */
    rsp_transfer = ogs_pkbuf_alloc(NULL, 2);
    ogs_pkbuf_put_data(rsp_transfer, "\x0a\x0b", 2);
    fail_transfer = ngap_build_broadcast_transport_failure_transfer(&ctx.tmgi, NGAP_Cause_PR_radioNetwork,
            NGAP_CauseRadioNetwork_unspecified);
    garbage = ogs_pkbuf_alloc(NULL, 3);
    ogs_pkbuf_put_data(garbage, "\xff\xff\xff", 3);

    /* the entries are set up directly: they name the NG-RAN node by its GlobalRanNodeId, whichever member name the
     * JSON of the model gives it */
    memset(&message, 0, sizeof(message));
    {
        OpenAPI_context_status_notification_response_t *rsp = NULL;
        OpenAPI_list_t *entries = OpenAPI_list_create();
        static const struct { OpenAPI_ngap_ie_type_e type; const char *content_id; const char *gnb_value; } e[] = {
            { OpenAPI_ngap_ie_type_BC_TRA_RSP, "rsp", "000400" },
            { OpenAPI_ngap_ie_type_BC_TRA_FAIL, "fail", NULL },
            { OpenAPI_ngap_ie_type_BC_TRA_RSP, "rsp", "000800" },
            { OpenAPI_ngap_ie_type_BC_TRA_FAIL, "garbage", NULL },
            { OpenAPI_ngap_ie_type_BC_TRA_RSP, "absent", NULL },
            { OpenAPI_ngap_ie_type_MBS_SES_RSP, "rsp", NULL },
        };

        for (i = 0; i < (int)OGS_ARRAY_SIZE(e); i++) {
            OpenAPI_global_ran_node_id_t *ran = NULL;

            if (e[i].gnb_value) {
                char text[200];

                snprintf(text, sizeof(text), "{\"plmnId\":{\"mcc\":\"999\",\"mnc\":\"70\"},"
                        "\"gNbId\":{\"bitLength\":22,\"gNBValue\":\"%s\"}}", e[i].gnb_value);
                json = cJSON_Parse(text);
                ran = OpenAPI_global_ran_node_id_parseFromJSON(json);
                cJSON_Delete(json);
            }
            OpenAPI_list_add(entries, OpenAPI_n2_mbs_sm_info_create(e[i].type,
                        OpenAPI_ref_to_binary_data_create(ogs_strdup((char *)e[i].content_id)), ran));
        }
        rsp = OpenAPI_context_status_notification_response_create(
                amf_namf_build_mbs_session_id(&ctx.tmgi), false, 0, entries);
        message.ContextStatusNotificationResponse = rsp;
    }
    CHECK(message.ContextStatusNotificationResponse != NULL);
    message.part[0].pkbuf = rsp_transfer;
    message.part[0].content_id = (char *)"rsp";
    message.part[1].pkbuf = fail_transfer;
    message.part[1].content_id = (char *)"fail";
    message.part[2].pkbuf = garbage;
    message.part[2].content_id = (char *)"garbage";
    message.num_of_part = 3;

    memset(out, 0, sizeof(out));
    n = amf_namf_mbs_transport_answers(gnb, &ctx.tmgi, &message, out, 8);
    /* the response of this node, and the failure that has no ranId: the entry of another node, the undecodable
     * failure, the absent part and the container of another type give none */
    CHECK(n == 2);
    if (n == 2) {
        CHECK(decode_pdu(out[0], &pdu) && pdu.present == NGAP_NGAP_PDU_PR_successfulOutcome);
        ogs_ngap_free(&pdu);
        CHECK(decode_pdu(out[1], &pdu) && pdu.present == NGAP_NGAP_PDU_PR_unsuccessfulOutcome);
        if (pdu.present == NGAP_NGAP_PDU_PR_unsuccessfulOutcome) {
            NGAP_BroadcastSessionTransportFailure_t *failure =
                &pdu.choice.unsuccessfulOutcome->value.choice.BroadcastSessionTransportFailure;

            /* the Cause is the one of the MB-SMF's Failure Transfer */
            for (i = 0; i < failure->protocolIEs.list.count; i++) {
                NGAP_BroadcastSessionTransportFailureIEs_t *ie = failure->protocolIEs.list.array[i];

                if (ie->id == NGAP_ProtocolIE_ID_id_Cause)
                    CHECK(ie->value.choice.Cause.present == NGAP_Cause_PR_radioNetwork &&
                            ie->value.choice.Cause.choice.radioNetwork == NGAP_CauseRadioNetwork_unspecified);
            }
        }
        ogs_ngap_free(&pdu);
    }
    for (i = 0; i < n; i++)
        ogs_pkbuf_free(out[i]);

    /* the other node gets its own entry; a bound that is too small leaves the rest out */
    n = amf_namf_mbs_transport_answers(other, &ctx.tmgi, &message, out, 8);
    CHECK(n == 2);
    for (i = 0; i < n; i++)
        ogs_pkbuf_free(out[i]);
    n = amf_namf_mbs_transport_answers(gnb, &ctx.tmgi, &message, out, 1);
    CHECK(n == 1);
    for (i = 0; i < n; i++)
        ogs_pkbuf_free(out[i]);

    /* no response content: no answer (the caller then sends the failure) */
    OpenAPI_context_status_notification_response_free(message.ContextStatusNotificationResponse);
    message.ContextStatusNotificationResponse = NULL;
    CHECK(amf_namf_mbs_transport_answers(gnb, &ctx.tmgi, &message, out, 8) == 0);

    ogs_pkbuf_free(rsp_transfer);
    ogs_pkbuf_free(fail_transfer);
    ogs_pkbuf_free(garbage);
    ogs_free(gnb);
    ogs_free(other);
}

/* TS 23.527 V18.6.0 cl. 8.3.2.2 step 1: "The AMF stores the last N2 MBS SM container (i.e. MBS Session Setup or
 * Modification Request Transfer IE defined in 3GPP TS 38.413 [11]) received from the MB-SMF during the establishment of
 * the broadcast MBS session or during a broadcast MBS session update." */
static void test_store_n2_container(void)
{
    static const uint8_t first[] = { 0x01, 0x02, 0x03 }, second[] = { 0x09 };
    amf_mbs_context_t ctx = make_context();
    ogs_pkbuf_t *a = ogs_pkbuf_alloc(NULL, sizeof(first)), *b = ogs_pkbuf_alloc(NULL, sizeof(second));

    ogs_pkbuf_put_data(a, first, sizeof(first));
    ogs_pkbuf_put_data(b, second, sizeof(second));

    CHECK(ctx.n2_mbs_sm_request == NULL);
    CHECK(amf_mbs_context_store_n2_container(&ctx, a));
    CHECK(ctx.n2_mbs_sm_request != NULL && ctx.n2_mbs_sm_request != a);
    CHECK(ctx.n2_mbs_sm_request && ctx.n2_mbs_sm_request->len == 3 && ctx.n2_mbs_sm_request->data[2] == 0x03);

    /* the later container replaces the earlier one, and the copy does not depend on the source */
    ogs_pkbuf_free(a);
    CHECK(ctx.n2_mbs_sm_request->data[0] == 0x01);
    CHECK(amf_mbs_context_store_n2_container(&ctx, b));
    CHECK(ctx.n2_mbs_sm_request && ctx.n2_mbs_sm_request->len == 1 && ctx.n2_mbs_sm_request->data[0] == 0x09);

    ogs_pkbuf_free(ctx.n2_mbs_sm_request);
    ogs_pkbuf_free(b);
}

/* The GlobalRanNodeId item of a gNB, as a ranIdList carries it */
static OpenAPI_global_ran_node_id_t *ran_id_of(const char *gnb_value, int bit_length)
{
    ogs_plmn_id_t plmn;

    ogs_plmn_id_build(&plmn, 999, 70, 2);
    return OpenAPI_global_ran_node_id_create(ogs_sbi_build_plmn_id(&plmn), NULL,
            OpenAPI_gnb_id_create(bit_length, ogs_strdup(gnb_value)), NULL, NULL, NULL, NULL, NULL);
}

static void free_ran_id_list(OpenAPI_list_t *list)
{
    OpenAPI_lnode_t *node = NULL;

    OpenAPI_list_for_each(list, node)
        OpenAPI_global_ran_node_id_free(node->data);
    OpenAPI_list_free(list);
}

static void free_involved(amf_mbs_context_t *ctx)
{
    amf_mbs_gnb_t *involved = NULL, *next = NULL;

    ogs_list_for_each_safe(&ctx->involved_gnb_list, next, involved) {
        ogs_list_remove(&ctx->involved_gnb_list, involved);
        ogs_free(involved);
    }
}

/* TS 29.518 V18.14.0 cl. 5.6.2.3: "During a broadcast MBS session restoration procedure for an NG-RAN failure with
 * restart, the MB-SMF may include one or more ranIds attibutes to request the AMF to setup the Broadcast MBS session in a
 * list of NG-RANs as identified by the NG-RAN ID(s), as specified in clause 8.3.2.3 of TS 23.527" and TS 23.527 V18.6.0
 * cl. 8.3.2.3 step 7: "The AMF sends the N2 MBS Session Setup Request message to the NG-RAN node(s) including the MBS
 * Session Setup or Modification Request Transfer IE." */
static void test_update_action_ran_id_list(void)
{
    amf_gnb_t *gnb1 = make_gnb(0x100, 22), *gnb2 = make_gnb(0x200, 22), *down = make_gnb(0x300, 22);
    amf_mbs_context_t ctx = make_context();
    amf_mbs_update_plan_t plan;
    OpenAPI_list_t *list = OpenAPI_list_create();

    ogs_list_init(&ctx.involved_gnb_list);
    gnb1->id = 1; gnb2->id = 2; down->id = 3;
    gnb1->state.ng_setup_success = true;
    gnb2->state.ng_setup_success = true;

    /* no ranIdList: every node that takes the session is modified; one that has not completed NG Setup is not reached */
    memset(&plan, 0, sizeof(plan));
    CHECK(amf_namf_mbs_update_action(&ctx, gnb1, &plan) == AMF_MBS_UPDATE_MODIFY);
    CHECK(amf_namf_mbs_update_action(&ctx, gnb2, &plan) == AMF_MBS_UPDATE_MODIFY);
    CHECK(amf_namf_mbs_update_action(&ctx, down, &plan) == AMF_MBS_UPDATE_SKIP);

    /* a node that released the session at its own request has nothing to modify */
    amf_mbs_context_involve_gnb(&ctx, gnb2);
    amf_mbs_context_find_involved_gnb(&ctx, gnb2)->release_required = true;
    CHECK(amf_namf_mbs_update_action(&ctx, gnb2, &plan) == AMF_MBS_UPDATE_SKIP);

    /* ranIdList [gNB 0x200]: that node gets a Setup (whatever it released before), no other node gets anything */
    OpenAPI_list_add(list, ran_id_of("000800", 22));
    plan.ran_id_list = list;
    CHECK(amf_namf_mbs_update_action(&ctx, gnb2, &plan) == AMF_MBS_UPDATE_SETUP);
    CHECK(amf_namf_mbs_update_action(&ctx, gnb1, &plan) == AMF_MBS_UPDATE_SKIP);
    CHECK(amf_namf_mbs_update_action(&ctx, down, &plan) == AMF_MBS_UPDATE_SKIP);

    /* noNgapSignallingInd true: no NGAP message to any node, with or without a ranIdList */
    plan.no_ngap_signalling = true;
    CHECK(amf_namf_mbs_update_action(&ctx, gnb2, &plan) == AMF_MBS_UPDATE_SKIP);
    plan.ran_id_list = NULL;
    CHECK(amf_namf_mbs_update_action(&ctx, gnb1, &plan) == AMF_MBS_UPDATE_SKIP);
    plan.no_ngap_signalling = false;
    plan.ran_id_list = list;

    /* a listed node that is not connected is not reached */
    gnb2->state.ng_setup_success = false;
    CHECK(amf_namf_mbs_update_action(&ctx, gnb2, &plan) == AMF_MBS_UPDATE_SKIP);

    free_ran_id_list(list);
    free_involved(&ctx);
    ogs_free(gnb1);
    ogs_free(gnb2);
    ogs_free(down);
}

/* TS 29.518 V18.14.0 Table 6.5.6.2.5-1 (ContextUpdateReqData): "noNgapSignallingInd | boolean | O | 0..1", and
 * TS29518_Namf_MBSBroadcast.yaml of that version: "noNgapSignallingInd:" "type: boolean" "enum: - true". The member is the
 * JSON boolean true, not the string "true". */
static void test_context_update_req_data_no_ngap_signalling_ind(void)
{
    cJSON *json = NULL;
    OpenAPI_context_update_req_data_t *parsed = NULL;
    cJSON *back = NULL;
    char *text = NULL;

    json = cJSON_Parse("{\"noNgapSignallingInd\":true}");
    parsed = OpenAPI_context_update_req_data_parseFromJSON(json);
    cJSON_Delete(json);
    CHECK(parsed != NULL);
    if (parsed) {
        CHECK(parsed->no_ngap_signalling_ind == OpenAPI_context_update_req_data_NONGAPSIGNALLINGIND__true);

        json = OpenAPI_context_update_req_data_convertToJSON(parsed);
        CHECK(json != NULL);
        if (json) {
            back = cJSON_GetObjectItemCaseSensitive(json, "noNgapSignallingInd");
            CHECK(back != NULL && cJSON_IsTrue(back));
            cJSON_Delete(json);
        }
        OpenAPI_context_update_req_data_free(parsed);
    }

    /* absent: not set, and not written */
    json = cJSON_Parse("{}");
    parsed = OpenAPI_context_update_req_data_parseFromJSON(json);
    cJSON_Delete(json);
    CHECK(parsed != NULL);
    if (parsed) {
        CHECK(parsed->no_ngap_signalling_ind == OpenAPI_context_update_req_data_NONGAPSIGNALLINGIND_NULL);
        json = OpenAPI_context_update_req_data_convertToJSON(parsed);
        text = json ? cJSON_PrintUnformatted(json) : NULL;
        CHECK(text != NULL && strstr(text, "noNgapSignallingInd") == NULL);
        if (text) ogs_free(text);
        if (json) cJSON_Delete(json);
        OpenAPI_context_update_req_data_free(parsed);
    }

    /* the old, string, form is not a boolean */
    json = cJSON_Parse("{\"noNgapSignallingInd\":\"true\"}");
    parsed = OpenAPI_context_update_req_data_parseFromJSON(json);
    cJSON_Delete(json);
    CHECK(parsed == NULL);
    if (parsed) OpenAPI_context_update_req_data_free(parsed);
}

/* A service area of TAIs of PLMN 999/70 with the given TACs (up to 4) */
static ogs_mbs_service_area_t *area_of(int n, const uint32_t *tacs)
{
    ogs_mbs_service_area_t *area = ogs_calloc(1, sizeof(*area));
    int i;

    area->tai_list = ogs_calloc(1, sizeof(*area->tai_list));
    ogs_list_init(area->tai_list);
    for (i = 0; i < n; i++) {
        ogs_tai_t *tai = ogs_calloc(1, sizeof(*tai));

        ogs_plmn_id_build(&tai->plmn_id, 999, 70, 2);
        tai->tac = ogs_msprintf("%06x", tacs[i]);
        ogs_list_add(area->tai_list, tai);
    }
    return area;
}

/* a gNB that completed NG Setup and serves the given TACs of PLMN 999/70 */
static void gnb_serves(amf_gnb_t *gnb, int n, const uint32_t *tacs)
{
    int i;

    gnb->state.ng_setup_success = true;
    gnb->num_of_supported_ta_list = n;
    for (i = 0; i < n; i++) {
        gnb->supported_ta_list[i].tac.v = tacs[i];
        gnb->supported_ta_list[i].num_of_bplmn_list = 1;
        ogs_plmn_id_build(&gnb->supported_ta_list[i].bplmn_list[0].plmn_id, 999, 70, 2);
    }
}

/* TS 23.247 V18.8.0 cl. 7.3.3 step 3: "Depending on the change of the MBS service area, the AMF may send MBS Session
 * Resource Setup to some NG-RANs in new MBS service area (see clause 7.3.1)" */
static void test_update_action_enters_area(void)
{
    static const uint32_t t1[] = { 1 }, t2[] = { 2 }, t12[] = { 1, 2 };
    amf_gnb_t *a = make_gnb(0x100, 22), *b = make_gnb(0x200, 22), *c = make_gnb(0x300, 22);
    amf_mbs_context_t ctx = make_context();
    amf_mbs_update_plan_t plan;
    ogs_mbs_service_area_t *old_area = area_of(1, t1), *new_area = area_of(2, t12);

    ogs_list_init(&ctx.involved_gnb_list);
    a->id = 1; b->id = 2; c->id = 3;
    gnb_serves(a, 1, t1);
    gnb_serves(b, 1, t2);
    gnb_serves(c, 2, t12);

    /* {1} to {1,2}: only a node that serves 2 and did not serve 1 enters, whatever n2MbsInfoChangeInd says */
    ctx.mbs_service_area = new_area;
    memset(&plan, 0, sizeof(plan));
    plan.old_area = old_area;
    CHECK(amf_namf_mbs_update_action(&ctx, a, &plan) == AMF_MBS_UPDATE_MODIFY);
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_SETUP);
    CHECK(amf_namf_mbs_update_action(&ctx, c, &plan) == AMF_MBS_UPDATE_MODIFY);
    plan.n2_info_unchanged = true;
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_SETUP);

    /* an update that does not change the area: nobody enters */
    plan.old_area = new_area;
    plan.n2_info_unchanged = false;
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_MODIFY);

    /* an old area that cannot be told by TAI (none): nobody is taken to enter */
    plan.old_area = NULL;
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_MODIFY);

    ctx.mbs_service_area = NULL;
    ogs_mbs_service_area_free(new_area);
    ogs_mbs_service_area_free(old_area);
    ogs_free(a);
    ogs_free(b);
    ogs_free(c);
}

/* TS 23.247 V18.8.0 cl. 7.3.3 step 3: "the AMF may send MBS Session Resource Setup to some NG-RANs in new MBS service
 * area (see clause 7.3.1) and MBS Session Resource Release to some other NG-RANs in old MBS service area." */
static void test_update_action_leaves_area(void)
{
    static const uint32_t t1[] = { 1 }, t2[] = { 2 }, t12[] = { 1, 2 };
    amf_gnb_t *a = make_gnb(0x100, 22), *b = make_gnb(0x200, 22), *c = make_gnb(0x300, 22);
    amf_mbs_context_t ctx = make_context();
    amf_mbs_update_plan_t plan;
    ogs_mbs_service_area_t *old_area = area_of(2, t12), *new_area = area_of(1, t1);
    amf_mbs_gnb_t *rec;

    ogs_list_init(&ctx.involved_gnb_list);
    a->id = 1; b->id = 2; c->id = 3;
    gnb_serves(a, 1, t1);
    gnb_serves(b, 1, t2);
    gnb_serves(c, 2, t12);
    amf_mbs_context_involve_gnb(&ctx, a);
    amf_mbs_context_involve_gnb(&ctx, b);
    amf_mbs_context_involve_gnb(&ctx, c);

    /* {1,2} to {1}: the node that served 2 only leaves, the others stay */
    ctx.mbs_service_area = new_area;
    memset(&plan, 0, sizeof(plan));
    plan.old_area = old_area;
    CHECK(amf_namf_mbs_update_action(&ctx, a, &plan) == AMF_MBS_UPDATE_MODIFY);
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_RELEASE);
    CHECK(amf_namf_mbs_update_action(&ctx, c, &plan) == AMF_MBS_UPDATE_MODIFY);
    plan.n2_info_unchanged = true;
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_RELEASE);

    /* a node that does not hold the session has nothing to release: it never got it, it failed its Setup, it asked for the
     * release, or it has been released already */
    plan.n2_info_unchanged = false;
    rec = amf_mbs_context_find_involved_gnb(&ctx, b);
    rec->setup_failed = true;
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_SKIP);
    rec->setup_failed = false;
    rec->release_required = true;
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_SKIP);
    rec->release_required = false;
    rec->area_released = true;
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_SKIP);
    ogs_list_remove(&ctx.involved_gnb_list, rec);
    ogs_free(rec);
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_SKIP);

    /* an update that does not change the area releases nobody */
    plan.old_area = new_area;
    rec = amf_mbs_context_find_involved_gnb(&ctx, c);
    CHECK(rec != NULL);
    CHECK(amf_namf_mbs_update_action(&ctx, c, &plan) == AMF_MBS_UPDATE_MODIFY);

    free_involved(&ctx);
    ctx.mbs_service_area = NULL;
    ogs_mbs_service_area_free(new_area);
    ogs_mbs_service_area_free(old_area);
    ogs_free(a);
    ogs_free(b);
    ogs_free(c);
}

/* TS 29.518 V18.14.0 cl. 5.6.2.3: "If the n2MbsInfoChangeInd IE is present in the request and set to "false", the AMF
 * does not need to contact NG-RAN nodes unaffected by the MBS service area change." */
static void test_update_action_info_unchanged(void)
{
    static const uint32_t t1[] = { 1 }, t2[] = { 2 }, t12[] = { 1, 2 };
    amf_gnb_t *a = make_gnb(0x100, 22), *b = make_gnb(0x200, 22), *c = make_gnb(0x300, 22);
    amf_mbs_context_t ctx = make_context();
    amf_mbs_update_plan_t plan;
    ogs_mbs_service_area_t *old_area = area_of(1, t1), *new_area = area_of(2, t12);

    ogs_list_init(&ctx.involved_gnb_list);
    a->id = 1; b->id = 2; c->id = 3;
    gnb_serves(a, 1, t1);
    gnb_serves(b, 1, t2);
    gnb_serves(c, 2, t12);

    /* the service area grows from {1} to {1,2}; the container is unchanged */
    ctx.mbs_service_area = new_area;
    memset(&plan, 0, sizeof(plan));
    plan.old_area = old_area;
    plan.n2_info_unchanged = true;
    CHECK(amf_namf_mbs_update_action(&ctx, a, &plan) == AMF_MBS_UPDATE_UNAFFECTED);  /* serves 1 only: not in the difference */
    CHECK(amf_namf_mbs_update_action(&ctx, b, &plan) == AMF_MBS_UPDATE_SETUP);       /* serves 2: enters the area */
    CHECK(amf_namf_mbs_update_action(&ctx, c, &plan) == AMF_MBS_UPDATE_MODIFY);      /* served 1 before: holds the session */

    /* the indication true, or absent: every node that takes the session is contacted */
    plan.n2_info_unchanged = false;
    CHECK(amf_namf_mbs_update_action(&ctx, a, &plan) == AMF_MBS_UPDATE_MODIFY);

    /* no change of the area at all: nobody is affected */
    plan.n2_info_unchanged = true;
    plan.old_area = new_area;
    CHECK(amf_namf_mbs_update_action(&ctx, a, &plan) == AMF_MBS_UPDATE_UNAFFECTED);
    CHECK(amf_namf_mbs_update_action(&ctx, c, &plan) == AMF_MBS_UPDATE_UNAFFECTED);

    /* an area that cannot be told by TAI (no TAI list): every node that takes the session is affected */
    ctx.mbs_service_area = ogs_calloc(1, sizeof(*ctx.mbs_service_area));
    plan.old_area = old_area;
    CHECK(amf_namf_mbs_update_action(&ctx, a, &plan) == AMF_MBS_UPDATE_MODIFY);
    ogs_free(ctx.mbs_service_area);

    ctx.mbs_service_area = NULL;
    ogs_mbs_service_area_free(new_area);
    ogs_mbs_service_area_free(old_area);
    ogs_free(a);
    ogs_free(b);
    ogs_free(c);
}

int main(int argc, const char *argv[])
{
    ogs_pkbuf_config_t config;

    ogs_core_initialize();
    ogs_pkbuf_default_init(&config);
    ogs_pkbuf_default_create(&config);
    ogs_log_install_domain(&__ogs_sbi_domain, "sbi", OGS_LOG_ERROR);
    ogs_log_install_domain(&__amf_log_domain, "amf", OGS_LOG_ERROR);
    ogs_log_install_domain(&__ogs_ngap_domain, "ngap", OGS_LOG_ERROR);
    ogs_sbi_message_init(32, 32);
    /* ogs_sbi_build_request() takes the maximum response time of a request from the configuration */
    ogs_local_conf()->time.message.duration = ogs_time_from_sec(10);

    test_n2_info_list();
    test_n2_mbs_sm_info_ran_id_key();
    test_n2_info_list_limit();
    test_status_notify();
    test_transport_messages();
    test_transport_relay();
    test_store_n2_container();
    test_update_action_ran_id_list();
    test_update_action_enters_area();
    test_update_action_leaves_area();
    test_update_action_info_unchanged();
    test_context_update_req_data_no_ngap_signalling_ind();

    ogs_sbi_message_final();
    ogs_pkbuf_default_destroy();
    ogs_core_terminate();

    printf("amf-mbs: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
