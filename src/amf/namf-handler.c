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

#include "namf-handler.h"
#include "nsmf-handler.h"

#include "nas-path.h"
#include "ngap-path.h"
#include "sbi-path.h"

static bool amf_namf_parse_tai(ogs_tai_t *tai, OpenAPI_tai_t *api_tai);
static bool amf_namf_parse_ncgi(ogs_ncgi_t *ncgi, OpenAPI_ncgi_t *api_ncgi);
static void amf_namf_parse_mbs_service_area(ogs_mbs_service_area_t **out, OpenAPI_mbs_service_area_t *api_area);

int amf_namf_comm_handle_n1_n2_message_transfer(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    int status, r;

    amf_ue_t *amf_ue = NULL;
    ran_ue_t *ran_ue = NULL;
    amf_sess_t *sess = NULL;

    ogs_pkbuf_t *n1buf = NULL;
    ogs_pkbuf_t *n2buf = NULL;

    ogs_pkbuf_t *gmmbuf = NULL;
    ogs_pkbuf_t *ngapbuf = NULL;

    char *supi = NULL;
    uint8_t pdu_session_id = OGS_NAS_PDU_SESSION_IDENTITY_UNASSIGNED;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    OpenAPI_n1_n2_message_transfer_req_data_t *N1N2MessageTransferReqData;
    OpenAPI_n1_n2_message_transfer_rsp_data_t N1N2MessageTransferRspData;
    OpenAPI_n1_message_container_t *n1MessageContainer = NULL;
    OpenAPI_ref_to_binary_data_t *n1MessageContent = NULL;
    OpenAPI_n2_info_container_t *n2InfoContainer = NULL;
    OpenAPI_n2_sm_information_t *smInfo = NULL;
    OpenAPI_n2_info_content_t *n2InfoContent = NULL;
    OpenAPI_ref_to_binary_data_t *ngapData = NULL;

    OpenAPI_ngap_ie_type_e ngapIeType = OpenAPI_ngap_ie_type_NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    N1N2MessageTransferReqData = recvmsg->N1N2MessageTransferReqData;
    if (!N1N2MessageTransferReqData) {
        ogs_error("No N1N2MessageTransferReqData");
        return OGS_ERROR;
    }

    if (N1N2MessageTransferReqData->is_pdu_session_id == false) {
        ogs_error("No PDU Session Identity");
        return OGS_ERROR;
    }
    pdu_session_id = N1N2MessageTransferReqData->pdu_session_id;

    supi = recvmsg->h.resource.component[1];
    if (!supi) {
        ogs_error("No SUPI");
        return OGS_ERROR;
    }

    amf_ue = amf_ue_find_by_supi(supi);
    if (!amf_ue) {
        ogs_error("No UE context [%s]", supi);
        return OGS_ERROR;
    }

    sess = amf_sess_find_by_psi(amf_ue, pdu_session_id);
    if (!sess) {
        ogs_error("[%s] No PDU Session Context [%d]",
                amf_ue->supi, pdu_session_id);
        return OGS_ERROR;
    }

    n1MessageContainer = N1N2MessageTransferReqData->n1_message_container;
    if (n1MessageContainer) {
        n1MessageContent = n1MessageContainer->n1_message_content;
        if (!n1MessageContent || !n1MessageContent->content_id) {
            ogs_error("No n1MessageContent");
            return OGS_ERROR;
        }

        n1buf = ogs_sbi_find_part_by_content_id(
                recvmsg, n1MessageContent->content_id);
        if (!n1buf) {
            ogs_error("[%s] No N1 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        /*
         * NOTE : The pkbuf created in the SBI message will be removed
         *        from ogs_sbi_message_free(), so it must be copied.
         */
        n1buf = ogs_pkbuf_copy(n1buf);
        ogs_assert(n1buf);
    }

    n2InfoContainer = N1N2MessageTransferReqData->n2_info_container;
    if (n2InfoContainer) {
        smInfo = n2InfoContainer->sm_info;
        if (!smInfo) {
            ogs_error("No smInfo");
            return OGS_ERROR;
        }

        n2InfoContent = smInfo->n2_info_content;
        if (!n2InfoContent) {
            ogs_error("No n2InfoContent");
            return OGS_ERROR;
        }

        ngapIeType = n2InfoContent->ngap_ie_type;

        ngapData = n2InfoContent->ngap_data;
        if (!ngapData || !ngapData->content_id) {
            ogs_error("No ngapData");
            return OGS_ERROR;
        }
        n2buf = ogs_sbi_find_part_by_content_id(
                recvmsg, ngapData->content_id);
        if (!n2buf) {
            ogs_error("[%s] No N2 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        /*
         * NOTE : The pkbuf created in the SBI message will be removed
         *        from ogs_sbi_message_free(), so it must be copied.
         */
        n2buf = ogs_pkbuf_copy(n2buf);
        ogs_assert(n2buf);
    }

    memset(&sendmsg, 0, sizeof(sendmsg));

    status = OGS_SBI_HTTP_STATUS_OK;

    memset(&N1N2MessageTransferRspData, 0, sizeof(N1N2MessageTransferRspData));
    N1N2MessageTransferRspData.cause =
        OpenAPI_n1_n2_message_transfer_cause_N1_N2_TRANSFER_INITIATED;

    sendmsg.N1N2MessageTransferRspData = &N1N2MessageTransferRspData;

    switch (ngapIeType) {
    case OpenAPI_ngap_ie_type_PDU_RES_SETUP_REQ:
        if (!n2buf) {
            ogs_error("[%s] No N2 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        if (n1buf) {
            gmmbuf = gmm_build_dl_nas_transport(sess,
                    OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, n1buf, 0, 0);
            ogs_assert(gmmbuf);
        }

        if (gmmbuf) {
            /***********************************
             * 4.3.2 PDU Session Establishment *
             ***********************************/

            ran_ue = ran_ue_find_by_id(sess->ran_ue_id);
            if (ran_ue) {
                if (sess->pdu_session_establishment_accept) {
                    ogs_pkbuf_free(sess->pdu_session_establishment_accept);
                    sess->pdu_session_establishment_accept = NULL;
                }

                if (ran_ue->initial_context_setup_request_sent == true) {
                    ngapbuf =
                        ngap_sess_build_pdu_session_resource_setup_request(
                                ran_ue, sess, gmmbuf, n2buf);
                    ogs_assert(ngapbuf);
                } else {
                    ngapbuf = ngap_sess_build_initial_context_setup_request(
                            ran_ue, sess, gmmbuf, n2buf);
                    ogs_assert(ngapbuf);

                    ran_ue->initial_context_setup_request_sent = true;
                }

                if (SESSION_CONTEXT_IN_SMF(sess)) {
                /*
                 * [1-CLIENT] /nsmf-pdusession/v1/sm-contexts
                 * [2-SERVER] /namf-comm/v1/ue-contexts/{supi}/n1-n2-messages
                 *
                 * If [2-SERVER] arrives after [1-CLIENT],
                 * sm-context-ref is created in [1-CLIENT].
                 * So, the PDU session establishment accpet can be transmitted.
                 */
                    r = ngap_send_to_ran_ue(ran_ue, ngapbuf);
                    ogs_expect(r == OGS_OK);
                    ogs_assert(r != OGS_ERROR);
                } else {
                    sess->pdu_session_establishment_accept = ngapbuf;
                }
            } else {
                ogs_warn("[%s] RAN-NG Context has already been removed",
                            amf_ue->supi);
            }

        } else {
            /*********************************************
             * 4.2.3.3 Network Triggered Service Request *
             *********************************************/

            if (CM_IDLE(amf_ue)) {
                bool rc;
                ogs_sbi_server_t *server = NULL;
                ogs_sbi_header_t header;
                ogs_sbi_client_t *client = NULL;
                OpenAPI_uri_scheme_e scheme = OpenAPI_uri_scheme_NULL;
                char *fqdn = NULL;
                uint16_t fqdn_port = 0;
                ogs_sockaddr_t *addr = NULL, *addr6 = NULL;

                if (!N1N2MessageTransferReqData->n1n2_failure_txf_notif_uri) {
                    ogs_error("[%s:%d] No n1-n2-failure-notification-uri",
                            amf_ue->supi, sess->psi);
                    return OGS_ERROR;
                }

                rc = ogs_sbi_getaddr_from_uri(
                        &scheme, &fqdn, &fqdn_port, &addr, &addr6,
                        N1N2MessageTransferReqData->n1n2_failure_txf_notif_uri);
                if (rc == false || scheme == OpenAPI_uri_scheme_NULL) {
                    ogs_error("[%s:%d] Invalid URI [%s]",
                            amf_ue->supi, sess->psi,
                            N1N2MessageTransferReqData->
                                n1n2_failure_txf_notif_uri);
                    return OGS_ERROR;
                }

                client = ogs_sbi_client_find(
                        scheme, fqdn, fqdn_port, addr, addr6);
                if (!client) {
                    ogs_debug("%s: ogs_sbi_client_add()", OGS_FUNC);
                    client = ogs_sbi_client_add(
                            scheme, fqdn, fqdn_port, addr, addr6);
                    if (!client) {
                        ogs_error("%s: ogs_sbi_client_add() failed", OGS_FUNC);

                        ogs_free(fqdn);
                        ogs_freeaddrinfo(addr);
                        ogs_freeaddrinfo(addr6);

                        return OGS_ERROR;
                    }
                }
                OGS_SBI_SETUP_CLIENT(&sess->paging, client);

                ogs_free(fqdn);
                ogs_freeaddrinfo(addr);
                ogs_freeaddrinfo(addr6);

                status = OGS_SBI_HTTP_STATUS_ACCEPTED;
                N1N2MessageTransferRspData.cause =
                    OpenAPI_n1_n2_message_transfer_cause_ATTEMPTING_TO_REACH_UE;

                /* Location */
                server = ogs_sbi_server_from_stream(stream);
                ogs_assert(server);

                memset(&header, 0, sizeof(header));
                header.service.name = (char *)OGS_SBI_SERVICE_NAME_NAMF_COMM;
                header.api.version = (char *)OGS_SBI_API_V1;
                header.resource.component[0] =
                    (char *)OGS_SBI_RESOURCE_NAME_UE_CONTEXTS;
                header.resource.component[1] = amf_ue->supi;
                header.resource.component[2] =
                    (char *)OGS_SBI_RESOURCE_NAME_N1_N2_MESSAGES;
                header.resource.component[3] = sess->sm_context.ref;

                sendmsg.http.location = ogs_sbi_server_uri(server, &header);

                /* Store Paging Info */
                AMF_SESS_STORE_PAGING_INFO(
                    sess, sendmsg.http.location,
                    N1N2MessageTransferReqData->n1n2_failure_txf_notif_uri);

                /* Store N2 Transfer message */
                AMF_SESS_STORE_N2_TRANSFER(
                        sess, pdu_session_resource_setup_request, n2buf);

                r = ngap_send_paging(amf_ue);
                ogs_expect(r == OGS_OK);
                ogs_assert(r != OGS_ERROR);

            } else if (CM_CONNECTED(amf_ue)) {
                r = nas_send_pdu_session_setup_request(sess, NULL, n2buf);
                ogs_expect(r == OGS_OK);
                ogs_assert(r != OGS_ERROR);

            } else {

                ogs_fatal("[%s] Invalid AMF-UE state", amf_ue->supi);
                ogs_assert_if_reached();

            }

        }
        break;

    case OpenAPI_ngap_ie_type_PDU_RES_MOD_REQ:
        if (!n1buf) {
            ogs_error("[%s] No N1 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }
        if (!n2buf) {
            ogs_error("[%s] No N2 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        if (CM_IDLE(amf_ue)) {
            ogs_sbi_server_t *server = NULL;
            ogs_sbi_header_t header;

            status = OGS_SBI_HTTP_STATUS_ACCEPTED;
            N1N2MessageTransferRspData.cause =
                OpenAPI_n1_n2_message_transfer_cause_ATTEMPTING_TO_REACH_UE;

            /* Location */
            server = ogs_sbi_server_from_stream(stream);
            ogs_assert(server);

            memset(&header, 0, sizeof(header));
            header.service.name = (char *)OGS_SBI_SERVICE_NAME_NAMF_COMM;
            header.api.version = (char *)OGS_SBI_API_V1;
            header.resource.component[0] =
                (char *)OGS_SBI_RESOURCE_NAME_UE_CONTEXTS;
            header.resource.component[1] = amf_ue->supi;
            header.resource.component[2] =
                (char *)OGS_SBI_RESOURCE_NAME_N1_N2_MESSAGES;
            header.resource.component[3] = sess->sm_context.ref;

            sendmsg.http.location = ogs_sbi_server_uri(server, &header);

            /* Store Paging Info */
            AMF_SESS_STORE_PAGING_INFO(sess, sendmsg.http.location, NULL);

            /* Store 5GSM Message */
            AMF_SESS_STORE_5GSM_MESSAGE(sess,
                    OGS_NAS_5GS_PDU_SESSION_MODIFICATION_COMMAND,
                    n1buf, n2buf);

            r = ngap_send_paging(amf_ue);
            ogs_expect(r == OGS_OK);
            ogs_assert(r != OGS_ERROR);

        } else if (CM_CONNECTED(amf_ue)) {
            if (CONTEXT_SETUP_ESTABLISHED(amf_ue)) {
                r = nas_send_pdu_session_modification_command(
                        sess, n1buf, n2buf);
                ogs_expect(r == OGS_OK);
                ogs_assert(r != OGS_ERROR);
            } else {
                /* Store 5GSM Message */
                ogs_warn("[Session MODIFY] Context setup is not established");
                AMF_SESS_STORE_5GSM_MESSAGE(sess,
                        OGS_NAS_5GS_PDU_SESSION_MODIFICATION_COMMAND,
                        n1buf, n2buf);
            }
        } else {
            ogs_fatal("[%s] Invalid AMF-UE state", amf_ue->supi);
            ogs_assert_if_reached();
        }

        break;

    case OpenAPI_ngap_ie_type_PDU_RES_REL_CMD:
        if (!n2buf) {
            ogs_error("[%s] No N2 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        if (CM_IDLE(amf_ue)) {
            if (N1N2MessageTransferReqData->is_skip_ind == true &&
                N1N2MessageTransferReqData->skip_ind == true) {

                if (n1buf)
                    ogs_pkbuf_free(n1buf);
                if (n2buf)
                    ogs_pkbuf_free(n2buf);

                N1N2MessageTransferRspData.cause =
                    OpenAPI_n1_n2_message_transfer_cause_N1_MSG_NOT_TRANSFERRED;

            } else {
                ogs_sbi_server_t *server = NULL;
                ogs_sbi_header_t header;

                status = OGS_SBI_HTTP_STATUS_ACCEPTED;
                N1N2MessageTransferRspData.cause =
                    OpenAPI_n1_n2_message_transfer_cause_ATTEMPTING_TO_REACH_UE;

                /* Location */
                server = ogs_sbi_server_from_stream(stream);
                ogs_assert(server);

                memset(&header, 0, sizeof(header));
                header.service.name = (char *)OGS_SBI_SERVICE_NAME_NAMF_COMM;
                header.api.version = (char *)OGS_SBI_API_V1;
                header.resource.component[0] =
                    (char *)OGS_SBI_RESOURCE_NAME_UE_CONTEXTS;
                header.resource.component[1] = amf_ue->supi;
                header.resource.component[2] =
                    (char *)OGS_SBI_RESOURCE_NAME_N1_N2_MESSAGES;
                header.resource.component[3] = sess->sm_context.ref;

                sendmsg.http.location = ogs_sbi_server_uri(server, &header);

                /* Store Paging Info */
                AMF_SESS_STORE_PAGING_INFO(sess, sendmsg.http.location, NULL);

                /* Store 5GSM Message */
                AMF_SESS_STORE_5GSM_MESSAGE(sess,
                        OGS_NAS_5GS_PDU_SESSION_RELEASE_COMMAND,
                        n1buf, n2buf);

                r = ngap_send_paging(amf_ue);
                ogs_expect(r == OGS_OK);
                ogs_assert(r != OGS_ERROR);
            }

        } else if (CM_CONNECTED(amf_ue)) {
            if (CONTEXT_SETUP_ESTABLISHED(amf_ue)) {
                r = nas_send_pdu_session_release_command(sess, n1buf, n2buf);
                ogs_expect(r == OGS_OK);
                ogs_assert(r != OGS_ERROR);
            } else {
                /* Store 5GSM Message */
                ogs_warn("[Session RELEASE] Context setup is not established");
                AMF_SESS_STORE_5GSM_MESSAGE(sess,
                        OGS_NAS_5GS_PDU_SESSION_RELEASE_COMMAND,
                        n1buf, n2buf);
            }
        } else {
            ogs_fatal("[%s] Invalid AMF-UE state", amf_ue->supi);
            ogs_assert_if_reached();
        }
        break;

    case OpenAPI_ngap_ie_type_NULL:
        /*
         * No n2InfoContainer. According to TS23.502, this means that SMF has
         * encountered an error and is rejecting the session.
         *
         * TS23.502
         * 6.3.1.7 4.3.2.2 UE Requested PDU Session Establishment
         * p100
         * 11.  ...
         * If the PDU session establishment failed anywhere between step 5
         * and step 11, then the Namf_Communication_N1N2MessageTransfer
         * request shall include the N1 SM container with a PDU Session
         * Establishment Reject message ...
         */
        if (!n1buf) {
            ogs_error("[%s] No N1 SM Content", amf_ue->supi);
            return OGS_ERROR;
        }

        ogs_error("[%d:%d] PDU session establishment reject",
                sess->psi, sess->pti);

        r = nas_5gs_send_gsm_reject(
                ran_ue_find_by_id(sess->ran_ue_id), sess,
                OGS_NAS_PAYLOAD_CONTAINER_N1_SM_INFORMATION, n1buf);
        ogs_expect(r == OGS_OK);
        ogs_assert(r != OGS_ERROR);

        amf_sess_remove(sess);
        break;

    default:
        ogs_error("Not implemented ngapIeType[%d]", ngapIeType);
        ogs_assert_if_reached();
    }

    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    if (sendmsg.http.location)
        ogs_free(sendmsg.http.location);

    return OGS_OK;
}

int amf_namf_callback_handle_sm_context_status(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    int status = OGS_SBI_HTTP_STATUS_NO_CONTENT;

    amf_ue_t *amf_ue = NULL;
    amf_sess_t *sess = NULL;

    uint8_t pdu_session_identity;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    OpenAPI_sm_context_status_notification_t *SmContextStatusNotification;
    OpenAPI_status_info_t *StatusInfo;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    if (!recvmsg->h.resource.component[0]) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("No SUPI");
        goto cleanup;
    }

    amf_ue = amf_ue_find_by_supi(recvmsg->h.resource.component[0]);
    if (!amf_ue) {
        status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
        ogs_error("Cannot find SUPI [%s]", recvmsg->h.resource.component[0]);
        goto cleanup;
    }

    if (!recvmsg->h.resource.component[2]) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s] No PDU Session Identity", amf_ue->supi);
        goto cleanup;
    }

    pdu_session_identity = atoi(recvmsg->h.resource.component[2]);
    if (pdu_session_identity == OGS_NAS_PDU_SESSION_IDENTITY_UNASSIGNED) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s] PDU Session Identity is unassigned", amf_ue->supi);
        goto cleanup;
    }

    sess = amf_sess_find_by_psi(amf_ue, pdu_session_identity);
    if (!sess) {
        status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
        ogs_warn("[%s] Cannot find session", amf_ue->supi);
        goto cleanup;
    }

    SmContextStatusNotification = recvmsg->SmContextStatusNotification;
    if (!SmContextStatusNotification) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s:%d] No SmContextStatusNotification",
                amf_ue->supi, sess->psi);
        goto cleanup;
    }

    StatusInfo = SmContextStatusNotification->status_info;
    if (!StatusInfo) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s:%d] No StatusInfo", amf_ue->supi, sess->psi);
        goto cleanup;
    }

    sess->resource_status = StatusInfo->resource_status;

    /*
     * Race condition for PDU session release complete
     *  - CLIENT : /nsmf-pdusession/v1/sm-contexts/{smContextRef}/modify
     *  - SERVER : /namf-callback/v1/{supi}/sm-context-status/{psi})
     *
     * If NOTIFICATION is received before the CLIENT response is received,
     * CLIENT sync is not finished. In this case, the session context
     * should not be removed.
     *
     * If NOTIFICATION comes after the CLIENT response is received,
     * sync is done. So, the session context can be removed.
     */
    ogs_info("[%s:%d][%d:%d:%s] "
            "/namf-callback/v1/{supi}/sm-context-status/{psi}",
            amf_ue->supi, sess->psi,
            sess->n1_released, sess->n2_released,
            OpenAPI_resource_status_ToString(sess->resource_status));

    if (sess->n1_released == true &&
        sess->n2_released == true &&
        sess->resource_status == OpenAPI_resource_status_RELEASED) {
        amf_nsmf_pdusession_handle_release_sm_context(
                sess, AMF_RELEASE_SM_CONTEXT_NO_STATE);
    }

