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
        amf_mbs_context_t *mbs_context, ogs_pkbuf_t *n2mbssmbuf, bool completed);

#ifdef __cplusplus
}
#endif

#endif /* AMF_NAMF_BUILD_H */
