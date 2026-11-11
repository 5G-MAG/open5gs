/*
 * Copyright (C) 2019,2020 by Sukchan Lee <acetcom@gmail.com>
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

#include <poll.h>

#include "test-common.h"
#include "af/sbi-path.h"

static test_bearer_t *test_af_complete_qos_flow_modify_qfi(
        abts_case *tc, test_ue_t *test_ue, test_sess_t *sess,
        ogs_socknode_t *ngap, uint8_t qfi)
{
    int rv;
    ogs_pkbuf_t *gmmbuf = NULL;
    ogs_pkbuf_t *gsmbuf = NULL;
    ogs_pkbuf_t *sendbuf = NULL;
    ogs_pkbuf_t *recvbuf = NULL;
    test_bearer_t *qos_flow = NULL;

    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    qos_flow = test_qos_flow_find_by_qfi(sess, qfi);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(100);

    return qos_flow;
}

static test_bearer_t *test_af_complete_qos_flow_modify(
        abts_case *tc, test_ue_t *test_ue, test_sess_t *sess,
        ogs_socknode_t *ngap)
{
    return test_af_complete_qos_flow_modify_qfi(tc, test_ue, sess, ngap, 2);
}

static void test_af_qos_reference_setup(abts_case *tc, const char *msin,
        test_ue_t **test_ue, test_sess_t **sess, af_sess_t **af_sess,
        ogs_socknode_t **ngap, ogs_socknode_t **gtpu)
{
    int rv;
    ogs_pkbuf_t *gmmbuf = NULL;
    ogs_pkbuf_t *gsmbuf = NULL;
    ogs_pkbuf_t *nasbuf = NULL;
    ogs_pkbuf_t *sendbuf = NULL;
    ogs_pkbuf_t *recvbuf = NULL;
    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_sess_t *internet_sess = NULL;
    test_bearer_t *qos_flow = NULL;
    bson_t *doc = NULL;

    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    *test_ue = test_ue_add_by_suci(&mobile_identity_suci, msin);
    ogs_assert(*test_ue);

    (*test_ue)->nr_cgi.cell_id = 0x40001;

    (*test_ue)->nas.registration.tsc = 0;
    (*test_ue)->nas.registration.ksi = OGS_NAS_KSI_NO_KEY_IS_AVAILABLE;
    (*test_ue)->nas.registration.follow_on_request = 1;
    (*test_ue)->nas.registration.value =
        OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    (*test_ue)->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    (*test_ue)->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    *ngap = testngap_client(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, *ngap);

    *gtpu = test_gtpu_server(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, *gtpu);

    sendbuf = testngap_build_ng_setup_request(0x4000, 22);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_ngap_read(*ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(*test_ue, recvbuf);

    doc = test_db_new_ims(*test_ue);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_insert_ue(*test_ue, doc));

    (*test_ue)->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(*test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    (*test_ue)->registration_request_param.gmm_capability = 1;
    (*test_ue)->registration_request_param.requested_nssai = 1;
    (*test_ue)->registration_request_param.last_visited_registered_tai = 1;
    (*test_ue)->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(*test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(*test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_ngap_read(*ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(*test_ue, recvbuf);

    gmmbuf = testgmm_build_identity_response(*test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(*test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_ngap_read(*ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(*test_ue, recvbuf);

    gmmbuf = testgmm_build_authentication_response(*test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(*test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_ngap_read(*ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(*test_ue, recvbuf);

    gmmbuf = testgmm_build_security_mode_complete(*test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(*test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_ngap_read(*ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(*test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            (*test_ue)->ngap_procedure_code);

    sendbuf = testngap_build_ue_radio_capability_info_indication(*test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    sendbuf = testngap_build_initial_context_setup_response(*test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    gmmbuf = testgmm_build_registration_complete(*test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(*test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_ngap_read(*ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(*test_ue, recvbuf);

    internet_sess = test_sess_add_by_dnn_and_psi(*test_ue, "internet", 5);
    ogs_assert(internet_sess);

    internet_sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    internet_sess->ul_nas_transport_param.dnn = 1;
    internet_sess->ul_nas_transport_param.s_nssai = 1;

    internet_sess->pdu_session_establishment_param.ssc_mode = 1;
    internet_sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(internet_sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(internet_sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(*test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_ngap_read(*ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(*test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            (*test_ue)->ngap_procedure_code);

    qos_flow = test_qos_flow_find_by_qfi(internet_sess, 1);
    ogs_assert(qos_flow);
    rv = test_gtpu_send_ping(*gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(
            internet_sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_gtpu_read(*gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    *sess = test_sess_add_by_dnn_and_psi(*test_ue, "ims", 6);
    ogs_assert(*sess);

    (*sess)->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    (*sess)->ul_nas_transport_param.dnn = 1;
    (*sess)->ul_nas_transport_param.s_nssai = 1;

    (*sess)->pdu_session_establishment_param.ssc_mode = 1;
    (*sess)->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(*sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(*sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(*test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_ngap_read(*ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(*test_ue, recvbuf);

    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(*sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(*ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    *af_sess = af_sess_add_by_ue_address(&(*sess)->ue_ip);
    ogs_assert(*af_sess);

    (*af_sess)->supi = ogs_strdup((*test_ue)->supi);
    ogs_assert((*af_sess)->supi);

    (*af_sess)->dnn = ogs_strdup((*sess)->dnn);
    ogs_assert((*af_sess)->dnn);

    af_local_discover_and_send(
            OpenAPI_service_name_nbsf_management,
            *af_sess, NULL,
            af_nbsf_management_build_discover);

    ogs_msleep(100);
}

/* The SMF releases the QoS flow of the PCC rule that the PCF removed */
static void test_af_qos_flow_release_qfi(
        abts_case *tc, test_ue_t *test_ue, test_sess_t *sess,
        ogs_socknode_t *ngap, uint8_t qfi)
{
    int rv;
    ogs_pkbuf_t *gmmbuf = NULL;
    ogs_pkbuf_t *gsmbuf = NULL;
    ogs_pkbuf_t *sendbuf = NULL;
    ogs_pkbuf_t *recvbuf = NULL;
    test_bearer_t *qos_flow = NULL;

    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    qos_flow = test_qos_flow_find_by_qfi(sess, qfi);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_release_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    test_bearer_remove(qos_flow);

    ogs_msleep(100);
}

static void test_af_qos_flow_release(
        abts_case *tc, test_ue_t *test_ue, test_sess_t *sess,
        ogs_socknode_t *ngap)
{
    test_af_qos_flow_release_qfi(tc, test_ue, sess, ngap, 2);
}

static void test_af_qos_reference_delete(
        abts_case *tc, test_ue_t *test_ue, test_sess_t *sess,
        af_sess_t *af_sess, ogs_socknode_t *ngap)
{
    af_local_send_to_pcf(af_sess, NULL,
            af_npcf_policyauthorization_build_delete);

    test_af_qos_flow_release(tc, test_ue, sess, ngap);
}

static void test_af_qos_reference_cleanup(
        abts_case *tc, test_ue_t *test_ue,
        ogs_socknode_t *ngap, ogs_socknode_t *gtpu)
{
    int rv;
    ogs_pkbuf_t *gmmbuf = NULL;
    ogs_pkbuf_t *sendbuf = NULL;
    ogs_pkbuf_t *recvbuf = NULL;

    gmmbuf = testgmm_build_de_registration_request(test_ue, 1, true, true);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    ABTS_INT_EQUAL(tc, OGS_OK, test_db_remove_ue(test_ue));

    testgnb_gtpu_close(gtpu);
    testgnb_ngap_close(ngap);

    test_ue_remove(test_ue);
}

static void test1_func(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *ngap;
    ogs_socknode_t *gtpu;
    ogs_pkbuf_t *gmmbuf;
    ogs_pkbuf_t *gsmbuf;
    ogs_pkbuf_t *nasbuf;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    ogs_ngap_message_t message;
    int i;

    uint8_t tmp[OGS_HUGE_LEN];
    char *_gtp_payload = "34ff0024"
        "0000000100000085 010002004500001c 0c0b000040015a7a 0a2d00010a2d0002"
        "00000964cd7c291f";

    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;

    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    bson_t *doc = NULL;

    /* Setup Test UE & Session Context */
    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    test_ue = test_ue_add_by_suci(&mobile_identity_suci, "0000000001");
    ogs_assert(test_ue);

    test_ue->nr_cgi.cell_id = 0x40001;

    test_ue->nas.registration.tsc = 0;
    test_ue->nas.registration.ksi = OGS_NAS_KSI_NO_KEY_IS_AVAILABLE;
    test_ue->nas.registration.follow_on_request = 1;
    test_ue->nas.registration.value = OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    test_ue->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    test_ue->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    /* gNB connects to AMF */
    ngap = testngap_client(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, ngap);

    /* gNB connects to UPF */
    gtpu = test_gtpu_server(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, gtpu);

    /* Send NG-Setup Reqeust */
    sendbuf = testngap_build_ng_setup_request(0x4000, 22);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive NG-Setup Response */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /********** Insert Subscriber in Database */
    doc = test_db_new_ims(test_ue);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_insert_ue(test_ue, doc));

    /* Send Registration request */
    test_ue->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    test_ue->registration_request_param.gmm_capability = 1;
    test_ue->registration_request_param.requested_nssai = 1;
    test_ue->registration_request_param.last_visited_registered_tai = 1;
    test_ue->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Identity request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Identity response */
    gmmbuf = testgmm_build_identity_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);

    /* Send UERadioCapabilityInfoIndication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send GTP-U ICMP Packet */
    qos_flow = test_qos_flow_find_by_qfi(sess, 1);
    ogs_assert(qos_flow);
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U ICMP Packet */
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U Router Solicitation */
    rv = test_gtpu_send_slacc_rs(gtpu, qos_flow);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U Router Advertisement */
    recvbuf = test_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testgtpu_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "ims", 6);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Add AF-Session */
    af_sess = af_sess_add_by_ue_address(&sess->ue_ip);
    ogs_assert(af_sess);

    af_sess->supi = ogs_strdup(test_ue->supi);
    ogs_assert(af_sess->supi);

    af_sess->dnn = ogs_strdup(sess->dnn);
    ogs_assert(af_sess->dnn);

    af_local_discover_and_send(
            OpenAPI_service_name_nbsf_management,
            af_sess, NULL,
            af_nbsf_management_build_discover);

    /* Wait for PCF-Discovery */
    ogs_msleep(100);

    /* Send AF-Session : CREATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99; /* For ping test */

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Send GTP-U ICMP Packet */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Waiting for creating dedicated QoS flow in PFCP protocol */
    ogs_msleep(100);

    /* Send AF-Session : UPDATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 2;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_update);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Send PDU session modification request */
    sess->pti = 8;
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_modification_request(
        qos_flow,
        0,
        OGS_NAS_QOS_CODE_MODIFY_EXISTING_QOS_RULE_AND_ADD_PACKET_FILTERS,
        OGS_NAS_MODIFY_NEW_QOS_FLOW_DESCRIPTION);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Send PDU session modification request */
    sess->pti = 9;
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_modification_request(
        qos_flow,
        0,
        OGS_NAS_QOS_CODE_MODIFY_EXISTING_QOS_RULE_AND_REPLACE_ALL_PACKET_FILTERS,
        0);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Send PDU session modification request */
    sess->pti = 10;
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_modification_request(
        qos_flow,
        OGS_5GSM_CAUSE_REGULAR_DEACTIVATION,
        OGS_NAS_QOS_CODE_MODIFY_EXISTING_QOS_RULE_AND_DELETE_PACKET_FILTERS,
        0);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Waiting for modifying dedicated QoS flow */
    ogs_msleep(100);

    /* Send AF-Session : DELETE */
    af_local_send_to_pcf(af_sess, NULL,
            af_npcf_policyauthorization_build_delete);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_release_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Test Bearer Remove */
    test_bearer_remove(qos_flow);

    /* Send De-registration request */
    gmmbuf = testgmm_build_de_registration_request(test_ue, 1, true, true);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_remove_ue(test_ue));

    /* gNB disonncect from UPF */
    testgnb_gtpu_close(gtpu);

    /* gNB disonncect from AMF */
    testgnb_ngap_close(ngap);

    /* Clear Test UE Context */
    test_ue_remove(test_ue);
}

static void test2_func(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *ngap;
    ogs_socknode_t *gtpu;
    ogs_pkbuf_t *gmmbuf;
    ogs_pkbuf_t *gsmbuf;
    ogs_pkbuf_t *nasbuf;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    ogs_ngap_message_t message;
    int i;

    uint8_t tmp[OGS_HUGE_LEN];
    char *_gtp_payload = "34ff0024"
        "0000000100000085 010002004500001c 0c0b000040015a7a 0a2d00010a2d0002"
        "00000964cd7c291f";

    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;

    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    bson_t *doc = NULL;

    /* Setup Test UE & Session Context */
    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    test_ue = test_ue_add_by_suci(&mobile_identity_suci, "0000000002");
    ogs_assert(test_ue);

    test_ue->nr_cgi.cell_id = 0x40001;

    test_ue->nas.registration.tsc = 0;
    test_ue->nas.registration.ksi = OGS_NAS_KSI_NO_KEY_IS_AVAILABLE;
    test_ue->nas.registration.follow_on_request = 1;
    test_ue->nas.registration.value = OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    test_ue->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    test_ue->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    /* gNB connects to AMF */
    ngap = testngap_client(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, ngap);

    /* gNB connects to UPF */
    gtpu = test_gtpu_server(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, gtpu);

    /* Send NG-Setup Reqeust */
    sendbuf = testngap_build_ng_setup_request(0x4000, 22);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive NG-Setup Response */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /********** Insert Subscriber in Database */
    doc = test_db_new_session(test_ue);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_insert_ue(test_ue, doc));

    /* Send Registration request */
    test_ue->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    test_ue->registration_request_param.gmm_capability = 1;
    test_ue->registration_request_param.requested_nssai = 1;
    test_ue->registration_request_param.last_visited_registered_tai = 1;
    test_ue->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Identity request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Identity response */
    gmmbuf = testgmm_build_identity_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);

    /* Send UERadioCapabilityInfoIndication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send GTP-U ICMP Packet */
    qos_flow = test_qos_flow_find_by_qfi(sess, 1);
    ogs_assert(qos_flow);
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U ICMP Packet */
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "ims", 6);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Send GTP-U ICMP Packet */
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Add AF-Session */
    af_sess = af_sess_add_by_ue_address(&sess->ue_ip);
    ogs_assert(af_sess);

    af_sess->supi = ogs_strdup(test_ue->supi);
    ogs_assert(af_sess->supi);

    af_sess->dnn = ogs_strdup(sess->dnn);
    ogs_assert(af_sess->dnn);

    af_local_discover_and_send(
            OpenAPI_service_name_nbsf_management,
            af_sess, NULL,
            af_nbsf_management_build_discover);

    /* Wait for PCF-Discovery */
    ogs_msleep(100);

    /* Send AF-Session : CREATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 0;
    af_param.flow_type = 1;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Send PDU Session release request */
    sess->pti = 9;
    sess->ul_nas_transport_param.request_type = 0;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_release_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceReleaseCommand +
     * DL NAS transport +
     * PDU session release command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceRelease,
            test_ue->ngap_procedure_code);

    /* Send PDUSessionResourceReleaseResponse */
    sendbuf = testngap_build_pdu_session_resource_release_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send UplinkNASTransport +
     * UL NAS trasnport +
     * PDU session resource release complete */
    sess->ul_nas_transport_param.request_type = 0;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_release_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource release complete */
    ogs_msleep(100);

    /* Test Session Remove */
    test_sess_remove(sess);

    /* Send UEContextReleaseRequest */
    sendbuf = testngap_build_ue_context_release_request(test_ue,
            NGAP_Cause_PR_radioNetwork, NGAP_CauseRadioNetwork_user_inactivity,
            true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_remove_ue(test_ue));

    /* gNB disonncect from UPF */
    testgnb_gtpu_close(gtpu);

    /* gNB disonncect from AMF */
    testgnb_ngap_close(ngap);

    /* Clear Test UE Context */
    test_ue_remove(test_ue);
}

