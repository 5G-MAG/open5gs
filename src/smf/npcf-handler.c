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
#include "pfcp-path.h"
#include "nas-path.h"
#include "local-path.h"
#include "binding.h"

#include "npcf-handler.h"

/* PduSetQosParaRm of a QosData (TS 29.571) into the PDU Set QoS parameters of the rule */
static void read_pdu_set_qos(ogs_pdu_set_qos_t *out, OpenAPI_pdu_set_qos_para_rm_t *in)
{
    memset(out, 0, sizeof(*out));
    if (!in)
        return;
    out->presence = true;
    if (in->is_pdu_set_delay_budget) {
        out->delay_budget_presence = true;
        out->delay_budget = in->pdu_set_delay_budget;
    }
    if (in->pdu_set_err_rate)
        ogs_cpystrn(out->error_rate, in->pdu_set_err_rate, sizeof(out->error_rate));
    if (in->pdu_set_handling_info == OpenAPI_pdu_set_handling_info_ALL_PDUS_NEEDED)
        out->handling_info = OGS_PDU_SET_ALL_PDUS_NEEDED;
    else if (in->pdu_set_handling_info == OpenAPI_pdu_set_handling_info_ALL_PDUS_NOT_NEEDED)
        out->handling_info = OGS_PDU_SET_ALL_PDUS_NOT_NEEDED;
}

