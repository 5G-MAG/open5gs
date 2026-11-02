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

#include <ctype.h>

#include "namf-handler.h"
#include "nsmf-handler.h"

#include "nas-path.h"
#include "ngap-path.h"
#include "sbi-path.h"
#include "namf-build.h"

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
        else {
            // The client holds the address only; the request's target is the whole URI.
            if (mbs_context->notify_uri)
                ogs_free(mbs_context->notify_uri);
            mbs_context->notify_uri = ogs_strdup(notify_uri);
        }
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

/* Whether \p gnb serves the TAI \p tai: its NG Setup Request listed the TAC with the PLMN */
static bool amf_namf_gnb_serves_tai(const amf_gnb_t *gnb, const ogs_tai_t *tai)
{
    ogs_uint24_t tac = ogs_uint24_from_string(tai->tac ? tai->tac : (char *)"0");
    int i, j;

    for (i = 0; i < gnb->num_of_supported_ta_list; i++) {
        if (gnb->supported_ta_list[i].tac.v != tac.v)
            continue;
        for (j = 0; j < gnb->supported_ta_list[i].num_of_bplmn_list; j++) {
            if (memcmp(&gnb->supported_ta_list[i].bplmn_list[j].plmn_id, &tai->plmn_id,
                        sizeof(ogs_plmn_id_t)) == 0)
                return true;
        }
    }

    return false;
}

/*
 * Whether \p gnb serves one of the TAIs of \p area, or \p area does not narrow the choice (none given, or without a TAI
 * list: NCGI matching is not implemented).
 */
static bool amf_namf_gnb_serves_tai_list(const amf_gnb_t *gnb, const ogs_mbs_service_area_t *area)
{
    ogs_tai_t *tai = NULL;

    if (!area || !area->tai_list || ogs_list_count(area->tai_list) == 0)
        return true;

    ogs_list_for_each(area->tai_list, tai) {
        if (amf_namf_gnb_serves_tai(gnb, tai))
            return true;
    }

    return false;
}

static bool amf_namf_area_has_tai(const ogs_mbs_service_area_t *area, const ogs_tai_t *tai)
{
    ogs_tai_t *other = NULL;

    ogs_list_for_each(area->tai_list, other) {
        if (ogs_uint24_from_string(other->tac ? other->tac : (char *)"0").v ==
                ogs_uint24_from_string(tai->tac ? tai->tac : (char *)"0").v &&
                memcmp(&other->plmn_id, &tai->plmn_id, sizeof(ogs_plmn_id_t)) == 0)
            return true;
    }

    return false;
}

/*
 * Whether the change of the MBS service area from \p old_area to \p new_area affects \p gnb: it serves a TAI that is in
 * one of them and not in the other. When either area has no TAI list (none given, or NCGIs only) the change cannot be
 * told by TAI and every node is taken to be affected: NCGI matching is not implemented.
 */
static bool amf_namf_gnb_affected_by_area_change(const amf_gnb_t *gnb,
        const ogs_mbs_service_area_t *old_area, const ogs_mbs_service_area_t *new_area)
{
    ogs_tai_t *tai = NULL;

    if (!old_area || !old_area->tai_list || ogs_list_count(old_area->tai_list) == 0 ||
        !new_area || !new_area->tai_list || ogs_list_count(new_area->tai_list) == 0)
        return true;

    ogs_list_for_each(new_area->tai_list, tai) {
        if (!amf_namf_area_has_tai(old_area, tai) && amf_namf_gnb_serves_tai(gnb, tai))
            return true;
    }
    ogs_list_for_each(old_area->tai_list, tai) {
        if (!amf_namf_area_has_tai(new_area, tai) && amf_namf_gnb_serves_tai(gnb, tai))
            return true;
    }

    return false;
}

/*
 * Whether an MBS session's setup or modification goes to \p gnb. TS 23.247 V18.8.0 cl. 7.3.1 step 3: "The AMF transfers
 * the MBS Session Resource Setup Request message ... to all NG-RANs which support MBS in the MBS service area."
 * A gNB that has not completed NG Setup is not reachable for non-UE signalling. When the stored service area carries
 * TAIs, the gNB must serve one of them. A service area without TAIs (none given, or NCGIs only) does not narrow the
 * choice here. NOT IMPLEMENTED: whether the gNB supports MBS (no NGAP indication of it is held), NCGI matching.
 */
static bool amf_gnb_takes_mbs_session(amf_gnb_t *gnb, amf_mbs_context_t *mbs_context)
{
    ogs_assert(gnb);
    ogs_assert(mbs_context);

    if (!gnb->state.ng_setup_success)
        return false;

    return amf_namf_gnb_serves_tai_list(gnb, mbs_context->mbs_service_area);
}

/*
 * What a ContextUpdate of \p mbs_context does at \p gnb. Without a ranIdList: the NG-RAN nodes that take the session
 * get a Broadcast Session Modification Request, except one that released it at its own request. With a ranIdList (TS 29.518
 * V18.14.0 cl. 5.6.2.3: "the MB-SMF may include one or more ranIds attibutes to request the AMF to setup the Broadcast MBS
 * session in a list of NG-RANs as identified by the NG-RAN ID(s), as specified in clause 8.3.2.3 of TS 23.527"): the listed
 * NG-RAN nodes get a Broadcast Session Setup Request and no other node gets anything (TS 23.527 V18.6.0 cl. 8.3.2.3 step 7:
 * "The AMF sends the N2 MBS Session Setup Request message to the NG-RAN node(s) including the MBS Session Setup or
 * Modification Request Transfer IE."). A node that enters the service area (serves the new one and did not serve the old
 * one) gets a Setup Request, not a Modification Request, and a node that holds the session and leaves it gets a Release
 * Request. With n2MbsInfoChangeInd false (plan->n2_info_unchanged) the nodes that the change
 * of the service area does not affect get nothing. With noNgapSignallingInd true (TS 29.518 V18.14.0 Table 6.5.6.2.5-1: "true: the AMF
 * may consider to not trigger a NGAP signaling message to any NG-RAN."; TS 23.527 V18.6.0 cl. 8.3.2.4 step 8: "The AMF
 * may consider to not trigger any NGAP signalling towards NG-RANs covering the MBS service area.") no node gets anything.
 */
amf_mbs_update_action_e amf_namf_mbs_update_action(
        amf_mbs_context_t *mbs_context, amf_gnb_t *gnb, const amf_mbs_update_plan_t *plan)
{
    amf_mbs_gnb_t *involved = NULL;

    ogs_assert(mbs_context);
    ogs_assert(gnb);
    ogs_assert(plan);

    if (plan->no_ngap_signalling || !gnb->state.ng_setup_success)
        return AMF_MBS_UPDATE_SKIP;

    if (plan->ran_id_list)
        return amf_namf_ran_id_list_has_gnb(plan->ran_id_list, gnb) ?
            AMF_MBS_UPDATE_SETUP : AMF_MBS_UPDATE_SKIP;

    involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);

    if (!amf_gnb_takes_mbs_session(gnb, mbs_context)) {
        // TS 23.247 V18.8.0 cl. 7.3.3 step 3: "the AMF may send ... MBS Session Resource Release to some other NG-RANs in
        // old MBS service area": a node that holds the session, served the old service area and does not serve the new
        // one leaves it.
        if (involved && amf_mbs_gnb_holds_session(involved) && amf_namf_gnb_serves_tai_list(gnb, plan->old_area))
            return AMF_MBS_UPDATE_RELEASE;
        return AMF_MBS_UPDATE_SKIP;
    }

    // The session has been released in this gNB at its own request (BROADCAST SESSION RELEASE REQUIRED): it holds no
    // session to modify.
    if (involved && involved->release_required)
        return AMF_MBS_UPDATE_SKIP;

    // TS 23.247 V18.8.0 cl. 7.3.3 step 3: "Depending on the change of the MBS service area, the AMF may send MBS Session
    // Resource Setup to some NG-RANs in new MBS service area (see clause 7.3.1)": a node that serves the new service area
    // and did not serve the old one holds no session, so it gets a Setup Request, not a Modification Request.
    if (!amf_namf_gnb_serves_tai_list(gnb, plan->old_area))
        return AMF_MBS_UPDATE_SETUP;

    // TS 29.518 V18.14.0 cl. 5.6.2.3: "If the n2MbsInfoChangeInd IE is present in the request and set to "false", the AMF
    // does not need to contact NG-RAN nodes unaffected by the MBS service area change."
    if (plan->n2_info_unchanged &&
            !amf_namf_gnb_affected_by_area_change(gnb, plan->old_area, mbs_context->mbs_service_area))
        return AMF_MBS_UPDATE_UNAFFECTED;

    return AMF_MBS_UPDATE_MODIFY;
}

/*
 * Arms the response window of \p mbs_context: TS 29.518 V18.14.0 cl. 5.6.2.2: "If the AMF does not receive responses
 * from all NG-RAN nodes before the maximum response time elapses since the reception of the
 * Namf_MBSBroadcast_ContextCreate Request, then the AMF should send one Namf_MBSBroadcast_ContextStatusNotify
 * request indicating the incompletion of the Broadcast MBS session establishment." The time is the request's
 * maxResponseTime, in seconds (TS 29.571 DurationSec). A request without it has no window: the specification
 * names no default (Table 6.5.6.2.3-1, NOTE, case (b): "the AMF has not received the maximum response timer in the
 * request from the MB-SMF").
 */
static void amf_mbs_context_start_response_window(
        amf_mbs_context_t *mbs_context, int max_response_time, bool update)
{
    ogs_time_t duration = ogs_time_from_sec(max_response_time);

    ogs_assert(mbs_context);
    ogs_assert(mbs_context->t_response);

    // DurationSec allows 0, a response time that has elapsed already; a timer cannot be armed with no duration.
    if (duration < 1)
        duration = 1;

    mbs_context->response_window_is_update = update;
    mbs_context->response_deadline = ogs_get_monotonic_time() + duration;
    ogs_timer_start(mbs_context->t_response, duration);
}

/*
 * The maximum response time has elapsed (event AMF_EVENT_MBS_TIMER): if not every NG-RAN has answered, one
 * ContextStatusNotify request says that the establishment or the update is incomplete. The context may have been
 * released or replaced since the timer was armed.
 */