static void test3_func(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *ngap;
    ogs_socknode_t *gtpu;
    ogs_pkbuf_t *gmmbuf;
    ogs_pkbuf_t *gsmbuf;
    ogs_pkbuf_t *nasbuf;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    ogs_ngap_message_t message;
    int i;

    uint8_t tmp[OGS_HUGE_LEN];
    char *_gtp_payload = "34ff0024"
        "0000000100000085 010002004500001c 0c0b000040015a7a 0a2d00010a2d0002"
        "00000964cd7c291f";

    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;

    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    bson_t *doc = NULL;

    /* Setup Test UE & Session Context */
    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    test_ue = test_ue_add_by_suci(&mobile_identity_suci, "0000000003");
    ogs_assert(test_ue);

    test_ue->nr_cgi.cell_id = 0x40001;

    test_ue->nas.registration.tsc = 0;
    test_ue->nas.registration.ksi = OGS_NAS_KSI_NO_KEY_IS_AVAILABLE;
    test_ue->nas.registration.follow_on_request = 1;
    test_ue->nas.registration.value = OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    test_ue->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    test_ue->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    /* gNB connects to AMF */
    ngap = testngap_client(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, ngap);

    /* gNB connects to UPF */
    gtpu = test_gtpu_server(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, gtpu);

    /* Send NG-Setup Reqeust */
    sendbuf = testngap_build_ng_setup_request(0x4000, 22);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive NG-Setup Response */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /********** Insert Subscriber in Database */
    doc = test_db_new_ims(test_ue);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_insert_ue(test_ue, doc));

    /* Send Registration request */
    test_ue->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    test_ue->registration_request_param.gmm_capability = 1;
    test_ue->registration_request_param.requested_nssai = 1;
    test_ue->registration_request_param.last_visited_registered_tai = 1;
    test_ue->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Identity request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Identity response */
    gmmbuf = testgmm_build_identity_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);

    /* Send UERadioCapabilityInfoIndication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send GTP-U ICMP Packet */
    qos_flow = test_qos_flow_find_by_qfi(sess, 1);
    ogs_assert(qos_flow);
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U ICMP Packet */
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U Router Solicitation */
    rv = test_gtpu_send_slacc_rs(gtpu, qos_flow);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U Router Advertisement */
    recvbuf = test_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testgtpu_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "ims", 6);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Add AF-Session */
    af_sess = af_sess_add_by_ue_address(&sess->ue_ip);
    ogs_assert(af_sess);

    af_sess->supi = ogs_strdup(test_ue->supi);
    ogs_assert(af_sess->supi);

    af_sess->dnn = ogs_strdup(sess->dnn);
    ogs_assert(af_sess->dnn);

    af_local_discover_and_send(
            OpenAPI_service_name_nbsf_management,
            af_sess, NULL,
            af_nbsf_management_build_discover);

    /* Wait for PCF-Discovery */
    ogs_msleep(100);

    /* Send AF-Session : CREATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_CONTROL;
    af_param.qos_type = 0;
    af_param.flow_type = 1;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    /* Wait for AF-Session */
    ogs_msleep(100);

    /* Send AF-Session : DELETE */
    af_local_send_to_pcf(af_sess, NULL,
            af_npcf_policyauthorization_build_delete);

    /* Wait for AF-Session */
    ogs_msleep(100);

    /* Send PDU Session release request */
    sess->pti = 38;
    sess->ul_nas_transport_param.request_type = 0;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_release_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceReleaseCommand +
     * DL NAS transport +
     * PDU session release command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceRelease,
            test_ue->ngap_procedure_code);

    /* Send PDUSessionResourceReleaseResponse */
    sendbuf = testngap_build_pdu_session_resource_release_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send UplinkNASTransport +
     * UL NAS trasnport +
     * PDU session resource release complete */
    sess->ul_nas_transport_param.request_type = 0;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_release_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource release complete */
    ogs_msleep(100);

    /* Test Session Remove */
    test_sess_remove(sess);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "ims", 7);
    ogs_assert(sess);
    sess->pti = 39;

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* In ./tests/vonr/af-test/test3_func()
     *
     * 1. Send PDU session establishment request
     * 2. Receive PDU session establishment accept
     * 3. Send PDUSessionResourceSetupResponse
     * 4. Send De-registration request
     * 5. SMF->AMF : RESPONSE /nsmf-pdusession/v1/sm-contexts/3/release
     * 6. SMF->AMF : RESPONSE /nsmf-pdusession/v1/sm-contexts/1/modify
     *
     * 01/01 23:28:53.736: [sbi] ERROR: SBI running [1]
     * (../lib/sbi/context.c:1161)
     * 01/01 23:28:53.736: [amf] ERROR: Session has already been removed
     * (../src/amf/amf-sm.c:424)
     *
     * We need the following sleep to avoid the above situation.
     */
    ogs_msleep(100);

    /* Send De-registration request */
    gmmbuf = testgmm_build_de_registration_request(test_ue, 1, true, true);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_remove_ue(test_ue));

    /* gNB disonncect from UPF */
    testgnb_gtpu_close(gtpu);

    /* gNB disonncect from AMF */
    testgnb_ngap_close(ngap);

    /* Clear Test UE Context */
    test_ue_remove(test_ue);
}

static void test4_func(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *ngap;
    ogs_socknode_t *gtpu;
    ogs_pkbuf_t *gmmbuf;
    ogs_pkbuf_t *gsmbuf;
    ogs_pkbuf_t *nasbuf;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    ogs_ngap_message_t message;
    int i;

    uint8_t tmp[OGS_HUGE_LEN];
    char *_gtp_payload = "34ff0024"
        "0000000100000085 010002004500001c 0c0b000040015a7a 0a2d00010a2d0002"
        "00000964cd7c291f";

    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;

    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    bson_t *doc = NULL;

    /* Setup Test UE & Session Context */
    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    test_ue = test_ue_add_by_suci(&mobile_identity_suci, "0000000004");
    ogs_assert(test_ue);

    test_ue->nr_cgi.cell_id = 0x40001;

    test_ue->nas.registration.tsc = 0;
    test_ue->nas.registration.ksi = OGS_NAS_KSI_NO_KEY_IS_AVAILABLE;
    test_ue->nas.registration.follow_on_request = 1;
    test_ue->nas.registration.value = OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    test_ue->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    test_ue->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    /* gNB connects to AMF */
    ngap = testngap_client(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, ngap);

    /* gNB connects to UPF */
    gtpu = test_gtpu_server(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, gtpu);

    /* Send NG-Setup Reqeust */
    sendbuf = testngap_build_ng_setup_request(0x4000, 22);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive NG-Setup Response */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /********** Insert Subscriber in Database */
    doc = test_db_new_ims(test_ue);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_insert_ue(test_ue, doc));

    /* Send Registration request */
    test_ue->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    test_ue->registration_request_param.gmm_capability = 1;
    test_ue->registration_request_param.requested_nssai = 1;
    test_ue->registration_request_param.last_visited_registered_tai = 1;
    test_ue->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Identity request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Identity response */
    gmmbuf = testgmm_build_identity_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);

    /* Send UERadioCapabilityInfoIndication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess);
    sess->pti = 6;

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "ims", 6);
    ogs_assert(sess);
    sess->pti = 7;

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU Session release request */
    sess = test_sess_find_by_psi(test_ue, 5);
    sess->pti = 8;

    sess->ul_nas_transport_param.request_type = 0;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_release_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceReleaseCommand +
     * DL NAS transport +
     * PDU session release command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceRelease,
            test_ue->ngap_procedure_code);

    /* Send PDUSessionResourceReleaseResponse */
    sendbuf = testngap_build_pdu_session_resource_release_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource release complete */
    ogs_msleep(100);

    /* Test Session Remove */
    test_sess_remove(sess);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess);
    sess->pti = 9;

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Add AF-Session */
    sess = test_sess_find_by_psi(test_ue, 6);

    af_sess = af_sess_add_by_ue_address(&sess->ue_ip);
    ogs_assert(af_sess);

    af_sess->supi = ogs_strdup(test_ue->supi);
    ogs_assert(af_sess->supi);

    af_sess->dnn = ogs_strdup(sess->dnn);
    ogs_assert(af_sess->dnn);

    af_local_discover_and_send(
            OpenAPI_service_name_nbsf_management,
            af_sess, NULL,
            af_nbsf_management_build_discover);

    /* Wait for PCF-Discovery */
    ogs_msleep(100);

    /* Send AF-Session : CREATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 1;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Send AF-Session : DELETE */
    af_local_send_to_pcf(af_sess, NULL,
            af_npcf_policyauthorization_build_delete);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_release_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Test Bearer Remove */
    test_bearer_remove(qos_flow);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Send UEContextReleaseRequest */
    sendbuf = testngap_build_ue_context_release_request(test_ue,
            NGAP_Cause_PR_radioNetwork, NGAP_CauseRadioNetwork_user_inactivity,
            true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    sess = test_sess_find_by_psi(test_ue, 5);
    ogs_assert(sess);
    test_sess_remove(sess);
    sess = test_sess_find_by_psi(test_ue, 6);
    ogs_assert(sess);
    test_sess_remove(sess);

    /* Send Registration request */
    memset(&test_ue->registration_request_param, 0,
            sizeof(test_ue->registration_request_param));
    test_ue->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    test_ue->registration_request_param.gmm_capability = 1;
    test_ue->registration_request_param.requested_nssai = 1;
    test_ue->registration_request_param.last_visited_registered_tai = 1;
    test_ue->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);

    /* Send UERadioCapabilityInfoIndication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess);
    sess->pti = 9;

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send De-registration request */
    gmmbuf = testgmm_build_de_registration_request(test_ue, 1, true, true);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_remove_ue(test_ue));

    /* gNB disonncect from UPF */
    testgnb_gtpu_close(gtpu);

    /* gNB disonncect from AMF */
    testgnb_ngap_close(ngap);

    /* Clear Test UE Context */
    test_ue_remove(test_ue);
}

