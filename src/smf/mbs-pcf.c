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

#include "mbs-pcf.h"
#include "nmbsmf-handler.h"
#include "sbi-path.h"
#include "event.h"

/* TS 29.537 V18.x clause 5.2.2.2.2 (Create), 5.2.2.3.2 (Update), 5.2.2.4.2 (Delete) */

void smf_mbs_pcf_classify(smf_mbs_pcf_op_e op, int pcf_status, const char *pcf_cause,
        smf_mbs_pcf_failure_action_e failure_action, smf_mbs_pcf_answer_t *answer)
{
    ogs_assert(answer);
    memset(answer, 0, sizeof(*answer));

    if ((op == SMF_MBS_PCF_OP_CREATE && pcf_status == OGS_SBI_HTTP_STATUS_CREATED) ||
            (op == SMF_MBS_PCF_OP_UPDATE && pcf_status == OGS_SBI_HTTP_STATUS_OK) ||
            (op == SMF_MBS_PCF_OP_DELETE && pcf_status == OGS_SBI_HTTP_STATUS_NO_CONTENT)) {
        answer->outcome = SMF_MBS_PCF_OUTCOME_OK;
        return;
    }

    answer->outcome = SMF_MBS_PCF_OUTCOME_REJECT;
    if (!pcf_cause)
        pcf_cause = "";

    if (pcf_status == OGS_SBI_HTTP_STATUS_BAD_REQUEST) {
        /* TS 29.532 V18.6.0 table 6.2.3.2.3.1-3, 400: "INVALID_MBS_SERVICE_INFO, ... e.g. if the PCF rejected the
         * creation or update of an MBS Policy Association with the same error" (and the same for the two others) */
        if (!strcmp(pcf_cause, NMBSMF_MBSSESSION_INVALID_MBS_SERVICE_INFO)) {
            answer->status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            answer->cause = NMBSMF_MBSSESSION_INVALID_MBS_SERVICE_INFO;
            answer->title = "Invalid MBS service information";
            return;
        }
        if (!strcmp(pcf_cause, NMBSMF_MBSSESSION_FILTER_RESTRICTIONS_NOT_RESPECTED)) {
            answer->status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            answer->cause = NMBSMF_MBSSESSION_FILTER_RESTRICTIONS_NOT_RESPECTED;
            answer->title = "Filter restrictions not respected";
            return;
        }
        if (!strcmp(pcf_cause, NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS)) {
            answer->status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            answer->cause = NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS;
            answer->title = "Error input parameters";
            return;
        }
    } else if (pcf_status == OGS_SBI_HTTP_STATUS_FORBIDDEN) {
        if (!strcmp(pcf_cause, NMBSMF_MBSSESSION_MBS_SERVICE_INFO_NOT_AUTHORIZED)) {
            answer->status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            answer->cause = NMBSMF_MBSSESSION_MBS_SERVICE_INFO_NOT_AUTHORIZED;
            answer->title = "MBS service information not authorized";
            return;
        }
        if (!strcmp(pcf_cause, NMBSMF_MBSSESSION_MBS_POLICY_CONTEXT_DENIED)) {
            /* TS 29.537 cl. 5.2.2.2.2: "At the reception of this error code and based on the internally configured
             * failure actions, the NF service consumer may reject or allow" */
            if (failure_action == SMF_MBS_PCF_FAILURE_ACTION_ALLOW) {
                answer->outcome = SMF_MBS_PCF_OUTCOME_ALLOW;
                return;
            }
            answer->status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            answer->cause = NMBSMF_MBSSESSION_MBS_POLICY_CONTEXT_DENIED;
            answer->title = "MBS policy context denied";
            return;
        }
    }

    /* no clause maps the other answers (a 404 of the association, a 5xx, a redirect): TS 29.500 V18.10.0 table
     * 5.2.7.2-1, row SYSTEM_FAILURE, "500 Internal Server Error": "The request is rejected due to generic error
     * condition in the NF." */
    answer->status = OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR;
    answer->cause = NMBSMF_MBSSESSION_SYSTEM_FAILURE;
    answer->title = "Internal Server Error";
}

