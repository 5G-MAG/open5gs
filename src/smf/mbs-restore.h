/*
 * 5G-MAG Reference Tools
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

#ifndef SMF_MBS_RESTORE_H
#define SMF_MBS_RESTORE_H

#include "context.h"
#include "namf-build.h"

#ifdef __cplusplus
extern "C" {
#endif

/* What the MB-SMF does when the AMF reports an NG-RAN event of a Broadcast MBS session in the operationEvents of a
 * Namf_MBSBroadcast_ContextStatusNotify (TS 29.518 V18.14.0 table 6.5.6.2.8-1, opEventType "NG_RAN_EVENT", with the
 * ngranFailureEventList of table 6.5.6.2.9-1), the Broadcast MBS session restoration of TS 23.527 V18.6.0 clauses
 * 8.3.2.3 and 8.3.3. */

/* The NG-RAN node an event is about, when the session is to be started in it again with a Namf_MBSBroadcast
 * ContextUpdate carrying its NG-RAN ID in the ranIdList. */
typedef struct smf_mbs_ngran_restore_s {
    bool restore;
    bool restore_later;     /* after the operator's time (smf_mbs_sess_schedule_ngran_restoration()) */
    char *ran_id;       /* GlobalRanNodeId as JSON text, owned by the caller (ogs_free) once restore is set */
} smf_mbs_ngran_restore_t;

/* Takes in one ngranFailureEvent:
 *  - NG_RAN_RESTART_OR_START (cl. 8.3.2.3 steps 5 and 6): the NG-RAN DL tunnel is taken away (recorded in
 *    \p changes, for smf_mbs_sess_send_tunnel_changes()), and the session is to be restored in that NG-RAN;
 *  - NG_RAN_FAILURE_WITHOUT_RESTART (cl. 8.3.3 step 5): the NG-RAN DL tunnel is taken away, nothing else;
 *  - NG_RAN_REQUIRED_RELEASE (TS 29.518 cl. 5.6.2.5): the session may be restored in that NG-RAN after the operator's
 *    time (restore_later, ran_id set, owned by the caller);
 *  - the other indications ask nothing of the MB-SMF in the clauses read.
 * \p changes is appended to (it is not cleared). Returns the number of DL tunnels taken away. */
int smf_mbs_sess_take_ngran_failure_event(smf_mbs_sess_t *mbs_sess, const OpenAPI_ngran_failure_event_t *event,
        smf_mbs_tunnel_changes_t *changes, smf_mbs_ngran_restore_t *restore);

/* The restoration of a broadcast MBS session in an NG-RAN that required its release, waiting for the operator's time
 * (smf.ngranRestoreDelay). TS 29.518 V18.14.0 clause 5.6.2.5: the AMF may report the release in an "NG_RAN_EVENT" with
 * the ngranFailureEvent of each NG-RAN that requested it, and "Upon receiving such a notification, per local policies,
 * the MB-SMF may attempt to re-establish the MBS session after some operator configurable time in these NG-RANs by
 * performing the Broadcast MBS session restoration by MB-SMF procedure specified in clause 8.3.2.3 of TS 23.527". */
typedef struct smf_mbs_ngran_restoration_s {
    ogs_lnode_t lnode;
    ogs_timer_t *timer;
    ogs_pool_id_t mbs_sess_id;
    uint32_t amf_id;            /* the entry of the AMF that reported the release */
    char *ran_id;               /* GlobalRanNodeId as JSON text */
} smf_mbs_ngran_restoration_t;

/* Waits the configured time before the NG-RAN \p ran_id gets the session again through \p amf. Returns 1 when it is
 * scheduled, 0 when it is not: no time is configured, or the same restoration is already waiting. */
int smf_mbs_sess_schedule_ngran_restoration(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, const char *ran_id);

/* The time has elapsed (the event SMF_EVT_MBS_NGRAN_RESTORE): the session is restored in the NG-RAN if it and the
 * AMF's context are still there. Frees \p restoration. */
void smf_mbs_ngran_restoration_expired(smf_mbs_ngran_restoration_t *restoration);

/* The restoration after an AMF failure, TS 23.527 V18.6.0 cl. 8.3.2.4 ("Selecting an alternative AMF for a Broadcast MBS
 * Session at AMF failure"): an AMF that restarted holds none of the broadcast MBS session contexts it had. The MB-SMF
 * finds out when an AMF registers again (smf_mbs_probe_amf_contexts(): a ContextUpdate that changes nothing, to every
 * context it holds, which an AMF without the context refuses with 404) or when any ContextUpdate is refused that way
 * (smf_mbs_amf_context_lost()), and then creates the context again from the data of the session. */

/* The AMF no longer holds the context \p amf of the session: the entry is dropped and the context is created again
 * (a Namf_MBSBroadcast ContextCreate) unless the session has another entry. Returns OGS_OK when the Create is sent
 * or not needed, OGS_ERROR when it could not be sent. */
int smf_mbs_amf_context_lost(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf);

/* An AMF registered at the NRF: each context the MB-SMF holds is checked with a ContextUpdate that changes nothing.
 * Returns the number of checks sent. */
int smf_mbs_probe_amf_contexts(void);

/* The ContextCreate sender, replaceable for a test that has no SBI (NULL: the real one) */
void smf_mbs_set_context_create_sender(int (*sender)(smf_mbs_sess_t *));

/* The number of restorations waiting; and dropping all of them (shut-down) */
int smf_mbs_ngran_restorations_pending(void);
smf_mbs_ngran_restoration_t *smf_mbs_ngran_restoration_first(void);
void smf_mbs_ngran_restoration_remove_all(void);