void amf_namf_handle_mbs_broadcast_response_timeout(uint32_t mbs_context_index)
{
    amf_mbs_context_t *mbs_context = amf_mbs_context_find_by_index(mbs_context_index);
    ogs_list_t n2_info_list;

    if (!mbs_context) {
        ogs_debug("MBS Broadcast response window: the context has been removed");
        return;
    }

    if (ogs_get_monotonic_time() < mbs_context->response_deadline) {
        ogs_debug("MBS Broadcast response window: stale timer event");
        return;
    }

    if (mbs_context->gnb_response_count >= mbs_context->gnb_request_count)
        return;

    ogs_warn("MBS Broadcast [%s]: %d of %d NG-RANs have answered when the maximum response time elapsed",
            mbs_context->mbs_context_ref, (int)mbs_context->gnb_response_count,
            (int)mbs_context->gnb_request_count);

    ogs_list_init(&n2_info_list);
    amf_sbi_send_mbs_broadcast_context_status_notify(mbs_context, &n2_info_list,
            mbs_context->response_window_is_update ?
                OpenAPI_operation_status_MBS_SESSION_UPDATE_INCOMPLETE :
                OpenAPI_operation_status_MBS_SESSION_START_INCOMPLETE, false, NULL);
}

/*
 * Whether \p list, the mbsServiceAreaInfoList of a request, can be applied: it has 1 to 256 items (TS 29.518 V18.14.0
 * Table 6.5.6.2.2-1: "array(MbsServiceAreaInfo) ... 1..N"; TS 38.413 V18.11.0 clause 9.3.1.208: "maxnoofMBSServiceAreaInformation
 * ... Value is 256" for the MBS Service Area Information Location Dependent List the AMF sends), each Area Session ID is in
 * 0..65535 (TS 29.571 AreaSessionId, Uint16; TS 38.413 MBS-AreaSessionID ::= INTEGER (0..65535, ...)) and no ID is used twice
 * (TS 23.247 V18.8.0 clause 6.2.3: "MB-SMF allocates Area Session ID for each MBS services area which is unique within the
 * MBS session.").
 */
bool amf_namf_area_info_list_is_valid(const OpenAPI_list_t *list)
{
    OpenAPI_lnode_t *node = NULL, *other = NULL;

    if (!list || list->count < 1 || list->count > 256)
        return false;

    OpenAPI_list_for_each(list, node) {
        const OpenAPI_mbs_service_area_info_t *info = node->data;

        if (!info || info->area_session_id < 0 || info->area_session_id > 65535)
            return false;
        for (other = node->next; other; other = other->next) {
            const OpenAPI_mbs_service_area_info_t *o = other->data;

            if (o && o->area_session_id == info->area_session_id)
                return false;
        }
    }

    return true;
}

/*
 * Parses a validated mbsServiceAreaInfoList into \p items (an amf_mbs_area_item_t per entry, in order) and adds the
 * areas of all its entries to \p union_area, which the choice of the NG-RANs uses.
 */
static void amf_namf_parse_area_info_list(const OpenAPI_list_t *list, ogs_list_t *items, ogs_mbs_service_area_t **union_area)
{
    OpenAPI_lnode_t *node = NULL;

    OpenAPI_list_for_each(list, node) {
        OpenAPI_mbs_service_area_info_t *info = node->data;
        amf_mbs_area_item_t *item = ogs_calloc(1, sizeof(*item));

        ogs_assert(item);
        item->area_session_id = (uint16_t)info->area_session_id;
        if (info->mbs_service_area) {
            amf_namf_parse_mbs_service_area(&item->area, info->mbs_service_area);
            amf_namf_parse_mbs_service_area(union_area, info->mbs_service_area);
        }
        ogs_list_add(items, item);
    }
}

/*
 * Whether \p tmgi can be used as the TMGI of an MBS session: the MBS Service ID is six hexadecimal digits and the PLMN
 * is an MCC of three digits and an MNC of two or three. The OpenAPI model does not check that the object, or its members,
 * are present (TS 29.571 R17 TS29571_CommonData.yaml, Tmgi: "mbsServiceId" with pattern '^[A-Fa-f0-9]{6}$', both members
 * required; PlmnId: mcc and mnc required), and ogs_sbi_parse_tmgi() asserts on what it is given.
 */
bool amf_namf_tmgi_is_valid(const OpenAPI_tmgi_t *tmgi)
{
    const char *id = NULL, *mcc = NULL, *mnc = NULL;
    size_t i;

    if (!tmgi || !tmgi->mbs_service_id || !tmgi->plmn_id)
        return false;

    id = tmgi->mbs_service_id;
    mcc = tmgi->plmn_id->mcc;
    mnc = tmgi->plmn_id->mnc;
    if (!mcc || !mnc || strlen(id) != 6 || strlen(mcc) != 3 || (strlen(mnc) != 2 && strlen(mnc) != 3))
        return false;

    for (i = 0; i < 6; i++)
        if (!isxdigit((unsigned char)id[i]))
            return false;
    for (i = 0; i < 3; i++)
        if (!isdigit((unsigned char)mcc[i]))
            return false;
    for (i = 0; mnc[i]; i++)
        if (!isdigit((unsigned char)mnc[i]))
            return false;

    return true;
}

/*
 * A Broadcast Session Setup, Modification or Release Request could not be sent to \p gnb. TS 29.518 V18.14.0 cl. 5.6.2.2:
 * "The AMF may send one or more Namf_MBSBroadcast_ContextStatusNotify request including an operationEvents attribute to
 * report the MB-SMF about failure to reach one or more NG-RANs."; cl. 6.5.6.3.6, NG_RAN_NOT_REACHABLE: "This value
 * indicates that the AMF has failed to reach the NG-RAN when sending a NGAP MBS Session Setup/Modification/Release Request
 * message." The other NG-RANs are still sent theirs.
 */
static void amf_mbs_context_gnb_not_reached(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb)
{
    ogs_error("MBS Broadcast [%s]: gNB %i could not be reached", mbs_context->mbs_context_ref, gnb->gnb_id);
    amf_sbi_send_mbs_broadcast_ngran_event(mbs_context, gnb, OpenAPI_ngran_failure_indication_NG_RAN_NOT_REACHABLE);
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

    if (ContextCreateReqData->mbs_service_area_info_list &&
            !amf_namf_area_info_list_is_valid(ContextCreateReqData->mbs_service_area_info_list)) {
        ogs_error("MBS Broadcast ContextCreate: mbsServiceAreaInfoList is not valid");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested MBS Broadcast ContextCreate failed, [mbsServiceAreaInfoList] must hold 1 to 256 entries with distinct Area Session IDs", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    if (!amf_namf_tmgi_is_valid(ContextCreateReqData->mbs_session_id->tmgi)) {
        ogs_error("MBS Broadcast ContextCreate: mbsSessionId carries no valid TMGI");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request", "Requested MBS Broadcast ContextCreate failed, no valid TMGI in mbsSessionId", NULL);
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
    } else if (ContextCreateReqData->mbs_service_area_info_list) {
                // TS 29.518 V18.14.0 cl. 5.6.2.2 lists, among the content of the POST request, "list of Area Session ID and
                // related MBS service area, for a Location dependent broadcast MBS service". Every entry is kept (the
                // list was validated above); the NG-RAN gets them all in one message (TS 23.247 V18.8.0 cl. 7.3.4: "which
                // additionally includes the Area Session IDs and MBS service areas").
        mbs_context->location_dependent = true;
        amf_namf_parse_area_info_list(ContextCreateReqData->mbs_service_area_info_list,
                &mbs_context->area_items, &mbs_context->mbs_service_area);
        mbs_context->area_session_id = ((amf_mbs_area_item_t *)ogs_list_first(&mbs_context->area_items))->area_session_id;
    }

    // TS 23.527 V18.6.0 cl. 8.3.2.2 step 1: the AMF keeps the last N2 MBS SM container, for the restoration of the
    // session in an NG-RAN.
    if (!amf_mbs_context_store_n2_container(mbs_context, n2mbssmbuf))
        ogs_warn("MBS Broadcast ContextCreate: the N2 MBS SM container could not be kept for restoration");

    // NGAP BROADCAST SESSION SETUP REQUEST message with MBS Session Setup or Modification Request Transfer IE
    n2msgreq = ngap_build_broadcast_session_setup_request(mbs_context, n2mbssmbuf);

        // The 201 is deferred until an NG-RAN answers (below), so a request that reaches no NG-RAN would never
        // be answered and its context never freed, and once the pool is exhausted every later ContextCreate
        // fails. Two cases: the NGAP message cannot be built (with no gNB connected its MBS-ServiceAreaTAIList is
        // empty, which the encoder rejects), checked here, and no gNB accepting it, checked after the loop. No
        // clause names the status for this; UNSPECIFIED_NF_FAILURE is TS 29.500 Table 5.2.7.2-1's generic 500
        // cause.
    if (!n2msgreq) {
        ogs_error("MBS Broadcast ContextCreate: the Broadcast Session Setup Request could not be built");
        amf_mbs_context_remove(mbs_context);
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR,
            recvmsg, "Internal Server Error",
            "Requested MBS Broadcast ContextCreate failed, no NG-RAN reachable", "UNSPECIFIED_NF_FAILURE");
        rv = OGS_ERROR;
        goto cleanup;
    }

        // Each gNB gets its own copy of the message. ngap_send_to_gnb() takes ownership of the pkbuf it is
        // given: it frees it on failure, and on success, for SOCK_STREAM gNBs, splices its embedded lnode into
        // that gNB's write_queue. Passing one buffer to every gNB in this loop would use an already-freed
        // buffer after the first iteration, or splice the same list node into a second queue while the first
        // still references it. The shared template is freed once after the loop, never being sent itself.
    ogs_list_for_each(&amf_self()->gnb_list, gnb) {
        if (!amf_gnb_takes_mbs_session(gnb, mbs_context))
            continue;
        ogs_pkbuf_t *n2msgreq_copy = ogs_pkbuf_copy(n2msgreq);
        if (!n2msgreq_copy) {
            ogs_error("ogs_pkbuf_copy() failed");
            amf_mbs_context_gnb_not_reached(mbs_context, gnb);
            continue;
        }
        ogs_debug("Sending N2 MBS SM info to gNB %i", gnb->gnb_id);
        gnb_rv = ngap_send_to_gnb(gnb, n2msgreq_copy, NGAP_NON_UE_SIGNALLING);
        if (gnb_rv != OGS_OK) {
            ogs_error("ngap_send_to_gnb() failed");
            amf_mbs_context_gnb_not_reached(mbs_context, gnb);
            continue;
        } else {
            ogs_debug("Sent to gnb [%i]", gnb->gnb_id);
            // B-2: how many gNBs the request actually reached, so ngap_handle_broadcast_session_setup_response()
            // can tell when every one of them has responded (ContextStatusNotify's operationStatus).
            mbs_context->gnb_request_count++;
            amf_mbs_context_involve_gnb(mbs_context, gnb);
        }
    }
    ogs_pkbuf_free(n2msgreq);

    if (mbs_context->gnb_request_count == 0) {
        ogs_error("MBS Broadcast ContextCreate: the Broadcast Session Setup Request reached no NG-RAN");
        amf_mbs_context_remove(mbs_context);
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR,
            recvmsg, "Internal Server Error",
            "Requested MBS Broadcast ContextCreate failed, no NG-RAN reachable", "UNSPECIFIED_NF_FAILURE");
        rv = OGS_ERROR;
        goto cleanup;
    }

    if (ContextCreateReqData->is_max_response_time)
        amf_mbs_context_start_response_window(mbs_context, ContextCreateReqData->max_response_time, false);

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