static void test5_func(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *ngap;
    ogs_socknode_t *gtpu;
    ogs_pkbuf_t *gmmbuf;
    ogs_pkbuf_t *gsmbuf;
    ogs_pkbuf_t *nasbuf;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    ogs_ngap_message_t message;
    int i;

    uint8_t tmp[OGS_HUGE_LEN];
    char *_gtp_payload = "34ff0024"
        "0000000100000085 010002004500001c 0c0b000040015a7a 0a2d00010a2d0002"
        "00000964cd7c291f";

    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;

    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    bson_t *doc = NULL;

    /* Setup Test UE & Session Context */
    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    test_ue = test_ue_add_by_suci(&mobile_identity_suci, "0000000005");
    ogs_assert(test_ue);

    test_ue->nr_cgi.cell_id = 0x40001;

    test_ue->nas.registration.tsc = 0;
    test_ue->nas.registration.ksi = OGS_NAS_KSI_NO_KEY_IS_AVAILABLE;
    test_ue->nas.registration.follow_on_request = 1;
    test_ue->nas.registration.value = OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    test_ue->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    test_ue->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    /* gNB connects to AMF */
    ngap = testngap_client(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, ngap);

    /* gNB connects to UPF */
    gtpu = test_gtpu_server(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, gtpu);

    /* Send NG-Setup Reqeust */
    sendbuf = testngap_build_ng_setup_request(0x4000, 22);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive NG-Setup Response */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /********** Insert Subscriber in Database */
    doc = test_db_new_ims(test_ue);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_insert_ue(test_ue, doc));

    /* Send Registration request */
    test_ue->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    test_ue->registration_request_param.gmm_capability = 1;
    test_ue->registration_request_param.requested_nssai = 1;
    test_ue->registration_request_param.last_visited_registered_tai = 1;
    test_ue->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Identity request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Identity response */
    gmmbuf = testgmm_build_identity_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);

    /* Send UERadioCapabilityInfoIndication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send GTP-U ICMP Packet */
    qos_flow = test_qos_flow_find_by_qfi(sess, 1);
    ogs_assert(qos_flow);
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U ICMP Packet */
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U Router Solicitation */
    rv = test_gtpu_send_slacc_rs(gtpu, qos_flow);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U Router Advertisement */
    recvbuf = test_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testgtpu_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "ims", 6);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send UEContextReleaseRequest */
    sendbuf = testngap_build_ue_context_release_request(test_ue,
            NGAP_Cause_PR_radioNetwork, NGAP_CauseRadioNetwork_user_inactivity,
            true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Add AF-Session */
    af_sess = af_sess_add_by_ue_address(&sess->ue_ip);
    ogs_assert(af_sess);

    af_sess->supi = ogs_strdup(test_ue->supi);
    ogs_assert(af_sess->supi);

    af_sess->dnn = ogs_strdup(sess->dnn);
    ogs_assert(af_sess->dnn);

    af_local_discover_and_send(
            OpenAPI_service_name_nbsf_management,
            af_sess, NULL,
            af_nbsf_management_build_discover);

    /* Wait for PCF-Discovery */
    ogs_msleep(100);

    /* Send AF-Session : CREATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99; /* For ping test */

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    /* Receive NG-Paging */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_Paging,
            test_ue->ngap_procedure_code);

    /*
     * Send InitialUEMessage +
     * Service request
     *  - Type: Mobile terminated services(2)
     *  - PDU Session Status
     */
    test_ue->service_request_param.pdu_session_status = 1;
    test_ue->service_request_param.
            psimask.pdu_session_status = (1 << 5 | 1 << 6);
    nasbuf = testgmm_build_service_request(
            test_ue, OGS_NAS_SERVICE_TYPE_MOBILE_TERMINATED_SERVICES,
            NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    test_ue->service_request_param.pdu_session_status = 0;
    gmmbuf = testgmm_build_service_request(
            test_ue, OGS_NAS_SERVICE_TYPE_MOBILE_TERMINATED_SERVICES,
            nasbuf, true, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, true, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Service accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);
    ABTS_INT_EQUAL(tc, 0x6000, test_ue->pdu_session_status);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Configuration update complete */
    gmmbuf = testgmm_build_configuration_update_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send UEContextReleaseRequest */
    sendbuf = testngap_build_ue_context_release_request(test_ue,
            NGAP_Cause_PR_radioNetwork, NGAP_CauseRadioNetwork_user_inactivity,
            true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send AF-Session : UPDATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 2;
    af_param.flow_type = 1;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_update);

    /* Receive NG-Paging */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_Paging,
            test_ue->ngap_procedure_code);

    /*
     * Send InitialUEMessage +
     * Service request
     *  - Type: Mobile terminated services(2)
     *  - PDU Session Status
     */
    test_ue->service_request_param.pdu_session_status = 1;
    test_ue->service_request_param.
            psimask.pdu_session_status = (1 << 5 | 1 << 6);
    nasbuf = testgmm_build_service_request(
            test_ue, OGS_NAS_SERVICE_TYPE_MOBILE_TERMINATED_SERVICES,
            NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    test_ue->service_request_param.pdu_session_status = 0;
    gmmbuf = testgmm_build_service_request(
            test_ue, OGS_NAS_SERVICE_TYPE_MOBILE_TERMINATED_SERVICES,
            nasbuf, true, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, true, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Service accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);
    ABTS_INT_EQUAL(tc, 0x6000, test_ue->pdu_session_status);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Configuration update complete */
    gmmbuf = testgmm_build_configuration_update_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send UEContextReleaseRequest */
    sendbuf = testngap_build_ue_context_release_request(test_ue,
            NGAP_Cause_PR_radioNetwork, NGAP_CauseRadioNetwork_user_inactivity,
            true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send AF-Session : DELETE */
    af_local_send_to_pcf(af_sess, NULL,
            af_npcf_policyauthorization_build_delete);

    /* Receive NG-Paging */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_Paging,
            test_ue->ngap_procedure_code);

    /*
     * Send InitialUEMessage +
     * Service request
     *  - Type: Mobile terminated services(2)
     *  - PDU Session Status
     */
    test_ue->service_request_param.pdu_session_status = 1;
    test_ue->service_request_param.
            psimask.pdu_session_status = (1 << 5 | 1 << 6);
    nasbuf = testgmm_build_service_request(
            test_ue, OGS_NAS_SERVICE_TYPE_MOBILE_TERMINATED_SERVICES,
            NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    test_ue->service_request_param.pdu_session_status = 0;
    gmmbuf = testgmm_build_service_request(
            test_ue, OGS_NAS_SERVICE_TYPE_MOBILE_TERMINATED_SERVICES,
            nasbuf, true, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, true, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Service accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);
    ABTS_INT_EQUAL(tc, 0x6000, test_ue->pdu_session_status);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_release_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Test Bearer Remove */
    test_bearer_remove(qos_flow);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Configuration update complete */
    gmmbuf = testgmm_build_configuration_update_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send De-registration request */
    gmmbuf = testgmm_build_de_registration_request(test_ue, 1, true, true);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_remove_ue(test_ue));

    /* gNB disonncect from UPF */
    testgnb_gtpu_close(gtpu);

    /* gNB disonncect from AMF */
    testgnb_ngap_close(ngap);

    /* Clear Test UE Context */
    test_ue_remove(test_ue);
}

static void test6_func(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *ngap;
    ogs_socknode_t *gtpu;
    ogs_pkbuf_t *gmmbuf;
    ogs_pkbuf_t *gsmbuf;
    ogs_pkbuf_t *nasbuf;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    ogs_ngap_message_t message;
    int i;

    uint8_t tmp[OGS_HUGE_LEN];
    char *_gtp_payload = "34ff0024"
        "0000000100000085 010002004500001c 0c0b000040015a7a 0a2d00010a2d0002"
        "00000964cd7c291f";

    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;

    af_sess_t *af_sess1 = NULL, *af_sess2 = NULL;
    af_npcf_policyauthorization_param_t af_param;

    bson_t *doc = NULL;

    /* Setup Test UE & Session Context */
    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    test_ue = test_ue_add_by_suci(&mobile_identity_suci, "0000000006");
    ogs_assert(test_ue);

    test_ue->nr_cgi.cell_id = 0x40001;

    test_ue->nas.registration.tsc = 0;
    test_ue->nas.registration.ksi = OGS_NAS_KSI_NO_KEY_IS_AVAILABLE;
    test_ue->nas.registration.follow_on_request = 1;
    test_ue->nas.registration.value = OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    test_ue->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    test_ue->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    /* gNB connects to AMF */
    ngap = testngap_client(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, ngap);

    /* gNB connects to UPF */
    gtpu = test_gtpu_server(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, gtpu);

    /* Send NG-Setup Reqeust */
    sendbuf = testngap_build_ng_setup_request(0x4000, 22);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive NG-Setup Response */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /********** Insert Subscriber in Database */
    doc = test_db_new_ims(test_ue);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_insert_ue(test_ue, doc));

    /* Send Registration request */
    test_ue->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    test_ue->registration_request_param.gmm_capability = 1;
    test_ue->registration_request_param.requested_nssai = 1;
    test_ue->registration_request_param.last_visited_registered_tai = 1;
    test_ue->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Identity request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Identity response */
    gmmbuf = testgmm_build_identity_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);

    /* Send UERadioCapabilityInfoIndication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send GTP-U ICMP Packet */
    qos_flow = test_qos_flow_find_by_qfi(sess, 1);
    ogs_assert(qos_flow);
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U ICMP Packet */
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U Router Solicitation */
    rv = test_gtpu_send_slacc_rs(gtpu, qos_flow);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U Router Advertisement */
    recvbuf = test_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testgtpu_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "ims", 6);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Add AF-Session */
    af_sess1 = af_sess_add_by_ue_address(&sess->ue_ip);
    ogs_assert(af_sess1);

    af_sess1->supi = ogs_strdup(test_ue->supi);
    ogs_assert(af_sess1->supi);

    af_sess1->dnn = ogs_strdup(sess->dnn);
    ogs_assert(af_sess1->dnn);

    af_local_discover_and_send(
            OpenAPI_service_name_nbsf_management,
            af_sess1, NULL,
            af_nbsf_management_build_discover);

    /* Wait for PCF-Discovery */
    ogs_msleep(100);

    /* Send AF-Session : CREATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess1, &af_param,
            af_npcf_policyauthorization_build_create);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Add AF-Session */
    af_sess2 = af_sess_add_by_ue_address(&sess->ue_ip);
    ogs_assert(af_sess2);

    af_sess2->supi = ogs_strdup(test_ue->supi);
    ogs_assert(af_sess2->supi);

    af_sess2->dnn = ogs_strdup(sess->dnn);
    ogs_assert(af_sess2->dnn);

    af_local_discover_and_send(
            OpenAPI_service_name_nbsf_management,
            af_sess2, NULL,
            af_nbsf_management_build_discover);

    /* Wait for PCF-Discovery */
    ogs_msleep(100);

    /* Send AF-Session : CREATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 2;

    af_local_send_to_pcf(af_sess2, &af_param,
            af_npcf_policyauthorization_build_create);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 3);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Send AF-Session : DELETE */
    af_local_send_to_pcf(af_sess1, NULL,
            af_npcf_policyauthorization_build_delete);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_release_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Test Bearer Remove */
    test_bearer_remove(qos_flow);

    /* Send AF-Session : DELETE */
    af_local_send_to_pcf(af_sess2, NULL,
            af_npcf_policyauthorization_build_delete);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 3);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_release_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Test Bearer Remove */
    test_bearer_remove(qos_flow);

    /* Send De-registration request */
    gmmbuf = testgmm_build_de_registration_request(test_ue, 1, true, true);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_remove_ue(test_ue));

    /* gNB disonncect from UPF */
    testgnb_gtpu_close(gtpu);

    /* gNB disonncect from AMF */
    testgnb_ngap_close(ngap);

    /* Clear Test UE Context */
    test_ue_remove(test_ue);
}

static void test7_func(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *ngap;
    ogs_socknode_t *gtpu;
    ogs_pkbuf_t *gmmbuf;
    ogs_pkbuf_t *gsmbuf;
    ogs_pkbuf_t *nasbuf;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    ogs_ngap_message_t message;
    int i;

    uint8_t tmp[OGS_HUGE_LEN];
    char *_gtp_payload = "34ff0024"
        "0000000100000085 010002004500001c 0c0b000040015a7a 0a2d00010a2d0002"
        "00000964cd7c291f";

    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;

    af_sess_t *af_sess = NULL, *af_sess2 = NULL;
    af_npcf_policyauthorization_param_t af_param;

    bson_t *doc = NULL;

    /* Setup Test UE & Session Context */
    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    test_ue = test_ue_add_by_suci(&mobile_identity_suci, "0000000007");
    ogs_assert(test_ue);

    test_ue->nr_cgi.cell_id = 0x40001;

    test_ue->nas.registration.tsc = 0;
    test_ue->nas.registration.ksi = OGS_NAS_KSI_NO_KEY_IS_AVAILABLE;
    test_ue->nas.registration.follow_on_request = 1;
    test_ue->nas.registration.value = OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    test_ue->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    test_ue->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    /* gNB connects to AMF */
    ngap = testngap_client(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, ngap);

    /* gNB connects to UPF */
    gtpu = test_gtpu_server(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, gtpu);

    /* Send NG-Setup Reqeust */
    sendbuf = testngap_build_ng_setup_request(0x4000, 22);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive NG-Setup Response */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /********** Insert Subscriber in Database */
    doc = test_db_new_ims(test_ue);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_insert_ue(test_ue, doc));

    /* Send Registration request */
    test_ue->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    test_ue->registration_request_param.gmm_capability = 1;
    test_ue->registration_request_param.requested_nssai = 1;
    test_ue->registration_request_param.last_visited_registered_tai = 1;
    test_ue->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Identity request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Identity response */
    gmmbuf = testgmm_build_identity_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);

    /* Send UERadioCapabilityInfoIndication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send GTP-U ICMP Packet */
    qos_flow = test_qos_flow_find_by_qfi(sess, 1);
    ogs_assert(qos_flow);
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U ICMP Packet */
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U ICMP Packet */
    recvbuf = testgnb_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    ogs_pkbuf_free(recvbuf);

    /* Send GTP-U Router Solicitation */
    rv = test_gtpu_send_slacc_rs(gtpu, qos_flow);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive GTP-U Router Advertisement */
    recvbuf = test_gtpu_read(gtpu);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testgtpu_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess = test_sess_add_by_dnn_and_psi(test_ue, "ims", 6);
    ogs_assert(sess);

    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess->ul_nas_transport_param.dnn = 1;
    sess->ul_nas_transport_param.s_nssai = 1;

    sess->pdu_session_establishment_param.ssc_mode = 1;
    sess->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Add AF-Session */
    af_sess = af_sess_add_by_ue_address(&sess->ue_ip);
    ogs_assert(af_sess);

    af_sess->supi = ogs_strdup(test_ue->supi);
    ogs_assert(af_sess->supi);

    af_sess->dnn = ogs_strdup(sess->dnn);
    ogs_assert(af_sess->dnn);

    af_local_discover_and_send(
            OpenAPI_service_name_nbsf_management,
            af_sess, NULL,
            af_nbsf_management_build_discover);

    /* Wait for PCF-Discovery */
    ogs_msleep(100);

    /* Send AF-Session : CREATE */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99; /* For ping test */
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PCF-Discovery */
    ogs_msleep(100);

    /* Send AF-Session : DELETE */
    af_local_send_to_pcf(af_sess, NULL,
            af_npcf_policyauthorization_build_delete);

    /* Receive PDUSessionResourceModifyRequest +
     * DL NAS transport +
     * PDU session modification command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);

    /* Send Error Indication */
    rv = test_gtpu_send_error_indication(gtpu, qos_flow);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceRelease,
            test_ue->ngap_procedure_code);

    /* Send PDU session resource modify response */
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    sendbuf = testngap_build_qos_flow_resource_release_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session resource modify complete */
    sess->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_MODIFICATION_REQUEST;
    sess->ul_nas_transport_param.dnn = 0;
    sess->ul_nas_transport_param.s_nssai = 0;

    sess->pdu_session_establishment_param.ssc_mode = 0;
    sess->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_modification_complete(sess);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait for PDU session resource modify complete */
    ogs_msleep(100);

    /* Test Bearer Remove */
    test_bearer_remove(qos_flow);

    /* Send PDUSessionResourceReleaseResponse */
    sendbuf = testngap_build_pdu_session_resource_release_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Wait to setup N3 data connection.
     * Otherwise, network-triggered service request is initiated */
    ogs_msleep(100);

    /* Send De-registration request */
    gmmbuf = testgmm_build_de_registration_request(test_ue, 1, true, true);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_remove_ue(test_ue));

    /* gNB disonncect from UPF */
    testgnb_gtpu_close(gtpu);

    /* gNB disonncect from AMF */
    testgnb_ngap_close(ngap);

    /* Clear Test UE Context */
    test_ue_remove(test_ue);
}

#if !HOME_ROUTED_ROAMING_TEST
/**
 * test8_func:
 *   - Simulates absence of N2 Resource Release Ack (step 8)
 *     and PDU Session Release Accept (step 11) from gNB/UE.
 *   - In Home Routed Roaming, V-SMF will not send the
 *     Nsmf_PDUSession_Update Response (step 14), causing
 *     the SBI client to timeout after 10 seconds.
 *   - Subsequent deregistration triggers a PFCP deactivation
 *     modification, but UPF has no session, leading to
 *     a No Context error.
 */
