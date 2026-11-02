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

#include "context.h"
#include "mbs-restore.h"
#include "mbs-n3mb.h"
#include "sbi-path.h"
#include "event.h"
#include "mbs-subsc.h"
#include "pfcp-path.h"
#include "smf-sm.h"

static ogs_list_t pending_restorations;

/* the ContextUpdate sender, replaceable for a test that has no SBI */
static int (*context_update_sender)(smf_mbs_sess_t *, smf_namf_context_update_param_t *) =
        smf_sbi_send_mbs_broadcast_context_update;

void smf_mbs_set_context_update_sender(int (*sender)(smf_mbs_sess_t *, smf_namf_context_update_param_t *))
{
    context_update_sender = sender ? sender : smf_sbi_send_mbs_broadcast_context_update;
}

/* The DL tunnel of the NG-RAN node \p ran_id taken away, the change recorded for the MB-UPF. Returns 1 when there
 * was one. */
static int remove_dl_tunnel_of(smf_mbs_sess_t *mbs_sess, const char *ran_id, smf_mbs_tunnel_changes_t *changes)
{
    smf_mbs_dl_tunnel_t *tunnel = smf_mbs_sess_dl_tunnel_find(mbs_sess, ran_id);

    if (!tunnel)
        return 0;

    if (changes && changes->count < SMF_MBS_MAX_TUNNEL_CHANGES) {
        smf_mbs_tunnel_change_t *tc = &changes->change[changes->count++];

        memset(tc, 0, sizeof(*tc));
        tc->has_remove = true;
        tc->remove_id = tunnel->unicast_params_id;
    }
    smf_mbs_sess_dl_tunnel_remove(mbs_sess, tunnel);
    return 1;
}

int smf_mbs_sess_take_ngran_failure_event(smf_mbs_sess_t *mbs_sess, const OpenAPI_ngran_failure_event_t *event,
        smf_mbs_tunnel_changes_t *changes, smf_mbs_ngran_restore_t *restore)
{
    char *ran_id = NULL;
    int removed = 0;

    ogs_assert(mbs_sess);
    ogs_assert(event);
    ogs_assert(restore);

    memset(restore, 0, sizeof(*restore));

    if (!event->ngran_id) {
        /* table 6.5.6.2.9-1: "ngranId | GlobalRanNodeId | M | 1" */
        ogs_error("MBS Broadcast ContextStatusNotify: an NG-RAN event without the NG-RAN ID");
        return 0;
    }
    ran_id = smf_mbs_ran_id_text((OpenAPI_global_ran_node_id_t *)event->ngran_id);

    switch (event->ngran_failure_indication) {
    case OpenAPI_ngran_failure_indication_NG_RAN_RESTART_OR_START:
        /* TS 23.527 V18.6.0 clause 8.3.2.3, step 5: "The MB-SMF modifies the PFCP sessions corresponding to the
         * affected MBS sessions to remove the NG-RAN DL F-TEID for unicast transport over N3mb." and step 6: "The
         * MB-SMF sends Namf_MBSBroadcast_ContextUpdate Request message to the AMF to (re)start the MBS session in
         * the related NG-RAN including the MBS Session Setup or Modification Request Transfer IE and the NG-RAN ID
         * received in the step 3." */
        removed = remove_dl_tunnel_of(mbs_sess, ran_id, changes);
        restore->restore = true;
        restore->ran_id = ran_id;
        ran_id = NULL;
        break;

    case OpenAPI_ngran_failure_indication_NG_RAN_FAILURE_WITHOUT_RESTART:
        /* clause 8.3.3, step 5: "The MB-SMF modifies the PFCP sessions corresponding to the affected MBS sessions to
         * remove the NG-RAN DL F-TEID in case NG-RAN failure without restart for unicast transport over N3mb." */
        removed = remove_dl_tunnel_of(mbs_sess, ran_id, changes);
        break;

    case OpenAPI_ngran_failure_indication_NG_RAN_REQUIRED_RELEASE:
        /* TS 29.518 V18.14.0 clause 5.6.2.5: "Upon receiving such a notification, per local policies, the MB-SMF
         * may attempt to re-establish the MBS session after some operator configurable time in these NG-RANs by
         * performing the Broadcast MBS session restoration by MB-SMF procedure specified in clause 8.3.2.3 of
         * TS 23.527". */
        restore->restore_later = true;
        restore->ran_id = ran_id;
        ran_id = NULL;
        break;

    default:
        /* NG_RAN_NOT_REACHABLE: no clause of TS 23.527 clause 8 read names an action of the MB-SMF for it */
        ogs_warn("MBS Broadcast ContextStatusNotify: NG-RAN [%s] event [%s]", ran_id,
                OpenAPI_ngran_failure_indication_ToString(event->ngran_failure_indication));
        break;
    }

    if (ran_id)
        ogs_free(ran_id);
    return removed;
}

