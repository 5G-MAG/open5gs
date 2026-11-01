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
#include "mbs-subsc.h"
#include "mbs-update.h"
#include "nmbsmf-handler.h"
#include "sbi-path.h"

uint32_t smf_mbs_subsc_events_from_list(OpenAPI_list_t *event_list, bool *unsupported)
{
    OpenAPI_lnode_t *node;
    uint32_t events = 0;

    if (unsupported)
        *unsupported = false;
    if (!event_list)
        return 0;

    OpenAPI_list_for_each(event_list, node) {
        OpenAPI_mbs_session_event_t *event = node->data;

        if (event && event->event_type > OpenAPI_mbs_session_event_type_NULL &&
                (SMF_MBS_EVENT(event->event_type) & SMF_MBS_SUPPORTED_EVENTS))
            events |= SMF_MBS_EVENT(event->event_type);
        else if (unsupported)
            *unsupported = true;
    }

    return events;
}

static OpenAPI_list_t *event_list_from_mask(uint32_t events)
{
    OpenAPI_list_t *list = OpenAPI_list_create();
    int type;

    for (type = 1; type < 32; type++)
        if (events & SMF_MBS_EVENT(type))
            OpenAPI_list_add(list, OpenAPI_mbs_session_event_create(type));

    return list;
}

static OpenAPI_mbs_session_id_t *build_mbs_session_id(smf_mbs_sess_t *mbs_sess)
{
    return OpenAPI_mbs_session_id_create(ogs_sbi_build_tmgi(mbs_sess->tmgi), NULL,
            mbs_sess->mbs_session_id.nid ? ogs_strdup(mbs_sess->mbs_session_id.nid) : NULL);
}

OpenAPI_mbs_session_subscription_t *smf_mbs_subsc_build(
        const smf_mbs_subsc_t *subsc, smf_mbs_sess_t *mbs_sess, const char *subscription_uri)
{
    ogs_assert(subsc);
    ogs_assert(mbs_sess);

    /* TS 29.571 V18.12.0 table 5.9.4.7-1: notifyUri, notifyCorrelationId and nfcInstanceId are "Write-Only: true",
     * mbsSessionSubscUri is "Read-Only: true" and "shall be present in the response to an MBS session creation
     * request that includes a subscription" */
    return OpenAPI_mbs_session_subscription_create(
            build_mbs_session_id(mbs_sess),
            mbs_sess->location_dependent, mbs_sess->area_session_id,
            event_list_from_mask(subsc->events),
            NULL, NULL,
            subsc->expiry_time ? ogs_strdup(subsc->expiry_time) : NULL,
            NULL,
            subscription_uri ? ogs_strdup(subscription_uri) : NULL);
}

char *smf_mbs_subsc_uri(ogs_sbi_server_t *server, const smf_mbs_subsc_t *subsc)
{
    ogs_sbi_header_t header;

    if (!server)
        server = ogs_sbi_server_first();
    ogs_assert(server);

    memset(&header, 0, sizeof(header));
    header.service.name = (char *) OGS_SBI_SERVICE_NAME_NMBSMF_MBS_SESSION;
    header.api.version = (char *) OGS_SBI_API_V1;
    header.resource.component[0] = (char *) OGS_SBI_RESOURCE_NAME_MBS_SESSIONS;
    header.resource.component[1] = (char *) OGS_SBI_RESOURCE_NAME_SUBSCRIPTIONS;
    header.resource.component[2] = subsc->id;

    return ogs_sbi_server_uri(server, &header);
}

/* The MBS session a subscription request names: by TMGI, and for a location dependent session by its Area Session
 * ID. TS 29.532 V18.6.0 table 6.2.3.4.3.1-3, 404: "UNKNOWN_MBS_SESSION, if the indicated MBS session is not found
 * in the MB-SMF." and "UNKNOWN_MBS_SERVICE_AREA, if the requested MBS Area Session ID is not found for a location
 * dependent MBS session." */
