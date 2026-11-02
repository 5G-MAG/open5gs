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

/* TS 29.537 cl. 5.2.2.3.2 step 1: "the NF service consumer shall invoke the "Update" resource custom operation by sending
 * an HTTP POST request to the PCF targeting the URI of the corresponding "Individual MBS Policy" resource custom
 * operation, i.e. "{apiRoot}/npcf-mbspolicycontrol/<apiVersion>/mbs-policies/{mbsPolicyId}/update", with the request
 * body including the MbsPolicyCtxtDataUpdate data structure that may contain: the updated MBS Service Information,
 * within the "mbsServInfo" attribute; the MBS Policy Control Request Triggers that are met, within the "mbsPcrts"
 * attribute". */
ogs_sbi_request_t *smf_npcf_mbspolicycontrol_build_update(smf_mbs_sess_t *mbs_sess, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    OpenAPI_mbs_policy_ctxt_data_update_t *update = NULL;
    OpenAPI_list_t *pcrts;

    ogs_assert(mbs_sess);
    ogs_assert(mbs_sess->pcf_policy_id);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.service.name = (char *)OGS_SBI_SERVICE_NAME_NPCF_MBSPOLICYCONTROL;
    message.h.api.version = (char *)OGS_SBI_API_V1;
    message.h.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_MBS_POLICIES;
    message.h.resource.component[1] = mbs_sess->pcf_policy_id;
    message.h.resource.component[2] = (char *)OGS_SBI_RESOURCE_NAME_UPDATE;

    pcrts = OpenAPI_list_create();
    OpenAPI_list_add(pcrts, ogs_strdup("MBS_SESSION_UPDATE"));
    update = OpenAPI_mbs_policy_ctxt_data_update_create(build_mbs_service_info(data), pcrts);
    message.MbsPolicyCtxtDataUpdate = update;

    message.http.accept = (char *)(OGS_SBI_CONTENT_JSON_TYPE "," OGS_SBI_CONTENT_PROBLEM_TYPE);

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    OpenAPI_mbs_policy_ctxt_data_update_free(update);

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
    if (mbs_sess->pcf_patch_json) {
        ogs_free(mbs_sess->pcf_patch_json);
        mbs_sess->pcf_patch_json = NULL;
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

/* The Update */

static bool is_modified_path(const char *path, const char *name)
{
    size_t len = strlen(name);

    return path && !strncmp(path, name, len) && (path[len] == '\0' || path[len] == '/');
}

bool smf_mbs_pcf_update_wanted(smf_mbs_sess_t *mbs_sess, OpenAPI_list_t *patch_items)
{
    OpenAPI_lnode_t *node;
    bool contact = false, no_contact = false, service_info = false;

    ogs_assert(mbs_sess);

    if (!smf_mbs_policy_control_enabled(NULL) || !mbs_sess->pcf_policy_id)
        return false;

    OpenAPI_list_for_each(patch_items, node) {
        OpenAPI_patch_item_t *item = node->data;

        if (!item || !item->path)
            continue;
        if (is_modified_path(item->path, "/contactPcfInd")) {
            if (item->op == OpenAPI_patch_operation_add || item->op == OpenAPI_patch_operation_replace) {
                if (item->value && item->value->json && cJSON_IsTrue(item->value->json))
                    contact = true;
                else if (item->value && item->value->json && cJSON_IsFalse(item->value->json))
                    no_contact = true;
            }
        } else if (is_modified_path(item->path, "/mbsServInfo")) {
            service_info = true;
        }
    }

    return contact || (service_info && !no_contact);
}

bool smf_mbs_pcf_update_send(smf_mbs_sess_t *mbs_sess, ogs_sbi_stream_t *stream, OpenAPI_list_t *patch_items,
        const char *serv_info_json)
{
    OpenAPI_lnode_t *node;
    cJSON *array = cJSON_CreateArray();
    bool service_info = false;

    ogs_assert(mbs_sess);
    ogs_assert(stream);
    ogs_assert(array);

    OpenAPI_list_for_each(patch_items, node) {
        OpenAPI_patch_item_t *item = node->data;
        cJSON *json = OpenAPI_patch_item_convertToJSON(item);

        ogs_assert(json);
        cJSON_AddItemToArray(array, json);
        if (is_modified_path(item->path, "/mbsServInfo"))
            service_info = true;
    }
    if (mbs_sess->pcf_patch_json)
        ogs_free(mbs_sess->pcf_patch_json);
    mbs_sess->pcf_patch_json = cJSON_PrintUnformatted(array);
    ogs_assert(mbs_sess->pcf_patch_json);
    cJSON_Delete(array);

    if (!pcf_send(mbs_sess, SMF_MBS_PCF_OP_UPDATE, stream, smf_npcf_mbspolicycontrol_build_update,
                service_info ? (void *)serv_info_json : NULL)) {
        ogs_free(mbs_sess->pcf_patch_json);
        mbs_sess->pcf_patch_json = NULL;
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_GATEWAY_TIMEOUT, NULL, "Cannot discover",
                "Requested MBS Session Update failed, no PCF could be reached for the MBS policy", NULL);
        return false;
    }
    return true;
}

/* The Update that waited for the PCF, as the list of patch items it was made of; NULL when there is none */
static OpenAPI_list_t *take_patch_items(smf_mbs_sess_t *mbs_sess)
{
    OpenAPI_list_t *items = NULL;
    cJSON *array, *entry;

    if (!mbs_sess->pcf_patch_json)
        return NULL;
    array = cJSON_Parse(mbs_sess->pcf_patch_json);
    ogs_free(mbs_sess->pcf_patch_json);
    mbs_sess->pcf_patch_json = NULL;
    if (!array)
        return NULL;

    items = OpenAPI_list_create();
    cJSON_ArrayForEach(entry, array) {
        OpenAPI_patch_item_t *item = OpenAPI_patch_item_parseFromJSON(entry);

        if (item)
            OpenAPI_list_add(items, item);
    }
    cJSON_Delete(array);
    return items;
}

static void free_patch_items(OpenAPI_list_t *items)
{
    OpenAPI_lnode_t *node;

    if (!items)
        return;
    OpenAPI_list_for_each(items, node)
        OpenAPI_patch_item_free(node->data);
    OpenAPI_list_free(items);
}

static void update_answered(smf_mbs_sess_t *mbs_sess, ogs_sbi_message_t *recvmsg)
{
    smf_mbs_pcf_failure_action_e action = SMF_MBS_PCF_FAILURE_ACTION_NONE;
    smf_mbs_pcf_answer_t answer;
    ogs_sbi_stream_t *stream = pcf_take_stream(mbs_sess);
    OpenAPI_list_t *items = take_patch_items(mbs_sess);

    smf_mbs_policy_control_enabled(&action);
    smf_mbs_pcf_classify(SMF_MBS_PCF_OP_UPDATE, recvmsg->res_status, problem_cause(recvmsg), action, &answer);

    if (answer.outcome == SMF_MBS_PCF_OUTCOME_REJECT) {
        ogs_error("MBS session mbsSessionRef[%s]: the PCF answered %d [%s] to the update of the MBS policy",
                mbs_sess->mbs_session_ref, recvmsg->res_status, problem_cause(recvmsg) ? problem_cause(recvmsg) : "");
        if (stream)
            smf_mbs_pcf_send_error(stream, &answer, "Requested MBS Session Update failed, the PCF refused the MBS policy",
                    recvmsg->AcceptableMbsServInfo);
        free_patch_items(items);
        return;
    }
    if (answer.outcome == SMF_MBS_PCF_OUTCOME_ALLOW)
        ogs_warn("MBS session mbsSessionRef[%s]: the PCF denied the update of the MBS policy context "
                "(MBS_POLICY_CONTEXT_DENIED); smf.mbsPolicyControl.failureAction is allow, the update is applied",
                mbs_sess->mbs_session_ref);

    if (!stream) {
        ogs_error("MBS session mbsSessionRef[%s]: the MBS Session Update request is no longer there",
                mbs_sess->mbs_session_ref);
        free_patch_items(items);
        return;
    }
    if (mbs_sess->release_triggered) {
        /* TS 29.532 V18.6.0 table 6.2.3.3.3.1-3, 404: "UNKNOWN_MBS_SESSION, if the indicated MBS session is not found in
         * the MB-SMF." */
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND, NULL, "Not Found",
                "Requested MBS Session Update failed, the MBS session is being released",
                NMBSMF_MBSESSION_UNKNOWN_MBS_SESSION);
        free_patch_items(items);
        return;
    }
    if (!items) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL, "Internal Server Error",
                "Requested MBS Session Update failed, the update is no longer known", NMBSMF_MBSSESSION_SYSTEM_FAILURE);
        return;
    }

    smf_nmbsmf_mbs_session_patch_apply(mbs_sess, stream, NULL, items, true);
    free_patch_items(items);
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
    case SMF_MBS_PCF_OP_UPDATE:
        update_answered(mbs_sess, recvmsg);
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
    case SMF_MBS_PCF_OP_UPDATE:
        if (mbs_sess->pcf_patch_json) {
            ogs_free(mbs_sess->pcf_patch_json);
            mbs_sess->pcf_patch_json = NULL;
        }
        if (stream)
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_GATEWAY_TIMEOUT, NULL, "Cannot receive SBI message",
                    "Requested MBS Session Update failed, the PCF did not answer", NULL);
        break;
    default:
        break;
    }
}