cleanup:
    memset(&sendmsg, 0, sizeof(sendmsg));

    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return OGS_OK;
}

int amf_namf_callback_handle_dereg_notify(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    int r, state, status = OGS_SBI_HTTP_STATUS_NO_CONTENT;

    amf_ue_t *amf_ue = NULL;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    OpenAPI_deregistration_data_t *DeregistrationData;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    if (!recvmsg->h.resource.component[0]) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("No SUPI");
        goto cleanup;
    }

    amf_ue = amf_ue_find_by_supi(recvmsg->h.resource.component[0]);
    if (!amf_ue) {
        status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
        ogs_error("Cannot find SUPI [%s]", recvmsg->h.resource.component[0]);
        goto cleanup;
    }

    DeregistrationData = recvmsg->DeregistrationData;
    if (!DeregistrationData) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s] No DeregistrationData", amf_ue->supi);
        goto cleanup;
    }

    if (DeregistrationData->dereg_reason ==
            OpenAPI_deregistration_reason_NULL) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s] No Deregistraion Reason ", amf_ue->supi);
        goto cleanup;
    }

    if (DeregistrationData->access_type != OpenAPI_access_type_3GPP_ACCESS) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("[%s] Deregistration access type not 3GPP", amf_ue->supi);
        goto cleanup;
    }

    ogs_info("Deregistration notify reason: %s:%s:%s",
        amf_ue->supi,
        OpenAPI_deregistration_reason_ToString(DeregistrationData->dereg_reason),
        OpenAPI_access_type_ToString(DeregistrationData->access_type));

    /*
     * TODO: do not start deregistration if UE has emergency sessions
     * 4.2.2.3.3
     * If the UE has established PDU Session associated with emergency service, the AMF shall not initiate
     * Deregistration procedure. In this case, the AMF performs network requested PDU Session Release for any PDU
     * session associated with non-emergency service as described in clause 4.3.4.
     */

    /*
     * - AMF_NETWORK_INITIATED_EXPLICIT_DE_REGISTERED
     * 1. UDM_UECM_DeregistrationNotification
     * 2. Deregistration request
     * 3. UDM_SDM_Unsubscribe
     * 4. UDM_UECM_Deregisration
     * 5. PDU session release request
     * 6. PDUSessionResourceReleaseCommand +
     *    PDU session release command
     * 7. PDUSessionResourceReleaseResponse
     * 8. AM_Policy_Association_Termination
     * 9.  Deregistration accept
     * 10. Signalling Connecion Release
     */
    if (CM_CONNECTED(amf_ue)) {
        r = nas_5gs_send_de_registration_request(
                amf_ue,
                DeregistrationData->dereg_reason,
                OGS_5GMM_CAUSE_5GS_SERVICES_NOT_ALLOWED);
        ogs_expect(r == OGS_OK);
        ogs_assert(r != OGS_ERROR);

        state = AMF_NETWORK_INITIATED_EXPLICIT_DE_REGISTERED;

    } else if (CM_IDLE(amf_ue)) {
        ogs_error("Not implemented : Use Implicit De-registration");

        state = AMF_NETWORK_INITIATED_IMPLICIT_DE_REGISTERED;

    } else {
        ogs_fatal("Invalid State");
        ogs_assert_if_reached();
    }

    if (UDM_SDM_SUBSCRIBED(amf_ue)) {
        r = amf_ue_sbi_discover_and_send(
                OGS_SBI_SERVICE_TYPE_NUDM_SDM, NULL,
                amf_nudm_sdm_build_subscription_delete,
                amf_ue, state, NULL);
        ogs_expect(r == OGS_OK);
        ogs_assert(r != OGS_ERROR);
    } else if (PCF_AM_POLICY_ASSOCIATED(amf_ue)) {
        r = amf_ue_sbi_discover_and_send(
                OGS_SBI_SERVICE_TYPE_NPCF_AM_POLICY_CONTROL,
                NULL,
                amf_npcf_am_policy_control_build_delete,
                amf_ue, state, NULL);
        ogs_expect(r == OGS_OK);
        ogs_assert(r != OGS_ERROR);
    }

cleanup:
    memset(&sendmsg, 0, sizeof(sendmsg));

    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return OGS_OK;
}

static int update_rat_res_add_one(cJSON *restriction,
                                  OpenAPI_list_t *restrictions, long index)
{
    void *restr;

    if (!cJSON_IsString(restriction)) {
        ogs_error("Invalid type of ratRestriction element");
        return OGS_ERROR;
    }

    restr = (void *) OpenAPI_rat_type_FromString(cJSON_GetStringValue(restriction));
    if (!restr) {
        ogs_error("No restr");
        return OGS_ERROR;
    }

    if (index == restrictions->count) {
        OpenAPI_list_add(restrictions, restr);
    } else if (restrictions->count < index && index <= 0) {
        OpenAPI_list_insert_prev(
            restrictions, OpenAPI_list_find(restrictions, index), restr);
    } else {
        ogs_error("Can't add RAT restriction to invalid index");
        return OGS_ERROR;
    }
    return OGS_OK;
}

static int update_rat_res_array(cJSON *json_restrictions,
                                OpenAPI_list_t *restrictions)
{
    cJSON *restriction;

    if (!cJSON_IsArray(json_restrictions)) {
        ogs_error("Invalid type of ratRestrictions");
        return OGS_ERROR;
    }

    OpenAPI_list_clear(restrictions);

    cJSON_ArrayForEach(restriction, json_restrictions) {
        if (update_rat_res_add_one(restriction, restrictions,
                                   restrictions->count) != OGS_OK) {
            return OGS_ERROR;
        }
    }
    return OGS_OK;
}

static int update_rat_res(OpenAPI_change_item_t *item_change,
                          OpenAPI_list_t *restrictions)
{
    cJSON* json = item_change->new_value->json;
    cJSON* json_restrictions;

    if (!item_change->path) {
        return OGS_ERROR;
    }

    switch (item_change->op) {
    case OpenAPI_change_type_REPLACE:
    case OpenAPI_change_type_ADD:
        if (!strcmp(item_change->path, "")) {
            if (!cJSON_IsObject(json)) {
                ogs_error("Invalid type of am-data");
            }
            json_restrictions = cJSON_GetObjectItemCaseSensitive(
                                    json, "ratRestrictions");
            if (json_restrictions) {
                return update_rat_res_array(json_restrictions, restrictions);
            } else {
                return OGS_OK;
            }
        } else if (!strcmp(item_change->path, "/ratRestrictions")) {
            return update_rat_res_array(json, restrictions);
        } else if (strstr(item_change->path, "/ratRestrictions/") ==
                   item_change->path) {
            char *index = item_change->path + strlen("/ratRestrictions/");
            long i = strcmp(index, "-") ? atol(index) : restrictions->count;

            return update_rat_res_add_one(json, restrictions, i);
        }
        return OGS_OK;

    case OpenAPI_change_type__REMOVE:
        if (!strcmp(item_change->path, "")) {
            OpenAPI_list_clear(restrictions);
            return OGS_OK;
        } else if (!strcmp(item_change->path, "/ratRestrictions")) {
            OpenAPI_list_clear(restrictions);
            return OGS_OK;
        } else if (strstr(item_change->path, "/ratRestrictions/") ==
                   item_change->path) {
            char *index = item_change->path + strlen("/ratRestrictions/");
            long i = atol(index);

            if (restrictions->count < i && i <= 0) {
            OpenAPI_list_remove(
                restrictions, OpenAPI_list_find(restrictions, i));
            } else {
                ogs_error("Can't add RAT restriction to invalid index");
                return OGS_ERROR;
            }
        }
        return OGS_OK;

    default:
        return OGS_OK;
    }

}

