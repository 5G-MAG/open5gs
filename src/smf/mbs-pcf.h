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

#ifndef SMF_MBS_PCF_H
#define SMF_MBS_PCF_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The MB-SMF as consumer of Npcf_MBSPolicyControl (TS 29.537): an MBS Policy Association is created at the PCF when
 * an MBS session is created, updated, and deleted with it, when smf.mbsPolicyControl is enabled (TS 23.247 V18.8.0
 * clause 7.1.1.3: "Deployment of dynamic PCC is optional."). */

typedef enum smf_mbs_pcf_op_e {
    SMF_MBS_PCF_OP_NONE = 0,
    SMF_MBS_PCF_OP_CREATE,
    SMF_MBS_PCF_OP_UPDATE,
    SMF_MBS_PCF_OP_DELETE,
} smf_mbs_pcf_op_e;

typedef enum smf_mbs_pcf_outcome_e {
    SMF_MBS_PCF_OUTCOME_OK = 0,     /* the PCF did what was asked */
    SMF_MBS_PCF_OUTCOME_ALLOW,      /* MBS_POLICY_CONTEXT_DENIED and the failure action is allow: go on */
    SMF_MBS_PCF_OUTCOME_REJECT,     /* answer the consumer with status and cause */
} smf_mbs_pcf_outcome_e;

typedef struct smf_mbs_pcf_answer_s {
    smf_mbs_pcf_outcome_e outcome;
    int status;                     /* REJECT: the status of the answer to the consumer */
    const char *cause;              /* REJECT: its application error code */
    const char *title;
} smf_mbs_pcf_answer_t;

/* What the MB-SMF does with the answer of the PCF to an operation (TS 29.532 V18.6.0 tables 6.2.3.2.3.1-3 and
 * 6.2.3.3.3.1-3, the cause "e.g. if the PCF rejected the creation or update of an MBS Policy Association with the
 * same error"). \p pcf_status is the HTTP status, \p pcf_cause the cause of its problem details (or NULL). */
void smf_mbs_pcf_classify(smf_mbs_pcf_op_e op, int pcf_status, const char *pcf_cause,
        smf_mbs_pcf_failure_action_e failure_action, smf_mbs_pcf_answer_t *answer);

/* The requests (the data of the builder is unused for Create and Delete) */
ogs_sbi_request_t *smf_npcf_mbspolicycontrol_build_create(smf_mbs_sess_t *mbs_sess, void *data);
ogs_sbi_request_t *smf_npcf_mbspolicycontrol_build_delete(smf_mbs_sess_t *mbs_sess, void *data);
/* The data of the Update is the MBS service information as JSON text, or NULL when the Update brings none */
ogs_sbi_request_t *smf_npcf_mbspolicycontrol_build_update(smf_mbs_sess_t *mbs_sess, void *data);

/* Whether an Nmbsmf_MBSSession Update (its patch items) needs an Npcf_MBSPolicyControl_Update: the session has an MBS
 * Policy Association, and the request carries contactPcfInd true, or new MBS service information without
 * contactPcfInd false (TS 29.532 V18.6.0 table 6.2.6.2.4-1: "false" means that the PCF shall not be contacted). */
bool smf_mbs_pcf_update_wanted(smf_mbs_sess_t *mbs_sess, OpenAPI_list_t *patch_items);

/* Starts Npcf_MBSPolicyControl_Update for the Update that waits on \p stream; \p serv_info_json is the MBS service
 * information the Update leaves. The Update is applied when the PCF answers. Returns false when the exchange could not
 * be started: the stream has then been answered. */
bool smf_mbs_pcf_update_send(smf_mbs_sess_t *mbs_sess, ogs_sbi_stream_t *stream, OpenAPI_list_t *patch_items,
        const char *serv_info_json);

/* Starts Npcf_MBSPolicyControl_Create for the session, whose Create request waits on \p stream. Returns false when the
 * exchange could not be started: the stream has then been answered. Otherwise the answer arrives in
 * smf_mbs_pcf_handle_response(). */
bool smf_mbs_pcf_create_send(smf_mbs_sess_t *mbs_sess, ogs_sbi_stream_t *stream);

/* Starts Npcf_MBSPolicyControl_Delete for the session whose Release request waits on \p stream. The release goes on when
 * the PCF has answered (or not, in time). Returns false when the exchange could not be started: nothing has been
 * answered, the caller goes on with the release. */
bool smf_mbs_pcf_delete_send(smf_mbs_sess_t *mbs_sess, ogs_sbi_stream_t *stream);

/* The answer of the PCF to the request in flight of the session (the SBI response event) */
void smf_mbs_pcf_handle_response(smf_mbs_sess_t *mbs_sess, int xact_state, ogs_sbi_message_t *recvmsg);

/* The answer of the PCF did not come in time (SMF_EVT_MBS_PCF_TIMEOUT) */
void smf_mbs_pcf_timeout(ogs_pool_id_t mbs_sess_id, int serial);

#define SMF_MBS_PCF_SUPP_FEAT_AREA_SESS_POLICY "1"   /* TS 29.537 table 6.1.8-1, feature 1 "AreaSessPolicy" */

/* The state of the exchange with the PCF goes with the session */
void smf_mbs_pcf_sess_clear(smf_mbs_sess_t *mbs_sess);

/* Sends the problem details of an answer to the consumer; \p acc_mbs_serv_info (may be NULL) is the AcceptableMbsServInfo
 * the PCF gave, sent as accMbsServiceInfo (TS 29.532 ProblemDetailsExtension). */
void smf_mbs_pcf_send_error(ogs_sbi_stream_t *stream, const smf_mbs_pcf_answer_t *answer, const char *detail,
        cJSON *acc_mbs_serv_info);

#ifdef __cplusplus
}
#endif

#endif /* SMF_MBS_PCF_H */
