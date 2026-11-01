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

#ifndef SMF_NGAP_BUILD_H
#define SMF_NGAP_BUILD_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

ogs_pkbuf_t *ngap_build_pdu_session_resource_setup_request_transfer(
        smf_sess_t *sess);

ogs_pkbuf_t *ngap_build_pdu_session_resource_modify_request_transfer(
        smf_sess_t *sess, bool qos_presence);
ogs_pkbuf_t *ngap_build_pdu_session_resource_release_request_transfer(
        smf_sess_t *sess, NGAP_Cause_PR group, long cause);

ogs_pkbuf_t *ngap_build_pdu_session_resource_release_command_transfer(
        smf_sess_t *sess, int state, NGAP_Cause_PR group, long cause);

ogs_pkbuf_t *ngap_build_path_switch_request_ack_transfer(smf_sess_t *sess);

ogs_pkbuf_t *ngap_build_handover_command_transfer(smf_sess_t *sess);

ogs_pkbuf_t *ngap_build_mbs_session_setup_or_modification_request_transfer(
        smf_mbs_sess_t *mbs_sess);

/* The same container of a Broadcast Session Modification, with the MBS NG-U Failure Indication of an N3mb path
 * failure when \p n3mb_path_failure is set. */
ogs_pkbuf_t *ngap_build_mbs_session_modification_request_transfer(smf_mbs_sess_t *mbs_sess, bool n3mb_path_failure);

/* The Broadcast Transport Response Transfer (TS 38.413 clause 9.3.5.16) and Failure Transfer (clause 9.3.5.17) for
 * the MBS session of \p tmgi; the Failure Transfer carries \p cause_value of the Cause group \p cause_group. */
ogs_pkbuf_t *ngap_build_broadcast_transport_response_transfer(const ogs_tmgi_t *tmgi);
ogs_pkbuf_t *ngap_build_broadcast_transport_failure_transfer(const ogs_tmgi_t *tmgi, NGAP_Cause_PR cause_group,
        long cause_value);

#ifdef __cplusplus
}
#endif

#endif /* SMF_NGAP_BUILD_H */
