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

/**
 * Fill common QoS flow level parameters: 5QI, ARP, and optional GBR/MBR.
 */
static void fill_qos_level_parameters(
    NGAP_QosFlowLevelQosParameters_t *params,
    const ogs_qos_t *qos,
    bool include_gbr)
{
    NGAP_AllocationAndRetentionPriority_t
        *allocationAndRetentionPriority = NULL;
    NGAP_QosCharacteristics_t *qosCharacteristics = NULL;
    NGAP_NonDynamic5QIDescriptor_t *nonDynamic5QI = NULL;

    ogs_assert(params);
    ogs_assert(qos);

    /* Allocation and Retention Priority */
    if (!params->allocationAndRetentionPriority) {
        params->allocationAndRetentionPriority =
            CALLOC(1, sizeof(*params->allocationAndRetentionPriority));
        ogs_assert(params->allocationAndRetentionPriority);
    }
    allocationAndRetentionPriority = params->allocationAndRetentionPriority;

    allocationAndRetentionPriority->priorityLevelARP = qos->arp.priority_level;
    if (qos->arp.pre_emption_capability == OGS_5GC_PRE_EMPTION_ENABLED)
        allocationAndRetentionPriority->pre_emptionCapability =
            NGAP_Pre_emptionCapability_may_trigger_pre_emption;
    if (qos->arp.pre_emption_vulnerability == OGS_5GC_PRE_EMPTION_ENABLED)
        allocationAndRetentionPriority->pre_emptionVulnerability =
            NGAP_Pre_emptionVulnerability_pre_emptable;

    /* Non-Dynamic 5QI Descriptor */
    if (!params->qosCharacteristics) {
        params->qosCharacteristics =
            CALLOC(1, sizeof(*params->qosCharacteristics));
        ogs_assert(params->qosCharacteristics);
    }
    qosCharacteristics = params->qosCharacteristics;
    qosCharacteristics->choice.nonDynamic5QI = nonDynamic5QI =
        CALLOC(1, sizeof(struct NGAP_NonDynamic5QIDescriptor));
    ogs_assert(nonDynamic5QI);
    qosCharacteristics->present = NGAP_QosCharacteristics_PR_nonDynamic5QI;

    nonDynamic5QI->fiveQI = qos->index;

    /* Optional GBR/MBR Information */
    if (include_gbr &&
        qos->mbr.downlink && qos->mbr.uplink &&
        qos->gbr.downlink && qos->gbr.uplink) {
        NGAP_GBR_QosInformation_t *gBR_QosInformation =
            params->gBR_QosInformation = CALLOC(1, sizeof(*gBR_QosInformation));
        ogs_assert(gBR_QosInformation);

        ogs_assert(qos->mbr.downlink <= OGS_MAX_BITRATE_NGAP);
        ogs_assert(qos->mbr.uplink <= OGS_MAX_BITRATE_NGAP);
        ogs_assert(qos->gbr.downlink <= OGS_MAX_BITRATE_NGAP);
        ogs_assert(qos->gbr.uplink <= OGS_MAX_BITRATE_NGAP);

        asn_uint642INTEGER(&gBR_QosInformation->maximumFlowBitRateDL,
                qos->mbr.downlink);
        asn_uint642INTEGER(&gBR_QosInformation->maximumFlowBitRateUL,
                qos->mbr.uplink);
        asn_uint642INTEGER(&gBR_QosInformation->
                guaranteedFlowBitRateDL, qos->gbr.downlink);
        asn_uint642INTEGER(&gBR_QosInformation->
                guaranteedFlowBitRateUL, qos->gbr.uplink);

        if (qos->qnc) {
            gBR_QosInformation->notificationControl =
                CALLOC(1, sizeof(*gBR_QosInformation->notificationControl));
            ogs_assert(gBR_QosInformation->notificationControl);
            *gBR_QosInformation->notificationControl =
                NGAP_NotificationControl_notification_requested;
        }
    } else if (include_gbr &&
               (qos->mbr.downlink || qos->mbr.uplink ||
                qos->gbr.downlink || qos->gbr.uplink)) {
        ogs_error("Missing one or more MBR/GBR parameters; "
                "defaulting to Non-GBR flow ");
        ogs_error("    MBR[DL:%lld,UL:%lld]",
            (long long)qos->mbr.downlink, (long long)qos->mbr.uplink);
        ogs_error("    GBR[DL:%lld,UL:%lld]",
            (long long)qos->gbr.downlink, (long long)qos->gbr.uplink);
    }
}


/*
 * The Alternative QoS Parameters Set List and the PDU Set QoS Parameters of a QoS flow (TS 38.413 V19.1.0 clauses 9.3.1.152 and 9.3.1.153 and the
 * PDU Set QoS Parameters IE of the QoS Flow Level QoS Parameters IE; TS 29.512 V19.7.0 clauses 4.2.6.2.1 and 4.2.6.6.2). The list is an
 * extension of the GBR QoS Information, so it goes with GBR flows only; the index of an item is its position in the list, 1 having the
 * highest priority. The packet delay budget is in units of 0.5 ms (clause 9.3.1.80) and the PDU Set delay budget in 0.01 ms (clause 9.3.1.135).
 */
static NGAP_PacketErrorRate_t *per_alloc(const char *string)
{
    NGAP_PacketErrorRate_t *per = NULL;

    if (!string || !string[0])
        return NULL;
    /* PacketErrRate of TS 29.571: one digit, "E-", one digit; the scalar and the exponent of TS 38.413 clause 9.3.1.80 are 0 to 9 */
    if (strlen(string) != 4 || string[0] < '0' || string[0] > '9' || string[1] != 'E' || string[2] != '-' ||
            string[3] < '0' || string[3] > '9') {
        ogs_error("The packet error rate [%s] is not of the form of PacketErrRate (digit, E-, digit) and is not sent", string);
        return NULL;
    }
    per = CALLOC(1, sizeof(*per));
    ogs_assert(per);
    per->pERScalar = string[0] - '0';
    per->pERExponent = string[3] - '0';
    return per;
}

static void alt_item_add_ext(NGAP_AlternativeQoSParaSetItem_t *item, long id, int present, void *value)
{
    NGAP_ProtocolExtensionContainer_14713P19_t *container = (NGAP_ProtocolExtensionContainer_14713P19_t *)item->iE_Extensions;
    NGAP_AlternativeQoSParaSetItem_ExtIEs_t *ext = NULL;

    if (!container) {
        container = CALLOC(1, sizeof(*container));
        ogs_assert(container);
        item->iE_Extensions = (struct NGAP_ProtocolExtensionContainer *)container;
    }
    ext = CALLOC(1, sizeof(*ext));
    ogs_assert(ext);
    ASN_SEQUENCE_ADD(&container->list, ext);
    ext->id = id;
    ext->criticality = NGAP_Criticality_ignore;
    ext->extensionValue.present = present;
    switch (present) {
    case NGAP_AlternativeQoSParaSetItem_ExtIEs__extensionValue_PR_ExtendedPacketDelayBudget:
        ext->extensionValue.choice.ExtendedPacketDelayBudget = value;
        break;
    case NGAP_AlternativeQoSParaSetItem_ExtIEs__extensionValue_PR_ExtendedPacketDelayBudget_1:
        ext->extensionValue.choice.ExtendedPacketDelayBudget_1 = value;
        break;
    case NGAP_AlternativeQoSParaSetItem_ExtIEs__extensionValue_PR_PacketErrorRate:
        ext->extensionValue.choice.PacketErrorRate = value;
        break;
    default:
        ext->extensionValue.choice.PacketErrorRate_1 = value;
        break;
    }
}

static long *long_alloc(long value)
{
    long *p = CALLOC(1, sizeof(*p));

    ogs_assert(p);
    *p = value;
    return p;
}