int smf_mbs_sess_send_ngran_restoration(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, const char *ran_id)
{
    smf_namf_context_update_param_t param;
    const char *ran_ids[1] = { ran_id };

    ogs_assert(mbs_sess);
    ogs_assert(ran_id);

    if (!amf || !amf->mbs_context_ref) {
        ogs_error("MBS session mbsSessionRef[%s]: NG-RAN [%s] cannot be restored, the AMF holds no context of the "
                "session", mbs_sess->mbs_session_ref, ran_id);
        return OGS_ERROR;
    }

    memset(&param, 0, sizeof(param));
    param.amf = amf;
    param.no_n2_info_change_ind = true;
    param.ran_ids = ran_ids;
    param.num_ran_ids = 1;

    ogs_info("MBS session mbsSessionRef[%s]: restoring it in NG-RAN [%s]", mbs_sess->mbs_session_ref, ran_id);

    /* ogs_sbi_xact_add() builds the request before it returns (lib/sbi/context.c), and keeps no pointer to param */
    if (smf_sbi_send_mbs_broadcast_context_update(mbs_sess, &param) != OGS_OK) {
        ogs_error("Failed to send MBS Broadcast ContextUpdate to restore an NG-RAN");
        return OGS_ERROR;
    }
    return OGS_OK;
}

static void restoration_remove(smf_mbs_ngran_restoration_t *restoration)
{
    ogs_list_remove(&pending_restorations, restoration);
    if (restoration->timer)
        ogs_timer_delete(restoration->timer);
    if (restoration->ran_id)
        ogs_free(restoration->ran_id);
    ogs_free(restoration);
}

/* the timer thread: the work is done by the SMF's own thread */
static void restoration_timeout(void *data)
{
    smf_event_t *e = smf_event_new(SMF_EVT_MBS_NGRAN_RESTORE);

    ogs_assert(e);
    e->mbs_ngran_restoration = data;
    if (ogs_queue_push(ogs_app()->queue, e) != OGS_OK) {
        ogs_error("ogs_queue_push() failed in the restoration of an NG-RAN");
        ogs_event_free(e);
    }
}

int smf_mbs_sess_schedule_ngran_restoration(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, const char *ran_id)
{
    smf_mbs_ngran_restoration_t *restoration = NULL;
    int seconds = 0;

    ogs_assert(mbs_sess);
    ogs_assert(amf);
    ogs_assert(ran_id);

    if (!smf_mbs_ngran_restore_delay(&seconds)) {
        ogs_info("MBS session mbsSessionRef[%s]: NG-RAN [%s] required the release; no smf.ngranRestoreDelay is "
                "configured, so the session is not restored there", mbs_sess->mbs_session_ref, ran_id);
        return 0;
    }

    ogs_list_for_each(&pending_restorations, restoration)
        if (restoration->mbs_sess_id == mbs_sess->id && restoration->amf_id == amf->id &&
                strcmp(restoration->ran_id, ran_id) == 0)
            return 0;

    restoration = ogs_calloc(1, sizeof(*restoration));
    ogs_assert(restoration);
    restoration->mbs_sess_id = mbs_sess->id;
    restoration->amf_id = amf->id;
    restoration->ran_id = ogs_strdup(ran_id);
    ogs_assert(restoration->ran_id);
    restoration->timer = ogs_timer_add(ogs_app()->timer_mgr, restoration_timeout, restoration);
    ogs_assert(restoration->timer);
    ogs_list_add(&pending_restorations, restoration);
    ogs_timer_start(restoration->timer, ogs_time_from_sec(seconds));

    ogs_info("MBS session mbsSessionRef[%s]: NG-RAN [%s] required the release; restoring it in %d s",
            mbs_sess->mbs_session_ref, ran_id, seconds);
    return 1;
}