static smf_mbs_sess_t *find_session(OpenAPI_mbs_session_subscription_t *request, const char **cause,
        const char **detail)
{
    ogs_tmgi_t tmgi;
    smf_mbs_sess_t *mbs_sess = NULL;

    memset(&tmgi, 0, sizeof(tmgi));
    if (request->mbs_session_id && request->mbs_session_id->tmgi)
        ogs_sbi_parse_tmgi(&tmgi, request->mbs_session_id->tmgi);

    if (tmgi.mbs_service_id) {
        mbs_sess = smf_mbs_sess_find_by_tmgi_and_area(&tmgi,
                request->is_area_session_id, (uint16_t)request->area_session_id);
        if (!mbs_sess && request->is_area_session_id && smf_mbs_sess_find_by_tmgi(&tmgi)) {
            *cause = "UNKNOWN_MBS_SERVICE_AREA";
            *detail = "the Area Session ID is not that of a part of the MBS session";
            return NULL;
        }
    }
    if (mbs_sess && mbs_sess->release_triggered)
        mbs_sess = NULL;
    if (!mbs_sess) {
        *cause = NMBSMF_MBSESSION_UNKNOWN_MBS_SESSION;
        *detail = "the MBS session is not known (a subscription is made by the TMGI)";
    }

    return mbs_sess;
}

bool smf_nmbsmf_handle_status_subscribe(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    OpenAPI_mbs_session_subscription_t *request = NULL;
    OpenAPI_status_subscribe_rsp_data_t *StatusSubscribeRspData = NULL;
    smf_mbs_sess_t *mbs_sess = NULL;
    smf_mbs_subsc_t *subsc = NULL;
    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;
    uint32_t events;
    bool unsupported = false;
    const char *cause = NULL, *detail = NULL;
    char *uri = NULL;

    ogs_assert(stream);
    ogs_assert(message);

    /* TS 29.532 V18.6.0 cl. 5.3.2.6.2 step 1: "The content of the POST request (StatusSubscribeReqData data
     * structure) shall contain: the MBS Session ID; Area Session ID, for a location dependent MBS session; the list
     * of MBS session events requested to be subscribed; the Notification URI". */
    if (!message->StatusSubscribeReqData || !message->StatusSubscribeReqData->subscription) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Bad Request",
                "Requested MBS Session StatusSubscribe failed, no subscription", NMBSMF_MBSSESSION_MANDATORY_IE_MISSING);
        return false;
    }
    request = message->StatusSubscribeReqData->subscription;

    if (!request->notify_uri || !request->event_list || request->event_list->count == 0 ||
            !request->mbs_session_id) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Bad Request",
                "Requested MBS Session StatusSubscribe failed, [mbsSessionId], [eventList] and [notifyUri] are mandatory",
                NMBSMF_MBSSESSION_MANDATORY_IE_MISSING);
        return false;
    }

    events = smf_mbs_subsc_events_from_list(request->event_list, &unsupported);
    if (!events) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Error input parameters",
                "Requested MBS Session StatusSubscribe failed, none of the events of [eventList] can be reported",
                NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        return false;
    }

    if (request->expiry_time && !smf_is_date_time(request->expiry_time)) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Error input parameters",
                "Requested MBS Session StatusSubscribe failed, [expiryTime] is not a date-time",
                NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        return false;
    }

    mbs_sess = find_session(request, &cause, &detail);
    if (!mbs_sess) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND, message, "Not Found", detail, cause);
        return false;
    }

    subsc = smf_mbs_subsc_add(mbs_sess, events, request->notify_uri, request->notify_correlation_id,
            request->expiry_time, request->nfc_instance_id);
    if (!subsc) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Error input parameters",
                "Requested MBS Session StatusSubscribe failed, [notifyUri] is not a URI to notify",
                NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        return false;
    }

    /* step 2a: "the MB-SMF shall return a "201 Created" response. The "Location" header shall be present and shall
     * contain the URI of the created resource. The content of the POST response shall include a representation of
     * the created subscription (StatusSubscribeRspData ...): MBS Session ID; Area Session ID, for a location
     * dependent MBS session; the list of MBS session events successfully subscribed; and the expiry time" */
    memset(&sendmsg, 0, sizeof(sendmsg));
    uri = smf_mbs_subsc_uri(ogs_sbi_server_from_stream(stream), subsc);
    sendmsg.http.location = uri;
    StatusSubscribeRspData = OpenAPI_status_subscribe_rsp_data_create(
            smf_mbs_subsc_build(subsc, mbs_sess, NULL), NULL);
    sendmsg.StatusSubscribeRspData = StatusSubscribeRspData;

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_CREATED);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    OpenAPI_status_subscribe_rsp_data_free(StatusSubscribeRspData);
    ogs_free(uri);

    return true;
}

/* TS 29.532 V18.6.0 cl. 5.3.2.6.3 step 1: the changes that may be requested are "Notification URI (callback URI)",
 * "Notification Correlation ID", "New expiration time" and "List of MBS Session events". */
