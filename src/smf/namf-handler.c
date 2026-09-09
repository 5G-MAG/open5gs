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

#include "sbi-path.h"
#include "ngap-path.h"
#include "binding.h"
#include "namf-handler.h"

bool smf_namf_comm_handle_n1_n2_message_transfer(
        smf_sess_t *sess, int state, ogs_sbi_message_t *recvmsg)
{
    smf_ue_t *smf_ue = NULL;
    OpenAPI_n1_n2_message_transfer_rsp_data_t *N1N2MessageTransferRspData;

    ogs_assert(sess);
    smf_ue = smf_ue_find_by_id(sess->smf_ue_id);
    ogs_assert(smf_ue);
    ogs_assert(state);
    ogs_assert(recvmsg);

    switch (state) {
    case SMF_UE_REQUESTED_PDU_SESSION_ESTABLISHMENT:
        if (recvmsg->res_status == OGS_SBI_HTTP_STATUS_OK) {
            smf_qos_flow_binding(sess);
        } else {
            ogs_error("[%s:%d] HTTP response error [%d]",
                smf_ue->supi, sess->psi, recvmsg->res_status);
        }
        break;

    case SMF_NETWORK_TRIGGERED_SERVICE_REQUEST:
    case SMF_NETWORK_REQUESTED_QOS_FLOW_MODIFICATION:
        N1N2MessageTransferRspData = recvmsg->N1N2MessageTransferRspData;
        if (!N1N2MessageTransferRspData) {
            ogs_error("No N1N2MessageTransferRspData [status:%d]",
                    recvmsg->res_status);
            break;
        }

        if (recvmsg->res_status == OGS_SBI_HTTP_STATUS_OK) {
            if (N1N2MessageTransferRspData->cause ==
                OpenAPI_n1_n2_message_transfer_cause_N1_N2_TRANSFER_INITIATED) {
                /* Nothing */
            } else {
                ogs_error("Not implemented [cause:%d]",
                        N1N2MessageTransferRspData->cause);
                ogs_assert_if_reached();
            }
        } else if (recvmsg->res_status == OGS_SBI_HTTP_STATUS_ACCEPTED) {
            if (N1N2MessageTransferRspData->cause ==
                OpenAPI_n1_n2_message_transfer_cause_ATTEMPTING_TO_REACH_UE) {
                if (recvmsg->http.location)
                    smf_sess_set_paging_n1n2message_location(
                            sess, recvmsg->http.location);
                else
                    ogs_error("No HTTP Location");
            } else {
                ogs_error("Not implemented [cause:%d]",
                        N1N2MessageTransferRspData->cause);
                ogs_assert_if_reached();
            }
        } else {

    /*
     * TODO:
     *
     * TS23.502 4.2.3.3 Network Triggered Service Request
     *
     * 3c. [Conditional] SMF responds to the UPF
     *
     * If the SMF receives an indication from the AMF that the UE is
     * unreachable or reachable only for regulatory prioritized service
     * and the SMF determines that Extended Buffering does not apply,
     * the SMF may, based on network policies, either:
     *
     * - indicate to the UPF to stop sending Data Notifications;
     * - indicate to the UPF to stop buffering DL data and
     *   discard the buffered data;
     * - indicate to the UPF to stop sending Data Notifications and
     *   stop buffering DL data and discard the buffered data; or
     * - refrains from sending further Namf_Communication_N1N2MessageTransfer
     *   message for DL data to the AMF while the UE is unreachable.
     */

            ogs_error("[%s:%d] HTTP response error [status:%d cause:%d]",
                smf_ue->supi, sess->psi, recvmsg->res_status,
                N1N2MessageTransferRspData->cause);
        }
        break;

    case SMF_NETWORK_REQUESTED_PDU_SESSION_RELEASE:
    case SMF_ERROR_INDICATON_RECEIVED_FROM_5G_AN:

        N1N2MessageTransferRspData = recvmsg->N1N2MessageTransferRspData;
        if (!N1N2MessageTransferRspData) {
            ogs_error("No N1N2MessageTransferRspData [status:%d]",
                    recvmsg->res_status);
            break;
        }

        if (recvmsg->res_status == OGS_SBI_HTTP_STATUS_ACCEPTED) {
    /*
     * OpenAPI_n1_n2_message_transfer_cause_ATTEMPTING_TO_REACH_UE and
     * HTTP_STATUS_ACCEPTED should be handled here when removing PDU session
     * due to the change of PDU Session Anchor.
     *
     * TS23.502
     * 4.3.4 PDU Session Release
     * 4.3.4.2 UE or network requested PDU Session Release for Non-Roaming
     * and Roaming with Local Breakout
     *
     * 3b. ...
     *
     * The "skip indicator" tells the AMF whether it may skip sending
     * the N1 SM container to the UE (e.g. when the UE is in CM-IDLE state).
     * SMF includes the "skip indicator"
     * in the Namf_Communication_N1N2MessageTransfer
     * except when the procedure is triggered to change PDU Session Anchor
     * of a PDU Session with SSC mode 2.
     *
     * Related Issue #2396
     */
            if (N1N2MessageTransferRspData->cause ==
                OpenAPI_n1_n2_message_transfer_cause_ATTEMPTING_TO_REACH_UE) {
                /* Nothing */
            } else {
                ogs_error("Not implemented [cause:%d]",
                        N1N2MessageTransferRspData->cause);
                ogs_assert_if_reached();
            }
        } else if (recvmsg->res_status == OGS_SBI_HTTP_STATUS_OK) {
            if (N1N2MessageTransferRspData->cause ==
                OpenAPI_n1_n2_message_transfer_cause_N1_MSG_NOT_TRANSFERRED) {
                smf_n1_n2_message_transfer_param_t param;

                memset(&param, 0, sizeof(param));
                param.state = SMF_NETWORK_TRIGGERED_SERVICE_REQUEST;
                param.n2smbuf =
                    ngap_build_pdu_session_resource_setup_request_transfer(
                            sess);
                ogs_assert(param.n2smbuf);

                param.n1n2_failure_txf_notif_uri = true;

                smf_namf_comm_send_n1_n2_message_transfer(sess, &param);
            } else if (N1N2MessageTransferRspData->cause ==
                OpenAPI_n1_n2_message_transfer_cause_N1_N2_TRANSFER_INITIATED) {
                /* Nothing */
            } else {
                ogs_error("Not implemented [cause:%d]",
                        N1N2MessageTransferRspData->cause);
                ogs_assert_if_reached();
            }
        } else {
            ogs_error("[%s:%d] HTTP response error [status:%d cause:%d]",
                smf_ue->supi, sess->psi, recvmsg->res_status,
                N1N2MessageTransferRspData->cause);
        }
        break;

    default:
        ogs_fatal("Unexpected state [%d]", state);
        ogs_assert_if_reached();
    }

    return true;
}

