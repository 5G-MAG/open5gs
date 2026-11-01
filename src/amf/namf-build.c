/*
 * Copyright (C) 2019,2020 by Sukchan Lee <acetcom@gmail.com>
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

#include "namf-build.h"

static char* ogs_guti_to_string(ogs_nas_5gs_guti_t *nas_guti)
{
    ogs_plmn_id_t plmn_id;
    char plmn_id_buff[OGS_PLMNIDSTRLEN];
    char *amf_id = NULL;
    char *tmsi = NULL;
    char *guti = NULL;

    ogs_assert(nas_guti);

    memset(&plmn_id, 0, sizeof(plmn_id));
    ogs_nas_to_plmn_id(&plmn_id, &nas_guti->nas_plmn_id);
    amf_id = ogs_amf_id_to_string(&nas_guti->amf_id);
    tmsi = ogs_uint32_to_0string(nas_guti->m_tmsi);

    guti = ogs_msprintf("5g-guti-%s%s%s",
            ogs_plmn_id_to_string(&plmn_id, plmn_id_buff),
            amf_id,
            tmsi);

    /* TS29.518 6.1.3.2.2 Guti pattern (27 or 28 characters):
    "5g-guti-[0-9]{5,6}[0-9a-fA-F]{14}" */
    ogs_assert(strlen(guti) == (OGS_MAX_5G_GUTI_LEN - 1) ||
            (strlen(guti)) == OGS_MAX_5G_GUTI_LEN);

    ogs_free(amf_id);
    ogs_free(tmsi);

    return guti;
}

static char* amf_ue_to_context_id(amf_ue_t *amf_ue)
{
    char *ue_context_id = NULL;

    if (amf_ue->supi) {
        ue_context_id = ogs_strdup(amf_ue->supi);
    } else {
        ue_context_id = ogs_guti_to_string(&amf_ue->old_guti);
    }

    return ue_context_id;
}

ogs_sbi_request_t *amf_namf_comm_build_ue_context_transfer(
        amf_ue_t *amf_ue, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    OpenAPI_ue_context_transfer_req_data_t UeContextTransferReqData;
    char *ue_context_id = NULL;

    ogs_assert(amf_ue);

    ue_context_id = amf_ue_to_context_id(amf_ue);
    ogs_assert(ue_context_id);

    memset(&UeContextTransferReqData, 0, sizeof(UeContextTransferReqData));
    UeContextTransferReqData.access_type = amf_ue->nas.access_type;
    UeContextTransferReqData.reason = amf_ue->nas.registration.value;

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.service.name = (char *)OGS_SBI_SERVICE_NAME_NAMF_COMM;
    message.h.api.version = (char *)OGS_SBI_API_V1;
    message.h.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_UE_CONTEXTS;
    message.h.resource.component[1] = ue_context_id;
    message.h.resource.component[2] = (char *)OGS_SBI_RESOURCE_NAME_TRANSFER;
    message.UeContextTransferReqData = &UeContextTransferReqData;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    if (ue_context_id)
        ogs_free(ue_context_id);

    return request;
}

ogs_sbi_request_t *amf_namf_comm_build_registration_status_update(
        amf_ue_t *amf_ue, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    OpenAPI_ue_reg_status_update_req_data_t UeRegStatusUpdateReqData;
    char *ue_context_id = NULL;

    ogs_assert(amf_ue);
    ogs_assert(data);

    ue_context_id = ogs_guti_to_string(&amf_ue->old_guti);
    ogs_assert(ue_context_id);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.service.name = (char *)OGS_SBI_SERVICE_NAME_NAMF_COMM;
    message.h.api.version = (char *)OGS_SBI_API_V1;
    message.h.resource.component[0] =
            (char *)OGS_SBI_RESOURCE_NAME_UE_CONTEXTS;
    message.h.resource.component[1] = ue_context_id;
    message.h.resource.component[2] =
            (char *)OGS_SBI_RESOURCE_NAME_TRANSFER_UPDATE;
    message.UeRegStatusUpdateReqData = &UeRegStatusUpdateReqData;

    memset(&UeRegStatusUpdateReqData, 0, sizeof(UeRegStatusUpdateReqData));

    UeRegStatusUpdateReqData.transfer_status = OGS_POINTER_TO_UINT(data);
    /*
     * TS 29.518
     * 5.2.2.2.2 Registration Status Update
     * If any network slice(s) become no longer available and there are PDU
     * Session(s) associated with them, the target AMF shall include these
     * PDU session(s) in the toReleaseSessionList attribute in the payload.
     */
    if (UeRegStatusUpdateReqData.transfer_status ==
                OpenAPI_ue_context_transfer_status_TRANSFERRED) {
        ogs_assert(amf_ue->to_release_session_list); /* For safety */
        if (amf_ue->to_release_session_list->count) {
            UeRegStatusUpdateReqData.to_release_session_list =
                    amf_ue->to_release_session_list;
        }
    }

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    if (ue_context_id)
        ogs_free(ue_context_id);

    return request;
}