static bool subsc_path_is_modifiable(const char *path)
{
    static const char *const attributes[] = { "/eventList", "/notifyUri", "/notifyCorrelationId", "/expiryTime" };
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(attributes); i++) {
        size_t len = strlen(attributes[i]);

        if (strncmp(path, attributes[i], len) == 0 && (path[len] == '\0' || path[len] == '/'))
            return true;
    }
    return false;
}

static bool subsc_path_is_fixed(const char *path)
{
    static const char *const attributes[] = { "/mbsSessionId", "/areaSessionId", "/nfcInstanceId", "/mbsSessionSubscUri" };
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(attributes); i++) {
        size_t len = strlen(attributes[i]);

        if (strncmp(path, attributes[i], len) == 0 && (path[len] == '\0' || path[len] == '/'))
            return true;
    }
    return false;
}

bool smf_nmbsmf_handle_status_subscribe_patch(
        smf_mbs_subsc_t *subsc, ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    smf_mbs_sess_t *mbs_sess = NULL;
    OpenAPI_lnode_t *node;
    OpenAPI_list_t *modifications = NULL;
    OpenAPI_mbs_session_subscription_t *answer = NULL;
    cJSON *doc = NULL, *member = NULL, *event = NULL;
    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;
    const char *err = NULL;
    uint32_t events = 0;
    const char *notify_uri = NULL, *correlation_id = NULL, *expiry_time = NULL;
    bool ok = false;

    ogs_assert(subsc);
    ogs_assert(stream);
    ogs_assert(message);

    mbs_sess = smf_mbs_sess_find_by_id(subsc->mbs_sess_id);
    if (!mbs_sess || mbs_sess->release_triggered || smf_mbs_subsc_is_expired(subsc)) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND, message, "Not Found",
                "Requested MBS Session StatusSubscribe update failed, the subscription is no longer there", NULL);
        return false;
    }

    if (!message->PatchItemList) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Missing patch list",
                "Requested MBS Session StatusSubscribe update failed, no patch list provided", "INVALID_MSG_FORMAT");
        return false;
    }

    modifications = OpenAPI_list_create();
    OpenAPI_list_for_each(message->PatchItemList, node) {
        OpenAPI_patch_item_t *item = node->data;

        if (!item || !item->path || !item->op) {
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Malformed patch list",
                    "Requested MBS Session StatusSubscribe update failed, patch list malformed", "INVALID_MSG_FORMAT");
            goto done;
        }
        if (subsc_path_is_fixed(item->path)) {
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN, message, "Update forbidden",
                    "Requested MBS Session StatusSubscribe update failed, update not allowed for one or more field paths",
                    "MODIFICATION_NOT_ALLOWED");
            goto done;
        }
        if (!subsc_path_is_modifiable(item->path)) {
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Update path not found",
                    "Requested MBS Session StatusSubscribe update failed, patch path not recognised", "INVALID_MSG_FORMAT");
            goto done;
        }
        OpenAPI_list_add(modifications, item);
    }

    /* the representation the consumer holds, restricted to what it may modify */
    doc = cJSON_CreateObject();
    {
        cJSON *array = cJSON_CreateArray();
        int type;

        for (type = 1; type < 32; type++) {
            if (subsc->events & SMF_MBS_EVENT(type)) {
                cJSON *entry = cJSON_CreateObject();

                cJSON_AddStringToObject(entry, "eventType", OpenAPI_mbs_session_event_type_ToString(type));
                cJSON_AddItemToArray(array, entry);
            }
        }
        cJSON_AddItemToObject(doc, "eventList", array);
    }
    cJSON_AddStringToObject(doc, "notifyUri", subsc->notify_uri);
    if (subsc->notify_correlation_id)
        cJSON_AddStringToObject(doc, "notifyCorrelationId", subsc->notify_correlation_id);
    if (subsc->expiry_time)
        cJSON_AddStringToObject(doc, "expiryTime", subsc->expiry_time);

    err = smf_json_patch_apply(doc, modifications);
    if (err) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Update failed", err,
                "INVALID_MSG_FORMAT");
        goto done;
    }

    /* the result is checked as a subscription request is: the events that can be reported, a URI to notify, a
     * date-time */
    member = cJSON_GetObjectItemCaseSensitive(doc, "eventList");
    if (!cJSON_IsArray(member) || cJSON_GetArraySize(member) == 0) {
        err = "[eventList] has no event";
    } else {
        cJSON_ArrayForEach(event, member) {
            cJSON *type = cJSON_GetObjectItemCaseSensitive(event, "eventType");
            OpenAPI_mbs_session_event_type_e t = OpenAPI_mbs_session_event_type_NULL;

            if (cJSON_IsString(type)) {
                t = OpenAPI_mbs_session_event_type_FromString(type->valuestring);
                if (t > OpenAPI_mbs_session_event_type_NULL && (SMF_MBS_EVENT(t) & SMF_MBS_SUPPORTED_EVENTS))
                    events |= SMF_MBS_EVENT(t);
                else
                    err = "an event of [eventList] cannot be reported";
            } else {
                err = "an entry of [eventList] has no [eventType]";
            }
        }
    }
    member = cJSON_GetObjectItemCaseSensitive(doc, "notifyUri");
    if (!err && !cJSON_IsString(member))
        err = "[notifyUri] is not a URI";
    else if (!err)
        notify_uri = member->valuestring;
    member = cJSON_GetObjectItemCaseSensitive(doc, "notifyCorrelationId");
    if (!err && member && !cJSON_IsString(member))
        err = "[notifyCorrelationId] is not a string";
    else if (!err && member)
        correlation_id = member->valuestring;
    member = cJSON_GetObjectItemCaseSensitive(doc, "expiryTime");
    if (!err && member && (!cJSON_IsString(member) || !smf_is_date_time(member->valuestring)))
        err = "[expiryTime] is not a date-time";
    else if (!err && member)
        expiry_time = member->valuestring;

    if (err) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Error input parameters", err,
                NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        goto done;
    }

    if (strcmp(notify_uri, subsc->notify_uri) != 0 && !smf_mbs_subsc_set_notify_uri(subsc, notify_uri)) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Error input parameters",
                "[notifyUri] is not a URI to notify", NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        goto done;
    }
    subsc->events = events;
    if (subsc->notify_correlation_id) {
        ogs_free(subsc->notify_correlation_id);
        subsc->notify_correlation_id = NULL;
    }
    if (correlation_id)
        subsc->notify_correlation_id = ogs_strdup(correlation_id);
    if (subsc->expiry_time) {
        ogs_free(subsc->expiry_time);
        subsc->expiry_time = NULL;
    }
    if (expiry_time)
        subsc->expiry_time = ogs_strdup(expiry_time);

    /* step 2a: "On success, the MB-SMF shall return a "200 Ok" response with a representation of the modified
     * subscription (MbsSessionSubscription data structure)." */
    memset(&sendmsg, 0, sizeof(sendmsg));
    answer = smf_mbs_subsc_build(subsc, mbs_sess, NULL);
    sendmsg.MbsSessionSubscription = answer;
    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_OK);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));
    OpenAPI_mbs_session_subscription_free(answer);
    ok = true;