static int update_ambr_check_one(cJSON *obj, uint64_t *limit,
                                 bool *ambr_changed)
{
    if (!cJSON_IsString(obj)) {
        ogs_error("Invalid type of subscribedUeAmbr");
        return OGS_ERROR;
    }
    *limit = ogs_sbi_bitrate_from_string(obj->valuestring);
    *ambr_changed = true;
    return OGS_OK;
}

static int update_ambr_check_obj(cJSON *obj, ogs_bitrate_t *ambr,
                                 bool *ambr_changed)
{
    if (!cJSON_IsObject(obj)) {
        if (obj == NULL || cJSON_IsNull(obj)) {
            /* Limit of 0 means unlimited. */
            ambr->uplink = 0;
            ambr->downlink = 0;
            *ambr_changed = true;
            return OGS_OK;
        } else {
            ogs_error("Invalid type of subscribedUeAmbr");
            return OGS_ERROR;
        }
    }

    if (update_ambr_check_one(
            cJSON_GetObjectItemCaseSensitive(obj, "uplink"),
            &ambr->uplink, ambr_changed)) {
        return OGS_ERROR;
    }
    if (update_ambr_check_one(
            cJSON_GetObjectItemCaseSensitive(obj, "downlink"),
            &ambr->downlink, ambr_changed)) {
        return OGS_ERROR;
    }
    return OGS_OK;
}

static int update_ambr(OpenAPI_change_item_t *item_change,
                       ogs_bitrate_t *ambr, bool *ambr_changed)
{
    cJSON* json = item_change->new_value->json;

    if (!item_change->path) {
        return OGS_ERROR;
    }

    switch (item_change->op) {
    case OpenAPI_change_type_REPLACE:
    case OpenAPI_change_type_ADD:
        if (!strcmp(item_change->path, "")) {
            if (!cJSON_IsObject(json)) {
                ogs_error("Invalid type of am-data");
            }
            return update_ambr_check_obj(
                cJSON_GetObjectItemCaseSensitive(json, "subscribedUeAmbr"),
                ambr, ambr_changed);
        } else if (!strcmp(item_change->path, "/subscribedUeAmbr")) {
            return update_ambr_check_obj(json, ambr, ambr_changed);
        } else if (!strcmp(item_change->path, "/subscribedUeAmbr/uplink")) {
            return update_ambr_check_one(json, &ambr->uplink, ambr_changed);
        } else if (!strcmp(item_change->path, "/subscribedUeAmbr/downlink")) {
            return update_ambr_check_one(json, &ambr->downlink, ambr_changed);
        }
        return OGS_OK;


    case OpenAPI_change_type__REMOVE:
        if (!strcmp(item_change->path, "/subscribedUeAmbr")) {
            update_ambr_check_obj(NULL, ambr, ambr_changed);
        }
        return OGS_OK;
    default:
        return OGS_OK;
    }
}

int amf_namf_callback_handle_sdm_data_change_notify(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    int r, state, status = OGS_SBI_HTTP_STATUS_NO_CONTENT;

    amf_ue_t *amf_ue = NULL;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    OpenAPI_modification_notification_t *ModificationNotification;
    OpenAPI_lnode_t *node;

    char *ueid = NULL;
    char *res_name = NULL;

    bool ambr_changed = false;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    ModificationNotification = recvmsg->ModificationNotification;
    if (!ModificationNotification) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        ogs_error("No ModificationNotification");
        goto cleanup;
    }

    OpenAPI_list_for_each(ModificationNotification->notify_items, node) {
        OpenAPI_notify_item_t *item = node->data;

        char *saveptr = NULL;

        ueid = ogs_sbi_parse_uri(item->resource_id, "/", &saveptr);
        if (!ueid) {
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            ogs_error("[%s] No UeId", item->resource_id);
            goto cleanup;
        }

        amf_ue = amf_ue_find_by_supi(ueid);
        if (!amf_ue) {
            status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
            ogs_error("Cannot find SUPI [%s]", ueid);
            goto cleanup;
        }

        res_name = ogs_sbi_parse_uri(NULL, "/", &saveptr);
        if (!res_name) {
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            ogs_error("[%s] No Resource Name", item->resource_id);
            goto cleanup;
        }

        SWITCH(res_name)
        CASE(OGS_SBI_RESOURCE_NAME_AM_DATA)
            OpenAPI_lnode_t *node_ci;

            OpenAPI_list_for_each(item->changes, node_ci) {
                OpenAPI_change_item_t *change_item = node_ci->data;
                if (update_rat_res(change_item, amf_ue->rat_restrictions) ||
                        update_ambr(change_item, &amf_ue->ue_ambr,
                            &ambr_changed)) {
                    status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
                    goto cleanup;
                }
            }
            break;
        DEFAULT
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            ogs_error("Unknown Resource Name: [%s]", res_name);
            goto cleanup;
        END

        ogs_free(ueid);
        ogs_free(res_name);

        ueid = NULL;
        res_name = NULL;
    }

    if (amf_ue) {
        ran_ue_t *ran_ue = ran_ue_find_by_id(amf_ue->ran_ue_id);
        if (!ran_ue) {
            ogs_error("NG context has already been removed");
            /* ran_ue is required for amf_ue_is_rat_restricted() */

            ogs_error("Not implemented : Use Implicit De-registration");
            state = AMF_NETWORK_INITIATED_IMPLICIT_DE_REGISTERED;

        } else if (amf_ue_is_rat_restricted(amf_ue)) {
            /*
             * - AMF_NETWORK_INITIATED_EXPLICIT_DE_REGISTERED
             * 1. UDM_UECM_DeregistrationNotification
             * 2. Deregistration request
             * 3. UDM_SDM_Unsubscribe
             * 4. UDM_UECM_Deregistration
             * 5. PDU session release request
             * 6. PDUSessionResourceReleaseCommand +
             *    PDU session release command
             * 7. PDUSessionResourceReleaseResponse
             * 8. AM_Policy_Association_Termination
             * 9.  Deregistration accept
             * 10. Signalling Connection Release
             */
            r = nas_5gs_send_de_registration_request(
                    amf_ue,
                    OpenAPI_deregistration_reason_REREGISTRATION_REQUIRED, 0);
            ogs_expect(r == OGS_OK);
            ogs_assert(r != OGS_ERROR);

            state = AMF_NETWORK_INITIATED_EXPLICIT_DE_REGISTERED;

            if (UDM_SDM_SUBSCRIBED(amf_ue)) {
                r = amf_ue_sbi_discover_and_send(
                        OGS_SBI_SERVICE_TYPE_NUDM_SDM, NULL,
                        amf_nudm_sdm_build_subscription_delete,
                        amf_ue, state, NULL);
                ogs_expect(r == OGS_OK);
                ogs_assert(r != OGS_ERROR);
            } else if (PCF_AM_POLICY_ASSOCIATED(amf_ue)) {
                r = amf_ue_sbi_discover_and_send(
                        OGS_SBI_SERVICE_TYPE_NPCF_AM_POLICY_CONTROL,
                        NULL,
                        amf_npcf_am_policy_control_build_delete,
                        amf_ue, state, NULL);
                ogs_expect(r == OGS_OK);
                ogs_assert(r != OGS_ERROR);
            }

        } else if (ambr_changed) {
            ogs_pkbuf_t *ngapbuf;

            ngapbuf = ngap_build_ue_context_modification_request(amf_ue);
            ogs_assert(ngapbuf);

            r = ngap_send_to_ran_ue(ran_ue, ngapbuf);
            ogs_expect(r == OGS_OK);
            ogs_assert(r != OGS_ERROR);
        }
    }

cleanup:
    if (ueid)
        ogs_free(ueid);
    if (res_name)
        ogs_free(res_name);

    memset(&sendmsg, 0, sizeof(sendmsg));

    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return OGS_OK;
}

static char *amf_namf_comm_base64_encode_ue_security_capability(
        ogs_nas_ue_security_capability_t ue_security_capability);
static char *amf_namf_comm_base64_encode_5gmm_capability(amf_ue_t *amf_ue);
static OpenAPI_list_t *amf_namf_comm_encode_ue_session_context_list(
        amf_ue_t *amf_ue);
static OpenAPI_list_t *amf_namf_comm_encode_ue_mm_context_list(
        amf_ue_t *amf_ue);

int amf_namf_comm_handle_ue_context_transfer_request(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    ogs_sbi_response_t *response = NULL;
    ogs_sbi_message_t sendmsg;
    amf_ue_t *amf_ue = NULL;

    OpenAPI_ambr_t *UeAmbr = NULL;
    OpenAPI_list_t *MmContextList = NULL;
    OpenAPI_mm_context_t *MmContext = NULL;
    OpenAPI_list_t *SessionContextList = NULL;
    OpenAPI_pdu_session_context_t *PduSessionContext = NULL;
    OpenAPI_lnode_t *node = NULL;
    OpenAPI_ue_context_t UeContext;
    OpenAPI_seaf_data_t SeafData;
    OpenAPI_ng_ksi_t Ng_ksi;
    OpenAPI_key_amf_t Key_amf;
    OpenAPI_sc_type_e Tsc_type;

    OpenAPI_ue_context_transfer_rsp_data_t UeContextTransferRspData;

    ogs_sbi_nf_instance_t *pcf_nf_instance = NULL;

    char *encoded_gmm_capability = NULL;
    int status = OGS_SBI_HTTP_STATUS_OK;
    char hxkamf_string[OGS_KEYSTRLEN(OGS_SHA256_DIGEST_SIZE)];
    char *strerror = NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    memset(&UeContextTransferRspData, 0, sizeof(UeContextTransferRspData));
    memset(&UeContext, 0, sizeof(UeContext));
    UeContextTransferRspData.ue_context = &UeContext;

    memset(&sendmsg, 0, sizeof(sendmsg));
    sendmsg.UeContextTransferRspData = &UeContextTransferRspData;

    if (!recvmsg->h.resource.component[1]) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        strerror = ogs_msprintf("No UE context ID");
        goto cleanup;
    }

    amf_ue = amf_ue_find_by_ue_context_id(recvmsg->h.resource.component[1]);
    if (!amf_ue) {
        status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
        strerror = ogs_msprintf("Cannot find Context ID [%s]",
                recvmsg->h.resource.component[1]);
        goto cleanup;
    }

    if (amf_ue->amf_ue_context_transfer_state != UE_CONTEXT_INITIAL_STATE) {
        ogs_warn("Incorrect UE context transfer state");
    }

    if (amf_ue->supi) {
        UeContext.supi = amf_ue->supi;
        if (amf_ue->auth_result !=
                OpenAPI_auth_result_AUTHENTICATION_SUCCESS) {
            UeContext.is_supi_unauth_ind = true;
            UeContext.supi_unauth_ind = amf_ue->auth_result;
        }
    }

    /* TODO UeContext.gpsi_list */

    if (amf_ue->pei) {
        UeContext.pei = amf_ue->pei;
    }

    if ((amf_ue->ue_ambr.uplink > 0) || (amf_ue->ue_ambr.downlink > 0)) {
        UeAmbr = ogs_calloc(1, sizeof(*UeAmbr));
        ogs_assert(UeAmbr);

        if (amf_ue->ue_ambr.uplink > 0)
            UeAmbr->uplink = ogs_sbi_bitrate_to_string(
                amf_ue->ue_ambr.uplink, OGS_SBI_BITRATE_KBPS);
        if (amf_ue->ue_ambr.downlink > 0)
            UeAmbr->downlink = ogs_sbi_bitrate_to_string(
                amf_ue->ue_ambr.downlink, OGS_SBI_BITRATE_KBPS);
        UeContext.sub_ue_ambr = UeAmbr;
    }

    if ((amf_ue->nas.ue.ksi != 0) && (amf_ue->nas.ue.tsc != 0)) {
        memset(&SeafData, 0, sizeof(SeafData));
        Tsc_type = (amf_ue->nas.ue.tsc == 0) ?
            OpenAPI_sc_type_NATIVE : OpenAPI_sc_type_MAPPED;

        memset(&Ng_ksi, 0, sizeof(Ng_ksi));
        SeafData.ng_ksi = &Ng_ksi;
        Ng_ksi.tsc = Tsc_type;
        Ng_ksi.ksi = (int)amf_ue->nas.ue.ksi;

        memset(&Key_amf, 0, sizeof(Key_amf));
        SeafData.key_amf = &Key_amf;
        OpenAPI_key_amf_type_e temp_key_type =
                (OpenAPI_key_amf_type_e)OpenAPI_key_amf_type_KAMF;
        Key_amf.key_type = temp_key_type;
        ogs_hex_to_ascii(amf_ue->kamf, sizeof(amf_ue->kamf),
                hxkamf_string, sizeof(hxkamf_string));
        Key_amf.key_val = hxkamf_string;
        UeContext.seaf_data = &SeafData;
    }

    encoded_gmm_capability =
        amf_namf_comm_base64_encode_5gmm_capability(amf_ue);
    UeContext._5g_mm_capability = encoded_gmm_capability;

    pcf_nf_instance = OGS_SBI_GET_NF_INSTANCE(
            amf_ue->sbi.service_type_array[
            OGS_SBI_SERVICE_TYPE_NPCF_AM_POLICY_CONTROL]);
    if (pcf_nf_instance) {
        UeContext.pcf_id = pcf_nf_instance->id;
    } else {
        ogs_warn("No PCF NF Instnace");
    }

    /* TODO UeContext.pcfAmPolicyUri */
    /* TODO UeContext.pcfUePolicyUri */

    MmContextList = amf_namf_comm_encode_ue_mm_context_list(amf_ue);
    UeContext.mm_context_list = MmContextList;

    if (recvmsg->UeContextTransferReqData->reason ==
            OpenAPI_transfer_reason_MOBI_REG) {
        SessionContextList =
	            amf_namf_comm_encode_ue_session_context_list(amf_ue);
        if (SessionContextList->count == 0) {
            OpenAPI_list_free(SessionContextList);
            SessionContextList = NULL;
        }
        UeContext.session_context_list = SessionContextList;
    }

    /* TODO ueRadioCapability */

    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    amf_ue->amf_ue_context_transfer_state = UE_CONTEXT_TRANSFER_OLD_AMF_STATE;

    if (encoded_gmm_capability)
        ogs_free(encoded_gmm_capability);

    if (UeAmbr)
        OpenAPI_ambr_free(UeAmbr);

    if (SessionContextList) {
        OpenAPI_list_for_each(SessionContextList, node) {
            PduSessionContext = node->data;
            OpenAPI_pdu_session_context_free(PduSessionContext);
        }
        OpenAPI_list_free(SessionContextList);
    }

    if (MmContextList) {
        OpenAPI_list_for_each(MmContextList, node) {
            MmContext = node->data;
            OpenAPI_mm_context_free(MmContext);
        }
        OpenAPI_list_free(MmContextList);
    }

    /*
     * Ue context is transfered, but we must keep the UE context until the
     * registartion status update is received.
     *
     * TS 23.502
     * 4.2.2.2.2 General Registration
     *
     * 10. [Conditional] new AMF to old AMF: Namf_Communication_RegistrationStatusUpdate
     * (PDU Session ID(s) to be released due to slice not supported).
	 * If the authentication/security procedure fails, then the Registration shall be
     * rejected and the new AMF invokes the Namf_Communication_RegistrationStatusUpdate
     * service operation with a reject indication towards the old AMF. The old AMF continues
     * as if the UE context transfer service operation was never received.
     */

    return OGS_OK;