/* Namf_MBSBroadcast Service - shared helpers */

OpenAPI_mbs_session_id_t *amf_namf_build_mbs_session_id(const ogs_tmgi_t *tmgi)
{
    char *mcc, *mnc;
    OpenAPI_plmn_id_t *plmn_id = NULL;
    OpenAPI_tmgi_t *Tmgi = NULL;

    ogs_assert(tmgi);
    ogs_assert(tmgi->mbs_service_id);

    mcc = ogs_plmn_id_mcc_string(&tmgi->plmn_id);
    mnc = ogs_plmn_id_mnc_string(&tmgi->plmn_id);
    plmn_id = OpenAPI_plmn_id_create(mcc, mnc);

    Tmgi = OpenAPI_tmgi_create(ogs_strdup(tmgi->mbs_service_id), plmn_id);

    // TODO: fill NID when present (see the ContextCreate request builder's own identical TODO)
    return OpenAPI_mbs_session_id_create(Tmgi, NULL, NULL);
}

/*
 * Content-IDs of the binary parts that carry the transfer IEs of a message's n2MbsSmInfoList, one per entry. The
 * Content-ID is only a reference from N2MbsSmInfo.ngapData.contentId to its part (TS 29.518 cl. 6.5.2.4).
 */
static const char *n2_mbs_sm_info_content_id[OGS_SBI_MAX_NUM_OF_PART] = {
    OGS_SBI_CONTENT_NGAP_SM_ID "-0", OGS_SBI_CONTENT_NGAP_SM_ID "-1",
    OGS_SBI_CONTENT_NGAP_SM_ID "-2", OGS_SBI_CONTENT_NGAP_SM_ID "-3",
    OGS_SBI_CONTENT_NGAP_SM_ID "-4", OGS_SBI_CONTENT_NGAP_SM_ID "-5",
    OGS_SBI_CONTENT_NGAP_SM_ID "-6", OGS_SBI_CONTENT_NGAP_SM_ID "-7",
};

static OpenAPI_global_ran_node_id_t *build_global_ran_node_id(
        const ogs_plmn_id_t *plmn_id, const uint8_t *gnb_id_buf, size_t gnb_id_buf_size, uint8_t gnb_id_bit_length)
{
    OpenAPI_gnb_id_t *gnb_id = NULL;
    char *gnb_value = NULL;
    int size = (gnb_id_bit_length + 7) / 8;

    /* TS 29.571 GNbId.gNBValue: the gNB ID in hexadecimal digits; bitLength: its length in bits */
    if (size == 0 || size > (int)gnb_id_buf_size)
        return NULL;

    gnb_value = ogs_calloc(1, OGS_KEYSTRLEN(size));
    ogs_assert(gnb_value);
    ogs_hex_to_ascii(gnb_id_buf, size, gnb_value, OGS_KEYSTRLEN(size));

    gnb_id = OpenAPI_gnb_id_create(gnb_id_bit_length, gnb_value);
    ogs_assert(gnb_id);

    return OpenAPI_global_ran_node_id_create(
            ogs_sbi_build_plmn_id((ogs_plmn_id_t *)plmn_id), NULL, gnb_id, NULL, NULL, NULL, NULL, NULL);
}

static OpenAPI_global_ran_node_id_t *build_ran_id(amf_mbs_n2_info_t *n2_info)
{
    return build_global_ran_node_id(&n2_info->ran_plmn_id,
            n2_info->ran_gnb_id_buf, sizeof(n2_info->ran_gnb_id_buf), n2_info->ran_gnb_id_bit_length);
}