static void amf_mbs_context_release(amf_mbs_context_t *mbs_context);
static void amf_mbs_context_restoration_response(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb, bool success,
        const OCTET_STRING_t *transfer);

/*
 * Builds and sends the deferred Namf_MBSBroadcast_ContextCreate response (OGS_SBI_HTTP_STATUS_CREATED),
 * once amf_namf_handle_mbs_broadcast_ran_response() has correlated the first successful NG-RAN response to \p
 * mbs_context. Split out of amf_namf_handle_mbs_broadcast_context_create() itself since that function
 * returns to its SBI caller long before this can run -- see mbs_context->stream_id's own comment
 * (context.h) for why the stream must be re-looked-up rather than captured by raw pointer.
 *
 * \p n2_info_list holds the containers of the NG-RAN responses received so far and \p operation_status is
 * START_COMPLETE when every NG-RAN the request was sent to has responded (TS 29.518 cl. 5.6.2.2: "If the AMF
 * received the NG-RAN responses from all involved NG-RAN(s) ... the AMF shall include an indication of completion
 * of the operation in all NG-RANs in the 201 Created response."), OpenAPI_operation_status_NULL otherwise.
 */
static void amf_namf_send_mbs_broadcast_context_create_response(amf_mbs_context_t *mbs_context,
        ogs_list_t *n2_info_list, OpenAPI_operation_status_e operation_status)
{
    OpenAPI_context_create_rsp_data_t *ContextCreateRspData = NULL;
    OpenAPI_tmgi_t *tmgi_copy = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id_copy = NULL;
    OpenAPI_list_t *n2_mbs_sm_info_list = NULL;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_stream_t *stream = NULL;
    ogs_sbi_server_t *server = NULL;
    ogs_sbi_header_t header;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(mbs_context);
    ogs_assert(n2_info_list);

    stream = ogs_sbi_stream_find_by_id(mbs_context->stream_id);
    if (!stream) {
        // The consumer's stream is gone by the time the first gNB responded: the MB-SMF released its session
        // (or gave up) before this ContextCreate was answered, so no consumer holds this context's mbsContextRef
        // and none can ever delete it. It is released here instead of being kept for ever.
        ogs_warn("MBS Broadcast ContextCreate: stream [%d] no longer exists, response not sent; "
                "releasing the context", (int)mbs_context->stream_id);
        mbs_context->stream_id = OGS_INVALID_POOL_ID;
        amf_mbs_context_release(mbs_context);
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

    n2_mbs_sm_info_list = amf_namf_build_n2_mbs_sm_info_list(&sendmsg, n2_info_list);

    ContextCreateRspData = OpenAPI_context_create_rsp_data_create(
            mbs_session_id_copy, n2_mbs_sm_info_list, operation_status);

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

    if (ContextCreateRspData) {
        // ContextCreateRspData owns n2_mbs_sm_info_list, which is freed with it
        OpenAPI_context_create_rsp_data_free(ContextCreateRspData);
        n2_mbs_sm_info_list = NULL;
    }

    if (sendmsg.http.location)
        ogs_free(sendmsg.http.location);

    // Mark as answered so a second/subsequent gNB response does not re-trigger this same 201 Created a second
    // time.
    mbs_context->stream_id = OGS_INVALID_POOL_ID;

    amf_namf_free_n2_mbs_sm_info_list(n2_mbs_sm_info_list);
    amf_mbs_n2_info_remove_all(n2_info_list);
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
/*
 * The ContextUpdate is accepted: its service area stays, the container it carries becomes the last one received (TS 23.527
 * V18.6.0 cl. 8.3.2.2 step 1: "The AMF stores the last N2 MBS SM container ... received from the MB-SMF during the
 * establishment of the broadcast MBS session or during a broadcast MBS session update."), and the notification URI it
 * carries replaces the earlier one (TS 29.518 V18.14.0 cl. 5.6.2.3: "if the NF Service Consumer wishes to modify the
 * notification URI where to be notified about the status change of the broadcast MBS session context").
 */
static void amf_mbs_update_accept(amf_mbs_context_t *mbs_context, amf_mbs_area_undo_t *area_undo,
        ogs_pkbuf_t *n2mbssmbuf, char *notify_uri)
{
    amf_mbs_context_keep_area(area_undo);
    if (n2mbssmbuf && !amf_mbs_context_store_n2_container(mbs_context, n2mbssmbuf))
        ogs_warn("MBS Broadcast ContextUpdate: the N2 MBS SM container could not be kept for restoration");
    if (notify_uri)
        amf_namf_set_mbs_context_notify_client(mbs_context, notify_uri, "ContextUpdate");
}

int amf_namf_handle_mbs_broadcast_context_update(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    ogs_debug("MBS Broadcast ContextUpdate request received");

    OpenAPI_context_update_req_data_t *ContextUpdateReqData = NULL;

    const char *mbs_context_ref = NULL;
    amf_mbs_context_t *mbs_context = NULL;
    ogs_pkbuf_t *n2mbssmbuf = NULL;
    ogs_pkbuf_t *n2msgreq = NULL;       /* the Modification Request, sent to the nodes that hold the session */
    ogs_pkbuf_t *n2setupreq = NULL;     /* the Setup Request, sent to the nodes that are to get the session */
    ogs_pkbuf_t *n2releasereq = NULL;   /* the Release Request, sent to the nodes that left the service area */
    int released = 0;
    amf_gnb_t *gnb = NULL;
    amf_mbs_update_plan_t plan;
    ogs_mbs_service_area_t *new_area = NULL;    /* the service area of the request, applied once it is accepted */
    ogs_list_t new_items;                       /* its Area Session IDs, when it carries the list */
    amf_mbs_area_undo_t area_undo;              /* what applying it replaced */
    bool has_service_area = false;
    int unaffected = 0;
    bool send_failed = false;

    int rv = OGS_OK;
    int gnb_rv = OGS_OK;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    memset(&plan, 0, sizeof(plan));
    memset(&area_undo, 0, sizeof(area_undo));
    ogs_list_init(&new_items);

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

        // One deferred response per context: the stream, the request count and the response count are shared by
        // ContextCreate, ContextUpdate and ContextRelease. An update that arrived now would take the stream of the
        // operation in progress and leave it unanswered. No clause names the status; UNSPECIFIED_NF_FAILURE is
        // TS 29.500 Table 5.2.7.2-1's generic 500 cause.
    if (ogs_sbi_stream_find_by_id(mbs_context->stream_id) || mbs_context->release_pending) {
        ogs_error("MBS Broadcast ContextUpdate: mbsContextRef[%s] has an operation in progress", mbs_context_ref);
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR,
            recvmsg, "Internal Server Error",
            "Requested MBS Broadcast ContextUpdate failed, another operation on the context is in progress",
            "UNSPECIFIED_NF_FAILURE");
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
    if (ContextUpdateReqData->mbs_service_area_info_list &&
            !amf_namf_area_info_list_is_valid(ContextUpdateReqData->mbs_service_area_info_list)) {
        ogs_error("MBS Broadcast ContextUpdate: mbsServiceAreaInfoList is not valid");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, "Bad Request",
            "Requested MBS Broadcast ContextUpdate failed, [mbsServiceAreaInfoList] must hold 1 to 256 entries with distinct Area Session IDs", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }
    has_service_area = ContextUpdateReqData->mbs_service_area ||
        ContextUpdateReqData->mbs_service_area_info_list;

        // The service area this Update carries is parsed here and applied only once the request has been accepted
        // (see the apply below): an update that is refused leaves the service area as it was. Without it
        // ngap_build_broadcast_session_modification_request() has nothing real to send and falls back to every
        // connected gNB's own TAI. This Update's mbsServiceArea replaces whatever ContextCreate stored, being
        // this update's authoritative value.
    if (ContextUpdateReqData->mbs_service_area) {
        amf_namf_parse_mbs_service_area(&new_area, ContextUpdateReqData->mbs_service_area);
        if (!new_area)
            ogs_warn("MBS Broadcast ContextUpdate: mbsServiceArea carried neither a TAI nor an NCGI; "
                    "NGAP will use whatever service area was previously stored, or fall back further");
    } else if (has_service_area && ContextUpdateReqData->mbs_service_area_info_list &&
            ContextUpdateReqData->mbs_service_area_info_list->count > 0) {
                // mbsServiceAreaInfoList replaces the Area Session IDs and their areas (TS 29.518 V18.14.0 cl. 5.6.2.3:
                // "updated list of Area Session ID and related MBS service area, for a Location dependent broadcast MBS
                // session."), as ContextCreate takes the list; it was validated above.
        amf_namf_parse_area_info_list(ContextUpdateReqData->mbs_service_area_info_list, &new_items, &new_area);
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

    // ranIdList (O): the update is performed in those NG-RANs only, with a Broadcast Session Setup Request that
    // carries the container of the request (TS 23.527 V18.6.0 cl. 8.3.2.3 step 6: "...ContextUpdate Request ...
    // including the MBS Session Setup or Modification Request Transfer IE and the NG-RAN ID").
    plan.no_ngap_signalling =
        ContextUpdateReqData->no_ngap_signalling_ind == OpenAPI_context_update_req_data_NONGAPSIGNALLINGIND__true;
    plan.n2_info_unchanged = ContextUpdateReqData->is_n2_mbs_info_change_ind && !ContextUpdateReqData->n2_mbs_info_change_ind;

    if (!plan.no_ngap_signalling && ContextUpdateReqData->ran_id_list && ContextUpdateReqData->ran_id_list->count > 0) {
        if (!n2mbssmbuf) {
            ogs_error("MBS Broadcast ContextUpdate: ranIdList without n2MbsSmInfo");
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "Bad Request",
                "Requested MBS Broadcast ContextUpdate failed, [ranIdList] needs the N2 MBS SM info to set the session up with", NULL);
            rv = OGS_ERROR;
            goto cleanup;
        }
        plan.ran_id_list = ContextUpdateReqData->ran_id_list;
    }

    // The request is valid: its service area is applied. Until amf_mbs_update_accept() keeps it, every exit that
    // refuses the update puts the earlier service area back (all or none).
    amf_mbs_context_apply_area(mbs_context, new_area, &new_items, &area_undo);
    new_area = NULL;
    plan.old_area = area_undo.replaced ? area_undo.old_area : mbs_context->mbs_service_area;

    // noNgapSignallingInd: this AMF takes over the session (TS 23.527 V18.6.0 cl. 8.3.2.4 step 8: "...to request the
    // AMF2 to be the AMF for the Broadcast MBS Session to handle subsequent MBS session signaling and be responsible
    // for triggering restoration procedures for NG-RAN failure with or without restart") and keeps the service area and
    // the container, both stored above, "just to store it for future potential NG-RAN restoration" (cl. 8.3.2.4).
    // Nothing is sent to an NG-RAN, so nothing waits for a response: step 9, "The AMF responds the
    // Namf_MBSBroadcast_ContextUpdate Request message.", is a 204 No Content (TS 29.518 cl. 5.6.2.3 step 2b).
    if (plan.no_ngap_signalling) {
        ogs_sbi_message_t sendmsg;
        ogs_sbi_response_t *response = NULL;

        amf_mbs_update_accept(mbs_context, &area_undo, n2mbssmbuf, ContextUpdateReqData->notify_uri);
        memset(&sendmsg, 0, sizeof(sendmsg));
        response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
        ogs_assert(response);
        ogs_assert(true == ogs_sbi_server_send_response(stream, response));
        goto cleanup;
    }

    // Reset B-2's completion-tracking counters: ContextCreate's own use of them has already finished by
    // the time an Update can happen (this AMF creates exactly one context per TMGI, and ContextCreate
    // always completes -- successfully or not -- before an SMF would send it an Update for the same
    // context), so reusing them here for this Update's own completion tracking is safe.
    mbs_context->gnb_request_count = 0;
    mbs_context->gnb_response_count = 0;
    amf_mbs_n2_info_remove_all(&mbs_context->pending_n2_info_list);
    ogs_timer_stop(mbs_context->t_response);

    if (!plan.ran_id_list)
        n2msgreq = ngap_build_broadcast_session_modification_request(mbs_context, has_service_area, n2mbssmbuf);

        // As in ContextCreate: a request that cannot be built, or that no gNB accepts, is answered here, since
        // no NG-RAN response will ever complete the deferred one. No clause names the status for this;
        // UNSPECIFIED_NF_FAILURE is TS 29.500 Table 5.2.7.2-1's generic 500 cause. The context itself stays: an
        // update that failed does not end the session.
    if (!n2msgreq && !plan.ran_id_list) {
        ogs_error("MBS Broadcast ContextUpdate: the Broadcast Session Modification Request could not be built");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR,
            recvmsg, "Internal Server Error",
            "Requested MBS Broadcast ContextUpdate failed, no NG-RAN reachable", "UNSPECIFIED_NF_FAILURE");
        rv = OGS_ERROR;
        goto cleanup;
    }

        // Each gNB gets its own copy, for the ownership reason given in ContextCreate's send loop.
    ogs_list_for_each(&amf_self()->gnb_list, gnb) {
        amf_mbs_update_action_e action = amf_namf_mbs_update_action(mbs_context, gnb, &plan);
        ogs_pkbuf_t *n2msgreq_copy = NULL;

        if (action == AMF_MBS_UPDATE_UNAFFECTED)
            unaffected++;
        if (action == AMF_MBS_UPDATE_SKIP || action == AMF_MBS_UPDATE_UNAFFECTED)
            continue;
        if (action == AMF_MBS_UPDATE_RELEASE) {
            // A node that left the service area is released; nothing waits for its answer, which goes to the MB-SMF in
            // a ContextStatusNotify request (amf_namf_handle_mbs_broadcast_area_release_response()).
            amf_mbs_gnb_t *leaving = amf_mbs_context_find_involved_gnb(mbs_context, gnb);

            if (!n2releasereq)
                n2releasereq = ngap_build_broadcast_session_release_request(mbs_context);
            n2msgreq_copy = n2releasereq ? ogs_pkbuf_copy(n2releasereq) : NULL;
            if (!n2msgreq_copy || ngap_send_to_gnb(gnb, n2msgreq_copy, NGAP_NON_UE_SIGNALLING) != OGS_OK) {
                ogs_error("MBS Broadcast ContextUpdate: the Broadcast Session Release Request could not be sent to gNB %i",
                        gnb->gnb_id);
                send_failed = true;
                amf_mbs_context_gnb_not_reached(mbs_context, gnb);
                continue;
            }
            if (leaving) {
                leaving->area_released = true;
                leaving->area_release_pending = true;
            }
            released++;
            continue;
        }
        // The Setup Request carries the container of the request, or else the last one received (TS 23.527 V18.6.0
        // cl. 8.3.2.2 step 1).
        if (action == AMF_MBS_UPDATE_SETUP && !n2setupreq) {
            ogs_pkbuf_t *container = n2mbssmbuf ? n2mbssmbuf : mbs_context->n2_mbs_sm_request;

            if (container)
                n2setupreq = ngap_build_broadcast_session_setup_request(mbs_context, container);
            if (!n2setupreq) {
                ogs_error("MBS Broadcast ContextUpdate: the Broadcast Session Setup Request could not be built");
                send_failed = true;
                continue;
            }
        }
        n2msgreq_copy = ogs_pkbuf_copy(action == AMF_MBS_UPDATE_SETUP ? n2setupreq : n2msgreq);
        if (!n2msgreq_copy) {
            ogs_error("ogs_pkbuf_copy() failed");
            send_failed = true;
            amf_mbs_context_gnb_not_reached(mbs_context, gnb);
            continue;
        }
        ogs_debug("Sending N2 MBS SM info (ContextUpdate) to gNB %i", gnb->gnb_id);
        gnb_rv = ngap_send_to_gnb(gnb, n2msgreq_copy, NGAP_NON_UE_SIGNALLING);
        if (gnb_rv != OGS_OK) {
            ogs_error("ngap_send_to_gnb() failed");
            send_failed = true;
            amf_mbs_context_gnb_not_reached(mbs_context, gnb);
            continue;
        } else {
            amf_mbs_gnb_t *involved = NULL;

            mbs_context->gnb_request_count++;
            amf_mbs_context_involve_gnb(mbs_context, gnb);
            involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);
            if (involved && action == AMF_MBS_UPDATE_MODIFY)
                involved->awaiting_modification = true;
            if (involved && action == AMF_MBS_UPDATE_SETUP) {
                // The session is set up in the node again: it serves it, whatever it asked for or failed before.
                involved->update_setup = true;
                involved->release_required = false;
                involved->released = false;
                involved->setup_failed = false;
                involved->area_released = false;
                involved->area_release_pending = false;
            }
        }
    }
    if (n2msgreq)
        ogs_pkbuf_free(n2msgreq);
    if (n2setupreq)
        ogs_pkbuf_free(n2setupreq);
    if (n2releasereq)
        ogs_pkbuf_free(n2releasereq);

    // Every node that takes the session was left out as not affected by the change (n2MbsInfoChangeInd false), or has
    // been released as it left the service area: there is nothing to wait for, and "the AMF does not need to contact
    // NG-RAN nodes unaffected by the MBS service area change" (TS 29.518 V18.14.0 cl. 5.6.2.3). 204 No Content: no
    // additional information is to be returned (step 2b).
    if (mbs_context->gnb_request_count == 0 && unaffected + released > 0 && !send_failed) {
        ogs_sbi_message_t sendmsg;
        ogs_sbi_response_t *response = NULL;

        amf_mbs_update_accept(mbs_context, &area_undo, n2mbssmbuf, ContextUpdateReqData->notify_uri);
        memset(&sendmsg, 0, sizeof(sendmsg));
        response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
        ogs_assert(response);
        ogs_assert(true == ogs_sbi_server_send_response(stream, response));
        goto cleanup;
    }

    if (mbs_context->gnb_request_count == 0) {
        ogs_error("MBS Broadcast ContextUpdate: the Broadcast Session Modification Request reached no NG-RAN");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR,
            recvmsg, "Internal Server Error",
            "Requested MBS Broadcast ContextUpdate failed, no NG-RAN reachable", "UNSPECIFIED_NF_FAILURE");
        rv = OGS_ERROR;
        goto cleanup;
    }

    amf_mbs_update_accept(mbs_context, &area_undo, n2mbssmbuf, ContextUpdateReqData->notify_uri);

    if (ContextUpdateReqData->is_max_response_time)
        amf_mbs_context_start_response_window(mbs_context, ContextUpdateReqData->max_response_time, true);

    // Defer the SBI response the same way ContextCreate does -- cl.5.6.2.3 step 2a/2b: "On success, '200
    // OK' shall be returned if additional information needs to be returned ... '204 No Content' shall be
    // returned if no additional information needs to be returned", both conditioned on NG-RAN responses
    // this AMF has not received yet.
    mbs_context->stream_id = ogs_sbi_id_from_stream(stream);

