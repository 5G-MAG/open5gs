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

#ifndef SMF_NAMF_HANDLER_H
#define SMF_NAMF_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "context.h"
#include "mbs-n3mb.h"

bool smf_namf_comm_handle_n1_n2_message_transfer(
        smf_sess_t *sess, int state, ogs_sbi_message_t *recvmsg);

bool smf_namf_comm_handle_n1_n2_message_transfer_failure_notify(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);

/* Namf_MBSBroadcast Service API */

bool smf_namf_handle_mbs_broadcast_context_create_response(
        smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, ogs_sbi_message_t *recvmsg);

bool smf_namf_handle_mbs_broadcast_context_update_response(
        smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, ogs_sbi_message_t *recvmsg);

bool smf_namf_handle_mbs_broadcast_context_delete_response(
        smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, ogs_sbi_message_t *recvmsg);

bool smf_namf_handle_mbs_broadcast_context_status_notify(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg);

/* The 200 OK of a ContextStatusNotify that carried Broadcast Session Transport Request Transfer containers: one
 * answer of \p answers for each, \p count of them (1..SMF_MBS_MAX_TRANSPORT_ANSWERS). */
ogs_sbi_response_t *smf_namf_build_transport_answers_response(
        OpenAPI_context_status_notification_t *notification, const smf_mbs_transport_answer_t *answers, int count);

#ifdef __cplusplus
}
#endif

#endif /* SMF_NAMF_HANDLER_H */