bool amf_namf_ran_id_is_gnb(const amf_gnb_t *gnb, OpenAPI_global_ran_node_id_t *ran_id)
{
    OpenAPI_global_ran_node_id_t *mine = NULL;
    cJSON *a = NULL, *b = NULL;
    char *text_a = NULL, *text_b = NULL;
    bool same = false;

    ogs_assert(gnb);

    if (!ran_id)
        return false;
    mine = build_global_ran_node_id(&gnb->plmn_id, gnb->gnb_id_buf, sizeof(gnb->gnb_id_buf), gnb->gnb_id_bit_length);
    if (!mine)
        return false;

    a = OpenAPI_global_ran_node_id_convertToJSON(mine);
    b = OpenAPI_global_ran_node_id_convertToJSON(ran_id);
    if (a && b) {
        text_a = cJSON_PrintUnformatted(a);
        text_b = cJSON_PrintUnformatted(b);
        same = text_a && text_b && strcmp(text_a, text_b) == 0;
    }
    if (text_a) ogs_free(text_a);
    if (text_b) ogs_free(text_b);
    if (a) cJSON_Delete(a);
    if (b) cJSON_Delete(b);
    OpenAPI_global_ran_node_id_free(mine);
    return same;
}

bool amf_namf_ran_id_list_has_gnb(const OpenAPI_list_t *ran_id_list, const amf_gnb_t *gnb)
{
    OpenAPI_lnode_t *node = NULL;

    if (!ran_id_list)
        return false;

    OpenAPI_list_for_each(ran_id_list, node) {
        if (amf_namf_ran_id_is_gnb(gnb, node->data))
            return true;
    }

    return false;
}

/*
 * Builds the n2MbsSmInfoList of a ContextCreate or ContextUpdate response or of a ContextStatusNotify request from
 * \p n2_info_list (amf_mbs_n2_info_t items) and adds each transfer IE to \p message as a binary part. At most
 * OGS_SBI_MAX_NUM_OF_PART entries are carried (the rest are dropped with a warning). Returns NULL when there is none.
 * The parts reference the pkbufs of the items, so \p n2_info_list must outlive the message's use. Free the result with
 * amf_namf_free_n2_mbs_sm_info_list().
 */
OpenAPI_list_t *amf_namf_build_n2_mbs_sm_info_list(ogs_sbi_message_t *message, ogs_list_t *n2_info_list)
{
    OpenAPI_list_t *list = NULL;
    amf_mbs_n2_info_t *n2_info = NULL;

    ogs_assert(message);
    ogs_assert(n2_info_list);

    ogs_list_for_each(n2_info_list, n2_info) {
        OpenAPI_ref_to_binary_data_t *ngap_data = NULL;

        if (message->num_of_part >= OGS_SBI_MAX_NUM_OF_PART) {
            ogs_warn("n2MbsSmInfoList: more than %d containers, the rest are not sent", OGS_SBI_MAX_NUM_OF_PART);
            break;
        }

        if (!list)
            list = OpenAPI_list_create();
        ogs_assert(list);

        ngap_data = OpenAPI_ref_to_binary_data_create(
                ogs_strdup(n2_mbs_sm_info_content_id[message->num_of_part]));
        OpenAPI_list_add(list, OpenAPI_n2_mbs_sm_info_create(
                    n2_info->ngap_ie_type, ngap_data, build_ran_id(n2_info)));

        message->part[message->num_of_part].pkbuf = n2_info->pkbuf;
        message->part[message->num_of_part].content_id =
            (char *)n2_mbs_sm_info_content_id[message->num_of_part];
        message->part[message->num_of_part].content_type = (char *)OGS_SBI_CONTENT_NGAP_TYPE;
        message->num_of_part++;
    }

    return list;
}

void amf_namf_free_n2_mbs_sm_info_list(OpenAPI_list_t *list)
{
    OpenAPI_lnode_t *node = NULL;

    if (!list)
        return;

    OpenAPI_list_for_each(list, node)
        OpenAPI_n2_mbs_sm_info_free(node->data);
    OpenAPI_list_free(list);
}