cleanup:
    // An update that was refused leaves the service area as it was; one that was not applied frees its area.
    amf_mbs_context_undo_area(mbs_context, &area_undo);
    if (new_area)
        ogs_mbs_service_area_free(new_area);
    amf_mbs_area_items_free(&new_items);
    if (rv == OGS_OK)
        return true;
    else
        return false;
}

/*
 * Builds and sends the deferred Namf_MBSBroadcast_ContextUpdate response, once
 * amf_namf_handle_mbs_broadcast_ran_response() has correlated the first successful NG-RAN response to \p mbs_context.
 * TS 29.518 cl. 5.6.2.3: "On success, "200 OK" shall be returned if additional information needs to be returned in
 * the response." and "On success, "204 No Content" shall be returned if no additional information needs to be
 * returned in the response." The additional information is the containers of the NG-RAN responses received so far
 * (\p n2_info_list) and the indication of completion (\p operation_status, UPDATE_COMPLETE when every NG-RAN the
 * request was sent to has responded).
 */
static void amf_namf_send_mbs_broadcast_context_update_response(amf_mbs_context_t *mbs_context,
        ogs_list_t *n2_info_list, OpenAPI_operation_status_e operation_status)
{
    OpenAPI_context_update_rsp_data_t ContextUpdateRspData;
    OpenAPI_list_t *n2_mbs_sm_info_list = NULL;
    ogs_sbi_message_t sendmsg;
    ogs_sbi_stream_t *stream = NULL;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(mbs_context);
    ogs_assert(n2_info_list);

    stream = ogs_sbi_stream_find_by_id(mbs_context->stream_id);
    if (!stream) {
        ogs_warn("MBS Broadcast ContextUpdate: stream [%d] no longer exists, response not sent",
                (int)mbs_context->stream_id);
        mbs_context->stream_id = OGS_INVALID_POOL_ID;
        amf_mbs_n2_info_remove_all(n2_info_list);
        return;
    }

    memset(&sendmsg, 0, sizeof(sendmsg));

    n2_mbs_sm_info_list = amf_namf_build_n2_mbs_sm_info_list(&sendmsg, n2_info_list);

    if (n2_mbs_sm_info_list || operation_status != OpenAPI_operation_status_NULL) {
        memset(&ContextUpdateRspData, 0, sizeof(ContextUpdateRspData));
        ContextUpdateRspData.n2_mbs_sm_info_list = n2_mbs_sm_info_list;
        ContextUpdateRspData.operation_status = operation_status;
        sendmsg.ContextUpdateRspData = &ContextUpdateRspData;

        response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_OK);
    } else {
        response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
    }
    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    amf_namf_free_n2_mbs_sm_info_list(n2_mbs_sm_info_list);
    amf_mbs_n2_info_remove_all(n2_info_list);

    mbs_context->stream_id = OGS_INVALID_POOL_ID;
}

