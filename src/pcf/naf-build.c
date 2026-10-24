/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
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

#include "naf-build.h"

ogs_sbi_request_t *pcf_naf_callback_build_policyauthorization_terminate(
        pcf_app_t *app_session, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_header_t header;
    ogs_sbi_request_t *request = NULL;
    ogs_sbi_server_t *server = NULL;
    char *res_uri = NULL;
    OpenAPI_termination_cause_e term_cause =
        (OpenAPI_termination_cause_e)(uintptr_t)data;

    ogs_assert(app_session);

    server = ogs_list_first(&ogs_sbi_self()->server_list);
    ogs_assert(server);

    /* TS 29.514 clause 4.2.5.3: the body is TerminationInfo, termCause and resUri */
    memset(&header, 0, sizeof(header));
    header.service.name = OpenAPI_service_name_ToString(
            OpenAPI_service_name_npcf_policyauthorization);
    header.api.version = (char *)OGS_SBI_API_V1;
    header.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_APP_SESSIONS;
    header.resource.component[1] = (char *)app_session->app_session_id;
    res_uri = ogs_sbi_server_uri(server, &header);
    ogs_assert(res_uri);

    memset(&message, 0, sizeof(message));
    message.TerminationInfo =
        OpenAPI_termination_info_create(term_cause, res_uri);
    ogs_assert(message.TerminationInfo);
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = ogs_msprintf("%s/%s",
                app_session->notif_uri, OGS_SBI_RESOURCE_NAME_TERMINATE);
    if (!message.h.uri) {
        ogs_error("No message.h.uri");
        goto end;
    }

    request = ogs_sbi_build_request(&message);
    ogs_assert(request);

end:

    if (message.h.uri)
        ogs_free(message.h.uri);
    /* res_uri is owned here, the model only points at it */
    if (message.TerminationInfo) {
        message.TerminationInfo->res_uri = NULL;
        OpenAPI_termination_info_free(message.TerminationInfo);
    }
    ogs_free(res_uri);

    return request;
}

ogs_sbi_request_t *pcf_naf_callback_build_policyauthorization_event_notify(
        pcf_app_t *app_session, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    cJSON *events_notification = data;
    char *text = NULL;

    ogs_assert(app_session);
    ogs_assert(app_session->ev_notif_uri);
    ogs_assert(events_notification);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = ogs_msprintf("%s/%s",
                app_session->ev_notif_uri, OGS_SBI_RESOURCE_NAME_NOTIFY);
    if (!message.h.uri) {
        ogs_error("No message.h.uri");
        return NULL;
    }

    request = ogs_sbi_build_request(&message);
    ogs_free(message.h.uri);
    if (!request)
        return NULL;

    /* The body is the EventsNotification data type */
    text = cJSON_Print(events_notification);
    ogs_assert(text);
    ogs_sbi_header_set(request->http.headers, OGS_SBI_CONTENT_TYPE,
            OGS_SBI_CONTENT_JSON_TYPE);
    request->http.content = text;
    request->http.content_length = strlen(text);

    return request;
}