static NGAP_PDUsetQoSInformation_t *pdu_set_information(const ogs_pdu_set_qos_t *pdu_set)
{
    NGAP_PDUsetQoSInformation_t *info = NULL;

    if (!pdu_set->presence)
        return NULL;
    info = CALLOC(1, sizeof(*info));
    ogs_assert(info);
    if (pdu_set->delay_budget_presence && pdu_set->delay_budget >= 1 && pdu_set->delay_budget <= 65535)
        info->pduSetDelayBudget = long_alloc(pdu_set->delay_budget);
    else if (pdu_set->delay_budget_presence)
        ogs_error("The PDU Set delay budget [%u] is outside of 1 to 65535 (0.01 ms) and is not sent", pdu_set->delay_budget);
    info->pduSetErrorRate = per_alloc(pdu_set->error_rate);
    if (pdu_set->handling_info == OGS_PDU_SET_ALL_PDUS_NEEDED)
        info->pduSetIntegratedHandlingInformation =
            long_alloc(NGAP_PDUsetQoSInformation__pduSetIntegratedHandlingInformation_true);
    else if (pdu_set->handling_info == OGS_PDU_SET_ALL_PDUS_NOT_NEEDED)
        info->pduSetIntegratedHandlingInformation =
            long_alloc(NGAP_PDUsetQoSInformation__pduSetIntegratedHandlingInformation_false);
    return info;
}

static void qos_flow_alt_and_pdu_set(NGAP_QosFlowLevelQosParameters_t *params, const smf_bearer_t *qos_flow)
{
    NGAP_PacketErrorRate_t *per = NULL;
    int i;

    if (!qos_flow)
        return;

    if (params->gBR_QosInformation && qos_flow->alt_qos.num_of_set) {
        NGAP_ProtocolExtensionContainer_14713P128_t *container = CALLOC(1, sizeof(*container));
        NGAP_GBR_QosInformation_ExtIEs_t *ext = CALLOC(1, sizeof(*ext));
        NGAP_AlternativeQoSParaSetList_t *list = CALLOC(1, sizeof(*list));

        ogs_assert(container);
        ogs_assert(ext);
        ogs_assert(list);
        for (i = 0; i < qos_flow->alt_qos.num_of_set; i++) {
            const ogs_alt_qos_t *alt = &qos_flow->alt_qos;
            NGAP_AlternativeQoSParaSetItem_t *item = CALLOC(1, sizeof(*item));

            ogs_assert(item);
            ASN_SEQUENCE_ADD(&list->list, item);
            item->alternativeQoSParaSetIndex = i + 1;
            if (alt->set[i].gbr.downlink) {
                item->guaranteedFlowBitRateDL = CALLOC(1, sizeof(*item->guaranteedFlowBitRateDL));
                ogs_assert(item->guaranteedFlowBitRateDL);
                asn_uint642INTEGER(item->guaranteedFlowBitRateDL, alt->set[i].gbr.downlink);
            }
            if (alt->set[i].gbr.uplink) {
                item->guaranteedFlowBitRateUL = CALLOC(1, sizeof(*item->guaranteedFlowBitRateUL));
                ogs_assert(item->guaranteedFlowBitRateUL);
                asn_uint642INTEGER(item->guaranteedFlowBitRateUL, alt->set[i].gbr.uplink);
            }
            if (alt->set[i].pdb_presence) {
                if (alt->set[i].pdb * 2 <= 1023)
                    item->packetDelayBudget = long_alloc(alt->set[i].pdb * 2);
                else
                    ogs_error("The packet delay budget [%u ms] of the alternative set is outside of 0.5 ms units up to 1023 and is not sent",
                            alt->set[i].pdb);
            }
            item->packetErrorRate = per_alloc(alt->set[i].per);

            /* the PDU Set delay budgets and error rates of the set (clause 9.3.1.151) */
            if (alt->set[i].pdu_set_dl.presence && alt->set[i].pdu_set_dl.delay_budget_presence)
                alt_item_add_ext(item, NGAP_ProtocolIE_ID_id_PduSetDelayBudgetDownlink,
                        NGAP_AlternativeQoSParaSetItem_ExtIEs__extensionValue_PR_ExtendedPacketDelayBudget,
                        long_alloc(alt->set[i].pdu_set_dl.delay_budget));
            if (alt->set[i].pdu_set_ul.presence && alt->set[i].pdu_set_ul.delay_budget_presence)
                alt_item_add_ext(item, NGAP_ProtocolIE_ID_id_PduSetDelayBudgetUplink,
                        NGAP_AlternativeQoSParaSetItem_ExtIEs__extensionValue_PR_ExtendedPacketDelayBudget_1,
                        long_alloc(alt->set[i].pdu_set_ul.delay_budget));
            if (alt->set[i].pdu_set_dl.presence && (per = per_alloc(alt->set[i].pdu_set_dl.error_rate)))
                alt_item_add_ext(item, NGAP_ProtocolIE_ID_id_PduSetErrorRateDownlink,
                        NGAP_AlternativeQoSParaSetItem_ExtIEs__extensionValue_PR_PacketErrorRate, per);
            if (alt->set[i].pdu_set_ul.presence && (per = per_alloc(alt->set[i].pdu_set_ul.error_rate)))
                alt_item_add_ext(item, NGAP_ProtocolIE_ID_id_PduSetErrorRateUplink,
                        NGAP_AlternativeQoSParaSetItem_ExtIEs__extensionValue_PR_PacketErrorRate_1, per);
        }
        ASN_SEQUENCE_ADD(&container->list, ext);
        ext->id = NGAP_ProtocolIE_ID_id_AlternativeQoSParaSetList;
        ext->criticality = NGAP_Criticality_ignore;
        ext->extensionValue.present = NGAP_GBR_QosInformation_ExtIEs__extensionValue_PR_AlternativeQoSParaSetList;
        ext->extensionValue.choice.AlternativeQoSParaSetList = list;
        params->gBR_QosInformation->iE_Extensions = (struct NGAP_ProtocolExtensionContainer *)container;
    }

    if (qos_flow->pdu_set_dl.presence || qos_flow->pdu_set_ul.presence) {
        NGAP_ProtocolExtensionContainer_14713P343_t *container = (NGAP_ProtocolExtensionContainer_14713P343_t *)params->iE_Extensions;
        NGAP_QosFlowLevelQosParameters_ExtIEs_t *ext = CALLOC(1, sizeof(*ext));
        NGAP_PDUsetQoSParameters_t *pdu_set = CALLOC(1, sizeof(*pdu_set));

        ogs_assert(ext);
        ogs_assert(pdu_set);
        if (!container) {
            container = CALLOC(1, sizeof(*container));
            ogs_assert(container);
            params->iE_Extensions = (struct NGAP_ProtocolExtensionContainer *)container;
        }
        pdu_set->dlPDUSetQoSInformation = pdu_set_information(&qos_flow->pdu_set_dl);
        pdu_set->ulPDUSetQoSInformation = pdu_set_information(&qos_flow->pdu_set_ul);
        ASN_SEQUENCE_ADD(&container->list, ext);
        ext->id = NGAP_ProtocolIE_ID_id_PDUsetQoSParameters;
        ext->criticality = NGAP_Criticality_ignore;
        ext->extensionValue.present = NGAP_QosFlowLevelQosParameters_ExtIEs__extensionValue_PR_PDUsetQoSParameters;
        ext->extensionValue.choice.PDUsetQoSParameters = pdu_set;
    }
}

/*
 * TS 38.413 V19.1.0 clause 9.3.1.266: ECN Marking at UPF Request tells the NG-RAN node to report the congestion information of a
 * QoS flow, as the UP function marks the packets for L4S (TS 29.512 V19.7.0 clause 4.2.6.2.21).
 */
