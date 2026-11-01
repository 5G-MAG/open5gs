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
    char *ran_id;       /* GlobalRanNodeId as JSON text, owned by the caller (ogs_free) once restore is set */
} smf_mbs_ngran_restore_t;

/* Takes in one ngranFailureEvent:
 *  - NG_RAN_RESTART_OR_START (cl. 8.3.2.3 steps 5 and 6): the NG-RAN DL tunnel is taken away (recorded in
 *    \p changes, for smf_mbs_sess_send_tunnel_changes()), and the session is to be restored in that NG-RAN;
 *  - NG_RAN_FAILURE_WITHOUT_RESTART (cl. 8.3.3 step 5): the NG-RAN DL tunnel is taken away, nothing else;
 *  - the other indications ask nothing of the MB-SMF in the clauses read.
 * \p changes is appended to (it is not cleared). Returns the number of DL tunnels taken away. */
int smf_mbs_sess_take_ngran_failure_event(smf_mbs_sess_t *mbs_sess, const OpenAPI_ngran_failure_event_t *event,
        smf_mbs_tunnel_changes_t *changes, smf_mbs_ngran_restore_t *restore);

#ifdef __cplusplus
}
#endif

#endif /* SMF_MBS_RESTORE_H */