static void test8_func(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *ngap;
    ogs_socknode_t *gtpu;
    ogs_pkbuf_t *gmmbuf;
    ogs_pkbuf_t *gsmbuf;
    ogs_pkbuf_t *nasbuf;
    ogs_pkbuf_t *sendbuf;
    ogs_pkbuf_t *recvbuf;
    ogs_ngap_message_t message;
    int i;

    uint8_t tmp[OGS_HUGE_LEN];
    char *_gtp_payload = "34ff0024"
        "0000000100000085 010002004500001c 0c0b000040015a7a 0a2d00010a2d0002"
        "00000964cd7c291f";

    ogs_nas_5gs_mobile_identity_suci_t mobile_identity_suci;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess5 = NULL, *sess6 = NULL;
    test_bearer_t *qos_flow = NULL;

    bson_t *doc = NULL;

    /* Setup Test UE & Session Context */
    memset(&mobile_identity_suci, 0, sizeof(mobile_identity_suci));

    mobile_identity_suci.h.supi_format = OGS_NAS_5GS_SUPI_FORMAT_IMSI;
    mobile_identity_suci.h.type = OGS_NAS_5GS_MOBILE_IDENTITY_SUCI;
    mobile_identity_suci.routing_indicator1 = 0;
    mobile_identity_suci.routing_indicator2 = 0xf;
    mobile_identity_suci.routing_indicator3 = 0xf;
    mobile_identity_suci.routing_indicator4 = 0xf;
    mobile_identity_suci.protection_scheme_id = OGS_PROTECTION_SCHEME_NULL;
    mobile_identity_suci.home_network_pki_value = 0;

    test_ue = test_ue_add_by_suci(&mobile_identity_suci, "0000000008");
    ogs_assert(test_ue);

    test_ue->nr_cgi.cell_id = 0x40001;

    test_ue->nas.registration.tsc = 0;
    test_ue->nas.registration.ksi = OGS_NAS_KSI_NO_KEY_IS_AVAILABLE;
    test_ue->nas.registration.follow_on_request = 1;
    test_ue->nas.registration.value = OGS_NAS_5GS_REGISTRATION_TYPE_INITIAL;

    test_ue->k_string = "465b5ce8b199b49faa5f0a2ee238a6bc";
    test_ue->opc_string = "e8ed289deba952e4283b54e88e6183ca";

    /* gNB connects to AMF */
    ngap = testngap_client(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, ngap);

    /* gNB connects to UPF */
    gtpu = test_gtpu_server(1, AF_INET);
    ABTS_PTR_NOTNULL(tc, gtpu);

    /* Send NG-Setup Reqeust */
    sendbuf = testngap_build_ng_setup_request(0x4000, 22);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive NG-Setup Response */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /********** Insert Subscriber in Database */
    doc = test_db_new_ims(test_ue);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_insert_ue(test_ue, doc));

    /* Send Registration request */
    test_ue->registration_request_param.guti = 1;
    gmmbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, gmmbuf);

    test_ue->registration_request_param.gmm_capability = 1;
    test_ue->registration_request_param.requested_nssai = 1;
    test_ue->registration_request_param.last_visited_registered_tai = 1;
    test_ue->registration_request_param.ue_usage_setting = 1;
    nasbuf = testgmm_build_registration_request(test_ue, NULL, false, false);
    ABTS_PTR_NOTNULL(tc, nasbuf);

    sendbuf = testngap_build_initial_ue_message(test_ue, gmmbuf,
                NGAP_RRCEstablishmentCause_mo_Signalling, false, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Identity request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Identity response */
    gmmbuf = testgmm_build_identity_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Authentication request */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Authentication response */
    gmmbuf = testgmm_build_authentication_response(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Security mode command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send Security mode complete */
    gmmbuf = testgmm_build_security_mode_complete(test_ue, nasbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive InitialContextSetupRequest +
     * Registration accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_InitialContextSetup,
            test_ue->ngap_procedure_code);

    /* Send UERadioCapabilityInfoIndication */
    sendbuf = testngap_build_ue_radio_capability_info_indication(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send InitialContextSetupResponse */
    sendbuf = testngap_build_initial_context_setup_response(test_ue, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send Registration complete */
    gmmbuf = testgmm_build_registration_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive Configuration update command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDU session establishment request */
    sess5 = test_sess_add_by_dnn_and_psi(test_ue, "internet", 5);
    ogs_assert(sess5);

    sess5->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess5->ul_nas_transport_param.dnn = 1;
    sess5->ul_nas_transport_param.s_nssai = 1;

    sess5->pdu_session_establishment_param.ssc_mode = 1;
    sess5->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess5);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess5,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceSetupRequest +
     * DL NAS transport +
     * PDU session establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceSetup,
            test_ue->ngap_procedure_code);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess5);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU session establishment request */
    sess6 = test_sess_add_by_dnn_and_psi(test_ue, "ims", 6);
    ogs_assert(sess6);

    sess6->ul_nas_transport_param.request_type =
        OGS_NAS_5GS_REQUEST_TYPE_INITIAL;
    sess6->ul_nas_transport_param.dnn = 1;
    sess6->ul_nas_transport_param.s_nssai = 1;

    sess6->pdu_session_establishment_param.ssc_mode = 1;
    sess6->pdu_session_establishment_param.epco = 1;

    gsmbuf = testgsm_build_pdu_session_establishment_request(sess6);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess6,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDU sess6ion establishment accept */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);

    /* Send PDUSessionResourceSetupResponse */
    sendbuf = testngap_sess_build_pdu_session_resource_setup_response(sess6);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU Session release request */
    sess6->pti = 38;
    sess6->ul_nas_transport_param.request_type = 0;
    sess6->ul_nas_transport_param.dnn = 0;
    sess6->ul_nas_transport_param.s_nssai = 0;

    sess6->pdu_session_establishment_param.ssc_mode = 0;
    sess6->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_release_request(sess6);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess6,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send PDU Session release request */
    sess5->pti = 39;
    sess5->ul_nas_transport_param.request_type = 0;
    sess5->ul_nas_transport_param.dnn = 0;
    sess5->ul_nas_transport_param.s_nssai = 0;

    sess5->pdu_session_establishment_param.ssc_mode = 0;
    sess5->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_release_request(sess5);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess5,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive PDUSessionResourceReleaseCommand +
     * DL NAS transport +
     * PDU session release command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceRelease,
            test_ue->ngap_procedure_code);

    /* Receive PDUSessionResourceReleaseCommand +
     * DL NAS transport +
     * PDU session release command */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_PDUSessionResourceRelease,
            test_ue->ngap_procedure_code);

    /* Send PDUSessionResourceReleaseResponse */
    sendbuf = testngap_build_pdu_session_resource_release_response(sess6);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Send UplinkNASTransport +
     * UL NAS trasnport +
     * PDU session resource release complete */
    sess6->ul_nas_transport_param.request_type = 0;
    sess6->ul_nas_transport_param.dnn = 0;
    sess6->ul_nas_transport_param.s_nssai = 0;

    sess6->pdu_session_establishment_param.ssc_mode = 0;
    sess6->pdu_session_establishment_param.epco = 0;

    gsmbuf = testgsm_build_pdu_session_release_complete(sess6);
    ABTS_PTR_NOTNULL(tc, gsmbuf);
    gmmbuf = testgmm_build_ul_nas_transport(sess6,
            OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, gsmbuf);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Test Session Remove */
    test_sess_remove(sess6);

    ogs_msleep(100);

    /* Send De-registration request */
    gmmbuf = testgmm_build_de_registration_request(test_ue, 1, true, true);
    ABTS_PTR_NOTNULL(tc, gmmbuf);
    sendbuf = testngap_build_uplink_nas_transport(test_ue, gmmbuf);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    /* Receive UEContextReleaseCommand */
    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc,
            NGAP_ProcedureCode_id_UEContextRelease,
            test_ue->ngap_procedure_code);

    /* Send UEContextReleaseComplete */
    sendbuf = testngap_build_ue_context_release_complete(test_ue);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);

    ogs_msleep(300);

    /********** Remove Subscriber in Database */
    ABTS_INT_EQUAL(tc, OGS_OK, test_db_remove_ue(test_ue));

    /* gNB disonncect from UPF */
    testgnb_gtpu_close(gtpu);

    /* gNB disonncect from AMF */
    testgnb_ngap_close(ngap);

    /* Clear Test UE Context */
    test_ue_remove(test_ue);
}
#endif

static void test10_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    test_af_qos_reference_setup(tc, "0000000010",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.qos_type = 1;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

static void test11_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    test_af_qos_reference_setup(tc, "0000000011",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.qos_reference = "test-audio";
    af_param.omit_med_type = true;
    af_param.qos_type = 1;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

static void test12_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    test_af_qos_reference_setup(tc, "0000000012",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "UNKNOWN";
    af_param.qos_type = 1;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    ogs_msleep(100);
    /* TS 29.514 V19.8.0 clauses 4.2.2.2 and 4.2.3.2: an invalid QoS reference is 400 with the cause INVALID_SERVICE_INFORMATION */
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            af_sess->last_pcf_status);
    ABTS_STR_EQUAL(tc, "INVALID_SERVICE_INFORMATION",
            af_sess->last_pcf_cause);

    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ABTS_TRUE(tc, qos_flow == NULL);

    af_sess_remove(af_sess);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

static void test13_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    test_af_qos_reference_setup(tc, "0000000013",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.qos_type = 2;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_update);

    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

static void test14_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    char *resource_uri = NULL;

    test_af_qos_reference_setup(tc, "0000000014",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    /* TS 29.514 V19.8.0 clause 5.3.3.3.1: GET returns the AppSessionContext of the Individual Application Session Context */
    af_local_send_to_pcf(af_sess, NULL, af_npcf_policyauthorization_build_get);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    ABTS_STR_EQUAL(tc, "IMS Services", af_sess->last_pcf_asc_af_app_id);
    ABTS_STR_EQUAL(tc, "", af_sess->last_pcf_asc_qos_ref);

    /* After a PATCH the resource reads with the modification applied */
    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.qos_type = 2;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_update);

    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    af_local_send_to_pcf(af_sess, NULL, af_npcf_policyauthorization_build_get);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    ABTS_STR_EQUAL(tc, "IMS Services", af_sess->last_pcf_asc_af_app_id);
    ABTS_STR_EQUAL(tc, "test-audio", af_sess->last_pcf_asc_qos_ref);

    /* A resource that does not exist is 404 with the application error */
    resource_uri = af_sess->app_session.pcf.resource_uri;
    af_sess->app_session.pcf.resource_uri =
        ogs_msprintf("%s999", resource_uri);
    af_local_send_to_pcf(af_sess, NULL, af_npcf_policyauthorization_build_get);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NOT_FOUND, af_sess->last_pcf_status);
    ABTS_STR_EQUAL(tc, "APPLICATION_SESSION_CONTEXT_NOT_FOUND",
            af_sess->last_pcf_cause);
    ogs_free(af_sess->app_session.pcf.resource_uri);
    af_sess->app_session.pcf.resource_uri = resource_uri;

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

static void test15_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    int med_comp_n = 0;

    test_af_qos_reference_setup(tc, "0000000015",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    /* TS 29.514 V19.8.0 clause 4.2.3.36: the key of the media component with the value null removes it, and the SMF releases its QoS flow */
    af_local_send_to_pcf(af_sess, &med_comp_n,
            af_npcf_policyauthorization_build_remove_media);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);

    test_af_qos_flow_release(tc, test_ue, sess, ngap);

    /* The resource has no media component any more: GET shows it and DELETE answers without a PDU session modification */
    af_local_send_to_pcf(af_sess, NULL, af_npcf_policyauthorization_build_get);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    ABTS_STR_EQUAL(tc, "", af_sess->last_pcf_asc_qos_ref);

    af_local_send_to_pcf(af_sess, NULL,
            af_npcf_policyauthorization_build_delete);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NO_CONTENT,
            af_sess->last_pcf_status);

    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/* TS 29.514 V19.8.0 clauses 4.2.2.10, 4.2.3.10 and 5.3.4: the Events Subscription of an application session */