cleanup:
    ogs_assert(strerror);
    ogs_error("%s", strerror);

    ogs_assert(true ==
        ogs_sbi_server_send_error(stream, status, NULL, strerror, NULL, NULL));
    ogs_free(strerror);

    return OGS_ERROR;
}

static ogs_nas_5gmm_capability_t
        amf_namf_comm_base64_decode_5gmm_capability(char *encoded);
static ogs_nas_ue_security_capability_t
        amf_namf_comm_base64_decode_ue_security_capability(char *encoded);
static void amf_namf_comm_decode_ue_mm_context_list(
            amf_ue_t *amf_ue, OpenAPI_list_t *MmContextList);
static void amf_namf_comm_decode_ue_session_context_list(
            amf_ue_t *amf_ue, OpenAPI_list_t *SessionContextList);

int amf_namf_comm_handle_ue_context_transfer_response(
        ogs_sbi_message_t *recvmsg, amf_ue_t *amf_ue)
{
    OpenAPI_ue_context_t *UeContext = NULL;

    if (!recvmsg->UeContextTransferRspData) {
        ogs_error("No UeContextTransferRspData");
        return OGS_ERROR;
    }

    if (!recvmsg->UeContextTransferRspData->ue_context) {
        ogs_error("No UE context");
        return OGS_ERROR;
    }

    UeContext = recvmsg->UeContextTransferRspData->ue_context;

    if (!UeContext->supi) {
        ogs_error("No SUPI");
        return OGS_ERROR;
    }

    amf_ue_set_supi(amf_ue, UeContext->supi);
    if (!UeContext->supi_unauth_ind){
        amf_ue->auth_result = OpenAPI_auth_result_AUTHENTICATION_SUCCESS;
    }

    if (UeContext->pei) {
        if (amf_ue->pei)
            ogs_free(amf_ue->pei);
        amf_ue->pei = ogs_strdup(UeContext->pei);
    }

    if (UeContext->sub_ue_ambr) {
        amf_ue->ue_ambr.downlink =
            ogs_sbi_bitrate_from_string(UeContext->sub_ue_ambr->downlink);
        amf_ue->ue_ambr.uplink =
            ogs_sbi_bitrate_from_string(UeContext->sub_ue_ambr->uplink);
    }

    if (UeContext->seaf_data) {
        if (UeContext->seaf_data->ng_ksi->tsc != OpenAPI_sc_type_NULL) {
            amf_ue->nas.ue.tsc =
                (UeContext->seaf_data->ng_ksi->tsc ==
                 OpenAPI_sc_type_NATIVE) ? 0 : 1;
            amf_ue->nas.ue.ksi = (uint8_t)UeContext->seaf_data->ng_ksi->ksi;

            ogs_ascii_to_hex(
                UeContext->seaf_data->key_amf->key_val,
                strlen(UeContext->seaf_data->key_amf->key_val),
                amf_ue->kamf,
                sizeof(amf_ue->kamf));
        }
    }

    if (UeContext->_5g_mm_capability) {
        ogs_nas_5gmm_capability_t gmm_capability;

        gmm_capability = amf_namf_comm_base64_decode_5gmm_capability(
                    UeContext->_5g_mm_capability);
        amf_ue->gmm_capability.lte_positioning_protocol_capability =
                (bool)gmm_capability.lte_positioning_protocol_capability;
        amf_ue->gmm_capability.ho_attach = (bool)gmm_capability.ho_attach;
        amf_ue->gmm_capability.s1_mode = (bool)gmm_capability.s1_mode;
    }

    if (UeContext->pcf_id) {
        /* TODO */
    }

    /* TODO UeContext->pcfAmPolicyUri */
    /* TODO UeContext->pcfUePolicyUri */

    if (UeContext->mm_context_list)
        amf_namf_comm_decode_ue_mm_context_list(
                amf_ue, UeContext->mm_context_list);

    if (UeContext->session_context_list) {
        amf_namf_comm_decode_ue_session_context_list(
                amf_ue, UeContext->session_context_list);
        /* Save a list of sessions to be released on old AMF */
        if (UeContext->mm_context_list)
            amf_ue_save_to_release_session_list(amf_ue);
    }
    /* TODO ueRadioCapability */

    return OGS_OK;
}

static char *amf_namf_comm_base64_encode_ue_security_capability(
        ogs_nas_ue_security_capability_t ue_security_capability)
{
    char *enc = NULL;
    int enc_len = 0;

    char num_of_octets =
            ue_security_capability.length +
            sizeof(ue_security_capability.length) +
            sizeof((uint8_t)OGS_NAS_5GS_REGISTRATION_REQUEST_UE_SECURITY_CAPABILITY_TYPE);
    /*
     * size [sizeof(ue_security_capability) + 1] is a sum of lengths:
     *        ue_security_capability (9 octets) +
     *        type (1 octet)
     */
    char security_octets_string[sizeof(ue_security_capability) + 1];

    /* Security guarantee */
    num_of_octets = ogs_min(
            num_of_octets, sizeof(ue_security_capability) + 1);
    enc_len = ogs_base64_encode_len(num_of_octets);

    enc = ogs_calloc(1, enc_len);
    ogs_assert(enc);

    security_octets_string[0] = (uint8_t)
        OGS_NAS_5GS_REGISTRATION_REQUEST_UE_SECURITY_CAPABILITY_TYPE;
    memcpy(security_octets_string + 1, &ue_security_capability, num_of_octets);
    ogs_base64_encode(enc , security_octets_string, num_of_octets);

    return enc;
}

static char *amf_namf_comm_base64_encode_5gmm_capability(amf_ue_t *amf_ue)
{
    ogs_nas_5gmm_capability_t nas_gmm_capability;
    int enc_len = 0;
    char *enc = NULL;

    memset(&nas_gmm_capability, 0, sizeof(nas_gmm_capability));

    /* 1 octet is mandatory, n.3 from TS 24.501 V16.12.0, 9.11.3.1 */
    nas_gmm_capability.length = 1;
    nas_gmm_capability.lte_positioning_protocol_capability =
            amf_ue->gmm_capability.lte_positioning_protocol_capability;
    nas_gmm_capability.ho_attach = amf_ue->gmm_capability.ho_attach;
    nas_gmm_capability.s1_mode = amf_ue->gmm_capability.s1_mode;

    uint8_t num_of_octets;

    char gmm_capability_octets_string[sizeof(ogs_nas_5gmm_capability_t) + 1];

    num_of_octets =
            nas_gmm_capability.length +
            sizeof(nas_gmm_capability.length) +
            sizeof((uint8_t)
                    OGS_NAS_5GS_REGISTRATION_REQUEST_5GMM_CAPABILITY_TYPE);

    /* Security guarantee. + 1 stands for 5GMM capability IEI */
    num_of_octets = ogs_min(
            num_of_octets, sizeof(ogs_nas_5gmm_capability_t) + 1);

    enc_len = ogs_base64_encode_len(num_of_octets);
    enc = ogs_calloc(1, enc_len);
    ogs_assert(enc);

    /* Fill the bytes of data */
    gmm_capability_octets_string[0] =
            (uint8_t)OGS_NAS_5GS_REGISTRATION_REQUEST_5GMM_CAPABILITY_TYPE;
    memcpy(gmm_capability_octets_string + 1,
            &nas_gmm_capability, num_of_octets);
    ogs_base64_encode(enc, gmm_capability_octets_string, num_of_octets);

    return enc;
}

static OpenAPI_list_t *amf_namf_comm_encode_ue_session_context_list(
        amf_ue_t *amf_ue)
{
    ogs_assert(amf_ue);

    amf_sess_t *sess = NULL;
    OpenAPI_list_t *PduSessionList = NULL;
    OpenAPI_pdu_session_context_t *PduSessionContext = NULL;
    OpenAPI_snssai_t *sNSSAI = NULL;

    PduSessionList = OpenAPI_list_create();
    ogs_assert(PduSessionList);

    ogs_list_for_each(&amf_ue->sess_list, sess) {
        PduSessionContext = ogs_calloc(1, sizeof(*PduSessionContext));
        ogs_assert(PduSessionContext);

        sNSSAI = ogs_calloc(1, sizeof(*sNSSAI));
        ogs_assert(sNSSAI);

        PduSessionContext->pdu_session_id = sess->psi;
        ogs_assert(sess->sm_context.resource_uri);
        PduSessionContext->sm_context_ref =
            ogs_strdup(sess->sm_context.resource_uri);

        sNSSAI->sst = sess->s_nssai.sst;
        sNSSAI->sd = ogs_s_nssai_sd_to_string(sess->s_nssai.sd);
        PduSessionContext->s_nssai = sNSSAI;

        ogs_assert(sess->dnn);
        PduSessionContext->dnn = ogs_strdup(sess->dnn);
        PduSessionContext->access_type =
            (OpenAPI_access_type_e)amf_ue->nas.access_type;

        OpenAPI_list_add(PduSessionList, PduSessionContext);
    }

    return PduSessionList;
}

static OpenAPI_list_t *amf_namf_comm_encode_ue_mm_context_list(amf_ue_t *amf_ue)
{
    OpenAPI_list_t *MmContextList = NULL;
    OpenAPI_mm_context_t *MmContext = NULL;

    int i;

    ogs_assert(amf_ue);

    MmContextList = OpenAPI_list_create();
    ogs_assert(MmContextList);

    MmContext = ogs_malloc(sizeof(*MmContext));
    ogs_assert(MmContext);
    memset(MmContext, 0, sizeof(*MmContext));

    MmContext->access_type = (OpenAPI_access_type_e)amf_ue->nas.access_type;

    if ((OpenAPI_ciphering_algorithm_e)amf_ue->selected_enc_algorithm &&
        (OpenAPI_integrity_algorithm_e)amf_ue->selected_int_algorithm) {

        OpenAPI_nas_security_mode_t *NasSecurityMode;

        NasSecurityMode = ogs_calloc(1, sizeof(*NasSecurityMode));
        ogs_assert(NasSecurityMode);

        NasSecurityMode->ciphering_algorithm =
                (OpenAPI_ciphering_algorithm_e)amf_ue->selected_enc_algorithm;
        NasSecurityMode->integrity_algorithm =
                (OpenAPI_integrity_algorithm_e)amf_ue->selected_int_algorithm;

        MmContext->nas_security_mode = NasSecurityMode;
    }

    if (amf_ue->dl_count > 0) {
        MmContext->is_nas_downlink_count = true;
        MmContext->nas_downlink_count = amf_ue->dl_count;
    }

    if (amf_ue->ul_count.i32 > 0) {
        MmContext->is_nas_uplink_count = true;
        MmContext->nas_uplink_count = amf_ue->ul_count.i32;
    }

    if (amf_ue->ue_security_capability.length > 0) {
        MmContext->ue_security_capability =
                amf_namf_comm_base64_encode_ue_security_capability(
                amf_ue->ue_security_capability);
    }

    if (amf_ue->allowed_nssai.num_of_s_nssai) {

        OpenAPI_list_t *AllowedNssaiList;

        /* This IE shall be present if the source AMF and the target AMF are
        *  in the same PLMN and if available. When present, this IE shall
        * contain the allowed NSSAI for the access type.
        */
        AllowedNssaiList = OpenAPI_list_create();

        ogs_assert(AllowedNssaiList);

        for (i = 0; i < amf_ue->allowed_nssai.num_of_s_nssai; i++) {
            OpenAPI_snssai_t *AllowedNssai;

            AllowedNssai = ogs_calloc(1, sizeof(*AllowedNssai));
            ogs_assert(AllowedNssai);

            AllowedNssai->sst = amf_ue->allowed_nssai.s_nssai[i].sst;
            AllowedNssai->sd = ogs_s_nssai_sd_to_string(
                    amf_ue->allowed_nssai.s_nssai[i].sd);

            OpenAPI_list_add(AllowedNssaiList, AllowedNssai);
        }

        MmContext->allowed_nssai = AllowedNssaiList;
    }

    OpenAPI_list_add(MmContextList, MmContext);

    return MmContextList;
}

