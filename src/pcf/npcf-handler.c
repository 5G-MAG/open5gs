/*
 * Copyright (C) 2019-2025 by Sukchan Lee <acetcom@gmail.com>
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

#include "npcf-handler.h"

static cJSON *flows_of_rules(pcf_app_t *app, OpenAPI_list_t *rule_ids);
static void app_usage_apply(pcf_sess_t *sess, pcf_app_t *app, bool thresholds_given);
static bool content_has_evsubsc(const char *content);
static bool sponsored_not_authorized(pcf_sess_t *sess, cJSON *context);
static void app_qnc_decisions(pcf_app_t *app, OpenAPI_list_t *QosDecisionList);
static void app_qnc_trigger(pcf_app_t *app, OpenAPI_sm_policy_decision_t *decision);

static uint8_t pcf_qos_index_from_media(
        const char *qos_reference,
        OpenAPI_media_type_e media_type,
        const char **err_out)
{
    int i;

    ogs_assert(err_out);
    *err_out = NULL;

    if (qos_reference) {
        for (i = 0; i < pcf_self()->num_of_qos_profile; i++) {
            if (!strcmp(pcf_self()->qos_profile[i].reference, qos_reference))
                return pcf_self()->qos_profile[i].qos_index;
        }

        *err_out = "Unknown qosReference - check PCF qos_profiles config";
        return 0;
    }

    switch (media_type) {
    case OpenAPI_media_type_AUDIO:
        return OGS_QOS_INDEX_1;
    case OpenAPI_media_type_VIDEO:
        return OGS_QOS_INDEX_2;
    case OpenAPI_media_type_CONTROL:
        return OGS_QOS_INDEX_5;
    case OpenAPI_media_type_NULL:
        *err_out = "Media-Type is Required";
        return 0;
    default:
        *err_out = "Unknown Media-Type";
        return 0;
    }
}

bool pcf_npcf_am_policy_control_handle_create(pcf_ue_am_t *pcf_ue_am,
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    bool rc;
    int r;

    OpenAPI_policy_association_request_t *PolicyAssociationRequest = NULL;
    OpenAPI_guami_t *Guami = NULL;
    OpenAPI_lnode_t *node = NULL;

    uint64_t supported_features = 0;

    ogs_sbi_server_t *server = NULL;
    ogs_sbi_client_t *client = NULL;
    OpenAPI_uri_scheme_e scheme = OpenAPI_uri_scheme_NULL;
    char *fqdn = NULL;
    uint16_t fqdn_port = 0;
    ogs_sockaddr_t *addr = NULL, *addr6 = NULL;

    ogs_assert(pcf_ue_am);
    ogs_assert(stream);
    server = ogs_sbi_server_from_stream(stream);
    ogs_assert(server);
    ogs_assert(message);

    PolicyAssociationRequest = message->PolicyAssociationRequest;
    if (!PolicyAssociationRequest) {
        ogs_error("[%s] No PolicyAssociationRequest", pcf_ue_am->supi);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                message, "[%s] No PolicyAssociationRequest", pcf_ue_am->supi,
                NULL));
        return false;
    }

    if (!PolicyAssociationRequest->notification_uri) {
        ogs_error("[%s] No notificationUri", pcf_ue_am->supi);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                message, "No notificationUri", pcf_ue_am->supi, NULL));
        return false;
    }

    if (!PolicyAssociationRequest->supi) {
        ogs_error("[%s] No supi", pcf_ue_am->supi);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                message, "No supi", pcf_ue_am->supi, NULL));
        return false;
    }

    if (!PolicyAssociationRequest->supp_feat) {
        ogs_error("[%s] No suppFeat", pcf_ue_am->supi);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                message, "No suppFeat", pcf_ue_am->supi, NULL));
        return false;
    }

    rc = ogs_sbi_getaddr_from_uri(&scheme, &fqdn, &fqdn_port, &addr, &addr6,
            PolicyAssociationRequest->notification_uri);
    if (rc == false || scheme == OpenAPI_uri_scheme_NULL) {
        ogs_error("[%s] Invalid URI [%s]",
                pcf_ue_am->supi, PolicyAssociationRequest->notification_uri);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                message, "[%s] Invalid URI", pcf_ue_am->supi, NULL));
        return false;
    }

    if (pcf_ue_am->notification_uri)
        ogs_free(pcf_ue_am->notification_uri);
    pcf_ue_am->notification_uri = ogs_strdup(
            PolicyAssociationRequest->notification_uri);
    ogs_assert(pcf_ue_am->notification_uri);

    client = ogs_sbi_client_find(scheme, fqdn, fqdn_port, addr, addr6);
    if (!client) {
        ogs_debug("%s: ogs_sbi_client_add()", OGS_FUNC);
        client = ogs_sbi_client_add(scheme, fqdn, fqdn_port, addr, addr6);
        if (!client) {
            ogs_error("%s: ogs_sbi_client_add() failed", OGS_FUNC);

            ogs_free(fqdn);
            ogs_freeaddrinfo(addr);
            ogs_freeaddrinfo(addr6);

            return false;
        }
    }
    OGS_SBI_SETUP_CLIENT(&pcf_ue_am->namf, client);

    ogs_free(fqdn);
    ogs_freeaddrinfo(addr);
    ogs_freeaddrinfo(addr6);

    supported_features =
        ogs_uint64_from_string_hexadecimal(
                PolicyAssociationRequest->supp_feat);
    pcf_ue_am->am_policy_control_features &= supported_features;

    if (PolicyAssociationRequest->gpsi) {
        if (pcf_ue_am->gpsi)
            ogs_free(pcf_ue_am->gpsi);
        pcf_ue_am->gpsi = ogs_strdup(PolicyAssociationRequest->gpsi);
    }

    pcf_ue_am->access_type = PolicyAssociationRequest->access_type;

    if (PolicyAssociationRequest->pei) {
        if (pcf_ue_am->pei)
            ogs_free(pcf_ue_am->pei);
        pcf_ue_am->pei = ogs_strdup(PolicyAssociationRequest->pei);
    }

    Guami = PolicyAssociationRequest->guami;
    if (Guami && Guami->amf_id &&
        Guami->plmn_id && Guami->plmn_id->mnc && Guami->plmn_id->mcc) {
        ogs_sbi_parse_guami(&pcf_ue_am->guami, PolicyAssociationRequest->guami);
    }

    OpenAPI_list_for_each(PolicyAssociationRequest->allowed_snssais, node) {
        struct OpenAPI_snssai_s *Snssai = node->data;
        if (Snssai) {
            ogs_s_nssai_t s_nssai;
            s_nssai.sst = Snssai->sst;
            s_nssai.sd = ogs_s_nssai_sd_from_string(Snssai->sd);

            pcf_metrics_inst_by_slice_add(&pcf_ue_am->guami.plmn_id,
                    &s_nssai, PCF_METR_CTR_PA_POLICYAMASSOREQ, 1);
        } else {
            ogs_error("[%s] No Snssai", pcf_ue_am->supi);
        }
    }

    if (PolicyAssociationRequest->rat_type)
        pcf_ue_am->rat_type = PolicyAssociationRequest->rat_type;

    pcf_ue_am->policy_association_request =
        OpenAPI_policy_association_request_copy(
                pcf_ue_am->policy_association_request,
                message->PolicyAssociationRequest);

    if (PolicyAssociationRequest->ue_ambr)
        pcf_ue_am->subscribed_ue_ambr = OpenAPI_ambr_copy(
                pcf_ue_am->subscribed_ue_ambr,
                PolicyAssociationRequest->ue_ambr);

    if (ogs_sbi_supi_in_vplmn(pcf_ue_am->supi) == true) {
        /* Visited PLMN */
        OpenAPI_policy_association_t PolicyAssociation;

        ogs_sbi_message_t sendmsg;
        ogs_sbi_header_t header;
        ogs_sbi_response_t *response = NULL;

        memset(&PolicyAssociation, 0, sizeof(PolicyAssociation));
        PolicyAssociation.request = pcf_ue_am->policy_association_request;
        PolicyAssociation.supp_feat =
            ogs_uint64_to_string(pcf_ue_am->am_policy_control_features);
        ogs_assert(PolicyAssociation.supp_feat);

        memset(&header, 0, sizeof(header));
        header.service.name =
            OpenAPI_service_name_ToString(
                    OpenAPI_service_name_npcf_am_policy_control);
        header.api.version = (char *)OGS_SBI_API_V1;
        header.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_POLICIES;
        header.resource.component[1] = pcf_ue_am->association_id;

        memset(&sendmsg, 0, sizeof(sendmsg));
        sendmsg.PolicyAssociation = &PolicyAssociation;
        sendmsg.http.location = ogs_sbi_server_uri(server, &header);

        response = ogs_sbi_build_response(
                &sendmsg, OGS_SBI_HTTP_STATUS_CREATED);
        ogs_assert(response);
        ogs_assert(true == ogs_sbi_server_send_response(stream, response));

        ogs_free(sendmsg.http.location);

        ogs_free(PolicyAssociation.supp_feat);

        return true;
    } else {
        /* Home PLMN */
        r = pcf_ue_am_sbi_discover_and_send(OpenAPI_service_name_nudr_dr, NULL,
                pcf_nudr_dr_build_query_am_data, pcf_ue_am, stream, NULL);
        ogs_expect(r == OGS_OK);
        ogs_assert(r != OGS_ERROR);

        return (r == OGS_OK);
    }
}