/*
 * Handles one NG-RAN node's response to a Broadcast Session Setup (\p update false) or Modification (\p update true)
 * Request: a BROADCAST SESSION SETUP or MODIFICATION RESPONSE (\p success) or FAILURE, with the MBS Session Setup or
 * Modification Response or Failure Transfer IE \p transfer if the node sent one (\p ngap_ie_type tells which). Decides
 * what goes to the MB-SMF, TS 29.518 cl. 5.6.2.2 and 5.6.2.3:
 *
 *   - the deferred ContextCreate or ContextUpdate response, on the first successful response; it carries the
 *     containers received so far and, when every NG-RAN has responded, the indication of completion;
 *   - a ContextStatusNotify request for each later response that has a container, and for the one that completes the
 *     set of responses, which carries the indication of completion;
 *   - a failure status when every NG-RAN a ContextCreate was sent to failed, or when the first response to a
 *     ContextUpdate is a failure.
 */
void amf_namf_handle_mbs_broadcast_ran_response(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb,
        bool update, bool success, OpenAPI_ngap_ie_type_e ngap_ie_type, const OCTET_STRING_t *transfer)
{
    const char *operation = update ? "ContextUpdate" : "ContextCreate";
    OpenAPI_operation_status_e operation_status = OpenAPI_operation_status_NULL;
    amf_mbs_n2_info_t *n2_info = NULL;
    bool completed;
    bool update_setup = false;

    ogs_assert(mbs_context);
    ogs_assert(gnb);

    {
        amf_mbs_gnb_t *involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);

        // Each gNB counts once per request: a response from a gNB that was not sent the request, or a second
        // response to the same request, is not one of the responses the consumer waits for.
        if (!involved || !involved->awaiting) {
            ogs_warn("%s: response from gNB %i, which has no request outstanding, ignored", operation, gnb->gnb_id);
            return;
        }
        involved->awaiting = false;
        update_setup = involved->update_setup;

        // The answer to the Setup Request that restores the session in a gNB that restarted is not one of the answers a
        // ContextCreate or ContextUpdate waits for.
        if (involved->restoring) {
            involved->restoring = false;
            amf_mbs_context_restoration_response(mbs_context, gnb, success, transfer);
            return;
        }

        // A failed Setup means the node does not serve the session (it is then not one of "all the NG-RANs serving
        // the Broadcast MBS session", TS 29.518 cl. 5.6.2.5); any success, or a failure of a Modification, leaves
        // it as it was.
        if (involved && success)
            involved->setup_failed = false;
        else if (involved && !update)
            involved->setup_failed = true;
    }

    // The response of a node to the Setup Request that a ContextUpdate sent it (ranIdList) completes that update.
    if (update_setup) {
        update = true;
        operation = "ContextUpdate";
    }

    mbs_context->gnb_response_count++;
    completed = mbs_context->gnb_response_count >= mbs_context->gnb_request_count;
    if (completed)
        ogs_timer_stop(mbs_context->t_response);
    if (completed)
        operation_status = update ?
            OpenAPI_operation_status_MBS_SESSION_UPDATE_COMPLETE : OpenAPI_operation_status_MBS_SESSION_START_COMPLETE;

    if (transfer && transfer->size > 0)
        n2_info = amf_mbs_n2_info_create(gnb, ngap_ie_type, transfer->buf, transfer->size);

    if (mbs_context->stream_id == OGS_INVALID_POOL_ID) {
        // The ContextCreate or ContextUpdate response has been sent: what this node reports goes in a
        // ContextStatusNotify request, if it has a container or completes the set of responses.
        ogs_list_t n2_info_list;

        if (!n2_info && !completed) {
            ogs_debug("%s: additional response carries nothing to notify, ignored", operation);
            return;
        }

        ogs_list_init(&n2_info_list);
        if (n2_info)
            ogs_list_add(&n2_info_list, n2_info);
        amf_sbi_send_mbs_broadcast_context_status_notify(mbs_context, &n2_info_list, operation_status, false, NULL);
        amf_mbs_n2_info_remove_all(&n2_info_list);
        return;
    }

    if (success) {
        if (n2_info)
            ogs_list_add(&mbs_context->pending_n2_info_list, n2_info);
        if (update)
            amf_namf_send_mbs_broadcast_context_update_response(
                    mbs_context, &mbs_context->pending_n2_info_list, operation_status);
        else
            amf_namf_send_mbs_broadcast_context_create_response(
                    mbs_context, &mbs_context->pending_n2_info_list, operation_status);
        return;
    }

    if (!update && !completed) {
        // No NG-RAN has succeeded yet and another may still: the ContextCreate is answered by that success (cl.
        // 5.6.2.2: "The AMF should respond success when it receives the first successful response from the
        // NG-RAN(s)."). This node's Failure Transfer goes in that response.
        if (n2_info) {
            if (ogs_list_count(&mbs_context->pending_n2_info_list) < OGS_SBI_MAX_NUM_OF_PART - 1)
                ogs_list_add(&mbs_context->pending_n2_info_list, n2_info);
            else {
                ogs_warn("%s: too many containers waiting for the response, one is not sent", operation);
                amf_mbs_n2_info_free(n2_info);
            }
        }
        return;
    }

    // A ContextCreate that every NG-RAN it reached has failed, or a ContextUpdate whose first response is a failure.
    // No clause names the failure status; a ProblemDetails cannot carry the container.
    {
        ogs_sbi_stream_t *stream = ogs_sbi_stream_find_by_id(mbs_context->stream_id);
        mbs_context->stream_id = OGS_INVALID_POOL_ID;

        if (stream) {
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL,
                    update ? "Requested MBS Broadcast ContextUpdate failed" :
                             "Requested MBS Broadcast ContextCreate failed",
                    update ? "NG-RAN reported a Broadcast Session Modification Failure" :
                             "NG-RAN reported a Broadcast Session Setup Failure", NULL);
        } else {
            ogs_warn("%s: stream no longer exists, response not sent", operation);
        }
    }

    if (n2_info)
        amf_mbs_n2_info_free(n2_info);

    // A ContextCreate that failed leaves the consumer no mbsContextRef to delete: the context is not kept. A
    // ContextUpdate that failed does not end the session.
    if (!update)
        amf_mbs_context_remove(mbs_context);
}

/*
 * TS 23.527 V18.6.0 cl. 8.3.3: "When the AMF detects NG-RAN failure without restart, e.g. using SCTP path failure
 * detection mechanism, it shall report such event to the MB-SMF for each of MBS session(s) affected by this failure, to
 * enable the MB-SMF to remove the DL F-TEID allocated by the failed NG-RAN in the MB-UPF when unicast transport is used
 * over N3mb interface." Step 3: "The AMF sends Namf_MBSBroadcast_ContextStatusNotify Request message to the MB-SMF for each
 * affected MBS session including the MBS Session ID, the NG-RAN ID and an indication of the NG-RAN failure without restart
 * event." The sessions affected are those the node holds: it did not fail its Setup, did not ask for the release and was
 * not released for leaving the service area. Whatever the option, the answers the lost node still owes to a
 * ContextCreate or ContextUpdate are taken as refusals, so that the operation ends.
 */
void amf_namf_handle_mbs_broadcast_gnb_lost(amf_gnb_t *gnb)
{
    amf_mbs_context_t *mbs_context = NULL, *next = NULL;

    ogs_assert(gnb);

    // The context may be removed while it is handled (a ContextCreate whose every NG-RAN has failed).
    ogs_list_for_each_safe(&amf_self()->amf_mbs_context_list, next, mbs_context) {
        amf_mbs_gnb_t *involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);

        if (!involved)
            continue;

        if (amf_self()->mbs_report_ng_ran_failure && amf_mbs_gnb_holds_session(involved)) {
            ogs_info("MBS Broadcast [%s]: gNB %i failed, the MB-SMF is told", mbs_context->mbs_context_ref, gnb->gnb_id);
            amf_sbi_send_mbs_broadcast_ngran_event(
                    mbs_context, gnb, OpenAPI_ngran_failure_indication_NG_RAN_FAILURE_WITHOUT_RESTART);
        }

        // The answers the gNB owes will not come: the operation they belong to goes on as if the gNB had refused the
        // request. TS 29.518 V18.14.0 cl. 5.6.2.2 and 5.6.2.3 name the outcome for NG-RANs that fail ("The AMF should respond
        // success when it receives the first successful response from the NG-RAN(s)" and failure when every one failed,
        // TS 23.247 V18.8.0 cl. 7.3.1 step 7: "if all NG-RAN(s) report failure, the AMF should respond failure"), and say
        // nothing about a lost association.
        involved->area_release_pending = false;
        // The release a ContextRelease sent to the gNB will not be answered: it counts as answered, without a transfer.
        if (mbs_context->release_pending && involved->release_sent) {
            amf_namf_send_mbs_broadcast_context_release_response(mbs_context, gnb, NULL);
            continue;
        }
        switch (amf_mbs_gnb_owes(involved)) {
        case AMF_MBS_OWES_SETUP_RESPONSE:
            amf_namf_handle_mbs_broadcast_ran_response(mbs_context, gnb, false, false, OpenAPI_ngap_ie_type_MBS_SES_FAIL, NULL);
            break;
        case AMF_MBS_OWES_UPDATE_RESPONSE:
            amf_namf_handle_mbs_broadcast_ran_response(mbs_context, gnb, true, false, OpenAPI_ngap_ie_type_MBS_SES_FAIL, NULL);
            break;
        case AMF_MBS_OWES_RESTORATION_RESPONSE:
            involved->awaiting = false;
            involved->restoring = false;
            break;
        default:
            break;
        }
    }
}