static ogs_nas_5gmm_capability_t
        amf_namf_comm_base64_decode_5gmm_capability(char *encoded)
{
    ogs_nas_5gmm_capability_t gmm_capability;
    char *gmm_capability_octets_string = NULL;
    uint8_t gmm_capability_iei = 0;
    int len;

    memset(&gmm_capability, 0, sizeof(gmm_capability));
    gmm_capability_octets_string =
            (char*) ogs_calloc(sizeof(gmm_capability) + 1, sizeof(char));
    ogs_assert(gmm_capability_octets_string);

    ogs_assert(ogs_base64_decode_len(encoded) <= sizeof(gmm_capability) + 1);

    len = ogs_base64_decode(gmm_capability_octets_string, encoded);

    if (len == 0)
        ogs_error("Gmm capability not decoded");

    gmm_capability_iei = // not copied anywhere for now
            gmm_capability_octets_string[0];
    if (gmm_capability_iei !=
            OGS_NAS_5GS_REGISTRATION_REQUEST_5GMM_CAPABILITY_TYPE) {
        ogs_error("Type of 5GMM capability IEI is incorrect");
    }
    memcpy(&gmm_capability,
            gmm_capability_octets_string + 1,
            sizeof(gmm_capability));
    if (gmm_capability_octets_string) {
        ogs_free(gmm_capability_octets_string);
    }

    return gmm_capability;
}

static ogs_nas_ue_security_capability_t
        amf_namf_comm_base64_decode_ue_security_capability(char *encoded)
{
    ogs_nas_ue_security_capability_t ue_security_capability;
    char *ue_security_capability_octets_string = NULL;
    uint8_t ue_security_capability_iei = 0;

    memset(&ue_security_capability, 0, sizeof(ue_security_capability));
    ue_security_capability_octets_string =
            (char*) ogs_calloc(sizeof(ue_security_capability)+1, sizeof(char));
    ogs_assert(ue_security_capability_octets_string);

    ogs_assert(ogs_base64_decode_len(encoded) <= sizeof(ue_security_capability)+1);
    ogs_base64_decode(ue_security_capability_octets_string, encoded);

    ue_security_capability_iei = // not copied anywhere for now
            ue_security_capability_octets_string[0];
    if (ue_security_capability_iei !=
            OGS_NAS_5GS_REGISTRATION_REQUEST_UE_SECURITY_CAPABILITY_TYPE) {
        ogs_error("UE security capability IEI is incorrect");
    }

    memcpy(&ue_security_capability, ue_security_capability_octets_string + 1,
            sizeof(ue_security_capability));

    if (ue_security_capability_octets_string) {
        ogs_free(ue_security_capability_octets_string);
    }

    return ue_security_capability;
}

static void amf_namf_comm_decode_ue_mm_context_list(
            amf_ue_t *amf_ue, OpenAPI_list_t *MmContextList) {

    OpenAPI_lnode_t *node = NULL;

    OpenAPI_list_for_each(MmContextList, node) {

        OpenAPI_mm_context_t *MmContext = NULL;
        OpenAPI_list_t *AllowedNssaiList = NULL;
        OpenAPI_lnode_t *node1 = NULL;
        OpenAPI_list_t *NssaiMappingList = NULL;
        int num_of_s_nssai = 0;
        int num_of_nssai_mapping = 0;

        MmContext = node->data;

        AllowedNssaiList = MmContext->allowed_nssai;
        NssaiMappingList = MmContext->nssai_mapping_list;

        OpenAPI_list_for_each(AllowedNssaiList, node1) {
            OpenAPI_snssai_t *AllowedNssai = node1->data;

            ogs_assert(num_of_s_nssai < OGS_MAX_NUM_OF_SLICE);

            amf_ue->allowed_nssai.s_nssai[num_of_s_nssai].sst =
                    (uint8_t)AllowedNssai->sst;
            amf_ue->allowed_nssai.s_nssai[num_of_s_nssai].sd =
                    ogs_s_nssai_sd_from_string(AllowedNssai->sd);

            num_of_s_nssai++;
            amf_ue->allowed_nssai.num_of_s_nssai = num_of_s_nssai;
        }

        OpenAPI_list_for_each(NssaiMappingList, node1) {
            OpenAPI_nssai_mapping_t *NssaiMapping = node1->data;
            OpenAPI_snssai_t *HSnssai = NssaiMapping->h_snssai;

            ogs_assert(num_of_nssai_mapping < OGS_MAX_NUM_OF_SLICE);

            amf_ue->allowed_nssai.s_nssai[num_of_nssai_mapping].
                    mapped_hplmn_sst = HSnssai->sst;
            amf_ue->allowed_nssai.s_nssai[num_of_nssai_mapping].
                    mapped_hplmn_sd = ogs_s_nssai_sd_from_string(HSnssai->sd);

            num_of_nssai_mapping++;
        }

        if (MmContext->ue_security_capability) {
            amf_ue->ue_security_capability =
                    amf_namf_comm_base64_decode_ue_security_capability(
                    MmContext->ue_security_capability);
        }
    }
}

static void amf_namf_comm_decode_ue_session_context_list(
            amf_ue_t *amf_ue, OpenAPI_list_t *SessionContextList)
{
    OpenAPI_lnode_t *node = NULL;

    OpenAPI_list_for_each(SessionContextList, node) {
        OpenAPI_pdu_session_context_t *PduSessionContext;
        PduSessionContext = node->data;
        amf_sess_t *sess = NULL;

        int rv;
        ogs_sbi_message_t message;
        ogs_sbi_header_t header;

        bool rc;
        ogs_sbi_client_t *client = NULL;
        OpenAPI_uri_scheme_e scheme = OpenAPI_uri_scheme_NULL;
        char *fqdn = NULL;
        uint16_t fqdn_port = 0;
        ogs_sockaddr_t *addr = NULL, *addr6 = NULL;

        if (!PduSessionContext->sm_context_ref) {
            ogs_error("No smContextRef [PSI:%d]",
                    PduSessionContext->pdu_session_id);
            continue;
        }

        if (!PduSessionContext->s_nssai) {
            ogs_error("No sNSSI [PSI:%d]", PduSessionContext->pdu_session_id);
            continue;
        }

        if (!PduSessionContext->dnn) {
            ogs_error("No DNN [PSI:%d]", PduSessionContext->pdu_session_id);
            continue;
        }

        if (!PduSessionContext->access_type) {
            ogs_error("No accessType [PSI:%d]",
                    PduSessionContext->pdu_session_id);
            continue;
        }

        memset(&header, 0, sizeof(header));
        header.uri = PduSessionContext->sm_context_ref;

        rv = ogs_sbi_parse_header(&message, &header);
        if (rv != OGS_OK) {
            ogs_error("[%d] Cannot parse sm_context_ref [%s]",
                    PduSessionContext->pdu_session_id,
                    PduSessionContext->sm_context_ref);
            continue;
        }

        if (!message.h.resource.component[1]) {
            ogs_error("[%d] No SmContextRef [%s]",
                    PduSessionContext->pdu_session_id,
                    PduSessionContext->sm_context_ref);

            ogs_sbi_header_free(&header);
            continue;
        }

        sess = amf_sess_add(amf_ue, PduSessionContext->pdu_session_id);
        ogs_assert(sess);

        rc = ogs_sbi_getaddr_from_uri(
                &scheme, &fqdn, &fqdn_port, &addr, &addr6, header.uri);
        if (rc == false || scheme == OpenAPI_uri_scheme_NULL) {
            ogs_error("[%s:%d] Invalid URI [%s]",
                    amf_ue->supi, sess->psi, header.uri);

            ogs_sbi_header_free(&header);
            continue;
        }

        client = ogs_sbi_client_find(scheme, fqdn, fqdn_port, addr, addr6);
        if (!client) {
            ogs_debug("[%s:%d] ogs_sbi_client_add()", amf_ue->supi, sess->psi);
            client = ogs_sbi_client_add(scheme, fqdn, fqdn_port, addr, addr6);
            if (!client) {
                ogs_error("[%s:%d] ogs_sbi_client_add() failed",
                        amf_ue->supi, sess->psi);

                ogs_sbi_header_free(&header);

                ogs_free(fqdn);
                ogs_freeaddrinfo(addr);
                ogs_freeaddrinfo(addr6);

                continue;
            }
        }
        OGS_SBI_SETUP_CLIENT(&sess->sm_context, client);

        ogs_free(fqdn);
        ogs_freeaddrinfo(addr);
        ogs_freeaddrinfo(addr6);

        sess->sm_context.resource_uri =
            ogs_strdup(PduSessionContext->sm_context_ref);
        sess->sm_context.ref =
            ogs_strdup(message.h.resource.component[1]);

        memset(&sess->s_nssai, 0, sizeof(sess->s_nssai));

        sess->s_nssai.sst = PduSessionContext->s_nssai->sst;
        sess->s_nssai.sd = ogs_s_nssai_sd_from_string(
                PduSessionContext->s_nssai->sd);

        sess->dnn = ogs_strdup(PduSessionContext->dnn);
        amf_ue->nas.access_type = (int)PduSessionContext->access_type;

        ogs_sbi_header_free(&header);
    }
}

int amf_namf_comm_handle_registration_status_update_request(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg) {

    ogs_sbi_response_t *response = NULL;
    ogs_sbi_message_t sendmsg;
    amf_ue_t *amf_ue = NULL;
    ran_ue_t *ran_ue = NULL;
    amf_sess_t *sess = NULL;

    OpenAPI_ue_reg_status_update_req_data_t *UeRegStatusUpdateReqData =
            recvmsg->UeRegStatusUpdateReqData;
    OpenAPI_ue_reg_status_update_rsp_data_t UeRegStatusUpdateRspData;

    int status = 0;
    char *strerror = NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    if (!recvmsg->h.resource.component[1]) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        strerror = ogs_msprintf("No UE context ID");
        goto cleanup;
    }
    amf_ue = amf_ue_find_by_ue_context_id(recvmsg->h.resource.component[1]);
    if (!amf_ue) {
        status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
        strerror = ogs_msprintf("Cannot find Context ID [%s]",
                recvmsg->h.resource.component[1]);
        goto cleanup;
    }

    if (amf_ue->amf_ue_context_transfer_state != UE_CONTEXT_TRANSFER_OLD_AMF_STATE) {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        strerror = ogs_msprintf("Incorrect UE context transfer state");
        goto cleanup;
    }

    memset(&UeRegStatusUpdateRspData, 0, sizeof(UeRegStatusUpdateRspData));
    memset(&sendmsg, 0, sizeof(sendmsg));
    sendmsg.UeRegStatusUpdateRspData = &UeRegStatusUpdateRspData;

    if (UeRegStatusUpdateReqData->transfer_status ==
            OpenAPI_ue_context_transfer_status_TRANSFERRED) {
    /*
    * TS 29.518
    * 5.2.2.2.2 Registration Status Update
    * Once the update is received, the source AMF shall:
    *  -   remove the individual ueContext resource and release any PDU session(s) in the
    *      toReleaseSessionList attribute, if the transferStatus attribute included in the
    *      POST request body is set to "TRANSFERRED" and if the source AMF transferred the
    *      complete UE Context including all MM contexts and PDU Session Contexts.
    */
        UeRegStatusUpdateRspData.reg_status_transfer_complete = 1;

        ran_ue = ran_ue_find_by_id(amf_ue->ran_ue_id);

        if (ran_ue) {
            if (UeRegStatusUpdateReqData->to_release_session_list) {
                OpenAPI_lnode_t *node = NULL;
                OpenAPI_list_for_each(UeRegStatusUpdateReqData->to_release_session_list, node) {
                    /* A double must be read */
                    uint8_t psi = *(double *)node->data;
                    sess = amf_sess_find_by_psi(amf_ue, psi);
                    if (SESSION_CONTEXT_IN_SMF(sess)) {
                        amf_sbi_send_release_session(ran_ue, sess, AMF_RELEASE_SM_CONTEXT_NO_STATE);
                    } else {
                        ogs_error("[%s] No Session Context PSI[%d]",
                                amf_ue->supi, psi);
                        UeRegStatusUpdateRspData.reg_status_transfer_complete = 0;
                    }
                }
            }
        }

        /* Clear UE context */
        CLEAR_NG_CONTEXT(amf_ue);
        AMF_UE_CLEAR_PAGING_INFO(amf_ue);
        AMF_UE_CLEAR_N2_TRANSFER(amf_ue, pdu_session_resource_setup_request);
        AMF_UE_CLEAR_5GSM_MESSAGE(amf_ue);
        CLEAR_AMF_UE_ALL_TIMERS(amf_ue);
        OGS_ASN_CLEAR_DATA(&amf_ue->ueRadioCapability);

    } else if (UeRegStatusUpdateReqData->transfer_status ==
            OpenAPI_ue_context_transfer_status_NOT_TRANSFERRED) {
    /*
    * TS 23.502
    * 4.2.2.2.2
    * If the authentication/security procedure fails, then the Registration shall be rejected and
    * the new AMF invokes the Namf_Communication_RegistrationStatusUpdate service operation with
    * a reject indication towards the old AMF. The old AMF continues as if the UE context transfer
    * service operation was never received.
    */
        UeRegStatusUpdateRspData.reg_status_transfer_complete = 0;

    } else {
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        strerror = ogs_msprintf("Transfer status not supported: [%d]",
                UeRegStatusUpdateReqData->transfer_status);
        goto cleanup;
    }

    status = OGS_SBI_HTTP_STATUS_OK;
    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    amf_ue->amf_ue_context_transfer_state = UE_CONTEXT_INITIAL_STATE;

    return OGS_OK;

cleanup:
    ogs_assert(strerror);
    ogs_error("%s", strerror);

    ogs_assert(true == ogs_sbi_server_send_error(stream, status, NULL, strerror, NULL, NULL));
    ogs_free(strerror);

    amf_ue->amf_ue_context_transfer_state = UE_CONTEXT_INITIAL_STATE;

    return OGS_ERROR;
}

int amf_namf_comm_handle_registration_status_update_response(
        ogs_sbi_message_t *recvmsg, amf_ue_t *amf_ue) {

    /* Nothing to do */

    return OGS_OK;
}

/* Namf_MBSBroadcast Service API */

