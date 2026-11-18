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

#ifndef UPF_N4MB_HANDLER_H
#define UPF_N4MB_HANDLER_H

#ifdef  __cplusplus
extern "C" {
#endif

bool upf_n4mb_check_session_establishment_request(
        ogs_pfcp_xact_t *xact, ogs_pfcp_session_establishment_request_t *req);

void upf_n4mb_handle_session_establishment_request(
        upf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_establishment_request_t *req);

void upf_n4mb_handle_session_modification_request(
        upf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_modification_request_t *req);

/* Reads one datagram of a session's UDP tunnel into buf, which holds mtu octets. Returns the number of octets of a
 * datagram that fits; 0 when there was nothing to read or the read failed; -1 when the datagram was longer than mtu,
 * which is refused (not cut): it is counted in *oversize, and logged the first time and then every thousandth. */
ssize_t upf_mbs_tunnel_recv(ogs_socket_t fd, void *buf, unsigned int mtu, unsigned int *oversize);

/* Whether the Apply Action of a FAR says to drop the packets (TS 29.244 V19.6.0 clause 8.2.26: "Bit 1 - DROP (Drop): when set
 * to "1", this indicates a request to drop the packets.") */
bool upf_mbs_far_drops_packets(const ogs_pfcp_far_t *far);

void upf_n4mb_handle_session_deletion_request(
        upf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_deletion_request_t *req);

#ifdef __cplusplus
}
#endif

#endif /* UPF_N4MB_HANDLER_H */