/*
 * 3GPP TS 29.518 - V18.14.0
 * Ch. 5.6.2.5 - Namf_MBSBroadcast Service API - ContextStatusNotify service operation
 *
 * Builds the POST request; the caller (amf_sbi_send_mbs_broadcast_context_status_notify(), sbi-path.c)
 * sends it to mbs_context->notify_client. \p n2_info_list holds the containers to carry (may be empty);
 * \p operation_status is OpenAPI_operation_status_NULL when the notification does not report the completion
 * or incompletion of an operation. \p released_ind true sets releasedInd (TS 29.518 V18.14.0 Table 6.5.6.2.4-1:
 * "This shall be present and set to "true" if all NG-RANs serving the Broadcast MBS session requested the AMF to
 * release the Broadcast MBS session and the Broadcast MBS session (context) has been released in the AMF.").
 * \p required_release_gnb, when not NULL, is reported as an operation event of type NG_RAN_EVENT with an
 * ngranFailureEvent NG_RAN_REQUIRED_RELEASE for that NG-RAN (TS 29.518 V18.14.0 cl. 5.6.2.5: "report the Broadcast MBS
 * Session release to the MB-SMF by including the operationEvent attribute in the MBS Context Status Notification
 * request with the opEventType set to "NG_RAN_EVENT" together with a list of "ngranFailureEvent" for each NG-RAN that
 * requested to release the Broadcast MBS Session").
 */
ogs_sbi_request_t *amf_namf_build_mbs_broadcast_context_status_notify(
        amf_mbs_context_t *mbs_context, ogs_list_t *n2_info_list, OpenAPI_operation_status_e operation_status,
        bool released_ind, const amf_gnb_t *required_release_gnb)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    OpenAPI_context_status_notification_t ContextStatusNotification;

    ogs_assert(mbs_context);
    ogs_assert(n2_info_list);

    if (!mbs_context->notify_uri) {
        ogs_error("MBS Broadcast ContextStatusNotify: no notifyUri");
        return NULL;
    }

    memset(&message, 0, sizeof(message));
    message.h.method = (char *) OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = mbs_context->notify_uri;

    memset(&ContextStatusNotification, 0, sizeof(ContextStatusNotification));
    ContextStatusNotification.mbs_session_id = amf_namf_build_mbs_session_id(&mbs_context->tmgi);

    /* TS 29.518 Table 6.5.6.2.4-1, areaSessionId: "This IE shall be present if this is a Location dependent
     * broadcast MBS service." */
    if (mbs_context->location_dependent) {
        ContextStatusNotification.is_area_session_id = true;
        ContextStatusNotification.area_session_id = mbs_context->area_session_id;
    }

    ContextStatusNotification.n2_mbs_sm_info_list =
        amf_namf_build_n2_mbs_sm_info_list(&message, n2_info_list);

    ContextStatusNotification.operation_status = operation_status;

    if (released_ind)
        ContextStatusNotification.released_ind = OpenAPI_context_status_notification_RELEASEDIND__true;

    if (required_release_gnb) {
        OpenAPI_global_ran_node_id_t *ran_id = build_global_ran_node_id(&required_release_gnb->plmn_id,
                required_release_gnb->gnb_id_buf, sizeof(required_release_gnb->gnb_id_buf),
                required_release_gnb->gnb_id_bit_length);

        // The gNB ID is mandatory in an ngranFailureEvent (Table 6.5.6.2.9-1): no event without it.
        if (ran_id) {
            OpenAPI_list_t *events = OpenAPI_list_create();
            OpenAPI_list_t *failure_events = OpenAPI_list_create();

            OpenAPI_list_add(failure_events, OpenAPI_ngran_failure_event_create(ran_id,
                        OpenAPI_ngran_failure_indication_NG_RAN_REQUIRED_RELEASE));
            OpenAPI_list_add(events, OpenAPI_operation_event_create(
                        OpenAPI_op_event_type_NG_RAN_EVENT, NULL, failure_events));
            ContextStatusNotification.operation_events = events;
        } else
            ogs_warn("MBS Broadcast ContextStatusNotify: the gNB ID of the NG-RAN is not known, no operation event");
    }

    message.ContextStatusNotification = &ContextStatusNotification;

    message.http.accept = (char *) (OGS_SBI_CONTENT_JSON_TYPE ","
        OGS_SBI_CONTENT_NGAP_TYPE "," OGS_SBI_CONTENT_PROBLEM_TYPE);

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    if (ContextStatusNotification.mbs_session_id)
        OpenAPI_mbs_session_id_free(ContextStatusNotification.mbs_session_id);
    amf_namf_free_n2_mbs_sm_info_list(ContextStatusNotification.n2_mbs_sm_info_list);
    if (ContextStatusNotification.operation_events) {
        OpenAPI_lnode_t *node = NULL;

        OpenAPI_list_for_each(ContextStatusNotification.operation_events, node)
            OpenAPI_operation_event_free(node->data);
        OpenAPI_list_free(ContextStatusNotification.operation_events);
    }

    return request;
}