/* The requests */

static OpenAPI_mbs_session_id_t *build_mbs_session_id(const smf_mbs_sess_t *mbs_sess)
{
    if (mbs_sess->mbs_session_id.is_tmgi)
        return OpenAPI_mbs_session_id_create(ogs_sbi_build_tmgi(mbs_sess->tmgi), NULL,
                mbs_sess->mbs_session_id.nid ? ogs_strdup(mbs_sess->mbs_session_id.nid) : NULL);
    if (mbs_sess->mbs_session_id.is_ssm)
        return OpenAPI_mbs_session_id_create(NULL, ogs_sbi_build_ssm(mbs_sess->mbs_session_id.ssm), NULL);
    return NULL;
}

static OpenAPI_mbs_service_info_t *build_mbs_service_info(const char *json_text)
{
    OpenAPI_mbs_service_info_t *info = NULL;
    cJSON *json;

    if (!json_text)
        return NULL;
    json = cJSON_Parse(json_text);
    if (!json)
        return NULL;
    info = OpenAPI_mbs_service_info_parseFromJSON(json);
    cJSON_Delete(json);
    return info;
}

/* TS 29.537 cl. 5.2.2.2.2 step 1: "the identifier of the concerned MBS Session, within the "mbsSessionId"
 * attribute; the MBS Service Information, if available, within the "mbsServInfo" attribute; the list of supported
 * features, if feature negotiation needs to take place, within the "suppFeat" attribute; and the Area Session Policy
 * ID within the "areaSessPolId" attribute, if the "AreaSessPolicy" feature is supported and the request corresponds
 * to an MBS Session that is an instance of a location-dependent MBS service" and "may contain: the DNN of the MBS
 * session, within the "dnn" attribute; and the S-NSSAI of the MBS session, within the "snssai" attribute." */
ogs_sbi_request_t *smf_npcf_mbspolicycontrol_build_create(smf_mbs_sess_t *mbs_sess, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    OpenAPI_mbs_policy_ctxt_data_t *ctxt = NULL;
    OpenAPI_mbs_session_id_t *session_id;

    ogs_assert(mbs_sess);

    session_id = build_mbs_session_id(mbs_sess);
    if (!session_id) {
        ogs_error("Npcf_MBSPolicyControl_Create: the MBS session has no MBS session ID");
        return NULL;
    }

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.service.name = (char *)OGS_SBI_SERVICE_NAME_NPCF_MBSPOLICYCONTROL;
    message.h.api.version = (char *)OGS_SBI_API_V1;
    message.h.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_MBS_POLICIES;

    ctxt = OpenAPI_mbs_policy_ctxt_data_create(session_id,
            mbs_sess->dnn ? ogs_strdup(mbs_sess->dnn) : NULL,
            smf_mbs_sess_build_snssai(mbs_sess),
            mbs_sess->location_dependent, mbs_sess->area_session_id,
            build_mbs_service_info(mbs_sess->mbs_serv_info_json),
            mbs_sess->location_dependent ? ogs_strdup(SMF_MBS_PCF_SUPP_FEAT_AREA_SESS_POLICY) : NULL);
    message.MbsPolicyCtxtData = ctxt;

    message.http.accept = (char *)(OGS_SBI_CONTENT_JSON_TYPE "," OGS_SBI_CONTENT_PROBLEM_TYPE);

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    OpenAPI_mbs_policy_ctxt_data_free(ctxt);

    return request;
}

/* TS 29.537 cl. 5.2.2.4.2 step 1: "the NF service consumer shall send an HTTP DELETE request to the PCF, targeting
 * the URI of the corresponding "Individual MBS Policy" resource." */
ogs_sbi_request_t *smf_npcf_mbspolicycontrol_build_delete(smf_mbs_sess_t *mbs_sess, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    ogs_assert(mbs_sess);
    ogs_assert(mbs_sess->pcf_policy_id);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_DELETE;
    message.h.service.name = (char *)OGS_SBI_SERVICE_NAME_NPCF_MBSPOLICYCONTROL;
    message.h.api.version = (char *)OGS_SBI_API_V1;
    message.h.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_MBS_POLICIES;
    message.h.resource.component[1] = mbs_sess->pcf_policy_id;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    return request;
}