static NGAP_ECNMarkingorCongestionInformationReportingRequest_t *l4s_request(smf_bearer_t *qos_flow)
{
    NGAP_ECNMarkingorCongestionInformationReportingRequest_t *request = NULL;
    long value;

    if (!qos_flow || (qos_flow->l4s.ind == OGS_PCC_RULE_L4S_NONE && !qos_flow->l4s.stop))
        return NULL;
    switch (qos_flow->l4s.ind) {
    case OGS_PCC_RULE_L4S_UL: value = NGAP_ECNMarkingAtUPFRequest_ul; break;
    case OGS_PCC_RULE_L4S_DL: value = NGAP_ECNMarkingAtUPFRequest_dl; break;
    case OGS_PCC_RULE_L4S_UL_DL: value = NGAP_ECNMarkingAtUPFRequest_both; break;
    default: value = NGAP_ECNMarkingAtUPFRequest_stop; break;
    }
    request = CALLOC(1, sizeof(*request));
    ogs_assert(request);
    request->present = NGAP_ECNMarkingorCongestionInformationReportingRequest_PR_eCNMarkingAtUPFRequest;
    request->choice.eCNMarkingAtUPFRequest = value;
    return request;
}

static void l4s_setup_item_ext(NGAP_QosFlowSetupRequestItem_t *item, smf_bearer_t *qos_flow)
{
    NGAP_ProtocolExtensionContainer_14713P350_t *container = NULL;
    NGAP_QosFlowSetupRequestItem_ExtIEs_t *ext = NULL;
    NGAP_ECNMarkingorCongestionInformationReportingRequest_t *request = l4s_request(qos_flow);

    if (!request)
        return;
    container = CALLOC(1, sizeof(*container));
    ogs_assert(container);
    ext = CALLOC(1, sizeof(*ext));
    ogs_assert(ext);
    ASN_SEQUENCE_ADD(&container->list, ext);
    ext->id = NGAP_ProtocolIE_ID_id_ECNMarkingorCongestionInformationReportingRequest;
    ext->criticality = NGAP_Criticality_ignore;
    ext->extensionValue.present = NGAP_QosFlowSetupRequestItem_ExtIEs__extensionValue_PR_ECNMarkingorCongestionInformationReportingRequest;
    ext->extensionValue.choice.ECNMarkingorCongestionInformationReportingRequest = request;
    item->iE_Extensions = (struct NGAP_ProtocolExtensionContainer *)container;
}

static void l4s_modify_item_ext(NGAP_QosFlowAddOrModifyRequestItem_t *item, smf_bearer_t *qos_flow)
{
    NGAP_ProtocolExtensionContainer_14713P339_t *container = NULL;
    NGAP_QosFlowAddOrModifyRequestItem_ExtIEs_t *ext = NULL;
    NGAP_ECNMarkingorCongestionInformationReportingRequest_t *request = l4s_request(qos_flow);

    if (!request)
        return;
    container = CALLOC(1, sizeof(*container));
    ogs_assert(container);
    ext = CALLOC(1, sizeof(*ext));
    ogs_assert(ext);
    ASN_SEQUENCE_ADD(&container->list, ext);
    ext->id = NGAP_ProtocolIE_ID_id_ECNMarkingorCongestionInformationReportingRequest;
    ext->criticality = NGAP_Criticality_ignore;
    ext->extensionValue.present = NGAP_QosFlowAddOrModifyRequestItem_ExtIEs__extensionValue_PR_ECNMarkingorCongestionInformationReportingRequest;
    ext->extensionValue.choice.ECNMarkingorCongestionInformationReportingRequest = request;
    item->iE_Extensions = (struct NGAP_ProtocolExtensionContainer *)container;
}

