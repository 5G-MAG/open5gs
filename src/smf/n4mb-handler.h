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

#ifndef SMF_N4MB_HANDLER_H
#define SMF_N4MB_HANDLER_H

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * TS 29.500 table 5.2.7.2-1 application-error cause, reused here from the same table
 * nmbsmf-handler.h's NMBSMF_TMGI_* constants already cite in this project.
 *
 * BUG FIX: smf_n4mb_handle_session_establishment_response()'s "No UP F-TEID" path used to send the
 * literal string "Unknown" as the ProblemDetails cause. CONTEXT_NOT_FOUND matches the
 * OGS_PFCP_CAUSE_SESSION_CONTEXT_NOT_FOUND value already returned there. The sibling "PFCP cause not
 * accepted" path had the same defect, independently fixed upstream (see _pfcp_cause_to_problem_cause()
 * in n4mb-handler.c) with a per-cause mapping more precise than a single blanket value -- kept in
 * preference to a second constant here once the two fixes collided in the same rebase.
 */
#define N4MB_CAUSE_CONTEXT_NOT_FOUND   "CONTEXT_NOT_FOUND"

uint8_t smf_n4mb_handle_session_establishment_response(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_establishment_response_t *rsp);

/* The answer to an N4mb Session Modification Request: the Cause decides (TS 29.244 V18.10.0 table 7.5.5.1-1, Cause
 * "M"); a rejection is reported, the MBS session keeps what the MB-SMF decided. */
uint8_t smf_n4mb_handle_session_modification_response(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_modification_response_t *rsp);

uint8_t smf_n4mb_handle_session_deletion_response(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_deletion_response_t *rsp);

/* The answer of the MB-UPF to the N4mb Session Establishment Request of a restoration after an MB-UPF restart (see
 * smf_mbs_sess_restore_n4mb()): takes in the UP F-SEID, the ingress tunnel and the PDRs, and gives the MB-UPF the N3mb
 * DL tunnels that the Create FAR could not hold. Returns the cause of the answer. */
uint8_t smf_n4mb_handle_session_restoration_response(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_establishment_response_t *rsp);

/* Whether a Local Ingress Tunnel IE of an answer is as long as its flags say and names an address (TS 29.244 V18.10.0
 * clause 8.2.209) */
bool smf_n4mb_local_ingress_tunnel_is_valid(const ogs_tlv_octet_t *ie);

/* The LL SSM and C-TEID of a Multicast Transport Information IE of an answer, taken in. Returns false, taking nothing,
 * when the IE is malformed. */
bool smf_n4mb_take_multicast_transport_information(smf_mbs_sess_t *mbs_sess, ogs_tlv_octet_t *octet);

/* A PFCP Session Report Request of the MB-UPF for the MBS session \p mbs_sess (TS 29.244 V18.10.0 clause 5.34.2.3:
 * a Downlink Data Report, report type DLDR, "identifying the PDR(s) for which downlink packets have been received",
 * and "User plane inactivity detection and reporting for an MBS session shall be supported as specified in clause
 * 5.11", report type UPIR). Returns the cause of the Session Report Response; *offending_ie is set with a refusal.
 * Both reports serve the activation and deactivation of a multicast MBS session (clause 5.34.2.4, TS 23.247
 * clauses 7.2.5.2 and 7.2.5.3), which this MB-SMF does not implement; a broadcast session is never given a
 * User Plane Inactivity Timer or a FAR that buffers and notifies, so there is nothing to act on: the report is
 * answered and logged. */
uint8_t smf_n4mb_handle_session_report_request(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_session_report_request_t *req, uint16_t *offending_ie);

/* A PFCP Node Report Request of the MB-UPF (TS 29.244 V18.10.0 clause 6.2.9.3: "process the information being
 * reported as appropriate and send a PFCP Node Report Response with the cause "success"; otherwise return an
 * appropriate error cause value."). The User Plane Path Failure Report is the N3mb path failure of TS 23.527
 * V18.6.0 clause 8.5. Returns the cause of the response; *offending_ie is set with a rejection. */
uint8_t smf_n4mb_handle_node_report_request(
        ogs_pfcp_node_t *node, ogs_pfcp_node_report_request_t *req, uint16_t *offending_ie);

#ifdef __cplusplus
}
#endif

#endif /* SMF_N4MB_HANDLER_H */