static void update_authorized_pcc_rule_and_qos(
        smf_sess_t *sess, OpenAPI_sm_policy_decision_t *SmPolicyDecision)
{
    OpenAPI_lnode_t *node = NULL, *node2 = NULL;

    ogs_assert(sess);
    ogs_assert(SmPolicyDecision);

    /*
     * TS29.512
     * 4.2.6 Provisioning and Enforcement of Policy Decisions
     *
     *
     * If no other rules are defined for specific data types
     * within the SmPolicyDecision data structure, the encoding of changes
     * of the policies decisions in the SmPolicyDecision data structure
     * shall follow the following principles:
     *
     * 1) To modify an attribute with a value of type map
     * (e.g. the "sessRules" attribute, the "pccRules" attribute,
     * the "qosDecs" attribute, the "traffContDecs" attribute,
     * the "umDecs" attribute, and the "conds" attribute) the attribute
     * shall be provided with a value containing a map with entries
     * according to the following principles:
     *
     * - A new entry shall be added by supplying a new identifier
     *   (e.g. rule / decision identifier) as key and the corresponding
     *   structured data type instance (e.g. PCC rule) with complete contents
     *   as value as an entry within the map.
     * - An existing entry shall be modified by supplying the existing
     *   identifier as key and the corresponding structured data type instance
     *   with the same existing identifier (e.g. set the "qosId"
     *   to the same existing QoS data decision identifier),
     *   which shall describe the modifications following bullets 1 to 6,
     *   as value as an entry within the map.
     * - An existing entry shall be deleted by supplying the existing
     *   identifier as key and "NULL" as value as an entry within the map.
     * - For an unmodified entry, no entry needs to be provided within the map.
     *
     * 2) To modify an attribute with a structured data type instance as value,
     * the attribute shall be provided with a value containing a structured data
     * type instance with entries according to bullets 1 to 6.
     *
     * 3) To modify an attribute with another type than map or structured data
     * type as value, the attribute shall be provided with a complete
     * representation of its value that shall replace the previous value.
     *
     * 4) To create an attribute of any type, the attribute shall be provided
     * with a complete representation of its value.
     *
     * 5) To delete an attribute of any type, the attribute shall be provided
     * with NULL as value.
     * NOTE 1: Attributes that are allowed to be deleted need to be marked as
     * "nullable" within the OpenAPI file in Annex A.
     *
     * 6) Attributes that are not added, modified, or deleted do not need to be
     * provided.
     * NOTE 2: In related data structures no attrib
     */

    /*
     * TS 29.512 V19.7.0 clause 4.2.4.14: with lastReqRuleData that carries SUCC_RES_ALLO in reqData, the SMF reports when the
     * resources of the referenced PCC rules are allocated.
     */
    OpenAPI_list_for_each(SmPolicyDecision->last_req_rule_data, node) {
        OpenAPI_requested_rule_data_t *RequestedRuleData = node->data;
        OpenAPI_lnode_t *req = NULL, *ref = NULL;
        bool succ_res_allo = false;

        if (!RequestedRuleData)
            continue;
        OpenAPI_list_for_each(RequestedRuleData->req_data, req) {
            if ((OpenAPI_requested_rule_data_type_e)(intptr_t)req->data ==
                    OpenAPI_requested_rule_data_type_SUCC_RES_ALLO)
                succ_res_allo = true;
        }
        if (!succ_res_allo)
            continue;
        OpenAPI_list_for_each(RequestedRuleData->ref_pcc_rule_ids, ref) {
            int k;

            if (!ref->data ||
                sess->policy.num_of_succ_res_allo_rule >=
                    OGS_MAX_NUM_OF_PCC_RULE)
                continue;
            for (k = 0; k < sess->policy.num_of_succ_res_allo_rule; k++) {
                if (!strcmp(sess->policy.succ_res_allo_rule_id[k], ref->data))
                    break;
            }
            if (k == sess->policy.num_of_succ_res_allo_rule)
                sess->policy.succ_res_allo_rule_id[
                    sess->policy.num_of_succ_res_allo_rule++] =
                        ogs_strdup(ref->data);
        }
    }

    /* L4S_SUPP armed by the PCF: the availability of ECN marking for L4S is reported to it (clause 4.2.6.2.21) */
    OpenAPI_list_for_each(SmPolicyDecision->policy_ctrl_req_triggers, node) {
        if ((OpenAPI_policy_control_request_trigger_e)(intptr_t)node->data ==
                OpenAPI_policy_control_request_trigger_L4S_SUPP)
            sess->policy.l4s_trigger = true;
    }

    /* QOS_MONITORING armed by the PCF: the reports of the UPF go to it (clause 4.2.4.24) */
    OpenAPI_list_for_each(SmPolicyDecision->policy_ctrl_req_triggers, node) {
        if ((OpenAPI_policy_control_request_trigger_e)(intptr_t)node->data ==
                OpenAPI_policy_control_request_trigger_QOS_MONITORING)
            sess->policy.qos_mon_trigger = true;
    }

    /*
     * TS 29.512 V19.7.0 clause 4.2.4.10: umDecs gives the usage monitoring control instances (a null value ends the monitoring) and
     * lastReqUsageData asks for the accumulated usage of instances.
     */
    OpenAPI_list_for_each(SmPolicyDecision->um_decs, node) {
        OpenAPI_map_t *UmMap = node->data;
        OpenAPI_usage_monitoring_data_t *Um = UmMap ? UmMap->value : NULL;
        int idx;

        if (!UmMap || !UmMap->key)
            continue;
        idx = smf_um_find(sess, UmMap->key);
        if (!Um) {
            /* the monitoring ends: the usage so far is reported (clause 4.2.6.5.3.2), the instance is not armed any more */
            if (idx >= 0) {
                sess->policy.um[idx].query_pending = true;
                sess->policy.um[idx].armed = false;
                smf_um_urr_update(sess, idx);
                sess->policy.um[idx].update_pending = true;
            }
            continue;
        }
        if (idx < 0)
            idx = smf_um_add(sess, UmMap->key);
        if (idx < 0)
            continue;
        sess->policy.um[idx].volume_threshold = Um->is_volume_threshold ? (uint64_t)Um->volume_threshold : 0;
        sess->policy.um[idx].volume_threshold_uplink = Um->is_volume_threshold_uplink ? (uint64_t)Um->volume_threshold_uplink : 0;
        sess->policy.um[idx].volume_threshold_downlink = Um->is_volume_threshold_downlink ? (uint64_t)Um->volume_threshold_downlink : 0;
        sess->policy.um[idx].time_threshold = Um->is_time_threshold ? (uint32_t)Um->time_threshold : 0;
        sess->policy.um[idx].armed =
            sess->policy.um[idx].volume_threshold || sess->policy.um[idx].volume_threshold_uplink ||
            sess->policy.um[idx].volume_threshold_downlink || sess->policy.um[idx].time_threshold;
        smf_um_urr_update(sess, idx);
        sess->policy.um[idx].update_pending = true;
    }
    if (SmPolicyDecision->last_req_usage_data) {
        OpenAPI_requested_usage_data_t *Requested = SmPolicyDecision->last_req_usage_data;
        int i;

        for (i = 0; i < sess->policy.num_of_um; i++) {
            bool wanted = Requested->is_all_um_ids && Requested->all_um_ids;

            OpenAPI_list_for_each(Requested->ref_um_ids, node)
                if (node->data && !strcmp(node->data, sess->policy.um[i].id))
                    wanted = true;
            if (wanted && sess->policy.um[i].created)
                sess->policy.um[i].query_pending = true;
        }
    }

    if (SmPolicyDecision->pcc_rules) {
        OpenAPI_map_t *PccRuleMap = NULL;
        OpenAPI_pcc_rule_t *PccRule = NULL;
        OpenAPI_flow_information_t *FlowInformation = NULL;
        OpenAPI_qos_data_t *QosData = NULL;
        char *QosId = NULL;
        int i;

        for (i = 0; i < sess->policy.num_of_pcc_rule; i++)
            OGS_PCC_RULE_FREE(&sess->policy.pcc_rule[i]);
        sess->policy.num_of_pcc_rule = 0;

        OpenAPI_list_for_each(SmPolicyDecision->pcc_rules, node) {
            ogs_pcc_rule_t *pcc_rule = NULL;

            if (sess->policy.num_of_pcc_rule >= OGS_MAX_NUM_OF_PCC_RULE) {
                ogs_error("Too many PccRules [%d:%d]",
                        sess->policy.num_of_pcc_rule + 1,
                        OGS_MAX_NUM_OF_PCC_RULE);
                break;
            }

            QosData = NULL;
            pcc_rule =
                &sess->policy.pcc_rule[sess->policy.num_of_pcc_rule];
            ogs_assert(pcc_rule);

            PccRuleMap = node->data;
            if (!PccRuleMap) {
                ogs_error("No PccRuleMap");
                continue;
            }

            if (!PccRuleMap->key) {
                ogs_error("No PccRule->id");
                continue;
            }

            PccRule = PccRuleMap->value;
            if (!PccRule) {
                pcc_rule->type = OGS_PCC_RULE_TYPE_REMOVE;
                pcc_rule->id = ogs_strdup(PccRuleMap->key);
                ogs_assert(pcc_rule->id);

                sess->policy.num_of_pcc_rule++;
                continue;
            }

            if (!PccRule->ref_qos_data) {
                ogs_error("No RefQosData");
                continue;
            }

            if (!PccRule->ref_qos_data->first) {
                ogs_error("No RefQosData->first");
                continue;
            }

            QosId = PccRule->ref_qos_data->first->data;
            if (!QosId) {
                ogs_error("no QosId");
                continue;
            }

            if (SmPolicyDecision->qos_decs) {
                OpenAPI_map_t *QosDecisionMap = NULL;
                OpenAPI_qos_data_t *QosDataIter = NULL;

                OpenAPI_list_for_each(SmPolicyDecision->qos_decs, node2) {
                    QosDecisionMap = node2->data;
                    if (!QosDecisionMap) {
                        ogs_error("No QosDecisionMap");
                        continue;
                    }

                    QosDataIter = QosDecisionMap->value;
                    if (!QosDataIter) {
                        ogs_error("No QosData");
                        continue;
                    }

                    if (!QosDataIter->qos_id) {
                        ogs_error("No QosId");
                        continue;

                    }

                    if (strcmp(QosId, QosDataIter->qos_id) == 0) {
                        QosData = QosDataIter;
                        break;
                    }
                }
            }

            if (!QosData) {
                ogs_error("no qosData");
                continue;
            }

            if (!PccRule->pcc_rule_id) {
                ogs_error("No PccRule->pcc_rule_id");
                continue;
            }

            pcc_rule->type = OGS_PCC_RULE_TYPE_INSTALL;
            pcc_rule->id = ogs_strdup(PccRule->pcc_rule_id);
            ogs_assert(pcc_rule->id);
            pcc_rule->precedence = PccRule->precedence;

            /*
             * TS 29.512 clause 5.6.2.10: the flowStatus of the TrafficControlData that the rule refers to is the gate of its
             * flows; without it the flows are enabled.
             */
            pcc_rule->flow_status = OGS_DIAM_RX_FLOW_STATUS_ENABLED;

            /* refQosMon: the QoS monitoring of the packet delay of the rule (TS 29.512 V19.7.0 clause 4.2.3.25) */
            if (PccRule->ref_qos_mon && PccRule->ref_qos_mon->first &&
                PccRule->ref_qos_mon->first->data && SmPolicyDecision->qos_mon_decs) {
                OpenAPI_lnode_t *qm_node = NULL;

                OpenAPI_list_for_each(SmPolicyDecision->qos_mon_decs, qm_node) {
                    OpenAPI_map_t *QmMap = qm_node->data;
                    OpenAPI_qos_monitoring_data_t *Qm = QmMap ? QmMap->value : NULL;
                    OpenAPI_lnode_t *p = NULL;

                    if (!Qm || !QmMap->key || strcmp(QmMap->key, PccRule->ref_qos_mon->first->data))
                        continue;
                    pcc_rule->qos_mon.presence = true;
                    OpenAPI_list_for_each(Qm->req_qos_mon_params, p) {
                        switch ((OpenAPI_requested_qos_monitoring_parameter_e)(intptr_t)p->data) {
                        case OpenAPI_requested_qos_monitoring_parameter_DOWNLINK: pcc_rule->qos_mon.dl_delay = true; break;
                        case OpenAPI_requested_qos_monitoring_parameter_UPLINK: pcc_rule->qos_mon.ul_delay = true; break;
                        case OpenAPI_requested_qos_monitoring_parameter_ROUND_TRIP: pcc_rule->qos_mon.rt_delay = true; break;
                        default: break;
                        }
                    }
                    OpenAPI_list_for_each(Qm->rep_freqs, p) {
                        if ((OpenAPI_reporting_frequency_e)(intptr_t)p->data == OpenAPI_reporting_frequency_EVENT_TRIGGERED)
                            pcc_rule->qos_mon.event_triggered = true;
                        else if ((OpenAPI_reporting_frequency_e)(intptr_t)p->data == OpenAPI_reporting_frequency_PERIODIC)
                            pcc_rule->qos_mon.periodic = true;
                    }
                    if (Qm->is_rep_thresh_dl) { pcc_rule->qos_mon.thr_dl_presence = true; pcc_rule->qos_mon.thr_dl = Qm->rep_thresh_dl; }
                    if (Qm->is_rep_thresh_ul) { pcc_rule->qos_mon.thr_ul_presence = true; pcc_rule->qos_mon.thr_ul = Qm->rep_thresh_ul; }
                    if (Qm->is_rep_thresh_rp) { pcc_rule->qos_mon.thr_rt_presence = true; pcc_rule->qos_mon.thr_rt = Qm->rep_thresh_rp; }
                    if (Qm->is_wait_time) { pcc_rule->qos_mon.wait_time_presence = true; pcc_rule->qos_mon.wait_time = Qm->wait_time; }
                    if (Qm->is_rep_period) { pcc_rule->qos_mon.rep_period_presence = true; pcc_rule->qos_mon.rep_period = Qm->rep_period; }
                }
            }
            /* refUmData: the usage monitoring control instance of the rule (clause 5.6.2.2) */
            if (PccRule->ref_um_data && PccRule->ref_um_data->first &&
                PccRule->ref_um_data->first->data)
                pcc_rule->um_id = ogs_strdup(PccRule->ref_um_data->first->data);
            if (PccRule->ref_tc_data && PccRule->ref_tc_data->first &&
                PccRule->ref_tc_data->first->data &&
                SmPolicyDecision->traff_cont_decs) {
                const char *TcId = PccRule->ref_tc_data->first->data;
                OpenAPI_lnode_t *tc_node = NULL;

                OpenAPI_list_for_each(SmPolicyDecision->traff_cont_decs, tc_node) {
                    OpenAPI_map_t *TcMap = tc_node->data;
                    OpenAPI_traffic_control_data_t *TcData =
                        TcMap ? TcMap->value : NULL;

                    if (TcData && TcData->tc_id &&
                        strcmp(TcId, TcData->tc_id) == 0) {
                        if (TcData->flow_status != OpenAPI_flow_status_NULL)
                            pcc_rule->flow_status =
                                ogs_sbi_flow_status_to_pcc_rule(
                                        TcData->flow_status);
                        /* l4sInd: ECN marking for L4S is supported for the UL, the DL or both (clause 4.2.6.2.21) */
                        switch (TcData->l4s_ind) {
                        case OpenAPI_uplink_downlink_support_UL: pcc_rule->l4s_ind = OGS_PCC_RULE_L4S_UL; break;
                        case OpenAPI_uplink_downlink_support_DL: pcc_rule->l4s_ind = OGS_PCC_RULE_L4S_DL; break;
                        case OpenAPI_uplink_downlink_support_UL_DL: pcc_rule->l4s_ind = OGS_PCC_RULE_L4S_UL_DL; break;
                        default: break;
                        }
                        break;
                    }
                }
            }

            if (PccRule->flow_infos) {
                ogs_assert(pcc_rule->num_of_flow == 0);
                OpenAPI_list_for_each(PccRule->flow_infos, node2) {
                    ogs_flow_t *flow = NULL;

                    if (pcc_rule->num_of_flow >=
                            OGS_MAX_NUM_OF_FLOW_IN_PCC_RULE) {
                        ogs_error("Too many FlowInfos [%d:%d]",
                                pcc_rule->num_of_flow + 1,
                                OGS_MAX_NUM_OF_FLOW_IN_PCC_RULE);
                        break;
                    }

                    flow = &pcc_rule->flow[pcc_rule->num_of_flow];
                    ogs_assert(flow);

                    FlowInformation = node2->data;
                    if (!FlowInformation) {
                        ogs_error("No FlowInformation");
                        continue;
                    }

                    if (FlowInformation->flow_direction ==
                        OpenAPI_flow_direction_UPLINK)
                        flow->direction = OGS_FLOW_UPLINK_ONLY;
                    else if (FlowInformation->flow_direction ==
                        OpenAPI_flow_direction_DOWNLINK)
                        flow->direction = OGS_FLOW_DOWNLINK_ONLY;
                    else if (FlowInformation->flow_direction ==
                        OpenAPI_flow_direction_BIDIRECTIONAL)
                        flow->direction = OGS_FLOW_BIDIRECTIONAL;
                    else {
                        ogs_error("Unsupported direction [%d]",
                                FlowInformation->flow_direction);
                        continue;
                    }

                    if (!FlowInformation->flow_description) {
                        ogs_error("No FlowDescription");
                        continue;
                    }

                    flow->description =
                        ogs_strdup(FlowInformation->flow_description);
                    ogs_assert(flow->description);

                    pcc_rule->num_of_flow++;
                }
            }

            pcc_rule->qos.index = QosData->_5qi;
            pcc_rule->qos.arp.priority_level = QosData->priority_level;

            if (QosData->arp) {
                pcc_rule->qos.arp.priority_level = QosData->arp->priority_level;
                if (QosData->arp->preempt_cap ==
                    OpenAPI_preemption_capability_NOT_PREEMPT)
                    pcc_rule->qos.arp.pre_emption_capability =
                        OGS_5GC_PRE_EMPTION_DISABLED;
                else if (QosData->arp->preempt_cap ==
                    OpenAPI_preemption_capability_MAY_PREEMPT)
                    pcc_rule->qos.arp.pre_emption_capability =
                        OGS_5GC_PRE_EMPTION_ENABLED;
                else {
                    ogs_error("Unknown preemptCap [%d], "
                            "fall back to NOT_PREEMPT",
                            (int)QosData->arp->preempt_cap);
                    pcc_rule->qos.arp.pre_emption_capability =
                        OGS_5GC_PRE_EMPTION_DISABLED;
                }

                if (QosData->arp->preempt_vuln ==
                    OpenAPI_preemption_vulnerability_NOT_PREEMPTABLE)
                    pcc_rule->qos.arp.pre_emption_vulnerability =
                        OGS_5GC_PRE_EMPTION_DISABLED;
                else if (QosData->arp->preempt_vuln ==
                    OpenAPI_preemption_vulnerability_PREEMPTABLE)
                    pcc_rule->qos.arp.pre_emption_vulnerability =
                        OGS_5GC_PRE_EMPTION_ENABLED;
                else {
                    ogs_error("Unknown preemptVuln [%d], "
                            "fall back to NOT_PREEMPTABLE",
                            (int)QosData->arp->preempt_vuln);
                    pcc_rule->qos.arp.pre_emption_vulnerability =
                        OGS_5GC_PRE_EMPTION_DISABLED;
                }
            }

            if (QosData->maxbr_ul)
                pcc_rule->qos.mbr.uplink =
                    ogs_sbi_bitrate_from_string(QosData->maxbr_ul);
            if (QosData->maxbr_dl)
                pcc_rule->qos.mbr.downlink =
                    ogs_sbi_bitrate_from_string(QosData->maxbr_dl);

            pcc_rule->qos.qnc = QosData->is_qnc && QosData->qnc;
            if (QosData->gbr_ul)
                pcc_rule->qos.gbr.uplink =
                    ogs_sbi_bitrate_from_string(QosData->gbr_ul);
            if (QosData->gbr_dl)
                pcc_rule->qos.gbr.downlink =
                    ogs_sbi_bitrate_from_string(QosData->gbr_dl);

            if (pcc_rule->qos.mbr.downlink || pcc_rule->qos.mbr.uplink ||
                pcc_rule->qos.gbr.downlink || pcc_rule->qos.gbr.uplink) {
                if (pcc_rule->qos.mbr.downlink == 0 ||
                    pcc_rule->qos.mbr.downlink > OGS_MAX_BITRATE_NGAP)
                    pcc_rule->qos.mbr.downlink = OGS_MAX_BITRATE_NGAP;
                if (pcc_rule->qos.mbr.uplink == 0 ||
                    pcc_rule->qos.mbr.uplink > OGS_MAX_BITRATE_NGAP)
                    pcc_rule->qos.mbr.uplink = OGS_MAX_BITRATE_NGAP;
                if (pcc_rule->qos.gbr.downlink == 0 ||
                    pcc_rule->qos.gbr.downlink > OGS_MAX_BITRATE_NGAP)
                    pcc_rule->qos.gbr.downlink = OGS_MAX_BITRATE_NGAP;
                if (pcc_rule->qos.gbr.uplink == 0 ||
                    pcc_rule->qos.gbr.uplink > OGS_MAX_BITRATE_NGAP)
                    pcc_rule->qos.gbr.uplink = OGS_MAX_BITRATE_NGAP;
            }

            /* the PDU Set QoS parameters of the QosData (TS 29.512 V19.7.0 clause 4.2.6.6.2) */
            read_pdu_set_qos(&pcc_rule->pdu_set_dl, QosData->pdu_set_qos_dl);
            read_pdu_set_qos(&pcc_rule->pdu_set_ul, QosData->pdu_set_qos_ul);

            /* the Alternative QoS parameter sets that refAltQosParams refers to, in the order of the rule (clause 4.2.6.2.1) */
            if (PccRule->ref_alt_qos_params) {
                OpenAPI_lnode_t *node_alt = NULL;

                OpenAPI_list_for_each(PccRule->ref_alt_qos_params, node_alt) {
                    OpenAPI_qos_data_t *Alt = NULL;
                    OpenAPI_lnode_t *node_dec = NULL;
                    int a = pcc_rule->alt_qos.num_of_set;

                    if (!node_alt->data)
                        continue;
                    if (a >= OGS_MAX_NUM_OF_ALT_QOS) {
                        ogs_error("Too many alternative QoS parameter sets [%d]", OGS_MAX_NUM_OF_ALT_QOS);
                        break;
                    }
                    OpenAPI_list_for_each(SmPolicyDecision->qos_decs, node_dec) {
                        OpenAPI_map_t *Dec = node_dec->data;

                        if (Dec && Dec->value && ((OpenAPI_qos_data_t *)Dec->value)->qos_id &&
                                !strcmp(((OpenAPI_qos_data_t *)Dec->value)->qos_id, node_alt->data)) {
                            Alt = Dec->value;
                            break;
                        }
                    }
                    if (!Alt) {
                        ogs_error("The alternative QoS data [%s] is not in qosDecs", (char *)node_alt->data);
                        continue;
                    }
                    ogs_cpystrn(pcc_rule->alt_qos.set[a].id, Alt->qos_id, sizeof(pcc_rule->alt_qos.set[a].id));
                    if (Alt->is_packet_delay_budget) {
                        pcc_rule->alt_qos.set[a].pdb_presence = true;
                        pcc_rule->alt_qos.set[a].pdb = Alt->packet_delay_budget;
                    }
                    if (Alt->packet_error_rate)
                        ogs_cpystrn(pcc_rule->alt_qos.set[a].per, Alt->packet_error_rate, sizeof(pcc_rule->alt_qos.set[a].per));
                    if (Alt->gbr_ul)
                        pcc_rule->alt_qos.set[a].gbr.uplink = ogs_sbi_bitrate_from_string(Alt->gbr_ul);
                    if (Alt->gbr_dl)
                        pcc_rule->alt_qos.set[a].gbr.downlink = ogs_sbi_bitrate_from_string(Alt->gbr_dl);
                    read_pdu_set_qos(&pcc_rule->alt_qos.set[a].pdu_set_dl, Alt->pdu_set_qos_dl);
                    read_pdu_set_qos(&pcc_rule->alt_qos.set[a].pdu_set_ul, Alt->pdu_set_qos_ul);
                    pcc_rule->alt_qos.num_of_set++;
                }
            }

            sess->policy.num_of_pcc_rule++;
        }
    }
}

