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

/*
 * Completes the deferred Namf_MBSBroadcast_ContextCreate response once the first NG-RAN response has been
 * correlated to \p mbs_context. Called from ngap_handle_broadcast_session_setup_response(); see
 * amf_mbs_context_t.stream_id (context.h) for why the response cannot be sent synchronously from
 * amf_namf_handle_mbs_broadcast_context_create() itself.
 */
void amf_namf_send_mbs_broadcast_context_create_response(amf_mbs_context_t *mbs_context);

/* Item B-3: Namf_MBSBroadcast Service - MBS Broadcast ContextUpdate service operation (TS 29.518 cl.5.6.2.3) */
int amf_namf_handle_mbs_broadcast_context_update(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);

/*
 * Completes the deferred Namf_MBSBroadcast_ContextUpdate response once the first NG-RAN response has been
 * correlated to \p mbs_context. Called from ngap_handle_broadcast_session_modification_response(); mirrors
 * amf_namf_send_mbs_broadcast_context_create_response()'s own reasoning exactly.
 */
void amf_namf_send_mbs_broadcast_context_update_response(amf_mbs_context_t *mbs_context);

/* Namf_MBSCommunication Service - N2MessageTransfer service operation (TS 29.518 cl.5.7.2.2) */
int amf_namf_mbs_comm_handle_n2_message_transfer(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);

#ifdef __cplusplus
}
#endif

#endif /* AMF_NAMF_HANDLER_H */