static void test16_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    const char *one =
        "{\"events\":[{\"event\":\"SUCCESSFUL_RESOURCES_ALLOCATION\"}],"
        "\"notifUri\":\"http://127.0.0.1:9/af\"}";
    const char *two =
        "{\"events\":[{\"event\":\"SUCCESSFUL_RESOURCES_ALLOCATION\"},"
        "{\"event\":\"FAILED_RESOURCES_ALLOCATION\"}],"
        "\"notifUri\":\"http://127.0.0.1:9/af\"}";

    test_af_qos_reference_setup(tc, "0000000016",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

#define RAW(__m, __p, __b) \
    do { \
        raw.method = (__m); raw.path = (__p); raw.body = (__b); \
        af_local_send_to_pcf(af_sess, &raw, \
                af_npcf_policyauthorization_build_raw); \
        ogs_msleep(100); \
    } while (0)
#define GET_EVENTS(__expect) \
    do { \
        af_local_send_to_pcf(af_sess, NULL, \
                af_npcf_policyauthorization_build_get); \
        ogs_msleep(100); \
        ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status); \
        ABTS_STR_EQUAL(tc, (__expect), af_sess->last_pcf_asc_events); \
    } while (0)

    /* the test AF subscribes to two events when it creates the application session */
    GET_EVENTS("CHARGING_CORRELATION,ANI_REPORT");

    /* DELETE removes the sub-resource (204); a second DELETE is 404 */
    RAW("DELETE", "/events-subscription", NULL);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NO_CONTENT, af_sess->last_pcf_status);
    GET_EVENTS("");
    RAW("DELETE", "/events-subscription", NULL);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NOT_FOUND, af_sess->last_pcf_status);

    /* PUT creates it (201), a second PUT modifies it (200) */
    RAW("PUT", "/events-subscription", one);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);
    GET_EVENTS("SUCCESSFUL_RESOURCES_ALLOCATION");
    RAW("PUT", "/events-subscription", two);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    GET_EVENTS("SUCCESSFUL_RESOURCES_ALLOCATION,FAILED_RESOURCES_ALLOCATION");

    /* a body without events is 400 */
    RAW("PUT", "/events-subscription", "{}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_pcf_status);

    /* a PATCH with only the events subscription (no media component) changes it; null removes it */
    RAW("PATCH", "",
        "{\"ascReqData\":{\"evSubsc\":{\"events\":[{\"event\":\"FAILED_RESOURCES_ALLOCATION\"}],"
        "\"notifUri\":\"http://127.0.0.1:9/af\"}}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    GET_EVENTS("FAILED_RESOURCES_ALLOCATION");
    RAW("PATCH", "", "{\"ascReqData\":{\"evSubsc\":null}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    GET_EVENTS("");

    RAW("PUT", "/events-subscription", one);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);
    GET_EVENTS("SUCCESSFUL_RESOURCES_ALLOCATION");

#undef RAW
#undef GET_EVENTS

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/* TS 29.514 V19.8.0 clauses 4.2.2.10 and 4.2.5.8: the NF service consumer is told when the resources are allocated */
static void test17_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    test_af_qos_reference_setup(tc, "0000000017",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_param.ev_resources_allocation = true;

    ABTS_INT_EQUAL(tc, 0, af_sess->notify_count);
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    /* the access network sets the QoS flow up: the SMF reports the rule as active and the PCF notifies the NF service consumer */
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(300);

    ABTS_INT_EQUAL(tc, 1, af_sess->notify_count);
    ABTS_STR_EQUAL(tc, "SUCCESSFUL_RESOURCES_ALLOCATION", af_sess->notify_event);
    ABTS_INT_EQUAL(tc, 0, af_sess->notify_med_comp_n);
    ABTS_STR_EQUAL(tc, "", af_sess->notify_failed_status);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/* TS 29.514 V19.8.0 clause 4.2.2.2: the AF charging identifier of an ongoing application session cannot be used again */
static void test18_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    test_af_qos_reference_setup(tc, "0000000018",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_param.af_charg_id = "charge-1";

    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    /* the same identifier for a new application session */
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    ogs_msleep(200);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_pcf_status);
    ABTS_STR_EQUAL(tc, "DUPLICATED_AF_SESSION", af_sess->last_pcf_cause);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/* TS 29.514 V19.8.0 clause 4.2.5.8: the NF service consumer is told when the resources cannot be allocated */
static void test19_func(abts_case *tc, void *data)
{
    int rv;
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    ogs_pkbuf_t *recvbuf = NULL;
    ogs_pkbuf_t *sendbuf = NULL;

    test_af_qos_reference_setup(tc, "0000000019",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_param.ev_resources_allocation = true;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);

    recvbuf = testgnb_ngap_read(ngap);
    ABTS_PTR_NOTNULL(tc, recvbuf);
    testngap_recv(test_ue, recvbuf);
    ABTS_INT_EQUAL(tc, NGAP_ProcedureCode_id_PDUSessionResourceModify,
            test_ue->ngap_procedure_code);
    qos_flow = test_qos_flow_find_by_qfi(sess, 2);
    ogs_assert(qos_flow);

    /* the access network cannot set the QoS flow up: the SMF reports the rule as inactive */
    sess->qos_flow_modify_failure = true;
    sendbuf = testngap_build_qos_flow_resource_modify_response(qos_flow);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    rv = testgnb_ngap_send(ngap, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    sess->qos_flow_modify_failure = false;
    ogs_msleep(300);

    ABTS_INT_EQUAL(tc, 1, af_sess->notify_count);
    ABTS_STR_EQUAL(tc, "FAILED_RESOURCES_ALLOCATION", af_sess->notify_event);
    ABTS_INT_EQUAL(tc, 0, af_sess->notify_med_comp_n);
    ABTS_STR_EQUAL(tc, "INACTIVE", af_sess->notify_failed_status);

    af_sess_remove(af_sess);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/*
 * TS 29.514 V19.8.0 clause 5.6.2.7 / TS 29.512 clause 5.6.2.10: the fStatus of a media component is the gate of its flows. The PCF sends it as
 * TrafficControlData, the SMF closes the gate of the QER and the UPF drops the packets of a closed gate (TS 29.244 clause 5.4.2).
 */
static bool test_af_gtpu_reply(ogs_socknode_t *gtpu, int msec)
{
    struct pollfd fds;
    ogs_pkbuf_t *recvbuf = NULL;

    memset(&fds, 0, sizeof(fds));
    fds.fd = gtpu->sock->fd;
    fds.events = POLLIN;
    if (poll(&fds, 1, msec) <= 0) return false;

    recvbuf = testgnb_gtpu_read(gtpu);
    if (!recvbuf) return false;
    ogs_pkbuf_free(recvbuf);
    return true;
}

static void test20_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    int rv;

    test_af_qos_reference_setup(tc, "0000000020",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(100);

#define SET_STATUS(__status) \
    do { \
        raw.method = "PATCH"; raw.path = ""; \
        raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"medType\":\"AUDIO\",\"fStatus\":\"" __status "\"}}}}"; \
        af_local_send_to_pcf(af_sess, &raw, \
                af_npcf_policyauthorization_build_raw); \
        ogs_msleep(100); \
        ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status); \
        test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap); \
        ogs_msleep(200); \
    } while (0)

    /* enabled: the ping goes through the QoS flow */
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    ABTS_TRUE(tc, test_af_gtpu_reply(gtpu, 2000));

    /* DISABLED: both gates are closed, the packet is dropped */
    SET_STATUS("DISABLED");
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    ABTS_TRUE(tc, !test_af_gtpu_reply(gtpu, 1000));

    /* ENABLED again: the flow works */
    SET_STATUS("ENABLED");
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    ABTS_TRUE(tc, test_af_gtpu_reply(gtpu, 2000));

    /* ENABLED_UPLINK: the downlink gate is closed, so the reply does not come back */
    SET_STATUS("ENABLED-UPLINK");
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    ABTS_TRUE(tc, !test_af_gtpu_reply(gtpu, 1000));

    /* ENABLED_DOWNLINK: the uplink gate is closed */
    SET_STATUS("ENABLED-DOWNLINK");
    rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
    ABTS_INT_EQUAL(tc, OGS_OK, rv);
    ABTS_TRUE(tc, !test_af_gtpu_reply(gtpu, 1000));

#undef SET_STATUS

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/*
 * TS 29.514 V19.8.0 clause 5.3.3.4.2.2: a deletion that asks for events gets 200 with evsNotif when the PCF has the information.
 * The SMF has confirmed the resources of the rule, so SUCCESSFUL_RESOURCES_ALLOCATION is reported; an event that did not occur is
 * not, and a deletion without a body is 204.
 */
static void test21_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;

    test_af_qos_reference_setup(tc, "0000000021",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(300);

    raw.method = "POST";
    raw.path = "/delete";
    raw.body = "{\"events\":[{\"event\":\"SUCCESSFUL_RESOURCES_ALLOCATION\"},"
        "{\"event\":\"FAILED_RESOURCES_ALLOCATION\"}],"
        "\"notifUri\":\"http://127.0.0.1:9/af\"}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    test_af_qos_flow_release(tc, test_ue, sess, ngap);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    ABTS_STR_EQUAL(tc, "SUCCESSFUL_RESOURCES_ALLOCATION", af_sess->last_pcf_asc_events);

    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/*
 * TS 29.514 V19.8.0 clause 4.2.4.3: the application function enabled sponsored data connectivity and gave a usage threshold, so the
 * answer of the deletion carries the accumulated usage (the event USAGE_REPORT and usgRep) that the PCF fetched from the SMF.
 */
static void test22_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    int i, rv;

    test_af_qos_reference_setup(tc, "0000000022",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_param.sponsor_id = "sponsor-1";
    af_param.usage_threshold = 10000000;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    for (i = 0; i < 3; i++) {
        rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4);
        ABTS_INT_EQUAL(tc, OGS_OK, rv);
        ABTS_TRUE(tc, test_af_gtpu_reply(gtpu, 2000));
    }
    ogs_msleep(200);
    /* the threshold is not reached: no notification */
    ABTS_INT_EQUAL(tc, 0, af_sess->notify_usg_count);

    raw.method = "POST";
    raw.path = "/delete";
    raw.body = NULL;
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    test_af_qos_flow_release(tc, test_ue, sess, ngap);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    ABTS_STR_EQUAL(tc, "USAGE_REPORT", af_sess->last_pcf_asc_events);
    ABTS_TRUE(tc, af_sess->delete_usg_total > 0);

    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/*
 * TS 29.514 V19.8.0 clause 4.2.5.6: the PCF notifies the accumulated usage when the threshold is reached and again when the
 * application function disables sponsoring; the monitoring does not continue after a report until new thresholds are given
 * (clause 4.2.3.5). The deletion that follows has no usage to report.
 */
static void test23_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    int i, rv;

    test_af_qos_reference_setup(tc, "0000000023",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_param.sponsor_id = "sponsor-2";
    af_param.usage_threshold = 100;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(500);

#define PINGS(__n) \
    do { \
        for (i = 0; i < (__n); i++) { \
            rv = test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4); \
            ABTS_INT_EQUAL(tc, OGS_OK, rv); \
            ABTS_TRUE(tc, test_af_gtpu_reply(gtpu, 2000)); \
        } \
        ogs_msleep(1500); \
    } while (0)

    /* the threshold is reached: one notification with the usage */
    PINGS(4);
    ABTS_INT_EQUAL(tc, 1, af_sess->notify_usg_count);
    ABTS_TRUE(tc, af_sess->notify_usg_total >= 100);

    /* the monitoring stopped with the report: more traffic is not reported */
    PINGS(4);
    ABTS_INT_EQUAL(tc, 1, af_sess->notify_usg_count);

    /* new thresholds start it again */
    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"evSubsc\":{\"events\":[{\"event\":\"USAGE_REPORT\"}],"
        "\"usgThres\":{\"totalVolume\":100}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    PINGS(4);
    ABTS_INT_EQUAL(tc, 2, af_sess->notify_usg_count);

    /* thresholds that are not reached, and the application function disables sponsoring: the usage so far is reported */
    raw.body = "{\"ascReqData\":{\"evSubsc\":{\"events\":[{\"event\":\"USAGE_REPORT\"}],"
        "\"usgThres\":{\"totalVolume\":10000000}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    PINGS(2);
    ABTS_INT_EQUAL(tc, 2, af_sess->notify_usg_count);
    raw.body = "{\"ascReqData\":{\"sponStatus\":\"SPONSOR_DISABLED\"}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    ABTS_INT_EQUAL(tc, 3, af_sess->notify_usg_count);
    ABTS_TRUE(tc, af_sess->notify_usg_total > 0);

#undef PINGS

    /* nothing is monitored any more: the deletion is answered at once, without usage */
    raw.method = "POST";
    raw.path = "/delete";
    raw.body = NULL;
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    test_af_qos_flow_release(tc, test_ue, sess, ngap);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NO_CONTENT, af_sess->last_pcf_status);
    ABTS_INT_EQUAL(tc, -1, (int)af_sess->delete_usg_total);

    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/*
 * TS 29.514 V19.8.0 clause 4.2.5.4: the AF subscribed to QOS_NOTIF; when the NG-RAN notifies that the GBR QoS targets of the flow
 * are not fulfilled, or fulfilled again, the PCF tells the AF with qncReports (notifType and the flows). After the subscription is
 * removed (clause 4.2.3.6) a notification of the NG-RAN is not passed on.
 */
static void test24_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    ogs_pkbuf_t *sendbuf = NULL;

    test_af_qos_reference_setup(tc, "0000000024",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_param.ev_qos_notif = true;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    sendbuf = testngap_build_qos_flow_notify(qos_flow, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, testgnb_ngap_send(ngap, sendbuf));
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 1, af_sess->notify_qnc_count);
    ABTS_STR_EQUAL(tc, "NOT_GUARANTEED", af_sess->notify_qnc_type);
    ABTS_INT_EQUAL(tc, 0, af_sess->notify_qnc_med_comp_n);

    sendbuf = testngap_build_qos_flow_notify(qos_flow, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, testgnb_ngap_send(ngap, sendbuf));
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 2, af_sess->notify_qnc_count);
    ABTS_STR_EQUAL(tc, "GUARANTEED", af_sess->notify_qnc_type);

    /* the subscription is removed: the notification is not passed on */
    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"evSubsc\":{\"events\":[{\"event\":\"ANI_REPORT\",\"notifMethod\":\"ONE_TIME\"}]}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    /* the QoS flow no longer asks the NG-RAN for notification: the SMF modifies it */
    test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_msleep(300);
    sendbuf = testngap_build_qos_flow_notify(qos_flow, false);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, testgnb_ngap_send(ngap, sendbuf));
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 2, af_sess->notify_qnc_count);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/* The test gNB answers a downlink packet with QMP that the UPF sends (TS 38.415 V19.1.0 clause 5.5.2.2); false when none comes in time */
static bool test_af_qos_monitoring_respond(ogs_socknode_t *gtpu, test_bearer_t *qos_flow,
        uint32_t dl_result, uint32_t ul_result, int msec)
{
    struct pollfd fds;
    ogs_pkbuf_t *recvbuf = NULL;
    ogs_time_t received_at;
    int rv;

    memset(&fds, 0, sizeof(fds));
    fds.fd = gtpu->sock->fd;
    fds.events = POLLIN;
    if (poll(&fds, 1, msec) <= 0)
        return false;
    recvbuf = testgnb_gtpu_read(gtpu);
    received_at = ogs_time_now();
    if (!recvbuf)
        return false;
    rv = test_gtpu_send_qos_monitoring_response(gtpu, qos_flow, recvbuf, received_at, dl_result, ul_result);
    ogs_pkbuf_free(recvbuf);
    return rv == OGS_OK;
}

/*
 * TS 29.514 V19.8.0 clauses 4.2.2.23 and 4.2.5.14, TS 29.512 V19.7.0 clauses 4.2.3.25 and 4.2.4.24, TS 29.244 V19.6.0 clause 5.24.4: the AF
 * subscribes to QOS_MONITORING (event detection, thresholds of 40 ms), the SMF has the UPF measure the packet delay of the QoS flow, the UPF
 * sends a packet with QMP and the test gNB answers it with the delays of the RAN part: the round trip delay reaches the threshold and the
 * AF is told the three delays. Delays below the thresholds are not reported.
 */
static void test25_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    int i;

    test_af_qos_reference_setup(tc, "0000000025",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_param.ev_qos_mon = true;
    af_param.qos_mon_threshold = 40;
    af_param.qos_mon_wait_time = 1;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    /* the first packet with QMP: the RAN part of the delays is 20 ms downlink and 30 ms uplink */
    ABTS_TRUE(tc, test_af_qos_monitoring_respond(gtpu, qos_flow, 20, 30, 5000));
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 1, af_sess->notify_qm_count);
    ABTS_TRUE(tc, af_sess->notify_qm_dl >= 20 && af_sess->notify_qm_dl < 25);
    ABTS_TRUE(tc, af_sess->notify_qm_ul >= 30 && af_sess->notify_qm_ul < 35);
    ABTS_TRUE(tc, af_sess->notify_qm_rt >= 50 && af_sess->notify_qm_rt < 60);
    ABTS_INT_EQUAL(tc, 0, af_sess->notify_qm_med_comp_n);

    /* delays below the thresholds: the next packets are answered, and nothing is reported */
    for (i = 0; i < 2; i++)
        ABTS_TRUE(tc, test_af_qos_monitoring_respond(gtpu, qos_flow, 1, 1, 5000));
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 1, af_sess->notify_qm_count);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}


/* One downlink RTP packet (UDP) from the test to the UE: the UPF gets it from the network side (TUN device) like any packet from the data network */
static bool test_af_send_rtp_ssrc(test_sess_t *sess, uint32_t timestamp, uint16_t seq, bool marker, uint8_t pt, uint32_t ssrc);

static bool test_af_send_rtp(test_sess_t *sess, uint32_t timestamp, uint16_t seq, bool marker)
{
    return test_af_send_rtp_ssrc(sess, timestamp, seq, marker, 96, 1);
}