done:
    if (doc)
        cJSON_Delete(doc);
    if (modifications)
        OpenAPI_list_free(modifications);
    return ok;
}

bool smf_nmbsmf_handle_status_unsubscribe(
        smf_mbs_subsc_t *subsc, ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(subsc);
    ogs_assert(stream);
    ogs_assert(message);

    smf_mbs_subsc_remove(subsc);

    /* cl. 5.3.2.7.1 step 2: "On success, the MB-SMF shall return a "204 No Content" response." */
    memset(&sendmsg, 0, sizeof(sendmsg));
    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return true;
}

static OpenAPI_list_t *build_ingress_tun_addr_list(smf_mbs_sess_t *mbs_sess)
{
    OpenAPI_list_t *list;
    char buf[OGS_ADDRSTRLEN];
    char *ipv4_str = NULL, *ipv6_str = NULL;

    if (!mbs_sess->ingress_tun_addr)
        return NULL;

    list = OpenAPI_list_create();
    if (mbs_sess->ingress_tun_addr->ogs_sa_family == AF_INET)
        ipv4_str = ogs_strdup(OGS_ADDR(mbs_sess->ingress_tun_addr, buf));
    else if (mbs_sess->ingress_tun_addr->ogs_sa_family == AF_INET6)
        ipv6_str = ogs_strdup(OGS_ADDR(mbs_sess->ingress_tun_addr, buf));
    OpenAPI_list_add(list, OpenAPI_tunnel_address_create(ipv4_str, ipv6_str, OGS_PORT(mbs_sess->ingress_tun_addr)));

    return list;
}