/* The exchange in flight */

static void pcf_timeout_cb(void *data)
{
    smf_event_t *e = smf_event_new(SMF_EVT_MBS_PCF_TIMEOUT);

    ogs_assert(e);
    e->mbs_sess_id = (ogs_pool_id_t)((uintptr_t)data & 0xffffffffu);
    e->h.sbi.state = (int)((uintptr_t)data >> 32);
    if (ogs_queue_push(ogs_app()->queue, e) != OGS_OK) {
        ogs_error("ogs_queue_push() failed for the timeout of the PCF");
        ogs_event_free(e);
    }
}

static void pcf_exchange_end(smf_mbs_sess_t *mbs_sess)
{
    if (mbs_sess->pcf_timer)
        ogs_timer_stop(mbs_sess->pcf_timer);
    mbs_sess->pcf_pending = SMF_MBS_PCF_OP_NONE;
}

void smf_mbs_pcf_sess_clear(smf_mbs_sess_t *mbs_sess)
{
    if (mbs_sess->pcf_timer) {
        ogs_timer_delete(mbs_sess->pcf_timer);
        mbs_sess->pcf_timer = NULL;
    }
    if (mbs_sess->pcf_policy_id) {
        ogs_free(mbs_sess->pcf_policy_id);
        mbs_sess->pcf_policy_id = NULL;
    }
    mbs_sess->pcf_pending = SMF_MBS_PCF_OP_NONE;
}

/* The requests of the exchange go through the discovery of a PCF at the NRF. The SBI client has its own time limit
 * (time.message.sbi.client_wait_duration), but a request that cannot be delivered is dropped without an event: the
 * session keeps its own timer of the same length, so that the consumer is always answered. */
static bool pcf_send(smf_mbs_sess_t *mbs_sess, smf_mbs_pcf_op_e op, ogs_sbi_stream_t *stream,
        ogs_sbi_request_t *(*build)(smf_mbs_sess_t *mbs_sess, void *data), void *data)
{
    ogs_time_t wait = ogs_local_conf()->time.message.sbi.client_wait_duration;

    ogs_assert(mbs_sess);
    ogs_assert(!mbs_sess->pcf_pending);

    if (!mbs_sess->pcf_timer) {
        mbs_sess->pcf_timer = ogs_timer_add(ogs_app()->timer_mgr, pcf_timeout_cb, NULL);
        ogs_assert(mbs_sess->pcf_timer);
    }
    mbs_sess->pcf_serial++;
    ogs_timer_start(mbs_sess->pcf_timer, wait);
    /* the timer's data names the session and the exchange, never a pointer */
    mbs_sess->pcf_timer->data = (void *)(((uintptr_t)mbs_sess->pcf_serial << 32) | mbs_sess->id);

    mbs_sess->pcf_pending = op;
    mbs_sess->pcf_stream_id = stream ? ogs_sbi_id_from_stream(stream) : 0;

    /* no stream is given to the sender, which would answer it with 504 itself: the caller answers */
    if (smf_sbi_old_discover_and_send(OGS_SBI_SERVICE_TYPE_NPCF_MBSPOLICYCONTROL, NULL, build, mbs_sess, NULL,
                (int)op, data) != OGS_OK) {
        ogs_error("MBS session mbsSessionRef[%s]: the request to the PCF could not be sent", mbs_sess->mbs_session_ref);
        pcf_exchange_end(mbs_sess);
        return false;
    }
    return true;
}

/* The stream of the exchange, if it is still there; ends the exchange */
static ogs_sbi_stream_t *pcf_take_stream(smf_mbs_sess_t *mbs_sess)
{
    ogs_sbi_stream_t *stream = NULL;

    if (mbs_sess->pcf_stream_id >= OGS_MIN_POOL_ID && mbs_sess->pcf_stream_id <= OGS_MAX_POOL_ID)
        stream = ogs_sbi_stream_find_by_id(mbs_sess->pcf_stream_id);
    mbs_sess->pcf_stream_id = 0;
    pcf_exchange_end(mbs_sess);
    return stream;
}