bool smf_namf_comm_handle_n1_n2_message_transfer_failure_notify(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    OpenAPI_n1_n2_msg_txfr_failure_notification_t
        *N1N2MsgTxfrFailureNotification = NULL;

    smf_sess_t *sess = NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    N1N2MsgTxfrFailureNotification = recvmsg->N1N2MsgTxfrFailureNotification;
    if (!N1N2MsgTxfrFailureNotification) {
        ogs_error("No N1N2MsgTxfrFailureNotification");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "No N1N2MsgTxfrFailureNotification", NULL, NULL));
        return false;
    }

    if (!N1N2MsgTxfrFailureNotification->cause) {
        ogs_error("No Cause");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "No Cause", NULL, NULL));
        return false;
    }

    if (!N1N2MsgTxfrFailureNotification->n1n2_msg_data_uri) {
        ogs_error("No n1n2MsgDataUri");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "No n1n2MsgDataUri", NULL, NULL));
        return false;
    }

    sess = smf_sess_find_by_paging_n1n2message_location(
        N1N2MsgTxfrFailureNotification->n1n2_msg_data_uri);
    if (!sess) {
        ogs_error("Not found");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
                recvmsg, N1N2MsgTxfrFailureNotification->n1n2_msg_data_uri,
                NULL, NULL));
        return false;
    }

    /*
     * TODO:
     *
     * TS23.502 4.2.3.3 Network Triggered Service Request
     *
     * 5. [Conditional] AMF to SMF:
     * Namf_Communication_N1N2Transfer Failure Notification.
     *
     * When a Namf_Communication_N1N2Transfer Failure Notification
     * is received, SMF informs the UPF (if applicable).
     *
     * Procedure for pause of charging at SMF is specified in clause 4.4.4.
     */

    ogs_assert(true == ogs_sbi_send_http_status_no_content(stream));
    return true;
}

