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

    default:
        /* NG_RAN_NOT_REACHABLE and NG_RAN_REQUIRED_RELEASE: no clause of TS 23.527 clause 8 read names an action
         * of the MB-SMF for them */
        ogs_warn("MBS Broadcast ContextStatusNotify: NG-RAN [%s] event [%s]", ran_id,
                OpenAPI_ngran_failure_indication_ToString(event->ngran_failure_indication));
        break;
    }

    if (ran_id)
        ogs_free(ran_id);
    return removed;
}