ogs_sbi_request_t *smf_mbs_subsc_build_status_notify(const smf_mbs_subsc_t *subsc, smf_mbs_sess_t *mbs_sess,
        OpenAPI_mbs_session_event_type_e event_type, OpenAPI_broadcast_delivery_status_e status)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    OpenAPI_mbs_session_event_report_t *report = NULL;
    OpenAPI_mbs_session_event_report_list_t *report_list = NULL;
    OpenAPI_status_notify_req_data_t *StatusNotifyReqData = NULL;
    OpenAPI_list_t *reports = NULL;
    OpenAPI_ingress_tun_addr_info_t *ingress_info = NULL;

    ogs_assert(subsc);
    ogs_assert(mbs_sess);

    if (!(subsc->events & SMF_MBS_EVENT(event_type)) || smf_mbs_subsc_is_expired(subsc) || !subsc->notify_uri)
        return NULL;

    /* cl. 5.3.2.8.1 step 1: "The content of the POST request (StatusNotifyReqData data structure) shall contain:
     * Notification Correlation ID, if this information is available in the MBS session status subscription; the list
     * of MBS session events to be reported; When reporting a BROADCAST_DELIVERY_STATUS event: the new broadcast
     * delivery status (e.g. the MBS session has been STARTED or TERMINATED); When reporting a
     * INGRESS_TUNNEL_ADD_CHANGE event: new ingress tunnel address used over the N6mb/Nmb9 reference point." */
    if (event_type == OpenAPI_mbs_session_event_type_INGRESS_TUNNEL_ADD_CHANGE) {
        OpenAPI_list_t *addrs = build_ingress_tun_addr_list(mbs_sess);

        if (!addrs)
            return NULL;
        ingress_info = OpenAPI_ingress_tun_addr_info_create(addrs);
    }
    report = OpenAPI_mbs_session_event_report_create(event_type, ogs_sbi_gmtime_string(ogs_time_now()), ingress_info,
            event_type == OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS ?
                status : OpenAPI_broadcast_delivery_status_NULL);
    reports = OpenAPI_list_create();
    OpenAPI_list_add(reports, report);
    report_list = OpenAPI_mbs_session_event_report_list_create(reports,
            subsc->notify_correlation_id ? ogs_strdup(subsc->notify_correlation_id) : NULL);
    StatusNotifyReqData = OpenAPI_status_notify_req_data_create(report_list);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *) OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = subsc->notify_uri;
    message.StatusNotifyReqData = StatusNotifyReqData;
    message.http.accept = (char *) (OGS_SBI_CONTENT_JSON_TYPE "," OGS_SBI_CONTENT_PROBLEM_TYPE);

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    OpenAPI_status_notify_req_data_free(StatusNotifyReqData);

    return request;
}

static void report_event(smf_mbs_sess_t *mbs_sess, OpenAPI_mbs_session_event_type_e event_type,
        OpenAPI_broadcast_delivery_status_e status)
{
    smf_mbs_subsc_t *subsc = NULL, *next = NULL;

    ogs_assert(mbs_sess);

    ogs_list_for_each_safe(&smf_self()->smf_mbs_subsc_list, next, subsc) {
        ogs_sbi_request_t *request;

        if (subsc->mbs_sess_id != mbs_sess->id)
            continue;
        /* an expired subscription "becomes invalid": it is not kept */
        if (smf_mbs_subsc_is_expired(subsc)) {
            smf_mbs_subsc_remove(subsc);
            continue;
        }
        request = smf_mbs_subsc_build_status_notify(subsc, mbs_sess, event_type, status);
        if (!request)
            continue;
        if (!smf_sbi_send_mbs_status_notify(subsc, request))
            ogs_error("MBS Session StatusNotify to [%s] could not be sent", subsc->notify_uri);
        ogs_sbi_request_free(request);
    }
}

void smf_mbs_sess_report_broadcast_delivery_status(smf_mbs_sess_t *mbs_sess, OpenAPI_broadcast_delivery_status_e status)
{
    report_event(mbs_sess, OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS, status);
}

void smf_mbs_sess_report_ingress_tunnel_change(smf_mbs_sess_t *mbs_sess)
{
    report_event(mbs_sess, OpenAPI_mbs_session_event_type_INGRESS_TUNNEL_ADD_CHANGE,
            OpenAPI_broadcast_delivery_status_NULL);
}