/* Namf_MBSBroadcast Service API */

bool smf_namf_handle_mbs_broadcast_context_create_response(
        smf_mbs_sess_t *mbs_sess, ogs_sbi_message_t *recvmsg)
{
    ogs_debug("Handling MBS Broadcast ContextCreate response");

    ogs_sbi_message_t location_message;
    ogs_sbi_header_t header;
    int rv;

    ogs_assert(mbs_sess);
    ogs_assert(recvmsg);

        // Stores the AMF-assigned mbsContextRef from the Location header of the 201 Created response
        // (TS 29.518 cl.5.6.2.2). Without it there is no way to address this context later for release
        // (DELETE /namf-mbs-bc/v1/mbs-contexts/{mbsContextRef}, TS 29.518 cl.5.6.2.4).
    if (!recvmsg->http.location) {
        ogs_error("MBS Broadcast ContextCreate response: No http.location, cannot capture mbsContextRef");
        return false;
    }

    memset(&header, 0, sizeof(header));
    header.uri = recvmsg->http.location;

    rv = ogs_sbi_parse_header(&location_message, &header);
    if (rv != OGS_OK) {
        ogs_error("MBS Broadcast ContextCreate response: Cannot parse http.location [%s]",
                recvmsg->http.location);
        return false;
    }

    if (!location_message.h.resource.component[1]) {
        ogs_sbi_header_free(&header);
        ogs_error("MBS Broadcast ContextCreate response: No mbsContextRef in Location [%s]",
                recvmsg->http.location);
        return false;
    }

    if (mbs_sess->mbs_context_ref)
        ogs_free(mbs_sess->mbs_context_ref);
    mbs_sess->mbs_context_ref = ogs_strdup(location_message.h.resource.component[1]);
    ogs_assert(mbs_sess->mbs_context_ref);

    ogs_sbi_header_free(&header);

    ogs_info("MBS Broadcast ContextCreate: mbsContextRef[%s]", mbs_sess->mbs_context_ref);

    return true;
}

bool smf_namf_handle_mbs_broadcast_context_delete_response(
        smf_mbs_sess_t *mbs_sess, ogs_sbi_message_t *recvmsg)
{
    ogs_assert(mbs_sess);
    ogs_assert(recvmsg);

    if (recvmsg->res_status != OGS_SBI_HTTP_STATUS_NO_CONTENT) {
        ogs_error("MBS Broadcast ContextDelete: unexpected HTTP response [%d] for mbsContextRef[%s]",
                recvmsg->res_status, mbs_sess->mbs_context_ref);
        return false;
    }

    ogs_info("MBS Broadcast ContextDelete: mbsContextRef[%s] released", mbs_sess->mbs_context_ref);

    return true;
}

