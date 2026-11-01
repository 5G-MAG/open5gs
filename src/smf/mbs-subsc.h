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

#ifndef SMF_MBS_SUBSC_H
#define SMF_MBS_SUBSC_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The events of TS 29.571 V18.12.0 MbsSessionEventType that this MB-SMF can report. MBS_REL_TMGI_EXPIRY is not
 * among them: no release is made for the expiry of a TMGI. */
#define SMF_MBS_SUPPORTED_EVENTS \
    (SMF_MBS_EVENT(OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS) | \
     SMF_MBS_EVENT(OpenAPI_mbs_session_event_type_INGRESS_TUNNEL_ADD_CHANGE))

/* The events of an eventList that are supported, as a mask; *unsupported tells whether others were listed. A list
 * without an entry gives 0. */
uint32_t smf_mbs_subsc_events_from_list(OpenAPI_list_t *event_list, bool *unsupported);

/* The MbsSessionSubscription of a subscription as the MB-SMF answers with it: the session, the area, the events
 * and the expiry time (the other attributes are write-only, TS 29.571 table 5.9.4.7-1). With subscription_uri, also
 * mbsSessionSubscUri. */
OpenAPI_mbs_session_subscription_t *smf_mbs_subsc_build(
        const smf_mbs_subsc_t *subsc, smf_mbs_sess_t *mbs_sess, const char *subscription_uri);

/* The URI of the individual subscription resource, as seen from the server of stream (NULL for the first server) */
char *smf_mbs_subsc_uri(ogs_sbi_server_t *server, const smf_mbs_subsc_t *subsc);

/* Nmbsmf_MBSSession StatusSubscribe, StatusUnsubscribe */
bool smf_nmbsmf_handle_status_subscribe(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message);
bool smf_nmbsmf_handle_status_subscribe_patch(
        smf_mbs_subsc_t *subsc, ogs_sbi_stream_t *stream, ogs_sbi_message_t *message);
bool smf_nmbsmf_handle_status_unsubscribe(
        smf_mbs_subsc_t *subsc, ogs_sbi_stream_t *stream, ogs_sbi_message_t *message);

/* StatusNotify of the events of an MBS session to its subscribers (TS 29.532 V18.6.0 cl. 5.3.2.8.1): the new
 * broadcast delivery status, the new ingress tunnel address (the current one of the session). */
void smf_mbs_sess_report_broadcast_delivery_status(smf_mbs_sess_t *mbs_sess, OpenAPI_broadcast_delivery_status_e status);
void smf_mbs_sess_report_ingress_tunnel_change(smf_mbs_sess_t *mbs_sess);

/* The request of a StatusNotify to the subscriber of subsc (NULL if the event is not subscribed, or the
 * subscription expired) */
ogs_sbi_request_t *smf_mbs_subsc_build_status_notify(const smf_mbs_subsc_t *subsc, smf_mbs_sess_t *mbs_sess,
        OpenAPI_mbs_session_event_type_e event_type, OpenAPI_broadcast_delivery_status_e status);

#ifdef __cplusplus
}
#endif

#endif /* SMF_MBS_SUBSC_H */