/*
 * Restores the session of \p mbs_context in \p gnb, which restarted or is new (TS 23.527 V18.6.0 cl. 8.3.2.2 step 4:
 * "The AMF re-establishes the broadcast MBS session in the restarted NG-RAN, by sending an NGAP Broadcast Session Setup
 * Request including the last N2 (NGAP) MBS Session Setup or Modification Request Transfer IE that was stored."). Its
 * response is handled by amf_mbs_context_restoration_response(). Returns false when nothing was sent.
 */
static bool amf_mbs_context_restore_in_gnb(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb)
{
    ogs_pkbuf_t *n2msgreq = NULL;
    amf_mbs_gnb_t *involved = NULL;

    ogs_assert(mbs_context);
    ogs_assert(gnb);

    if (!mbs_context->n2_mbs_sm_request) {
        ogs_warn("MBS Broadcast [%s]: no N2 MBS SM container is held, the session is not restored in gNB %i",
                mbs_context->mbs_context_ref, gnb->gnb_id);
        return false;
    }

    n2msgreq = ngap_build_broadcast_session_setup_request(mbs_context, mbs_context->n2_mbs_sm_request);
    if (!n2msgreq) {
        ogs_error("MBS Broadcast [%s]: the Broadcast Session Setup Request could not be built", mbs_context->mbs_context_ref);
        return false;
    }
    if (ngap_send_to_gnb(gnb, n2msgreq, NGAP_NON_UE_SIGNALLING) != OGS_OK) {
        ogs_error("MBS Broadcast [%s]: the Broadcast Session Setup Request could not be sent to gNB %i",
                mbs_context->mbs_context_ref, gnb->gnb_id);
        amf_mbs_context_gnb_not_reached(mbs_context, gnb);
        return false;
    }

    // The same gNB, connected anew, keeps its record; a gNB the session has not been sent to gets one.
    involved = amf_mbs_context_find_stale_involved_gnb(mbs_context, gnb);
    if (involved) {
        involved->gnb_id = gnb->id;
        involved->awaiting = true;
    } else {
        amf_mbs_context_involve_gnb(mbs_context, gnb);
        involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);
    }
    if (involved) {
        involved->restoring = true;
        involved->update_setup = false;
        involved->release_required = false;
        involved->released = false;
        involved->setup_failed = false;
        involved->area_released = false;
        involved->area_release_pending = false;
    }

    ogs_info("MBS Broadcast [%s]: the session is restored in gNB %i", mbs_context->mbs_context_ref, gnb->gnb_id);
    return true;
}

/*
 * The answer of \p gnb to the Setup Request that restores the session. TS 23.527 V18.6.0 cl. 8.3.2.2 step 5: "If an N2
 * MBS SM Container (i.e. N2 (NGAP) MBS Session Setup or Modification Response Transfer IE) was received from the NG-RAN
 * in step 4, the AMF sends an Namf_MBSBroadcast_ContextStatusNotify Request to the MB-SMF including the N2 MBS SM
 * Container and the RAN-ID of the NG-RAN." The answer is not one of the responses a ContextCreate or ContextUpdate waits
 * for. A failure is only logged: the clause says nothing to send for it.
 */
static void amf_mbs_context_restoration_response(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb, bool success,
        const OCTET_STRING_t *transfer)
{
    amf_mbs_n2_info_t *n2_info = NULL;
    ogs_list_t n2_info_list;

    if (!success) {
        amf_mbs_gnb_t *involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);

        ogs_warn("MBS Broadcast [%s]: gNB %i did not take the restored session", mbs_context->mbs_context_ref, gnb->gnb_id);
        if (involved)
            involved->setup_failed = true;
        return;
    }

    if (!transfer || transfer->size == 0)
        return;

    n2_info = amf_mbs_n2_info_create(gnb, OpenAPI_ngap_ie_type_MBS_SES_RSP, transfer->buf, transfer->size);
    if (!n2_info)
        return;
    ogs_list_init(&n2_info_list);
    ogs_list_add(&n2_info_list, n2_info);
    amf_sbi_send_mbs_broadcast_context_status_notify(
            mbs_context, &n2_info_list, OpenAPI_operation_status_NULL, false, NULL);
    amf_mbs_n2_info_remove_all(&n2_info_list);
}

void amf_namf_handle_mbs_broadcast_ng_setup(amf_gnb_t *gnb)
{
    amf_mbs_context_t *mbs_context = NULL;

    ogs_assert(gnb);

    if (amf_self()->mbs_restoration == AMF_MBS_RESTORATION_NONE)
        return;

    // TS 23.527 V18.6.0 cl. 8.3.2.2 step 3: the NG-RAN "serves at least one TAI or one cell being part of the MBS service
    // area of an existing MBS session". A session that is being released is not restored.
    ogs_list_for_each(&amf_self()->amf_mbs_context_list, mbs_context) {
        if (mbs_context->release_pending || !amf_gnb_takes_mbs_session(gnb, mbs_context))
            continue;

        if (amf_self()->mbs_restoration == AMF_MBS_RESTORATION_BY_AMF)
            amf_mbs_context_restore_in_gnb(mbs_context, gnb);
        else if (amf_self()->mbs_restoration == AMF_MBS_RESTORATION_BY_MBSMF) {
            // TS 23.527 V18.6.0 cl. 8.3.2.3 step 3: "The AMF sends Namf_MBSBroadcast_ContextStatusNotify Request message to
            // the MB-SMF for each affected MBS session including the MBS Session ID, the NG-RAN ID and an indication of
            // the event, either NG-RAN restart, or NG-RAN failure without restart, or a (new) NG-RAN getting into
            // service to serve a TAI that is part of the MBS session service area." The MB-SMF then restores the session
            // with a ContextUpdate that names the NG-RAN in ranIdList (step 6).
            ogs_info("MBS Broadcast [%s]: gNB %i (re)started, the MB-SMF is told", mbs_context->mbs_context_ref, gnb->gnb_id);
            amf_sbi_send_mbs_broadcast_ngran_event(mbs_context, gnb, OpenAPI_ngran_failure_indication_NG_RAN_RESTART_OR_START);
        }
    }
}

/*
 * The release that NG-RAN \p involved asked for is over: when amf.mbs.reestablish_after is set, the session is set up
 * again in it after that time (TS 29.518 V18.14.0 cl. 5.6.2.5: "attempt to re-establish the MBS session after some
 * operator configurable time in these NG-RANs by performing the Broadcast MBS session restoration by AMF procedure
 * specified in clause 8.3.2.2 of TS 23.527"). One timer of the context serves every NG-RAN: it is armed for the
 * earliest time.
 */
static void amf_mbs_context_schedule_reestablish(amf_mbs_context_t *mbs_context, amf_mbs_gnb_t *involved)
{
    ogs_time_t now = ogs_get_monotonic_time(), duration;

    ogs_assert(mbs_context);
    ogs_assert(involved);

    if (!amf_self()->mbs_reestablish)
        return;

    // A timer cannot be armed with no duration; 0 seconds is "at once".
    involved->reestablish_at = now + ogs_time_from_sec(amf_self()->mbs_reestablish_after);
    if (involved->reestablish_at <= now)
        involved->reestablish_at = now + 1;

    duration = amf_mbs_context_earliest_reestablish(mbs_context) - now;
    if (duration < 1)
        duration = 1;
    ogs_timer_start(mbs_context->t_reestablish, duration);
}

/*
 * The time to set the session up again in the NG-RANs that asked for its release has elapsed (event
 * AMF_EVENT_MBS_TIMER, AMF_TIMER_MBS_REESTABLISH): the NG-RANs whose time it is get the session as after a restart
 * (amf_mbs_context_restore_in_gnb()), if they are still connected and the session is not being released. The timer is
 * armed again for the next of the others. The context may have been released since the timer was armed.
 */
void amf_namf_handle_mbs_broadcast_reestablish_timeout(uint32_t mbs_context_index)
{
    amf_mbs_context_t *mbs_context = amf_mbs_context_find_by_index(mbs_context_index);
    amf_mbs_gnb_t *involved = NULL;
    ogs_time_t now = ogs_get_monotonic_time(), next;

    if (!mbs_context) {
        ogs_debug("MBS Broadcast re-establishment: the context has been removed");
        return;
    }

    ogs_list_for_each(&mbs_context->involved_gnb_list, involved) {
        amf_gnb_t *gnb = NULL;

        if (!involved->reestablish_at || involved->reestablish_at > now)
            continue;
        involved->reestablish_at = 0;

        gnb = amf_gnb_find_by_id(involved->gnb_id);
        if (!gnb || mbs_context->release_pending || !involved->release_required || !involved->released ||
                !amf_gnb_takes_mbs_session(gnb, mbs_context)) {
            ogs_debug("MBS Broadcast [%s]: the session is not set up again in gNB %u",
                    mbs_context->mbs_context_ref, (unsigned)involved->gnb_id);
            continue;
        }

        amf_mbs_context_restore_in_gnb(mbs_context, gnb);
    }

    next = amf_mbs_context_earliest_reestablish(mbs_context);
    if (next)
        ogs_timer_start(mbs_context->t_reestablish, next > now ? next - now : 1);
}

/*
 * Sends the NGAP BROADCAST SESSION RELEASE REQUEST for \p mbs_context to the gNBs that have been involved in the
 * session and are still connected (TS 23.247 V18.8.0 cl. 7.3.2 step 4: "The AMF sends an N2 message to all RAN nodes
 * that have been involved to release MBS session."), and returns how many it reached. Used by ContextDelete and when
 * a ContextCreate's consumer is gone before it could be answered.
 */
static int amf_mbs_context_send_release(amf_mbs_context_t *mbs_context)
{
    ogs_pkbuf_t *n2msgreq = NULL;
    amf_mbs_gnb_t *involved = NULL;
    amf_gnb_t *gnb = NULL;
    int gnb_rv = OGS_OK;
    int reached = 0;

    // NGAP BROADCAST SESSION RELEASE REQUEST message
    n2msgreq = ngap_build_broadcast_session_release_request(mbs_context);
    if (!n2msgreq) {
        ogs_error("The Broadcast Session Release Request could not be built");
        return 0;
    }

        // Each gNB gets its own copy and the shared template is freed once after the loop, for the ownership
        // reason given in amf_namf_handle_mbs_broadcast_context_create()'s send loop above.
    ogs_list_for_each(&mbs_context->involved_gnb_list, involved) {
        ogs_pkbuf_t *n2msgreq_copy = NULL;

        gnb = amf_gnb_find_by_id(involved->gnb_id);
        if (!gnb) {
            ogs_debug("A gNB involved in the MBS session has disconnected, no release sent to it");
            continue;
        }
        if (involved->release_required) {
            ogs_debug("gNB %i has asked for the release and has already been sent it", gnb->gnb_id);
            continue;
        }
        if (involved->area_released) {
            ogs_debug("gNB %i left the service area and has already been sent the release", gnb->gnb_id);
            continue;
        }
        if (involved->setup_failed) {
            ogs_debug("gNB %i failed the Setup of the session: it holds nothing to release", gnb->gnb_id);
            continue;
        }

        n2msgreq_copy = ogs_pkbuf_copy(n2msgreq);
        if (!n2msgreq_copy) {
            ogs_error("ogs_pkbuf_copy() failed");
            amf_mbs_context_gnb_not_reached(mbs_context, gnb);
            continue;
        }
        ogs_debug("Sending N2 MBS Session Release to gNB %i", gnb->gnb_id);
        gnb_rv = ngap_send_to_gnb(gnb, n2msgreq_copy, NGAP_NON_UE_SIGNALLING);
        if (gnb_rv != OGS_OK) {
            ogs_error("ngap_send_to_gnb() failed");
            amf_mbs_context_gnb_not_reached(mbs_context, gnb);
            continue;
        }
        involved->release_sent = true;
        reached++;
    }
    ogs_pkbuf_free(n2msgreq);

    return reached;
}