static bool test_af_send_rtp_ssrc(test_sess_t *sess, uint32_t timestamp, uint16_t seq, bool marker, uint8_t pt, uint32_t ssrc)
{
    struct sockaddr_in to;
    uint8_t rtp[16 + 20];
    int fd, rv;
    char addr[INET_ADDRSTRLEN];

    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(30000);
    memcpy(&to.sin_addr, &sess->ue_ip.addr, sizeof(to.sin_addr));
    (void)addr;
    memset(rtp, 0, sizeof(rtp));
    rtp[0] = 0x80;                                  /* version 2 */
    rtp[1] = (marker ? 0x80 : 0) | pt;              /* marker bit and payload type */
    rtp[2] = seq >> 8; rtp[3] = seq & 0xff;
    rtp[4] = timestamp >> 24; rtp[5] = timestamp >> 16; rtp[6] = timestamp >> 8; rtp[7] = timestamp;
    rtp[8] = ssrc >> 24; rtp[9] = ssrc >> 16; rtp[10] = ssrc >> 8; rtp[11] = ssrc;   /* SSRC */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;
    rv = sendto(fd, rtp, sizeof(rtp), 0, (struct sockaddr *)&to, sizeof(to));
    close(fd);
    return rv == (int)sizeof(rtp);
}

/* The GTP-U header of the next packet that the test gNB receives; false when none comes in time */
static bool test_af_gtpu_read_header(ogs_socknode_t *gtpu, int msec, ogs_gtp2_header_desc_t *desc)
{
    struct pollfd fds;
    ogs_pkbuf_t *recvbuf = NULL;
    int hlen;

    memset(&fds, 0, sizeof(fds));
    fds.fd = gtpu->sock->fd;
    fds.events = POLLIN;
    if (poll(&fds, 1, msec) <= 0) return false;
    recvbuf = testgnb_gtpu_read(gtpu);
    if (!recvbuf) return false;
    hlen = ogs_gtpu_parse_header(desc, recvbuf);
    ogs_pkbuf_free(recvbuf);
    return hlen > 0;
}

/* The ECN field of the IP packet that the test gNB receives from the UPF; false when none comes in time */
static bool test_af_gtpu_reply_ecn(ogs_socknode_t *gtpu, int msec, int *ecn)
{
    struct pollfd fds;
    ogs_pkbuf_t *recvbuf = NULL;
    int hlen;

    memset(&fds, 0, sizeof(fds));
    fds.fd = gtpu->sock->fd;
    fds.events = POLLIN;
    if (poll(&fds, 1, msec) <= 0) return false;

    recvbuf = testgnb_gtpu_read(gtpu);
    if (!recvbuf) return false;
    hlen = ogs_gtpu_parse_header(NULL, recvbuf);
    *ecn = hlen > 0 ? (((uint8_t *)recvbuf->data)[hlen + 1] & 0x03) : -1;
    ogs_pkbuf_free(recvbuf);
    return true;
}

/*
 * TS 29.514 V19.8.0 clauses 4.2.2.43 and 4.2.5.27, TS 29.512 V19.7.0 clause 4.2.6.2.21, TS 38.413 V19.1.0 clause 9.3.1.266, TS 29.244
 * V19.6.0 clause 5.37 and TS 38.415 V19.1.0 clause 5.5.2.2: the AF gives l4sInd UL_DL and subscribes to L4S_SUPP; the SMF has the UPF
 * mark the packets and asks the NG-RAN for the congestion information (ECN Marking at UPF Request both). With the congestion that the
 * test gNB reports, the UPF sets CE in the ECN-capable packets, and does not touch the packets that are not ECN-capable. A status
 * that says the reporting is not active is passed on to the AF as NOT_AVAILABLE and the UPF stops the marking; active again is
 * AVAILABLE and the marking resumes with the next congestion information.
 */
static void test26_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    int ecn = -1;

    test_af_qos_reference_setup(tc, "0000000026",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_param.l4s_ind = 3;
    af_param.ev_l4s = true;
    sess->ecn_status = 1;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    /* ECN Marking at UPF Request both (0 ul, 1 dl, 2 both, 3 stop, plus 1); the NG-RAN said active: nothing is reported */
    ABTS_INT_EQUAL(tc, 3, sess->ecn_request);
    ABTS_INT_EQUAL(tc, 0, af_sess->notify_l4s_count);

    /* no congestion: an ECT(1) packet is not marked */
    test_gtpu_ping_tos = 0x01;
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4));
    ABTS_TRUE(tc, test_af_gtpu_reply_ecn(gtpu, 2000, &ecn));
    ABTS_INT_EQUAL(tc, 0x01, ecn);

    /* the NG-RAN reports congestion of 100 percent downlink: the ECT(1) packet comes back with CE */
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_congestion(gtpu, qos_flow, 10000, 10000));
    ogs_msleep(200);
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4));
    ABTS_TRUE(tc, test_af_gtpu_reply_ecn(gtpu, 2000, &ecn));
    ABTS_INT_EQUAL(tc, 0x03, ecn);

    /* a packet that is not ECN-capable stays as it is */
    test_gtpu_ping_tos = 0x00;
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4));
    ABTS_TRUE(tc, test_af_gtpu_reply_ecn(gtpu, 2000, &ecn));
    ABTS_INT_EQUAL(tc, 0x00, ecn);

    /* no more congestion: the packets are not marked any more */
    test_gtpu_ping_tos = 0x01;
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_congestion(gtpu, qos_flow, 0, 0));
    ogs_msleep(200);
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4));
    ABTS_TRUE(tc, test_af_gtpu_reply_ecn(gtpu, 2000, &ecn));
    ABTS_INT_EQUAL(tc, 0x01, ecn);

    /* congestion again, then l4sInd DL only, and the NG-RAN answers that the reporting is not active: ECN Marking at UPF Request dl,
     * NOT_AVAILABLE to the AF, and the UPF stops the marking (TS 29.244 V19.6.0 clause 5.38.1) */
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_congestion(gtpu, qos_flow, 10000, 10000));
    ogs_msleep(200);
    sess->ecn_status = 2;
    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"medType\":\"AUDIO\",\"l4sInd\":\"DL\"}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 2, sess->ecn_request);
    ABTS_INT_EQUAL(tc, 1, af_sess->notify_l4s_count);
    ABTS_STR_EQUAL(tc, "NOT_AVAILABLE", af_sess->notify_l4s_type);
    ABTS_INT_EQUAL(tc, 0, af_sess->notify_l4s_med_comp_n);
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4));
    ABTS_TRUE(tc, test_af_gtpu_reply_ecn(gtpu, 2000, &ecn));
    ABTS_INT_EQUAL(tc, 0x01, ecn);

    /* l4sInd UL_DL again and the NG-RAN reports active: AVAILABLE */
    sess->ecn_status = 1;
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"medType\":\"AUDIO\",\"l4sInd\":\"UL_DL\"}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 3, sess->ecn_request);
    ABTS_INT_EQUAL(tc, 2, af_sess->notify_l4s_count);
    ABTS_STR_EQUAL(tc, "AVAILABLE", af_sess->notify_l4s_type);

    /* the marking resumes with the congestion information that follows */
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_congestion(gtpu, qos_flow, 10000, 10000));
    ogs_msleep(200);
    ABTS_INT_EQUAL(tc, OGS_OK, test_gtpu_send_ping(gtpu, qos_flow, TEST_PING_IPV4));
    ABTS_TRUE(tc, test_af_gtpu_reply_ecn(gtpu, 2000, &ecn));
    ABTS_INT_EQUAL(tc, 0x03, ecn);

    test_gtpu_ping_tos = 0;
    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/* One request to the Npcf_BDTPolicyControl service; the answer is in af_sess->last_bdt_* */
static void test_af_bdt(af_sess_t *af_sess, const char *method, const char *path, const char *body)
{
    af_npcf_bdtpolicycontrol_raw_t raw;

    raw.method = method;
    raw.path = path;
    raw.body = body;
    af_sess->last_bdt_status = 0;
    af_local_send_to_pcf(af_sess, &raw, af_npcf_bdtpolicycontrol_build_raw);
    ogs_msleep(300);
}

#define BDT_REQ(__asp, __des, __extra) \
    "{\"aspId\":\"" __asp "\",\"desTimeInt\":" __des ",\"numOfUes\":10,\"volPerUe\":{\"totalVolume\":1000000}" __extra "}"

/*
 * TS 29.554 V19.2.1 clauses 4.2.2.2, 4.2.3.2 and 4.2.5.2 and tables 5.3.2.3.1-3, 5.3.3.3.1-3 and 5.3.3.3.2-3: the BDT policy of the
 * Media AF. A request creates an Individual BDT policy with the transfer policies of the PCF (here a policy for the window from 01:00 to
 * 05:00 UTC and one for the whole desired window); the same request again is 303 to the existing resource; a transfer policy that was
 * offered is selected by PATCH, in the form that the negotiated features give; the resource can be read and deleted.
 */
static void test27_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    cJSON *doc = NULL, *pol = NULL, *list = NULL, *t = NULL, *req = NULL;
    char first[200], second[200], ref1[80];

    test_af_qos_reference_setup(tc, "0000000027",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    /* invalid requests: a mandatory attribute is missing, numOfUes is 0, the window ends before it starts */
    test_af_bdt(af_sess, "POST", "", "{\"desTimeInt\":{\"startTime\":\"2030-03-05T00:00:00Z\",\"stopTime\":\"2030-03-05T12:00:00Z\"},\"numOfUes\":10,\"volPerUe\":{\"totalVolume\":1}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "POST", "", "{\"aspId\":\"asp\",\"desTimeInt\":{\"startTime\":\"2030-03-05T00:00:00Z\",\"stopTime\":\"2030-03-05T12:00:00Z\"},\"numOfUes\":0,\"volPerUe\":{\"totalVolume\":1}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "POST", "", "{\"aspId\":\"asp\",\"desTimeInt\":{\"startTime\":\"2030-03-05T12:00:00Z\",\"stopTime\":\"2030-03-05T00:00:00Z\"},\"numOfUes\":1,\"volPerUe\":{\"totalVolume\":1}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_bdt_status);

    /* create, with PatchCorrection (feature 3) in suppFeat; the notification attributes are not kept: BdtNotification_5G is not supported */
    test_af_bdt(af_sess, "POST", "", BDT_REQ("asp-1",
            "{\"startTime\":\"2030-03-05T00:00:00Z\",\"stopTime\":\"2030-03-05T12:00:00Z\"}",
            ",\"suppFeat\":\"4\",\"warnNotifReq\":true,\"notifUri\":\"http://127.0.0.1:9/n\""));
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_bdt_status);
    ABTS_PTR_NOTNULL(tc, strstr(af_sess->last_bdt_location, "/npcf-bdtpolicycontrol/v1/bdtpolicies/"));
    ogs_cpystrn(first, strstr(af_sess->last_bdt_location, "/bdtpolicies") + strlen("/bdtpolicies"), sizeof(first));
    doc = cJSON_Parse(af_sess->last_bdt_body);
    ABTS_PTR_NOTNULL(tc, doc);
    pol = cJSON_GetObjectItem(doc, "bdtPolData");
    req = cJSON_GetObjectItem(doc, "bdtReqData");
    ABTS_TRUE(tc, cJSON_IsString(cJSON_GetObjectItem(pol, "bdtRefId")));
    ogs_cpystrn(ref1, cJSON_GetObjectItem(pol, "bdtRefId")->valuestring, sizeof(ref1));
    ABTS_STR_EQUAL(tc, "4", cJSON_GetObjectItem(pol, "suppFeat")->valuestring);
    ABTS_STR_EQUAL(tc, "asp-1", cJSON_GetObjectItem(req, "aspId")->valuestring);
    ABTS_PTR_EQUAL(tc, NULL, cJSON_GetObjectItem(req, "warnNotifReq"));
    ABTS_PTR_EQUAL(tc, NULL, cJSON_GetObjectItem(req, "notifUri"));
    list = cJSON_GetObjectItem(pol, "transfPolicies");
    ABTS_INT_EQUAL(tc, 2, cJSON_GetArraySize(list));
    t = cJSON_GetArrayItem(list, 0);
    ABTS_INT_EQUAL(tc, 1, cJSON_GetObjectItem(t, "transPolicyId")->valueint);
    ABTS_INT_EQUAL(tc, 10, cJSON_GetObjectItem(t, "ratingGroup")->valueint);
    ABTS_STR_EQUAL(tc, "2030-03-05T01:00:00Z", cJSON_GetObjectItem(cJSON_GetObjectItem(t, "recTimeInt"), "startTime")->valuestring);
    ABTS_STR_EQUAL(tc, "2030-03-05T05:00:00Z", cJSON_GetObjectItem(cJSON_GetObjectItem(t, "recTimeInt"), "stopTime")->valuestring);
    ABTS_STR_EQUAL(tc, "10000000 bps", cJSON_GetObjectItem(t, "maxBitRateDl")->valuestring);
    ABTS_STR_EQUAL(tc, "2000000 bps", cJSON_GetObjectItem(t, "maxBitRateUl")->valuestring);
    t = cJSON_GetArrayItem(list, 1);
    ABTS_INT_EQUAL(tc, 2, cJSON_GetObjectItem(t, "transPolicyId")->valueint);
    ABTS_INT_EQUAL(tc, 20, cJSON_GetObjectItem(t, "ratingGroup")->valueint);
    ABTS_STR_EQUAL(tc, "2030-03-05T00:00:00Z", cJSON_GetObjectItem(cJSON_GetObjectItem(t, "recTimeInt"), "startTime")->valuestring);
    ABTS_STR_EQUAL(tc, "2030-03-05T12:00:00Z", cJSON_GetObjectItem(cJSON_GetObjectItem(t, "recTimeInt"), "stopTime")->valuestring);
    ABTS_PTR_EQUAL(tc, NULL, cJSON_GetObjectItem(t, "maxBitRateDl"));
    cJSON_Delete(doc);

    /* the same service requirements: 303 See Other to the existing resource */
    test_af_bdt(af_sess, "POST", "", BDT_REQ("asp-1",
            "{\"startTime\":\"2030-03-05T00:00:00Z\",\"stopTime\":\"2030-03-05T12:00:00Z\"}", ""));
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_SEE_OTHER, af_sess->last_bdt_status);
    ABTS_PTR_NOTNULL(tc, strstr(af_sess->last_bdt_location, first));

    /* read it, and one that does not exist */
    test_af_bdt(af_sess, "GET", first, NULL);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_bdt_status);
    doc = cJSON_Parse(af_sess->last_bdt_body);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_STR_EQUAL(tc, ref1, cJSON_GetObjectItem(cJSON_GetObjectItem(doc, "bdtPolData"), "bdtRefId")->valuestring);
    cJSON_Delete(doc);
    test_af_bdt(af_sess, "GET", "/9999", NULL);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NOT_FOUND, af_sess->last_bdt_status);
    ABTS_STR_EQUAL(tc, "BDT_POLICY_NOT_FOUND", af_sess->last_bdt_cause);
    test_af_bdt(af_sess, "PATCH", "/9999", "{\"bdtPolData\":{\"selTransPolicyId\":1}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NOT_FOUND, af_sess->last_bdt_status);
    ABTS_STR_EQUAL(tc, "BDT_POLICY_NOT_FOUND", af_sess->last_bdt_cause);

    /* PatchCorrection: the selection is bdtPolData.selTransPolicyId; a policy that was not offered, the old form and value 0 are refused */
    test_af_bdt(af_sess, "PATCH", first, "{\"bdtPolData\":{\"selTransPolicyId\":7}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "PATCH", first, "{\"bdtPolData\":{\"selTransPolicyId\":0}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "PATCH", first, "{\"selTransPolicyId\":2}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "PATCH", first, "{\"bdtPolData\":{\"selTransPolicyId\":2}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_bdt_status);
    doc = cJSON_Parse(af_sess->last_bdt_body);
    ABTS_PTR_NOTNULL(tc, doc);
    ABTS_INT_EQUAL(tc, 2, cJSON_GetObjectItem(cJSON_GetObjectItem(doc, "bdtPolData"), "selTransPolicyId")->valueint);
    cJSON_Delete(doc);

    /* without PatchCorrection the selection is the BdtPolicyDataPatch; a window that the daily policy does not overlap only has policy 2 */
    test_af_bdt(af_sess, "POST", "", BDT_REQ("asp-2",
            "{\"startTime\":\"2030-03-06T06:00:00Z\",\"stopTime\":\"2030-03-06T07:00:00Z\"}", ""));
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_bdt_status);
    ogs_cpystrn(second, strstr(af_sess->last_bdt_location, "/bdtpolicies") + strlen("/bdtpolicies"), sizeof(second));
    doc = cJSON_Parse(af_sess->last_bdt_body);
    ABTS_PTR_NOTNULL(tc, doc);
    pol = cJSON_GetObjectItem(doc, "bdtPolData");
    ABTS_PTR_EQUAL(tc, NULL, cJSON_GetObjectItem(pol, "suppFeat"));
    list = cJSON_GetObjectItem(pol, "transfPolicies");
    ABTS_INT_EQUAL(tc, 1, cJSON_GetArraySize(list));
    ABTS_INT_EQUAL(tc, 2, cJSON_GetObjectItem(cJSON_GetArrayItem(list, 0), "transPolicyId")->valueint);
    cJSON_Delete(doc);
    test_af_bdt(af_sess, "PATCH", second, "{\"bdtPolData\":{\"selTransPolicyId\":2}}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "PATCH", second, "{\"selTransPolicyId\":2}");
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_bdt_status);

    /* delete both: 204, then they are gone; the collection takes no GET */
    test_af_bdt(af_sess, "GET", "", NULL);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_METHOD_NOT_ALLOWED, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "DELETE", first, NULL);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NO_CONTENT, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "GET", first, NULL);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NOT_FOUND, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "DELETE", first, NULL);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NOT_FOUND, af_sess->last_bdt_status);
    test_af_bdt(af_sess, "DELETE", second, NULL);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NO_CONTENT, af_sess->last_bdt_status);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/* TS 29.514 V19.8.0 clause 4.2.3.2: fStatus REMOVED on every media sub component removes the flows of the media component, so the PCC rule */