bool smf_npcf_smpolicycontrol_handle_create(
        smf_sess_t *sess, ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    int rv;
    char buf1[OGS_ADDRSTRLEN];
    char buf2[OGS_ADDRSTRLEN];

    smf_ue_t *smf_ue = NULL;
    smf_bearer_t *qos_flow = NULL;
    ogs_pfcp_pdr_t *dl_pdr = NULL;
    ogs_pfcp_pdr_t *ul_pdr = NULL;
    ogs_pfcp_pdr_t *cp2up_pdr = NULL;
    ogs_pfcp_pdr_t *up2cp_pdr = NULL;
    ogs_pfcp_far_t *dl_far = NULL;
    ogs_pfcp_far_t *up2cp_far = NULL;
    ogs_pfcp_qer_t *qer = NULL;

    OpenAPI_sm_policy_decision_t *SmPolicyDecision = NULL;
    OpenAPI_lnode_t *node = NULL;

#define MAX_TRIGGER_ID 128
    bool trigger_results[MAX_TRIGGER_ID];

    ogs_sbi_message_t message;
    ogs_sbi_header_t header;

    bool rc;
    ogs_sbi_client_t *client = NULL;
    OpenAPI_uri_scheme_e scheme = OpenAPI_uri_scheme_NULL;
    char *fqdn = NULL;
    uint16_t fqdn_port = 0;
    ogs_sockaddr_t *addr = NULL, *addr6 = NULL;

    ogs_assert(sess);
    smf_ue = smf_ue_find_by_id(sess->smf_ue_id);
    ogs_assert(smf_ue);

    ogs_assert(recvmsg);

    if (!recvmsg->http.location) {
        ogs_error("[%s:%d] No http.location", smf_ue->supi, sess->psi);
        return false;
    }

    SmPolicyDecision = recvmsg->SmPolicyDecision;
    if (!SmPolicyDecision) {
        ogs_error("[%s:%d] No SmPolicyDecision", smf_ue->supi, sess->psi);
        return false;
    }

    memset(&header, 0, sizeof(header));
    header.uri = recvmsg->http.location;

    rv = ogs_sbi_parse_header(&message, &header);
    if (rv != OGS_OK) {
        ogs_error("[%s:%d] Cannot parse http.location [%s]",
                smf_ue->supi, sess->psi, recvmsg->http.location);
        return false;
    }

    if (!message.h.resource.component[1]) {
        ogs_error("[%s:%d] No Assocication ID [%s]",
                smf_ue->supi, sess->psi, recvmsg->http.location);

        ogs_sbi_header_free(&header);
        return false;
    }

    rc = ogs_sbi_getaddr_from_uri(
            &scheme, &fqdn, &fqdn_port, &addr, &addr6, header.uri);
    if (rc == false || scheme == OpenAPI_uri_scheme_NULL) {
        ogs_error("[%s:%d] Invalid URI [%s]",
                smf_ue->supi, sess->psi, header.uri);
        ogs_sbi_header_free(&header);
        return false;
    }

    client = ogs_sbi_client_find(scheme, fqdn, fqdn_port, addr, addr6);
    if (!client) {
        ogs_debug("[%s:%d] ogs_sbi_client_add()", smf_ue->supi, sess->psi);
        client = ogs_sbi_client_add(scheme, fqdn, fqdn_port, addr, addr6);
        if (!client) {
            ogs_error("[%s:%d] ogs_sbi_client_add() failed",
                    smf_ue->supi, sess->psi);

            ogs_sbi_header_free(&header);
            ogs_free(fqdn);
            ogs_freeaddrinfo(addr);
            ogs_freeaddrinfo(addr6);

            return false;
        }
    }

    OGS_SBI_SETUP_CLIENT(&sess->policy_association, client);

    ogs_free(fqdn);
    ogs_freeaddrinfo(addr);
    ogs_freeaddrinfo(addr6);

    PCF_SM_POLICY_STORE(sess, header.uri, message.h.resource.component[1]);

    ogs_sbi_header_free(&header);

    /* SBI Features */
    if (SmPolicyDecision->supp_feat) {
        uint64_t supported_features, supported_features_hi;

        ogs_sbi_features_from_string(SmPolicyDecision->supp_feat, &supported_features, &supported_features_hi);
        sess->smpolicycontrol_features &= supported_features;
        sess->smpolicycontrol_features_hi &= supported_features_hi;
    } else {
        sess->smpolicycontrol_features = 0;
        sess->smpolicycontrol_features_hi = 0;
    }

    /*********************************************************************
     * Handle Policy Control Request Triggers
     *********************************************************************/

    /* Get policy control request triggers */
    memset(&trigger_results, 0, sizeof(trigger_results));
    OpenAPI_list_for_each(SmPolicyDecision->policy_ctrl_req_triggers, node) {
        if (node->data) {
            OpenAPI_policy_control_request_trigger_e trigger_id =
                (intptr_t)node->data;

            ogs_assert(trigger_id < MAX_TRIGGER_ID);
            trigger_results[trigger_id] = true;
        }
    }

    /* Update authorized session-AMBR */
    if (SmPolicyDecision->sess_rules) {
        OpenAPI_map_t *SessRuleMap = NULL;
        OpenAPI_session_rule_t *SessionRule = NULL;

        OpenAPI_ambr_t *AuthSessAmbr = NULL;
        OpenAPI_authorized_default_qos_t *AuthDefQos = NULL;

        OpenAPI_list_for_each(SmPolicyDecision->sess_rules, node) {
            SessRuleMap = node->data;
            if (!SessRuleMap) {
                ogs_error("No SessRuleMap");
                continue;
            }

            SessionRule = SessRuleMap->value;
            if (!SessionRule) {
                ogs_error("No SessionRule");
                continue;
            }


            AuthSessAmbr = SessionRule->auth_sess_ambr;
            if (AuthSessAmbr && trigger_results[
                OpenAPI_policy_control_request_trigger_SE_AMBR_CH] == true) {
                if (AuthSessAmbr->uplink)
                    sess->session.ambr.uplink =
                        ogs_sbi_bitrate_from_string(AuthSessAmbr->uplink);
                if (AuthSessAmbr->downlink)
                    sess->session.ambr.downlink =
                        ogs_sbi_bitrate_from_string(AuthSessAmbr->downlink);
            }

            AuthDefQos = SessionRule->auth_def_qos;
            if (AuthDefQos && trigger_results[
                OpenAPI_policy_control_request_trigger_DEF_QOS_CH] == true) {
                sess->session.qos.index = AuthDefQos->_5qi;
                sess->session.qos.arp.priority_level =
                    AuthDefQos->priority_level;
                if (AuthDefQos->arp) {
                    sess->session.qos.arp.priority_level =
                            AuthDefQos->arp->priority_level;
                    if (AuthDefQos->arp->preempt_cap ==
                        OpenAPI_preemption_capability_NOT_PREEMPT)
                        sess->session.qos.arp.pre_emption_capability =
                            OGS_5GC_PRE_EMPTION_DISABLED;
                    else if (AuthDefQos->arp->preempt_cap ==
                        OpenAPI_preemption_capability_MAY_PREEMPT)
                        sess->session.qos.arp.pre_emption_capability =
                            OGS_5GC_PRE_EMPTION_ENABLED;
                    else {
                        ogs_error("Unknown preemptCap [%d], "
                                "fall back to NOT_PREEMPT",
                                (int)AuthDefQos->arp->preempt_cap);
                        sess->session.qos.arp.pre_emption_capability =
                            OGS_5GC_PRE_EMPTION_DISABLED;
                    }

                    if (AuthDefQos->arp->preempt_vuln ==
                        OpenAPI_preemption_vulnerability_NOT_PREEMPTABLE)
                        sess->session.qos.arp.pre_emption_vulnerability =
                            OGS_5GC_PRE_EMPTION_DISABLED;
                    else if (AuthDefQos->arp->preempt_vuln ==
                        OpenAPI_preemption_vulnerability_PREEMPTABLE)
                        sess->session.qos.arp.pre_emption_vulnerability =
                            OGS_5GC_PRE_EMPTION_ENABLED;
                    else {
                        ogs_error("Unknown preemptVuln [%d], "
                                "fall back to NOT_PREEMPTABLE",
                                (int)AuthDefQos->arp->preempt_vuln);
                        sess->session.qos.arp.pre_emption_vulnerability =
                            OGS_5GC_PRE_EMPTION_DISABLED;
                    }
                }
            }
        }
    }

    /* Update authorized PCC rule & QoS */
    update_authorized_pcc_rule_and_qos(sess, SmPolicyDecision);

    /*********************************************************************
     * Send PFCP Session Establiashment Request to the UPF
     *********************************************************************/

    /* Select UPF based on UE Location Information */
    smf_sess_select_upf(sess);

    /* Check if UPF selection was successful */
    if (!sess->pfcp_node) {
        ogs_error("[%s:%d] No UPF available for session",
                  smf_ue->supi, sess->psi);
        return false;
    }

    /* Check if selected UPF is associated with SMF */
    if (!OGS_FSM_CHECK(&sess->pfcp_node->sm, smf_pfcp_state_associated)) {
        ogs_error("[%s:%d] No associated UPF", smf_ue->supi, sess->psi);
        return false;
    }

    /* Remove all previous QoS flow */
    smf_bearer_remove_all(sess);

    /* Setup Default QoS flow */
    qos_flow = smf_qos_flow_add(sess);
    ogs_assert(qos_flow);

    /* Setup CP/UP Data Forwarding PDR/FAR */
    smf_sess_create_cp_up_data_forwarding(sess);

    /* Copy Session QoS information to Default QoS Flow */
    memcpy(&qos_flow->qos, &sess->session.qos, sizeof(ogs_qos_t));

    /* Setup QER */
    qer = qos_flow->qer;
    ogs_assert(qer);
    qer->mbr.uplink = sess->session.ambr.uplink;
    qer->mbr.downlink = sess->session.ambr.downlink;

    /* Setup PDR */
    dl_pdr = qos_flow->dl_pdr;
    ogs_assert(dl_pdr);
    ul_pdr = qos_flow->ul_pdr;
    ogs_assert(ul_pdr);
    cp2up_pdr = sess->cp2up_pdr;
    ogs_assert(cp2up_pdr);
    up2cp_pdr = sess->up2cp_pdr;
    ogs_assert(up2cp_pdr);

    /* Setup FAR */
    dl_far = qos_flow->dl_far;
    ogs_assert(dl_far);
    up2cp_far = sess->up2cp_far;
    ogs_assert(up2cp_far);

    /* Set UE IP Address to the Default DL PDR */
    ogs_assert(OGS_OK ==
        ogs_pfcp_paa_to_ue_ip_addr(&sess->paa,
            &dl_pdr->ue_ip_addr, &dl_pdr->ue_ip_addr_len));
    dl_pdr->ue_ip_addr.sd = OGS_PFCP_UE_IP_DST;

    if (ogs_global_conf()->parameter.use_upg_vpp == true) {
        ogs_assert(OGS_OK ==
            ogs_pfcp_paa_to_ue_ip_addr(&sess->paa,
                &ul_pdr->ue_ip_addr, &ul_pdr->ue_ip_addr_len));
    }

    if (sess->session.ipv4_framed_routes &&
        sess->pfcp_node->up_function_features.frrt) {
        int i = 0;
        for (i = 0; i < OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI; i++) {
            const char *route = sess->session.ipv4_framed_routes[i];
            if (!route) break;

            if (!dl_pdr->ipv4_framed_routes) {
                dl_pdr->ipv4_framed_routes =
                    ogs_calloc(OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI,
                               sizeof(dl_pdr->ipv4_framed_routes[0]));
                ogs_assert(dl_pdr->ipv4_framed_routes);
            }
            dl_pdr->ipv4_framed_routes[i] = ogs_strdup(route);

            if (!ul_pdr->ipv4_framed_routes) {
                ul_pdr->ipv4_framed_routes =
                    ogs_calloc(OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI,
                               sizeof(ul_pdr->ipv4_framed_routes[0]));
                ogs_assert(ul_pdr->ipv4_framed_routes);
            }
            ul_pdr->ipv4_framed_routes[i] = ogs_strdup(route);
        }
    }

    if (sess->session.ipv6_framed_routes &&
        sess->pfcp_node->up_function_features.frrt) {
        int i = 0;
        for (i = 0; i < OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI; i++) {
            const char *route = sess->session.ipv6_framed_routes[i];
            if (!route) break;

            if (!dl_pdr->ipv6_framed_routes) {
                dl_pdr->ipv6_framed_routes =
                    ogs_calloc(OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI,
                               sizeof(dl_pdr->ipv6_framed_routes[0]));
                ogs_assert(dl_pdr->ipv6_framed_routes);
            }
            dl_pdr->ipv6_framed_routes[i] = ogs_strdup(route);

            if (!ul_pdr->ipv6_framed_routes) {
                ul_pdr->ipv6_framed_routes =
                    ogs_calloc(OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI,
                               sizeof(ul_pdr->ipv6_framed_routes[0]));
                ogs_assert(ul_pdr->ipv6_framed_routes);
            }
            ul_pdr->ipv6_framed_routes[i] = ogs_strdup(route);
        }
    }

    ogs_info("UE SUPI[%s] DNN[%s] IPv4[%s] IPv6[%s]",
        smf_ue->supi, sess->session.name,
        sess->ipv4 ? OGS_INET_NTOP(&sess->ipv4->addr, buf1) : "",
        sess->ipv6 ? OGS_INET6_NTOP(&sess->ipv6->addr, buf2) : "");

    /* Set UPF N3 DL Outer-Header-Creation */
    if (sess->remote_dl_ip.ipv4 || sess->remote_dl_ip.ipv6) {
        ogs_assert(OGS_OK ==
            ogs_pfcp_ip_to_outer_header_creation(
                &sess->remote_dl_ip,
                &dl_far->outer_header_creation,
                &dl_far->outer_header_creation_len));
        dl_far->outer_header_creation.teid = sess->remote_dl_teid;
        dl_far->apply_action = OGS_PFCP_APPLY_ACTION_FORW;
    }

    /* Set UE-to-CP Flow-Description and Outer-Header-Creation */
    up2cp_pdr->flow[up2cp_pdr->num_of_flow].fd = 1;
    up2cp_pdr->flow[up2cp_pdr->num_of_flow].description =
        (char *)"permit out 58 from ff02::2/128 to assigned";
    up2cp_pdr->num_of_flow++;

    ogs_assert(OGS_OK ==
        ogs_pfcp_ip_to_outer_header_creation(
            &ogs_gtp_self()->gtpu_ip,
            &up2cp_far->outer_header_creation,
            &up2cp_far->outer_header_creation_len));
    up2cp_far->outer_header_creation.teid = sess->index;

    /* Set UPF-N3 TEID & ADDR to the Default UL PDR */
    ogs_assert(sess->pfcp_node);
    if (sess->pfcp_node->up_function_features.ftup) {

       /* TS 129 244 V16.5.0 8.2.3
        *
        * At least one of the V4 and V6 flags shall be set to "1",
        * and both may be set to "1" for both scenarios:
        *
        * - when the CP function is providing F-TEID, i.e.
        *   both IPv4 address field and IPv6 address field may be present;
        *   or
        * - when the UP function is requested to allocate the F-TEID,
        *   i.e. when CHOOSE bit is set to "1",
        *   and the IPv4 address and IPv6 address fields are not present.
        */

        ul_pdr->f_teid.ipv4 = 1;
        ul_pdr->f_teid.ipv6 = 1;
        ul_pdr->f_teid.ch = 1;
        ul_pdr->f_teid.chid = 1;
        ul_pdr->f_teid.choose_id = OGS_PFCP_DEFAULT_CHOOSE_ID;
        ul_pdr->f_teid_len = 2;

        cp2up_pdr->f_teid.ipv4 = 1;
        cp2up_pdr->f_teid.ipv6 = 1;
        cp2up_pdr->f_teid.ch = 1;
        cp2up_pdr->f_teid_len = 1;

        up2cp_pdr->f_teid.ipv4 = 1;
        up2cp_pdr->f_teid.ipv6 = 1;
        up2cp_pdr->f_teid.ch = 1;
        up2cp_pdr->f_teid.chid = 1;
        up2cp_pdr->f_teid.choose_id = OGS_PFCP_DEFAULT_CHOOSE_ID;
        up2cp_pdr->f_teid_len = 2;
    } else {
        ogs_assert(sess->pfcp_node->addr_list);
        if (sess->pfcp_node->addr_list->ogs_sa_family == AF_INET)
            ogs_assert(OGS_OK ==
                ogs_copyaddrinfo(
                    &sess->local_ul_addr, sess->pfcp_node->addr_list));
        else if (sess->pfcp_node->addr_list->ogs_sa_family == AF_INET6)
            ogs_assert(OGS_OK ==
                ogs_copyaddrinfo(
                    &sess->local_ul_addr6, sess->pfcp_node->addr_list));
        else
            ogs_assert_if_reached();

        sess->local_ul_teid = ul_pdr->teid;

        ogs_assert(OGS_OK ==
            ogs_pfcp_sockaddr_to_f_teid(
                sess->local_ul_addr, sess->local_ul_addr6,
                &ul_pdr->f_teid, &ul_pdr->f_teid_len));
        ul_pdr->f_teid.teid = sess->local_ul_teid;

        ogs_assert(OGS_OK ==
            ogs_pfcp_sockaddr_to_f_teid(
                sess->local_ul_addr, sess->local_ul_addr6,
                &cp2up_pdr->f_teid, &cp2up_pdr->f_teid_len));
        cp2up_pdr->f_teid.teid = cp2up_pdr->teid;

        ogs_assert(OGS_OK ==
            ogs_pfcp_sockaddr_to_f_teid(
                sess->local_ul_addr, sess->local_ul_addr6,
                &up2cp_pdr->f_teid, &up2cp_pdr->f_teid_len));
        up2cp_pdr->f_teid.teid = sess->local_ul_teid;
    }

    dl_pdr->precedence = OGS_PFCP_DEFAULT_PDR_PRECEDENCE;
    ul_pdr->precedence = OGS_PFCP_DEFAULT_PDR_PRECEDENCE;

    cp2up_pdr->precedence = OGS_PFCP_CP2UP_PDR_PRECEDENCE;
    up2cp_pdr->precedence = OGS_PFCP_UP2CP_PDR_PRECEDENCE;

    ogs_assert(OGS_OK ==
            smf_5gc_pfcp_send_session_establishment_request(sess, stream, 0));

    return true;
}