static void amf_mbs_context_release(amf_mbs_context_t *mbs_context)
{
    amf_mbs_context_send_release(mbs_context);
    amf_mbs_context_remove(mbs_context);
}

/*
 * An NG-RAN has answered the release that the ContextRelease sent it (ngap_handle_broadcast_session_release_response()),
 * or it is lost and never will. TS 38.413 V18.8.0 cl. 8.17.3.2: "Upon reception of the BROADCAST SESSION RELEASE RESPONSE
 * message, the AMF shall transfer transparently the Broadcast Session Release Response Transfer IE, if available, to the
 * MB-SMF." The 204 cannot carry it (TS 29.518 cl. 5.6.2.4), so each answer's transfer goes in its own ContextStatusNotify
 * request (cl. 5.6.2.5), with the identity of the NG-RAN. TS 23.247 V18.8.0 cl. 7.3.2 step 7: "The NG-RAN reports successful
 * release of resources for the MBS Session by sending MBS Session Resource Release Response (TMGI) message(s) to the AMF(s)."
 * and step 8: "The AMF sends Namf_MBSBroadcast_ContextRelease response (TMGI) to the MB-SMF." The clause does not say after
 * which answer: the 204 is sent on the first, so that an NG-RAN that never answers cannot keep the consumer waiting, and
 * the context, which the later answers are matched to, is removed after the last. If the consumer's stream is gone, only
 * the notifications and the removal are done.
 */
void amf_namf_send_mbs_broadcast_context_release_response(amf_mbs_context_t *mbs_context,
        amf_gnb_t *gnb, const OCTET_STRING_t *transfer)
{
    ogs_sbi_message_t sendmsg;
    ogs_sbi_stream_t *stream = NULL;
    ogs_sbi_response_t *response = NULL;
    ogs_list_t n2_info_list;
    amf_mbs_n2_info_t *n2_info = NULL;
    amf_mbs_gnb_t *involved = NULL;
    bool first, last;

    ogs_assert(mbs_context);
    ogs_assert(gnb);

    involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);
    if (!involved || !amf_mbs_context_release_answered(mbs_context, involved, &first, &last)) {
        ogs_warn("BROADCAST SESSION RELEASE RESPONSE from gNB %i, which was sent no release by the ContextRelease, ignored",
                gnb->gnb_id);
        return;
    }

    if (transfer && transfer->size > 0)
        n2_info = amf_mbs_n2_info_create(gnb, OpenAPI_ngap_ie_type_MBS_SES_REL_RSP, transfer->buf, transfer->size);
    if (n2_info) {
        ogs_list_init(&n2_info_list);
        ogs_list_add(&n2_info_list, n2_info);
        amf_sbi_send_mbs_broadcast_context_status_notify(
                mbs_context, &n2_info_list, OpenAPI_operation_status_NULL, false, NULL);
        amf_mbs_n2_info_remove_all(&n2_info_list);
    }

    if (first) {
        stream = ogs_sbi_stream_find_by_id(mbs_context->stream_id);
        if (!stream) {
            ogs_warn("MBS Broadcast ContextRelease: stream [%d] no longer exists, response not sent",
                    (int)mbs_context->stream_id);
        } else {
            memset(&sendmsg, 0, sizeof(sendmsg));
            response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
            ogs_assert(response);
            ogs_assert(true == ogs_sbi_server_send_response(stream, response));
        }
        mbs_context->stream_id = OGS_INVALID_POOL_ID;
    }

    if (last)
        amf_mbs_context_remove(mbs_context);
}

/*
 * Sends a ContextStatusNotify request that reports a release that NG-RAN \p gnb asked for: the Broadcast Session
 * Release Response Transfer IE it returned, if any (\p transfer), and releasedInd when \p released_ind.
 */
static void amf_mbs_context_notify_required_release(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb,
        const OCTET_STRING_t *transfer, bool released_ind)
{
    ogs_list_t n2_info_list;
    amf_mbs_n2_info_t *n2_info = NULL;
    bool report;

    if (gnb && transfer && transfer->size > 0)
        n2_info = amf_mbs_n2_info_create(gnb, OpenAPI_ngap_ie_type_MBS_SES_REL_RSP, transfer->buf, transfer->size);

    report = amf_self()->mbs_report_required_release && gnb;

    if (!n2_info && !released_ind && !report)
        return;

    ogs_list_init(&n2_info_list);
    if (n2_info)
        ogs_list_add(&n2_info_list, n2_info);
    amf_sbi_send_mbs_broadcast_context_status_notify(
            mbs_context, &n2_info_list, OpenAPI_operation_status_NULL, released_ind, report ? gnb : NULL);
    amf_mbs_n2_info_remove_all(&n2_info_list);
}

/*
 * True when every NG-RAN serving the session has asked for its release and has been released: each gNB of the record
 * that is still connected and did not fail its Setup (TS 29.518 V18.14.0 cl. 5.6.2.5: "If all the NG-RANs serving the
 * Broadcast MBS session requested the AMF to release the Broadcast MBS session"). False when no gNB serves it.
 */
static bool amf_mbs_context_all_released_on_request(amf_mbs_context_t *mbs_context)
{
    amf_mbs_gnb_t *involved = NULL;
    int serving = 0;

    ogs_list_for_each(&mbs_context->involved_gnb_list, involved) {
        if (!amf_gnb_find_by_id(involved->gnb_id) || involved->setup_failed || involved->area_released)
            continue;
        if (!involved->release_required || !involved->released)
            return false;
        serving++;
    }

    return serving > 0;
}

/*
 * The release asked for by NG-RAN \p gnb is over (it answered, \p transfer is the Broadcast Session Release Response
 * Transfer IE if it sent one, or the request could not be sent): tell the MB-SMF, and when this was the last of all the
 * NG-RANs serving the session, release the context (TS 29.518 V18.14.0 cl. 5.6.2.5: "the AMF shall release the
 * Broadcast MBS session context and send a notification with the releaseInd attribute set to true to report to the
 * MB-SMF that the Broadcast MBS session (context) is released at the AMF and NG-RANs.").
 */
static void amf_mbs_context_required_release_done(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb,
        const OCTET_STRING_t *transfer)
{
    bool all;

    ogs_assert(mbs_context);
    ogs_assert(gnb);

    all = !mbs_context->release_pending && amf_mbs_context_all_released_on_request(mbs_context);

    // TS 23.247 V18.8.0 cl. 7.3.6 step 3: "The AMF notifies the MB-SMF about the DL tunnel release via
    // Namf_MBSBroadcast_ContextStatusNotify with N3mb DL Tunnel Info and corresponding NG-RAN ID." The container is
    // what the NG-RAN returned (TS 38.413 V18.8.0 cl. 8.17.3.2), with the identity of the node.
    amf_mbs_context_notify_required_release(mbs_context, gnb, transfer, all);

    if (!all) {
        amf_mbs_gnb_t *involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);

        if (involved)
            amf_mbs_context_schedule_reestablish(mbs_context, involved);
        return;
    }

    // A ContextCreate or ContextUpdate still waiting for its NG-RAN responses will never be answered by them. No
    // clause names the status (as for the failure of a Setup).
    if (mbs_context->stream_id != OGS_INVALID_POOL_ID) {
        ogs_sbi_stream_t *stream = ogs_sbi_stream_find_by_id(mbs_context->stream_id);

        mbs_context->stream_id = OGS_INVALID_POOL_ID;
        if (stream)
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL,
                    "MBS Broadcast context released",
                    "Every NG-RAN asked for the release of the Broadcast MBS session", NULL);
    }

    amf_mbs_context_remove(mbs_context);
}

/*
 * BROADCAST SESSION RELEASE REQUIRED of NG-RAN \p gnb for \p mbs_context. TS 38.413 V18.8.0 cl. 8.17.4.2: "Upon
 * reception of the BROADCAST SESSION RELEASE REQUIRED message, the AMF shall realize that the NG-RAN node is lacking
 * adequate MBS session resources for a previously established broadcast MBS session and initiate the release of the
 * MBS session resources." TS 23.247 V18.8.0 cl. 7.3.6 step 2: "The AMF initiates Broadcast Session Release towards
 * the NG-RAN as defined in steps 4 - 7 in clause 7.3.2." The release goes to that NG-RAN only; the context goes when
 * all the NG-RANs serving the session have asked (amf_mbs_context_required_release_done()).
 */
void amf_namf_handle_mbs_broadcast_release_required(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb)
{
    amf_mbs_gnb_t *involved = NULL;
    ogs_pkbuf_t *n2msgreq = NULL;

    ogs_assert(mbs_context);
    ogs_assert(gnb);

    involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);
    if (!involved) {
        ogs_warn("BROADCAST SESSION RELEASE REQUIRED from gNB %i, which has not been sent the session, ignored",
                gnb->gnb_id);
        return;
    }
    if (involved->release_required) {
        ogs_warn("BROADCAST SESSION RELEASE REQUIRED from gNB %i, which has asked before, ignored", gnb->gnb_id);
        return;
    }
    if (involved->area_released) {
        ogs_warn("BROADCAST SESSION RELEASE REQUIRED from gNB %i, which left the service area, ignored", gnb->gnb_id);
        return;
    }
    if (mbs_context->release_pending) {
        // The consumer's ContextRelease is releasing the session in every NG-RAN.
        ogs_warn("BROADCAST SESSION RELEASE REQUIRED from gNB %i while the context is being released, ignored",
                gnb->gnb_id);
        return;
    }

    involved->release_required = true;

    n2msgreq = ngap_build_broadcast_session_release_request(mbs_context);
    if (!n2msgreq || ngap_send_to_gnb(gnb, n2msgreq, NGAP_NON_UE_SIGNALLING) != OGS_OK) {
        // No release response will come: the NG-RAN has asked for the release and counts as released.
        ogs_error("The Broadcast Session Release Request could not be sent to gNB %i", gnb->gnb_id);
        amf_mbs_context_gnb_not_reached(mbs_context, gnb);
        involved->released = true;
        amf_mbs_context_required_release_done(mbs_context, gnb, NULL);
    }
}