/*
 * 3GPP TS 29.518 - Release 17.11.0
 * 5G System; Access and Mobility Management Services; Stage 3
 * Ch. 5.6.2.2 - Namf_MBSBroadcast Service API - MBS Broadcast ContextCreate service operation
 */
/*
 * Sets mbs_context->notify_client from a caller-supplied notifyUri (ContextCreateReqData's own,
 * ContextUpdateReqData's own re-set) -- notifyUri is a caller-supplied URI, not a discovered NF, so this
 * cannot go through NRF discovery; same idiom as sess->paging.client for N1N2MessageTransfer. Factored out
 * of amf_namf_handle_mbs_broadcast_context_create() (item B-3 needs the identical logic a second time, in
 * amf_namf_handle_mbs_broadcast_context_update(), below).
 */
static void amf_namf_set_mbs_context_notify_client(
        amf_mbs_context_t *mbs_context, char *notify_uri, const char *op_name)
{
    bool                 uri_rc;
    OpenAPI_uri_scheme_e scheme = OpenAPI_uri_scheme_NULL;
    char                *fqdn = NULL;
    uint16_t             fqdn_port = 0;
    ogs_sockaddr_t      *addr = NULL, *addr6 = NULL;

    uri_rc = ogs_sbi_getaddr_from_uri(&scheme, &fqdn, &fqdn_port, &addr, &addr6, notify_uri);
    if (uri_rc == false || scheme == OpenAPI_uri_scheme_NULL) {
        ogs_error("MBS Broadcast %s: invalid notifyUri [%s]", op_name, notify_uri);
    } else {
        mbs_context->notify_client = ogs_sbi_client_find(scheme, fqdn, fqdn_port, addr, addr6);
        if (!mbs_context->notify_client)
            mbs_context->notify_client =
                ogs_sbi_client_add(scheme, fqdn, fqdn_port, addr, addr6);
        if (!mbs_context->notify_client)
            ogs_error("MBS Broadcast %s: ogs_sbi_client_add() failed for notifyUri", op_name);
    }
    ogs_free(fqdn);
    ogs_freeaddrinfo(addr);
    ogs_freeaddrinfo(addr6);
}

/* Mirrors smf_nmbsmf_parse_tai() (src/smf/nmbsmf-handler.c), the SMF-side equivalent for the same
 * OpenAPI_tai_t -> ogs_tai_t conversion; no shared helper exists between the two NFs in this codebase. */
static bool amf_namf_parse_tai(ogs_tai_t *tai, OpenAPI_tai_t *api_tai)
{
    if (!ogs_sbi_parse_plmn_id(&tai->plmn_id, api_tai->plmn_id)) {
        ogs_error("MBS Broadcast: TAI: unable to parse the PLMN Id");
        return false;
    }
    if (api_tai->tac) tai->tac = ogs_strdup(api_tai->tac);
    if (api_tai->nid) tai->nid = ogs_strdup(api_tai->nid);
    return true;
}

/* Mirrors smf_nmbsmf_parse_ncgi() (src/smf/nmbsmf-handler.c), same reasoning as amf_namf_parse_tai(). */
static bool amf_namf_parse_ncgi(ogs_ncgi_t *ncgi, OpenAPI_ncgi_t *api_ncgi)
{
    if (!ogs_sbi_parse_plmn_id(&ncgi->plmn_id, api_ncgi->plmn_id)) {
        ogs_error("MBS Broadcast: NCGI: unable to parse the PLMN Id");
        return false;
    }
    if (api_ncgi->nr_cell_id) ncgi->nr_cell_id = ogs_strdup(api_ncgi->nr_cell_id);
    if (api_ncgi->nid) ncgi->nid = ogs_strdup(api_ncgi->nid);
    return true;
}

/* Fills (allocating as needed) *out with api_area's own tai_list and ncgi_list, converted. Shared by
 * both branches of amf_namf_handle_mbs_broadcast_context_create() (mbsServiceArea and each entry of
 * mbsServiceAreaInfoList carry the same OpenAPI_mbs_service_area_t shape). */
static void amf_namf_parse_mbs_service_area(ogs_mbs_service_area_t **out, OpenAPI_mbs_service_area_t *api_area)
{
    OpenAPI_lnode_t *node;

    if (api_area->tai_list) {
        OpenAPI_list_for_each(api_area->tai_list, node) {
            OpenAPI_tai_t *api_tai = (OpenAPI_tai_t *)node->data;
            ogs_tai_t *tai = ogs_calloc(1, sizeof(*tai));
            ogs_assert(tai);
            if (!amf_namf_parse_tai(tai, api_tai)) {
                ogs_free(tai);
                continue;
            }
            if (!*out) {
                *out = ogs_calloc(1, sizeof(**out));
                ogs_assert(*out);
            }
            if (!(*out)->tai_list) {
                (*out)->tai_list = ogs_calloc(1, sizeof(*(*out)->tai_list));
                ogs_assert((*out)->tai_list);
            }
            ogs_list_add((*out)->tai_list, tai);
        }
    }

    if (api_area->ncgi_list) {
        OpenAPI_list_for_each(api_area->ncgi_list, node) {
            OpenAPI_ncgi_tai_t *api_ncgi_tai = (OpenAPI_ncgi_tai_t *)node->data;
            OpenAPI_lnode_t *cell_node;
            ogs_ncgi_tai_t *ncgi_tai = NULL;

            if (!api_ncgi_tai->tai) continue;
            ncgi_tai = ogs_calloc(1, sizeof(*ncgi_tai));
            ogs_assert(ncgi_tai);
            if (!amf_namf_parse_tai(&ncgi_tai->tai, api_ncgi_tai->tai)) {
                ogs_free(ncgi_tai);
                continue;
            }

            if (api_ncgi_tai->cell_list) {
                OpenAPI_list_for_each(api_ncgi_tai->cell_list, cell_node) {
                    OpenAPI_ncgi_t *api_ncgi = (OpenAPI_ncgi_t *)cell_node->data;
                    ogs_ncgi_t *ncgi = ogs_calloc(1, sizeof(*ncgi));
                    ogs_assert(ncgi);
                    if (!amf_namf_parse_ncgi(ncgi, api_ncgi)) {
                        ogs_free(ncgi);
                        continue;
                    }
                    ogs_list_add(&ncgi_tai->cell_list, ncgi);
                }
            }

            if (!*out) {
                *out = ogs_calloc(1, sizeof(**out));
                ogs_assert(*out);
            }
            if (!(*out)->ncgi_tai_list) {
                (*out)->ncgi_tai_list = ogs_calloc(1, sizeof(*(*out)->ncgi_tai_list));
                ogs_assert((*out)->ncgi_tai_list);
            }
            ogs_list_add((*out)->ncgi_tai_list, ncgi_tai);
        }
    }
}

