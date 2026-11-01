/*
 * 5G-MAG Reference Tools
 * Copyright (C) 2024 iTEAM UPV <borieher@iteam.upv.es>
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

#ifndef SMF_N4MB_BUILD_H
#define SMF_N4MB_BUILD_H

#ifdef  __cplusplus
extern "C" {
#endif

/* The NID field of the MBS Session Identifier IE (TS 29.244 V18.10.0 cl. 8.2.206) from 11 hexadecimal
 * digits. Returns false for anything else. */
bool smf_n4mb_nid_to_octets(const char *nid, uint8_t octets[6]);

ogs_pkbuf_t *smf_n4mb_build_session_establishment_request(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact);

/* The N4mb Session Modification Request that tells the MB-UPF about the change of the QoS flows of an MBS session
 * (TS 29.244 V18.10.0 table 7.5.4.1-1: Create PDR, Create FAR, Create QER for a flow that came; Remove PDR, Remove
 * FAR, Remove QER for one that went) and of its N3mb DL tunnels (table 7.5.4.3-1: Update FAR with Apply Action,
 * Add MBS Unicast Parameters and Remove MBS Unicast Parameters). */
typedef struct smf_n4mb_modification_s {
    const smf_mbs_flow_change_t *flows;         /* Create/Remove PDR, FAR, QER of the flows that came and went */
    const smf_mbs_tunnel_change_t *tunnel;      /* Update FAR: Add and Remove MBS Unicast Parameters */
    bool update_apply_action;                   /* with a tunnel change: the Update FARs carry the Apply Action */
} smf_n4mb_modification_t;

ogs_pkbuf_t *smf_n4mb_build_session_modification_request(
        smf_mbs_sess_t *mbs_sess, const smf_n4mb_modification_t *mod);

ogs_pkbuf_t *smf_n4mb_build_session_deletion_request(
        uint8_t type, smf_mbs_sess_t *mbs_sess);

#ifdef __cplusplus
}
#endif

#endif /* SMF_N4MB_BUILD_H */