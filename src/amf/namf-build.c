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
 * 3GPP TS 29.518 - V18.14.0
 * Ch. 5.6.2.5 - Namf_MBSBroadcast Service API - ContextStatusNotify service operation
 *
 * BUG FIX: this operation did not exist at all -- see amf_mbs_context_t.notify_client's own comment
 * (context.h) and item B-2 (Standards2Deployments/projects/rt-mbs/open5gs.md) for the full account.
 * Builds the POST request; the caller (amf_sbi_send_mbs_broadcast_context_status_notify(), sbi-path.c)
 * sends it to mbs_context->notify_client.
 *
 * NOTE: cl.5.6.2.5 requires "the AMF shall insert the identifier of the NG-RAN node that generated
 * [each container] in the corresponding entry of the n2MbsSmInfoList attribute" (randId in the
 * TS29518_Namf_MBSBroadcast.yaml schema). OpenAPI_n2_mbs_sm_info_t now has a rand_id field (fixed by
 * regenerating the model, see the schema-collision fix this session -- TS29532_Nmbsmf_MBSSession.yaml's
 * own N2MbsSmInfo schema had the same name and no randId, and its generation pass was silently clobbering
 * this one's output). Passing NULL here for now regardless: populating a real GlobalRanNodeId needs the
 * responding gNB's own gnb_id/gnb_id_len, which nothing in this AMF currently tracks per-gNB -- a
 * follow-up item, not folded into the model fix itself.
 */
ogs_sbi_request_t *amf_namf_build_mbs_broadcast_context_status_notify(
        amf_mbs_context_t *mbs_context, ogs_pkbuf_t *n2mbssmbuf, bool completed)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    OpenAPI_context_status_notification_t ContextStatusNotification;
    OpenAPI_list_t *n2_mbs_sm_info_list = NULL;
    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info = NULL;
    OpenAPI_ref_to_binary_data_t *ngap_data = NULL;
    OpenAPI_lnode_t *node = NULL;

    ogs_assert(mbs_context);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *) OGS_SBI_HTTP_METHOD_POST;

    memset(&ContextStatusNotification, 0, sizeof(ContextStatusNotification));
    ContextStatusNotification.mbs_session_id = amf_namf_build_mbs_session_id(&mbs_context->tmgi);

    if (n2mbssmbuf) {
        n2_mbs_sm_info_list = OpenAPI_list_create();

        ngap_data = OpenAPI_ref_to_binary_data_create(ogs_strdup((char *) OGS_SBI_CONTENT_NGAP_SM_ID));
        n2_mbs_sm_info = OpenAPI_n2_mbs_sm_info_create(OpenAPI_ngap_ie_type_MBS_SES_RSP, ngap_data, NULL);
        OpenAPI_list_add(n2_mbs_sm_info_list, n2_mbs_sm_info);

        ContextStatusNotification.n2_mbs_sm_info_list = n2_mbs_sm_info_list;

        message.part[message.num_of_part].pkbuf = n2mbssmbuf;
        message.part[message.num_of_part].content_id = (char *) OGS_SBI_CONTENT_NGAP_SM_ID;
        message.part[message.num_of_part].content_type = (char *) OGS_SBI_CONTENT_NGAP_TYPE;
        message.num_of_part++;
    }

    // cl.5.6.2.5: "the operationStatus IE indicating the completion of the Broadcast MBS session
    // establishment or update, if ... a response has been received from all NG-RANs."
    if (completed) {
        ContextStatusNotification.operation_status =
                OpenAPI_operation_status_MBS_SESSION_START_COMPLETE;
    }

    message.ContextStatusNotification = &ContextStatusNotification;

    message.http.accept = (char *) (OGS_SBI_CONTENT_JSON_TYPE ","
        OGS_SBI_CONTENT_NGAP_TYPE "," OGS_SBI_CONTENT_PROBLEM_TYPE);

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    if (ContextStatusNotification.mbs_session_id)
        OpenAPI_mbs_session_id_free(ContextStatusNotification.mbs_session_id);
    if (n2_mbs_sm_info_list) {
        OpenAPI_list_for_each(n2_mbs_sm_info_list, node)
            OpenAPI_n2_mbs_sm_info_free(node->data);
        OpenAPI_list_free(n2_mbs_sm_info_list);
    }

    return request;
}