static void test28_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;

    test_af_qos_reference_setup(tc, "0000000028",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,"
        "\"medSubComps\":{\"0\":{\"fNum\":0,\"fStatus\":\"REMOVED\"}}}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);

    /* the rule is gone: the SMF releases the QoS flow */
    test_af_qos_flow_release(tc, test_ue, sess, ngap);

    af_local_send_to_pcf(af_sess, NULL,
            af_npcf_policyauthorization_build_delete);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NO_CONTENT,
            af_sess->last_pcf_status);

    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}


/*
 * TS 29.514 V19.8.0 clauses 4.2.2.32 and 4.2.3.30, TS 29.512 V19.7.0 clause 4.2.6.2.1, TS 38.413 V19.1.0 clauses 9.3.1.152 and 9.3.1.153: the AF
 * gives a QoS reference with two alternative QoS references. The PCF derives the alternative QoS parameter sets from the qos_profiles
 * entries (delay budget, error rate, guaranteed bit rates), the SMF tells the NG-RAN the list in the order of priority, the NG-RAN reports
 * the set that it fulfils with the index of the list, and the AF is told the alternative that it can have as the QoS reference it gave.
 * A null altSerReqs removes the sets.
 */
static void test29_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    ogs_pkbuf_t *sendbuf = NULL;

    test_af_qos_reference_setup(tc, "0000000029",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.alt_ser_req1 = "test-audio-alt1";
    af_param.alt_ser_req2 = "test-audio-alt2";
    af_param.ev_qos_notif = true;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    /* the list of the QoS flow, with the values of the qos_profiles entries (0.5 ms units for the delay budget) */
    ABTS_INT_EQUAL(tc, 2, sess->qos_ext.count);
    ABTS_INT_EQUAL(tc, 1, (int)sess->qos_ext.set[0].index);
    ABTS_INT_EQUAL(tc, 64000, (int)sess->qos_ext.set[0].gfbr_dl);
    ABTS_INT_EQUAL(tc, 48000, (int)sess->qos_ext.set[0].gfbr_ul);
    ABTS_INT_EQUAL(tc, 200, (int)sess->qos_ext.set[0].pdb);
    ABTS_INT_EQUAL(tc, 1, (int)sess->qos_ext.set[0].per_scalar);
    ABTS_INT_EQUAL(tc, 2, (int)sess->qos_ext.set[0].per_exponent);
    ABTS_INT_EQUAL(tc, 2, (int)sess->qos_ext.set[1].index);
    ABTS_INT_EQUAL(tc, 32000, (int)sess->qos_ext.set[1].gfbr_dl);
    ABTS_INT_EQUAL(tc, 300, (int)sess->qos_ext.set[1].pdb);
    ABTS_INT_EQUAL(tc, 3, (int)sess->qos_ext.set[1].per_exponent);

    /* the NG-RAN fulfils the first alternative: the AF is told that alternative */
    sendbuf = testngap_build_qos_flow_notify_alternative(qos_flow, 1);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, testgnb_ngap_send(ngap, sendbuf));
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 1, af_sess->notify_qnc_count);
    ABTS_STR_EQUAL(tc, "NOT_GUARANTEED", af_sess->notify_qnc_type);
    ABTS_STR_EQUAL(tc, "test-audio-alt1", af_sess->notify_qnc_alt);

    sendbuf = testngap_build_qos_flow_notify_alternative(qos_flow, 2);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, testgnb_ngap_send(ngap, sendbuf));
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 2, af_sess->notify_qnc_count);
    ABTS_STR_EQUAL(tc, "test-audio-alt2", af_sess->notify_qnc_alt);

    /* not even the lowest alternative is fulfilled: no alternative is named */
    sendbuf = testngap_build_qos_flow_notify_alternative(qos_flow, 0);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, testgnb_ngap_send(ngap, sendbuf));
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 3, af_sess->notify_qnc_count);
    ABTS_STR_EQUAL(tc, "NOT_GUARANTEED", af_sess->notify_qnc_type);
    ABTS_STR_EQUAL(tc, "", af_sess->notify_qnc_alt);

    /* the QoS reference is fulfilled again */
    sendbuf = testngap_build_qos_flow_notify(qos_flow, true);
    ABTS_PTR_NOTNULL(tc, sendbuf);
    ABTS_INT_EQUAL(tc, OGS_OK, testgnb_ngap_send(ngap, sendbuf));
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, 4, af_sess->notify_qnc_count);
    ABTS_STR_EQUAL(tc, "GUARANTEED", af_sess->notify_qnc_type);

    /* a null altSerReqs removes the alternative sets from the QoS profile */
    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"altSerReqs\":null}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ABTS_INT_EQUAL(tc, 0, sess->qos_ext.count);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/* An alternative QoS reference that the PCF is not configured with is refused like an unknown qosReference (TS 29.514 V19.8.0 clause 4.2.2.2) */
static void test30_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    test_af_qos_reference_setup(tc, "0000000030",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.alt_ser_req1 = "no-such-reference";
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_BAD_REQUEST, af_sess->last_pcf_status);

    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/*
 * TS 29.514 V19.8.0 clause 4.2.2.39, TS 29.512 V19.7.0 clause 4.2.6.6.2, TS 38.413 V19.1.0 clause 9.3.1.151 (PDU Set QoS Parameters): the AF gives
 * the PDU Set QoS parameters of the downlink, the PCF puts them in the QosData of the rule and the SMF tells the NG-RAN; a null value
 * removes them.
 */
static void test31_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;

    test_af_qos_reference_setup(tc, "0000000031",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.pdu_set_qos = true;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    ABTS_TRUE(tc, sess->qos_ext.pdu_set);
    ABTS_INT_EQUAL(tc, 1500, (int)sess->qos_ext.pdu_set_dl_budget);
    ABTS_INT_EQUAL(tc, 1, (int)sess->qos_ext.pdu_set_dl_per_scalar);
    ABTS_INT_EQUAL(tc, 3, (int)sess->qos_ext.pdu_set_dl_per_exponent);
    ABTS_INT_EQUAL(tc, 0, (int)sess->qos_ext.pdu_set_dl_handling);

    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"pduSetQosDl\":null}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ABTS_TRUE(tc, !sess->qos_ext.pdu_set);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}


/*
 * TS 29.514 V19.8.0 clauses 4.2.2.39, 4.2.2.47 and 4.2.2.48, TS 29.512 V19.7.0 clauses 4.2.6.10.3 to 4.2.6.10.9: the AF gives a protocol
 * description, the indications of the data burst size and the time to the next burst, the expedited transfer indication and the multiplexed
 * media identification of a sub component. The PCF has MpxMedia and TrafficCharChange only because the SMF offers them, and tells the AF so.
 */
static void test32_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;

    test_af_qos_reference_setup(tc, "0000000032",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_VIDEO;
    af_param.qos_reference = "test-video";
    af_param.proto_desc = true;
    af_param.burst_inds = true;
    af_param.exp_tran = true;
    af_param.mpx = true;
    af_param.qos_type = 1;
    af_param.flow_type = 1;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);
    ABTS_TRUE(tc, OGS_SBI_FEATURES_HI_IS_SET(af_sess->policyauthorization_features_hi, OGS_SBI_NPCF_POLICYAUTHORIZATION_MPX_MEDIA));
    ABTS_TRUE(tc, OGS_SBI_FEATURES_HI_IS_SET(af_sess->policyauthorization_features_hi, OGS_SBI_NPCF_POLICYAUTHORIZATION_TRAFFIC_CHAR_CHANGE));

    /* a null protoDescDl and indications that are given up */
    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"protoDescDl\":null,\"datBurstSizeInd\":null,"
        "\"timetoNextBurstInd\":null,\"expTranInd\":null}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}


/*
 * TS 29.514 V19.8.0 clause 4.2.5.8, TS 29.512 V19.7.0 clause 4.2.4.14, TS 38.413 V19.1.0 clause 9.3.1.152: the NG-RAN sets the QoS flow up with the
 * second alternative QoS parameters set (Current QoS Parameters Set Index), the SMF reports the rule with that set (altQosParamId) and the AF is told
 * the alternative service requirement as the QoS reference it gave.
 */
static void test33_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;

    test_af_qos_reference_setup(tc, "0000000033",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.alt_ser_req1 = "test-audio-alt1";
    af_param.alt_ser_req2 = "test-audio-alt2";
    af_param.ev_resources_allocation = true;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    sess->alt_current_index = 2;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(300);

    ABTS_INT_EQUAL(tc, 1, af_sess->notify_count);
    ABTS_STR_EQUAL(tc, "SUCCESSFUL_RESOURCES_ALLOCATION", af_sess->notify_event);
    ABTS_STR_EQUAL(tc, "test-audio-alt2", af_sess->notify_succ_alt);

    sess->alt_current_index = 0;
    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}


/* An RTP packet with the RTP header extension for PDU Set marking (TS 26.522 V19.4.0 clause 4.2) in the one-byte or the two-byte format, with the PDU Set Size if pssize is not 0 */
static bool test_af_send_rtp_he(test_sess_t *sess, uint32_t timestamp, uint16_t seq, uint8_t pt, bool two_byte, uint8_t id,
        bool end, uint8_t psi, uint16_t pssn, uint8_t psn, uint32_t pssize)
{
    struct sockaddr_in to;
    uint8_t rtp[64], *e;
    int fd, rv, len = 0, data_len = pssize ? 6 : 3, element_len = (two_byte ? 2 : 1) + data_len, words;

    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(30000);
    memcpy(&to.sin_addr, &sess->ue_ip.addr, sizeof(to.sin_addr));
    memset(rtp, 0, sizeof(rtp));
    rtp[0] = 0x90;                                  /* version 2, extension */
    rtp[1] = (end ? 0x80 : 0) | pt;
    rtp[2] = seq >> 8; rtp[3] = seq & 0xff;
    rtp[4] = timestamp >> 24; rtp[5] = timestamp >> 16; rtp[6] = timestamp >> 8; rtp[7] = timestamp;
    rtp[11] = 0x01;                                 /* SSRC */
    words = (element_len + 3) / 4;
    rtp[12] = two_byte ? 0x10 : 0xbe; rtp[13] = two_byte ? 0x00 : 0xde;
    rtp[14] = 0; rtp[15] = words;
    e = rtp + 16;
    if (two_byte) {
        *e++ = id;
        *e++ = data_len;
    } else
        *e++ = (id << 4) | (data_len - 1);
    *e++ = (end ? 0x80 : 0) | (psi & 0x0f);
    *e++ = pssn >> 2;
    *e++ = ((pssn & 3) << 6) | (psn & 0x3f);
    if (pssize) {
        *e++ = pssize >> 16; *e++ = pssize >> 8; *e++ = pssize;
    }
    len = 16 + 4 * words + 4;                       /* header, extension, a payload of four octets */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;
    rv = sendto(fd, rtp, len, 0, (struct sockaddr *)&to, sizeof(to));
    close(fd);
    return rv == len;
}

/*
 * TS 29.244 V19.6.0 clause 5.38.3, TS 29.281 V19.2.0 clause 5.2.2.8, TS 38.415 V19.1.0 clause 6.5.2.1, TS 38.413 V19.1.0 clause 9.3.1.268: the AF gives
 * PDU Set QoS parameters, the NG-RAN says that it supports PDU Set based Handling, and the SMF has the UPF mark the downlink packets of the QoS
 * flow. The UPF identifies the PDU Sets of RTP packets by the RTP timestamp, the marker bit ends a set, and the PDU Set Information Container follows the
 * PDU Session Container with the QFI of the flow. When the parameters are removed the marking stops.
 */
static void test34_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    ogs_gtp2_header_desc_t d;

    test_af_qos_reference_setup(tc, "0000000034",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.pdu_set_qos = true;
    af_param.qos_type = 1;
    af_param.flow_type = 98;
    sess->pdu_set_indicator = true;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(800);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    /* a PDU Set of three RTP packets, then one of a single packet */
    ABTS_TRUE(tc, test_af_send_rtp(sess, 1000, 1, false));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_TRUE(tc, d.pdu_set.presence);
    ABTS_INT_EQUAL(tc, qos_flow->qfi, d.qos_flow_identifier);
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.psn);
    ABTS_TRUE(tc, !d.pdu_set.end_pdu);
    ABTS_TRUE(tc, test_af_send_rtp(sess, 1000, 2, false));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 1, d.pdu_set.psn);
    ABTS_TRUE(tc, !d.pdu_set.end_pdu);
    ABTS_TRUE(tc, test_af_send_rtp(sess, 1000, 3, true));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 2, d.pdu_set.psn);
    ABTS_TRUE(tc, d.pdu_set.end_pdu);
    ABTS_TRUE(tc, test_af_send_rtp(sess, 4000, 4, true));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, 1, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.psn);
    ABTS_TRUE(tc, d.pdu_set.end_pdu);
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.psi);

    /* the PDU Set QoS parameters are removed: no more marking */
    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"pduSetQosDl\":null}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_msleep(800);
    ABTS_TRUE(tc, test_af_send_rtp(sess, 7000, 5, true));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_TRUE(tc, !d.pdu_set.presence);

    sess->pdu_set_indicator = false;
    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}