void smf_mbs_pcf_send_error(ogs_sbi_stream_t *stream, const smf_mbs_pcf_answer_t *answer, const char *detail,
        cJSON *acc_mbs_serv_info)
{
    OpenAPI_problem_details_t problem;
    ogs_sbi_response_t *response;
    cJSON *json, *member;

    ogs_assert(stream);
    ogs_assert(answer);

    if (!acc_mbs_serv_info) {
        ogs_sbi_server_send_error(stream, answer->status, NULL, answer->title, detail, answer->cause);
        return;
    }

    /* TS 29.532 V18.6.0 ExtProblemDetails: the ProblemDetails with "accMbsServiceInfo", the AcceptableMbsServInfo
     * the PCF gave */
    memset(&problem, 0, sizeof(problem));
    problem.is_status = true;
    problem.status = answer->status;
    problem.title = (char *)answer->title;
    problem.detail = (char *)detail;
    problem.cause = (char *)answer->cause;
    json = OpenAPI_problem_details_convertToJSON(&problem);
    ogs_assert(json);
    member = cJSON_Duplicate(acc_mbs_serv_info, 1);
    ogs_assert(member);
    cJSON_AddItemToObject(json, "accMbsServiceInfo", member);

    response = ogs_sbi_response_new();
    ogs_assert(response);
    response->status = answer->status;
    response->http.content = cJSON_PrintUnformatted(json);
    ogs_assert(response->http.content);
    response->http.content_length = strlen(response->http.content);
    ogs_sbi_header_set(response->http.headers, OGS_SBI_CONTENT_TYPE, OGS_SBI_CONTENT_PROBLEM_TYPE);
    cJSON_Delete(json);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));
}

static const char *problem_cause(ogs_sbi_message_t *recvmsg)
{
    return recvmsg->ProblemDetails ? recvmsg->ProblemDetails->cause : NULL;
}

bool smf_mbs_pcf_create_send(smf_mbs_sess_t *mbs_sess, ogs_sbi_stream_t *stream)
{
    ogs_assert(mbs_sess);
    ogs_assert(stream);

    if (!pcf_send(mbs_sess, SMF_MBS_PCF_OP_CREATE, stream, smf_npcf_mbspolicycontrol_build_create, NULL)) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_GATEWAY_TIMEOUT, NULL, "Cannot discover",
                "MBS Session Create failed, no PCF could be reached for the MBS policy", NULL);
        return false;
    }
    return true;
}

/* The Individual MBS Policy resource of the 201: the mbsPolicyId is the last component of its Location (TS 29.537
 * cl. 5.2.2.2.2: "a Location header field containing the URI of the created "Individual MBS Policy" resource") */
static bool take_policy_id(smf_mbs_sess_t *mbs_sess, ogs_sbi_message_t *recvmsg)
{
    ogs_sbi_message_t location_message;
    ogs_sbi_header_t header;
    bool ok = false;

    if (!recvmsg->http.location)
        return false;

    memset(&header, 0, sizeof(header));
    header.uri = recvmsg->http.location;
    if (ogs_sbi_parse_header(&location_message, &header) != OGS_OK)
        return false;

    if (location_message.h.resource.component[1]) {
        if (mbs_sess->pcf_policy_id)
            ogs_free(mbs_sess->pcf_policy_id);
        mbs_sess->pcf_policy_id = ogs_strdup(location_message.h.resource.component[1]);
        ogs_assert(mbs_sess->pcf_policy_id);
        ok = true;
    }
    ogs_sbi_header_free(&header);
    return ok;
}