bool smf_npcf_smpolicycontrol_handle_update_notify(
        smf_sess_t *sess, ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    char *strerror = NULL;
    smf_ue_t *smf_ue = NULL;

    OpenAPI_sm_policy_notification_t *SmPolicyNotification = NULL;
    OpenAPI_sm_policy_decision_t *SmPolicyDecision = NULL;

    ogs_assert(sess);
    ogs_assert(stream);
    smf_ue = smf_ue_find_by_id(sess->smf_ue_id);
    ogs_assert(smf_ue);

    ogs_assert(recvmsg);

    SmPolicyNotification = recvmsg->SmPolicyNotification;
    if (!SmPolicyNotification) {
        strerror = ogs_msprintf("[%s:%d] No SmPolicyNotification",
                smf_ue->supi, sess->psi);
        goto cleanup;
    }

    SmPolicyDecision = SmPolicyNotification->sm_policy_decision;
    if (!SmPolicyDecision) {
        strerror = ogs_msprintf("[%s:%d] No SmPolicyDecision",
                smf_ue->supi, sess->psi);
        goto cleanup;
    }

    /* Update authorized PCC rule & QoS */
    update_authorized_pcc_rule_and_qos(sess, SmPolicyDecision);

    ogs_assert(true == ogs_sbi_send_http_status_no_content(stream));

    smf_qos_flow_binding(sess);

    return true;

cleanup:
    ogs_assert(strerror);

    ogs_error("%s", strerror);
    ogs_assert(true ==
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            recvmsg, strerror, NULL, NULL));
    ogs_free(strerror);

    return false;
}

bool smf_npcf_smpolicycontrol_handle_terminate_notify(
        smf_sess_t *sess, ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    smf_ue_t *smf_ue = NULL;

    ogs_assert(sess);
    ogs_assert(stream);
    smf_ue = smf_ue_find_by_id(sess->smf_ue_id);
    ogs_assert(smf_ue);

    ogs_assert(recvmsg);

    ogs_assert(true == ogs_sbi_send_http_status_no_content(stream));

    smf_trigger_session_release(
            sess, NULL, OGS_PFCP_DELETE_TRIGGER_PCF_INITIATED);

    return true;
}
