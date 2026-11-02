/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
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

#ifndef AMF_NAMF_HANDLER_H
#define AMF_NAMF_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "context.h"

int amf_namf_comm_handle_n1_n2_message_transfer(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);
int amf_namf_callback_handle_sm_context_status(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);
int amf_namf_callback_handle_dereg_notify(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);
int amf_namf_callback_handle_sdm_data_change_notify(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);
int amf_namf_comm_handle_ue_context_transfer_request(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);
int amf_namf_comm_handle_ue_context_transfer_response(
        ogs_sbi_message_t *recvmsg, amf_ue_t *amf_ue);
int amf_namf_comm_handle_registration_status_update_request(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);
int amf_namf_comm_handle_registration_status_update_response(
        ogs_sbi_message_t *recvmsg, amf_ue_t *amf_ue);

/* Namf_MBSBroadcast Service API */

/* Application error codes */
// TODO (borieher): Add the error codes from:
// The common application errors defined in the Table 5.2.7.2-1 in 3GPP TS 29.500

/* MBS Broadcast API constants */

/* Namf_MBSBroadcast Service - MBS Broadcast ContextCreate service operation */
int amf_namf_handle_mbs_broadcast_context_create(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);

/* Namf_MBSBroadcast Service - MBS Broadcast ContextDelete service operation */
int amf_namf_handle_mbs_broadcast_context_delete(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);

/* Item B-3: Namf_MBSBroadcast Service - MBS Broadcast ContextUpdate service operation (TS 29.518 cl.5.6.2.3) */
int amf_namf_handle_mbs_broadcast_context_update(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);

/* Whether \p tmgi is present and has an MBS Service ID of six hexadecimal digits and a PLMN of 3 + 2 or 3 digits */
bool amf_namf_tmgi_is_valid(const OpenAPI_tmgi_t *tmgi);

/*
 * What a ContextUpdate does at one NG-RAN node (amf_namf_mbs_update_action()).
 */
typedef enum {
    AMF_MBS_UPDATE_SKIP,    /* nothing is sent to the node */
    AMF_MBS_UPDATE_UNAFFECTED, /* nothing is sent: the node is not affected by the change the update carries */
    AMF_MBS_UPDATE_MODIFY,  /* a Broadcast Session Modification Request */
    AMF_MBS_UPDATE_SETUP,   /* a Broadcast Session Setup Request */
    AMF_MBS_UPDATE_RELEASE, /* a Broadcast Session Release Request: the node left the service area */
} amf_mbs_update_action_e;

/* What a ContextUpdate carries that decides which NG-RAN nodes it goes to */
typedef struct amf_mbs_update_plan_s {
    /* ranIdList: the GlobalRanNodeId items of the request, NULL when it carries none */
    OpenAPI_list_t *ran_id_list;
    /* noNgapSignallingInd is true: no NGAP message goes to any NG-RAN */
    bool no_ngap_signalling;
    /* n2MbsInfoChangeInd is present and false: the container has not changed, so only the NG-RAN nodes affected by
     * the change of the service area are contacted */
    bool n2_info_unchanged;
    /* the service area of the context before this update (the same object as the context's when the update carries
     * none) */
    const ogs_mbs_service_area_t *old_area;
} amf_mbs_update_plan_t;

/* The action of a ContextUpdate of \p mbs_context (whose service area is already the updated one) at \p gnb */
amf_mbs_update_action_e amf_namf_mbs_update_action(
        amf_mbs_context_t *mbs_context, amf_gnb_t *gnb, const amf_mbs_update_plan_t *plan);

/*
 * Handles an NG-RAN node's response to a Broadcast Session Setup or Modification Request (success or failure, with
 * the Response or Failure Transfer IE if it carried one): completes the deferred ContextCreate or ContextUpdate
 * response or sends a ContextStatusNotify request. See the function's own comment (namf-handler.c).
 */
void amf_namf_handle_mbs_broadcast_release_required(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb);