/*
 * BROADCAST SESSION RELEASE RESPONSE of NG-RAN \p gnb to the release the AMF sent it because it left the service area.
 * Returns false when it is not the answer to such a release. TS 38.413 V18.8.0 cl. 8.17.3.2: "Upon reception of the
 * BROADCAST SESSION RELEASE RESPONSE message, the AMF shall transfer transparently the Broadcast Session Release Response
 * Transfer IE, if available, to the MB-SMF." It goes in a ContextStatusNotify request (TS 29.518 V18.14.0 cl. 5.6.2.5).
 */
bool amf_namf_handle_mbs_broadcast_area_release_response(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb,
        const OCTET_STRING_t *transfer)
{
    amf_mbs_gnb_t *involved = NULL;
    amf_mbs_n2_info_t *n2_info = NULL;
    ogs_list_t n2_info_list;

    ogs_assert(mbs_context);
    ogs_assert(gnb);

    involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);
    if (!involved || !involved->area_release_pending)
        return false;

    involved->area_release_pending = false;

    if (transfer && transfer->size > 0)
        n2_info = amf_mbs_n2_info_create(gnb, OpenAPI_ngap_ie_type_MBS_SES_REL_RSP, transfer->buf, transfer->size);
    if (n2_info) {
        ogs_list_init(&n2_info_list);
        ogs_list_add(&n2_info_list, n2_info);
        amf_sbi_send_mbs_broadcast_context_status_notify(
                mbs_context, &n2_info_list, OpenAPI_operation_status_NULL, false, NULL);
        amf_mbs_n2_info_remove_all(&n2_info_list);
    }

    return true;
}

/*
 * BROADCAST SESSION RELEASE RESPONSE of NG-RAN \p gnb to a release that it asked for. Returns false when it is not
 * the answer to such a release.
 */
bool amf_namf_handle_mbs_broadcast_required_release_response(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb,
        const OCTET_STRING_t *transfer)
{
    amf_mbs_gnb_t *involved = NULL;

    ogs_assert(mbs_context);
    ogs_assert(gnb);

    involved = amf_mbs_context_find_involved_gnb(mbs_context, gnb);
    if (!involved || !involved->release_required || involved->released)
        return false;

    involved->released = true;
    amf_mbs_context_required_release_done(mbs_context, gnb, transfer);
    return true;
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

    if (mbs_context->release_pending) {
        ogs_error("MBS Broadcast ContextDelete: mbsContextRef[%s] is already being released", mbs_context_ref);
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR,
            recvmsg, "Internal Server Error",
            "Requested MBS Broadcast ContextDelete failed, the release is already in progress", "UNSPECIFIED_NF_FAILURE");
        return OGS_ERROR;
    }

    // A ContextCreate or ContextUpdate still waiting for its NG-RAN responses is ended by the release: its stream is
    // answered here, since the Delete takes the stream over below. No clause names the status (as for a failed Setup).
    {
        ogs_sbi_stream_t *pending = ogs_sbi_stream_find_by_id(mbs_context->stream_id);

        mbs_context->stream_id = OGS_INVALID_POOL_ID;
        if (pending)
            ogs_sbi_server_send_error(pending, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL,
                    "MBS Broadcast context released",
                    "The Broadcast MBS session context has been released before the NG-RANs answered", NULL);
    }

    ogs_timer_stop(mbs_context->t_response);
    mbs_context->gnb_request_count = 0;
    mbs_context->gnb_response_count = 0;
    mbs_context->gnb_request_count = amf_mbs_context_send_release(mbs_context);

    if (mbs_context->gnb_request_count > 0) {
        // TS 23.247 V18.8.0 cl. 7.3.2 step 8, "The AMF sends Namf_MBSBroadcast_ContextRelease response (TMGI) to the
        // MB-SMF", follows step 7, the NG-RAN's release response: the 204 waits for the first one.
        mbs_context->release_pending = true;
        mbs_context->stream_id = ogs_sbi_id_from_stream(stream);
        return OGS_OK;
    }

    // No NG-RAN was reached: none holds the session, so there is nothing to wait for.
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
            !amf_namf_tmgi_is_valid(MbsN2MessageTransferReqData->mbs_session_id->tmgi)) {
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

/* The Cause of a Broadcast Transport Failure Transfer (TS 38.413 clause 9.3.5.17: "Cause | M"). */
static bool transport_failure_cause(ogs_pkbuf_t *pkbuf, NGAP_Cause_PR *group, long *cause)
{
    NGAP_BroadcastTransportFailureTransfer_t transfer;
    bool ok = true;

    memset(&transfer, 0, sizeof(transfer));
    if (ogs_asn_decode(&asn_DEF_NGAP_BroadcastTransportFailureTransfer, &transfer, sizeof(transfer), pkbuf) !=
            OGS_OK)
        return false;

    *group = transfer.cause.present;
    switch (transfer.cause.present) {
    case NGAP_Cause_PR_radioNetwork: *cause = transfer.cause.choice.radioNetwork; break;
    case NGAP_Cause_PR_transport: *cause = transfer.cause.choice.transport; break;
    case NGAP_Cause_PR_nas: *cause = transfer.cause.choice.nas; break;
    case NGAP_Cause_PR_protocol: *cause = transfer.cause.choice.protocol; break;
    case NGAP_Cause_PR_misc: *cause = transfer.cause.choice.misc; break;
    default: ok = false; break;
    }
    ogs_asn_free(&asn_DEF_NGAP_BroadcastTransportFailureTransfer, &transfer);
    return ok;
}

int amf_namf_mbs_transport_answers(const amf_gnb_t *gnb, const ogs_tmgi_t *tmgi, ogs_sbi_message_t *message,
        ogs_pkbuf_t **out, int max)
{
    OpenAPI_context_status_notification_response_t *response = NULL;
    OpenAPI_lnode_t *node = NULL;
    int count = 0;

    ogs_assert(gnb);
    ogs_assert(tmgi);
    ogs_assert(message);
    ogs_assert(out);

    response = message->ContextStatusNotificationResponse;
    if (!response || !response->n2_mbs_sm_info_list)
        return 0;

    OpenAPI_list_for_each(response->n2_mbs_sm_info_list, node) {
        OpenAPI_n2_mbs_sm_info_t *info = node->data;
        ogs_pkbuf_t *transfer = NULL, *ngap = NULL;

        if (!info || count >= max)
            continue;
        if (info->ngap_ie_type != OpenAPI_ngap_ie_type_BC_TRA_RSP &&
                info->ngap_ie_type != OpenAPI_ngap_ie_type_BC_TRA_FAIL)
            continue;
        /* TS 29.518 V18.14.0 clause 5.6.2.5: "the MB-SMF shall insert the identifier of the NG-RAN node to which the
         * information in the container relates"; an entry that names another node is not this node's answer. */
        if (info->ran_id && !amf_namf_ran_id_is_gnb(gnb, info->ran_id)) {
            ogs_warn("Broadcast Session Transport: a container of the MB-SMF names another NG-RAN node");
            continue;
        }
        if (info->ngap_data && info->ngap_data->content_id)
            transfer = ogs_sbi_find_part_by_content_id(message, info->ngap_data->content_id);
        if (!transfer) {
            ogs_warn("Broadcast Session Transport: a container of the MB-SMF is not in the message");
            continue;
        }

        if (info->ngap_ie_type == OpenAPI_ngap_ie_type_BC_TRA_RSP) {
            ngap = ngap_build_broadcast_session_transport_response(tmgi, transfer->data, transfer->len);
        } else {
            NGAP_Cause_PR group = NGAP_Cause_PR_NOTHING;
            long cause = 0;

            /* The message carries the Cause beside the transfer (clause 9.2.16.12); it is the one of the transfer. */
            if (!transport_failure_cause(transfer, &group, &cause)) {
                ogs_warn("Broadcast Session Transport: the Failure Transfer of the MB-SMF cannot be decoded");
                continue;
            }
            ngap = ngap_build_broadcast_session_transport_failure(tmgi, transfer->data, transfer->len, group, cause);
        }
        if (ngap)
            out[count++] = ngap;
    }

    return count;
}

void amf_namf_send_transport_failure(amf_gnb_t *gnb, const ogs_tmgi_t *tmgi, NGAP_Cause_PR group, long cause)
{
    ogs_pkbuf_t *transfer = NULL;

    ogs_assert(gnb);
    ogs_assert(tmgi);

    transfer = ngap_build_broadcast_transport_failure_transfer(tmgi, group, cause);
    if (!transfer) {
        ogs_error("ngap_build_broadcast_transport_failure_transfer() failed");
        return;
    }
    ngap_send_broadcast_session_transport_failure(gnb, tmgi, transfer->data, transfer->len, group, cause);
    ogs_pkbuf_free(transfer);
}

void amf_namf_handle_mbs_broadcast_transport_request(amf_gnb_t *gnb, const ogs_tmgi_t *tmgi,
        const uint8_t *transfer, size_t transfer_len)
{
    amf_mbs_context_t *mbs_context = NULL;

    ogs_assert(gnb);
    ogs_assert(tmgi);
    ogs_assert(transfer);

    mbs_context = amf_mbs_context_find_by_tmgi(tmgi);
    if (!mbs_context) {
        /* TS 38.413 V18.8.0 clause 8.17.5.3: "In case the shared NG-U transport cannot be setup successfully, the AMF
         * shall respond with the BROADCAST SESSION TRANSPORT FAILURE message to the NG-RAN node with an appropriate
         * cause value." and clause 9.3.1.2: "Unknown MBS Session ID | The action failed because the MBS Session ID is
         * unknown." */
        ogs_warn("BROADCAST SESSION TRANSPORT REQUEST: no MBS context found for the given TMGI");
        amf_namf_send_transport_failure(gnb, tmgi, NGAP_Cause_PR_radioNetwork,
                NGAP_CauseRadioNetwork_unknown_MBS_Session_ID);
        return;
    }

    if (!amf_sbi_send_mbs_broadcast_transport_request(mbs_context, gnb, transfer, transfer_len))
        amf_namf_send_transport_failure(gnb, tmgi, NGAP_Cause_PR_transport,
                NGAP_CauseTransport_transport_resource_unavailable);
}