/*
 * BUG FIX: this receiving-side handler and its own callback resource
 * (OGS_SBI_RESOURCE_NAME_MBS_CONTEXT_STATUS_NOTIFY, smf-sm.c's own dispatch case) did not previously exist
 * at all -- the SMF sends a real notifyUri when it calls ContextCreate (namf-build.c), but nothing in this
 * SMF was listening at any URI for the AMF's own Namf_MBSBroadcast_ContextStatusNotify callback (TS 29.518
 * cl.5.6.2.5), so every such notification the AMF ever sent would have hit the generic
 * unrecognised-resource path and been rejected. Mirrors smf_namf_comm_handle_n1_n2_message_transfer_failure_
 * notify()'s own shape (a flat NSMF_CALLBACK-style resource, correlated by body content rather than a URL
 * path parameter, acknowledged with 204 No Content) and ogs_nnrf_nfm_handle_nf_status_notify()'s own
 * mandatory-IE-then-204 pattern.
 *
 * Scope: parses and logs everything TS 29.518 cl.5.6.2.5 defines for this notification (MBS Session ID,
 * Area Session ID, N2 MBS SM Info presence, operationStatus, operationEvents, releasedInd) and correlates
 * it to the right smf_mbs_sess_t by TMGI. Does not yet act on operationStatus/releasedInd beyond logging:
 * this SMF has no established behaviour anywhere else for "an MBS session is now known complete/released
 * from a second notification path" to extend (ContextCreate's own response already reports completion for
 * the common single-gNB case), so inventing one here would be new policy without a clause requiring it --
 * recorded as a follow-up, not attempted in this pass (rule 12).
 */
bool smf_namf_handle_mbs_broadcast_context_status_notify(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    OpenAPI_context_status_notification_t *ContextStatusNotification = NULL;
    OpenAPI_mbs_session_id_t *MbsSessionId = NULL;
    ogs_tmgi_t tmgi;
    smf_mbs_sess_t *mbs_sess = NULL;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    ContextStatusNotification = recvmsg->ContextStatusNotification;
    if (!ContextStatusNotification) {
        ogs_error("MBS Broadcast ContextStatusNotify: No ContextStatusNotification");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "No ContextStatusNotification", NULL, NULL));
        return false;
    }

    MbsSessionId = ContextStatusNotification->mbs_session_id;
    if (!MbsSessionId || !MbsSessionId->tmgi) {
        ogs_error("MBS Broadcast ContextStatusNotify: No mbsSessionId.tmgi");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "No mbsSessionId.tmgi", NULL, NULL));
        return false;
    }

    memset(&tmgi, 0, sizeof(tmgi));
    ogs_sbi_parse_tmgi(&tmgi, MbsSessionId->tmgi);

    mbs_sess = smf_mbs_sess_find_by_tmgi(&tmgi);
    if (!mbs_sess) {
        ogs_warn("MBS Broadcast ContextStatusNotify: no MBS session found for the given TMGI");
        /* TS 29.518 defines no specific error cause for this case; a genuine race (e.g. very late
         * notification after this SMF already released the session locally) is not itself malformed. */
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
                recvmsg, "No MBS session found for the given TMGI", NULL, NULL));
        return false;
    }

    ogs_info("MBS Broadcast ContextStatusNotify: mbsContextRef[%s] "
            "operationStatus[%s] areaSessionId%s n2MbsSmInfo[%s] releasedInd[%s]",
            mbs_sess->mbs_context_ref ? mbs_sess->mbs_context_ref : "(none)",
            OpenAPI_operation_status_ToString(ContextStatusNotification->operation_status),
            ContextStatusNotification->is_area_session_id ? "[present]" : "[absent]",
            (ContextStatusNotification->n2_mbs_sm_info_list &&
             ContextStatusNotification->n2_mbs_sm_info_list->count > 0) ? "present" : "absent",
            ContextStatusNotification->released_ind ==
                    OpenAPI_context_status_notification_RELEASEDIND__true ? "true" : "false");

    response = ogs_sbi_build_response(recvmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return true;
}