static void create_answered(smf_mbs_sess_t *mbs_sess, ogs_sbi_message_t *recvmsg)
{
    smf_mbs_pcf_failure_action_e action = SMF_MBS_PCF_FAILURE_ACTION_NONE;
    smf_mbs_pcf_answer_t answer;
    ogs_sbi_stream_t *stream = pcf_take_stream(mbs_sess);

    smf_mbs_policy_control_enabled(&action);
    smf_mbs_pcf_classify(SMF_MBS_PCF_OP_CREATE, recvmsg->res_status, problem_cause(recvmsg), action, &answer);

    if (answer.outcome == SMF_MBS_PCF_OUTCOME_OK && !take_policy_id(mbs_sess, recvmsg)) {
        /* without the URI the association cannot be updated or deleted later */
        ogs_error("MBS session mbsSessionRef[%s]: the 201 of the PCF has no Location with an mbsPolicyId",
                mbs_sess->mbs_session_ref);
        answer.outcome = SMF_MBS_PCF_OUTCOME_REJECT;
        answer.status = OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR;
        answer.cause = NMBSMF_MBSSESSION_SYSTEM_FAILURE;
        answer.title = "Internal Server Error";
    }

    if (answer.outcome == SMF_MBS_PCF_OUTCOME_ALLOW)
        ogs_warn("MBS session mbsSessionRef[%s]: the PCF denied the MBS policy context (MBS_POLICY_CONTEXT_DENIED); "
                "smf.mbsPolicyControl.failureAction is allow, the session is created without an MBS Policy Association",
                mbs_sess->mbs_session_ref);

    if (answer.outcome == SMF_MBS_PCF_OUTCOME_REJECT) {
        ogs_error("MBS session mbsSessionRef[%s]: the PCF answered %d [%s]", mbs_sess->mbs_session_ref,
                recvmsg->res_status, problem_cause(recvmsg) ? problem_cause(recvmsg) : "");
        if (stream)
            smf_mbs_pcf_send_error(stream, &answer, "MBS Session Create failed, the PCF refused the MBS policy",
                    recvmsg->AcceptableMbsServInfo);
        smf_mbs_sess_release(mbs_sess);
        return;
    }

    if (!stream) {
        /* the consumer is gone: nothing is waiting for the session */
        ogs_error("MBS session mbsSessionRef[%s]: the MBS Session Create request is no longer there",
                mbs_sess->mbs_session_ref);
        smf_mbs_sess_release(mbs_sess);
        return;
    }

    smf_nmbsmf_mbs_session_create_continue(mbs_sess, stream);
}

void smf_mbs_pcf_handle_response(smf_mbs_sess_t *mbs_sess, int xact_state, ogs_sbi_message_t *recvmsg)
{
    ogs_assert(mbs_sess);
    ogs_assert(recvmsg);

    if (mbs_sess->pcf_pending != (smf_mbs_pcf_op_e)xact_state) {
        ogs_warn("MBS session mbsSessionRef[%s]: an answer of the PCF to a request that is no longer waited for",
                mbs_sess->mbs_session_ref);
        return;
    }

    switch (mbs_sess->pcf_pending) {
    case SMF_MBS_PCF_OP_CREATE:
        create_answered(mbs_sess, recvmsg);
        break;
    default:
        ogs_error("MBS session mbsSessionRef[%s]: unknown exchange with the PCF", mbs_sess->mbs_session_ref);
        pcf_exchange_end(mbs_sess);
        break;
    }
}

void smf_mbs_pcf_timeout(ogs_pool_id_t mbs_sess_id, int serial)
{
    smf_mbs_sess_t *mbs_sess = smf_mbs_sess_find_by_id(mbs_sess_id);
    smf_mbs_pcf_op_e op;
    ogs_sbi_stream_t *stream;

    if (!mbs_sess || !mbs_sess->pcf_pending || mbs_sess->pcf_serial != serial)
        return;

    op = mbs_sess->pcf_pending;
    ogs_error("MBS session mbsSessionRef[%s]: no answer of the PCF in time", mbs_sess->mbs_session_ref);
    stream = pcf_take_stream(mbs_sess);

    switch (op) {
    case SMF_MBS_PCF_OP_CREATE:
        if (stream)
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_GATEWAY_TIMEOUT, NULL, "Cannot receive SBI message",
                    "MBS Session Create failed, the PCF did not answer", NULL);
        smf_mbs_sess_release(mbs_sess);
        break;
    default:
        break;
    }
}