/*
 * TS 29.244 V19.6.0 clause 5.38.3 and tables 7.5.2.2-7 and 8.2.233 to 8.2.237, TS 26.522 V19.4.0 clause 4.2: the AF gives PDU Set QoS parameters and a
 * Protocol Description (protoDescDl) with the RTP header extension for PDU Set marking and a payload type; the SMF sends the Protocol Description in the
 * DL PDR and the UPF takes the PDU Set information from the extension of the RTP packets of that payload type, in the one-byte and in the two-byte
 * format, with the PDU Set Size when the extension has it. A packet without the extension, and one of another payload type, is a PDU Set of its own.
 */
static void test36_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    ogs_gtp2_header_desc_t d;

    test_af_qos_reference_setup(tc, "0000000036",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.pdu_set_qos = true;
    af_param.proto_desc = true;
    af_param.proto_desc_he_id = 5;
    af_param.proto_desc_pt = 96;
    af_param.qos_type = 1;
    af_param.flow_type = 98;
    sess->pdu_set_indicator = true;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(800);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    /* the one-byte format: the sender's PDU Set Sequence Number, PDU Sequence Number, importance and end */
    ABTS_TRUE(tc, test_af_send_rtp_he(sess, 1000, 1, 96, false, 5, false, 3, 700, 0, 0));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_TRUE(tc, d.pdu_set.presence);
    ABTS_INT_EQUAL(tc, qos_flow->qfi, d.qos_flow_identifier);
    ABTS_INT_EQUAL(tc, 700, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.psn);
    ABTS_INT_EQUAL(tc, 3, d.pdu_set.psi);
    ABTS_TRUE(tc, !d.pdu_set.end_pdu);
    ABTS_TRUE(tc, !d.pdu_set.pssize_presence);
    ABTS_TRUE(tc, test_af_send_rtp_he(sess, 1000, 2, 96, false, 5, true, 3, 700, 1, 0));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, 700, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 1, d.pdu_set.psn);
    ABTS_TRUE(tc, d.pdu_set.end_pdu);

    /* the two-byte format with the PDU Set Size of 1200 octets, the PDU Set Sequence Number at its limit of 1023 */
    ABTS_TRUE(tc, test_af_send_rtp_he(sess, 2000, 3, 96, true, 5, true, 1, 1023, 63, 1200));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, 1023, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 63, d.pdu_set.psn);
    ABTS_INT_EQUAL(tc, 1, d.pdu_set.psi);
    ABTS_TRUE(tc, d.pdu_set.end_pdu);
    ABTS_TRUE(tc, d.pdu_set.pssize_presence);
    ABTS_INT_EQUAL(tc, 1200, (int)d.pdu_set.pssize);

    /* an element with another ID is not the one of the Protocol Description; neither is a packet of a payload type that the UPF does not parse */
    ABTS_TRUE(tc, test_af_send_rtp_he(sess, 3000, 4, 96, false, 6, false, 5, 12, 0, 0));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.psi);
    ABTS_TRUE(tc, d.pdu_set.end_pdu);
    ABTS_TRUE(tc, test_af_send_rtp_he(sess, 4000, 5, 97, false, 5, false, 5, 500, 0, 0));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, 1, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.psi);
    ABTS_TRUE(tc, d.pdu_set.end_pdu);

    /* a new Protocol Description (another extension ID) reaches the UPF in an Update PDR */
    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"protoDescDl\":{\"transportProto\":\"RTP\","
        "\"rtpHeaderExtInfo\":{\"rtpHeaderExtType\":\"PDU_SET_MARKING\",\"rtpHeaderExtId\":7},"
        "\"rtpPayloadInfoList\":[{\"rtpPayloadTypeList\":[96]}]}}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(800);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    ABTS_TRUE(tc, test_af_send_rtp_he(sess, 5000, 6, 96, false, 7, true, 2, 33, 4, 0));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, 33, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 4, d.pdu_set.psn);
    ABTS_INT_EQUAL(tc, 2, d.pdu_set.psi);
    ABTS_TRUE(tc, test_af_send_rtp_he(sess, 6000, 7, 96, false, 5, true, 2, 99, 0, 0));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, 34, d.pdu_set.pssn);
    ABTS_INT_EQUAL(tc, 0, d.pdu_set.psi);

    sess->pdu_set_indicator = false;
    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}


/*
 * TS 29.244 V19.6.0 clause 5.38.6 and clause 8.2.5 (the SDF Filter with (S)RTP Multiplexed Media Identification Information): the AF gives the multiplexed
 * media information of a flow (mpxMediaDlInfos: an SSRC and a payload type), the SMF sends it in the SDF Filter of the DL PDR to a UPF that supports MUXMF,
 * and the UPF puts only the RTP packets that match an instance on the QoS flow; the packets of another SSRC or payload type go the default way.
 */
static void test37_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    ogs_gtp2_header_desc_t d;

    test_af_qos_reference_setup(tc, "0000000037",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.qos_type = 1;
    af_param.flow_type = 98;
    af_param.mpx = true;
    af_param.mpx_dl_ssrc = 1;
    af_param.mpx_dl_pt = 96;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(800);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    /* the SSRC and the payload type of the instance: the QoS flow */
    ABTS_TRUE(tc, test_af_send_rtp_ssrc(sess, 1000, 1, false, 96, 1));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, qos_flow->qfi, d.qos_flow_identifier);
    /* another SSRC, and another payload type: not the QoS flow */
    ABTS_TRUE(tc, test_af_send_rtp_ssrc(sess, 2000, 2, false, 96, 2));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_TRUE(tc, d.qos_flow_identifier != qos_flow->qfi);
    ABTS_TRUE(tc, test_af_send_rtp_ssrc(sess, 3000, 3, false, 97, 1));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_TRUE(tc, d.qos_flow_identifier != qos_flow->qfi);
    /* the instance again */
    ABTS_TRUE(tc, test_af_send_rtp_ssrc(sess, 4000, 4, true, 96, 1));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));
    ABTS_INT_EQUAL(tc, qos_flow->qfi, d.qos_flow_identifier);

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}


/*
 * TS 29.514 V19.8.0 table 5.6.2.3-1 (medComponents is optional) and clause 4.2.6.3 (subscription to events without service information): the PCF accepts an
 * application session context with an events subscription and no media components, makes no PCC rule for it, and the session can be deleted.
 */
static void test38_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;

    test_af_qos_reference_setup(tc, "0000000038",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.qos_type = 1;
    af_param.flow_type = 98;
    af_param.no_med_components = true;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    ogs_msleep(800);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_CREATED, af_sess->last_pcf_status);

    raw.method = "POST";
    raw.path = "/delete";
    raw.body = NULL;
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(500);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NO_CONTENT, af_sess->last_pcf_status);

    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}


/*
 * TS 29.514 V19.8.0 clause 5.6.2.7 (fStatus of a media component), TS 29.244 V19.6.0 clause 8.2.7 (gate status): an update that names only the fStatus of a
 * media component, with no QoS reference or media type, closes and opens the gate of the flow: the downlink packets stop and come again.
 */
static void test35_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;
    ogs_gtp2_header_desc_t d;

    test_af_qos_reference_setup(tc, "0000000035",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_reference = "test-audio";
    af_param.qos_type = 1;
    af_param.flow_type = 98;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);
    ogs_msleep(500);

    ABTS_TRUE(tc, test_af_send_rtp(sess, 1000, 1, true));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));

    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"fStatus\":\"DISABLED\"}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_msleep(800);
    ABTS_TRUE(tc, test_af_send_rtp(sess, 2000, 2, true));
    ABTS_TRUE(tc, !test_af_gtpu_read_header(gtpu, 1000, &d));

    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":{\"medCompN\":0,\"fStatus\":\"ENABLED\"}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);
    test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_msleep(800);
    ABTS_TRUE(tc, test_af_send_rtp(sess, 3000, 3, true));
    ABTS_TRUE(tc, test_af_gtpu_read_header(gtpu, 2000, &d));

    test_af_qos_reference_delete(tc, test_ue, sess, af_sess, ngap);
    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

/* One PATCH removes a media component and creates another: the SMF removes the QoS flow first, then creates the new one */
static void test39_func(abts_case *tc, void *data)
{
    ogs_socknode_t *ngap = NULL;
    ogs_socknode_t *gtpu = NULL;
    test_ue_t *test_ue = NULL;
    test_sess_t *sess = NULL;
    test_bearer_t *qos_flow = NULL;
    af_sess_t *af_sess = NULL;
    af_npcf_policyauthorization_param_t af_param;
    af_npcf_policyauthorization_raw_t raw;

    test_af_qos_reference_setup(tc, "0000000039",
            &test_ue, &sess, &af_sess, &ngap, &gtpu);

    memset(&af_param, 0, sizeof(af_param));
    af_param.med_type = OpenAPI_media_type_AUDIO;
    af_param.qos_type = 1;
    af_param.flow_type = 99;
    af_local_send_to_pcf(af_sess, &af_param,
            af_npcf_policyauthorization_build_create);
    qos_flow = test_af_complete_qos_flow_modify(tc, test_ue, sess, ngap);
    ogs_assert(qos_flow);

    /* media component 0 goes (null), media component 1 comes, in one decision of the PCF */
    raw.method = "PATCH";
    raw.path = "";
    raw.body = "{\"ascReqData\":{\"medComponents\":{\"0\":null,"
        "\"1\":{\"medCompN\":1,\"fStatus\":\"ENABLED\",\"medType\":\"AUDIO\","
        "\"marBwDl\":\"96000 bps\",\"marBwUl\":\"96000 bps\","
        "\"medSubComps\":{\"0\":{\"fNum\":0,\"flowUsage\":\"NO_INFO\","
        "\"fDescs\":[\"permit out 17 from 172.20.166.84 to 10.45.0.2 20001\","
        "\"permit in 17 from 10.45.0.2 to 172.20.166.84 20360\"]}}}}}}";
    af_local_send_to_pcf(af_sess, &raw, af_npcf_policyauthorization_build_raw);
    ogs_msleep(300);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, af_sess->last_pcf_status);

    /* The first PDU session modification command only releases the QoS flow */
    test_af_qos_flow_release(tc, test_ue, sess, ngap);
    ABTS_INT_EQUAL(tc, 1, sess->qos_flow_release);
    ABTS_INT_EQUAL(tc, 0, sess->qos_flow_add_or_modify);

    /* The second one, sent when the removal is answered, creates the new one: QFI 3, as the SMF takes QFIs in sequence */
    qos_flow = test_af_complete_qos_flow_modify_qfi(tc, test_ue, sess, ngap, 3);
    ogs_assert(qos_flow);
    ABTS_INT_EQUAL(tc, 0, sess->qos_flow_release);
    ABTS_INT_EQUAL(tc, 1, sess->qos_flow_add_or_modify);

    /* The SMF is up: the application session ends and the new QoS flow goes */
    af_local_send_to_pcf(af_sess, NULL,
            af_npcf_policyauthorization_build_delete);
    ogs_msleep(100);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_NO_CONTENT,
            af_sess->last_pcf_status);
    test_af_qos_flow_release_qfi(tc, test_ue, sess, ngap, 3);

    test_af_qos_reference_cleanup(tc, test_ue, ngap, gtpu);
}

abts_suite *test_af(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

/**
 * abts_suite test_af:
 *   - Always run test1_func through test7_func.
 *   - Conditionally include test8_func based on
 *     HOME_ROUTED_ROAMING_TEST flag.
 */
    abts_run_test(suite, test1_func, NULL);
    abts_run_test(suite, test2_func, NULL);
    abts_run_test(suite, test3_func, NULL);
    abts_run_test(suite, test4_func, NULL);
    abts_run_test(suite, test5_func, NULL);
    abts_run_test(suite, test6_func, NULL);
    abts_run_test(suite, test7_func, NULL);
#if !HOME_ROUTED_ROAMING_TEST
    abts_run_test(suite, test8_func, NULL);
#endif
    abts_run_test(suite, test10_func, NULL);
    abts_run_test(suite, test11_func, NULL);
    abts_run_test(suite, test12_func, NULL);
    abts_run_test(suite, test13_func, NULL);
    abts_run_test(suite, test14_func, NULL);
    abts_run_test(suite, test15_func, NULL);
    abts_run_test(suite, test16_func, NULL);
    abts_run_test(suite, test17_func, NULL);
    abts_run_test(suite, test18_func, NULL);
    abts_run_test(suite, test19_func, NULL);
    abts_run_test(suite, test20_func, NULL);
    abts_run_test(suite, test21_func, NULL);
    abts_run_test(suite, test22_func, NULL);
    abts_run_test(suite, test23_func, NULL);
    abts_run_test(suite, test24_func, NULL);
    abts_run_test(suite, test25_func, NULL);
    abts_run_test(suite, test26_func, NULL);
    abts_run_test(suite, test27_func, NULL);
    abts_run_test(suite, test28_func, NULL);
    abts_run_test(suite, test29_func, NULL);
    abts_run_test(suite, test30_func, NULL);
    abts_run_test(suite, test31_func, NULL);
    abts_run_test(suite, test32_func, NULL);
    abts_run_test(suite, test33_func, NULL);
    abts_run_test(suite, test34_func, NULL);
    abts_run_test(suite, test35_func, NULL);
    abts_run_test(suite, test36_func, NULL);
    abts_run_test(suite, test37_func, NULL);
    abts_run_test(suite, test38_func, NULL);
    abts_run_test(suite, test39_func, NULL);

    return suite;
}
