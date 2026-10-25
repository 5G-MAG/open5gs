/*
 * Copyright (C) 2019-2024 by Sukchan Lee <acetcom@gmail.com>
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

#include "sbi-path.h"
#include "nnrf-handler.h"
#include "nbsf-handler.h"
#include "npcf-handler.h"

void af_state_initial(ogs_fsm_t *s, af_event_t *e)
{
    af_sm_debug(e);

    ogs_assert(s);

    OGS_FSM_TRAN(s, &af_state_operational);
}

void af_state_final(ogs_fsm_t *s, af_event_t *e)
{
    af_sm_debug(e);

    ogs_assert(s);
}

void af_state_operational(ogs_fsm_t *s, af_event_t *e)
{
    int rv;

    af_sess_t *sess = NULL;

    ogs_sbi_stream_t *stream = NULL;
    ogs_pool_id_t stream_id = 0;
    ogs_sbi_request_t *request = NULL;

    ogs_sbi_nf_instance_t *nf_instance = NULL;
    ogs_sbi_subscription_data_t *subscription_data = NULL;
    ogs_sbi_response_t *response = NULL;
    ogs_sbi_message_t message;
    ogs_sbi_xact_t *sbi_xact = NULL;
    ogs_pool_id_t sbi_xact_id = 0;
    int service_name_id = OpenAPI_service_name_NULL;

    af_sm_debug(e);

    ogs_assert(s);

    switch (e->h.id) {
    case OGS_FSM_ENTRY_SIG:
        break;

    case OGS_FSM_EXIT_SIG:
        break;

    case OGS_EVENT_SBI_SERVER:
        request = e->h.sbi.request;
        ogs_assert(request);

        stream_id = OGS_POINTER_TO_UINT(e->h.sbi.data);
        ogs_assert(stream_id >= OGS_MIN_POOL_ID &&
                stream_id <= OGS_MAX_POOL_ID);

        stream = ogs_sbi_stream_find_by_id(stream_id);
        if (!stream) {
            ogs_error("STREAM has already been removed [%d]", stream_id);
            break;
        }

        rv = ogs_sbi_parse_request(&message, request);
        if (rv != OGS_OK) {
            /* 'sbi_message' buffer is released in ogs_sbi_parse_request() */
            ogs_error("cannot parse HTTP sbi_message");
            ogs_assert(true ==
                ogs_sbi_server_send_error(
                    stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                    NULL, "cannot parse HTTP sbi_message", NULL, NULL));
            break;
        }

        if (strcmp(message.h.api.version, OGS_SBI_API_V1) != 0) {
            ogs_error("Not supported version [%s]", message.h.api.version);
            ogs_assert(true ==
                ogs_sbi_server_send_error(
                    stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                    &message, "Not supported version", NULL, NULL));
            ogs_sbi_message_free(&message);
            break;
        }

        service_name_id = ogs_sbi_service_name_id_from_string(
                message.h.service.name);
        switch (service_name_id) {
        case OpenAPI_service_name_nnrf_nfm:

            SWITCH(message.h.resource.component[0])
            CASE(OGS_SBI_RESOURCE_NAME_NF_STATUS_NOTIFY)
                SWITCH(message.h.method)
                CASE(OGS_SBI_HTTP_METHOD_POST)
                    ogs_nnrf_nfm_handle_nf_status_notify(stream, &message);
                    break;

                DEFAULT
                    ogs_error("Invalid HTTP method [%s]", message.h.method);
                    ogs_assert(true ==
                        ogs_sbi_server_send_error(stream,
                            OGS_SBI_HTTP_STATUS_FORBIDDEN, &message,
                            "Invalid HTTP method", message.h.method, NULL));
                END
                break;

            DEFAULT
                ogs_error("Invalid resource name [%s]",
                        message.h.resource.component[0]);
                ogs_assert(true ==
                    ogs_sbi_server_send_error(stream,
                        OGS_SBI_HTTP_STATUS_BAD_REQUEST, &message,
                        "Invalid resource name",
                        message.h.resource.component[0], NULL));
            END
            break;

        case OpenAPI_service_name_npcf_policyauthorization:
            SWITCH(message.h.resource.component[0])
            CASE(OGS_SBI_RESOURCE_NAME_APP_SESSIONS)
                af_sess_t *app_session = NULL;

                if (!message.h.resource.component[1]) {
                    ogs_error("[%s] Unknown app session id",
                            message.h.resource.component[1]);
                    ogs_assert(true ==
                        ogs_sbi_server_send_error(stream,
                            OGS_SBI_HTTP_STATUS_NOT_FOUND,
                            &message, "Not found",
                            message.h.resource.component[1], NULL));
                    break;
                }

                app_session = af_sess_find_by_af_app_session_id(
                            message.h.resource.component[1]);
                if (!app_session) {
                    ogs_error("[%s] Unknown app session id",
                            message.h.resource.component[1]);
                    ogs_assert(true ==
                        ogs_sbi_server_send_error(stream,
                            OGS_SBI_HTTP_STATUS_NOT_FOUND,
                            &message, "Not found",
                            message.h.resource.component[1], NULL));
                    break;
                }

                SWITCH(message.h.resource.component[2])
                CASE(OGS_SBI_RESOURCE_NAME_NOTIFY)
                    /* TS 29.514 V19.8.0 clause 4.2.5.2: the PCF notifies an event with EventsNotification */
                    {
                        cJSON *body = request->http.content ?
                            cJSON_Parse(request->http.content) : NULL;
                        cJSON *notifs = body ?
                            cJSON_GetObjectItem(body, "evNotifs") : NULL;
                        cJSON *first = cJSON_GetArrayItem(notifs, 0);
                        cJSON *event = first ?
                            cJSON_GetObjectItem(first, "event") : NULL;
                        cJSON *flow = first ? cJSON_GetArrayItem(
                                cJSON_GetObjectItem(first, "flows"), 0) : NULL;
                        cJSON *medcomp = flow ?
                            cJSON_GetObjectItem(flow, "medCompN") : NULL;
                        cJSON *failed = body ? cJSON_GetArrayItem(
                                cJSON_GetObjectItem(body,
                                    "failedResourcAllocReports"), 0) : NULL;
                        cJSON *status = failed ?
                            cJSON_GetObjectItem(failed, "mcResourcStatus") :
                            NULL;

                        ogs_assert(body);
                        ogs_assert(cJSON_IsString(
                                    cJSON_GetObjectItem(body, "evSubsUri")));
                        app_session->notify_count++;
                        if (cJSON_IsString(event) && !strcmp(event->valuestring, "QOS_MONITORING")) {
                            cJSON *qm = cJSON_GetArrayItem(cJSON_GetObjectItem(body, "qosMonReports"), 0);
                            cJSON *qflow = qm ? cJSON_GetArrayItem(cJSON_GetObjectItem(qm, "flows"), 0) : NULL;
                            cJSON *qmed = qflow ? cJSON_GetObjectItem(qflow, "medCompN") : NULL;
                            cJSON *d;

                            ogs_assert(qm);
                            app_session->notify_qm_count++;
                            d = cJSON_GetArrayItem(cJSON_GetObjectItem(qm, "dlDelays"), 0);
                            app_session->notify_qm_dl = cJSON_IsNumber(d) ? (long)d->valuedouble : -1;
                            d = cJSON_GetArrayItem(cJSON_GetObjectItem(qm, "ulDelays"), 0);
                            app_session->notify_qm_ul = cJSON_IsNumber(d) ? (long)d->valuedouble : -1;
                            d = cJSON_GetArrayItem(cJSON_GetObjectItem(qm, "rtDelays"), 0);
                            app_session->notify_qm_rt = cJSON_IsNumber(d) ? (long)d->valuedouble : -1;
                            app_session->notify_qm_med_comp_n = cJSON_IsNumber(qmed) ? qmed->valueint : -1;
                        }
                        if (cJSON_IsString(event) && !strcmp(event->valuestring, "L4S_SUPP")) {
                            cJSON *l4s = cJSON_GetArrayItem(cJSON_GetObjectItem(body, "l4sReports"), 0);
                            cJSON *ltype = l4s ? cJSON_GetObjectItem(l4s, "notifType") : NULL;
                            cJSON *lflow = l4s ? cJSON_GetArrayItem(cJSON_GetObjectItem(l4s, "flows"), 0) : NULL;
                            cJSON *lmed = lflow ? cJSON_GetObjectItem(lflow, "medCompN") : NULL;

                            ogs_assert(cJSON_IsString(ltype));
                            app_session->notify_l4s_count++;
                            ogs_cpystrn(app_session->notify_l4s_type, ltype->valuestring,
                                    sizeof(app_session->notify_l4s_type));
                            app_session->notify_l4s_med_comp_n = cJSON_IsNumber(lmed) ? lmed->valueint : -1;
                        }
                        if (cJSON_IsString(event) && !strcmp(event->valuestring, "QOS_NOTIF")) {
                            cJSON *qnc = cJSON_GetArrayItem(cJSON_GetObjectItem(body, "qncReports"), 0);
                            cJSON *qtype = qnc ? cJSON_GetObjectItem(qnc, "notifType") : NULL;
                            cJSON *qflow = qnc ? cJSON_GetArrayItem(cJSON_GetObjectItem(qnc, "flows"), 0) : NULL;
                            cJSON *qmed = qflow ? cJSON_GetObjectItem(qflow, "medCompN") : NULL;

                            ogs_assert(cJSON_IsString(qtype));
                            app_session->notify_qnc_count++;
                            ogs_cpystrn(app_session->notify_qnc_type, qtype->valuestring,
                                    sizeof(app_session->notify_qnc_type));
                            app_session->notify_qnc_med_comp_n = cJSON_IsNumber(qmed) ? qmed->valueint : -1;
                        }
                        if (cJSON_IsString(event) && !strcmp(event->valuestring, "USAGE_REPORT")) {
                            cJSON *usg = cJSON_GetObjectItem(body, "usgRep");
                            cJSON *total = usg ? cJSON_GetObjectItem(usg, "totalVolume") : NULL;

                            ogs_assert(cJSON_IsObject(usg));
                            app_session->notify_usg_count++;
                            app_session->notify_usg_total = cJSON_IsNumber(total) ? (long)total->valuedouble : -1;
                        }
                        ogs_cpystrn(app_session->notify_event,
                                cJSON_IsString(event) ? event->valuestring : "",
                                sizeof(app_session->notify_event));
                        app_session->notify_med_comp_n =
                            cJSON_IsNumber(medcomp) ? medcomp->valueint : -1;
                        ogs_cpystrn(app_session->notify_failed_status,
                                cJSON_IsString(status) ?
                                    status->valuestring : "",
                                sizeof(app_session->notify_failed_status));
                        cJSON_Delete(body);
                    }
                    ogs_assert(true ==
                            ogs_sbi_send_http_status_no_content(stream));
                    break;
                CASE(OGS_SBI_RESOURCE_NAME_TERMINATE)
                    /*
                     * TS 29.514 clause 4.2.5.3: the PCF provides TerminationInfo
                     * with resUri and termCause
                     */
                    ogs_assert(message.TerminationInfo);
                    ogs_assert(message.TerminationInfo->res_uri);
                    ogs_assert(strstr(message.TerminationInfo->res_uri,
                                message.h.resource.component[1]));
                    ogs_assert(message.TerminationInfo->term_cause ==
                            OpenAPI_termination_cause_PDU_SESSION_TERMINATION);
                    ogs_assert(true ==
                            ogs_sbi_send_http_status_no_content(stream));
                    af_sess_remove(app_session);
                    break;
                DEFAULT
                    ogs_error("Invalid resource name [%s]",
                            message.h.resource.component[2]);
                    ogs_assert(true ==
                        ogs_sbi_server_send_error(stream,
                            OGS_SBI_HTTP_STATUS_BAD_REQUEST, &message,
                            "Invalid resource name",
                            message.h.resource.component[2], NULL));
                END
                break;
            DEFAULT
                ogs_error("Invalid resource name [%s]",
                        message.h.resource.component[0]);
                ogs_assert(true ==
                    ogs_sbi_server_send_error(stream,
                        OGS_SBI_HTTP_STATUS_BAD_REQUEST, &message,
                        "Invalid resource name",
                        message.h.resource.component[0], NULL));
            END
            break;

        default:
            ogs_error("Invalid API name [%s]", message.h.service.name);
            ogs_assert(true ==
                ogs_sbi_server_send_error(stream,
                    OGS_SBI_HTTP_STATUS_BAD_REQUEST, &message,
                    "Invalid API name", message.h.service.name, NULL));
        }

        /* In lib/sbi/server.c, notify_completed() releases 'request' buffer. */
        ogs_sbi_message_free(&message);
        break;

    case OGS_EVENT_SBI_CLIENT:
        ogs_assert(e);

        response = e->h.sbi.response;
        ogs_assert(response);
        rv = ogs_sbi_parse_response(&message, response);
        if (rv != OGS_OK) {
            ogs_error("cannot parse HTTP response");
            ogs_sbi_message_free(&message);
            ogs_sbi_response_free(response);
            break;
        }

        if (strcmp(message.h.api.version, OGS_SBI_API_V1) != 0) {
            ogs_error("Not supported version [%s]", message.h.api.version);
            ogs_sbi_message_free(&message);
            ogs_sbi_response_free(response);
            break;
        }

        service_name_id = ogs_sbi_service_name_id_from_string(
                message.h.service.name);
        switch (service_name_id) {
        case OpenAPI_service_name_nnrf_nfm:

            SWITCH(message.h.resource.component[0])
            CASE(OGS_SBI_RESOURCE_NAME_NF_INSTANCES)
                nf_instance = e->h.sbi.data;
                ogs_assert(nf_instance);
                ogs_assert(OGS_FSM_STATE(&nf_instance->sm));

                e->h.sbi.message = &message;
                ogs_fsm_dispatch(&nf_instance->sm, e);
                break;

            CASE(OGS_SBI_RESOURCE_NAME_SUBSCRIPTIONS)
                subscription_data = e->h.sbi.data;
                ogs_assert(subscription_data);

                SWITCH(message.h.method)
                CASE(OGS_SBI_HTTP_METHOD_POST)
                    if (message.res_status == OGS_SBI_HTTP_STATUS_CREATED ||
                        message.res_status == OGS_SBI_HTTP_STATUS_OK) {
                        ogs_nnrf_nfm_handle_nf_status_subscribe(
                                subscription_data, &message);
                    } else {
                        ogs_error("HTTP response error : %d",
                                message.res_status);
                    }
                    break;

                CASE(OGS_SBI_HTTP_METHOD_PATCH)
                    if (message.res_status == OGS_SBI_HTTP_STATUS_OK ||
                        message.res_status == OGS_SBI_HTTP_STATUS_NO_CONTENT) {
                        ogs_nnrf_nfm_handle_nf_status_update(
                                subscription_data, &message);
                    } else {
                        ogs_error("[%s] HTTP response error [%d]",
                                subscription_data->id ?
                                    subscription_data->id : "Unknown",
                                message.res_status);
                    }
                    break;

                CASE(OGS_SBI_HTTP_METHOD_DELETE)
                    if (message.res_status == OGS_SBI_HTTP_STATUS_NO_CONTENT)
                        ogs_info("[%s] Subscription deleted",
                                subscription_data->id ?
                                    subscription_data->id : "Unknown");
                    else
                        ogs_error("[%s] HTTP response error [%d]",
                                subscription_data->id ?
                                    subscription_data->id : "Unknown",
                                message.res_status);

                    ogs_sbi_subscription_data_remove(subscription_data);
                    break;


                DEFAULT
                    ogs_error("Invalid HTTP method [%s]", message.h.method);
                    ogs_assert_if_reached();
                END
                break;
            
            DEFAULT
                ogs_error("Invalid resource name [%s]",
                        message.h.resource.component[0]);
                ogs_assert_if_reached();
            END
            break;

        case OpenAPI_service_name_nnrf_disc:
            SWITCH(message.h.resource.component[0])
            CASE(OGS_SBI_RESOURCE_NAME_NF_INSTANCES)
                sbi_xact_id = OGS_POINTER_TO_UINT(e->h.sbi.data);
                ogs_assert(sbi_xact_id >= OGS_MIN_POOL_ID &&
                        sbi_xact_id <= OGS_MAX_POOL_ID);

                sbi_xact = ogs_sbi_xact_find_by_id(sbi_xact_id);
                if (!sbi_xact) {
                    /* CLIENT_WAIT timer could remove SBI transaction
                     * before receiving SBI message */
                    ogs_error("SBI transaction has already been removed");
                    break;
                }

                SWITCH(message.h.method)
                CASE(OGS_SBI_HTTP_METHOD_GET)
                    if (message.res_status == OGS_SBI_HTTP_STATUS_OK)
                        af_nnrf_handle_nf_discover(sbi_xact, &message);
                    else
                        ogs_error("HTTP response error [%d]",
                                message.res_status);
                    break;

                DEFAULT
                    ogs_error("Invalid HTTP method [%s]", message.h.method);
                    ogs_assert_if_reached();
                END
                break;

            DEFAULT
                ogs_error("Invalid resource name [%s]",
                        message.h.resource.component[0]);
                ogs_assert_if_reached();
            END
            break;

        case OpenAPI_service_name_nbsf_management:
            SWITCH(message.h.resource.component[0])
            CASE(OGS_SBI_RESOURCE_NAME_PCF_BINDINGS)
                sbi_xact_id = OGS_POINTER_TO_UINT(e->h.sbi.data);
                ogs_assert(sbi_xact_id >= OGS_MIN_POOL_ID &&
                        sbi_xact_id <= OGS_MAX_POOL_ID);

                sbi_xact = ogs_sbi_xact_find_by_id(sbi_xact_id);
                if (!sbi_xact) {
                    /* CLIENT_WAIT timer could remove SBI transaction
                     * before receiving SBI message */
                    ogs_error("SBI transaction has already been removed");
                    break;
                }

                sess = (af_sess_t *)sbi_xact->sbi_object;
                ogs_assert(sess);

                ogs_sbi_xact_remove(sbi_xact);

                SWITCH(message.h.method)
                CASE(OGS_SBI_HTTP_METHOD_GET)
                    if (message.res_status == OGS_SBI_HTTP_STATUS_OK)
                        af_nbsf_management_handle_pcf_binding(sess, &message);
                    else
                        ogs_error("HTTP response error [%d]",
                                message.res_status);
                    break;

                DEFAULT
                    ogs_error("Invalid HTTP method [%s]", message.h.method);
                    ogs_assert_if_reached();
                END
                break;
            DEFAULT
                ogs_error("Invalid resource name [%s]",
                        message.h.resource.component[0]);
                ogs_assert_if_reached();
            END
            break;

        case OpenAPI_service_name_npcf_bdtpolicycontrol:
            sess = e->h.sbi.data;
            ogs_assert(sess);
            sess->last_bdt_status = message.res_status;
            sess->last_bdt_cause[0] = '\0';
            sess->last_bdt_location[0] = '\0';
            sess->last_bdt_body[0] = '\0';
            if (message.ProblemDetails && message.ProblemDetails->cause)
                ogs_cpystrn(sess->last_bdt_cause, message.ProblemDetails->cause, sizeof(sess->last_bdt_cause));
            if (message.http.location)
                ogs_cpystrn(sess->last_bdt_location, message.http.location, sizeof(sess->last_bdt_location));
            if (e->h.sbi.response && e->h.sbi.response->http.content)
                ogs_cpystrn(sess->last_bdt_body, e->h.sbi.response->http.content, sizeof(sess->last_bdt_body));
            break;

        case OpenAPI_service_name_npcf_policyauthorization:
            SWITCH(message.h.resource.component[0])
            CASE(OGS_SBI_RESOURCE_NAME_APP_SESSIONS)
                sess = e->h.sbi.data;
                ogs_assert(sess);
                sess->last_pcf_status = message.res_status;
                sess->last_pcf_cause[0] = '\0';
                if (message.ProblemDetails && message.ProblemDetails->cause)
                    ogs_cpystrn(sess->last_pcf_cause, message.ProblemDetails->cause,
                            sizeof(sess->last_pcf_cause));

                if (message.h.resource.component[1]) {
                    if (message.h.resource.component[2]) {
                        SWITCH(message.h.resource.component[2])
                        CASE(OGS_SBI_RESOURCE_NAME_DELETE)
                            /* TS 29.514 clause 5.3.3.4.2.2: a 200 carries evsNotif, whose events are recorded before the session goes */
                            sess->last_pcf_asc_events[0] = '\0';
                            sess->delete_usg_total = -1;
                            if (message.res_status == OGS_SBI_HTTP_STATUS_OK &&
                                e->h.sbi.response && e->h.sbi.response->http.content) {
                                cJSON *asc = cJSON_Parse(e->h.sbi.response->http.content);
                                cJSON *notifs = asc ? cJSON_GetObjectItemCaseSensitive(
                                        cJSON_GetObjectItemCaseSensitive(asc, "evsNotif"), "evNotifs") : NULL;
                                cJSON *usg = asc ? cJSON_GetObjectItemCaseSensitive(
                                        cJSON_GetObjectItemCaseSensitive(asc, "evsNotif"), "usgRep") : NULL;
                                cJSON *notif = NULL;

                                if (cJSON_IsObject(usg)) {
                                    cJSON *total = cJSON_GetObjectItemCaseSensitive(usg, "totalVolume");

                                    sess->delete_usg_total = cJSON_IsNumber(total) ? (long)total->valuedouble : -1;
                                }

                                cJSON_ArrayForEach(notif, notifs) {
                                    const cJSON *name = cJSON_GetObjectItemCaseSensitive(notif, "event");

                                    if (!cJSON_IsString(name))
                                        continue;
                                    if (sess->last_pcf_asc_events[0])
                                        strncat(sess->last_pcf_asc_events, ",",
                                                sizeof(sess->last_pcf_asc_events) -
                                                strlen(sess->last_pcf_asc_events) - 1);
                                    strncat(sess->last_pcf_asc_events, name->valuestring,
                                            sizeof(sess->last_pcf_asc_events) -
                                            strlen(sess->last_pcf_asc_events) - 1);
                                }
                                if (asc)
                                    cJSON_Delete(asc);
                            }
                            af_sess_remove(sess);
                            break;
                        CASE(OGS_SBI_RESOURCE_NAME_EVENTS_SUBSCRIPTION)
                            /* the status is recorded above; the body is not read */
                            break;
                        DEFAULT
                            ogs_error("Invalid resource name [%s]",
                                    message.h.resource.component[2]);
                            ogs_assert_if_reached();
                        END
                    } else {
                        SWITCH(message.h.method)
                        CASE(OGS_SBI_HTTP_METHOD_GET)
                            sess->last_pcf_asc_af_app_id[0] = '\0';
                            sess->last_pcf_asc_qos_ref[0] = '\0';
                            sess->last_pcf_asc_events[0] = '\0';
                            if (message.res_status == OGS_SBI_HTTP_STATUS_OK &&
                                message.AppSessionContext &&
                                message.AppSessionContext->asc_req_data) {
                                OpenAPI_app_session_context_req_data_t *asc =
                                    message.AppSessionContext->asc_req_data;
                                OpenAPI_lnode_t *node = NULL;

                                if (asc->ev_subsc) {
                                    OpenAPI_list_for_each(asc->ev_subsc->events,
                                            node) {
                                        OpenAPI_af_event_subscription_t *ev =
                                            node->data;
                                        const char *name = ev ?
                                            OpenAPI_npcf_af_event_ToString(ev->event) :
                                            NULL;

                                        if (!name)
                                            continue;
                                        if (sess->last_pcf_asc_events[0])
                                            strncat(sess->last_pcf_asc_events,
                                                    ",", sizeof(sess->last_pcf_asc_events) -
                                                    strlen(sess->last_pcf_asc_events) - 1);
                                        strncat(sess->last_pcf_asc_events, name,
                                                sizeof(sess->last_pcf_asc_events) -
                                                strlen(sess->last_pcf_asc_events) - 1);
                                    }
                                }
                                if (asc->af_app_id)
                                    ogs_cpystrn(sess->last_pcf_asc_af_app_id,
                                            asc->af_app_id,
                                            sizeof(sess->last_pcf_asc_af_app_id));
                                OpenAPI_list_for_each(asc->med_components, node) {
                                    OpenAPI_map_t *map = node->data;
                                    OpenAPI_media_component_t *mc =
                                        map ? map->value : NULL;

                                    if (mc && mc->qos_reference) {
                                        ogs_cpystrn(sess->last_pcf_asc_qos_ref,
                                                mc->qos_reference,
                                                sizeof(sess->last_pcf_asc_qos_ref));
                                        break;
                                    }
                                }
                            }
                            break;
                        CASE(OGS_SBI_HTTP_METHOD_PATCH)
                            if (message.res_status == OGS_SBI_HTTP_STATUS_OK)
                                af_npcf_policyauthorization_handle_update(
                                        sess, &message);
                            else
                                ogs_error("HTTP response error [%d]",
                                        message.res_status);
                            break;
                        DEFAULT
                            ogs_error("Invalid HTTP method [%s]",
                                    message.h.method);
                            ogs_assert_if_reached();
                        END
                    }
                } else {
                    SWITCH(message.h.method)
                    CASE(OGS_SBI_HTTP_METHOD_POST)
                        if (message.res_status == OGS_SBI_HTTP_STATUS_CREATED)
                            af_npcf_policyauthorization_handle_create(
                                    sess, &message);
                        else if (message.res_status ==
                                OGS_SBI_HTTP_STATUS_FORBIDDEN ||
                                message.res_status ==
                                OGS_SBI_HTTP_STATUS_BAD_REQUEST)
        /*
         * 400 and 403 may be expected in negative tests, such as unknown
         * qosReference. Do not report them as an AF test error here.
         */
                            ogs_warn("HTTP response error [%d]",
                                    message.res_status);
                        else
                            ogs_error("HTTP response error [%d]",
                                    message.res_status);
                        break;
                    DEFAULT
                        ogs_error("Invalid HTTP method [%s]", message.h.method);
                        ogs_assert_if_reached();
                    END
                }
                break;
            DEFAULT
                ogs_error("Invalid resource name [%s]",
                        message.h.resource.component[0]);
                ogs_assert_if_reached();
            END
            break;

        default:
            ogs_error("Invalid service name [%s]", message.h.service.name);
            ogs_assert_if_reached();
        }

        ogs_sbi_message_free(&message);
        ogs_sbi_response_free(response);
        break;

    case OGS_EVENT_SBI_TIMER:
        ogs_assert(e);

        switch(e->h.timer_id) {
        case OGS_TIMER_NF_INSTANCE_REGISTRATION_INTERVAL:
        case OGS_TIMER_NF_INSTANCE_HEARTBEAT_INTERVAL:
        case OGS_TIMER_NF_INSTANCE_NO_HEARTBEAT:
        case OGS_TIMER_NF_INSTANCE_VALIDITY:
            nf_instance = e->h.sbi.data;
            ogs_assert(nf_instance);
            ogs_assert(OGS_FSM_STATE(&nf_instance->sm));

            ogs_fsm_dispatch(&nf_instance->sm, e);
            if (OGS_FSM_CHECK(&nf_instance->sm, ogs_sbi_nf_state_exception))
                ogs_error("State machine exception [%d]", e->h.timer_id);
            break;

        case OGS_TIMER_SUBSCRIPTION_VALIDITY:
            subscription_data = e->h.sbi.data;
            ogs_assert(subscription_data);

            ogs_assert(true ==
                ogs_nnrf_nfm_send_nf_status_subscribe(
                    ogs_sbi_self()->nf_instance->nf_type,
                    subscription_data->req_nf_instance_id,
                    subscription_data->subscr_cond.nf_type,
                    subscription_data->subscr_cond.service_name));

            ogs_error("[%s] Subscription validity expired",
                subscription_data->id);
            ogs_sbi_subscription_data_remove(subscription_data);
            break;

        case OGS_TIMER_SUBSCRIPTION_PATCH:
            subscription_data = e->h.sbi.data;
            ogs_assert(subscription_data);

            ogs_assert(true ==
                ogs_nnrf_nfm_send_nf_status_update(subscription_data));

            ogs_info("[%s] Need to update Subscription",
                    subscription_data->id);
            break;

        case OGS_TIMER_SBI_CLIENT_WAIT:
            /*
             * ogs_pollset_poll() receives the time of the expiration
             * of next timer as an argument. If this timeout is
             * in very near future (1 millisecond), and if there are
             * multiple events that need to be processed by ogs_pollset_poll(),
             * these could take more than 1 millisecond for processing,
             * resulting in the timer already passed the expiration.
             *
             * In case that another NF is under heavy load and responds
             * to an SBI request with some delay of a few seconds,
             * it can happen that ogs_pollset_poll() adds SBI responses
             * to the event list for further processing,
             * then ogs_timer_mgr_expire() is called which will add
             * an additional event for timer expiration. When all events are
             * processed one-by-one, the SBI xact would get deleted twice
             * in a row, resulting in a crash.
             *
             * 1. ogs_pollset_poll()
             *    message was received and put into an event list,
             * 2. ogs_timer_mgr_expire()
             *    add an additional event for timer expiration
             * 3. message event is processed. (free SBI xact)
             * 4. timer expiration event is processed. (double-free SBI xact)
             *
             * To avoid double-free SBI xact,
             * we need to check ogs_sbi_xact_find_by_id()
             */
            sbi_xact_id = OGS_POINTER_TO_UINT(e->h.sbi.data);
            ogs_assert(sbi_xact_id >= OGS_MIN_POOL_ID &&
                    sbi_xact_id <= OGS_MAX_POOL_ID);

            sbi_xact = ogs_sbi_xact_find_by_id(sbi_xact_id);
            if (!sbi_xact) {
                ogs_error("SBI transaction has already been removed");
                break;
            }

            if (sbi_xact->assoc_stream_id >= OGS_MIN_POOL_ID &&
                sbi_xact->assoc_stream_id <= OGS_MAX_POOL_ID)
                stream = ogs_sbi_stream_find_by_id(sbi_xact->assoc_stream_id);
            /* Here, we should not use ogs_assert(stream)
             * since 'namf-comm' service has no an associated stream. */

            ogs_sbi_xact_remove(sbi_xact);

            ogs_error("Cannot receive SBI message");
            if (stream) {
                ogs_assert(true ==
                    ogs_sbi_server_send_error(stream,
                        OGS_SBI_HTTP_STATUS_GATEWAY_TIMEOUT, NULL,
                        "Cannot receive SBI message", NULL, NULL));
            }
            break;

        default:
            ogs_error("Unknown timer[%s:%d]",
                    ogs_timer_get_name(e->h.timer_id), e->h.timer_id);
        }
        break;

    case AF_EVENT_SBI_LOCAL:
        ogs_assert(e);

        switch(e->local_id) {
        case AF_LOCAL_DISCOVER_AND_SEND:
            af_sbi_discover_and_send(e->local.service_name, NULL,
                    e->local.build, e->sess, e->local.data);
            break;
        case AF_LOCAL_SEND_TO_PCF:
            af_sbi_send_to_pcf(e->sess, e->local.data, e->local.build);
            break;
        default:
            ogs_error("Unknown local[%s:%d]",
                    af_local_get_name(e->local_id), e->local_id);
        }
        break;

    default:
        ogs_error("No handler for event %s", af_event_get_name(e));
        break;
    }
}
