/*
 * Copyright (C) 2019-2025 by Sukchan Lee <acetcom@gmail.com>
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

#ifndef UPF_QOS_MONITORING_H
#define UPF_QOS_MONITORING_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Per QoS flow QoS monitoring of the packet delay (TS 29.244 V19.6.0 clauses 5.24.4 and 5.39) */

/* An SRR with QoS monitoring control was created or updated: (re)start the measurement of its QoS flow */
void upf_qos_monitoring_configure(upf_sess_t *sess, ogs_pfcp_srr_t *srr);
/* An SRR was removed: stop the measurement and report what was detected */
void upf_qos_monitoring_stop(upf_sess_t *sess, uint8_t srr_id);
/* The session goes away */
void upf_qos_monitoring_remove_all(upf_sess_t *sess);
/* A packet of the NG-RAN with the QMP field set in the UL PDU SESSION INFORMATION frame (t4: when it was received) */
void upf_qos_monitoring_uplink(upf_sess_t *sess, ogs_gtp2_header_desc_t *header_desc, ogs_time_t t4);

#ifdef __cplusplus
}
#endif

#endif /* UPF_QOS_MONITORING_H */