void smf_mbs_ngran_restoration_expired(smf_mbs_ngran_restoration_t *restoration)
{
    smf_mbs_sess_t *mbs_sess = NULL;
    smf_mbs_amf_t *amf = NULL;

    ogs_assert(restoration);

    mbs_sess = smf_mbs_sess_find_by_id(restoration->mbs_sess_id);
    if (!mbs_sess) {
        ogs_info("Restoration of NG-RAN [%s]: the MBS session is gone", restoration->ran_id);
    } else if (mbs_sess->release_triggered) {
        ogs_info("Restoration of NG-RAN [%s]: the MBS session mbsSessionRef[%s] is being released",
                restoration->ran_id, mbs_sess->mbs_session_ref);
    } else {
        amf = smf_mbs_sess_amf_find(mbs_sess, restoration->amf_id);
        if (amf)
            smf_mbs_sess_send_ngran_restoration(mbs_sess, amf, restoration->ran_id);
        else
            ogs_info("Restoration of NG-RAN [%s]: the AMF of the MBS session mbsSessionRef[%s] is gone",
                    restoration->ran_id, mbs_sess->mbs_session_ref);
    }

    restoration_remove(restoration);
}

int smf_mbs_ngran_restorations_pending(void)
{
    return ogs_list_count(&pending_restorations);
}

smf_mbs_ngran_restoration_t *smf_mbs_ngran_restoration_first(void)
{
    return ogs_list_first(&pending_restorations);
}

void smf_mbs_ngran_restoration_remove_all(void)
{
    smf_mbs_ngran_restoration_t *restoration = NULL, *next = NULL;

    ogs_list_for_each_safe(&pending_restorations, next, restoration)
        restoration_remove(restoration);
}

bool smf_mbs_decode_remote_gtp_u_peer(const uint8_t *data, int len, ogs_ip_t *peer)
{
    int offset = 1;
    bool v6, v4;

    ogs_assert(peer);
    memset(peer, 0, sizeof(*peer));

    if (!data || len < 1)
        return false;
    v6 = data[0] & 0x01;
    v4 = data[0] & 0x02;
    if (v4) {
        if (len < offset + OGS_IPV4_LEN)
            return false;
        peer->ipv4 = 1;
        memcpy(&peer->addr, data + offset, OGS_IPV4_LEN);
        offset += OGS_IPV4_LEN;
    }
    if (v6) {
        if (len < offset + OGS_IPV6_LEN)
            return false;
        peer->ipv6 = 1;
        memcpy(peer->addr6, data + offset, OGS_IPV6_LEN);
    }
    if (!peer->ipv4 && !peer->ipv6)
        return false;
    peer->len = (peer->ipv4 ? OGS_IPV4_LEN : 0) + (peer->ipv6 ? OGS_IPV6_LEN : 0);
    return true;
}

bool smf_mbs_node_has_sessions(ogs_pfcp_node_t *node)
{
    smf_mbs_sess_t *mbs_sess = NULL;

    ogs_assert(node);

    ogs_list_for_each(&smf_self()->smf_mbs_sess_list, mbs_sess)
        if (mbs_sess->pfcp_node == node)
            return true;
    return false;
}

smf_mbs_dl_tunnel_t *smf_mbs_sess_dl_tunnel_find_by_peer(smf_mbs_sess_t *mbs_sess, const ogs_ip_t *peer)
{
    smf_mbs_dl_tunnel_t *tunnel = NULL;

    ogs_assert(mbs_sess);
    ogs_assert(peer);

    ogs_list_for_each(&mbs_sess->dl_tunnel_list, tunnel) {
        if ((peer->ipv4 && tunnel->addr.ipv4 && peer->addr == tunnel->addr.addr) ||
            (peer->ipv6 && tunnel->addr.ipv6 && memcmp(peer->addr6, tunnel->addr.addr6, OGS_IPV6_LEN) == 0))
            return tunnel;
    }
    return NULL;
}

