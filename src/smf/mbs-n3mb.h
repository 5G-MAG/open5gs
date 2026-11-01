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

#ifndef SMF_MBS_N3MB_H
#define SMF_MBS_N3MB_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The N2 MBS Session Management containers an AMF returns for a broadcast MBS session (TS 29.518 V18.14.0 table
 * 6.5.6.2.3-1 n2MbsSmInfoList, 6.5.6.2.6-1 and 6.5.6.2.4-1 for the ContextUpdate answer and the notification) and
 * what the MB-SMF takes from them. */

/* The MBS Session Setup or Modification Response Transfer (TS 38.413 clause 9.3.5.5): the shared NG-U unicast tunnel
 * the NG-RAN node offers, if it does ("MBS Session TNL Information NG-RAN | O"). For a location dependent session
 * the item of the Area Session ID. Returns false when the container cannot be decoded; *has_tunnel is false when it
 * carries no unicast tunnel (multicast transport: nothing to give). */
bool smf_mbs_n3mb_parse_response_transfer(ogs_pkbuf_t *pkbuf, bool has_area_session_id, uint16_t area_session_id,
        bool *has_tunnel, ogs_ip_t *addr, uint32_t *teid);

/* The MBS Session Setup or Modification Failure Transfer (clause 9.3.5.6): its Cause as group and value
 * ("Cause | M"). Returns false when the container cannot be decoded. */
bool smf_mbs_n3mb_parse_failure_transfer(ogs_pkbuf_t *pkbuf, int *group, long *value);

/* The text that names an NG-RAN node in the answer of an AMF: its GlobalRanNodeId as JSON, or "(none)" */
char *smf_mbs_ran_id_text(OpenAPI_global_ran_node_id_t *ran_id);

/* What the answer of an AMF says, taken in: the completion of the start or of the update (and so the start of the
 * session, reported to the subscribers once every AMF has completed it), the N3mb DL tunnels, the failures. Used for
 * the 201 of a ContextCreate, the 200 of a ContextUpdate and a ContextStatusNotify. Returns the number of tunnels
 * that were added, changed or taken away (the user plane has to be told). */
int smf_mbs_sess_take_amf_answer(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf,
        OpenAPI_operation_status_e operation_status, OpenAPI_list_t *n2_mbs_sm_info_list, ogs_sbi_message_t *recvmsg);

#ifdef __cplusplus
}
#endif

#endif /* SMF_MBS_N3MB_H */
