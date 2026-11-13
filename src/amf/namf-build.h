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

#ifndef AMF_NAMF_BUILD_H
#define AMF_NAMF_BUILD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "context.h"

ogs_sbi_request_t *amf_namf_comm_build_ue_context_transfer(
        amf_ue_t *amf_ue, void *data);
ogs_sbi_request_t *amf_namf_comm_build_registration_status_update(
        amf_ue_t *amf_ue, void *data);

/* Namf_MBSBroadcast Service - shared helpers */
OpenAPI_mbs_session_id_t *amf_namf_build_mbs_session_id(const ogs_tmgi_t *tmgi);

/* Namf_MBSBroadcast Service - ContextStatusNotify service operation (TS 29.518 cl.5.6.2.5) */
ogs_sbi_request_t *amf_namf_build_mbs_broadcast_context_status_notify(
        amf_mbs_context_t *mbs_context, ogs_list_t *n2_info_list, OpenAPI_operation_status_e operation_status,
        bool released_ind, const amf_gnb_t *required_release_gnb);

/* The same, reporting the NG-RAN \p ngran_gnb as \p indication (NG_RAN_RESTART_OR_START, ...) in the operation event */
ogs_sbi_request_t *amf_namf_build_mbs_broadcast_context_status_notify_event(
        amf_mbs_context_t *mbs_context, ogs_list_t *n2_info_list, OpenAPI_operation_status_e operation_status,
        bool released_ind, const amf_gnb_t *ngran_gnb, OpenAPI_ngran_failure_indication_e indication);

/* Whether the n2MbsSmInfo entry's ranId \p ran_id names \p gnb (the same GlobalRanNodeId the AMF builds for it) */
bool amf_namf_ran_id_is_gnb(const amf_gnb_t *gnb, OpenAPI_global_ran_node_id_t *ran_id);

/* Whether one of the GlobalRanNodeId items of \p ran_id_list names \p gnb */
bool amf_namf_ran_id_list_has_gnb(const OpenAPI_list_t *ran_id_list, const amf_gnb_t *gnb);

/* n2MbsSmInfoList of a Namf_MBSBroadcast response or notification, with its binary parts added to \p message */
OpenAPI_list_t *amf_namf_build_n2_mbs_sm_info_list(ogs_sbi_message_t *message, ogs_list_t *n2_info_list);
void amf_namf_free_n2_mbs_sm_info_list(OpenAPI_list_t *list);

#ifdef __cplusplus
}
#endif

#endif /* AMF_NAMF_BUILD_H */