int amf_namf_handle_mbs_broadcast_context_create(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    // TODO (borieher): Not handling the 307 Temporary Redirect and 308 Permanent Redirect errors for now
    ogs_debug("MBS Broadcast ContextCreate request received");

    OpenAPI_context_create_req_data_t *ContextCreateReqData = NULL;

    OpenAPI_ref_to_binary_data_t *n2_mbs_sm_info = NULL;
    ogs_pkbuf_t *n2mbssmbuf = NULL;
    ogs_pkbuf_t *n2msgreq = NULL;
    amf_gnb_t *gnb = NULL;

    amf_mbs_context_t *mbs_context = NULL;
    ogs_tmgi_t tmgi;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    int rv = OGS_OK;
    int gnb_rv = OGS_OK;

    ContextCreateReqData = recvmsg->ContextCreateReqData;

    if (!ContextCreateReqData) {
        ogs_error("MBS Broadcast ContextCreate: No ContextCreateReqData");
        // Extracted from the OpenAPI spec, not the 3GPP TS
        // ContextCreateReqData must be present, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextCreate failed, no ContextCreateReqData", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    n2_mbs_sm_info = ContextCreateReqData->n2_mbs_sm_info->ngap_data;

    if (!n2_mbs_sm_info || !n2_mbs_sm_info->content_id) {
        ogs_error("MBS Broadcast ContextCreate: n2_mbs_sm_info not present");
        // Extracted from the OpenAPI spec, not the 3GPP TS
        // n2_mbs_sm_info must be present, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextCreate failed, no n2MbsSmInfo", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    // Grab the N2 MBS SM info from the multipart message
    // This contains the MBS Session Setup or Modification Request Transfer IE
    n2mbssmbuf = ogs_sbi_find_part_by_content_id(recvmsg, n2_mbs_sm_info->content_id);

    if(!n2mbssmbuf) {
        ogs_error("MBS Broadcast ContextCreate: n2mbssmbuf not found in the multipart message");
        // n2mbssmbuf not found in the multipart message, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextCreate failed, N2 MBS SM info not found in the multipart message", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    if(!ContextCreateReqData->mbs_session_id) {
        ogs_error("MBS Broadcast ContextCreate: mbs_session_id not present");
        // mbs_session_id must be present, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextCreate failed, no mbsSessionId", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    if(!ContextCreateReqData->notify_uri) {
        ogs_error("MBS Broadcast ContextCreate: notify_uri not present");
        // notify_uri must be present, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextCreate failed, no notifyUri", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    if(!ContextCreateReqData->snssai) {
        ogs_error("MBS Broadcast ContextCreate: snssai not present");
        // snssai must be present, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextCreate failed, no snssai", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    if(!ContextCreateReqData->mbs_service_area && !ContextCreateReqData->mbs_service_area_info_list) {
        ogs_error("MBS Broadcast ContextCreate: mbs_service_area or mbs_service_area_info_list not present");
        // mbs_service_area or mbs_service_area_info_list should be present, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested MBS Broadcast ContextCreate failed, no [mbsServiceArea] nor [mbsServiceAreaInfoList] present", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

        // ContextCreateReqData's schema is a oneOf over mbsServiceArea and mbsServiceAreaInfoList, so exactly
        // one must be present. The check above rejects only the both-absent case; this rejects both-present.
    if (ContextCreateReqData->mbs_service_area && ContextCreateReqData->mbs_service_area_info_list) {
        ogs_error("MBS Broadcast ContextCreate: mbs_service_area and mbs_service_area_info_list are mutually exclusive");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested MBS Broadcast ContextCreate failed, [mbsServiceArea] and [mbsServiceAreaInfoList] are mutually exclusive", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    ogs_sbi_parse_tmgi(&tmgi, ContextCreateReqData->mbs_session_id->tmgi);

        // Remove any stale context for this TMGI first, so a retried or duplicate ContextCreate does not
        // allocate a second one; see amf_mbs_context_find_by_tmgi().
    {
        amf_mbs_context_t *stale_mbs_context = amf_mbs_context_find_by_tmgi(&tmgi);
        if (stale_mbs_context) {
            ogs_warn("MBS Broadcast ContextCreate: replacing existing context [%s] for the same TMGI",
                    stale_mbs_context->mbs_context_ref);
            amf_mbs_context_remove(stale_mbs_context);
        }
    }

    mbs_context = amf_mbs_context_create(&tmgi);

        // amf_mbs_context_create() returns NULL when its fixed-size pool is exhausted (amf_mbs_context_add()
        // logs "Maximum number of MBS Contexts[%d] reached"), and the result must be checked before use:
        // ngap_build_broadcast_session_setup_request() dereferences mbs_context->tmgi near its start, so an
        // unchecked NULL ends the process rather than reporting the failure. This is the only call site.
    if (!mbs_context) {
        ogs_error("MBS Broadcast ContextCreate: amf_mbs_context_create() failed");
        // No clause governs the pool's own size or this failure's status code; matching the sibling
        // TMGI-pool-exhaustion case in MB-SMF's own nmbsmf-handler.c ("Cannot allocate TMGIs", 403).
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
            recvmsg, "Forbidden", "Requested MBS Broadcast ContextCreate failed, no MBS context available", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    // B-2: Namf_MBSBroadcast_ContextStatusNotify (TS 29.518 V18.14.0 cl.5.6.2.5) posts to this same
    // notify_uri for any gNB response after the first.
    amf_namf_set_mbs_context_notify_client(mbs_context, ContextCreateReqData->notify_uri, "ContextCreate");

        // Store the requested S-NSSAI and MBS Service Area, both validated as present just above, so
        // ngap_build_broadcast_session_setup_request() can send what was actually asked for rather than
        // hardcoded values.
    mbs_context->s_nssai.sst = ContextCreateReqData->snssai->sst;
    mbs_context->s_nssai.sd = ogs_s_nssai_sd_from_string(ContextCreateReqData->snssai->sd);

    if (ContextCreateReqData->mbs_service_area) {
        amf_namf_parse_mbs_service_area(&mbs_context->mbs_service_area, ContextCreateReqData->mbs_service_area);
        if (!mbs_context->mbs_service_area)
            ogs_warn("MBS Broadcast ContextCreate: mbsServiceArea carried neither a TAI nor an NCGI; "
                    "NGAP will fall back to all connected gNBs");
    } else if (ContextCreateReqData->mbs_service_area_info_list &&
            ContextCreateReqData->mbs_service_area_info_list->count > 0) {
                // At most one Area Session ID per session reaches here: this MB-SMF's northbound schema carries at
                // most one (TS 29.532 cl.5.3.2.2.1), so the first entry is the only one.
        OpenAPI_lnode_t *info_node = ContextCreateReqData->mbs_service_area_info_list->first;
        OpenAPI_mbs_service_area_info_t *api_info = (OpenAPI_mbs_service_area_info_t *)info_node->data;

        mbs_context->location_dependent = true;
        mbs_context->area_session_id = (uint16_t)api_info->area_session_id;

        if (api_info->mbs_service_area)
            amf_namf_parse_mbs_service_area(&mbs_context->mbs_service_area, api_info->mbs_service_area);

        if (ContextCreateReqData->mbs_service_area_info_list->count > 1) {
            ogs_warn("MBS Broadcast ContextCreate: mbsServiceAreaInfoList carries more than one entry "
                    "(%ld); this AMF only supports one Area Session ID per session, using the first",
                    ContextCreateReqData->mbs_service_area_info_list->count);
        }
    }

    // NGAP BROADCAST SESSION SETUP REQUEST message with MBS Session Setup or Modification Request Transfer IE
    n2msgreq = ngap_build_broadcast_session_setup_request(mbs_context, n2mbssmbuf);

        // Each gNB gets its own copy of the message. ngap_send_to_gnb() takes ownership of the pkbuf it is
        // given: it frees it on failure, and on success, for SOCK_STREAM gNBs, splices its embedded lnode into
        // that gNB's write_queue. Passing one buffer to every gNB in this loop would use an already-freed
        // buffer after the first iteration, or splice the same list node into a second queue while the first
        // still references it. The shared template is freed once after the loop, never being sent itself.
    ogs_list_for_each(&amf_self()->gnb_list, gnb) {
        ogs_pkbuf_t *n2msgreq_copy = ogs_pkbuf_copy(n2msgreq);
        if (!n2msgreq_copy) {
            ogs_error("ogs_pkbuf_copy() failed");
            break;
        }
        ogs_debug("Sending N2 MBS SM info to gNB %i", gnb->gnb_id);
        gnb_rv = ngap_send_to_gnb(gnb, n2msgreq_copy, NGAP_NON_UE_SIGNALLING);
        if (gnb_rv != OGS_OK) {
            ogs_error("ngap_send_to_gnb() failed");
            break;
        } else {
            ogs_debug("Sent to gnb [%i]", gnb->gnb_id);
            // B-2: how many gNBs the request actually reached, so ngap_handle_broadcast_session_setup_response()
            // can tell when every one of them has responded (ContextStatusNotify's operationStatus).
            mbs_context->gnb_request_count++;
        }
    }
    ogs_pkbuf_free(n2msgreq);

    // TODO (borieher): Start timer to wait for reception?
    //ogs_timer_start(mbs_context->gnb_timer,
    //    amf_timer_cfg(AMF_TIMER_X)->duration);

        // The response is deferred onto mbs_context->stream_id rather than sent as soon as the NGAP request is
        // broadcast. TS 29.518 V18.14.0 clause 5.6.2.2 step 2a: "The AMF should respond success when it
        // receives the first successful response from the NG-RAN(s)."
        // ngap_handle_broadcast_session_setup_response() completes it once the first gNB replies, through
        // amf_namf_send_mbs_broadcast_context_create_response() below.
    mbs_context->stream_id = ogs_sbi_id_from_stream(stream);

cleanup:
    if (rv == OGS_OK)
        return true;
    else
        return false;
}

/*
 * Builds and sends the deferred Namf_MBSBroadcast_ContextCreate response (OGS_SBI_HTTP_STATUS_CREATED),
 * once ngap_handle_broadcast_session_setup_response() has correlated the first NG-RAN response to \p
 * mbs_context. Split out of amf_namf_handle_mbs_broadcast_context_create() itself since that function
 * returns to its SBI caller long before this can run -- see mbs_context->stream_id's own comment
 * (context.h) for why the stream must be re-looked-up rather than captured by raw pointer.
 */
void amf_namf_send_mbs_broadcast_context_create_response(amf_mbs_context_t *mbs_context)
{
    OpenAPI_context_create_rsp_data_t *ContextCreateRspData = NULL;
    OpenAPI_tmgi_t *tmgi_copy = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_copy = NULL;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_stream_t *stream = NULL;
    ogs_sbi_server_t *server = NULL;
    ogs_sbi_header_t header;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(mbs_context);

    stream = ogs_sbi_stream_find_by_id(mbs_context->stream_id);
    if (!stream) {
        // The client's stream is gone (e.g. it gave up and disconnected) by the time the first gNB
        // actually responded. Nothing to send a response to; the MBS context itself is unaffected.
        ogs_warn("MBS Broadcast ContextCreate: stream [%d] no longer exists, response not sent",
                (int)mbs_context->stream_id);
        return;
    }

    memset(&sendmsg, 0, sizeof(sendmsg));

    // Build MBS Session ID / TMGI directly from the stored context (rather than the original
    // ContextCreateReqData, which is long gone by the time this runs).
    tmgi_copy = OpenAPI_tmgi_create(
        ogs_strdup(mbs_context->tmgi.mbs_service_id),
        ogs_sbi_build_plmn_id(&mbs_context->tmgi.plmn_id));
    ogs_assert(tmgi_copy);

    mbs_session_id_copy = OpenAPI_mbs_session_id_create(tmgi_copy, NULL, NULL);
    ogs_assert(mbs_session_id_copy);

    ContextCreateRspData = OpenAPI_context_create_rsp_data_create(mbs_session_id_copy, NULL, OpenAPI_operation_status_NULL);

    server = ogs_sbi_server_from_stream(stream);
    ogs_assert(server);

    // Adding the mbsContextRef in the headers for the created resource
    memset(&header, 0, sizeof(header));
    header.service.name = (char *) OGS_SBI_SERVICE_NAME_NAMF_MBS_BC;
    header.api.version = (char *) OGS_SBI_API_V1;
    header.resource.component[0] =
        (char *) OGS_SBI_RESOURCE_NAME_MBS_CONTEXTS;
    header.resource.component[1] = mbs_context->mbs_context_ref;

    sendmsg.http.location = ogs_sbi_server_uri(server, &header);
    ogs_assert(sendmsg.http.location);

    sendmsg.ContextCreateRspData = ContextCreateRspData;

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_CREATED);
    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    if (ContextCreateRspData)
        OpenAPI_context_create_rsp_data_free(ContextCreateRspData);

    if (sendmsg.http.location)
        ogs_free(sendmsg.http.location);

    // Mark as answered so a second/subsequent gNB response (item B-2, deferred separately) does not
    // re-trigger this same 201 Created a second time.
    mbs_context->stream_id = OGS_INVALID_POOL_ID;
}

/*
 * 3GPP TS 29.518 V18.14.0 cl.5.6.2.3 - Namf_MBSBroadcast Service API - ContextUpdate service operation
 * (item B-3). "The ContextUpdate service operation shall be used by the NF Service Consumer (e.g. MB-SMF)
 * to request the AMF to update a broadcast MBS session context ... by using the HTTP POST method"
 * targeting the individual resource (POST /namf-mbs-bc/v1/mbs-contexts/{mbsContextRef}).
 *
 * Scope, deliberately bounded (see ngap_build_broadcast_session_modification_request()'s own comment):
 * this handler forwards whichever of mbsServiceArea/mbsServiceAreaInfoList and n2MbsSmInfo the request
 * carries to NG-RAN; ranIdList/noNgapSignallingInd (restoration-procedure fields, TS 23.527 cl.8.3.2.3/
 * 8.3.2.4) and maxResponseTime/n2MbsInfoChangeInd are read off the request but not yet acted on -- no
 * restoration procedure exists anywhere in this AMF to react to ranIdList/noNgapSignallingInd, and
 * maxResponseTime's own timeout-driven "incomplete" ContextStatusNotify (cl.5.6.2.3's own text) needs a
 * timer this handler does not yet start (same TODO as ContextCreate's own "start timer to wait for
 * reception").
 */
int amf_namf_handle_mbs_broadcast_context_update(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    ogs_debug("MBS Broadcast ContextUpdate request received");

    OpenAPI_context_update_req_data_t *ContextUpdateReqData = NULL;

    const char *mbs_context_ref = NULL;
    amf_mbs_context_t *mbs_context = NULL;
    ogs_pkbuf_t *n2mbssmbuf = NULL;
    ogs_pkbuf_t *n2msgreq = NULL;
    amf_gnb_t *gnb = NULL;
    bool has_service_area = false;

    int rv = OGS_OK;
    int gnb_rv = OGS_OK;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    mbs_context_ref = recvmsg->h.resource.component[1];
    if (!mbs_context_ref) {
        ogs_error("MBS Broadcast ContextUpdate: No mbsContextRef in the request path");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextUpdate failed, no mbsContextRef", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    mbs_context = amf_mbs_context_find_by_ref(mbs_context_ref);
    if (!mbs_context) {
        ogs_error("MBS Broadcast ContextUpdate: mbsContextRef[%s] not found", mbs_context_ref);
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
            recvmsg, "Not Found", "Requested MBS Broadcast ContextUpdate failed, mbsContextRef not found", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    ContextUpdateReqData = recvmsg->ContextUpdateReqData;
    if (!ContextUpdateReqData) {
        ogs_error("MBS Broadcast ContextUpdate: No ContextUpdateReqData");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextUpdate failed, no ContextUpdateReqData", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

        // Schema (oneOf, the same shape as ContextCreateReqData's): mbsServiceArea and
        // mbsServiceAreaInfoList are mutually exclusive.
    if (ContextUpdateReqData->mbs_service_area && ContextUpdateReqData->mbs_service_area_info_list) {
        ogs_error("MBS Broadcast ContextUpdate: mbs_service_area and mbs_service_area_info_list are mutually exclusive");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested MBS Broadcast ContextUpdate failed, [mbsServiceArea] and [mbsServiceAreaInfoList] are mutually exclusive", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }
    has_service_area = ContextUpdateReqData->mbs_service_area ||
        ContextUpdateReqData->mbs_service_area_info_list;

        // The TAI list this Update carries is parsed and stored, not merely noted as present: without it
        // ngap_build_broadcast_session_modification_request() has nothing real to send and falls back to every
        // connected gNB's own TAI. This Update's mbsServiceArea replaces whatever ContextCreate stored, being
        // this update's authoritative value.
    if (ContextUpdateReqData->mbs_service_area) {
        ogs_mbs_service_area_t *new_service_area = NULL;
        amf_namf_parse_mbs_service_area(&new_service_area, ContextUpdateReqData->mbs_service_area);
        if (new_service_area) {
            if (mbs_context->mbs_service_area)
                ogs_mbs_service_area_free(mbs_context->mbs_service_area);
            mbs_context->mbs_service_area = new_service_area;
        } else {
            ogs_warn("MBS Broadcast ContextUpdate: mbsServiceArea carried neither a TAI nor an NCGI; "
                    "NGAP will use whatever service area was previously stored, or fall back further");
        }
    } else if (has_service_area && ContextUpdateReqData->mbs_service_area_info_list &&
            ContextUpdateReqData->mbs_service_area_info_list->count > 0) {
                // mbsServiceAreaInfoList updates the stored service area rather than only warning.
                // TS 29.518 V18.14.0 cl.5.6.2.3 lets the NF Service Consumer change the MBS Service Area through
                // ContextUpdate, and this IE is one of the two mutually-exclusive ways to carry it (see the check
                // above). Handled as ContextCreate handles the same IE, under the same one-entry northbound-schema
                // constraint.
        OpenAPI_lnode_t *info_node = ContextUpdateReqData->mbs_service_area_info_list->first;
        OpenAPI_mbs_service_area_info_t *api_info = (OpenAPI_mbs_service_area_info_t *)info_node->data;

        mbs_context->location_dependent = true;
        mbs_context->area_session_id = (uint16_t)api_info->area_session_id;

        if (api_info->mbs_service_area) {
            ogs_mbs_service_area_t *new_service_area = NULL;
            amf_namf_parse_mbs_service_area(&new_service_area, api_info->mbs_service_area);
            if (new_service_area) {
                if (mbs_context->mbs_service_area)
                    ogs_mbs_service_area_free(mbs_context->mbs_service_area);
                mbs_context->mbs_service_area = new_service_area;
            } else {
                ogs_warn("MBS Broadcast ContextUpdate: mbsServiceAreaInfoList's own mbsServiceArea "
                        "carried neither a TAI nor an NCGI; NGAP will use whatever service area was "
                        "previously stored, or fall back further");
            }
        }

        if (ContextUpdateReqData->mbs_service_area_info_list->count > 1) {
            ogs_warn("MBS Broadcast ContextUpdate: mbsServiceAreaInfoList carries more than one entry "
                    "(%ld); this AMF only supports one Area Session ID per session, using the first",
                    ContextUpdateReqData->mbs_service_area_info_list->count);
        }
    } else if (has_service_area) {
        // mbsServiceAreaInfoList present but empty -- genuinely nothing to apply, distinct from the
        // real forwarding case just above.
        ogs_warn("MBS Broadcast ContextUpdate: request's mbsServiceAreaInfoList carried no entries; "
                "NGAP will use whatever service area was previously stored, or fall back further");
    }

    // n2MbsSmInfo (O): grab the transfer IE from the multipart message, same lookup as ContextCreate's own.
    if (ContextUpdateReqData->n2_mbs_sm_info && ContextUpdateReqData->n2_mbs_sm_info->ngap_data &&
            ContextUpdateReqData->n2_mbs_sm_info->ngap_data->content_id) {
        n2mbssmbuf = ogs_sbi_find_part_by_content_id(
                recvmsg, ContextUpdateReqData->n2_mbs_sm_info->ngap_data->content_id);
        if (!n2mbssmbuf) {
            ogs_error("MBS Broadcast ContextUpdate: n2mbssmbuf not found in the multipart message");
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "Bad Request",
                "Requested MBS Broadcast ContextUpdate failed, N2 MBS SM info not found in the multipart message", NULL);
            rv = OGS_ERROR;
            goto cleanup;
        }
    }

    if (!has_service_area && !n2mbssmbuf) {
        ogs_error("MBS Broadcast ContextUpdate: nothing in this request maps to an NG-RAN-facing update "
                "(no mbsServiceArea/mbsServiceAreaInfoList, no n2MbsSmInfo)");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextUpdate failed, nothing to update", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    // notifyUri (O): "if the NF Service Consumer wishes to modify the notification URI".
    if (ContextUpdateReqData->notify_uri) {
        amf_namf_set_mbs_context_notify_client(
                mbs_context, ContextUpdateReqData->notify_uri, "ContextUpdate");
    }

    // Reset B-2's completion-tracking counters: ContextCreate's own use of them has already finished by
    // the time an Update can happen (this AMF creates exactly one context per TMGI, and ContextCreate
    // always completes -- successfully or not -- before an SMF would send it an Update for the same
    // context), so reusing them here for this Update's own completion tracking is safe.
    mbs_context->gnb_request_count = 0;
    mbs_context->gnb_response_count = 0;

    n2msgreq = ngap_build_broadcast_session_modification_request(mbs_context, has_service_area, n2mbssmbuf);

        // Each gNB gets its own copy, for the ownership reason given in ContextCreate's send loop.
    ogs_list_for_each(&amf_self()->gnb_list, gnb) {
        ogs_pkbuf_t *n2msgreq_copy = ogs_pkbuf_copy(n2msgreq);
        if (!n2msgreq_copy) {
            ogs_error("ogs_pkbuf_copy() failed");
            break;
        }
        ogs_debug("Sending N2 MBS SM info (ContextUpdate) to gNB %i", gnb->gnb_id);
        gnb_rv = ngap_send_to_gnb(gnb, n2msgreq_copy, NGAP_NON_UE_SIGNALLING);
        if (gnb_rv != OGS_OK) {
            ogs_error("ngap_send_to_gnb() failed");
            break;
        } else {
            mbs_context->gnb_request_count++;
        }
    }
    ogs_pkbuf_free(n2msgreq);

    // Defer the SBI response the same way ContextCreate does -- cl.5.6.2.3 step 2a/2b: "On success, '200
    // OK' shall be returned if additional information needs to be returned ... '204 No Content' shall be
    // returned if no additional information needs to be returned", both conditioned on NG-RAN responses
    // this AMF has not received yet.
    mbs_context->stream_id = ogs_sbi_id_from_stream(stream);

cleanup:
    if (rv == OGS_OK)
        return true;
    else
        return false;
}

/*
 * Builds and sends the deferred Namf_MBSBroadcast_ContextUpdate response (200 OK if there is a transfer
 * IE to return, 204 No Content otherwise), once ngap_handle_broadcast_session_modification_response() has
 * correlated the first NG-RAN response to \p mbs_context. Mirrors
 * amf_namf_send_mbs_broadcast_context_create_response()'s own reasoning for why this cannot run
 * synchronously from amf_namf_handle_mbs_broadcast_context_update() itself.
 */
void amf_namf_send_mbs_broadcast_context_update_response(amf_mbs_context_t *mbs_context)
{
    ogs_sbi_message_t sendmsg;
    ogs_sbi_stream_t *stream = NULL;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(mbs_context);

    stream = ogs_sbi_stream_find_by_id(mbs_context->stream_id);
    if (!stream) {
        ogs_warn("MBS Broadcast ContextUpdate: stream [%d] no longer exists, response not sent",
                (int)mbs_context->stream_id);
        mbs_context->stream_id = OGS_INVALID_POOL_ID;
        return;
    }

    memset(&sendmsg, 0, sizeof(sendmsg));

    // NOT IN THIS CHANGE: this AMF has no MBS Session Setup or Modification Response Transfer IE to
    // report back yet (see ngap_handle_broadcast_session_modification_response()'s own comment on why the
    // transfer IE, if any, is only forwarded via ContextStatusNotify for gNB responses after the first) --
    // so the first response always completes with 204 No Content, never 200 OK. A genuine transfer-IE
    // round-trip on the *first* response would need this function to build ContextUpdateRspData with a
    // populated n2_mbs_sm_info_list, which nothing currently threads through from
    // ngap_handle_broadcast_session_modification_response() to here.
    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    mbs_context->stream_id = OGS_INVALID_POOL_ID;
}

/*
 * 3GPP TS 29.518 - Release 17.11.0
 * 5G System; Access and Mobility Management Services; Stage 3
 * Ch. 5.6.2.4 - Namf_MBSBroadcast Service API - MBS Broadcast ContextRelease service operation
 * (this operation is named ContextRelease per the TS 29.518 table of contents -- 5.6.2.3 is
 * ContextUpdate, item B-3, implemented above; this DELETE-triggered handler's behaviour was already
 * correct, only the cited clause number and operation name were wrong)
 *
 * BUG FIX: this handler did not exist before -- the SBI dispatcher (amf-sm.c) had no DELETE case at all,
 * so nothing ever called this. Without it, a broadcast session's AMF-side context lived forever and no
 * NGAP BroadcastSessionRelease was ever sent, leaving the gNB's MCCH content and MAC/scheduler resources
 * permanently allocated for a session the SMF believes has been released.
 */
int amf_namf_handle_mbs_broadcast_context_delete(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    ogs_debug("MBS Broadcast ContextDelete request received");

    const char *mbs_context_ref = NULL;
    amf_mbs_context_t *mbs_context = NULL;
    ogs_pkbuf_t *n2msgreq = NULL;
    amf_gnb_t *gnb = NULL;
    int gnb_rv = OGS_OK;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    mbs_context_ref = recvmsg->h.resource.component[1];
    if (!mbs_context_ref) {
        ogs_error("MBS Broadcast ContextDelete: No mbsContextRef in the request path");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextDelete failed, no mbsContextRef", NULL);
        return OGS_ERROR;
    }

    mbs_context = amf_mbs_context_find_by_ref(mbs_context_ref);
    if (!mbs_context) {
        ogs_error("MBS Broadcast ContextDelete: mbsContextRef[%s] not found", mbs_context_ref);
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
            recvmsg, "Not Found", "Requested MBS Broadcast ContextDelete failed, mbsContextRef not found", NULL);
        return OGS_ERROR;
    }

    // NGAP BROADCAST SESSION RELEASE REQUEST message
    n2msgreq = ngap_build_broadcast_session_release_request(mbs_context);

        // Each gNB gets its own copy and the shared template is freed once after the loop, for the ownership
        // reason given in amf_namf_handle_mbs_broadcast_context_create()'s send loop above.
    ogs_list_for_each(&amf_self()->gnb_list, gnb) {
        ogs_pkbuf_t *n2msgreq_copy = ogs_pkbuf_copy(n2msgreq);
        if (!n2msgreq_copy) {
            ogs_error("ogs_pkbuf_copy() failed");
            break;
        }
        ogs_debug("Sending N2 MBS Session Release to gNB %i", gnb->gnb_id);
        gnb_rv = ngap_send_to_gnb(gnb, n2msgreq_copy, NGAP_NON_UE_SIGNALLING);
        if (gnb_rv != OGS_OK) {
            ogs_error("ngap_send_to_gnb() failed");
            break;
        }
    }
    ogs_pkbuf_free(n2msgreq);

    amf_mbs_context_remove(mbs_context);

    /*********************************************************************
     * Send OGS_SBI_HTTP_STATUS_NO_CONTENT (/namf-mbs-bc/v1/mbs-contexts/{mbsContextRef}) to the consumer NF
     *********************************************************************/

    memset(&sendmsg, 0, sizeof(sendmsg));

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return OGS_OK;
}

/*
 * Namf_MBSCommunication Service - N2MessageTransfer service operation (TS 29.518 cl.5.7.2.2): "The
 * N2MessageTransfer service operation shall be used by the NF Service Consumer (e.g. MB-SMF) to request
 * the AMF to transfer an MBS related N2 message to the NG-RAN nodes serving the multicast MBS session."
 *
 * TARGET SELECTION LIMITATION: cl.5.7.2.2 also states that without the optional RAN-ID-LIST feature (which
 * this vendored TS 29.518 V17.8.0 copy predates -- see the register), "the AMF distributes the MBS related
 * N2 message to the list of NG-RAN nodes having established shared delivery that the AMF stores locally."
 * That registry -- which NG-RAN node(s) have established shared delivery for a given MBS session, per
 * TS 23.247 cl.7.2.1.4 -- does not exist in this codebase yet; cl.7.2.1.4's own NGAP-level mapping (which
 * top-level NGAP procedure a gNB uses to report establishing shared delivery to the AMF) is not yet
 * confirmed against the pinned ASN.1 module either. Until that exists, this handler relays to every
 * currently-connected gNB, mirroring the same simplification already established and documented for
 * Namf_MBSBroadcast (amf_namf_handle_mbs_broadcast_context_delete() above) -- correct for this project's
 * single-cell reference deployment (there is only ever one gNB to target), not a general multi-gNB
 * solution. See open5gs.md's own account of this limitation before relying on it in a multi-gNB
 * deployment.
 */
int amf_namf_mbs_comm_handle_n2_message_transfer(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    ogs_debug("Namf_MBSCommunication N2MessageTransfer request received");

    OpenAPI_mbs_n2_message_transfer_req_data_t *MbsN2MessageTransferReqData = NULL;
    OpenAPI_ref_to_binary_data_t *n2_mbs_sm_info = NULL;
    ogs_pkbuf_t *n2mbssmbuf = NULL;
    ogs_pkbuf_t *n2msgreq = NULL;
    amf_gnb_t *gnb = NULL;
    int gnb_rv = OGS_OK;

    ogs_tmgi_t tmgi;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;
    OpenAPI_mbs_n2_message_transfer_rsp_data_t MbsN2MessageTransferRspData;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    MbsN2MessageTransferReqData = recvmsg->MbsN2MessageTransferReqData;

    if (!MbsN2MessageTransferReqData) {
        ogs_error("N2MessageTransfer: No MbsN2MessageTransferReqData");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested N2MessageTransfer failed, no MbsN2MessageTransferReqData", NULL);
        return OGS_ERROR;
    }

    if (!MbsN2MessageTransferReqData->mbs_session_id ||
            !MbsN2MessageTransferReqData->mbs_session_id->tmgi) {
        ogs_error("N2MessageTransfer: mbsSessionId not present");
        // TS 29.518 cl.5.7.2.2 step 1: "The MbsN2MessageTransferReqData shall contain: - MBS Session ID"
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested N2MessageTransfer failed, no mbsSessionId", NULL);
        return OGS_ERROR;
    }

    if (!MbsN2MessageTransferReqData->n2_mbs_sm_info) {
        ogs_error("N2MessageTransfer: n2MbsSmInfo not present");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested N2MessageTransfer failed, no n2MbsSmInfo", NULL);
        return OGS_ERROR;
    }

    n2_mbs_sm_info = MbsN2MessageTransferReqData->n2_mbs_sm_info->ngap_data;
    if (!n2_mbs_sm_info || !n2_mbs_sm_info->content_id) {
        ogs_error("N2MessageTransfer: n2MbsSmInfo.ngapData not present");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested N2MessageTransfer failed, no n2MbsSmInfo.ngapData", NULL);
        return OGS_ERROR;
    }

    // Grab the N2 MBS SM info (the already-encoded transfer IE) from the multipart message, same
    // mechanism Namf_MBSBroadcast's ContextCreate/ContextUpdate already use.
    n2mbssmbuf = ogs_sbi_find_part_by_content_id(recvmsg, n2_mbs_sm_info->content_id);
    if (!n2mbssmbuf) {
        ogs_error("N2MessageTransfer: n2mbssmbuf not found in the multipart message");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested N2MessageTransfer failed, N2 MBS SM info not found in the multipart message", NULL);
        return OGS_ERROR;
    }

    ogs_sbi_parse_tmgi(&tmgi, MbsN2MessageTransferReqData->mbs_session_id->tmgi);

    switch (MbsN2MessageTransferReqData->n2_mbs_sm_info->ngap_ie_type) {
    case OpenAPI_mbs_ngap_ie_type_MBS_SES_ACT_REQ:
        n2msgreq = ngap_build_multicast_session_activation_request(&tmgi, n2mbssmbuf);
        break;
    case OpenAPI_mbs_ngap_ie_type_MBS_SES_DEACT_REQ:
        n2msgreq = ngap_build_multicast_session_deactivation_request(&tmgi, n2mbssmbuf);
        break;
    case OpenAPI_mbs_ngap_ie_type_MBS_SES_UPD_REQ:
        n2msgreq = ngap_build_multicast_session_update_request(&tmgi,
                MbsN2MessageTransferReqData->is_area_session_id,
                (uint16_t)MbsN2MessageTransferReqData->area_session_id, n2mbssmbuf);
        break;
    default:
        ogs_error("N2MessageTransfer: unsupported ngapIeType [%d]",
                MbsN2MessageTransferReqData->n2_mbs_sm_info->ngap_ie_type);
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested N2MessageTransfer failed, unsupported ngapIeType", NULL);
        return OGS_ERROR;
    }

    if (!n2msgreq) {
        ogs_error("N2MessageTransfer: failed to build the NGAP PDU");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR,
            recvmsg, "Internal Server Error",
            "Requested N2MessageTransfer failed, could not build the NGAP PDU", NULL);
        return OGS_ERROR;
    }

    // See this function's own doc comment above for the target-selection limitation this loop rests on.
    ogs_list_for_each(&amf_self()->gnb_list, gnb) {
        ogs_pkbuf_t *n2msgreq_copy = ogs_pkbuf_copy(n2msgreq);
        if (!n2msgreq_copy) {
            ogs_error("ogs_pkbuf_copy() failed");
            break;
        }
        ogs_debug("Sending N2 Message Transfer (ngapIeType=%d) to gNB %i",
                MbsN2MessageTransferReqData->n2_mbs_sm_info->ngap_ie_type, gnb->gnb_id);
        gnb_rv = ngap_send_to_gnb(gnb, n2msgreq_copy, NGAP_NON_UE_SIGNALLING);
        if (gnb_rv != OGS_OK) {
            ogs_error("ngap_send_to_gnb() failed");
            break;
        }
    }
    ogs_pkbuf_free(n2msgreq);

    /*********************************************************************
     * Send 200 OK (MbsN2MessageTransferRspData) to the consumer NF
     *********************************************************************/

    memset(&sendmsg, 0, sizeof(sendmsg));
    memset(&MbsN2MessageTransferRspData, 0, sizeof(MbsN2MessageTransferRspData));

    // TS 29.518 cl.5.7.2.2 step 2a: "On success, the AMF shall respond with a '200 OK' status code with
    // MbsN2MessageTransferRspData data structure." No failureList: this pass has no per-gNB response
    // correlation (see the doc comment above -- there is no NGAP response handler for these three
    // procedures yet either, so per-target success/failure cannot be tracked).
    MbsN2MessageTransferRspData.result =
        OpenAPI_n2_information_transfer_result_N2_INFO_TRANSFER_INITIATED;

    sendmsg.MbsN2MessageTransferRspData = &MbsN2MessageTransferRspData;

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_OK);
    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return OGS_OK;
}
