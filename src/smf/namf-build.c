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
#include "gsm-build.h"
#include "ngap-build.h"

ogs_sbi_request_t *smf_namf_comm_build_n1_n2_message_transfer(
        smf_sess_t *sess, smf_n1_n2_message_transfer_param_t *param)
{
    int i;
    smf_ue_t *smf_ue = NULL;

    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    ogs_sbi_server_t *server = NULL;
    ogs_sbi_header_t header;

    OpenAPI_n1_n2_message_transfer_req_data_t N1N2MessageTransferReqData;

    OpenAPI_n1_message_container_t n1MessageContainer;
    OpenAPI_ref_to_binary_data_t n1MessageContent;

    OpenAPI_n2_info_container_t n2InfoContainer;
    OpenAPI_n2_sm_information_t smInfo;
    OpenAPI_n2_info_content_t n2InfoContent;
    OpenAPI_ref_to_binary_data_t ngapData;

    ogs_assert(sess);
    smf_ue = smf_ue_find_by_id(sess->smf_ue_id);
    ogs_assert(smf_ue);
    ogs_assert(smf_ue->supi);

    ogs_assert(param);
    ogs_assert(param->state);
    ogs_assert(param->n1smbuf || param->n2smbuf);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.service.name = (char *)OGS_SBI_SERVICE_NAME_NAMF_COMM;
    message.h.api.version = (char *)OGS_SBI_API_V1;
    message.h.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_UE_CONTEXTS;
    message.h.resource.component[1] = smf_ue->supi;
    message.h.resource.component[2] =
        (char *)OGS_SBI_RESOURCE_NAME_N1_N2_MESSAGES;
    message.N1N2MessageTransferReqData = &N1N2MessageTransferReqData;

    memset(&N1N2MessageTransferReqData, 0, sizeof(N1N2MessageTransferReqData));
    N1N2MessageTransferReqData.is_pdu_session_id = true;
    N1N2MessageTransferReqData.pdu_session_id = sess->psi;

    if (param->n1smbuf) {
        N1N2MessageTransferReqData.n1_message_container = &n1MessageContainer;

        memset(&n1MessageContainer, 0, sizeof(n1MessageContainer));
        n1MessageContainer.n1_message_class = OpenAPI_n1_message_class_SM;
        n1MessageContainer.n1_message_content = &n1MessageContent;

        memset(&n1MessageContent, 0, sizeof(n1MessageContent));
        n1MessageContent.content_id = (char *)OGS_SBI_CONTENT_5GNAS_SM_ID;

        message.part[message.num_of_part].pkbuf = param->n1smbuf;
        message.part[message.num_of_part].content_id =
            (char *)OGS_SBI_CONTENT_5GNAS_SM_ID;
        message.part[message.num_of_part].content_type =
            (char *)OGS_SBI_CONTENT_5GNAS_TYPE;
        message.num_of_part++;
    }

    if (param->n2smbuf) {
        N1N2MessageTransferReqData.n2_info_container = &n2InfoContainer;

        memset(&n2InfoContainer, 0, sizeof(n2InfoContainer));
        n2InfoContainer.n2_information_class = OpenAPI_n2_information_class_SM;
        n2InfoContainer.sm_info = &smInfo;

        memset(&smInfo, 0, sizeof(smInfo));
        smInfo.pdu_session_id = sess->psi;
        smInfo.n2_info_content = &n2InfoContent;

        memset(&n2InfoContent, 0, sizeof(n2InfoContent));
        switch (param->state) {
        case SMF_UE_REQUESTED_PDU_SESSION_ESTABLISHMENT:
        case SMF_NETWORK_TRIGGERED_SERVICE_REQUEST:
            n2InfoContent.ngap_ie_type = OpenAPI_ngap_ie_type_PDU_RES_SETUP_REQ;
            break;
        case SMF_NETWORK_REQUESTED_PDU_SESSION_MODIFICATION:
        case SMF_NETWORK_REQUESTED_QOS_FLOW_MODIFICATION:
            n2InfoContent.ngap_ie_type = OpenAPI_ngap_ie_type_PDU_RES_MOD_REQ;
            break;
        case SMF_NETWORK_REQUESTED_PDU_SESSION_RELEASE:
        case SMF_ERROR_INDICATON_RECEIVED_FROM_5G_AN:
            n2InfoContent.ngap_ie_type = OpenAPI_ngap_ie_type_PDU_RES_REL_CMD;
            break;
        default:
            ogs_fatal("Unexpected state [%d]", param->state);
            ogs_assert_if_reached();
        }
        n2InfoContent.ngap_data = &ngapData;

        memset(&ngapData, 0, sizeof(ngapData));
        ngapData.content_id = (char *)OGS_SBI_CONTENT_NGAP_SM_ID;

        message.part[message.num_of_part].pkbuf = param->n2smbuf;
        message.part[message.num_of_part].content_id =
            (char *)OGS_SBI_CONTENT_NGAP_SM_ID;
        message.part[message.num_of_part].content_type =
            (char *)OGS_SBI_CONTENT_NGAP_TYPE;
        message.num_of_part++;
    }

    if (param->n1n2_failure_txf_notif_uri == true) {
        server = ogs_sbi_server_first();
        ogs_assert(server);

        memset(&header, 0, sizeof(header));
        header.service.name = (char *)OGS_SBI_SERVICE_NAME_NSMF_CALLBACK;
        header.api.version = (char *)OGS_SBI_API_V1;
        header.resource.component[0] =
                (char *)OGS_SBI_RESOURCE_NAME_N1_N2_FAILURE_NOTIFY;
        N1N2MessageTransferReqData.n1n2_failure_txf_notif_uri =
            ogs_sbi_server_uri(server, &header);
        ogs_assert(N1N2MessageTransferReqData.n1n2_failure_txf_notif_uri);
    }

    if (param->skip_ind == true) {
        N1N2MessageTransferReqData.is_skip_ind = true;
        N1N2MessageTransferReqData.skip_ind = true;
    }

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    for (i = 0; i < message.num_of_part; i++)
        if (message.part[i].pkbuf)
            ogs_pkbuf_free(message.part[i].pkbuf);

    if (N1N2MessageTransferReqData.n1n2_failure_txf_notif_uri)
        ogs_free(N1N2MessageTransferReqData.n1n2_failure_txf_notif_uri);

    return request;
}