int smf_mbs_node_report_path_failure(ogs_pfcp_node_t *node, const ogs_ip_t *peer)
{
    smf_mbs_sess_t *mbs_sess = NULL, *next = NULL;
    int sent = 0;

    ogs_assert(node);
    ogs_assert(peer);

    ogs_list_for_each_safe(&smf_self()->smf_mbs_sess_list, next, mbs_sess) {
        smf_mbs_dl_tunnel_t *tunnel = NULL;
        smf_mbs_amf_t *amf = NULL;
        smf_namf_context_update_param_t param;
        const char *ran_ids[1];

        if (mbs_sess->pfcp_node != node || mbs_sess->release_triggered)
            continue;
        tunnel = smf_mbs_sess_dl_tunnel_find_by_peer(mbs_sess, peer);
        if (!tunnel)
            continue;

        amf = smf_mbs_sess_amf_find(mbs_sess, tunnel->amf_id);
        if (!amf || !amf->mbs_context_ref) {
            ogs_error("MBS session mbsSessionRef[%s]: N3mb path failure towards NG-RAN [%s], but the AMF that gave "
                    "the tunnel holds no context of the session", mbs_sess->mbs_session_ref, tunnel->ran_id);
            continue;
        }

        ran_ids[0] = tunnel->ran_id;
        memset(&param, 0, sizeof(param));
        param.amf = amf;
        param.no_n2_info_change_ind = true;
        param.ran_ids = ran_ids;
        param.num_ran_ids = 1;
        param.n3mb_path_failure = true;

        ogs_warn("MBS session mbsSessionRef[%s]: N3mb path failure towards NG-RAN [%s]", mbs_sess->mbs_session_ref,
                tunnel->ran_id);
        if (context_update_sender(mbs_sess, &param) == OGS_OK)
            sent++;
        else
            ogs_error("Failed to send MBS Broadcast ContextUpdate for an N3mb path failure");
    }

    return sent;
}

void smf_mbs_sess_release_lost(smf_mbs_sess_t *mbs_sess)
{
    ogs_assert(mbs_sess);

    if (mbs_sess->release_triggered)
        return;     /* an explicit release is already in flight */
    mbs_sess->release_triggered = true;
    mbs_sess->restoring = false;

    ogs_error("MBS Session mbsSessionRef[%s] lost at the MB-UPF -- releasing the SMF-side state so its TMGI/SSM can "
            "be reused", mbs_sess->mbs_session_ref);

    /* TS 29.532 V18.6.0 cl. 5.3.2.8.1: "the new broadcast delivery status (e.g. the MBS session has been STARTED or
     * TERMINATED)" */
    if (ogs_strcasecmp(mbs_sess->service_type, "BROADCAST") == 0)
        smf_mbs_sess_report_broadcast_delivery_status(mbs_sess, OpenAPI_broadcast_delivery_status_TERMINATED);

    smf_sbi_send_mbs_broadcast_context_delete_all(mbs_sess);
    smf_mbs_sess_release(mbs_sess);
}

int smf_mbs_sess_restore_n4mb(smf_mbs_sess_t *mbs_sess)
{
    ogs_assert(mbs_sess);
    ogs_assert(mbs_sess->pfcp_node);

    /* a new PFCP session at the MB-UPF: the request goes with SEID 0 (TS 29.244 V18.10.0 clause 7.2.2.4.2) */
    mbs_sess->restoring = true;
    mbs_sess->upf_n4mb_seid = 0;
    ogs_info("MBS Session mbsSessionRef[%s]: restoring its PFCP session at the MB-UPF", mbs_sess->mbs_session_ref);

    if (smf_5gc_pfcp_n4mb_send_session_establishment_request(mbs_sess, 0, NULL) != OGS_OK) {
        mbs_sess->restoring = false;
        return OGS_ERROR;
    }
    return OGS_OK;
}