/* A Namf_MBSBroadcast ContextUpdate to \p amf for the NG-RAN \p ran_id (the text is copied), cl. 8.3.2.3 step 6.
 * Returns OGS_OK when the request was handed to the SBI. */
int smf_mbs_sess_send_ngran_restoration(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, const char *ran_id);

/* The N3mb path failure (TS 23.527 V18.6.0 clause 8.5): the MB-UPF reports a user plane path failure in a PFCP Node
 * Report Request, with the IP address of the remote GTP-U peer in the Remote GTP-U Peer IE (TS 29.244 V18.10.0
 * clause 8.2.70). The MB-SMF determines the NG-RAN affected "based on the IP address of the remote GTP-U peer". */

/* The address of a Remote GTP-U Peer IE (octet 5: flags V6 bit 1, V4 bit 2, DI bit 3, NI bit 4, RTS bit 5; then the
 * IPv4 and IPv6 addresses when flagged). Returns false when it carries no address or is too short. */
bool smf_mbs_decode_remote_gtp_u_peer(const uint8_t *data, int len, ogs_ip_t *peer);

/* Whether any MBS session uses \p node for its N4mb PFCP session */
bool smf_mbs_node_has_sessions(ogs_pfcp_node_t *node);

/* Replaces the sender of the ContextUpdates of the N3mb path failure (NULL: the SBI one), for tests */
void smf_mbs_set_context_update_sender(int (*sender)(smf_mbs_sess_t *, smf_namf_context_update_param_t *));

/* The DL tunnel of the session that ends at \p peer; NULL if there is none */
smf_mbs_dl_tunnel_t *smf_mbs_sess_dl_tunnel_find_by_peer(smf_mbs_sess_t *mbs_sess, const ogs_ip_t *peer);

/* Clause 8.5 step 3: for each broadcast MBS session of \p node that has a DL tunnel ending at \p peer, a
 * Namf_MBSBroadcast ContextUpdate with the NG-RAN ID of the NG-RAN and an N3mb path failure indication goes to the AMF
 * that gave the tunnel. Returns the number of requests sent. */
int smf_mbs_node_report_path_failure(ogs_pfcp_node_t *node, const ogs_ip_t *peer);

/* The restoration of the PFCP sessions of the broadcast MBS sessions of an MB-UPF that restarted (TS 23.527 V18.6.0
 * clause 8.2.2): "Immediately after the re-establishment of a PFCP association between the MB-SMF and the MB-UPF,
 * the MB-SMF may start restoring the PFCP sessions of the affected MBS sessions in the MB-UPF, by following PFCP
 * requirements for establishing a PFCP session over N4mb as specified in clause 5.34.2 of 3GPP TS 29.244 [4], with
 * the following modifications" (the MBS RESTI flag, the LL SSM and C-TEID, the ingress tunnel of the session, see
 * smf_n4mb_build_session_establishment_request()). Sends the N4mb Session Establishment Request of \p mbs_sess.
 * Returns OGS_OK when it was sent. */
int smf_mbs_sess_restore_n4mb(smf_mbs_sess_t *mbs_sess);

/* The MB-UPF answered the restoration request. Clause 8.2.2: "If multicast transport is used on N3mb ... and the
 * MB-UPF cannot accept the requested ... LL SSM address ... the MB-UPF shall reject the PFCP Session Establishment
 * Request with the cause "PFCP session restoration failure due to requested resource not available". The MB-SMF may
 * then proceed as specified for the restoration procedure for MB-UPF failure without restart". That procedure
 * (clause 8.2.3, an alternative MB-UPF) is not implemented: the session is then released, as when no restoration
 * is attempted. Returns true when the session was restored. */
bool smf_mbs_sess_restoration_response(smf_mbs_sess_t *mbs_sess, uint8_t cause);

/* The MB-UPF \p failed without restart (no PFCP heartbeat answer, TS 23.527 V18.6.0 clause 8.1A: "an MB-SMF and
 * MB-UPF shall utilize the PFCP Heartbeat Request and Heartbeat Response messages to detect a peer PFCP entity
 * failure or restart"). Clause 8.2.3: "the MB-SMF may restore the MBS sessions that were served by the failed MB-UPF by
 * selecting an alternative MB-UPF and by restoring the PFCP sessions of the MBS sessions in the alternative
 * MB-UPF". Each broadcast session of the node moves to an alternative: another associated PFCP peer, taken as the
 * MB-SMF takes it for a new session (the next one of the peer list). A session with no alternative stays where it
 * is. Returns the number of sessions whose establishment at an alternative was sent. */
int smf_mbs_node_failed(ogs_pfcp_node_t *failed);

/* The alternative MB-UPF for sessions of \p failed: an associated peer other than it; NULL if there is none */
ogs_pfcp_node_t *smf_mbs_alternative_upf(ogs_pfcp_node_t *failed);

/* Sends the establishment of \p mbs_sess to \p alternative. The LL SSM, the C-TEID and the N3mb address of the failed
 * MB-UPF are forgotten: the alternative assigns new ones. */
int smf_mbs_sess_relocate_n4mb(smf_mbs_sess_t *mbs_sess, ogs_pfcp_node_t *alternative);

/* The MBS session is lost with the MB-UPF (no restoration, or it failed): the subscribers are told the broadcast has
 * ended, every AMF is told to release its context, and the session is released. */
void smf_mbs_sess_release_lost(smf_mbs_sess_t *mbs_sess);

#ifdef __cplusplus
}
#endif

#endif /* SMF_MBS_RESTORE_H */