/*
 * The Broadcast Session Transport procedure (TS 38.413 V18.8.0 clause 8.17.5) of NG-RAN node \p gnb for the MBS session
 * \p tmgi: relays the Broadcast Transport Request Transfer \p transfer to the MB-SMF in a ContextStatusNotify, and
 * answers the NG-RAN node with the BROADCAST SESSION TRANSPORT FAILURE when there is no such context or the notification
 * cannot be sent. The answer of the MB-SMF is handled by amf_namf_mbs_transport_answers().
 */
void amf_namf_handle_mbs_broadcast_transport_request(amf_gnb_t *gnb, const ogs_tmgi_t *tmgi,
        const uint8_t *transfer, size_t transfer_len);

/* Answers the NG-RAN node with the BROADCAST SESSION TRANSPORT FAILURE, the Failure Transfer being the AMF's own */
void amf_namf_send_transport_failure(amf_gnb_t *gnb, const ogs_tmgi_t *tmgi, NGAP_Cause_PR group, long cause);

/*
 * The NGAP messages that answer a ContextStatusNotify with a Broadcast Session Transport Request Transfer, from the
 * ContextStatusNotificationResponse of \p message (TS 29.518 V18.14.0 clause 5.6.2.5 step 2a): one BROADCAST SESSION
 * TRANSPORT RESPONSE or FAILURE for each container whose ranId names \p gnb (or has none). Returns how many were built
 * into \p out (owned by the caller); 0 when the response carries no usable container.
 */
int amf_namf_mbs_transport_answers(const amf_gnb_t *gnb, const ogs_tmgi_t *tmgi, ogs_sbi_message_t *message,
        ogs_pkbuf_t **out, int max);
/*
 * The NG-RAN node \p gnb has completed NG Setup (it restarted, or it is new): the Broadcast MBS sessions whose service
 * area it serves are restored in it as amf.mbs.restoration says (TS 23.527 V18.6.0 cl. 8.3.2). Does nothing when the
 * option is not set.
 */
void amf_namf_handle_mbs_broadcast_ng_setup(amf_gnb_t *gnb);

/*
 * The association with NG-RAN node \p gnb is lost (it is about to be removed): the answers it still owes to a ContextCreate
 * or ContextUpdate are taken as refusals, and when amf.mbs.report_ng_ran_failure is set the MB-SMF is told for each
 * Broadcast MBS session the node served (TS 23.527 V18.6.0 cl. 8.3.3).
 */
void amf_namf_handle_mbs_broadcast_gnb_lost(amf_gnb_t *gnb);

bool amf_namf_handle_mbs_broadcast_area_release_response(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb,
        const OCTET_STRING_t *transfer);
bool amf_namf_handle_mbs_broadcast_required_release_response(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb,
        const OCTET_STRING_t *transfer);
void amf_namf_handle_mbs_broadcast_ran_response(amf_mbs_context_t *mbs_context, amf_gnb_t *gnb,
        bool update, bool success, OpenAPI_ngap_ie_type_e ngap_ie_type, const OCTET_STRING_t *transfer);
/* The maximum response time of a ContextCreate or ContextUpdate has elapsed (AMF_EVENT_MBS_TIMER) */
void amf_namf_handle_mbs_broadcast_response_timeout(uint32_t mbs_context_index);
void amf_namf_handle_mbs_broadcast_reestablish_timeout(uint32_t mbs_context_index);
void amf_namf_send_mbs_broadcast_context_release_response(amf_mbs_context_t *mbs_context,
        amf_gnb_t *gnb, const OCTET_STRING_t *transfer);

/* Namf_MBSCommunication Service - N2MessageTransfer service operation (TS 29.518 cl.5.7.2.2) */
int amf_namf_mbs_comm_handle_n2_message_transfer(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);

#ifdef __cplusplus
}
#endif

#endif /* AMF_NAMF_HANDLER_H */
