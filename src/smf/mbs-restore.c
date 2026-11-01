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

static ogs_list_t pending_restorations;

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
    smf_namf_context_update_param_t *param = NULL;
    const char **ran_ids = NULL;

    ogs_assert(mbs_sess);
    ogs_assert(ran_id);

    if (!amf || !amf->mbs_context_ref) {
        ogs_error("MBS session mbsSessionRef[%s]: NG-RAN [%s] cannot be restored, the AMF holds no context of the "
                "session", mbs_sess->mbs_session_ref, ran_id);
        return OGS_ERROR;
    }

    param = ogs_calloc(1, sizeof(*param));
    ogs_assert(param);
    ran_ids = ogs_calloc(1, sizeof(*ran_ids));
    ogs_assert(ran_ids);
    ran_ids[0] = ogs_strdup(ran_id);
    ogs_assert(ran_ids[0]);
    param->amf = amf;
    param->no_n2_info_change_ind = true;
    param->ran_ids = ran_ids;
    param->num_ran_ids = 1;
    param->heap_owned = true;

    ogs_info("MBS session mbsSessionRef[%s]: restoring it in NG-RAN [%s]", mbs_sess->mbs_session_ref, ran_id);

    /* the builder frees param, ran_ids and the text once it ran; when sending fails it may or may not have run, so
     * nothing is freed here (a failed send leaks these few bytes rather than risk a double free) */
    if (smf_sbi_send_mbs_broadcast_context_update(mbs_sess, param) != OGS_OK) {
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
