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

#include "ngap-build.h"

ogs_pkbuf_t *ngap_build_pdu_session_resource_setup_request_transfer(
        smf_sess_t *sess)
{
    smf_bearer_t *qos_flow = NULL;
    ogs_ip_t upf_n3_ip;

    NGAP_PDUSessionResourceSetupRequestTransfer_t message;

    NGAP_PDUSessionResourceSetupRequestTransferIEs_t *ie = NULL;
    NGAP_PDUSessionAggregateMaximumBitRate_t *PDUSessionAggregateMaximumBitRate;
    NGAP_UPTransportLayerInformation_t *UPTransportLayerInformation = NULL;
    NGAP_GTPTunnel_t *gTPTunnel = NULL;
    NGAP_DataForwardingNotPossible_t *DataForwardingNotPossible = NULL;
    NGAP_PDUSessionType_t *PDUSessionType = NULL;
    NGAP_SecurityIndication_t *SecurityIndication = NULL;
    NGAP_QosFlowSetupRequestList_t *QosFlowSetupRequestList = NULL;
    NGAP_QosFlowSetupRequestItem_t *QosFlowSetupRequestItem = NULL;
    NGAP_QosFlowIdentifier_t *qosFlowIdentifier = NULL;
    NGAP_QosFlowLevelQosParameters_t *qosFlowLevelQosParameters = NULL;
    NGAP_QosCharacteristics_t *qosCharacteristics = NULL;
    NGAP_NonDynamic5QIDescriptor_t *nonDynamic5QI = NULL;
    NGAP_AllocationAndRetentionPriority_t *allocationAndRetentionPriority;
    NGAP_GBR_QosInformation_t *gBR_QosInformation = NULL;

    ogs_assert(sess);

    ogs_debug("PDUSessionResourceSetupRequestTransfer");
    memset(&message, 0, sizeof(NGAP_PDUSessionResourceSetupRequestTransfer_t));

    if (sess->session.ambr.downlink || sess->session.ambr.uplink) {
        ie = CALLOC(1,
                sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
        ogs_assert(ie);
        ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_PDUSessionAggregateMaximumBitRate;
        ie->criticality = NGAP_Criticality_reject;
        ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_PDUSessionAggregateMaximumBitRate;

        PDUSessionAggregateMaximumBitRate =
            &ie->value.choice.PDUSessionAggregateMaximumBitRate;

        asn_uint642INTEGER(&PDUSessionAggregateMaximumBitRate->
            pDUSessionAggregateMaximumBitRateUL, sess->session.ambr.uplink);
        asn_uint642INTEGER(&PDUSessionAggregateMaximumBitRate->
            pDUSessionAggregateMaximumBitRateDL, sess->session.ambr.downlink);
    }

    ie = CALLOC(1, sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_UL_NGU_UP_TNLInformation;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_UPTransportLayerInformation;

    UPTransportLayerInformation = &ie->value.choice.UPTransportLayerInformation;

    gTPTunnel = CALLOC(1, sizeof(struct NGAP_GTPTunnel));
    ogs_assert(gTPTunnel);
    UPTransportLayerInformation->present =
        NGAP_UPTransportLayerInformation_PR_gTPTunnel;
    UPTransportLayerInformation->choice.gTPTunnel = gTPTunnel;

    ogs_assert(OGS_OK == ogs_sockaddr_to_ip(
                sess->upf_n3_addr, sess->upf_n3_addr6, &upf_n3_ip));
    ogs_asn_ip_to_BIT_STRING(&upf_n3_ip, &gTPTunnel->transportLayerAddress);
    ogs_asn_uint32_to_OCTET_STRING(sess->upf_n3_teid, &gTPTunnel->gTP_TEID);

    if (sess->handover.data_forwarding_not_possible == true) {
        ie = CALLOC(1,
                sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
        ogs_assert(ie);
        ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_DataForwardingNotPossible;
        ie->criticality = NGAP_Criticality_reject;
        ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_DataForwardingNotPossible;

        DataForwardingNotPossible = &ie->value.choice.DataForwardingNotPossible;

        *DataForwardingNotPossible =
            NGAP_DataForwardingNotPossible_data_forwarding_not_possible;
    }

    ie = CALLOC(1, sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_PDUSessionType;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_PDUSessionType;

    PDUSessionType = &ie->value.choice.PDUSessionType;

    *PDUSessionType = OGS_PDU_SESSION_TYPE_IPV4;
    switch (sess->session.session_type) {
    case OGS_PDU_SESSION_TYPE_IPV4 :
        *PDUSessionType = NGAP_PDUSessionType_ipv4;
        break;
    case OGS_PDU_SESSION_TYPE_IPV6 :
        *PDUSessionType = NGAP_PDUSessionType_ipv6;
        break;
    case OGS_PDU_SESSION_TYPE_IPV4V6 :
        *PDUSessionType = NGAP_PDUSessionType_ipv4v6;
        break;
    default:
        ogs_fatal("Unknown PDU Session Type [%d]", sess->session.session_type);
        ogs_assert_if_reached();
    }

    if (smf_self()->security_indication.integrity_protection_indication &&
        smf_self()->security_indication.confidentiality_protection_indication) {

        ie = CALLOC(1,
                sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
        ogs_assert(ie);
        ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_SecurityIndication;
        ie->criticality = NGAP_Criticality_reject;
        ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_SecurityIndication;

        SecurityIndication = &ie->value.choice.SecurityIndication;

        SecurityIndication->integrityProtectionIndication =
                smf_integrity_protection_indication_value2enum(
                    smf_self()->security_indication.
                        integrity_protection_indication);
        ogs_assert(SecurityIndication->integrityProtectionIndication >= 0);

        SecurityIndication->confidentialityProtectionIndication =
                smf_confidentiality_protection_indication_value2enum(
                    smf_self()->security_indication.
                        confidentiality_protection_indication);
        ogs_assert(SecurityIndication->
                confidentialityProtectionIndication >= 0);

        if (smf_self()->security_indication.
                maximum_integrity_protected_data_rate_uplink) {

            ogs_assert(
                SecurityIndication->integrityProtectionIndication ==
                    NGAP_IntegrityProtectionIndication_required ||
                SecurityIndication->integrityProtectionIndication ==
                    NGAP_IntegrityProtectionIndication_preferred);

            SecurityIndication->maximumIntegrityProtectedDataRate_UL =
                CALLOC(1, sizeof(NGAP_MaximumIntegrityProtectedDataRate_t));
            ogs_assert(SecurityIndication->
                    maximumIntegrityProtectedDataRate_UL);
            *(SecurityIndication->maximumIntegrityProtectedDataRate_UL) =
                smf_maximum_integrity_protected_data_rate_uplink_value2enum(
                    smf_self()->security_indication.
                        maximum_integrity_protected_data_rate_uplink);
            ogs_assert(
                *(SecurityIndication->
                    maximumIntegrityProtectedDataRate_UL) >= 0);

            if (smf_self()->security_indication.
                    maximum_integrity_protected_data_rate_downlink) {
                NGAP_ProtocolExtensionContainer_11905P297_t *extContainer = NULL;
                NGAP_SecurityIndication_ExtIEs_t *extIe = NULL;
                NGAP_MaximumIntegrityProtectedDataRate_t
                    *MaximumIntegrityProtectedDataRate = NULL;

                extContainer = CALLOC(1,
                        sizeof(NGAP_ProtocolExtensionContainer_11905P297_t));
                ogs_assert(extContainer);
                SecurityIndication->iE_Extensions =
                    (struct NGAP_ProtocolExtensionContainer *)extContainer;

                extIe = CALLOC(1, sizeof(NGAP_SecurityIndication_ExtIEs_t));
                ogs_assert(extIe);
                ASN_SEQUENCE_ADD(&extContainer->list, extIe);

                extIe->id =
                    NGAP_ProtocolIE_ID_id_MaximumIntegrityProtectedDataRate_DL;
                extIe->criticality = NGAP_Criticality_ignore;
                extIe->extensionValue.present = NGAP_SecurityIndication_ExtIEs__extensionValue_PR_MaximumIntegrityProtectedDataRate;

                MaximumIntegrityProtectedDataRate =
                    &extIe->extensionValue.choice.
                        MaximumIntegrityProtectedDataRate;

                *MaximumIntegrityProtectedDataRate =
                smf_maximum_integrity_protected_data_rate_downlink_value2enum(
                        smf_self()->security_indication.
                            maximum_integrity_protected_data_rate_downlink);
                ogs_assert(*MaximumIntegrityProtectedDataRate >= 0);
            }
        }
    }

    ie = CALLOC(1, sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_QosFlowSetupRequestList;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_QosFlowSetupRequestList;

    QosFlowSetupRequestList = &ie->value.choice.QosFlowSetupRequestList;

    ogs_list_for_each(&sess->bearer_list, qos_flow) {
        QosFlowSetupRequestItem =
            CALLOC(1, sizeof(struct NGAP_QosFlowSetupRequestItem));
        ogs_assert(QosFlowSetupRequestItem);
        ASN_SEQUENCE_ADD(&QosFlowSetupRequestList->list,
            QosFlowSetupRequestItem);

        qosFlowIdentifier = &QosFlowSetupRequestItem->qosFlowIdentifier;
        qosFlowLevelQosParameters =
            &QosFlowSetupRequestItem->qosFlowLevelQosParameters;

        allocationAndRetentionPriority =
            &qosFlowLevelQosParameters->allocationAndRetentionPriority;
        qosCharacteristics = &qosFlowLevelQosParameters->qosCharacteristics;
        nonDynamic5QI = CALLOC(1, sizeof(struct NGAP_NonDynamic5QIDescriptor));
        ogs_assert(nonDynamic5QI);
        qosCharacteristics->choice.nonDynamic5QI = nonDynamic5QI;
        qosCharacteristics->present = NGAP_QosCharacteristics_PR_nonDynamic5QI;

        *qosFlowIdentifier = qos_flow->qfi;

        nonDynamic5QI->fiveQI = qos_flow->qos.index;

        allocationAndRetentionPriority->priorityLevelARP =
            qos_flow->qos.arp.priority_level;
        if (qos_flow->qos.arp.pre_emption_capability ==
                OGS_5GC_PRE_EMPTION_ENABLED)
            allocationAndRetentionPriority->pre_emptionCapability =
                NGAP_Pre_emptionCapability_may_trigger_pre_emption;
        if (qos_flow->qos.arp.pre_emption_vulnerability ==
                OGS_5GC_PRE_EMPTION_ENABLED)
            allocationAndRetentionPriority->pre_emptionVulnerability =
                NGAP_Pre_emptionVulnerability_pre_emptable;

        if (qos_flow->qos.mbr.downlink || qos_flow->qos.mbr.uplink ||
            qos_flow->qos.gbr.downlink || qos_flow->qos.gbr.uplink) {
            ogs_assert(qos_flow->qos.mbr.downlink);
            ogs_assert(qos_flow->qos.mbr.uplink);
            ogs_assert(qos_flow->qos.gbr.downlink);
            ogs_assert(qos_flow->qos.gbr.uplink);

            qosFlowLevelQosParameters->gBR_QosInformation =
                gBR_QosInformation = CALLOC(1, sizeof(*gBR_QosInformation));
            ogs_assert(gBR_QosInformation);

            asn_uint642INTEGER(&gBR_QosInformation->maximumFlowBitRateDL,
                    qos_flow->qos.mbr.downlink);
            asn_uint642INTEGER(&gBR_QosInformation->maximumFlowBitRateUL,
                    qos_flow->qos.mbr.uplink);
            asn_uint642INTEGER(&gBR_QosInformation->
                    guaranteedFlowBitRateDL, qos_flow->qos.gbr.downlink);
            asn_uint642INTEGER(&gBR_QosInformation->
                    guaranteedFlowBitRateUL, qos_flow->qos.gbr.uplink);
        }
    }

    return ogs_asn_encode(
            &asn_DEF_NGAP_PDUSessionResourceSetupRequestTransfer, &message);
}

ogs_pkbuf_t *ngap_build_pdu_session_resource_modify_request_transfer(
        smf_sess_t *sess, bool qos_presence)
{
    NGAP_PDUSessionResourceModifyRequestTransfer_t message;

    NGAP_PDUSessionResourceModifyRequestTransferIEs_t *ie = NULL;

    NGAP_QosFlowAddOrModifyRequestList_t *QosFlowAddOrModifyRequestList = NULL;
    NGAP_QosFlowAddOrModifyRequestItem_t *QosFlowAddOrModifyRequestItem = NULL;
    NGAP_QosFlowIdentifier_t *qosFlowIdentifier = NULL;
    NGAP_QosFlowLevelQosParameters_t *qosFlowLevelQosParameters = NULL;
    NGAP_QosCharacteristics_t *qosCharacteristics = NULL;
    NGAP_NonDynamic5QIDescriptor_t *nonDynamic5QI = NULL;
    NGAP_AllocationAndRetentionPriority_t *allocationAndRetentionPriority;
    NGAP_GBR_QosInformation_t *gBR_QosInformation;

    smf_bearer_t *qos_flow = NULL;

    ogs_assert(sess);

    ogs_debug("PDUSessionResourceModifyRequestTransfer");
    memset(&message, 0, sizeof(NGAP_PDUSessionResourceModifyRequestTransfer_t));

    ie = CALLOC(1, sizeof(NGAP_PDUSessionResourceModifyRequestTransferIEs_t));
    ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_QosFlowAddOrModifyRequestList;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceModifyRequestTransferIEs__value_PR_QosFlowAddOrModifyRequestList;

    QosFlowAddOrModifyRequestList = &ie->value.choice.QosFlowAddOrModifyRequestList;

    ogs_list_for_each_entry(
            &sess->qos_flow_to_modify_list, qos_flow, to_modify_node) {

        QosFlowAddOrModifyRequestItem =
            CALLOC(1, sizeof(*QosFlowAddOrModifyRequestItem));
        ASN_SEQUENCE_ADD(&QosFlowAddOrModifyRequestList->list, QosFlowAddOrModifyRequestItem);

        qosFlowIdentifier = &QosFlowAddOrModifyRequestItem->qosFlowIdentifier;

        QosFlowAddOrModifyRequestItem->qosFlowLevelQosParameters =
            qosFlowLevelQosParameters =
                CALLOC(1, sizeof(*qosFlowLevelQosParameters));

        allocationAndRetentionPriority =
            &qosFlowLevelQosParameters->allocationAndRetentionPriority;
        qosCharacteristics = &qosFlowLevelQosParameters->qosCharacteristics;

        qosCharacteristics->present = NGAP_QosCharacteristics_PR_nonDynamic5QI;
        qosCharacteristics->choice.nonDynamic5QI =
            nonDynamic5QI = CALLOC(1, sizeof(struct NGAP_NonDynamic5QIDescriptor));

        *qosFlowIdentifier = qos_flow->qfi;

        nonDynamic5QI->fiveQI = qos_flow->qos.index;

        allocationAndRetentionPriority->priorityLevelARP =
            qos_flow->qos.arp.priority_level;
        if (qos_flow->qos.arp.pre_emption_capability ==
                OGS_5GC_PRE_EMPTION_ENABLED)
            allocationAndRetentionPriority->pre_emptionCapability =
                NGAP_Pre_emptionCapability_may_trigger_pre_emption;
        if (qos_flow->qos.arp.pre_emption_vulnerability ==
                OGS_5GC_PRE_EMPTION_ENABLED)
            allocationAndRetentionPriority->pre_emptionVulnerability =
                NGAP_Pre_emptionVulnerability_pre_emptable;

        if (qos_presence == true &&
            (qos_flow->qos.mbr.downlink || qos_flow->qos.mbr.uplink ||
             qos_flow->qos.gbr.downlink || qos_flow->qos.gbr.uplink)) {
            ogs_assert(qos_flow->qos.mbr.downlink);
            ogs_assert(qos_flow->qos.mbr.uplink);
            ogs_assert(qos_flow->qos.gbr.downlink);
            ogs_assert(qos_flow->qos.gbr.uplink);

            qosFlowLevelQosParameters->gBR_QosInformation =
                gBR_QosInformation = CALLOC(1, sizeof(*gBR_QosInformation));

            asn_uint642INTEGER(&gBR_QosInformation->maximumFlowBitRateDL,
                    qos_flow->qos.mbr.downlink);
            asn_uint642INTEGER(&gBR_QosInformation->maximumFlowBitRateUL,
                    qos_flow->qos.mbr.uplink);
            asn_uint642INTEGER(&gBR_QosInformation->
                    guaranteedFlowBitRateDL, qos_flow->qos.gbr.downlink);
            asn_uint642INTEGER(&gBR_QosInformation->
                    guaranteedFlowBitRateUL, qos_flow->qos.gbr.uplink);
        }
    }

    return ogs_asn_encode(
            &asn_DEF_NGAP_PDUSessionResourceModifyRequestTransfer, &message);
}

ogs_pkbuf_t *ngap_build_pdu_session_resource_release_request_transfer(
        smf_sess_t *sess, NGAP_Cause_PR group, long cause)
{
    NGAP_PDUSessionResourceModifyRequestTransfer_t message;

    NGAP_PDUSessionResourceModifyRequestTransferIEs_t *ie = NULL;

    NGAP_QosFlowListWithCause_t *QosFlowListWithCause = NULL;
    NGAP_QosFlowWithCauseItem_t *QosFlowWithCauseItem = NULL;
    NGAP_QosFlowIdentifier_t *qosFlowIdentifier = NULL;
    NGAP_Cause_t *Cause = NULL;

    smf_bearer_t *qos_flow = NULL;

    ogs_assert(sess);

    ogs_debug("PDUSessionResourceModifyRequestTransfer");
    memset(&message, 0, sizeof(NGAP_PDUSessionResourceModifyRequestTransfer_t));

    ie = CALLOC(1, sizeof(NGAP_PDUSessionResourceModifyRequestTransferIEs_t));
    ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_QosFlowToReleaseList;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceModifyRequestTransferIEs__value_PR_QosFlowListWithCause;

    QosFlowListWithCause = &ie->value.choice.QosFlowListWithCause;

    ogs_list_for_each_entry(
            &sess->qos_flow_to_modify_list, qos_flow, to_modify_node) {

        QosFlowWithCauseItem = CALLOC(1, sizeof(*QosFlowWithCauseItem));
        ASN_SEQUENCE_ADD(&QosFlowListWithCause->list, QosFlowWithCauseItem);

        qosFlowIdentifier = &QosFlowWithCauseItem->qosFlowIdentifier;

        *qosFlowIdentifier = qos_flow->qfi;

        Cause = &QosFlowWithCauseItem->cause;
        Cause->present = group;
        Cause->choice.radioNetwork = cause;

    }

    return ogs_asn_encode(
            &asn_DEF_NGAP_PDUSessionResourceModifyRequestTransfer, &message);
}

ogs_pkbuf_t *ngap_build_pdu_session_resource_release_command_transfer(
        smf_sess_t *sess, int state, NGAP_Cause_PR group, long cause)
{
    NGAP_PDUSessionResourceReleaseCommandTransfer_t message;
    NGAP_Cause_t *Cause = NULL;

    ogs_assert(sess);
    ogs_assert(state);
    sess->ngap_state.pdu_session_resource_release = state;

    ogs_debug("PDUSessionResourceReleaseCommandTransfer");
    memset(&message, 0,
            sizeof(NGAP_PDUSessionResourceReleaseCommandTransfer_t));

    ogs_debug("    Group[%d] Cause[%d]", group, (int)cause);

    Cause = &message.cause;
    Cause->present = group;
    Cause->choice.radioNetwork = cause;

    return ogs_asn_encode(
            &asn_DEF_NGAP_PDUSessionResourceReleaseCommandTransfer, &message);
}

ogs_pkbuf_t *ngap_build_path_switch_request_ack_transfer(smf_sess_t *sess)
{
    NGAP_PathSwitchRequestAcknowledgeTransfer_t message;

#if 0 /* The following is optional. So I've removed */
    ogs_ip_t upf_n3_ip;

    NGAP_UPTransportLayerInformation_t *UPTransportLayerInformation = NULL;
    NGAP_GTPTunnel_t *gTPTunnel = NULL;
#endif

    ogs_assert(sess);

    ogs_debug("PathSwitchRequestAcknowledgeTransfer");
    memset(&message, 0, sizeof(NGAP_PathSwitchRequestAcknowledgeTransfer_t));

#if 0 /* The following is optional. So I've removed */

    message.uL_NGU_UP_TNLInformation = CALLOC(1, sizeof(NGAP_UPTransportLayerInformation_t));
    UPTransportLayerInformation = message.uL_NGU_UP_TNLInformation;

    gTPTunnel = CALLOC(1, sizeof(struct NGAP_GTPTunnel));
    UPTransportLayerInformation->present =
        NGAP_UPTransportLayerInformation_PR_gTPTunnel;
    UPTransportLayerInformation->choice.gTPTunnel = gTPTunnel;

    ogs_assert(OGS_OK == ogs_sockaddr_to_ip(
                sess->upf_n3_addr, sess->upf_n3_addr6, &upf_n3_ip));
    ogs_asn_ip_to_BIT_STRING(&upf_n3_ip, &gTPTunnel->transportLayerAddress);
    ogs_asn_uint32_to_OCTET_STRING(sess->upf_n3_teid, &gTPTunnel->gTP_TEID);

#endif

    return ogs_asn_encode(
            &asn_DEF_NGAP_PathSwitchRequestAcknowledgeTransfer, &message);
}

ogs_pkbuf_t *ngap_build_handover_command_transfer(smf_sess_t *sess)
{
    NGAP_HandoverCommandTransfer_t message;

    ogs_ip_t upf_dl_ip;

    ogs_assert(sess);

    ogs_debug("HandoverCommandTransfer");
    memset(&message, 0, sizeof(NGAP_HandoverCommandTransfer_t));

    if (sess->handover.indirect_data_forwarding == true) {
        ogs_pfcp_pdr_t *pdr = NULL;

        NGAP_UPTransportLayerInformation_t
            *dLForwardingUP_TNLInformation = NULL;
        NGAP_GTPTunnel_t *gTPTunnel = NULL;
        NGAP_QosFlowToBeForwardedList_t *qosFlowToBeForwardedList = NULL;

        message.dLForwardingUP_TNLInformation = dLForwardingUP_TNLInformation =
            CALLOC(1, sizeof(*dLForwardingUP_TNLInformation));
        ogs_assert(dLForwardingUP_TNLInformation);

        dLForwardingUP_TNLInformation->present =
            NGAP_UPTransportLayerInformation_PR_gTPTunnel;
        dLForwardingUP_TNLInformation->choice.gTPTunnel = gTPTunnel =
            CALLOC(1, sizeof(*gTPTunnel));
        ogs_assert(gTPTunnel);

        ogs_assert(OGS_OK == ogs_sockaddr_to_ip(
                sess->handover.upf_dl_addr, sess->handover.upf_dl_addr6,
                &upf_dl_ip));
        ogs_asn_ip_to_BIT_STRING(&upf_dl_ip, &gTPTunnel->transportLayerAddress);
        ogs_asn_uint32_to_OCTET_STRING(
                sess->handover.upf_dl_teid, &gTPTunnel->gTP_TEID);

        ogs_list_for_each(&sess->pfcp.pdr_list, pdr) {
            ogs_pfcp_far_t *far = pdr->far;
            ogs_assert(far);

            if (pdr->src_if == OGS_PFCP_INTERFACE_ACCESS &&
                far->dst_if == OGS_PFCP_INTERFACE_ACCESS) {
                NGAP_QosFlowToBeForwardedItem_t *qosFlowToBeForwardedItem;
                NGAP_QosFlowIdentifier_t *qosFlowIdentifier = NULL;

                if (!qosFlowToBeForwardedList) {
                    message.qosFlowToBeForwardedList =
                        qosFlowToBeForwardedList =
                            CALLOC(1, sizeof(*qosFlowToBeForwardedList));
                    ogs_assert(qosFlowToBeForwardedList);
                }

                qosFlowToBeForwardedItem =
                    CALLOC(1, sizeof(*qosFlowToBeForwardedItem));
                ogs_assert(qosFlowToBeForwardedItem);

                ASN_SEQUENCE_ADD(&qosFlowToBeForwardedList->list,
                        qosFlowToBeForwardedItem);

                qosFlowIdentifier =
                    &qosFlowToBeForwardedItem->qosFlowIdentifier;

                *qosFlowIdentifier = pdr->qfi;
            }
        }
    }

    return ogs_asn_encode(&asn_DEF_NGAP_HandoverCommandTransfer, &message);
}

ogs_pkbuf_t *ngap_build_mbs_session_setup_or_modification_request_transfer(smf_mbs_sess_t *mbs_sess)
{
    /*
    maxnoofMBSQoSFlows - Maximum no. of QoS Flows allowed within one MBS session. Value is 64.
    maxnoofMBSFSAs - Maximum no. of FSA IDs for one MBS session. Value is 64.
    */

    ogs_uint24_t mbs_session_FSA_id;

    NGAP_MBSSessionSetupOrModRequestTransfer_t message;

    NGAP_MBSSessionSetupOrModRequestTransferIEs_t *ie = NULL;

    // MBS Session TNL Information 5GC
    NGAP_MBS_SessionTNLInfo5GC_t *mBS_SessionTNLInfo5GC = NULL;

    // Broadcast service is location dependent
    NGAP_MBS_SessionTNLInfo5GCList_t *mBS_SessionTNLInfo5GCList = NULL;

    NGAP_MBS_SessionTNLInfo5GCItem_t *mBS_SessionTNLInfo5GCItem = NULL;

    NGAP_MBS_AreaSessionID_t *mBS_AreaSessionID = NULL;

    NGAP_SharedNGU_MulticastTNLInformation_t *sharedNGU_MulticastTNLInformation = NULL;
    NGAP_TransportLayerAddress_t *iP_MulticastAddress = NULL;
	NGAP_TransportLayerAddress_t *iP_SourceAddress = NULL;
    NGAP_GTP_TEID_t *gTP_TEID = NULL;

    // MBS QoS Flows To Be Setup or Modified List
    NGAP_MBS_QoSFlowsToBeSetupList_t *mBS_QoSFlowsToBeSetupList;

    NGAP_MBS_QoSFlowsToBeSetupItem_t *mBS_QoSFlowsToBeSetupItem;

    NGAP_QosFlowIdentifier_t *mBS_QoSFlowIdentifier = NULL;
	NGAP_QosFlowLevelQosParameters_t *mBS_QoSFlowLevelQoSParameters = NULL;

    NGAP_QosCharacteristics_t *qoSCharacteristics = NULL;
    NGAP_NonDynamic5QIDescriptor_t *nonDynamic5QIDescriptor = NULL;
    NGAP_FiveQI_t *fiveQI = NULL;

    NGAP_AllocationAndRetentionPriority_t *allocationAndRetentionPriority = NULL;
    NGAP_PriorityLevelARP_t *priorityLevelARP = NULL;
	NGAP_Pre_emptionCapability_t *pre_emptionCapability = NULL;
	NGAP_Pre_emptionVulnerability_t *pre_emptionVulnerability = NULL;

    // MBS Session FSA ID List (MBS Frequency Selection Area Identity)
    NGAP_MBS_SessionFSAIDList_t *mBS_SessionFSAIDList = NULL;
    NGAP_MBS_SessionFSAID_t *mBS_SessionFSAID = NULL;

    ogs_assert(mbs_sess);

    ogs_debug("MBSSessionSetupOrModRequestTransfer");
    memset(&message, 0, sizeof(NGAP_MBSSessionSetupOrModRequestTransfer_t));

    // MBS Session TNL Information 5GC - 9.3.2.15 (O)
    ie = CALLOC(1, sizeof(NGAP_MBSSessionSetupOrModRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_MBS_SessionTNLInfo5GC;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_MBSSessionSetupOrModRequestTransferIEs__value_PR_MBS_SessionTNLInfo5GC;

    mBS_SessionTNLInfo5GC = &ie->value.choice.MBS_SessionTNLInfo5GC;

    mBS_SessionTNLInfo5GCList = CALLOC(1, sizeof(NGAP_MBS_SessionTNLInfo5GCList_t));
    ogs_assert(mBS_SessionTNLInfo5GCList);

    mBS_SessionTNLInfo5GC->present = NGAP_MBS_SessionTNLInfo5GC_PR_locationdependent;
    mBS_SessionTNLInfo5GC->choice.locationdependent = mBS_SessionTNLInfo5GCList;

    mBS_SessionTNLInfo5GCItem = CALLOC(1, sizeof(NGAP_MBS_SessionTNLInfo5GCItem_t));
    ogs_assert(mBS_SessionTNLInfo5GCItem);
    ASN_SEQUENCE_ADD(mBS_SessionTNLInfo5GCList, mBS_SessionTNLInfo5GCItem);

    // MBS Area Session ID - 9.3.1.207 (M)
    mBS_AreaSessionID = &mBS_SessionTNLInfo5GCItem->mBS_AreaSessionID;
    // TODO (borieher): Fill the MBS Area Session ID without hardcoded values
    *mBS_AreaSessionID = 13;

    // Shared NG-U Multicast TNL Information - 9.3.2.16 (M)
    sharedNGU_MulticastTNLInformation = &mBS_SessionTNLInfo5GCItem->sharedNGU_MulticastTNLInformation;

    // Transport Layer Address - 9.3.2.4 (M)
    // TODO (borieher): Fix this
    iP_MulticastAddress = &sharedNGU_MulticastTNLInformation->iP_MulticastAddress;
    ogs_asn_ip_to_BIT_STRING(&mbs_sess->ll_ssm.dest_ip_addr, iP_MulticastAddress);

    // Transport Layer Address - 9.3.2.4 (M)
    // TODO (borieher): Or this
    iP_SourceAddress = &sharedNGU_MulticastTNLInformation->iP_SourceAddress;
    ogs_asn_ip_to_BIT_STRING(&mbs_sess->ll_ssm.src_ip_addr, iP_SourceAddress);

    // GTP-TEID - 9.3.2.5 (M)
    gTP_TEID = &sharedNGU_MulticastTNLInformation->gTP_TEID;
    ogs_asn_uint32_to_OCTET_STRING(mbs_sess->c_teid, gTP_TEID);

    // MBS QoS Flows To Be Setup List - 9.3.1.236 (M)
    ie = CALLOC(1, sizeof(NGAP_MBSSessionSetupOrModRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_MBS_QoSFlowsToBeSetupList;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_MBSSessionSetupOrModRequestTransferIEs__value_PR_MBS_QoSFlowsToBeSetupList;

    mBS_QoSFlowsToBeSetupList = &ie->value.choice.MBS_QoSFlowsToBeSetupList;

    mBS_QoSFlowsToBeSetupItem = CALLOC(1, sizeof(NGAP_MBS_QoSFlowsToBeSetupItem_t));
    ogs_assert(mBS_QoSFlowsToBeSetupItem);
    ASN_SEQUENCE_ADD(mBS_QoSFlowsToBeSetupList, mBS_QoSFlowsToBeSetupItem);

    // QoS Flow Identifier - 9.3.1.51 (M)
    mBS_QoSFlowIdentifier = &mBS_QoSFlowsToBeSetupItem->mBSqosFlowIdentifier;
    // TODO (borieher): Fill the MBS QoS Flow Identifier without hardcoded values
    *mBS_QoSFlowIdentifier = 13;

    // Non Dynamic 5QI Descriptor - 9.3.1.28 (M)
    nonDynamic5QIDescriptor = CALLOC(1, sizeof(NGAP_NonDynamic5QIDescriptor_t));
    ogs_assert(nonDynamic5QIDescriptor);

    // 5QI - INTEGER (M)
    fiveQI = &nonDynamic5QIDescriptor->fiveQI;
    // TODO (borieher): Fill the 5QI without hardcoded values
    *fiveQI = 9;

    // QoS Flow Level QoS Parameters - 9.3.1.12 (M)
    mBS_QoSFlowLevelQoSParameters = &mBS_QoSFlowsToBeSetupItem->mBSqosFlowLevelQosParameters;

    qoSCharacteristics = &mBS_QoSFlowLevelQoSParameters->qosCharacteristics;
    qoSCharacteristics->present = NGAP_QosCharacteristics_PR_nonDynamic5QI;
    qoSCharacteristics->choice.nonDynamic5QI = nonDynamic5QIDescriptor;

    // Allocation and Retention Priority - 9.3.1.19 (M)
    allocationAndRetentionPriority = &mBS_QoSFlowLevelQoSParameters->allocationAndRetentionPriority;

    // Priority Level - INTEGER (M)
    priorityLevelARP = &allocationAndRetentionPriority->priorityLevelARP;
    // TODO (borieher): Fill the Priority Level ARP without hardcoded values
    *priorityLevelARP = 13;

    // Pre-emption Capability - ENUMERATED (M)
    pre_emptionCapability = &allocationAndRetentionPriority->pre_emptionCapability;
    // TODO (borieher): Fill the Pre-emption Capability without hardcoded values
    *pre_emptionCapability = NGAP_Pre_emptionCapability_shall_not_trigger_pre_emption;

    // Pre-emption Vulnerability - ENUMERATED (M)
    pre_emptionVulnerability = &allocationAndRetentionPriority->pre_emptionVulnerability;
    // TODO (borieher): Fill the Pre-emption Vulnerability without hardcoded values
    *pre_emptionVulnerability = NGAP_Pre_emptionVulnerability_not_pre_emptable;

    // MBS Frequency Selection Area Identity - OCTET STRING (SIZE(3)) (M)
    ie = CALLOC(1, sizeof(NGAP_MBSSessionSetupOrModRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(&message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_MBS_SessionFSAIDList;
    ie->criticality = NGAP_Criticality_ignore;
    ie->value.present = NGAP_MBSSessionSetupOrModRequestTransferIEs__value_PR_MBS_SessionFSAIDList;

    mBS_SessionFSAIDList = &ie->value.choice.MBS_SessionFSAIDList;

    mBS_SessionFSAID = CALLOC(1, sizeof(NGAP_MBS_SessionFSAID_t));
    ogs_assert(mBS_SessionFSAID);
    ASN_SEQUENCE_ADD(mBS_SessionFSAIDList, mBS_SessionFSAID);

    // MBS Frequency Selection Area Identity - OCTET STRING (M)
    // TODO (borieher): Fill MBS Frequency Selection Area Identity without hardcoded values
    mbs_session_FSA_id = ogs_uint24_from_string(ogs_strdup("13"));
    ogs_asn_uint24_to_OCTET_STRING(mbs_session_FSA_id, mBS_SessionFSAID);

    return ogs_asn_encode(
            &asn_DEF_NGAP_MBSSessionSetupOrModRequestTransfer, &message);
}