ogs_sbi_request_t *smf_namf_callback_build_sm_context_status(
        smf_sess_t *sess, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    OpenAPI_sm_context_status_notification_t SmContextStatusNotification;
    OpenAPI_status_info_t StatusInfo;

    ogs_assert(sess);
    ogs_assert(sess->sm_context_status_uri);

    memset(&StatusInfo, 0, sizeof(StatusInfo));
    StatusInfo.resource_status = OpenAPI_resource_status_RELEASED;

    memset(&SmContextStatusNotification, 0,
            sizeof(SmContextStatusNotification));
    SmContextStatusNotification.status_info = &StatusInfo;

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = sess->sm_context_status_uri;

    message.SmContextStatusNotification = &SmContextStatusNotification;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    return request;
}

/* Namf_MBSBroadcast Service API */

ogs_sbi_request_t *smf_namf_build_mbs_broadcast_context_create_request(
        smf_mbs_sess_t *mbs_sess, void *data)
{
    ogs_debug("Building MBS Broadcast ContextCreate request");

    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    OpenAPI_context_create_req_data_t *ContextCreateReqData = NULL;

    char *mcc, *mnc;
    OpenAPI_plmn_id_t *plmn_id = NULL;
    OpenAPI_tmgi_t *tmgi = NULL;
    OpenAPI_mbs_session_id_t *mbs_session_id = NULL;

    // Fill mbsServiceArea / mbsServiceAreaInfoList
    OpenAPI_plmn_id_t *plmn_id_copy = NULL;
    OpenAPI_tai_t *tai_list_item = NULL;
    OpenAPI_list_t *tai_list = NULL;
    OpenAPI_list_t *ncgi_list = NULL;
    OpenAPI_mbs_service_area_t *mbs_service_area = NULL;
    OpenAPI_list_t *mbs_service_area_info_list = NULL;

    OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info = NULL;
    OpenAPI_ref_to_binary_data_t *ngap_data = NULL;

    char *notify_uri = NULL;
    ogs_sbi_server_t *server = NULL;
    ogs_sbi_header_t header;

    ogs_sbi_nf_instance_t *nf_instance = NULL;
    ogs_sbi_nf_info_t *nf_info = NULL;
    ogs_s_nssai_t *self_s_nssai = NULL;
    OpenAPI_snssai_t *snssai = NULL;

    ogs_assert(mbs_sess);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *) OGS_SBI_HTTP_METHOD_POST;
    message.h.service.name = (char *) OGS_SBI_SERVICE_NAME_NAMF_MBS_BC;
    message.h.api.version = (char *) OGS_SBI_API_V1;
    message.h.resource.component[0] = (char *) OGS_SBI_RESOURCE_NAME_MBS_CONTEXTS;

    memset(&ContextCreateReqData, 0, sizeof(ContextCreateReqData));

    // mbsSessionId
    mcc = ogs_plmn_id_mcc_string(&mbs_sess->tmgi->plmn_id);
    mnc = ogs_plmn_id_mnc_string(&mbs_sess->tmgi->plmn_id);
    plmn_id = OpenAPI_plmn_id_create(mcc, mnc);

    tmgi = OpenAPI_tmgi_create(mbs_sess->tmgi->mbs_service_id, plmn_id);

    // TODO (borieher): Fill NID when present
    mbs_session_id = OpenAPI_mbs_session_id_create(tmgi, NULL, NULL);

    // mbsServiceArea / mbsServiceAreaInfoList (mutually exclusive, TS29518_Namf_MBSBroadcast.yaml's own
    // ContextCreateReqData schema)
    // Forward the TAI list and NCGI-TAI list this session's own MBS Session Create request actually
    // carried (mbs_sess->mbs_service_area->tai_list/ncgi_tai_list, populated by
    // smf_nmbsmf_handle_mbs_session_create() from the request's own tai_list/ncgi_list) instead of a
    // synthetic single-entry list: a fabricated area does not reflect what was requested and would
    // send the broadcast to the wrong TAIs/cells.
    tai_list = OpenAPI_list_create();
    if (mbs_sess->mbs_service_area && mbs_sess->mbs_service_area->tai_list) {
        ogs_tai_t *tai = NULL;
        ogs_list_for_each(mbs_sess->mbs_service_area->tai_list, tai) {
            plmn_id_copy = OpenAPI_plmn_id_create(
                    ogs_plmn_id_mcc_string(&tai->plmn_id),
                    ogs_plmn_id_mnc_string(&tai->plmn_id));
            tai_list_item = OpenAPI_tai_create(plmn_id_copy,
                    tai->tac ? ogs_strdup(tai->tac) : NULL,
                    tai->nid ? ogs_strdup(tai->nid) : NULL);
            OpenAPI_list_add(tai_list, tai_list_item);
        }
    }

    if (mbs_sess->mbs_service_area && mbs_sess->mbs_service_area->ncgi_tai_list) {
        ogs_ncgi_tai_t *ncgi_tai = NULL;
        ncgi_list = OpenAPI_list_create();
        ogs_list_for_each(mbs_sess->mbs_service_area->ncgi_tai_list, ncgi_tai) {
            OpenAPI_tai_t *api_tai = OpenAPI_tai_create(
                    OpenAPI_plmn_id_create(
                            ogs_plmn_id_mcc_string(&ncgi_tai->tai.plmn_id),
                            ogs_plmn_id_mnc_string(&ncgi_tai->tai.plmn_id)),
                    ncgi_tai->tai.tac ? ogs_strdup(ncgi_tai->tai.tac) : NULL,
                    ncgi_tai->tai.nid ? ogs_strdup(ncgi_tai->tai.nid) : NULL);
            OpenAPI_list_t *cell_list = OpenAPI_list_create();
            ogs_ncgi_t *ncgi = NULL;
            ogs_list_for_each(&ncgi_tai->cell_list, ncgi) {
                OpenAPI_list_add(cell_list, OpenAPI_ncgi_create(
                        OpenAPI_plmn_id_create(
                                ogs_plmn_id_mcc_string(&ncgi->plmn_id),
                                ogs_plmn_id_mnc_string(&ncgi->plmn_id)),
                        ncgi->nr_cell_id ? ogs_strdup(ncgi->nr_cell_id) : NULL,
                        ncgi->nid ? ogs_strdup(ncgi->nid) : NULL));
            }
            OpenAPI_list_add(ncgi_list, OpenAPI_ncgi_tai_create(api_tai, cell_list));
        }
    }

    if (!tai_list->count && !ncgi_list) {
        ogs_error("MBS Broadcast ContextCreate: no TAI or NCGI in this session's own MBS Service Area; "
                "sending an empty TAI list");
    }

        // Builds mbsServiceAreaInfoList, one entry carrying this session's area_session_id and service area,
        // when the session is location dependent, and mbsServiceArea otherwise. The two are mutually exclusive,
        // and sending mbsServiceArea unconditionally would claim location-independent delivery for every
        // session whatever was requested. See smf_nmbsmf_handle_mbs_session_create() (TS 29.532 cl.5.3.2.2.1).
    if (mbs_sess->location_dependent) {
        mbs_service_area_info_list = OpenAPI_list_create();
        OpenAPI_list_add(mbs_service_area_info_list,
                OpenAPI_mbs_service_area_info_create(mbs_sess->area_session_id,
                        OpenAPI_mbs_service_area_create(ncgi_list, tai_list)));
    } else {
        mbs_service_area = OpenAPI_mbs_service_area_create(ncgi_list, tai_list);
    }

    // n2MbsSmInfo. randId (added to OpenAPI_n2_mbs_sm_info_t by the schema-collision fix this session --
    // see namf-build.c's own account in src/amf/) is NULL here deliberately, not a stopgap: TS 29.518
    // cl.5.6.2.5 only requires it to identify "the NG-RAN node that generated" a container, which only
    // applies to a container carried on a *response* coming from a gNB -- this is the SMF's own outgoing
    // ContextCreate *request*, which has no responding gNB to identify yet.
    ngap_data = OpenAPI_ref_to_binary_data_create(ogs_strdup((char *) OGS_SBI_CONTENT_NGAP_SM_ID));
    n2_mbs_sm_info = OpenAPI_n2_mbs_sm_info_create(OpenAPI_ngap_ie_type_MBS_SES_REQ, ngap_data, NULL);

    // notifyUri
    server = ogs_sbi_server_first();
    ogs_assert(server);

    memset(&header, 0, sizeof(header));
    header.service.name = (char *) OGS_SBI_SERVICE_NAME_NAMF_MBS_BC;
    header.api.version = (char *) OGS_SBI_API_V1;
    header.resource.component[0] =
            (char *) OGS_SBI_RESOURCE_NAME_MBS_CONTEXT_STATUS_NOTIFY;
    notify_uri = ogs_sbi_server_uri(server, &header);
    ogs_assert(notify_uri);

    // snssai
    // Vendored schema, TS29518_Namf_MBSBroadcast.yaml, ContextCreateReqData's own
    // "required" list: "- mbsSessionId\n- n2MbsSmInfo\n- notifyUri\n- snssai" -- this
    // field is mandatory. The Nmbsmf_MBSSession Create request this SMF received has no
    // S-NSSAI field at all (confirmed absent from TS29532_Nmbsmf_MBSSession.yaml's
    // MbsSession schema), so no per-request value exists to forward. This SMF's own
    // configured slice list (smf.yaml's info.s_nssai, the same config
    // smf_context_parse_config() already requires to be non-empty at startup) is a
    // documented, operator-set default; the first configured slice is used (rule 12 --
    // no authority opened this session chooses among several, so this is an engineering
    // default resting on that config, not a guess).
    nf_instance = ogs_sbi_self()->nf_instance;
    ogs_assert(nf_instance);
    ogs_list_for_each(&nf_instance->nf_info_list, nf_info) {
        if (nf_info->nf_type == OpenAPI_nf_type_SMF &&
                nf_info->smf.num_of_slice > 0) {
            self_s_nssai = &nf_info->smf.slice[0].s_nssai;
            break;
        }
    }
    ogs_assert(self_s_nssai);
    snssai = OpenAPI_snssai_create(self_s_nssai->sst,
            self_s_nssai->sd.v != OGS_S_NSSAI_NO_SD_VALUE ?
                ogs_s_nssai_sd_to_string(self_s_nssai->sd) : NULL);

    ContextCreateReqData = OpenAPI_context_create_req_data_create(mbs_session_id, mbs_service_area_info_list,
        mbs_service_area, n2_mbs_sm_info, notify_uri, NULL, 0, snssai, NULL, NULL);

    message.ContextCreateReqData = ContextCreateReqData;

    message.part[message.num_of_part].pkbuf = ngap_build_mbs_session_setup_or_modification_request_transfer(mbs_sess);
    if (message.part[message.num_of_part].pkbuf) {
        message.part[message.num_of_part].content_id = (char *) OGS_SBI_CONTENT_NGAP_SM_ID;
        message.part[message.num_of_part].content_type = (char *) OGS_SBI_CONTENT_NGAP_TYPE;
        message.num_of_part++;
    }

    message.http.accept = (char *) (OGS_SBI_CONTENT_JSON_TYPE ","
        OGS_SBI_CONTENT_NGAP_TYPE "," OGS_SBI_CONTENT_PROBLEM_TYPE);

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