ogs_pkbuf_t *ngap_build_pdu_session_resource_setup_request_transfer(
        smf_sess_t *sess)
{
    smf_bearer_t *qos_flow = NULL;
    ogs_ip_t upf_n3_ip;

    NGAP_PDUSessionResourceSetupRequestTransfer_t message;

    NGAP_PDUSessionResourceSetupRequestTransferIEs_t *ie = NULL;
    NGAP_PDUSessionAggregateMaximumBitRate_t *PDUSessionAggregateMaximumBitRate = NULL;
    NGAP_UPTransportLayerInformation_t *UPTransportLayerInformation = NULL;
    NGAP_GTPTunnel_t *gTPTunnel = NULL;
    NGAP_DataForwardingNotPossible_t *DataForwardingNotPossible = NULL;
    NGAP_PDUSessionType_t *PDUSessionType = NULL;
    NGAP_SecurityIndication_t *SecurityIndication = NULL;
    NGAP_QosFlowSetupRequestList_t *QosFlowSetupRequestList = NULL;
    NGAP_QosFlowSetupRequestItem_t *QosFlowSetupRequestItem = NULL;

    ogs_assert(sess);

    ogs_debug("PDUSessionResourceSetupRequestTransfer");
    memset(&message, 0, sizeof(NGAP_PDUSessionResourceSetupRequestTransfer_t));
    message.protocolIEs = ogs_asn_calloc_protocol_ies(
            &asn_DEF_NGAP_PDUSessionResourceSetupRequestTransfer);
    ogs_assert(message.protocolIEs);

    if (sess->session.ambr.downlink || sess->session.ambr.uplink) {
        ie = CALLOC(1,
                sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
        ogs_assert(ie);
        ASN_SEQUENCE_ADD(message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_PDUSessionAggregateMaximumBitRate;
        ie->criticality = NGAP_Criticality_reject;
        ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_PDUSessionAggregateMaximumBitRate;

        PDUSessionAggregateMaximumBitRate =
            CALLOC(1, sizeof(*PDUSessionAggregateMaximumBitRate));
        ogs_assert(PDUSessionAggregateMaximumBitRate);
        ie->value.choice.PDUSessionAggregateMaximumBitRate =
            PDUSessionAggregateMaximumBitRate;

        asn_uint642INTEGER(&PDUSessionAggregateMaximumBitRate->
            pDUSessionAggregateMaximumBitRateUL, sess->session.ambr.uplink);
        asn_uint642INTEGER(&PDUSessionAggregateMaximumBitRate->
            pDUSessionAggregateMaximumBitRateDL, sess->session.ambr.downlink);
    }

    ie = CALLOC(1, sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_UL_NGU_UP_TNLInformation;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_UPTransportLayerInformation;

    UPTransportLayerInformation = CALLOC(1, sizeof(*UPTransportLayerInformation));
    ogs_assert(UPTransportLayerInformation);
    ie->value.choice.UPTransportLayerInformation = UPTransportLayerInformation;

    gTPTunnel = CALLOC(1, sizeof(struct NGAP_GTPTunnel));
    ogs_assert(gTPTunnel);
    UPTransportLayerInformation->present =
        NGAP_UPTransportLayerInformation_PR_gTPTunnel;
    UPTransportLayerInformation->choice.gTPTunnel = gTPTunnel;

    ogs_assert(OGS_OK == ogs_sockaddr_to_ip(
                sess->local_ul_addr, sess->local_ul_addr6, &upf_n3_ip));
    ogs_assert(OGS_OK == ogs_asn_ip_to_BIT_STRING(
                &upf_n3_ip, &gTPTunnel->transportLayerAddress));
    ogs_asn_uint32_to_OCTET_STRING(sess->local_ul_teid, &gTPTunnel->gTP_TEID);

    if (sess->handover.data_forwarding_not_possible == true) {
        ie = CALLOC(1,
                sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
        ogs_assert(ie);
        ASN_SEQUENCE_ADD(message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_DataForwardingNotPossible;
        ie->criticality = NGAP_Criticality_reject;
        ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_DataForwardingNotPossible;

        DataForwardingNotPossible = CALLOC(1, sizeof(*DataForwardingNotPossible));
        ogs_assert(DataForwardingNotPossible);
        ie->value.choice.DataForwardingNotPossible = DataForwardingNotPossible;

        *DataForwardingNotPossible =
            NGAP_DataForwardingNotPossible_data_forwarding_not_possible;
    }

    ie = CALLOC(1, sizeof(NGAP_PDUSessionResourceSetupRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_PDUSessionType;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_PDUSessionType;

    PDUSessionType = CALLOC(1, sizeof(*PDUSessionType));
    ogs_assert(PDUSessionType);
    ie->value.choice.PDUSessionType = PDUSessionType;

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
        ASN_SEQUENCE_ADD(message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_SecurityIndication;
        ie->criticality = NGAP_Criticality_reject;
        ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_SecurityIndication;

        SecurityIndication = CALLOC(1, sizeof(*SecurityIndication));
        ogs_assert(SecurityIndication);
        ie->value.choice.SecurityIndication = SecurityIndication;

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
                NGAP_ProtocolExtensionContainer_14713P376_t
                    *extContainer = NULL;
                NGAP_SecurityIndication_ExtIEs_t *extIe = NULL;
                NGAP_MaximumIntegrityProtectedDataRate_t
                    *MaximumIntegrityProtectedDataRate = NULL;

                extContainer = CALLOC(1, sizeof(*extContainer));
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
                    CALLOC(1, sizeof(*MaximumIntegrityProtectedDataRate));
                ogs_assert(MaximumIntegrityProtectedDataRate);
                extIe->extensionValue.choice.MaximumIntegrityProtectedDataRate =
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
    ASN_SEQUENCE_ADD(message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_QosFlowSetupRequestList;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceSetupRequestTransferIEs__value_PR_QosFlowSetupRequestList;

    QosFlowSetupRequestList = CALLOC(1, sizeof(*QosFlowSetupRequestList));
    ogs_assert(QosFlowSetupRequestList);
    ie->value.choice.QosFlowSetupRequestList = QosFlowSetupRequestList;

    if (HOME_ROUTED_ROAMING_IN_VSMF(sess)) {
        OpenAPI_list_t *qosFlowsSetupList = NULL;
        OpenAPI_qos_flow_setup_item_t *qosFlowSetupItem = NULL;
        OpenAPI_qos_flow_profile_t *qosFlowProfile = NULL;
        OpenAPI_lnode_t *node = NULL;

        ogs_qos_t qos;

        qosFlowsSetupList = sess->h_smf_qos_flows_setup_list;
        ogs_assert(qosFlowsSetupList);

        node = qosFlowsSetupList->first;
        ogs_assert(node);
        qosFlowSetupItem = node->data;
        ogs_assert(qosFlowSetupItem);

        qosFlowProfile = qosFlowSetupItem->qos_flow_profile;
        ogs_assert(qosFlowProfile);

        memset(&qos, 0, sizeof(qos));

        qos.index = qosFlowProfile->_5qi;
        ogs_assert(qosFlowProfile->arp);
        qos.arp.priority_level = qosFlowProfile->arp->priority_level;
        if (qosFlowProfile->arp->preempt_cap ==
            OpenAPI_preemption_capability_NOT_PREEMPT)
            qos.arp.pre_emption_capability = OGS_5GC_PRE_EMPTION_DISABLED;
        else if (qosFlowProfile->arp->preempt_cap ==
            OpenAPI_preemption_capability_MAY_PREEMPT)
            qos.arp.pre_emption_capability = OGS_5GC_PRE_EMPTION_ENABLED;
        else {
            ogs_error("Invalid preempt_cap [%d]",
                    qosFlowProfile->arp->preempt_cap);
            ogs_assert_if_reached();
        }

        if (qosFlowProfile->arp->preempt_vuln ==
            OpenAPI_preemption_vulnerability_NOT_PREEMPTABLE)
            qos.arp.pre_emption_vulnerability = OGS_5GC_PRE_EMPTION_DISABLED;
        else if (qosFlowProfile->arp->preempt_vuln ==
            OpenAPI_preemption_vulnerability_PREEMPTABLE)
            qos.arp.pre_emption_vulnerability = OGS_5GC_PRE_EMPTION_ENABLED;
        else {
            ogs_error("Invalid preempt_vuln [%d]",
                    qosFlowProfile->arp->preempt_vuln);
            ogs_assert_if_reached();
        }

        QosFlowSetupRequestItem =
            CALLOC(1, sizeof(struct NGAP_QosFlowSetupRequestItem));
        ogs_assert(QosFlowSetupRequestItem);
        ASN_SEQUENCE_ADD(&QosFlowSetupRequestList->list,
            QosFlowSetupRequestItem);

        QosFlowSetupRequestItem->qosFlowIdentifier = qosFlowSetupItem->qfi;
        QosFlowSetupRequestItem->qosFlowLevelQosParameters =
            CALLOC(1,
                    sizeof(*QosFlowSetupRequestItem->
                        qosFlowLevelQosParameters));
        ogs_assert(QosFlowSetupRequestItem->qosFlowLevelQosParameters);

        fill_qos_level_parameters(
                QosFlowSetupRequestItem->qosFlowLevelQosParameters,
                &qos, true);
        qos_flow_alt_and_pdu_set(QosFlowSetupRequestItem->qosFlowLevelQosParameters, smf_qos_flow_find_by_qfi(sess, qosFlowSetupItem->qfi));
        l4s_setup_item_ext(QosFlowSetupRequestItem, smf_qos_flow_find_by_qfi(sess, qosFlowSetupItem->qfi));

    } else {
        ogs_list_for_each(&sess->bearer_list, qos_flow) {
            QosFlowSetupRequestItem =
                CALLOC(1, sizeof(struct NGAP_QosFlowSetupRequestItem));
            ogs_assert(QosFlowSetupRequestItem);
            ASN_SEQUENCE_ADD(&QosFlowSetupRequestList->list,
                QosFlowSetupRequestItem);

            QosFlowSetupRequestItem->qosFlowIdentifier = qos_flow->qfi;
            QosFlowSetupRequestItem->qosFlowLevelQosParameters =
                CALLOC(1,
                        sizeof(*QosFlowSetupRequestItem->
                            qosFlowLevelQosParameters));
            ogs_assert(QosFlowSetupRequestItem->qosFlowLevelQosParameters);

            fill_qos_level_parameters(
                    QosFlowSetupRequestItem->qosFlowLevelQosParameters,
                    &qos_flow->qos, true);
            qos_flow_alt_and_pdu_set(QosFlowSetupRequestItem->qosFlowLevelQosParameters, qos_flow);
            l4s_setup_item_ext(QosFlowSetupRequestItem, qos_flow);
        }
    }

    return ogs_asn_encode(
            &asn_DEF_NGAP_PDUSessionResourceSetupRequestTransfer, &message);
}

ogs_pkbuf_t *ngap_build_pdu_session_resource_modify_request_transfer(
        smf_sess_t *sess, bool include_gbr)
{
    NGAP_PDUSessionResourceModifyRequestTransfer_t message;

    NGAP_PDUSessionResourceModifyRequestTransferIEs_t *ie = NULL;

    NGAP_QosFlowAddOrModifyRequestList_t *QosFlowAddOrModifyRequestList = NULL;
    NGAP_QosFlowAddOrModifyRequestItem_t *QosFlowAddOrModifyRequestItem = NULL;

    smf_bearer_t *qos_flow = NULL;

    ogs_assert(sess);

    ogs_debug("PDUSessionResourceModifyRequestTransfer");
    memset(&message, 0, sizeof(NGAP_PDUSessionResourceModifyRequestTransfer_t));
    message.protocolIEs = ogs_asn_calloc_protocol_ies(
            &asn_DEF_NGAP_PDUSessionResourceModifyRequestTransfer);
    ogs_assert(message.protocolIEs);

    ie = CALLOC(1, sizeof(NGAP_PDUSessionResourceModifyRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_QosFlowAddOrModifyRequestList;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceModifyRequestTransferIEs__value_PR_QosFlowAddOrModifyRequestList;

    QosFlowAddOrModifyRequestList =
        CALLOC(1, sizeof(*QosFlowAddOrModifyRequestList));
    ogs_assert(QosFlowAddOrModifyRequestList);
    ie->value.choice.QosFlowAddOrModifyRequestList =
        QosFlowAddOrModifyRequestList;

    /* Home-Routed V-SMF: QoS flow */
    if (HOME_ROUTED_ROAMING_IN_VSMF(sess)) {
        OpenAPI_lnode_t *node = NULL;
        OpenAPI_list_for_each(
                sess->h_smf_qos_flows_add_mod_request_list, node) {
            OpenAPI_qos_flow_add_modify_request_item_t
                *qosFlowAddModRequestItem = node->data;
            if (qosFlowAddModRequestItem) {
                OpenAPI_qos_flow_profile_t *qosFlowProfile =
                    qosFlowAddModRequestItem->qos_flow_profile;
                if (qosFlowProfile) {
                    ogs_qos_t qos;

                    memset(&qos, 0, sizeof(qos));

                    qos.index = qosFlowProfile->_5qi;
                    ogs_assert(qosFlowProfile->arp);
                    qos.arp.priority_level =
                        qosFlowProfile->arp->priority_level;
                    if (qosFlowProfile->arp->preempt_cap ==
                        OpenAPI_preemption_capability_NOT_PREEMPT)
                        qos.arp.pre_emption_capability =
                            OGS_5GC_PRE_EMPTION_DISABLED;
                    else if (qosFlowProfile->arp->preempt_cap ==
                        OpenAPI_preemption_capability_MAY_PREEMPT)
                        qos.arp.pre_emption_capability =
                            OGS_5GC_PRE_EMPTION_ENABLED;
                    else {
                        ogs_error("Invalid preempt_cap [%d]",
                                qosFlowProfile->arp->preempt_cap);
                        ogs_assert_if_reached();
                    }

                    if (qosFlowProfile->arp->preempt_vuln ==
                        OpenAPI_preemption_vulnerability_NOT_PREEMPTABLE)
                        qos.arp.pre_emption_vulnerability =
                            OGS_5GC_PRE_EMPTION_DISABLED;
                    else if (qosFlowProfile->arp->preempt_vuln ==
                        OpenAPI_preemption_vulnerability_PREEMPTABLE)
                        qos.arp.pre_emption_vulnerability =
                            OGS_5GC_PRE_EMPTION_ENABLED;
                    else {
                        ogs_error("Invalid preempt_vuln [%d]",
                                qosFlowProfile->arp->preempt_vuln);
                        ogs_assert_if_reached();
                    }

                    if (qosFlowProfile->gbr_qos_flow_info) {
                        OpenAPI_gbr_qos_flow_information_t *gbrQosFlowInfo =
                                qosFlowProfile->gbr_qos_flow_info;
                        if (gbrQosFlowInfo->max_fbr_dl)
                            qos.mbr.downlink =
                                ogs_sbi_bitrate_from_string(
                                        gbrQosFlowInfo->max_fbr_dl);
                        if (gbrQosFlowInfo->max_fbr_ul)
                            qos.mbr.uplink =
                                ogs_sbi_bitrate_from_string(
                                        gbrQosFlowInfo->max_fbr_ul);
                        if (gbrQosFlowInfo->gua_fbr_dl)
                            qos.gbr.downlink =
                                ogs_sbi_bitrate_from_string(
                                        gbrQosFlowInfo->gua_fbr_dl);
                        if (gbrQosFlowInfo->gua_fbr_ul)
                            qos.gbr.uplink =
                                ogs_sbi_bitrate_from_string(
                                        gbrQosFlowInfo->gua_fbr_ul);
                    }

                    QosFlowAddOrModifyRequestItem =
                        CALLOC(1, sizeof(*QosFlowAddOrModifyRequestItem));
                    ogs_assert(QosFlowAddOrModifyRequestItem);
                    ASN_SEQUENCE_ADD(
                            &QosFlowAddOrModifyRequestList->list,
                            QosFlowAddOrModifyRequestItem);

                    QosFlowAddOrModifyRequestItem->qosFlowIdentifier =
                        qosFlowAddModRequestItem->qfi;

                    QosFlowAddOrModifyRequestItem->qosFlowLevelQosParameters =
                            CALLOC(1, sizeof(NGAP_QosFlowLevelQosParameters_t));
                    ogs_assert(QosFlowAddOrModifyRequestItem->
                            qosFlowLevelQosParameters);

                    fill_qos_level_parameters(
                            QosFlowAddOrModifyRequestItem->
                                qosFlowLevelQosParameters, &qos, true);
                    qos_flow_alt_and_pdu_set(QosFlowAddOrModifyRequestItem->qosFlowLevelQosParameters,
                            smf_qos_flow_find_by_qfi(sess, qosFlowAddModRequestItem->qfi));
                    l4s_modify_item_ext(QosFlowAddOrModifyRequestItem, smf_qos_flow_find_by_qfi(sess, qosFlowAddModRequestItem->qfi));
                }
            }
        }

    } else {
        /* Default: iterate modify-list */
        ogs_list_for_each_entry(&sess->qos_flow_to_modify_list,
                                 qos_flow, to_modify_node) {
            QosFlowAddOrModifyRequestItem =
                CALLOC(1, sizeof(*QosFlowAddOrModifyRequestItem));
            ogs_assert(QosFlowAddOrModifyRequestItem);
            ASN_SEQUENCE_ADD(
                    &QosFlowAddOrModifyRequestList->list,
                    QosFlowAddOrModifyRequestItem);
            QosFlowAddOrModifyRequestItem->qosFlowIdentifier = qos_flow->qfi;

            QosFlowAddOrModifyRequestItem->qosFlowLevelQosParameters =
                    CALLOC(1, sizeof(NGAP_QosFlowLevelQosParameters_t));
            ogs_assert(
                    QosFlowAddOrModifyRequestItem->qosFlowLevelQosParameters);
            fill_qos_level_parameters(
                    QosFlowAddOrModifyRequestItem->qosFlowLevelQosParameters,
                    &qos_flow->qos, include_gbr);
            qos_flow_alt_and_pdu_set(QosFlowAddOrModifyRequestItem->qosFlowLevelQosParameters, qos_flow);
            l4s_modify_item_ext(QosFlowAddOrModifyRequestItem, qos_flow);
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
    message.protocolIEs = ogs_asn_calloc_protocol_ies(
            &asn_DEF_NGAP_PDUSessionResourceModifyRequestTransfer);
    ogs_assert(message.protocolIEs);

    ie = CALLOC(1, sizeof(NGAP_PDUSessionResourceModifyRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_QosFlowToReleaseList;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_PDUSessionResourceModifyRequestTransferIEs__value_PR_QosFlowListWithCause;

    QosFlowListWithCause = CALLOC(1, sizeof(*QosFlowListWithCause));
    ogs_assert(QosFlowListWithCause);
    ie->value.choice.QosFlowListWithCause = QosFlowListWithCause;

    /* Home-Routed V-SMF: QoS flow */
    if (HOME_ROUTED_ROAMING_IN_VSMF(sess)) {
        OpenAPI_lnode_t *node = NULL;
        OpenAPI_list_for_each(sess->h_smf_qos_flows_rel_request_list, node) {
            OpenAPI_qos_flow_release_request_item_t
                *qosFlowRelRequestItem = node->data;
            if (qosFlowRelRequestItem) {

                QosFlowWithCauseItem = CALLOC(1, sizeof(*QosFlowWithCauseItem));
                ogs_assert(QosFlowWithCauseItem);
                ASN_SEQUENCE_ADD(&QosFlowListWithCause->list,
                        QosFlowWithCauseItem);

                qosFlowIdentifier = &QosFlowWithCauseItem->qosFlowIdentifier;

                *qosFlowIdentifier = qosFlowRelRequestItem->qfi;

                QosFlowWithCauseItem->cause =
                    CALLOC(1, sizeof(*QosFlowWithCauseItem->cause));
                ogs_assert(QosFlowWithCauseItem->cause);
                Cause = QosFlowWithCauseItem->cause;
                Cause->present = group;
                Cause->choice.radioNetwork = cause;
            }
        }
    } else {
        ogs_list_for_each_entry(
                &sess->qos_flow_to_modify_list, qos_flow, to_modify_node) {

            QosFlowWithCauseItem = CALLOC(1, sizeof(*QosFlowWithCauseItem));
            ogs_assert(QosFlowWithCauseItem);
            ASN_SEQUENCE_ADD(&QosFlowListWithCause->list, QosFlowWithCauseItem);

            qosFlowIdentifier = &QosFlowWithCauseItem->qosFlowIdentifier;

            *qosFlowIdentifier = qos_flow->qfi;

            QosFlowWithCauseItem->cause =
                CALLOC(1, sizeof(*QosFlowWithCauseItem->cause));
            ogs_assert(QosFlowWithCauseItem->cause);
            Cause = QosFlowWithCauseItem->cause;
            Cause->present = group;
            Cause->choice.radioNetwork = cause;

        }
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

    message.cause = CALLOC(1, sizeof(*message.cause));
    ogs_assert(message.cause);
    Cause = message.cause;
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
                sess->local_ul_addr, sess->local_ul_addr6, &upf_n3_ip));
    ogs_assert(OGS_OK == ogs_asn_ip_to_BIT_STRING(
                &upf_n3_ip, &gTPTunnel->transportLayerAddress));
    ogs_asn_uint32_to_OCTET_STRING(sess->local_ul_teid, &gTPTunnel->gTP_TEID);

#endif

    return ogs_asn_encode(
            &asn_DEF_NGAP_PathSwitchRequestAcknowledgeTransfer, &message);
}

ogs_pkbuf_t *ngap_build_handover_command_transfer(smf_sess_t *sess)
{
    NGAP_HandoverCommandTransfer_t message;

    ogs_ip_t local_dl_ip;

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
                sess->handover.local_dl_addr, sess->handover.local_dl_addr6,
                &local_dl_ip));
        ogs_assert(OGS_OK == ogs_asn_ip_to_BIT_STRING(
                    &local_dl_ip, &gTPTunnel->transportLayerAddress));
        ogs_asn_uint32_to_OCTET_STRING(
                sess->handover.local_dl_teid, &gTPTunnel->gTP_TEID);

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
    return ngap_build_mbs_session_modification_request_transfer(mbs_sess, false);
}

ogs_pkbuf_t *ngap_build_mbs_session_modification_request_transfer(smf_mbs_sess_t *mbs_sess, bool n3mb_path_failure)
{
    /*
    maxnoofMBSQoSFlows - Maximum no. of QoS Flows allowed within one MBS session. Value is 64.
    maxnoofMBSFSAs - Maximum no. of FSA IDs for one MBS session. Value is 64.
    */

    NGAP_MBSSessionSetupOrModRequestTransfer_t message;

    NGAP_MBSSessionSetupOrModRequestTransferIEs_t *ie = NULL;

    // MBS Session TNL Information 5GC
    NGAP_MBS_SessionTNLInfo5GC_t *mBS_SessionTNLInfo5GC = NULL;

    NGAP_SharedNGU_MulticastTNLInformation_t *sharedNGU_MulticastTNLInformation = NULL;
    NGAP_TransportLayerAddress_t *iP_MulticastAddress = NULL;
	NGAP_TransportLayerAddress_t *iP_SourceAddress = NULL;
    NGAP_GTP_TEID_t *gTP_TEID = NULL;

    // MBS QoS Flows To Be Setup or Modified List
    NGAP_MBS_QoSFlowsToBeSetupList_t *mBS_QoSFlowsToBeSetupList = NULL;

    NGAP_MBS_QoSFlowsToBeSetupItem_t *mBS_QoSFlowsToBeSetupItem = NULL;

    NGAP_QosFlowIdentifier_t *mBS_QoSFlowIdentifier = NULL;
	NGAP_QosFlowLevelQosParameters_t *mBS_QoSFlowLevelQoSParameters = NULL;

    NGAP_QosCharacteristics_t *qoSCharacteristics = NULL;
    NGAP_NonDynamic5QIDescriptor_t *nonDynamic5QIDescriptor = NULL;

    NGAP_AllocationAndRetentionPriority_t *allocationAndRetentionPriority = NULL;

    ogs_assert(mbs_sess);

    ogs_debug("MBSSessionSetupOrModRequestTransfer");
    memset(&message, 0, sizeof(NGAP_MBSSessionSetupOrModRequestTransfer_t));
    message.protocolIEs = ogs_asn_calloc_protocol_ies(
            &asn_DEF_NGAP_MBSSessionSetupOrModRequestTransfer);
    ogs_assert(message.protocolIEs);

    // The IE is optional (TS 38.413 V18.8.0 cl. 9.3.5.3, "MBS Session TNL Information 5GC | O"): it is left out when
    // the MB-UPF returned no low-layer SSM, so there is no multicast address to carry.
    if (mbs_sess->ll_ssm.dest_ip_addr.ipv4 || mbs_sess->ll_ssm.dest_ip_addr.ipv6) {
        // MBS Session TNL Information 5GC - 9.3.2.15 (O)
        ie = CALLOC(1, sizeof(NGAP_MBSSessionSetupOrModRequestTransferIEs_t));
        ogs_assert(ie);
        ASN_SEQUENCE_ADD(message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_MBS_SessionTNLInfo5GC;
        ie->criticality = NGAP_Criticality_reject;
        ie->value.present = NGAP_MBSSessionSetupOrModRequestTransferIEs__value_PR_MBS_SessionTNLInfo5GC;

        mBS_SessionTNLInfo5GC = CALLOC(1, sizeof(*mBS_SessionTNLInfo5GC));
        ogs_assert(mBS_SessionTNLInfo5GC);
        ie->value.choice.MBS_SessionTNLInfo5GC = mBS_SessionTNLInfo5GC;

            /* TS 38.413 V18.10.0 clause 9.3.2.15: "location independent: Shared NG-U Multicast TNL Information";
             * "location dependent: MBS Session TNL Information 5GC List ... MBS Session TNL Information 5GC Item
             * (1..<maxnoofMBSServiceAreaInformation>): MBS Area Session ID, Shared NG-U Multicast TNL Information".
             * An MBS session of this MB-SMF is one area session of a location dependent MBS service (it has its own
             * Area Session ID and LL SSM), so its list has the one item. */
        sharedNGU_MulticastTNLInformation = CALLOC(1, sizeof(NGAP_SharedNGU_MulticastTNLInformation_t));
        ogs_assert(sharedNGU_MulticastTNLInformation);

        if (mbs_sess->location_dependent) {
            NGAP_MBS_SessionTNLInfo5GCList_t *list = CALLOC(1, sizeof(*list));
            NGAP_MBS_SessionTNLInfo5GCItem_t *item = CALLOC(1, sizeof(*item));

            ogs_assert(list);
            ogs_assert(item);
            item->mBS_AreaSessionID = mbs_sess->area_session_id;
            item->sharedNGU_MulticastTNLInformation = sharedNGU_MulticastTNLInformation;
            ASN_SEQUENCE_ADD(&list->list, item);
            mBS_SessionTNLInfo5GC->present = NGAP_MBS_SessionTNLInfo5GC_PR_locationdependent;
            mBS_SessionTNLInfo5GC->choice.locationdependent = list;
        } else {
            mBS_SessionTNLInfo5GC->present = NGAP_MBS_SessionTNLInfo5GC_PR_locationindependent;
            mBS_SessionTNLInfo5GC->choice.locationindependent = sharedNGU_MulticastTNLInformation;
        }

        // Transport Layer Address - 9.3.2.4 (M)
        iP_MulticastAddress = &sharedNGU_MulticastTNLInformation->iP_MulticastAddress;
        ogs_asn_ip_to_BIT_STRING(&mbs_sess->ll_ssm.dest_ip_addr, iP_MulticastAddress);

        // Transport Layer Address - 9.3.2.4 (M)
        iP_SourceAddress = &sharedNGU_MulticastTNLInformation->iP_SourceAddress;
        ogs_asn_ip_to_BIT_STRING(&mbs_sess->ll_ssm.src_ip_addr, iP_SourceAddress);

        // GTP-TEID - 9.3.2.5 (M)
        gTP_TEID = &sharedNGU_MulticastTNLInformation->gTP_TEID;
        ogs_asn_uint32_to_OCTET_STRING(mbs_sess->c_teid, gTP_TEID);
    }

    // MBS QoS Flows To Be Setup List - 9.3.1.236 (M)
    ie = CALLOC(1, sizeof(NGAP_MBSSessionSetupOrModRequestTransferIEs_t));
    ogs_assert(ie);
    ASN_SEQUENCE_ADD(message.protocolIEs, ie);

    ie->id = NGAP_ProtocolIE_ID_id_MBS_QoSFlowsToBeSetupModList;
    ie->criticality = NGAP_Criticality_reject;
    ie->value.present = NGAP_MBSSessionSetupOrModRequestTransferIEs__value_PR_MBS_QoSFlowsToBeSetupList;

    mBS_QoSFlowsToBeSetupList = CALLOC(1, sizeof(*mBS_QoSFlowsToBeSetupList));
    ogs_assert(mBS_QoSFlowsToBeSetupList);
    ie->value.choice.MBS_QoSFlowsToBeSetupList = mBS_QoSFlowsToBeSetupList;

        // The QoS flows come from what the AF requested: TS 29.571 MbsSession.mbsServInfo.mbsMediaComps[*].
        // mbsQoSReq, which smf_nmbsmf_handle_mbs_session_create() parses into mbs_sess->mbs_qos_flow_list.
        // The Allocation and Retention Priority is the reqMbsArp of the mbsQoSReq when the AF gave one (no PCF
        // decides the MBS policy here); otherwise priority 8, no pre-emption either way.
    {
        smf_mbs_qos_flow_t *qos_flow = NULL;
        ogs_list_for_each(&mbs_sess->mbs_qos_flow_list, qos_flow) {
            mBS_QoSFlowsToBeSetupItem = CALLOC(1, sizeof(struct NGAP_MBS_QoSFlowsToBeSetupItem));
            ogs_assert(mBS_QoSFlowsToBeSetupItem);
            ASN_SEQUENCE_ADD(&mBS_QoSFlowsToBeSetupList->list, mBS_QoSFlowsToBeSetupItem);

            // QoS Flow Identifier - 9.3.1.51 (M)
            mBS_QoSFlowIdentifier = &mBS_QoSFlowsToBeSetupItem->mBSqosFlowIdentifier;

            // QoS Flow Level QoS Parameters - 9.3.1.12 (M)
            mBS_QoSFlowLevelQoSParameters = CALLOC(1, sizeof(*mBS_QoSFlowLevelQoSParameters));
            ogs_assert(mBS_QoSFlowLevelQoSParameters);
            mBS_QoSFlowsToBeSetupItem->mBSqosFlowLevelQosParameters = mBS_QoSFlowLevelQoSParameters;

            // Allocation and Retention Priority - 9.3.1.19 (M)
            allocationAndRetentionPriority = CALLOC(1, sizeof(*allocationAndRetentionPriority));
            ogs_assert(allocationAndRetentionPriority);
            mBS_QoSFlowLevelQoSParameters->allocationAndRetentionPriority = allocationAndRetentionPriority;
            qoSCharacteristics = CALLOC(1, sizeof(*qoSCharacteristics));
            ogs_assert(qoSCharacteristics);
            mBS_QoSFlowLevelQoSParameters->qosCharacteristics = qoSCharacteristics;

            // Non Dynamic 5QI Descriptor - 9.3.1.28 (M)
            nonDynamic5QIDescriptor = CALLOC(1, sizeof(struct NGAP_NonDynamic5QIDescriptor));
            ogs_assert(nonDynamic5QIDescriptor);
            qoSCharacteristics->choice.nonDynamic5QI = nonDynamic5QIDescriptor;
            qoSCharacteristics->present = NGAP_QosCharacteristics_PR_nonDynamic5QI;

            *mBS_QoSFlowIdentifier = qos_flow->qfi;

            // 5QI - INTEGER (M)
            nonDynamic5QIDescriptor->fiveQI = qos_flow->five_qi;

            // Priority Level - INTEGER (M)
            allocationAndRetentionPriority->priorityLevelARP = qos_flow->arp_present ? qos_flow->arp_priority_level : 8;

            // Pre-emption Capability - ENUMERATED (M)
            allocationAndRetentionPriority->pre_emptionCapability =
                qos_flow->arp_present && qos_flow->arp_may_preempt ?
                    NGAP_Pre_emptionCapability_may_trigger_pre_emption :
                    NGAP_Pre_emptionCapability_shall_not_trigger_pre_emption;

            // Pre-emption Vulnerability - ENUMERATED (M)
            allocationAndRetentionPriority->pre_emptionVulnerability =
                qos_flow->arp_present && qos_flow->arp_preemptable ?
                    NGAP_Pre_emptionVulnerability_pre_emptable :
                    NGAP_Pre_emptionVulnerability_not_pre_emptable;
        }
    }

        // MBS Session FSA ID List (MBS Frequency Selection Area Identity, 9.3.2.16) is Optional per
        // 38413-h30.asn's "{ ID id-MBS-SessionFSAIDList ... PRESENCE optional }". It is sent when the AF supplies
        // one: TS 29.532 MbsSession.mbsFsaIdList is a real field, which rt-mbs-function builds through
        // MBSMFMBSSession::setFsaId() and smf_nmbsmf_handle_mbs_session_create() parses into
        // mbs_sess->mbs_fsa_id_list. It is omitted, not fabricated, when the AF supplies none, which remains the
        // common case while the USD source for MBS Interest Indication is absent.
    if (ogs_list_count(&mbs_sess->mbs_fsa_id_list) > 0) {
        NGAP_MBS_SessionFSAIDList_t *mBS_SessionFSAIDList = NULL;
        NGAP_MBS_SessionFSAID_t *mBS_SessionFSAID = NULL;
        smf_mbs_fsa_id_t *fsa_id = NULL;

        ie = CALLOC(1, sizeof(NGAP_MBSSessionSetupOrModRequestTransferIEs_t));
        ogs_assert(ie);
        ASN_SEQUENCE_ADD(message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_MBS_SessionFSAIDList;
        ie->criticality = NGAP_Criticality_ignore;
        ie->value.present = NGAP_MBSSessionSetupOrModRequestTransferIEs__value_PR_MBS_SessionFSAIDList;

        mBS_SessionFSAIDList = CALLOC(1, sizeof(*mBS_SessionFSAIDList));
        ogs_assert(mBS_SessionFSAIDList);
        ie->value.choice.MBS_SessionFSAIDList = mBS_SessionFSAIDList;

        // 38413-h30.asn: "MBS-SessionFSAIDList ::= SEQUENCE (SIZE(1.. maxnoofMBSFSAs)) OF
        // MBS-SessionFSAID", and "maxnoofMBSFSAs INTEGER ::= 64" -- a real, quoted bound (rule 12),
        // not invented here. The AF-supplied list is otherwise unbounded (nothing at parse time in
        // smf_nmbsmf_handle_mbs_session_create() enforces this either); logging and clamping rather
        // than silently truncating or asserting mid-encode.
        {
            int count = 0;
            const int kMaxNgapFsaIds = 64;
            int total = ogs_list_count(&mbs_sess->mbs_fsa_id_list);
            if (total > kMaxNgapFsaIds) {
                ogs_warn("MBS Session Create: AF supplied %d MBS FSA IDs, NGAP "
                    "MBS-SessionFSAIDList allows at most %d (38413-h30.asn maxnoofMBSFSAs) -- "
                    "sending only the first %d", total, kMaxNgapFsaIds, kMaxNgapFsaIds);
            }
            ogs_list_for_each(&mbs_sess->mbs_fsa_id_list, fsa_id) {
                if (count >= kMaxNgapFsaIds) break;
                mBS_SessionFSAID = CALLOC(1, sizeof(NGAP_MBS_SessionFSAID_t));
                ogs_assert(mBS_SessionFSAID);
                ogs_asn_uint24_to_OCTET_STRING(fsa_id->id, (OCTET_STRING_t *)mBS_SessionFSAID);
                ASN_SEQUENCE_ADD(&mBS_SessionFSAIDList->list, mBS_SessionFSAID);
                count++;
            }
        }
    }

    /* Supported UE Type List (TS 38.413 V18.8.0 clause 9.3.5.3, optional, ignore): the "NR RedCap UE information" of
     * a broadcast session, forwarded as the UE types the session is intended for. TS 23.247 V18.8.0 clause 6.19: "The
     * "NR RedCap UE information" parameter comprises an indication for a broadcast MBS session that can take the
     * following values: "MBS session expected to be received only by (e)RedCap UEs", "MBS session expected to be
     * received by any kind of UEs", or "MBS session expected to be received only by UEs that are neither RedCap UEs
     * nor eRedCap UEs"." and clause 7.3.1: "the MB-SMF includes the "NR RedCap UE Information" parameter in N2 SM
     * information." The mapping to the two UE types is the reading of those three values; no clause states it. */
    if (mbs_sess->service_type && strcmp(mbs_sess->service_type, "BROADCAST") == 0 &&
            mbs_sess->nr_red_cap_ue_info != SMF_MBS_NR_RED_CAP_NONE) {
        NGAP_SupportedUETypeList_t *supportedUETypeList = NULL;
        NGAP_SupportedUEType_t *supportedUEType = NULL;

        ie = CALLOC(1, sizeof(NGAP_MBSSessionSetupOrModRequestTransferIEs_t));
        ogs_assert(ie);
        ASN_SEQUENCE_ADD(message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_SupportedUETypeList;
        ie->criticality = NGAP_Criticality_ignore;
        ie->value.present = NGAP_MBSSessionSetupOrModRequestTransferIEs__value_PR_SupportedUETypeList;

        supportedUETypeList = CALLOC(1, sizeof(*supportedUETypeList));
        ogs_assert(supportedUETypeList);
        ie->value.choice.SupportedUETypeList = supportedUETypeList;

        if (mbs_sess->nr_red_cap_ue_info != SMF_MBS_NR_RED_CAP_UE_ONLY) {
            supportedUEType = CALLOC(1, sizeof(NGAP_SupportedUEType_t));
            ogs_assert(supportedUEType);
            *supportedUEType = NGAP_SupportedUEType_non_RedCap_eRedCap_UE;
            ASN_SEQUENCE_ADD(&supportedUETypeList->list, supportedUEType);
        }
        if (mbs_sess->nr_red_cap_ue_info != SMF_MBS_NR_RED_CAP_NON_REDCAP_UE_ONLY) {
            supportedUEType = CALLOC(1, sizeof(NGAP_SupportedUEType_t));
            ogs_assert(supportedUEType);
            *supportedUEType = NGAP_SupportedUEType_redCap_eRedCap_UE;
            ASN_SEQUENCE_ADD(&supportedUETypeList->list, supportedUEType);
        }
    }

    /* MBS NG-U Failure Indication (TS 38.413 V18.8.0 clause 9.3.5.3, "MBS NG-U Failure Indication | O |  | 9.3.2.18",
     * criticality reject): "This IE provides information that the 5GC has detected an NG-U failure." For the
     * Broadcast MBS session restoration upon N3mb path failure (TS 23.527 V18.6.0 clause 8.5 step 3, "a Broadcast
     * Session Modification Request Transfer including an N3mb path failure indication"). Clause 9.3.2.18 has the
     * location independent arm, and for a location dependent session an item of the MBS Area Session ID. */
    if (n3mb_path_failure) {
        NGAP_MBS_NGUFailureIndication_t *failure = NULL;

        ie = CALLOC(1, sizeof(NGAP_MBSSessionSetupOrModRequestTransferIEs_t));
        ogs_assert(ie);
        ASN_SEQUENCE_ADD(message.protocolIEs, ie);

        ie->id = NGAP_ProtocolIE_ID_id_MBS_NGUFailureIndication;
        ie->criticality = NGAP_Criticality_reject;
        ie->value.present = NGAP_MBSSessionSetupOrModRequestTransferIEs__value_PR_MBS_NGUFailureIndication;
        failure = CALLOC(1, sizeof(*failure));
        ogs_assert(failure);
        ie->value.choice.MBS_NGUFailureIndication = failure;

        if (mbs_sess->location_dependent) {
            NGAP_MBS_UP_FailureIndicationList_t *list = NULL;
            NGAP_MBS_UP_FailureIndicationItem_t *item = NULL;

            list = CALLOC(1, sizeof(*list));
            ogs_assert(list);
            item = CALLOC(1, sizeof(*item));
            ogs_assert(item);
            item->mBS_AreaSessionID = mbs_sess->area_session_id;
            item->mBS_UP_FailureIndication = NGAP_MBS_UP_FailureIndication_ng_u_path_failure_detected;
            ASN_SEQUENCE_ADD(&list->list, item);
            failure->present = NGAP_MBS_NGUFailureIndication_PR_locationdependent;
            failure->choice.locationdependent = list;
        } else {
            failure->present = NGAP_MBS_NGUFailureIndication_PR_locationindependent;
            failure->choice.locationindependent = NGAP_MBS_UP_FailureIndication_ng_u_path_failure_detected;
        }
    }

    return ogs_asn_encode(
            &asn_DEF_NGAP_MBSSessionSetupOrModRequestTransfer, &message);
}

/*
 * Broadcast Transport Response Transfer, TS 38.413 V18.8.0 clause 9.3.5.16: "MBS Session ID | M |  | 9.3.1.206"; the
 * MB-SMF sends it in a ContextStatusNotificationResponse (TS 29.518 V18.14.0 clause 5.6.2.5 step 2a). The NID of the
 * MBS Session ID is not set: this MB-SMF holds none.
 */
ogs_pkbuf_t *ngap_build_broadcast_transport_response_transfer(const ogs_tmgi_t *tmgi)
{
    NGAP_BroadcastTransportResponseTransfer_t message;

    ogs_assert(tmgi);

    memset(&message, 0, sizeof(message));
    message.mBS_SessionID = CALLOC(1, sizeof(*message.mBS_SessionID));
    ogs_assert(message.mBS_SessionID);
    ogs_ngap_5gs_tmgi_to_ASN((ogs_tmgi_t *)tmgi, &message.mBS_SessionID->tMGI);

    return ogs_asn_encode(&asn_DEF_NGAP_BroadcastTransportResponseTransfer, &message);
}

/*
 * Broadcast Transport Failure Transfer, TS 38.413 V18.8.0 clause 9.3.5.17: "MBS Session ID | M |  | 9.3.1.206",
 * "Cause | M |  | 9.3.1.2" and "Criticality Diagnostics | O |  | 9.3.1.3" (not sent).
 */
ogs_pkbuf_t *ngap_build_broadcast_transport_failure_transfer(const ogs_tmgi_t *tmgi, NGAP_Cause_PR cause_group,
        long cause_value)
{
    NGAP_BroadcastTransportFailureTransfer_t message;

    ogs_assert(tmgi);

    memset(&message, 0, sizeof(message));
    message.mBS_SessionID = CALLOC(1, sizeof(*message.mBS_SessionID));
    ogs_assert(message.mBS_SessionID);
    ogs_ngap_5gs_tmgi_to_ASN((ogs_tmgi_t *)tmgi, &message.mBS_SessionID->tMGI);
    message.cause = CALLOC(1, sizeof(*message.cause));
    ogs_assert(message.cause);
    message.cause->present = cause_group;
    switch (cause_group) {
    case NGAP_Cause_PR_radioNetwork: message.cause->choice.radioNetwork = cause_value; break;
    case NGAP_Cause_PR_transport: message.cause->choice.transport = cause_value; break;
    case NGAP_Cause_PR_nas: message.cause->choice.nas = cause_value; break;
    case NGAP_Cause_PR_protocol: message.cause->choice.protocol = cause_value; break;
    default:
        message.cause->present = NGAP_Cause_PR_misc;
        message.cause->choice.misc = cause_value;
        break;
    }

    return ogs_asn_encode(&asn_DEF_NGAP_BroadcastTransportFailureTransfer, &message);
}