bool pcf_npcf_smpolicycontrol_handle_create(pcf_sess_t *sess,
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    bool rc;
    int status = 0;
    int r;
    char *strerror = NULL;
    pcf_ue_sm_t *pcf_ue_sm = NULL;

    OpenAPI_sm_policy_context_data_t *SmPolicyContextData = NULL;
    OpenAPI_plmn_id_nid_t *servingNetwork = NULL;
    OpenAPI_snssai_t *sliceInfo = NULL;

    ogs_sbi_client_t *client = NULL;
    OpenAPI_uri_scheme_e scheme = OpenAPI_uri_scheme_NULL;
    char *fqdn = NULL;
    uint16_t fqdn_port = 0;
    ogs_sockaddr_t *addr = NULL, *addr6 = NULL;

    char *dnn_oi = NULL;

    ogs_assert(sess);
    pcf_ue_sm = pcf_ue_sm_find_by_id(sess->pcf_ue_sm_id);
    ogs_assert(stream);
    ogs_assert(message);

    SmPolicyContextData = message->SmPolicyContextData;
    if (!SmPolicyContextData) {
        strerror = ogs_msprintf("[%s:%d] No SmPolicyContextData",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (!SmPolicyContextData->supi) {
        strerror = ogs_msprintf("[%s:%d] No supi", pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (!SmPolicyContextData->pdu_session_id) {
        strerror = ogs_msprintf("[%s:%d] No pduSessionId",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (!SmPolicyContextData->pdu_session_type) {
        strerror = ogs_msprintf("[%s:%d] No pduSessionType",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (!SmPolicyContextData->dnn) {
        strerror = ogs_msprintf("[%s:%d] No dnn", pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (!SmPolicyContextData->notification_uri) {
        strerror = ogs_msprintf("[%s:%d] No notificationUri",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (!SmPolicyContextData->ipv4_address &&
        !SmPolicyContextData->ipv6_address_prefix) {
        strerror = ogs_msprintf("[%s:%d] No IPv4 address or IPv6 prefix",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    sliceInfo = SmPolicyContextData->slice_info;
    if (!sliceInfo) {
        strerror = ogs_msprintf("[%s:%d] No sliceInfo",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    servingNetwork = SmPolicyContextData->serving_network;
    if (servingNetwork) {
        if (!servingNetwork->mcc) {
            strerror = ogs_msprintf("[%s:%d] No servingNetwork->mcc",
                    pcf_ue_sm->supi, sess->psi);
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            goto cleanup;
        }
        if (!servingNetwork->mnc) {
            strerror = ogs_msprintf("[%s:%d] No servingNetwork->mnc",
                    pcf_ue_sm->supi, sess->psi);
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            goto cleanup;
        }
    } else {
        ogs_warn("No servingNetwork");
    }

    rc = ogs_sbi_getaddr_from_uri(&scheme, &fqdn, &fqdn_port, &addr, &addr6,
            SmPolicyContextData->notification_uri);
    if (rc == false || scheme == OpenAPI_uri_scheme_NULL) {
        strerror = ogs_msprintf("[%s:%d] Invalid URI [%s]",
                pcf_ue_sm->supi, sess->psi,
                SmPolicyContextData->notification_uri);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (SmPolicyContextData->gpsi) {
        if (pcf_ue_sm->gpsi)
            ogs_free(pcf_ue_sm->gpsi);
        pcf_ue_sm->gpsi = ogs_strdup(SmPolicyContextData->gpsi);
    }

    if (SmPolicyContextData->supp_feat) {
        uint64_t supported_features =
            ogs_uint64_from_string_hexadecimal(
                    SmPolicyContextData->supp_feat);
        sess->smpolicycontrol_features &= supported_features;
    } else {
        sess->smpolicycontrol_features = 0;
    }

    sess->pdu_session_type = SmPolicyContextData->pdu_session_type;

    /* Serving PLMN & Home PLMN */
    if (servingNetwork) {
        sess->serving.presence = true;
        ogs_sbi_parse_plmn_id_nid(&sess->serving.plmn_id, servingNetwork);

        sess->home.presence = true;
        memcpy(&sess->home.plmn_id, &sess->serving.plmn_id, OGS_PLMN_ID_LEN);
    }

    /*
     * TS29.512
     * 5 Npcf_SMPolicyControl Service API
     * 5.6 Data Model
     * 5.6.2 Structured data types
     * Table 5.6.2.3-1: Definition of type SmPolicyContextData
     *
     * NAME: dnn
     * Data type: Dnn
     * P: M
     * Cardinality: 1
     * The DNN of the PDU session, a full DNN with both the Network Identifier
     * and Operator Identifier, or a DNN with the Network Identifier only
     */
    dnn_oi = ogs_dnn_oi_from_fqdn(SmPolicyContextData->dnn);

    if (dnn_oi) {
        char dnn_ni[OGS_MAX_DNN_LEN+1];
        uint16_t mcc = 0, mnc = 0;

        ogs_assert(dnn_oi > SmPolicyContextData->dnn);

        ogs_cpystrn(dnn_ni, SmPolicyContextData->dnn,
            ogs_min(OGS_MAX_DNN_LEN, dnn_oi - SmPolicyContextData->dnn));

        if (sess->dnn)
            ogs_free(sess->dnn);
        sess->dnn = ogs_strdup(dnn_ni);
        ogs_assert(sess->dnn);

        if (sess->full_dnn)
            ogs_free(sess->full_dnn);
        sess->full_dnn = ogs_strdup(SmPolicyContextData->dnn);
        ogs_assert(sess->full_dnn);

        mcc = ogs_plmn_id_mcc_from_fqdn(sess->full_dnn);
        mnc = ogs_plmn_id_mnc_from_fqdn(sess->full_dnn);

        /*
         * To generate the Home PLMN ID of the SMF-UE,
         * the length of the MNC is obtained
         * by comparing the MNC part of the SUPI and full-DNN.
         */
        if (mcc && mnc &&
            strncmp(pcf_ue_sm->supi, "imsi-", strlen("imsi-")) == 0) {
            int mnc_len = 0;
            char buf[OGS_PLMNIDSTRLEN];

            ogs_snprintf(buf, OGS_PLMNIDSTRLEN, "%03d%02d", mcc, mnc);
            if (strncmp(pcf_ue_sm->supi + 5, buf, strlen(buf)) == 0)
                mnc_len = 2;

            ogs_snprintf(buf, OGS_PLMNIDSTRLEN, "%03d%03d", mcc, mnc);
            if (strncmp(pcf_ue_sm->supi + 5, buf, strlen(buf)) == 0)
                mnc_len = 3;

            /* Change Home PLMN for VPLMN */
            if (mnc_len == 2 || mnc_len == 3) {
                if (sess->home.presence == true)
                    ogs_plmn_id_build(&sess->home.plmn_id, mcc, mnc, mnc_len);
            }
        }
    } else {
        if (sess->dnn)
            ogs_free(sess->dnn);
        sess->dnn = ogs_strdup(SmPolicyContextData->dnn);
        ogs_assert(sess->dnn);

        if (sess->full_dnn)
            ogs_free(sess->full_dnn);
        sess->full_dnn = NULL;
    }

    if (sess->notification_uri)
        ogs_free(sess->notification_uri);
    sess->notification_uri = ogs_strdup(SmPolicyContextData->notification_uri);
    ogs_assert(sess->notification_uri);

    client = ogs_sbi_client_find(scheme, fqdn, fqdn_port, addr, addr6);
    if (!client) {
        ogs_debug("%s: ogs_sbi_client_add()", OGS_FUNC);
        client = ogs_sbi_client_add(scheme, fqdn, fqdn_port, addr, addr6);
        if (!client) {
            strerror = ogs_msprintf("%s: ogs_sbi_client_add() failed",
                    OGS_FUNC);
            status = OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR;
            ogs_freeaddrinfo(addr);
            goto cleanup;
        }
    }
    OGS_SBI_SETUP_CLIENT(&sess->nsmf, client);

    ogs_free(fqdn);
    ogs_freeaddrinfo(addr);
    ogs_freeaddrinfo(addr6);

    if (SmPolicyContextData->ipv4_address) {
        if (pcf_sess_set_ipv4addr(
                    sess, SmPolicyContextData->ipv4_address) == false) {
            strerror = ogs_msprintf("[%s:%d] Invalid ipv4Address [%s]",
                    pcf_ue_sm->supi, sess->psi,
                    SmPolicyContextData->ipv4_address);
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            goto cleanup;
        }
    }

    if (SmPolicyContextData->ipv6_address_prefix) {
        if (pcf_sess_set_ipv6prefix(
                    sess, SmPolicyContextData->ipv6_address_prefix) == false) {
            strerror = ogs_msprintf("[%s:%d] Invalid ipv6AddressPrefix [%s]",
                    pcf_ue_sm->supi, sess->psi,
                    SmPolicyContextData->ipv6_address_prefix);
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            goto cleanup;
        }
    }

    if (SmPolicyContextData->ipv4_frame_route_list) {
        OpenAPI_lnode_t *node = NULL;

        OpenAPI_clear_and_free_string_list(sess->ipv4_frame_route_list);
        sess->ipv4_frame_route_list = OpenAPI_list_create();
        OpenAPI_list_for_each(
                SmPolicyContextData->ipv4_frame_route_list, node) {
            if (!node->data)
                continue;
            OpenAPI_list_add(
                    sess->ipv4_frame_route_list, ogs_strdup(node->data));
        }
    }

    if (SmPolicyContextData->ipv6_frame_route_list) {
        OpenAPI_lnode_t *node = NULL;

        OpenAPI_clear_and_free_string_list(sess->ipv6_frame_route_list);
        sess->ipv6_frame_route_list = OpenAPI_list_create();
        OpenAPI_list_for_each(
                SmPolicyContextData->ipv6_frame_route_list, node) {
            if (!node->data)
                continue;
            OpenAPI_list_add(
                    sess->ipv6_frame_route_list, ogs_strdup(node->data));
        }
    }

    sess->s_nssai.sst = sliceInfo->sst;
    sess->s_nssai.sd = ogs_s_nssai_sd_from_string(sliceInfo->sd);

    pcf_metrics_inst_by_slice_add(
            sess->home.presence == true ? &sess->home.plmn_id : NULL,
            &sess->s_nssai, PCF_METR_GAUGE_PA_SESSIONNBR, 1);
    pcf_metrics_inst_by_slice_add(
            sess->home.presence == true ? &sess->home.plmn_id : NULL,
            &sess->s_nssai, PCF_METR_CTR_PA_POLICYSMASSOREQ, 1);

    if (SmPolicyContextData->subs_sess_ambr)
        sess->subscribed_sess_ambr = OpenAPI_ambr_copy(
            sess->subscribed_sess_ambr, SmPolicyContextData->subs_sess_ambr);

    if (SmPolicyContextData->subs_def_qos)
        sess->subscribed_default_qos = OpenAPI_subscribed_default_qos_copy(
            sess->subscribed_default_qos, SmPolicyContextData->subs_def_qos);

    if (ogs_sbi_supi_in_vplmn(pcf_ue_sm->supi) == true) {
        /* Visited PLMN */
        r = pcf_sess_sbi_discover_and_send(
                    OpenAPI_service_name_nbsf_management, NULL,
                    pcf_nbsf_management_build_register,
                    sess, stream, NULL);
        ogs_expect(r == OGS_OK);
        ogs_assert(r != OGS_ERROR);

        return (r == OGS_OK);
    } else {
        /* Home PLMN */
        r = pcf_sess_sbi_discover_and_send(
                OpenAPI_service_name_nudr_dr, NULL,
                pcf_nudr_dr_build_query_sm_data, sess, stream, NULL);
        ogs_expect(r == OGS_OK);
        ogs_assert(r != OGS_ERROR);

        return (r == OGS_OK);
    }

cleanup:
    ogs_assert(status);
    ogs_assert(strerror);
    ogs_error("%s", strerror);
    /*
     * TS29.512
     * 4.2.2.2 SM Policy Association establishment 
     *
     * If the PCF is, due to incomplete, erroneous or missing
     * information (e.g. QoS, RAT type, subscriber information)
     * not able to provision a policy decision as response to
     * the request for PCC rules by the SMF, the PCF may reject
     * the request and include in an HTTP "400 Bad Request"
     * response message the "cause" attribute of the ProblemDetails
     * data structure set to "ERROR_INITIAL_PARAMETERS". 
     */
    ogs_assert(true ==
            ogs_sbi_server_send_error(stream, status, message,
                    strerror, NULL, "ERROR_INITIAL_PARAMETERS"));
    ogs_free(strerror);

    return false;
}

bool pcf_npcf_smpolicycontrol_handle_delete(pcf_sess_t *sess,
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    int r;
    int status = 0;
    char *strerror = NULL;
    pcf_ue_sm_t *pcf_ue_sm = NULL;
    pcf_app_t *app_session = NULL;

    OpenAPI_sm_policy_delete_data_t *SmPolicyDeleteData = NULL;

    ogs_assert(sess);
    pcf_ue_sm = pcf_ue_sm_find_by_id(sess->pcf_ue_sm_id);
    ogs_assert(stream);
    ogs_assert(message);

    SmPolicyDeleteData = message->SmPolicyDeleteData;
    if (!SmPolicyDeleteData) {
        strerror = ogs_msprintf("[%s:%d] No SmPolicyDeleteData",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    ogs_list_for_each(&sess->app_list, app_session) {
        /* the SMF indicated the termination of the SM Policy Context (TS 29.514 clause 4.2.5.3) */
        pcf_sbi_send_policyauthorization_terminate_notify(app_session,
                OpenAPI_termination_cause_PDU_SESSION_TERMINATION);
    }

    if (pcf_sessions_number_by_snssai_and_dnn(
                pcf_ue_sm, &sess->s_nssai, sess->dnn) > 1) {
        ogs_expect(true ==
                ogs_sbi_send_response(stream, OGS_SBI_HTTP_STATUS_NO_CONTENT));
    } else if (sess->binding.resource_uri) {
        r = pcf_sess_sbi_discover_and_send(
                OpenAPI_service_name_nbsf_management, NULL,
                pcf_nbsf_management_build_de_register, sess, stream, NULL);
        ogs_expect(r == OGS_OK);
        ogs_assert(r != OGS_ERROR);
    } else {
        ogs_expect(true ==
                ogs_sbi_send_response(stream, OGS_SBI_HTTP_STATUS_NO_CONTENT));
    }

    return true;

cleanup:
    ogs_assert(status);
    ogs_assert(strerror);
    ogs_error("%s", strerror);
    ogs_assert(true ==
        ogs_sbi_server_send_error(stream, status, message, strerror, NULL,
                NULL));
    ogs_free(strerror);

    return false;
}

/*
 * TS 29.514 V19.8.0 clause 5.7.3: the application error (the "cause" of the ProblemDetails) of the status codes that this
 * PCF sends to an Npcf_PolicyAuthorization client.
 */
static const char *policyauthorization_cause(int status)
{
    switch (status) {
    case OGS_SBI_HTTP_STATUS_BAD_REQUEST:
        return "INVALID_SERVICE_INFORMATION";
    case OGS_SBI_HTTP_STATUS_FORBIDDEN:
        return "REQUESTED_SERVICE_NOT_AUTHORIZED";
    default:
        return NULL;
    }
}

/* RFC 7396 JSON Merge Patch: the way TS 29.514 V19.8.0 clause 5.3.3.3.2 modifies the resource */
static void json_merge_patch(cJSON *target, const cJSON *patch)
{
    cJSON *item = NULL, *existing = NULL;

    cJSON_ArrayForEach(item, patch) {
        if (cJSON_IsNull(item)) {
            cJSON_DeleteItemFromObject(target, item->string);
        } else if (cJSON_IsObject(item)) {
            existing = cJSON_GetObjectItem(target, item->string);
            if (!cJSON_IsObject(existing)) {
                cJSON_DeleteItemFromObject(target, item->string);
                existing = cJSON_AddObjectToObject(target, item->string);
                ogs_assert(existing);
            }
            json_merge_patch(existing, item);
        } else {
            cJSON_DeleteItemFromObject(target, item->string);
            cJSON_AddItemToObject(target, item->string,
                    cJSON_Duplicate(item, 1));
        }
    }
}

/* Answer with the AppSessionContext that the resource holds */
static void send_app_session_context(
        ogs_sbi_stream_t *stream, pcf_app_t *app_session, int status,
        const char *location)
{
    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;
    OpenAPI_app_session_context_t *AppSessionContext = NULL;

    ogs_assert(app_session);
    ogs_assert(app_session->context);

    AppSessionContext =
        OpenAPI_app_session_context_parseFromJSON(app_session->context);
    ogs_assert(AppSessionContext);

    memset(&sendmsg, 0, sizeof(sendmsg));
    sendmsg.http.location = (char *)location;
    sendmsg.AppSessionContext = AppSessionContext;

    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    OpenAPI_app_session_context_free(AppSessionContext);
}

/*
 * The events that the PCF acts on, read from the AppSessionContext that the resource now holds: the events of the Events
 * Subscription of TS 29.514 V19.8.0 clause 4.2.2.10 and the URI to notify them at (the notifUri of the subscription, else the
 * one of the application session).
 */
static void app_events_refresh(pcf_app_t *app)
{
    cJSON *asc = NULL, *ev_subsc = NULL, *events = NULL, *event = NULL, *uri = NULL;

    ogs_assert(app);

    app->ev_succ_res_alloc = false;
    app->ev_fail_res_alloc = false;
    app->ev_usage_report = false;
    app->ev_qos_notif = false;
    if (app->ev_notif_uri) {
        ogs_free(app->ev_notif_uri);
        app->ev_notif_uri = NULL;
    }
    if (!app->context)
        return;

    asc = cJSON_GetObjectItem(app->context, "ascReqData");
    ev_subsc = asc ? cJSON_GetObjectItem(asc, "evSubsc") : NULL;
    if (!cJSON_IsObject(ev_subsc))
        return;

    events = cJSON_GetObjectItem(ev_subsc, "events");
    cJSON_ArrayForEach(event, events) {
        cJSON *name = cJSON_GetObjectItem(event, "event");

        if (!cJSON_IsString(name))
            continue;
        if (!strcmp(name->valuestring, "SUCCESSFUL_RESOURCES_ALLOCATION"))
            app->ev_succ_res_alloc = true;
        else if (!strcmp(name->valuestring, "FAILED_RESOURCES_ALLOCATION"))
            app->ev_fail_res_alloc = true;
        else if (!strcmp(name->valuestring, "USAGE_REPORT"))
            app->ev_usage_report = true;
        else if (!strcmp(name->valuestring, "QOS_NOTIF"))
            app->ev_qos_notif = true;
    }

    uri = cJSON_GetObjectItem(ev_subsc, "notifUri");
    if (cJSON_IsString(uri))
        app->ev_notif_uri = ogs_strdup(uri->valuestring);
    else
        app->ev_notif_uri = ogs_strdup(app->notif_uri);
}

/*
 * TS 29.512 V19.7.0 clause 4.2.6.5.? (resource allocation outcome): the PCF asks the SMF to confirm that the resources of the
 * PCC rules are allocated with the trigger SUCC_RES_ALLO and one RequestedRuleData that names the rules and carries the
 * value SUCC_RES_ALLO in reqData. The lists belong to the decision until succ_res_alloc_request_free().
 */
static void succ_res_alloc_request(
        OpenAPI_sm_policy_decision_t *SmPolicyDecision,
        OpenAPI_list_t *PccRuleList)
{
    OpenAPI_list_t *triggers = NULL, *ids = NULL, *req_data = NULL;
    OpenAPI_lnode_t *node = NULL;

    ogs_assert(SmPolicyDecision);
    ogs_assert(PccRuleList);

    ids = OpenAPI_list_create();
    ogs_assert(ids);
    OpenAPI_list_for_each(PccRuleList, node) {
        OpenAPI_map_t *PccRuleMap = node->data;

        /* a null value removes the rule: nothing to confirm */
        if (PccRuleMap && PccRuleMap->key && PccRuleMap->value)
            OpenAPI_list_add(ids, ogs_strdup(PccRuleMap->key));
    }
    if (ids->count == 0) {
        OpenAPI_list_free(ids);
        return;
    }

    triggers = OpenAPI_list_create();
    ogs_assert(triggers);
    OpenAPI_list_add(triggers,
            (void *)OpenAPI_policy_control_request_trigger_SUCC_RES_ALLO);
    SmPolicyDecision->policy_ctrl_req_triggers = triggers;

    req_data = OpenAPI_list_create();
    ogs_assert(req_data);
    OpenAPI_list_add(req_data,
            (void *)OpenAPI_requested_rule_data_type_SUCC_RES_ALLO);

    SmPolicyDecision->last_req_rule_data = OpenAPI_list_create();
    ogs_assert(SmPolicyDecision->last_req_rule_data);
    OpenAPI_list_add(SmPolicyDecision->last_req_rule_data,
            OpenAPI_requested_rule_data_create(ids, req_data));
}

static void succ_res_alloc_request_free(
        OpenAPI_sm_policy_decision_t *SmPolicyDecision)
{
    OpenAPI_lnode_t *node = NULL, *node2 = NULL;

    ogs_assert(SmPolicyDecision);

    OpenAPI_list_for_each(SmPolicyDecision->last_req_rule_data, node) {
        OpenAPI_requested_rule_data_t *data = node->data;

        if (!data)
            continue;
        OpenAPI_list_for_each(data->ref_pcc_rule_ids, node2) {
            if (node2->data)
                ogs_free(node2->data);
        }
        OpenAPI_list_free(data->ref_pcc_rule_ids);
        OpenAPI_list_free(data->req_data);
        ogs_free(data);
    }
    OpenAPI_list_free(SmPolicyDecision->last_req_rule_data);
    SmPolicyDecision->last_req_rule_data = NULL;
    OpenAPI_list_free(SmPolicyDecision->policy_ctrl_req_triggers);
    SmPolicyDecision->policy_ctrl_req_triggers = NULL;
}

/*
 * QoS notification control (TS 29.514 V19.8.0 clause 4.2.2.6; TS 29.512 V19.7.0 clause 4.2.6.2.3): the AF subscribed to QOS_NOTIF, so the
 * QoS data of the GBR rules asks the NG-RAN for notification (qnc true; false when the subscription is gone) and the SMF is armed to
 * report with the trigger QOS_NOTIF.
 */
static void app_qnc_decisions(pcf_app_t *app, OpenAPI_list_t *QosDecisionList)
{
    OpenAPI_lnode_t *node = NULL;
    int i;

    OpenAPI_list_for_each(QosDecisionList, node) {
        OpenAPI_map_t *map = node->data;
        OpenAPI_qos_data_t *qos = map ? map->value : NULL;

        if (!qos || !qos->qos_id)
            continue;
        for (i = 0; i < app->num_of_pcc_rule; i++) {
            ogs_pcc_rule_t *rule = &app->pcc_rule[i];

            if (rule->id && !strcmp(rule->id, qos->qos_id) &&
                (rule->qos.gbr.downlink || rule->qos.gbr.uplink)) {
                qos->is_qnc = true;
                qos->qnc = app->ev_qos_notif;
            }
        }
    }
}

static void app_qnc_trigger(pcf_app_t *app, OpenAPI_sm_policy_decision_t *decision)
{
    if (!app->ev_qos_notif)
        return;
    if (!decision->policy_ctrl_req_triggers) {
        decision->policy_ctrl_req_triggers = OpenAPI_list_create();
        ogs_assert(decision->policy_ctrl_req_triggers);
    }
    OpenAPI_list_add(decision->policy_ctrl_req_triggers,
            (void *)OpenAPI_policy_control_request_trigger_QOS_NOTIF);
}

/*
 * Sponsored connectivity and usage monitoring (TS 29.514 V19.8.0 clauses 4.2.2.5, 4.2.3.5, 4.2.4.3 and 4.2.5.6; TS 29.512 V19.7.0
 * clauses 4.2.6.2.13 and 4.2.6.2.5). What the application session asks for, read from the AppSessionContext that the resource holds.
 */
typedef struct usage_params_s {
    bool sponsored;             /* aspId and sponId given and sponStatus is not SPONSOR_DISABLED */
    const char *asp_id;
    const char *spon_id;
    bool usage_report;          /* the event USAGE_REPORT is subscribed */
    bool has_thres;
    long total, uplink, downlink;
    int duration;
} usage_params_t;

static void app_usage_params(pcf_sess_t *sess, cJSON *context, usage_params_t *p)
{
    cJSON *asc = NULL, *item = NULL, *ev_subsc = NULL, *events = NULL, *event = NULL, *thres = NULL;

    memset(p, 0, sizeof(*p));
    if (!context ||
        !OGS_SBI_FEATURES_IS_SET(sess->policyauthorization_features, OGS_SBI_NPCF_POLICYAUTHORIZATION_SPONSORED_CONNECTIVITY))
        return;

    asc = cJSON_GetObjectItem(context, "ascReqData");
    if (!cJSON_IsObject(asc))
        return;

    item = cJSON_GetObjectItem(asc, "aspId");
    if (cJSON_IsString(item)) p->asp_id = item->valuestring;
    item = cJSON_GetObjectItem(asc, "sponId");
    if (cJSON_IsString(item)) p->spon_id = item->valuestring;
    item = cJSON_GetObjectItem(asc, "sponStatus");
    p->sponsored = p->asp_id && p->spon_id &&
        !(cJSON_IsString(item) && !strcmp(item->valuestring, "SPONSOR_DISABLED"));

    ev_subsc = cJSON_GetObjectItem(asc, "evSubsc");
    if (!cJSON_IsObject(ev_subsc))
        return;
    events = cJSON_GetObjectItem(ev_subsc, "events");
    cJSON_ArrayForEach(event, events) {
        item = cJSON_GetObjectItem(event, "event");
        if (cJSON_IsString(item) && !strcmp(item->valuestring, "USAGE_REPORT"))
            p->usage_report = true;
    }
    thres = cJSON_GetObjectItem(ev_subsc, "usgThres");
    if (cJSON_IsObject(thres)) {
        item = cJSON_GetObjectItem(thres, "totalVolume");
        if (cJSON_IsNumber(item)) p->total = (long)item->valuedouble;
        item = cJSON_GetObjectItem(thres, "uplinkVolume");
        if (cJSON_IsNumber(item)) p->uplink = (long)item->valuedouble;
        item = cJSON_GetObjectItem(thres, "downlinkVolume");
        if (cJSON_IsNumber(item)) p->downlink = (long)item->valuedouble;
        item = cJSON_GetObjectItem(thres, "duration");
        if (cJSON_IsNumber(item)) p->duration = (int)item->valuedouble;
        p->has_thres = p->total || p->uplink || p->downlink || p->duration;
    }
}

/*
 * TS 29.514 V19.8.0 clause 4.2.2.5: when the SMF does not support sponsored connectivity, the PCF rejects a request that enables
 * it. The usage thresholds need the usage monitoring control of the SMF (TS 29.512 V19.7.0 clause 4.2.4.10.1).
 */
static bool sponsored_not_authorized(pcf_sess_t *sess, cJSON *context)
{
    usage_params_t p;

    app_usage_params(sess, context, &p);
    if (!p.sponsored)
        return false;
    if (!OGS_SBI_FEATURES_IS_SET(sess->smpolicycontrol_features, OGS_SBI_NPCF_SMPOLICYCONTROL_SPONSORED_CONNECTIVITY))
        return true;
    if (p.usage_report && p.has_thres &&
        !OGS_SBI_FEATURES_IS_SET(sess->smpolicycontrol_features, OGS_SBI_NPCF_SMPOLICYCONTROL_UMC))
        return true;
    return false;
}

/* The event USAGE_REPORT and the usage (AccumulatedUsage, TS 29.122 V19.7.0 clause 5.2.1.2.24; the usage since the last report) in an EventsNotification */
static void usage_report_add(cJSON *evs, const OpenAPI_accu_usage_report_t *report)
{
    cJSON *notifs = cJSON_GetObjectItem(evs, "evNotifs"), *notif = NULL, *usg = NULL;

    ogs_assert(notifs);
    notif = cJSON_CreateObject();
    ogs_assert(notif);
    cJSON_AddStringToObject(notif, "event", "USAGE_REPORT");
    cJSON_AddItemToArray(notifs, notif);

    usg = cJSON_AddObjectToObject(evs, "usgRep");
    ogs_assert(usg);
    if (report->is_time_usage)
        cJSON_AddNumberToObject(usg, "duration", report->time_usage);
    if (report->is_vol_usage)
        cJSON_AddNumberToObject(usg, "totalVolume", (double)report->vol_usage);
    if (report->is_vol_usage_downlink)
        cJSON_AddNumberToObject(usg, "downlinkVolume", (double)report->vol_usage_downlink);
    if (report->is_vol_usage_uplink)
        cJSON_AddNumberToObject(usg, "uplinkVolume", (double)report->vol_usage_uplink);
}

/* The EventsNotification of the usage report that the SMF sends; free with cJSON_Delete() */
static cJSON *usage_report_notif(pcf_app_t *app, const OpenAPI_accu_usage_report_t *report)
{
    cJSON *evs = NULL, *notifs = NULL;
    ogs_sbi_server_t *server = ogs_sbi_server_first();
    ogs_sbi_header_t header;
    char *location = NULL;

    evs = cJSON_CreateObject();
    ogs_assert(evs);
    if (server) {
        memset(&header, 0, sizeof(header));
        header.service.name = OpenAPI_service_name_ToString(OpenAPI_service_name_npcf_policyauthorization);
        header.api.version = (char *)OGS_SBI_API_V1;
        header.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_APP_SESSIONS;
        header.resource.component[1] = (char *)app->app_session_id;
        header.resource.component[2] = (char *)OGS_SBI_RESOURCE_NAME_EVENTS_SUBSCRIPTION;
        location = ogs_sbi_server_uri(server, &header);
        ogs_assert(location);
        cJSON_AddStringToObject(evs, "evSubsUri", location);
        ogs_free(location);
    }
    notifs = cJSON_AddArrayToObject(evs, "evNotifs");
    ogs_assert(notifs);
    usage_report_add(evs, report);
    return evs;
}

/*
 * Send the SMF the PCC rules of the application session with what the sponsor and the usage thresholds change: the charging data
 * (SPON_CON_LEVEL) and the usage monitoring data they refer to, or the end of the monitoring.
 */
static void app_usage_send(pcf_sess_t *sess, pcf_app_t *app, const usage_params_t *p, bool monitor, bool end_monitor)
{
    OpenAPI_sm_policy_decision_t SmPolicyDecision;
    OpenAPI_list_t *PccRuleList = NULL, *QosDecisionList = NULL;
    OpenAPI_lnode_t *node = NULL;
    int i;

    if (app->num_of_pcc_rule == 0)
        return;

    memset(&SmPolicyDecision, 0, sizeof(SmPolicyDecision));
    PccRuleList = OpenAPI_list_create();
    ogs_assert(PccRuleList);
    QosDecisionList = OpenAPI_list_create();
    ogs_assert(QosDecisionList);

    for (i = 0; i < app->num_of_pcc_rule; i++) {
        ogs_pcc_rule_t *pcc_rule = &app->pcc_rule[i];
        OpenAPI_pcc_rule_t *PccRule = ogs_sbi_build_pcc_rule(pcc_rule, 0);
        OpenAPI_qos_data_t *QosData = NULL;

        ogs_assert(PccRule);
        ogs_sbi_decision_add_traffic_control(&SmPolicyDecision, PccRule, pcc_rule);
        if (p->sponsored)
            ogs_sbi_decision_add_sponsor(&SmPolicyDecision, PccRule, app->chg_id, p->spon_id, p->asp_id);
        if (monitor)
            ogs_sbi_decision_add_usage_monitoring(&SmPolicyDecision, PccRule, app->um_id,
                    p->total, p->uplink, p->downlink, p->duration);
        OpenAPI_list_add(PccRuleList, OpenAPI_map_create(PccRule->pcc_rule_id, PccRule));

        QosData = ogs_sbi_build_qos_data(pcc_rule);
        ogs_assert(QosData);
        OpenAPI_list_add(QosDecisionList, OpenAPI_map_create(QosData->qos_id, QosData));
    }
    app_qnc_decisions(app, QosDecisionList);
    app_qnc_trigger(app, &SmPolicyDecision);
    if (end_monitor)
        ogs_sbi_decision_end_usage_monitoring(&SmPolicyDecision, app->um_id);
    if (monitor || end_monitor)
        ogs_sbi_decision_add_us_re_trigger(&SmPolicyDecision);

    SmPolicyDecision.pcc_rules = PccRuleList;
    SmPolicyDecision.qos_decs = QosDecisionList;
    ogs_assert(true == pcf_sbi_send_smpolicycontrol_update_notify(sess, &SmPolicyDecision));

    ogs_sbi_decision_free_traffic_control(&SmPolicyDecision);
    OpenAPI_list_free(SmPolicyDecision.policy_ctrl_req_triggers);
    OpenAPI_list_for_each(PccRuleList, node) {
        OpenAPI_map_t *Map = node->data;

        if (Map) {
            if (Map->value) ogs_sbi_free_pcc_rule(Map->value);
            ogs_free(Map);
        }
    }
    OpenAPI_list_free(PccRuleList);
    OpenAPI_list_for_each(QosDecisionList, node) {
        OpenAPI_map_t *Map = node->data;

        if (Map) {
            if (Map->value) ogs_sbi_free_qos_data(Map->value);
            ogs_free(Map);
        }
    }
    OpenAPI_list_free(QosDecisionList);
}

/*
 * Bring the SMF to what the application session now asks for: the sponsor identity of the charging data, the thresholds of the usage
 * monitoring, or the end of both. thresholds_given: the request carried the subscription, so the thresholds apply anew (a report
 * of reached thresholds stops the monitoring until the application function provides them again, clause 4.2.5.6).
 */
static void app_usage_apply(pcf_sess_t *sess, pcf_app_t *app, bool thresholds_given)
{
    usage_params_t p;
    bool monitor, end_monitor;
    char *sig = NULL;

    app_usage_params(sess, app->context, &p);

    monitor = p.sponsored && p.usage_report && p.has_thres &&
        OGS_SBI_FEATURES_IS_SET(sess->smpolicycontrol_features, OGS_SBI_NPCF_SMPOLICYCONTROL_UMC);
    sig = ogs_msprintf("%d|%s|%s|%d|%ld|%ld|%ld|%d|%d", p.sponsored, p.asp_id ? p.asp_id : "", p.spon_id ? p.spon_id : "",
            monitor, p.total, p.uplink, p.downlink, p.duration, app->ev_qos_notif);
    ogs_assert(sig);

    if (!app->chg_id) {
        app->chg_id = ogs_msprintf("chg-%s", app->app_session_id);
        app->um_id = ogs_msprintf("um-%s", app->app_session_id);
        ogs_assert(app->chg_id && app->um_id);
    }

    /* the thresholds that are reached stay stopped until the application function gives new ones */
    if (app->um_sig && !strcmp(app->um_sig, sig) && !(monitor && thresholds_given)) {
        ogs_free(sig);
        return;
    }
    if (!app->um_sig && !p.sponsored) {
        /* nothing was ever configured and nothing is asked for */
        app->um_sig = sig;
        return;
    }

    end_monitor = app->um_active && !monitor;
    app_usage_send(sess, app, &p, monitor, end_monitor);

    app->um_active = monitor;
    if (app->um_sig)
        ogs_free(app->um_sig);
    app->um_sig = sig;
}

static bool content_has_evsubsc(const char *content)
{
    cJSON *patch = content ? cJSON_Parse(content) : NULL;
    cJSON *asc = patch ? cJSON_GetObjectItem(patch, "ascReqData") : NULL;
    bool has = asc && cJSON_HasObjectItem(asc, "evSubsc");

    if (patch)
        cJSON_Delete(patch);
    return has;
}

bool pcf_npcf_policyauthorization_handle_events_subscription_put(
        pcf_sess_t *sess, pcf_app_t *app_session, ogs_sbi_stream_t *stream,
        ogs_sbi_message_t *recvmsg, const char *content)
{
    cJSON *body = NULL, *events = NULL, *asc = NULL, *held = NULL;
    bool created;
    char *location = NULL, *text = NULL;
    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;
    ogs_sbi_header_t header;
    ogs_sbi_server_t *server = NULL;

    ogs_assert(sess);
    ogs_assert(app_session);
    ogs_assert(stream);
    ogs_assert(recvmsg);

    /* TS 29.514 V19.8.0 table 5.3.4.3.1-2: the request body is an EventsSubscReqData, whose "events" attribute is mandatory */
    body = content ? cJSON_Parse(content) : NULL;
    events = body ? cJSON_GetObjectItem(body, "events") : NULL;
    if (!cJSON_IsArray(events)) {
        if (body)
            cJSON_Delete(body);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "Invalid EventsSubscReqData", recvmsg->h.uri, NULL));
        return false;
    }

    ogs_assert(app_session->context);
    asc = cJSON_GetObjectItem(app_session->context, "ascReqData");
    if (!cJSON_IsObject(asc))
        asc = cJSON_AddObjectToObject(app_session->context, "ascReqData");
    ogs_assert(asc);
    held = cJSON_GetObjectItem(asc, "evSubsc");
    created = !cJSON_IsObject(held);
    cJSON_DeleteItemFromObject(asc, "evSubsc");
    cJSON_AddItemToObject(asc, "evSubsc", body);
    app_events_refresh(app_session);

    /* EventsSubscPutData carries the properties of the EventsSubscReqData */
    text = cJSON_Print(body);
    ogs_assert(text);

    server = ogs_sbi_server_from_stream(stream);
    ogs_assert(server);
    memset(&header, 0, sizeof(header));
    header.service.name = OpenAPI_service_name_ToString(
            OpenAPI_service_name_npcf_policyauthorization);
    header.api.version = (char *)OGS_SBI_API_V1;
    header.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_APP_SESSIONS;
    header.resource.component[1] = (char *)app_session->app_session_id;
    header.resource.component[2] =
        (char *)OGS_SBI_RESOURCE_NAME_EVENTS_SUBSCRIPTION;
    location = created ? ogs_sbi_server_uri(server, &header) : NULL;

    memset(&sendmsg, 0, sizeof(sendmsg));
    sendmsg.http.location = location;
    response = ogs_sbi_build_response(&sendmsg,
            created ? OGS_SBI_HTTP_STATUS_CREATED : OGS_SBI_HTTP_STATUS_OK);
    ogs_assert(response);
    ogs_sbi_header_set(response->http.headers, OGS_SBI_CONTENT_TYPE,
            OGS_SBI_CONTENT_JSON_TYPE);
    response->http.content = text;
    response->http.content_length = strlen(text);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    if (location)
        ogs_free(location);
    app_usage_apply(sess, app_session, true);
    return true;
}

bool pcf_npcf_policyauthorization_handle_events_subscription_delete(
        pcf_sess_t *sess, pcf_app_t *app_session, ogs_sbi_stream_t *stream,
        ogs_sbi_message_t *recvmsg)
{
    cJSON *asc = NULL;

    ogs_assert(sess);
    ogs_assert(app_session);
    ogs_assert(stream);
    ogs_assert(recvmsg);

    asc = app_session->context ?
        cJSON_GetObjectItem(app_session->context, "ascReqData") : NULL;
    if (!asc || !cJSON_IsObject(cJSON_GetObjectItem(asc, "evSubsc"))) {
        /* TS 29.514 V19.8.0 table 5.3.4.3.2-3: 404 when the Events Subscription sub-resource does not exist */
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
                recvmsg, "Not found", recvmsg->h.uri, NULL));
        return false;
    }

    cJSON_DeleteItemFromObject(asc, "evSubsc");
    app_events_refresh(app_session);
    ogs_assert(true == ogs_sbi_send_http_status_no_content(stream));
    app_usage_apply(sess, app_session, false);
    return true;
}

bool pcf_npcf_policyauthorization_handle_get(pcf_sess_t *sess,
        pcf_app_t *app_session, ogs_sbi_stream_t *stream,
        ogs_sbi_message_t *recvmsg)
{
    ogs_assert(sess);
    ogs_assert(app_session);
    ogs_assert(stream);
    ogs_assert(recvmsg);

    if (!app_session->context) {
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_NOT_FOUND, recvmsg, "Not found",
                recvmsg->h.uri, "APPLICATION_SESSION_CONTEXT_NOT_FOUND"));
        return false;
    }

    send_app_session_context(stream, app_session, OGS_SBI_HTTP_STATUS_OK, NULL);
    return true;
}

bool pcf_npcf_policyauthorization_handle_create(pcf_sess_t *sess,
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    bool rc;
    int i, j, rv, status = 0;
    bool cause_duplicate = false;
    char *strerror = NULL;
    pcf_ue_sm_t *pcf_ue_sm = NULL;
    pcf_app_t *app_session = NULL;

    ogs_sbi_client_t *client = NULL;
    OpenAPI_uri_scheme_e scheme = OpenAPI_uri_scheme_NULL;
    char *fqdn = NULL;
    uint16_t fqdn_port = 0;
    ogs_sockaddr_t *addr = NULL, *addr6 = NULL;

    OpenAPI_app_session_context_t *AppSessionContext = NULL;
    OpenAPI_app_session_context_req_data_t *AscReqData = NULL;

    uint64_t supported_features = 0;

    ogs_sbi_server_t *server = NULL;
    ogs_sbi_header_t header;
    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    ogs_session_data_t session_data;

    ogs_ims_data_t ims_data;
    ogs_media_component_t *media_component = NULL;
    ogs_media_sub_component_t *sub = NULL;
    const char *qos_reference[OGS_MAX_NUM_OF_MEDIA_COMPONENT] = {0};

    OpenAPI_list_t *MediaComponentList = NULL;
    OpenAPI_map_t *MediaComponentMap = NULL;
    OpenAPI_media_component_t *MediaComponent = NULL;

    OpenAPI_list_t *SubComponentList = NULL;
    OpenAPI_map_t *SubComponentMap = NULL;
    OpenAPI_media_sub_component_t *SubComponent = NULL;

    OpenAPI_list_t *fDescList = NULL;

    OpenAPI_sm_policy_decision_t SmPolicyDecision;

    OpenAPI_list_t *PccRuleList = NULL;
    OpenAPI_map_t *PccRuleMap = NULL;
    OpenAPI_pcc_rule_t *PccRule = NULL;

    OpenAPI_list_t *QosDecisionList = NULL;
    OpenAPI_map_t *QosDecisionMap = NULL;
    OpenAPI_qos_data_t *QosData = NULL;

    OpenAPI_lnode_t *node = NULL, *node2 = NULL, *node3 = NULL;

    ogs_assert(sess);
    pcf_ue_sm = pcf_ue_sm_find_by_id(sess->pcf_ue_sm_id);
    ogs_assert(stream);
    ogs_assert(recvmsg);

    server = ogs_sbi_server_from_stream(stream);
    ogs_assert(server);

    memset(&ims_data, 0, sizeof(ims_data));
    memset(&session_data, 0, sizeof(ogs_session_data_t));

    AppSessionContext = recvmsg->AppSessionContext;
    if (!AppSessionContext) {
        strerror = ogs_msprintf("[%s:%d] No AppSessionContext",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    AscReqData = AppSessionContext->asc_req_data;
    if (!AscReqData) {
        strerror = ogs_msprintf("[%s:%d] No AscReqData",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (!AscReqData->supp_feat) {
        strerror = ogs_msprintf("[%s:%d] No AscReqData->suppFeat",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (!AscReqData->notif_uri) {
        strerror = ogs_msprintf("[%s:%d] No AscReqData->notifUri",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    if (!AscReqData->med_components) {
        strerror = ogs_msprintf("[%s:%d] No AscReqData->MediaCompoenent",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    rc = ogs_sbi_getaddr_from_uri(&scheme, &fqdn, &fqdn_port, &addr, &addr6,
            AscReqData->notif_uri);
    if (rc == false || scheme == OpenAPI_uri_scheme_NULL) {
        strerror = ogs_msprintf("[%s:%d] Invalid URI [%s]",
                pcf_ue_sm->supi, sess->psi, AscReqData->notif_uri);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    supported_features = ogs_uint64_from_string_hexadecimal(
            AscReqData->supp_feat);
    sess->policyauthorization_features &= supported_features;

    if (sess->policyauthorization_features != supported_features) {
        ogs_free(AscReqData->supp_feat);
        AscReqData->supp_feat =
            ogs_uint64_to_string(sess->policyauthorization_features);
        ogs_assert(AscReqData->supp_feat);
    }

    {
        cJSON *requested = OpenAPI_app_session_context_convertToJSON(recvmsg->AppSessionContext);
        bool refused = requested && sponsored_not_authorized(sess, requested);

        if (requested)
            cJSON_Delete(requested);
        if (refused) {
            strerror = ogs_msprintf("[%s:%d] The SMF does not support the sponsored connectivity that the request enables",
                    pcf_ue_sm->supi, sess->psi);
            status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            goto cleanup;
        }
    }

    MediaComponentList = AscReqData->med_components;
    OpenAPI_list_for_each(MediaComponentList, node) {
        MediaComponentMap = node->data;
        if (MediaComponentMap) {
            MediaComponent = MediaComponentMap->value;
            if (MediaComponent) {
                int n;
                if (ims_data.num_of_media_component >=
                        OGS_ARRAY_SIZE(ims_data.media_component)) {
                    ogs_error("OVERFLOW ims_data.num_of_media_component "
                            "[%d:%d:%d]",
                            ims_data.num_of_media_component,
                            OGS_MAX_NUM_OF_MEDIA_COMPONENT,
                            (int)OGS_ARRAY_SIZE(ims_data.media_component));
                    break;
                }
                n = ims_data.num_of_media_component;

                media_component = &ims_data.media_component[n];
                qos_reference[n] = MediaComponent->qos_reference;
                media_component->media_component_number =
                    MediaComponent->med_comp_n;
                media_component->media_type = MediaComponent->med_type;
                if (MediaComponent->mar_bw_dl)
                    media_component->max_requested_bandwidth_dl =
                        ogs_sbi_bitrate_from_string(MediaComponent->mar_bw_dl);
                if (MediaComponent->mar_bw_ul)
                    media_component->max_requested_bandwidth_ul =
                        ogs_sbi_bitrate_from_string(MediaComponent->mar_bw_ul);
                if (MediaComponent->mir_bw_dl)
                    media_component->min_requested_bandwidth_dl =
                        ogs_sbi_bitrate_from_string(MediaComponent->mir_bw_dl);
                if (MediaComponent->mir_bw_ul)
                    media_component->min_requested_bandwidth_ul =
                        ogs_sbi_bitrate_from_string(MediaComponent->mir_bw_ul);
                if (MediaComponent->rr_bw)
                    media_component->rr_bandwidth =
                        ogs_sbi_bitrate_from_string(MediaComponent->rr_bw);
                if (MediaComponent->rs_bw)
                    media_component->rs_bandwidth =
                        ogs_sbi_bitrate_from_string(MediaComponent->rs_bw);
                media_component->flow_status = MediaComponent->f_status;

                SubComponentList = MediaComponent->med_sub_comps;
                OpenAPI_list_for_each(SubComponentList, node2) {
                    if (media_component->num_of_sub >=
                            OGS_ARRAY_SIZE(media_component->sub)) {
                        ogs_error("OVERFLOW media_component->num_of_sub "
                                "[%d:%d:%d]",
                                media_component->num_of_sub,
                                OGS_MAX_NUM_OF_MEDIA_SUB_COMPONENT,
                                (int)OGS_ARRAY_SIZE(media_component->sub));
                        break;
                    }
                    sub = &media_component->sub[media_component->num_of_sub];

                    SubComponentMap = node2->data;
                    if (SubComponentMap) {
                        SubComponent = SubComponentMap->value;
                        if (SubComponent) {
                            sub->flow_number = SubComponent->f_num;
                            sub->flow_usage = SubComponent->flow_usage;

                            fDescList = SubComponent->f_descs;
                            OpenAPI_list_for_each(fDescList, node3) {
                                ogs_flow_t *flow = NULL;

                                if (sub->num_of_flow >=
                                        OGS_ARRAY_SIZE(sub->flow)) {
                                    ogs_error(
                                        "OVERFLOW sub->num_of_flow [%d:%d:%d]",
                                        sub->num_of_flow,
                                        OGS_MAX_NUM_OF_FLOW_IN_MEDIA_SUB_COMPONENT,
                                        (int)OGS_ARRAY_SIZE(sub->flow));
                                    break;
                                }
                                flow = &sub->flow[sub->num_of_flow];
                                if (node3->data) {
                                    flow->description = ogs_strdup(node3->data);
                                    ogs_assert(flow->description);

                                    sub->num_of_flow++;
                                }
                            }
                            media_component->num_of_sub++;
                        }
                    }
                }
                ims_data.num_of_media_component++;
            }
        }
    }

    /*
     * TS 29.514 V19.8.0 clause 4.2.2.2: the same AF charging identifier as that of another ongoing application session is
     * rejected with 400 and the application error DUPLICATED_AF_SESSION.
     */
    if (AscReqData->af_charg_id) {
        pcf_ue_sm_t *other_ue = NULL;
        pcf_sess_t *other_sess = NULL;
        pcf_app_t *other_app = NULL;

        ogs_list_for_each(&pcf_self()->pcf_ue_sm_list, other_ue) {
            ogs_list_for_each(&other_ue->sess_list, other_sess) {
                ogs_list_for_each(&other_sess->app_list, other_app) {
                    cJSON *asc = other_app->context ?
                        cJSON_GetObjectItem(other_app->context, "ascReqData") :
                        NULL;
                    cJSON *held = asc ?
                        cJSON_GetObjectItem(asc, "afChargId") : NULL;

                    if (cJSON_IsString(held) && !strcmp(held->valuestring,
                                AscReqData->af_charg_id)) {
                        strerror = ogs_msprintf("[%s:%d] AF charging "
                                "identifier [%s] is in use",
                                pcf_ue_sm->supi, sess->psi,
                                AscReqData->af_charg_id);
                        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
                        cause_duplicate = true;
                        ogs_freeaddrinfo(addr);
                        ogs_freeaddrinfo(addr6);
                        if (fqdn)
                            ogs_free(fqdn);
                        goto cleanup;
                    }
                }
            }
        }
    }

    app_session = pcf_app_add(sess);
    ogs_assert(app_session);

    if (app_session->notif_uri)
        ogs_free(app_session->notif_uri);
    app_session->notif_uri = ogs_strdup(AscReqData->notif_uri);
    ogs_assert(app_session->notif_uri);

    client = ogs_sbi_client_find(scheme, fqdn, fqdn_port, addr, addr6);
    if (!client) {
        ogs_debug("%s: ogs_sbi_client_add()", OGS_FUNC);
        client = ogs_sbi_client_add(scheme, fqdn, fqdn_port, addr, addr6);
        if (!client) {
            strerror = ogs_msprintf("%s: ogs_sbi_client_add() failed",
                    OGS_FUNC);
            status = OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR;
            ogs_freeaddrinfo(addr);
            goto cleanup;
        }
    }
    OGS_SBI_SETUP_CLIENT(&app_session->naf, client);

    ogs_free(fqdn);
    ogs_freeaddrinfo(addr);
    ogs_freeaddrinfo(addr6);

    rv = pcf_get_session_data(
            pcf_ue_sm->supi,
            sess->home.presence == true ? &sess->home.plmn_id : NULL,
            &sess->s_nssai, sess->dnn, &session_data, 0);
    if (rv != OGS_OK) {
        strerror = ogs_msprintf("[%s:%d] Cannot find SUPI in DB",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
        goto cleanup;
    }

    memset(&SmPolicyDecision, 0, sizeof(SmPolicyDecision));

    PccRuleList = OpenAPI_list_create();
    ogs_assert(PccRuleList);

    QosDecisionList = OpenAPI_list_create();
    ogs_assert(QosDecisionList);

    for (i = 0; i < ims_data.num_of_media_component; i++) {
        int flow_presence = 0;

        ogs_pcc_rule_t *pcc_rule = NULL;
        ogs_pcc_rule_t *db_pcc_rule = NULL;
        uint8_t qos_index = 0;
        ogs_media_component_t *media_component = &ims_data.media_component[i];
        const char *reference = qos_reference[i];
        const char *err_str = NULL;

        qos_index = pcf_qos_index_from_media(
                reference, media_component->media_type, &err_str);
        if (qos_index == 0) {
            strerror = ogs_msprintf("[%s:%d] %s",
                    pcf_ue_sm->supi, sess->psi, err_str);
            /* TS 29.514 V19.8.0 clauses 4.2.2.2 and 4.2.3.2: an invalid QoS reference is 400 with INVALID_SERVICE_INFORMATION */
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            goto cleanup;
        }

        for (j = 0; j < session_data.num_of_pcc_rule; j++) {
            if (session_data.pcc_rule[j].qos.index == qos_index) {
                db_pcc_rule = &session_data.pcc_rule[j];
                break;
            }
        }

        /*
         * TS 29.514 clause 4.2.2.2: the PCF derives the PCC rules from the service information. With no PCC rule of
         * the subscriber for the QoS index, the operator-defined parameters of the qos_profiles entry give the rule.
         */
        if (!db_pcc_rule)
            db_pcc_rule = reference ? pcf_qos_profile_dynamic_pcc_rule(reference) :
                pcf_qos_profile_dynamic_pcc_rule_by_index(qos_index);

        if (!db_pcc_rule && !reference &&
            (media_component->media_type == OpenAPI_media_type_CONTROL)) {
            /*
             * Check for default bearer for IMS signalling
             * QCI 5 and ARP 1
             */
            if (session_data.session.qos.index != OGS_QOS_INDEX_5 ||
                session_data.session.qos.arp.priority_level != 1) {
                strerror = ogs_msprintf("[%s:%d] CHECK WEBUI : "
                    "Even the Default Bearer(QCI:%d,ARP:%d) "
                    "cannot support IMS signalling.",
                    pcf_ue_sm->supi, sess->psi,
                    session_data.session.qos.index,
                    session_data.session.qos.arp.priority_level);
                status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
                goto cleanup;
            } else {
                continue;
            }
        }

        if (!db_pcc_rule) {
            strerror = ogs_msprintf("[%s:%d] CHECK WEBUI : "
                "No PCC Rule in DB [QoS Index:%d] - "
                "Please add PCC Rule using WEBUI",
                pcf_ue_sm->supi, sess->psi, qos_index);
            status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            goto cleanup;
        }

        for (j = 0; j < app_session->num_of_pcc_rule; j++) {
            if (app_session->med_comp_n[j] ==
                    media_component->media_component_number) {
                pcc_rule = &app_session->pcc_rule[j];
                break;
            }
        }

        if (!pcc_rule &&
            app_session->num_of_pcc_rule >= OGS_MAX_NUM_OF_PCC_RULE) {
            strerror = ogs_msprintf("[%s:%d] Too many media components "
                    "in the application session [%d]",
                    pcf_ue_sm->supi, sess->psi, OGS_MAX_NUM_OF_PCC_RULE);
            status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            goto cleanup;
        }

        if (!pcc_rule) {
            pcc_rule = &app_session->pcc_rule[app_session->num_of_pcc_rule];
            ogs_assert(pcc_rule);

            pcc_rule->id = ogs_msprintf("%s-a%s-m%d",
                            db_pcc_rule->id, app_session->app_session_id,
                            media_component->media_component_number);
            ogs_assert(pcc_rule->id);
            app_session->med_comp_n[app_session->num_of_pcc_rule] =
                media_component->media_component_number;

            memcpy(&pcc_rule->qos, &db_pcc_rule->qos, sizeof(ogs_qos_t));

            pcc_rule->flow_status = db_pcc_rule->flow_status;
            pcc_rule->precedence = db_pcc_rule->precedence;

            /* Install Flow */
            flow_presence = 1;
            rv = ogs_pcc_rule_install_flow_from_media(
                    pcc_rule, media_component);
            if (rv != OGS_OK) {
                strerror = ogs_msprintf("[%s:%d] install_flow() failed",
                    pcf_ue_sm->supi, sess->psi);
                status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
                goto cleanup;
            }

            app_session->num_of_pcc_rule++;

        } else {
            int count = 0;

            /* Check Flow */
            count = ogs_pcc_rule_num_of_flow_equal_to_media(
                    pcc_rule, media_component);
            if (count == -1) {
                strerror = ogs_msprintf("[%s:%d] matched_flow() failed",
                    pcf_ue_sm->supi, sess->psi);
                status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
                goto cleanup;
            }

            if (pcc_rule->num_of_flow != count) {
                /* Re-install Flow */
                flow_presence = 1;
                rv = ogs_pcc_rule_install_flow_from_media(
                        pcc_rule, media_component);
                if (rv != OGS_OK) {
                    strerror = ogs_msprintf("[%s:%d] re-install_flow() failed",
                        pcf_ue_sm->supi, sess->psi);
                    status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
                    goto cleanup;
                }
            }
        }

        /* Update QoS */
        rv = ogs_pcc_rule_update_qos_from_media(pcc_rule, media_component);
        if (rv != OGS_OK) {
            strerror = ogs_msprintf("[%s:%d] update_qos() failed",
                pcf_ue_sm->supi, sess->psi);
            status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            goto cleanup;
        }

        /* fStatus of the media component: the gate of the flows (TS 29.514 clause 5.6.2.7) */
        if (media_component->flow_status != OpenAPI_flow_status_NULL)
            pcc_rule->flow_status =
                ogs_sbi_flow_status_to_pcc_rule(media_component->flow_status);

        /* if we failed to get QoS from IMS, apply WEBUI QoS */
        if (pcc_rule->qos.mbr.downlink == 0)
            pcc_rule->qos.mbr.downlink = db_pcc_rule->qos.mbr.downlink;
        if (pcc_rule->qos.mbr.uplink == 0)
            pcc_rule->qos.mbr.uplink = db_pcc_rule->qos.mbr.uplink;
        if (pcc_rule->qos.gbr.downlink == 0)
            pcc_rule->qos.gbr.downlink = db_pcc_rule->qos.gbr.downlink;
        if (pcc_rule->qos.gbr.uplink == 0)
            pcc_rule->qos.gbr.uplink = db_pcc_rule->qos.gbr.uplink;

        /**************************************************************
         * Build PCC Rule & QoS Decision
         *************************************************************/
        PccRule = ogs_sbi_build_pcc_rule(pcc_rule, flow_presence);
        ogs_assert(PccRule->pcc_rule_id);

        /* the status of the flows (fStatus of the media component) goes to the SMF as TrafficControlData (TS 29.512 clause 5.6.2.10) */
        ogs_sbi_decision_add_traffic_control(&SmPolicyDecision, PccRule, pcc_rule);

        PccRuleMap = OpenAPI_map_create(PccRule->pcc_rule_id, PccRule);
        ogs_assert(PccRuleMap);

        OpenAPI_list_add(PccRuleList, PccRuleMap);

        QosData = ogs_sbi_build_qos_data(pcc_rule);
        ogs_assert(QosData);
        ogs_assert(QosData->qos_id);

        QosDecisionMap = OpenAPI_map_create(QosData->qos_id, QosData);
        ogs_assert(QosDecisionMap);

        OpenAPI_list_add(QosDecisionList, QosDecisionMap);
    }

    if (PccRuleList->count)
        SmPolicyDecision.pcc_rules = PccRuleList;

    if (QosDecisionList->count)
        SmPolicyDecision.qos_decs = QosDecisionList;

    /* a subscription to SUCCESSFUL_RESOURCES_ALLOCATION (TS 29.514 V19.8.0 clause 4.2.2.10) asks the SMF to confirm the rules */
    app_session->context =
        OpenAPI_app_session_context_convertToJSON(recvmsg->AppSessionContext);
    ogs_assert(app_session->context);
    app_events_refresh(app_session);
    /* the outcome is asked for in every case: it answers a deletion that asks for the events, too (clause 4.2.4.2) */
    succ_res_alloc_request(&SmPolicyDecision, PccRuleList);
    app_qnc_trigger(app_session, &SmPolicyDecision);
    app_qnc_decisions(app_session, QosDecisionList);

    memset(&sendmsg, 0, sizeof(sendmsg));

    memset(&header, 0, sizeof(header));
    header.service.name =
        OpenAPI_service_name_ToString(
                OpenAPI_service_name_npcf_policyauthorization);
    header.api.version = (char *)OGS_SBI_API_V1;
    header.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_APP_SESSIONS;
    header.resource.component[1] = (char *)app_session->app_session_id;
    sendmsg.http.location = ogs_sbi_server_uri(server, &header);
    ogs_assert(sendmsg.http.location);

    sendmsg.AppSessionContext = recvmsg->AppSessionContext;

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_CREATED);
    ogs_assert(response);
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    ogs_free(sendmsg.http.location);

    if (PccRuleList->count || QosDecisionList->count) {
        ogs_assert(true == pcf_sbi_send_smpolicycontrol_update_notify(
                                sess, &SmPolicyDecision));
    }
    succ_res_alloc_request_free(&SmPolicyDecision);
    ogs_sbi_decision_free_traffic_control(&SmPolicyDecision);

    OpenAPI_list_for_each(PccRuleList, node) {
        PccRuleMap = node->data;
        if (PccRuleMap) {
            PccRule = PccRuleMap->value;
            if (PccRule)
                ogs_sbi_free_pcc_rule(PccRule);
            ogs_free(PccRuleMap);
        }
    }
    OpenAPI_list_free(PccRuleList);

    OpenAPI_list_for_each(QosDecisionList, node) {
        QosDecisionMap = node->data;
        if (QosDecisionMap) {
            QosData = QosDecisionMap->value;
            if (QosData)
                ogs_sbi_free_qos_data(QosData);
            ogs_free(QosDecisionMap);
        }
    }
    OpenAPI_list_free(QosDecisionList);

    /* the sponsor and the usage thresholds of the application function (clause 4.2.2.5) */
    app_usage_apply(sess, app_session, true);

    ogs_ims_data_free(&ims_data);
    OGS_SESSION_DATA_FREE(&session_data);

    return true;

cleanup:
    ogs_assert(status);
    ogs_assert(strerror);

    /*
     * Unknown qosReference is rejected with 400 in a negative test case.
     * Log it as warning to avoid treating the expected rejection as an error.
     */
    ogs_log_message(
            (status == OGS_SBI_HTTP_STATUS_FORBIDDEN ||
             status == OGS_SBI_HTTP_STATUS_BAD_REQUEST) ?
                OGS_LOG_WARN : OGS_LOG_ERROR, 0, "%s", strerror);
    ogs_assert(true ==
        ogs_sbi_server_send_error(stream, status, recvmsg, strerror, NULL,
                cause_duplicate ? "DUPLICATED_AF_SESSION" :
                    policyauthorization_cause(status)));
    ogs_free(strerror);

    ogs_sbi_decision_free_traffic_control(&SmPolicyDecision);

    OpenAPI_list_for_each(PccRuleList, node) {
        PccRuleMap = node->data;
        if (PccRuleMap) {
            PccRule = PccRuleMap->value;
            if (PccRule)
                ogs_sbi_free_pcc_rule(PccRule);
            ogs_free(PccRuleMap);
        }
    }
    OpenAPI_list_free(PccRuleList);

    OpenAPI_list_for_each(QosDecisionList, node) {
        QosDecisionMap = node->data;
        if (QosDecisionMap) {
            QosData = QosDecisionMap->value;
            if (QosData)
                ogs_sbi_free_qos_data(QosData);
            ogs_free(QosDecisionMap);
        }
    }
    OpenAPI_list_free(QosDecisionList);

    ogs_ims_data_free(&ims_data);
    OGS_SESSION_DATA_FREE(&session_data);

    if (app_session)
        pcf_app_remove(app_session);

    return false;
}

bool pcf_npcf_policyauthorization_handle_update(
        pcf_sess_t *sess, pcf_app_t *app_session,
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg,
        const char *content)
{
    int i, j, rv, status = 0;
    char *strerror = NULL;
    pcf_ue_sm_t *pcf_ue_sm = NULL;

    OpenAPI_app_session_context_update_data_patch_t
        *AppSessionContextUpdateDataPatch = NULL;
    OpenAPI_app_session_context_update_data_t *AscUpdateData = NULL;


    ogs_session_data_t session_data;

    ogs_ims_data_t ims_data;
    ogs_media_component_t *media_component = NULL;
    ogs_media_sub_component_t *sub = NULL;
    const char *qos_reference[OGS_MAX_NUM_OF_MEDIA_COMPONENT] = {0};
    /* The media components that the request removes: a null value in the map of media components, or fStatus REMOVED */
    int remove_med_comp_n[OGS_MAX_NUM_OF_MEDIA_COMPONENT];
    int num_of_remove = 0;
    ogs_pcc_rule_t removed_rule[OGS_MAX_NUM_OF_PCC_RULE];
    int num_of_removed_rule = 0;

    OpenAPI_list_t *MediaComponentList = NULL;
    OpenAPI_map_t *MediaComponentMap = NULL;
    OpenAPI_media_component_rm_t *MediaComponent = NULL;

    OpenAPI_list_t *SubComponentList = NULL;
    OpenAPI_map_t *SubComponentMap = NULL;
    OpenAPI_media_sub_component_rm_t *SubComponent = NULL;

    OpenAPI_list_t *fDescList = NULL;

    OpenAPI_sm_policy_decision_t SmPolicyDecision;

    OpenAPI_list_t *PccRuleList = NULL;
    OpenAPI_map_t *PccRuleMap = NULL;
    OpenAPI_pcc_rule_t *PccRule = NULL;

    OpenAPI_list_t *QosDecisionList = NULL;
    OpenAPI_map_t *QosDecisionMap = NULL;
    OpenAPI_qos_data_t *QosData = NULL;

    OpenAPI_lnode_t *node = NULL, *node2 = NULL, *node3 = NULL;

    ogs_assert(sess);
    pcf_ue_sm = pcf_ue_sm_find_by_id(sess->pcf_ue_sm_id);
    ogs_assert(app_session);
    ogs_assert(stream);
    ogs_assert(recvmsg);

    memset(&ims_data, 0, sizeof(ims_data));
    memset(&session_data, 0, sizeof(ogs_session_data_t));

    AppSessionContextUpdateDataPatch =
        recvmsg->AppSessionContextUpdateDataPatch;
    if (!AppSessionContextUpdateDataPatch) {
        strerror = ogs_msprintf("[%s:%d] No AppSessionContextUpdateDataPatch",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    AscUpdateData = AppSessionContextUpdateDataPatch->asc_req_data;
    if (!AscUpdateData) {
        strerror = ogs_msprintf("[%s:%d] No AscUpdateData",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    /* TS 29.514 V19.8.0 clause 4.2.3.2: a PATCH may carry only the events subscription (evSubsc, or null to remove it) */
    if (!AscUpdateData->med_components && !AscUpdateData->ev_subsc &&
        !AscUpdateData->is_ev_subsc_null && !AscUpdateData->spon_id && !AscUpdateData->asp_id &&
        AscUpdateData->spon_status == OpenAPI_sponsoring_status_NULL) {
        strerror = ogs_msprintf("[%s:%d] No AscUpdateData->MediaCompoenent",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
        goto cleanup;
    }

    /* the context as the PATCH leaves it (clause 4.2.3.5): a sponsor that the SMF cannot serve is refused before anything changes */
    if (content && app_session->context) {
        cJSON *patch = cJSON_Parse(content);
        cJSON *asc_patch = patch ? cJSON_GetObjectItem(patch, "ascReqData") : NULL;
        cJSON *after = cJSON_Duplicate(app_session->context, 1);
        cJSON *asc_after = after ? cJSON_GetObjectItem(after, "ascReqData") : NULL;
        bool refused = false;

        if (cJSON_IsObject(asc_patch) && cJSON_IsObject(asc_after)) {
            json_merge_patch(asc_after, asc_patch);
            refused = sponsored_not_authorized(sess, after);
        }
        if (after)
            cJSON_Delete(after);
        if (patch)
            cJSON_Delete(patch);
        if (refused) {
            strerror = ogs_msprintf("[%s:%d] The SMF does not support the sponsored connectivity that the request enables",
                    pcf_ue_sm->supi, sess->psi);
            status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            goto cleanup;
        }
    }

    MediaComponentList = AscUpdateData->med_components;
    OpenAPI_list_for_each(MediaComponentList, node) {
        MediaComponentMap = node->data;
        if (MediaComponentMap) {
            MediaComponent = MediaComponentMap->value;
            if (!MediaComponent || MediaComponent->f_status ==
                    OpenAPI_flow_status_REMOVED) {
                /*
                 * TS 29.514 V19.8.0 clause 4.2.3.36: the media component number as the key of the map with the value set to
                 * NULL removes the media component; clause 5.6.3.12: fStatus REMOVED removes its service data flows.
                 */
                if (num_of_remove < OGS_ARRAY_SIZE(remove_med_comp_n)) {
                    remove_med_comp_n[num_of_remove++] = MediaComponent ?
                        MediaComponent->med_comp_n :
                        (MediaComponentMap->key ?
                            atoi(MediaComponentMap->key) : 0);
                } else {
                    ogs_error("OVERFLOW remove_med_comp_n [%d]",
                            num_of_remove);
                    break;
                }
            } else if (MediaComponent) {
                int n;
                if (ims_data.num_of_media_component >=
                        OGS_ARRAY_SIZE(ims_data.media_component)) {
                    ogs_error("OVERFLOW ims_data.num_of_media_component "
                            "[%d:%d:%d]",
                            ims_data.num_of_media_component,
                            OGS_MAX_NUM_OF_MEDIA_COMPONENT,
                            (int)OGS_ARRAY_SIZE(ims_data.media_component));
                    break;
                }
                n = ims_data.num_of_media_component;

                media_component = &ims_data.media_component[n];
                qos_reference[n] = MediaComponent->qos_reference;
                media_component->media_component_number =
                    MediaComponent->med_comp_n;
                media_component->media_type = MediaComponent->med_type;
                if (MediaComponent->mar_bw_dl)
                    media_component->max_requested_bandwidth_dl =
                        ogs_sbi_bitrate_from_string(MediaComponent->mar_bw_dl);
                if (MediaComponent->mar_bw_ul)
                    media_component->max_requested_bandwidth_ul =
                        ogs_sbi_bitrate_from_string(MediaComponent->mar_bw_ul);
                if (MediaComponent->mir_bw_dl)
                    media_component->min_requested_bandwidth_dl =
                        ogs_sbi_bitrate_from_string(MediaComponent->mir_bw_dl);
                if (MediaComponent->mir_bw_ul)
                    media_component->min_requested_bandwidth_ul =
                        ogs_sbi_bitrate_from_string(MediaComponent->mir_bw_ul);
                if (MediaComponent->rr_bw)
                    media_component->rr_bandwidth =
                        ogs_sbi_bitrate_from_string(MediaComponent->rr_bw);
                if (MediaComponent->rs_bw)
                    media_component->rs_bandwidth =
                        ogs_sbi_bitrate_from_string(MediaComponent->rs_bw);
                media_component->flow_status = MediaComponent->f_status;

                SubComponentList = MediaComponent->med_sub_comps;
                OpenAPI_list_for_each(SubComponentList, node2) {
                    if (media_component->num_of_sub >=
                            OGS_ARRAY_SIZE(media_component->sub)) {
                        ogs_error("OVERFLOW media_component->num_of_sub "
                                "[%d:%d:%d]",
                                media_component->num_of_sub,
                                OGS_MAX_NUM_OF_MEDIA_SUB_COMPONENT,
                                (int)OGS_ARRAY_SIZE(media_component->sub));
                        break;
                    }
                    sub = &media_component->sub[media_component->num_of_sub];

                    SubComponentMap = node2->data;
                    if (SubComponentMap) {
                        SubComponent = SubComponentMap->value;
                        if (SubComponent) {
                            sub->flow_number = SubComponent->f_num;
                            sub->flow_usage = SubComponent->flow_usage;

                            fDescList = SubComponent->f_descs;
                            OpenAPI_list_for_each(fDescList, node3) {
                                ogs_flow_t *flow = NULL;

                                if (sub->num_of_flow >=
                                        OGS_ARRAY_SIZE(sub->flow)) {
                                    ogs_error(
                                        "OVERFLOW sub->num_of_flow [%d:%d:%d]",
                                        sub->num_of_flow,
                                        OGS_MAX_NUM_OF_FLOW_IN_MEDIA_SUB_COMPONENT,
                                        (int)OGS_ARRAY_SIZE(sub->flow));
                                    break;
                                }
                                flow = &sub->flow[sub->num_of_flow];
                                if (node3->data) {
                                    flow->description = ogs_strdup(node3->data);
                                    ogs_assert(flow->description);

                                    sub->num_of_flow++;
                                }
                            }
                            media_component->num_of_sub++;
                        }
                    }
                }
                ims_data.num_of_media_component++;
            }
        }
    }

    rv = pcf_get_session_data(
            pcf_ue_sm->supi,
            sess->home.presence == true ? &sess->home.plmn_id : NULL,
            &sess->s_nssai, sess->dnn, &session_data, 0);
    if (rv != OGS_OK) {
        strerror = ogs_msprintf("[%s:%d] Cannot find SUPI in DB",
                pcf_ue_sm->supi, sess->psi);
        status = OGS_SBI_HTTP_STATUS_NOT_FOUND;
        goto cleanup;
    }

    memset(&SmPolicyDecision, 0, sizeof(SmPolicyDecision));

    PccRuleList = OpenAPI_list_create();
    ogs_assert(PccRuleList);

    QosDecisionList = OpenAPI_list_create();
    ogs_assert(QosDecisionList);

    for (i = 0; i < ims_data.num_of_media_component; i++) {
        int flow_presence = 0;

        ogs_pcc_rule_t *pcc_rule = NULL;
        ogs_pcc_rule_t *db_pcc_rule = NULL;
        uint8_t qos_index = 0;
        ogs_media_component_t *media_component = &ims_data.media_component[i];
        const char *reference = qos_reference[i];
        const char *err_str = NULL;

        qos_index = pcf_qos_index_from_media(
                reference, media_component->media_type, &err_str);
        if (qos_index == 0) {
            strerror = ogs_msprintf("[%s:%d] %s",
                    pcf_ue_sm->supi, sess->psi, err_str);
            /* TS 29.514 V19.8.0 clauses 4.2.2.2 and 4.2.3.2: an invalid QoS reference is 400 with INVALID_SERVICE_INFORMATION */
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            goto cleanup;
        }

        for (j = 0; j < session_data.num_of_pcc_rule; j++) {
            if (session_data.pcc_rule[j].qos.index == qos_index) {
                db_pcc_rule = &session_data.pcc_rule[j];
                break;
            }
        }

        /*
         * TS 29.514 clause 4.2.2.2: the PCF derives the PCC rules from the service information. With no PCC rule of
         * the subscriber for the QoS index, the operator-defined parameters of the qos_profiles entry give the rule.
         */
        if (!db_pcc_rule)
            db_pcc_rule = reference ? pcf_qos_profile_dynamic_pcc_rule(reference) :
                pcf_qos_profile_dynamic_pcc_rule_by_index(qos_index);

        if (!db_pcc_rule && !reference &&
            (media_component->media_type == OpenAPI_media_type_CONTROL)) {
            /*
             * Check for default bearer for IMS signalling
             * QCI 5 and ARP 1
             */
            if (session_data.session.qos.index != OGS_QOS_INDEX_5 ||
                session_data.session.qos.arp.priority_level != 1) {
                strerror = ogs_msprintf("[%s:%d] CHECK WEBUI : "
                    "Even the Default Bearer(QCI:%d,ARP:%d) "
                    "cannot support IMS signalling.",
                    pcf_ue_sm->supi, sess->psi,
                    session_data.session.qos.index,
                    session_data.session.qos.arp.priority_level);
                status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
                goto cleanup;
            } else {
                continue;
            }
        }

        if (!db_pcc_rule) {
            strerror = ogs_msprintf("[%s:%d] CHECK WEBUI : "
                "No PCC Rule in DB [QoS Index:%d] - "
                "Please add PCC Rule using WEBUI",
                pcf_ue_sm->supi, sess->psi, qos_index);
            status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            goto cleanup;
        }

        for (j = 0; j < app_session->num_of_pcc_rule; j++) {
            if (app_session->med_comp_n[j] ==
                    media_component->media_component_number) {
                pcc_rule = &app_session->pcc_rule[j];
                break;
            }
        }

        if (!pcc_rule &&
            app_session->num_of_pcc_rule >= OGS_MAX_NUM_OF_PCC_RULE) {
            strerror = ogs_msprintf("[%s:%d] Too many media components "
                    "in the application session [%d]",
                    pcf_ue_sm->supi, sess->psi, OGS_MAX_NUM_OF_PCC_RULE);
            status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            goto cleanup;
        }

        if (!pcc_rule) {
            pcc_rule = &app_session->pcc_rule[app_session->num_of_pcc_rule];
            ogs_assert(pcc_rule);

            pcc_rule->id = ogs_msprintf("%s-a%s-m%d",
                            db_pcc_rule->id, app_session->app_session_id,
                            media_component->media_component_number);
            ogs_assert(pcc_rule->id);
            app_session->med_comp_n[app_session->num_of_pcc_rule] =
                media_component->media_component_number;

            memcpy(&pcc_rule->qos, &db_pcc_rule->qos, sizeof(ogs_qos_t));

            pcc_rule->flow_status = db_pcc_rule->flow_status;
            pcc_rule->precedence = db_pcc_rule->precedence;

            /* Install Flow */
            flow_presence = 1;
            rv = ogs_pcc_rule_install_flow_from_media(
                    pcc_rule, media_component);
            if (rv != OGS_OK) {
                strerror = ogs_msprintf("[%s:%d] install_flow() failed",
                    pcf_ue_sm->supi, sess->psi);
                status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
                goto cleanup;
            }

            app_session->num_of_pcc_rule++;

        } else {
            int count = 0;

            /* The media component names another QoS reference: the rule takes the QoS of that reference */
            if (pcc_rule->qos.index != db_pcc_rule->qos.index) {
                memcpy(&pcc_rule->qos, &db_pcc_rule->qos, sizeof(ogs_qos_t));
                pcc_rule->flow_status = db_pcc_rule->flow_status;
                pcc_rule->precedence = db_pcc_rule->precedence;
            }

            /* Check Flow */
            count = ogs_pcc_rule_num_of_flow_equal_to_media(
                    pcc_rule, media_component);
            if (count == -1) {
                strerror = ogs_msprintf("[%s:%d] matched_flow() failed",
                    pcf_ue_sm->supi, sess->psi);
                status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
                goto cleanup;
            }

            if (pcc_rule->num_of_flow != count) {
                /* Re-install Flow */
                flow_presence = 1;
                rv = ogs_pcc_rule_install_flow_from_media(
                        pcc_rule, media_component);
                if (rv != OGS_OK) {
                    strerror = ogs_msprintf("[%s:%d] re-install_flow() failed",
                        pcf_ue_sm->supi, sess->psi);
                    status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
                    goto cleanup;
                }
            }
        }

        /* Update QoS */
        rv = ogs_pcc_rule_update_qos_from_media(pcc_rule, media_component);
        if (rv != OGS_OK) {
            strerror = ogs_msprintf("[%s:%d] update_qos() failed",
                pcf_ue_sm->supi, sess->psi);
            status = OGS_SBI_HTTP_STATUS_FORBIDDEN;
            goto cleanup;
        }

        /* fStatus of the media component: the gate of the flows (TS 29.514 clause 5.6.2.7) */
        if (media_component->flow_status != OpenAPI_flow_status_NULL)
            pcc_rule->flow_status =
                ogs_sbi_flow_status_to_pcc_rule(media_component->flow_status);

        /* if we failed to get QoS from IMS, apply WEBUI QoS */
        if (pcc_rule->qos.mbr.downlink == 0)
            pcc_rule->qos.mbr.downlink = db_pcc_rule->qos.mbr.downlink;
        if (pcc_rule->qos.mbr.uplink == 0)
            pcc_rule->qos.mbr.uplink = db_pcc_rule->qos.mbr.uplink;
        if (pcc_rule->qos.gbr.downlink == 0)
            pcc_rule->qos.gbr.downlink = db_pcc_rule->qos.gbr.downlink;
        if (pcc_rule->qos.gbr.uplink == 0)
            pcc_rule->qos.gbr.uplink = db_pcc_rule->qos.gbr.uplink;

        /**************************************************************
         * Build PCC Rule & QoS Decision
         *************************************************************/
        PccRule = ogs_sbi_build_pcc_rule(pcc_rule, flow_presence);
        ogs_assert(PccRule->pcc_rule_id);

        /* the status of the flows (fStatus of the media component) goes to the SMF as TrafficControlData (TS 29.512 clause 5.6.2.10) */
        ogs_sbi_decision_add_traffic_control(&SmPolicyDecision, PccRule, pcc_rule);

        PccRuleMap = OpenAPI_map_create(PccRule->pcc_rule_id, PccRule);
        ogs_assert(PccRuleMap);

        OpenAPI_list_add(PccRuleList, PccRuleMap);

        QosData = ogs_sbi_build_qos_data(pcc_rule);
        ogs_assert(QosData);
        ogs_assert(QosData->qos_id);

        QosDecisionMap = OpenAPI_map_create(QosData->qos_id, QosData);
        ogs_assert(QosDecisionMap);

        OpenAPI_list_add(QosDecisionList, QosDecisionMap);
    }

    for (i = 0; i < num_of_remove; i++) {
        for (j = 0; j < app_session->num_of_pcc_rule; j++) {
            if (app_session->med_comp_n[j] == remove_med_comp_n[i])
                break;
        }
        if (j == app_session->num_of_pcc_rule)
            continue;

        /* The SMF is told to remove the PCC rule and its QoS data */
        removed_rule[num_of_removed_rule] = app_session->pcc_rule[j];

        PccRuleMap = OpenAPI_map_create(
                removed_rule[num_of_removed_rule].id, NULL);
        ogs_assert(PccRuleMap);
        OpenAPI_list_add(PccRuleList, PccRuleMap);

        QosDecisionMap = OpenAPI_map_create(
                removed_rule[num_of_removed_rule].id, NULL);
        ogs_assert(QosDecisionMap);
        OpenAPI_list_add(QosDecisionList, QosDecisionMap);

        num_of_removed_rule++;

        for (; j < app_session->num_of_pcc_rule - 1; j++) {
            app_session->pcc_rule[j] = app_session->pcc_rule[j + 1];
            app_session->med_comp_n[j] = app_session->med_comp_n[j + 1];
        }
        memset(&app_session->pcc_rule[j], 0, sizeof(ogs_pcc_rule_t));
        app_session->med_comp_n[j] = 0;
        app_session->num_of_pcc_rule--;
    }

    if (PccRuleList->count)
        SmPolicyDecision.pcc_rules = PccRuleList;

    if (QosDecisionList->count)
        SmPolicyDecision.qos_decs = QosDecisionList;

    /* the subscription may have changed with this PATCH (TS 29.514 V19.8.0 clause 4.2.3.10) */
    {
        cJSON *patch_for_events = content ? cJSON_Parse(content) : NULL;
        cJSON *asc_for_events = patch_for_events ?
            cJSON_GetObjectItem(patch_for_events, "ascReqData") : NULL;

        if (asc_for_events && cJSON_HasObjectItem(asc_for_events, "evSubsc")) {
            /* the merge below changes the stored subscription; apply it first for the decision */
            cJSON *held_asc = app_session->context ?
                cJSON_GetObjectItem(app_session->context, "ascReqData") : NULL;
            cJSON *new_subsc = cJSON_GetObjectItem(asc_for_events, "evSubsc");

            if (held_asc) {
                cJSON_DeleteItemFromObject(held_asc, "evSubsc");
                if (!cJSON_IsNull(new_subsc))
                    cJSON_AddItemToObject(held_asc, "evSubsc",
                            cJSON_Duplicate(new_subsc, 1));
            }
            app_events_refresh(app_session);
        }
        if (patch_for_events)
            cJSON_Delete(patch_for_events);
    }
    /* the outcome is asked for in every case: it answers a deletion that asks for the events, too (clause 4.2.4.2) */
    succ_res_alloc_request(&SmPolicyDecision, PccRuleList);
    app_qnc_trigger(app_session, &SmPolicyDecision);
    app_qnc_decisions(app_session, QosDecisionList);

    /* The resource now reads as the creation request with the modification applied (clause 5.3.3.3.1 GET) */
    ogs_assert(app_session->context);
    {
        cJSON *patch = content ? cJSON_Parse(content) : NULL;
        cJSON *asc_patch = patch ?
            cJSON_GetObjectItem(patch, "ascReqData") : NULL;

        if (cJSON_IsObject(asc_patch)) {
            cJSON *asc = cJSON_GetObjectItem(app_session->context, "ascReqData");
            if (!cJSON_IsObject(asc))
                asc = cJSON_AddObjectToObject(app_session->context,
                        "ascReqData");
            ogs_assert(asc);
            json_merge_patch(asc, asc_patch);
        }
        if (patch)
            cJSON_Delete(patch);
    }
    app_events_refresh(app_session);

    /* TS 29.514 V19.8.0 table 5.3.3.3.2-3: the 200 carries the AppSessionContext after the modification */
    send_app_session_context(stream, app_session, OGS_SBI_HTTP_STATUS_OK, NULL);

    if (PccRuleList->count || QosDecisionList->count) {
        ogs_assert(true == pcf_sbi_send_smpolicycontrol_update_notify(
                            sess, &SmPolicyDecision));
    }
    succ_res_alloc_request_free(&SmPolicyDecision);
    ogs_sbi_decision_free_traffic_control(&SmPolicyDecision);

    /* the sponsor and the usage thresholds that the PATCH changes (clause 4.2.3.5) */
    app_usage_apply(sess, app_session, content_has_evsubsc(content));

    OpenAPI_list_for_each(PccRuleList, node) {
        PccRuleMap = node->data;
        if (PccRuleMap) {
            PccRule = PccRuleMap->value;
            if (PccRule)
                ogs_sbi_free_pcc_rule(PccRule);
            ogs_free(PccRuleMap);
        }
    }
    OpenAPI_list_free(PccRuleList);

    OpenAPI_list_for_each(QosDecisionList, node) {
        QosDecisionMap = node->data;
        if (QosDecisionMap) {
            QosData = QosDecisionMap->value;
            if (QosData)
                ogs_sbi_free_qos_data(QosData);
            ogs_free(QosDecisionMap);
        }
    }
    OpenAPI_list_free(QosDecisionList);

    for (i = 0; i < num_of_removed_rule; i++)
        OGS_PCC_RULE_FREE(&removed_rule[i]);

    ogs_ims_data_free(&ims_data);
    OGS_SESSION_DATA_FREE(&session_data);

    return true;

cleanup:
    ogs_assert(status);
    ogs_assert(strerror);
    ogs_error("%s", strerror);
    ogs_assert(true ==
        ogs_sbi_server_send_error(stream, status, recvmsg, strerror,
                NULL, policyauthorization_cause(status)));
    ogs_free(strerror);

    ogs_sbi_decision_free_traffic_control(&SmPolicyDecision);

    OpenAPI_list_for_each(PccRuleList, node) {
        PccRuleMap = node->data;
        if (PccRuleMap) {
            PccRule = PccRuleMap->value;
            if (PccRule)
                ogs_sbi_free_pcc_rule(PccRule);
            ogs_free(PccRuleMap);
        }
    }
    OpenAPI_list_free(PccRuleList);

    OpenAPI_list_for_each(QosDecisionList, node) {
        QosDecisionMap = node->data;
        if (QosDecisionMap) {
            QosData = QosDecisionMap->value;
            if (QosData)
                ogs_sbi_free_qos_data(QosData);
            ogs_free(QosDecisionMap);
        }
    }
    OpenAPI_list_free(QosDecisionList);

    ogs_ims_data_free(&ims_data);
    OGS_SESSION_DATA_FREE(&session_data);

    return false;
}

static cJSON *delete_events_report(pcf_app_t *app, const char *content, const OpenAPI_accu_usage_report_t *usage);

bool pcf_npcf_policyauthorization_handle_delete(
        pcf_sess_t *sess, pcf_app_t *app_session,
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg,
        const char *content)
{
    int i;

    OpenAPI_sm_policy_decision_t SmPolicyDecision;

    OpenAPI_list_t *PccRuleList = NULL;
    OpenAPI_map_t *PccRuleMap = NULL;

    OpenAPI_list_t *QosDecisionList = NULL;
    OpenAPI_map_t *QosDecisionMap = NULL;

    OpenAPI_lnode_t *node = NULL;
    cJSON *report = NULL;

    ogs_assert(app_session);

    /*
     * TS 29.514 V19.8.0 clause 4.2.4.3: while the SMF monitors the usage, the PCF immediately configures the SMF to report it (the
     * monitoring ends together with the PCC rules, TS 29.512 V19.7.0 clause 4.2.6.5.3.2) and answers when the usage arrives.
     */
    if (app_session->um_active && app_session->um_id) {
        memset(&SmPolicyDecision, 0, sizeof(SmPolicyDecision));
        PccRuleList = OpenAPI_list_create();
        ogs_assert(PccRuleList);
        QosDecisionList = OpenAPI_list_create();
        ogs_assert(QosDecisionList);
        for (i = 0; i < app_session->num_of_pcc_rule; i++) {
            OpenAPI_list_add(PccRuleList, OpenAPI_map_create(app_session->pcc_rule[i].id, NULL));
            OpenAPI_list_add(QosDecisionList, OpenAPI_map_create(app_session->pcc_rule[i].id, NULL));
        }
        if (PccRuleList->count) SmPolicyDecision.pcc_rules = PccRuleList;
        if (QosDecisionList->count) SmPolicyDecision.qos_decs = QosDecisionList;
        ogs_sbi_decision_end_usage_monitoring(&SmPolicyDecision, app_session->um_id);
        ogs_sbi_decision_add_us_re_trigger(&SmPolicyDecision);

        app_session->delete_pending = true;
        app_session->delete_stream_id = ogs_sbi_id_from_stream(stream);
        if (content && *content)
            app_session->delete_content = ogs_strdup(content);
        if (!pcf_sbi_send_smpolicycontrol_delete_pending_notify(sess, app_session, &SmPolicyDecision))
            pcf_npcf_policyauthorization_delete_complete(app_session, NULL);

        ogs_sbi_decision_free_traffic_control(&SmPolicyDecision);
        OpenAPI_list_free(SmPolicyDecision.policy_ctrl_req_triggers);
        OpenAPI_list_for_each(PccRuleList, node) {
            if (node->data) ogs_free(node->data);
        }
        OpenAPI_list_free(PccRuleList);
        OpenAPI_list_for_each(QosDecisionList, node) {
            if (node->data) ogs_free(node->data);
        }
        OpenAPI_list_free(QosDecisionList);
        return true;
    }

    /* TS 29.514 V19.8.0 clause 5.3.3.4.2.2: 200 with the events that the deletion asked for, else 204 */
    report = delete_events_report(app_session, content, NULL);
    if (report) {
        ogs_sbi_response_t *response = ogs_sbi_response_new();
        char *text = cJSON_Print(report);

        ogs_assert(response);
        ogs_assert(text);
        response->status = OGS_SBI_HTTP_STATUS_OK;
        ogs_sbi_header_set(response->http.headers, OGS_SBI_CONTENT_TYPE, OGS_SBI_CONTENT_JSON_TYPE);
        response->http.content = text;
        response->http.content_length = strlen(text);
        ogs_assert(true == ogs_sbi_server_send_response(stream, response));
        cJSON_Delete(report);
    } else {
        ogs_assert(true == ogs_sbi_send_http_status_no_content(stream));
    }

    memset(&SmPolicyDecision, 0, sizeof(SmPolicyDecision));

    PccRuleList = OpenAPI_list_create();
    ogs_assert(PccRuleList);

    QosDecisionList = OpenAPI_list_create();
    ogs_assert(QosDecisionList);

    for (i = 0; i < app_session->num_of_pcc_rule; i++) {
        ogs_pcc_rule_t *pcc_rule = &app_session->pcc_rule[i];

        ogs_assert(pcc_rule);

        PccRuleMap = OpenAPI_map_create(pcc_rule->id, NULL);
        ogs_assert(PccRuleMap);

        OpenAPI_list_add(PccRuleList, PccRuleMap);

        QosDecisionMap = OpenAPI_map_create(pcc_rule->id, NULL);
        ogs_assert(QosDecisionMap);

        OpenAPI_list_add(QosDecisionList, QosDecisionMap);
    }

    if (PccRuleList->count)
        SmPolicyDecision.pcc_rules = PccRuleList;

    if (QosDecisionList->count)
        SmPolicyDecision.qos_decs = QosDecisionList;

    if (PccRuleList->count || QosDecisionList->count) {
        ogs_assert(true == pcf_sbi_send_smpolicycontrol_delete_notify(
                            sess, app_session, &SmPolicyDecision));
    } else {
        pcf_app_remove(app_session);
    }

    OpenAPI_list_for_each(PccRuleList, node) {
        PccRuleMap = node->data;
        if (PccRuleMap) {
            ogs_free(PccRuleMap);
        }
    }
    OpenAPI_list_free(PccRuleList);

    OpenAPI_list_for_each(QosDecisionList, node) {
        QosDecisionMap = node->data;
        if (QosDecisionMap) {
            ogs_free(QosDecisionMap);
        }
    }
    OpenAPI_list_free(QosDecisionList);

    return true;
}

/*
 * The Flows of an EventsNotification (TS 29.514 V19.8.0 table 5.6.2.? Flows): the media components whose PCC rules the report
 * names.
 */
/* Remember the outcome of the resources allocation of a rule (a later report replaces the earlier one). */
static void app_outcome_set(pcf_app_t *app, const char *rule_id, bool active)
{
    int i;

    for (i = 0; i < app->num_of_outcome; i++) {
        if (!strcmp(app->outcome[i].rule_id, rule_id)) {
            app->outcome[i].active = active;
            return;
        }
    }
    if (app->num_of_outcome >= OGS_MAX_NUM_OF_PCC_RULE)
        return;
    app->outcome[app->num_of_outcome].rule_id = ogs_strdup(rule_id);
    app->outcome[app->num_of_outcome].active = active;
    app->num_of_outcome++;
}

/*
 * The events that a deletion asks for (TS 29.514 V19.8.0 clause 5.3.3.4.2.2, EventsSubscReqData in the body) and for which the PCF has
 * the information: the outcome of the resources allocation of the rules that the application session holds. Returns the partial
 * AppSessionContext with evsNotif (clause 4.2.4.2), or NULL when there is nothing to report.
 */
static cJSON *delete_events_report(pcf_app_t *app, const char *content, const OpenAPI_accu_usage_report_t *usage)
{
    cJSON *body = NULL, *events = NULL, *event = NULL;
    cJSON *asc = NULL, *evs_notif = NULL, *notifs = NULL;
    OpenAPI_list_t *ok_ids = NULL, *failed_ids = NULL;
    bool want_ok = false, want_failed = false;
    int i;

    body = (content && *content) ? cJSON_Parse(content) : NULL;
    events = body ? cJSON_GetObjectItemCaseSensitive(body, "events") : NULL;
    cJSON_ArrayForEach(event, events) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(event, "event");

        if (cJSON_IsString(name)) {
            if (!strcmp(name->valuestring, "SUCCESSFUL_RESOURCES_ALLOCATION")) want_ok = true;
            if (!strcmp(name->valuestring, "FAILED_RESOURCES_ALLOCATION")) want_failed = true;
        }
    }
    if (body) cJSON_Delete(body);
    if (!want_ok && !want_failed && !usage) return NULL;

    ok_ids = OpenAPI_list_create();
    failed_ids = OpenAPI_list_create();
    for (i = 0; i < app->num_of_outcome; i++)
        OpenAPI_list_add(app->outcome[i].active ? ok_ids : failed_ids, app->outcome[i].rule_id);

    asc = cJSON_CreateObject();
    evs_notif = cJSON_AddObjectToObject(asc, "evsNotif");
    notifs = cJSON_AddArrayToObject(evs_notif, "evNotifs");
    if (want_ok && ok_ids->count) {
        cJSON *notif = cJSON_CreateObject(), *flows = flows_of_rules(app, ok_ids);

        cJSON_AddStringToObject(notif, "event", "SUCCESSFUL_RESOURCES_ALLOCATION");
        if (cJSON_GetArraySize(flows) > 0) {
            cJSON_AddItemToObject(notif, "flows", flows);
            cJSON_AddItemToArray(notifs, notif);
        } else {
            cJSON_Delete(flows);
            cJSON_Delete(notif);
        }
    }
    if (want_failed && failed_ids->count) {
        cJSON *notif = cJSON_CreateObject(), *flows = flows_of_rules(app, failed_ids);

        cJSON_AddStringToObject(notif, "event", "FAILED_RESOURCES_ALLOCATION");
        if (cJSON_GetArraySize(flows) > 0) {
            cJSON_AddItemToObject(notif, "flows", flows);
            cJSON_AddItemToArray(notifs, notif);
        } else {
            cJSON_Delete(flows);
            cJSON_Delete(notif);
        }
    }
    OpenAPI_list_free(ok_ids);
    OpenAPI_list_free(failed_ids);

    /* the accumulated usage of the sponsored data connectivity (TS 29.514 V19.8.0 clause 4.2.4.3) */
    if (usage)
        usage_report_add(evs_notif, usage);
    if (cJSON_GetArraySize(notifs) == 0) {
        cJSON_Delete(asc);
        return NULL;
    }
    /* evSubsUri is mandatory in EventsNotification (table 5.6.2.5-1) */
    {
        ogs_sbi_server_t *server = ogs_sbi_server_first();
        ogs_sbi_header_t header;
        char *location;

        if (server) {
            memset(&header, 0, sizeof(header));
            header.service.name = OpenAPI_service_name_ToString(OpenAPI_service_name_npcf_policyauthorization);
            header.api.version = (char *)OGS_SBI_API_V1;
            header.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_APP_SESSIONS;
            header.resource.component[1] = (char *)app->app_session_id;
            header.resource.component[2] = (char *)OGS_SBI_RESOURCE_NAME_EVENTS_SUBSCRIPTION;
            location = ogs_sbi_server_uri(server, &header);
            ogs_assert(location);
            cJSON_AddStringToObject(evs_notif, "evSubsUri", location);
            ogs_free(location);
        }
    }
    return asc;
}

static cJSON *flows_of_rules(pcf_app_t *app, OpenAPI_list_t *rule_ids)
{
    cJSON *flows = cJSON_CreateArray();
    OpenAPI_lnode_t *node = NULL;
    int i;

    ogs_assert(flows);
    OpenAPI_list_for_each(rule_ids, node) {
        const char *id = node->data;

        for (i = 0; id && i < app->num_of_pcc_rule; i++) {
            if (app->pcc_rule[i].id && !strcmp(app->pcc_rule[i].id, id)) {
                cJSON *flow = cJSON_CreateObject();

                ogs_assert(flow);
                cJSON_AddNumberToObject(flow, "medCompN", app->med_comp_n[i]);
                cJSON_AddItemToArray(flows, flow);
            }
        }
    }
    return flows;
}

/*
 * The deletion that waited for the usage (clause 4.2.4.3): answer the application function, with the usage when the SMF reported it,
 * and remove the application session.
 */
void pcf_npcf_policyauthorization_delete_complete(pcf_app_t *app, const OpenAPI_accu_usage_report_t *usage)
{
    ogs_sbi_stream_t *stream = NULL;
    cJSON *report = NULL;

    ogs_assert(app);
    if (!app->delete_pending)
        return;
    app->delete_pending = false;

    stream = ogs_sbi_stream_find_by_id(app->delete_stream_id);
    if (stream) {
        report = delete_events_report(app, app->delete_content, usage);
        if (report) {
            ogs_sbi_response_t *response = ogs_sbi_response_new();
            char *text = cJSON_Print(report);

            ogs_assert(response);
            ogs_assert(text);
            response->status = OGS_SBI_HTTP_STATUS_OK;
            ogs_sbi_header_set(response->http.headers, OGS_SBI_CONTENT_TYPE, OGS_SBI_CONTENT_JSON_TYPE);
            response->http.content = text;
            response->http.content_length = strlen(text);
            ogs_assert(true == ogs_sbi_server_send_response(stream, response));
            cJSON_Delete(report);
        } else {
            ogs_assert(true == ogs_sbi_send_http_status_no_content(stream));
        }
    }
    pcf_app_remove(app);
}

static bool app_owns_rule(pcf_app_t *app, const char *id)
{
    int i;

    for (i = 0; id && i < app->num_of_pcc_rule; i++) {
        if (app->pcc_rule[i].id && !strcmp(app->pcc_rule[i].id, id))
            return true;
    }
    return false;
}

/*
 * Npcf_SMPolicyControl_Update (TS 29.512 V19.7.0 clause 4.2.4.14 and 4.2.4.15): the SMF reports the outcome of the resource
 * allocation of PCC rules with ruleReports (ACTIVE when the resources are allocated, INACTIVE when they are not). The PCF
 * tells the NF service consumer that subscribed to it (TS 29.514 V19.8.0 clause 4.2.5.8).
 */
/*
 * qncReports of the SMF (TS 29.512 V19.7.0 clause 4.2.4.20): the GBR QoS targets of PCC rules are not guaranteed or guaranteed again.
 * The generated model of QosNotificationControlInfo is the one of TS 29.514 (both YAML files use the name), so the report is read from
 * the JSON of the request. The AF that subscribed to QOS_NOTIF is told with the flows of the rules (TS 29.514 V19.8.0 clause 4.2.5.4).
 */
static void qnc_reports_notify(pcf_sess_t *sess, const char *content)
{
    cJSON *body = NULL, *reports = NULL, *report = NULL;
    pcf_app_t *app = NULL;

    if (!content)
        return;
    body = cJSON_Parse(content);
    reports = body ? cJSON_GetObjectItem(body, "qncReports") : NULL;
    cJSON_ArrayForEach(report, reports) {
        cJSON *ids = cJSON_GetObjectItem(report, "refPccRuleIds");
        cJSON *type = cJSON_GetObjectItem(report, "notifType");

        if (!cJSON_IsArray(ids) || !cJSON_IsString(type))
            continue;
        ogs_list_for_each(&sess->app_list, app) {
            OpenAPI_list_t *owned = OpenAPI_list_create();
            cJSON *id = NULL, *evs = NULL, *notifs = NULL, *notif = NULL, *info = NULL, *flows = NULL, *arr = NULL;
            ogs_sbi_server_t *server = ogs_sbi_server_first();
            ogs_sbi_header_t header;
            char *location = NULL;

            ogs_assert(owned);
            cJSON_ArrayForEach(id, ids) {
                if (cJSON_IsString(id) && app_owns_rule(app, id->valuestring))
                    OpenAPI_list_add(owned, id->valuestring);
            }
            if (owned->count == 0 || !app->ev_qos_notif || !app->ev_notif_uri || !server) {
                OpenAPI_list_free(owned);
                continue;
            }

            evs = cJSON_CreateObject();
            ogs_assert(evs);
            memset(&header, 0, sizeof(header));
            header.service.name = OpenAPI_service_name_ToString(OpenAPI_service_name_npcf_policyauthorization);
            header.api.version = (char *)OGS_SBI_API_V1;
            header.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_APP_SESSIONS;
            header.resource.component[1] = (char *)app->app_session_id;
            header.resource.component[2] = (char *)OGS_SBI_RESOURCE_NAME_EVENTS_SUBSCRIPTION;
            location = ogs_sbi_server_uri(server, &header);
            ogs_assert(location);
            cJSON_AddStringToObject(evs, "evSubsUri", location);
            ogs_free(location);
            notifs = cJSON_AddArrayToObject(evs, "evNotifs");
            notif = cJSON_CreateObject();
            ogs_assert(notifs && notif);
            cJSON_AddStringToObject(notif, "event", "QOS_NOTIF");
            cJSON_AddItemToArray(notifs, notif);
            arr = cJSON_AddArrayToObject(evs, "qncReports");
            info = cJSON_CreateObject();
            ogs_assert(arr && info);
            cJSON_AddStringToObject(info, "notifType", type->valuestring);
            flows = flows_of_rules(app, owned);
            if (cJSON_GetArraySize(flows) > 0)
                cJSON_AddItemToObject(info, "flows", flows);
            else
                cJSON_Delete(flows);
            cJSON_AddItemToArray(arr, info);

            pcf_sbi_send_policyauthorization_event_notify(app, evs);
            cJSON_Delete(evs);
            OpenAPI_list_free(owned);
        }
    }
    if (body)
        cJSON_Delete(body);
}

bool pcf_npcf_smpolicycontrol_handle_update(pcf_sess_t *sess,
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg,
        const char *content)
{
    OpenAPI_sm_policy_update_context_data_t *UpdateData = NULL;
    OpenAPI_lnode_t *node = NULL;
    pcf_app_t *app = NULL;

    ogs_assert(sess);
    ogs_assert(stream);
    ogs_assert(recvmsg);

    UpdateData = recvmsg->SmPolicyUpdateContextData;
    if (!UpdateData) {
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "No SmPolicyUpdateContextData", recvmsg->h.uri,
                NULL));
        return false;
    }

    OpenAPI_list_for_each(UpdateData->rule_reports, node) {
        OpenAPI_rule_report_t *report = node->data;
        bool active;

        if (!report || !report->pcc_rule_ids)
            continue;
        active = report->rule_status == OpenAPI_rule_status_ACTIVE;

        ogs_list_for_each(&sess->app_list, app) {
            cJSON *notification = NULL, *notifs = NULL, *notif = NULL;
            cJSON *flows = NULL, *alloc = NULL, *reports = NULL;
            OpenAPI_lnode_t *id_node = NULL;
            bool owns = false;
            char *location = NULL;
            ogs_sbi_header_t header;
            ogs_sbi_server_t *server = ogs_sbi_server_first();

            OpenAPI_list_for_each(report->pcc_rule_ids, id_node) {
                if (app_owns_rule(app, id_node->data)) {
                    owns = true;
                    app_outcome_set(app, id_node->data, active);
                }
            }
            if (!owns || !app->ev_notif_uri || !server)
                continue;
            if (active ? !app->ev_succ_res_alloc : !app->ev_fail_res_alloc)
                continue;

            memset(&header, 0, sizeof(header));
            header.service.name = OpenAPI_service_name_ToString(
                    OpenAPI_service_name_npcf_policyauthorization);
            header.api.version = (char *)OGS_SBI_API_V1;
            header.resource.component[0] =
                (char *)OGS_SBI_RESOURCE_NAME_APP_SESSIONS;
            header.resource.component[1] = (char *)app->app_session_id;
            header.resource.component[2] =
                (char *)OGS_SBI_RESOURCE_NAME_EVENTS_SUBSCRIPTION;
            location = ogs_sbi_server_uri(server, &header);
            ogs_assert(location);

            /* EventsNotification: evSubsUri and evNotifs are mandatory (TS 29.514 V19.8.0 table 5.6.2.5-1) */
            notification = cJSON_CreateObject();
            ogs_assert(notification);
            cJSON_AddStringToObject(notification, "evSubsUri", location);
            ogs_free(location);
            notifs = cJSON_AddArrayToObject(notification, "evNotifs");
            ogs_assert(notifs);
            notif = cJSON_CreateObject();
            ogs_assert(notif);
            cJSON_AddStringToObject(notif, "event", active ?
                    "SUCCESSFUL_RESOURCES_ALLOCATION" :
                    "FAILED_RESOURCES_ALLOCATION");
            flows = flows_of_rules(app, report->pcc_rule_ids);
            if (cJSON_GetArraySize(flows) > 0)
                cJSON_AddItemToObject(notif, "flows", flows);
            else
                cJSON_Delete(flows);
            cJSON_AddItemToArray(notifs, notif);

            if (!active) {
                reports = cJSON_AddArrayToObject(notification,
                        "failedResourcAllocReports");
                ogs_assert(reports);
                alloc = cJSON_CreateObject();
                ogs_assert(alloc);
                cJSON_AddStringToObject(alloc, "mcResourcStatus", "INACTIVE");
                flows = flows_of_rules(app, report->pcc_rule_ids);
                if (cJSON_GetArraySize(flows) > 0)
                    cJSON_AddItemToObject(alloc, "flows", flows);
                else
                    cJSON_Delete(flows);
                cJSON_AddItemToArray(reports, alloc);
            }

            pcf_sbi_send_policyauthorization_event_notify(app, notification);
            cJSON_Delete(notification);
        }
    }

    qnc_reports_notify(sess, content);

    /*
     * accuUsageReports (clause 4.2.4.10): the usage of the sponsored data connectivity goes to the application function, in the
     * answer of its deletion when that waits for it (TS 29.514 V19.8.0 clause 4.2.4.3), else as a notification (clause 4.2.5.6).
     * The monitoring does not continue: the response carries no thresholds.
     */
    OpenAPI_list_for_each(UpdateData->accu_usage_reports, node) {
        OpenAPI_accu_usage_report_t *usage = node->data;
        pcf_app_t *owner = NULL;

        if (!usage || !usage->ref_um_ids)
            continue;
        ogs_list_for_each(&sess->app_list, app) {
            if (app->um_id && !strcmp(app->um_id, usage->ref_um_ids)) {
                owner = app;
                break;
            }
        }
        if (!owner)
            continue;

        owner->um_active = false;
        if (owner->delete_pending) {
            pcf_npcf_policyauthorization_delete_complete(owner, usage);
        } else if (owner->ev_usage_report && owner->ev_notif_uri) {
            cJSON *notification = usage_report_notif(owner, usage);

            pcf_sbi_send_policyauthorization_event_notify(owner, notification);
            cJSON_Delete(notification);
        }
    }

    /* no change of the decision: 204 (TS 29.512 V19.7.0 table 5.2.2.3.2-3) */
    ogs_assert(true == ogs_sbi_send_http_status_no_content(stream));
    return true;
}