cleanup:
    if (ContextCreateReqData)
        OpenAPI_context_create_req_data_free(ContextCreateReqData);

    return request;
}

/*
 * 3GPP TS 29.518 - Release 17.11.0
 * 5G System; Access and Mobility Management Services; Stage 3
 * Ch. 5.6.2.3 - Namf_MBSBroadcast Service API - MBS Broadcast ContextRelease service operation
 *
 * BUG FIX: this builder did not exist before -- only the ContextCreate builder above did, which is why
 * a broadcast session's AMF/NGAP-side state (and, transitively, the DU's MCCH content and MAC/scheduler
 * resources) was never released. Mirrors the ContextCreate builder's header/request-building pattern.
 */
ogs_sbi_request_t *smf_namf_build_mbs_broadcast_context_delete_request(
        smf_mbs_sess_t *mbs_sess, void *data)
{
    ogs_debug("Building MBS Broadcast ContextDelete request");

    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    ogs_assert(mbs_sess);
    ogs_assert(mbs_sess->mbs_context_ref);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *) OGS_SBI_HTTP_METHOD_DELETE;
    message.h.service.name = (char *) OGS_SBI_SERVICE_NAME_NAMF_MBS_BC;
    message.h.api.version = (char *) OGS_SBI_API_V1;
    message.h.resource.component[0] = (char *) OGS_SBI_RESOURCE_NAME_MBS_CONTEXTS;
    message.h.resource.component[1] = mbs_sess->mbs_context_ref;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    return request;
}