bool smf_mbs_sess_restoration_response(smf_mbs_sess_t *mbs_sess, uint8_t cause)
{
    ogs_assert(mbs_sess);

    mbs_sess->restoring = false;
    mbs_sess->relocating = false;

    if (cause == OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
        ogs_info("MBS Session mbsSessionRef[%s] restored at the MB-UPF", mbs_sess->mbs_session_ref);
        return true;
    }

    if (cause == OGS_PFCP_CAUSE_RESTORATION_FAILURE_RESOURCE_NOT_AVAILABLE)
        ogs_error("MBS Session mbsSessionRef[%s]: the MB-UPF cannot restore it, the requested resource is not "
                "available (the restoration for MB-UPF failure without restart is not implemented)",
                mbs_sess->mbs_session_ref);
    else
        ogs_error("MBS Session mbsSessionRef[%s]: restoration at the MB-UPF refused, cause [%d]",
                mbs_sess->mbs_session_ref, cause);

    smf_mbs_sess_release_lost(mbs_sess);
    return false;
}

ogs_pfcp_node_t *smf_mbs_alternative_upf(ogs_pfcp_node_t *failed)
{
    ogs_pfcp_node_t *node = NULL;

    ogs_list_for_each(&ogs_pfcp_self()->pfcp_peer_list, node)
        if (node != failed && OGS_FSM_CHECK(&node->sm, smf_pfcp_state_associated))
            return node;
    return NULL;
}

int smf_mbs_sess_relocate_n4mb(smf_mbs_sess_t *mbs_sess, ogs_pfcp_node_t *alternative)
{
    ogs_assert(mbs_sess);
    ogs_assert(alternative);

    /* "the MB-SMF shall request the alternative MB-UPF to allocate a new N3mb and/or N19mb LL SSM address and C-TEID,
     * if multicast transport is used over N3mb and/or N19mb" (cl. 8.2.3): what the failed MB-UPF gave is forgotten */
    memset(&mbs_sess->ll_ssm, 0, sizeof(mbs_sess->ll_ssm));
    mbs_sess->c_teid = 0;
    if (mbs_sess->upf_n3mb_addr) {
        ogs_freeaddrinfo(mbs_sess->upf_n3mb_addr);
        mbs_sess->upf_n3mb_addr = NULL;
    }
    if (mbs_sess->upf_n3mb_addr6) {
        ogs_freeaddrinfo(mbs_sess->upf_n3mb_addr6);
        mbs_sess->upf_n3mb_addr6 = NULL;
    }
    mbs_sess->upf_n4mb_seid = 0;
    OGS_SETUP_PFCP_NODE(mbs_sess, alternative);
    /* the N3mb address of the session is the address of its MB-UPF, as at the creation */
    if (alternative->addr.ogs_sa_family == AF_INET)
        ogs_assert(OGS_OK == ogs_copyaddrinfo(&mbs_sess->upf_n3mb_addr, &alternative->addr));
    else if (alternative->addr.ogs_sa_family == AF_INET6)
        ogs_assert(OGS_OK == ogs_copyaddrinfo(&mbs_sess->upf_n3mb_addr6, &alternative->addr));
    mbs_sess->relocating = true;

    ogs_info("MBS Session mbsSessionRef[%s]: establishing its PFCP session at an alternative MB-UPF",
            mbs_sess->mbs_session_ref);
    if (smf_5gc_pfcp_n4mb_send_session_establishment_request(mbs_sess, 0, NULL) != OGS_OK) {
        mbs_sess->relocating = false;
        return OGS_ERROR;
    }
    return OGS_OK;
}

int smf_mbs_node_failed(ogs_pfcp_node_t *failed)
{
    smf_mbs_sess_t *mbs_sess = NULL, *next = NULL;
    ogs_pfcp_node_t *alternative = NULL;
    int sent = 0;

    ogs_assert(failed);

    ogs_list_for_each_safe(&smf_self()->smf_mbs_sess_list, next, mbs_sess) {
        if (mbs_sess->pfcp_node != failed || mbs_sess->release_triggered || mbs_sess->restoring ||
                mbs_sess->relocating)
            continue;
        if (ogs_strcasecmp(mbs_sess->service_type, "BROADCAST") != 0)
            continue;

        alternative = smf_mbs_alternative_upf(failed);
        if (!alternative) {
            ogs_warn("MBS Session mbsSessionRef[%s]: its MB-UPF failed and there is no alternative MB-UPF",
                    mbs_sess->mbs_session_ref);
            continue;
        }
        if (smf_mbs_sess_relocate_n4mb(mbs_sess, alternative) == OGS_OK)
            sent++;
        else
            smf_mbs_sess_release_lost(mbs_sess);
    }

    return sent;
}
