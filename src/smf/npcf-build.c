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

#include "npcf-build.h"

ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_create(
        smf_sess_t *sess, void *data)
{
    smf_ue_t *smf_ue = NULL;

    ogs_sbi_message_t message;
    ogs_sbi_header_t header;
    ogs_sbi_request_t *request = NULL;
    ogs_sbi_server_t *server = NULL;

    OpenAPI_sm_policy_context_data_t SmPolicyContextData;
    OpenAPI_ambr_t SubsSessAmbr;
    OpenAPI_subscribed_default_qos_t SubsDefQos;
    OpenAPI_arp_t Arp;
    OpenAPI_snssai_t sNssai;
    OpenAPI_user_location_t ueLocation;

    ogs_assert(sess);
    ogs_assert(sess->sm_context_ref);
    ogs_assert(sess->session.name);
    smf_ue = smf_ue_find_by_id(sess->smf_ue_id);
    ogs_assert(smf_ue);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.service.name =
        OpenAPI_service_name_ToString(
                OpenAPI_service_name_npcf_smpolicycontrol);
    message.h.api.version = (char *)OGS_SBI_API_V1;
    message.h.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_SM_POLICIES;

    memset(&SmPolicyContextData, 0, sizeof(SmPolicyContextData));
    memset(&sNssai, 0, sizeof(sNssai));
    memset(&SubsSessAmbr, 0, sizeof(SubsSessAmbr));
    memset(&ueLocation, 0, sizeof(ueLocation));

    SmPolicyContextData.supi = smf_ue->supi;
    if (!SmPolicyContextData.supi) {
        ogs_error("No supi");
        goto end;
    }
    SmPolicyContextData.pdu_session_id = sess->psi;
    if (!SmPolicyContextData.pdu_session_id) {
        ogs_error("No pdu_session_id");
        goto end;
    }
    SmPolicyContextData.pdu_session_type = sess->session.session_type;
    if (!SmPolicyContextData.pdu_session_type) {
        ogs_error("No pdu_session_type");
        goto end;
    }

    /*
     * TODO: Currently hard-coded to "00000800";
     * replace with dynamic value if needed in future
     */
    SmPolicyContextData.chargingcharacteristics = (char *)"00000800";

    /* GPSI */
    SmPolicyContextData.gpsi = smf_ue->gpsi;

    /*
     * Use ogs_sbi_supi_in_vplmn() instead of ogs_sbi_plmn_id_in_vplmn().
     * This is because some vendors might not use the full DNN in LBO and
     * Open5GS cannot derive the home PLMN ID without the full DNN.
     *
     * TS29.502
     * 6.1 Nsmf_PDUSession Service API
     * Table 6.1.6.2.2-1: Definition of type SmContextCreateData
     *
     * NAME: dnn
     * Data type: Dnn
     * P: C
     * Cardinality: 0..1
     *
     * This IE shall be present, except during an EPS to 5GS Idle mode mobility
     * or handover using the N26 interface.
     *
     * When present, it shall contain the requested DNN; the DNN shall
     * be the full DNN (i.e. with both the Network Identifier and
     * Operator Identifier) for a HR PDU session, and it should be
     * the full DNN in LBO and non-roaming scenarios. If the Operator Identifier
     * is absent, the serving core network operator shall be assumed.
     *
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
    if (ogs_sbi_supi_in_vplmn(smf_ue->supi) == true) {
        char *dnn_oi = NULL;

        dnn_oi = ogs_dnn_oi_from_plmn_id(&sess->home_plmn_id);
        ogs_assert(dnn_oi);

        SmPolicyContextData.dnn =
            ogs_msprintf("%s.%s", sess->session.name, dnn_oi);
        ogs_assert(SmPolicyContextData.dnn);

        ogs_free(dnn_oi);

    } else {
        SmPolicyContextData.dnn = ogs_strdup(sess->session.name);
        ogs_assert(SmPolicyContextData.dnn);
    }

    server = ogs_sbi_server_first();
    if (!server) {
        ogs_error("No server");
        goto end;
    }

    memset(&header, 0, sizeof(header));
    header.service.name = (char *)OGS_SBI_SERVICE_NAME_NSMF_CALLBACK;
    header.api.version = (char *)OGS_SBI_API_V1;
    header.resource.component[0] =
            (char *)OGS_SBI_RESOURCE_NAME_SM_POLICY_NOTIFY;
    header.resource.component[1] = sess->sm_context_ref;
    SmPolicyContextData.notification_uri = ogs_sbi_server_uri(server, &header);
    if (!SmPolicyContextData.notification_uri) {
        ogs_error("No notification_uri");
        goto end;
    }

    SmPolicyContextData.rat_type = OpenAPI_rat_type_NR;

    SmPolicyContextData.serving_network =
        ogs_sbi_build_plmn_id_nid(&sess->serving_plmn_id);
    if (!SmPolicyContextData.serving_network) {
        ogs_error("No serving_network");
        goto end;
    }

    ueLocation.nr_location = ogs_sbi_build_nr_location(
            &sess->nr_tai, &sess->nr_cgi);
    if (!ueLocation.nr_location) {
        ogs_error("ueLocation.nr_location");
        goto end;
    }
    if (sess->ue_location_timestamp)
        ueLocation.nr_location->ue_location_timestamp =
            ogs_sbi_gmtime_string(sess->ue_location_timestamp);

    SmPolicyContextData.user_location_info = &ueLocation;

    SmPolicyContextData.ue_time_zone =
        ogs_sbi_timezone_string(ogs_timezone());
    if (!SmPolicyContextData.ue_time_zone) {
        ogs_error("SmPolicyContextData.ue_time_zone");
        goto end;
    }

    if (sess->ipv4) {
        SmPolicyContextData.ipv4_address =
                ogs_ipv4_to_string(sess->ipv4->addr[0]);
        if (!SmPolicyContextData.ipv4_address) {
            ogs_error("No ipv4_address");
            goto end;
        }
    }

    if (sess->ipv6) {
        SmPolicyContextData.ipv6_address_prefix = ogs_ipv6prefix_to_string(
                (uint8_t *)sess->ipv6->addr, OGS_IPV6_DEFAULT_PREFIX_LEN);
        if (!SmPolicyContextData.ipv6_address_prefix) {
            ogs_error("No ipv6_address_prefix");
            goto end;
        }
    }

    if (sess->session.ipv4_framed_routes) {
        int i;
        OpenAPI_list_t *FrameRouteList = OpenAPI_list_create();

        for (i = 0; i < OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI; i++) {
            const char *route = sess->session.ipv4_framed_routes[i];
            if (!route) break;
            OpenAPI_list_add(FrameRouteList, ogs_strdup(route));
        }
        SmPolicyContextData.ipv4_frame_route_list = FrameRouteList;
    }

    if (sess->session.ipv6_framed_routes) {
        int i;
        OpenAPI_list_t *FrameRouteList = OpenAPI_list_create();

        for (i = 0; i < OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI; i++) {
            const char *route = sess->session.ipv6_framed_routes[i];
            if (!route) break;
            OpenAPI_list_add(FrameRouteList, ogs_strdup(route));
        }
        SmPolicyContextData.ipv6_frame_route_list = FrameRouteList;
    }

    if (OGS_SBI_FEATURES_IS_SET(sess->smpolicycontrol_features,
                OGS_SBI_NPCF_SMPOLICYCONTROL_DN_AUTHORIZATION)) {
        if (sess->session.ambr.uplink) {
            SubsSessAmbr.uplink = ogs_sbi_bitrate_to_string(
                sess->session.ambr.uplink, OGS_SBI_BITRATE_KBPS);
        }
        if (sess->session.ambr.downlink) {
            SubsSessAmbr.downlink = ogs_sbi_bitrate_to_string(
                sess->session.ambr.downlink, OGS_SBI_BITRATE_KBPS);
        }
        if (SubsSessAmbr.downlink || SubsSessAmbr.uplink) {
            SmPolicyContextData.subs_sess_ambr = &SubsSessAmbr;
        }
    }

    memset(&Arp, 0, sizeof(Arp));
    if (sess->session.qos.arp.pre_emption_capability ==
            OGS_5GC_PRE_EMPTION_ENABLED)
        Arp.preempt_cap = OpenAPI_preemption_capability_MAY_PREEMPT;
    else if (sess->session.qos.arp.pre_emption_capability ==
            OGS_5GC_PRE_EMPTION_DISABLED)
        Arp.preempt_cap = OpenAPI_preemption_capability_NOT_PREEMPT;
    if (!Arp.preempt_cap) {
        ogs_error("No Arp.preempt_cap");
        goto end;
    }
    if (sess->session.qos.arp.pre_emption_vulnerability ==
            OGS_5GC_PRE_EMPTION_ENABLED)
        Arp.preempt_vuln = OpenAPI_preemption_vulnerability_PREEMPTABLE;
    else if (sess->session.qos.arp.pre_emption_vulnerability ==
            OGS_5GC_PRE_EMPTION_DISABLED)
        Arp.preempt_vuln = OpenAPI_preemption_vulnerability_NOT_PREEMPTABLE;
    if (!Arp.preempt_vuln) {
        ogs_error("No Arp.preempt_vuln");
        goto end;
    }
    Arp.priority_level = sess->session.qos.arp.priority_level;

    memset(&SubsDefQos, 0, sizeof(SubsDefQos));
    SubsDefQos.arp = &Arp;
    SubsDefQos._5qi = sess->session.qos.index;
    SubsDefQos.is_priority_level = true;
    SubsDefQos.priority_level = sess->session.qos.arp.priority_level;

    SmPolicyContextData.subs_def_qos = &SubsDefQos;

    if (sess->smpolicycontrol_features || sess->smpolicycontrol_features_hi) {
        SmPolicyContextData.supp_feat =
            ogs_sbi_features_to_string(sess->smpolicycontrol_features, sess->smpolicycontrol_features_hi);
        if (!SmPolicyContextData.supp_feat) {
            ogs_error("No supp_feat");
            goto end;
        }
    }

    sNssai.sst = sess->s_nssai.sst;
    sNssai.sd = ogs_s_nssai_sd_to_string(sess->s_nssai.sd);
    SmPolicyContextData.slice_info = &sNssai;

    message.SmPolicyContextData = &SmPolicyContextData;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

end:
    if (SmPolicyContextData.notification_uri)
        ogs_free(SmPolicyContextData.notification_uri);

    if (SmPolicyContextData.dnn)
        ogs_free(SmPolicyContextData.dnn);
    if (SmPolicyContextData.serving_network)
        ogs_sbi_free_plmn_id_nid(SmPolicyContextData.serving_network);

    if (ueLocation.nr_location) {
        if (ueLocation.nr_location->ue_location_timestamp)
            ogs_free(ueLocation.nr_location->ue_location_timestamp);
        ogs_sbi_free_nr_location(ueLocation.nr_location);
    }
    if (SmPolicyContextData.ue_time_zone)
        ogs_free(SmPolicyContextData.ue_time_zone);

    if (sNssai.sd)
        ogs_free(sNssai.sd);

    if (SubsSessAmbr.downlink) ogs_free(SubsSessAmbr.downlink);
    if (SubsSessAmbr.uplink) ogs_free(SubsSessAmbr.uplink);

    if (SmPolicyContextData.supp_feat)
        ogs_free(SmPolicyContextData.supp_feat);

    if (SmPolicyContextData.ipv4_address)
        ogs_free(SmPolicyContextData.ipv4_address);
    if (SmPolicyContextData.ipv6_address_prefix)
        ogs_free(SmPolicyContextData.ipv6_address_prefix);

    OpenAPI_clear_and_free_string_list(SmPolicyContextData.ipv4_frame_route_list);
    OpenAPI_clear_and_free_string_list(SmPolicyContextData.ipv6_frame_route_list);

    return request;
}

ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_usage_update(
        smf_sess_t *sess, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    smf_npcf_smpolicycontrol_usage_report_t *report = data;

    OpenAPI_sm_policy_update_context_data_t UpdateData;
    OpenAPI_accu_usage_report_t AccuUsageReport;
    OpenAPI_list_t *Triggers = NULL, *Reports = NULL;

    ogs_assert(sess);
    ogs_assert(sess->policy_association.resource_uri);
    ogs_assert(report);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = ogs_msprintf("%s/%s",
            sess->policy_association.resource_uri,
            OGS_SBI_RESOURCE_NAME_UPDATE);

    memset(&UpdateData, 0, sizeof(UpdateData));
    memset(&AccuUsageReport, 0, sizeof(AccuUsageReport));

    AccuUsageReport.ref_um_ids = report->um_id;
    AccuUsageReport.is_vol_usage = true;
    AccuUsageReport.vol_usage = (long)report->total;
    AccuUsageReport.is_vol_usage_uplink = true;
    AccuUsageReport.vol_usage_uplink = (long)report->uplink;
    AccuUsageReport.is_vol_usage_downlink = true;
    AccuUsageReport.vol_usage_downlink = (long)report->downlink;
    AccuUsageReport.is_time_usage = true;
    AccuUsageReport.time_usage = (int)report->time;

    Reports = OpenAPI_list_create();
    ogs_assert(Reports);
    OpenAPI_list_add(Reports, &AccuUsageReport);
    UpdateData.accu_usage_reports = Reports;

    /* TS 29.512 V19.7.0 clause 4.2.4.10.1: with the report the SMF includes US_RE in repPolicyCtrlReqTriggers */
    if (report->trigger) {
        Triggers = OpenAPI_list_create();
        ogs_assert(Triggers);
        OpenAPI_list_add(Triggers,
                (void *)OpenAPI_policy_control_request_trigger_US_RE);
        UpdateData.rep_policy_ctrl_req_triggers = Triggers;
    }

    message.SmPolicyUpdateContextData = &UpdateData;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    OpenAPI_list_free(Reports);
    if (Triggers)
        OpenAPI_list_free(Triggers);
    ogs_free(message.h.uri);
    ogs_free(report->um_id);
    ogs_free(report);

    return request;
}

/*
 * qncReports of SmPolicyUpdateContextData is an array of the QosNotificationControlInfo of TS 29.512 (refPccRuleIds, notifType). The
 * generated model of that name is the one of TS 29.514 (flows, notifType), because both YAML files define the schema under one name,
 * so the report is added to the JSON of the request.
 */
ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_qnc_update(
        smf_sess_t *sess, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    smf_npcf_smpolicycontrol_qnc_report_t *report = data;
    OpenAPI_sm_policy_update_context_data_t UpdateData;
    OpenAPI_list_t *Triggers = NULL;
    cJSON *body = NULL, *reports = NULL, *info = NULL, *ids = NULL;
    char *text = NULL;
    int i;

    ogs_assert(sess);
    ogs_assert(sess->policy_association.resource_uri);
    ogs_assert(report);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = ogs_msprintf("%s/%s",
            sess->policy_association.resource_uri,
            OGS_SBI_RESOURCE_NAME_UPDATE);

    memset(&UpdateData, 0, sizeof(UpdateData));
    Triggers = OpenAPI_list_create();
    ogs_assert(Triggers);
    OpenAPI_list_add(Triggers, (void *)OpenAPI_policy_control_request_trigger_QOS_NOTIF);
    UpdateData.rep_policy_ctrl_req_triggers = Triggers;
    message.SmPolicyUpdateContextData = &UpdateData;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    if (request && request->http.content) {
        body = cJSON_Parse(request->http.content);
        ogs_assert(body);
        reports = cJSON_AddArrayToObject(body, "qncReports");
        info = cJSON_CreateObject();
        ogs_assert(reports && info);
        ids = cJSON_AddArrayToObject(info, "refPccRuleIds");
        ogs_assert(ids);
        for (i = 0; i < report->num_of_rule; i++)
            cJSON_AddItemToArray(ids, cJSON_CreateString(report->rule_id[i]));
        cJSON_AddStringToObject(info, "notifType", OpenAPI_qos_notif_type_ToString(report->notif_type));
        if (report->alt_qos_param_id)
            cJSON_AddStringToObject(info, "altQosParamId", report->alt_qos_param_id);
        cJSON_AddItemToArray(reports, info);
        text = cJSON_PrintUnformatted(body);
        ogs_assert(text);
        ogs_free(request->http.content);
        request->http.content = ogs_strdup(text);
        request->http.content_length = strlen(text);
        cJSON_free(text);
        cJSON_Delete(body);
    }

    OpenAPI_list_free(Triggers);
    ogs_free(message.h.uri);
    for (i = 0; i < report->num_of_rule; i++)
        ogs_free(report->rule_id[i]);
    if (report->alt_qos_param_id)
        ogs_free(report->alt_qos_param_id);
    ogs_free(report);

    return request;
}

/*
 * l4sReports of SmPolicyUpdateContextData is an array of L4sSupportInfo (refPccRuleIds, notifType NOT_AVAILABLE or AVAILABLE, TS 29.512
 * V19.7.0 table 5.6.2.57-1), with L4S_SUPP in the triggers.
 */
ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_l4s_update(
        smf_sess_t *sess, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    smf_npcf_smpolicycontrol_l4s_report_t *report = data;
    OpenAPI_sm_policy_update_context_data_t UpdateData;
    OpenAPI_list_t *Triggers = NULL;
    cJSON *body = NULL, *reports = NULL, *info = NULL, *ids = NULL;
    char *text = NULL;
    int i;

    ogs_assert(sess);
    ogs_assert(sess->policy_association.resource_uri);
    ogs_assert(report);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = ogs_msprintf("%s/%s", sess->policy_association.resource_uri, OGS_SBI_RESOURCE_NAME_UPDATE);

    memset(&UpdateData, 0, sizeof(UpdateData));
    Triggers = OpenAPI_list_create();
    ogs_assert(Triggers);
    OpenAPI_list_add(Triggers, (void *)OpenAPI_policy_control_request_trigger_L4S_SUPP);
    UpdateData.rep_policy_ctrl_req_triggers = Triggers;
    message.SmPolicyUpdateContextData = &UpdateData;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    if (request && request->http.content) {
        body = cJSON_Parse(request->http.content);
        ogs_assert(body);
        reports = cJSON_AddArrayToObject(body, "l4sReports");
        info = cJSON_CreateObject();
        ogs_assert(reports && info);
        ids = cJSON_AddArrayToObject(info, "refPccRuleIds");
        ogs_assert(ids);
        for (i = 0; i < report->num_of_rule; i++)
            cJSON_AddItemToArray(ids, cJSON_CreateString(report->rule_id[i]));
        cJSON_AddStringToObject(info, "notifType", report->available ? "AVAILABLE" : "NOT_AVAILABLE");
        cJSON_AddItemToArray(reports, info);
        text = cJSON_PrintUnformatted(body);
        ogs_assert(text);
        ogs_free(request->http.content);
        request->http.content = ogs_strdup(text);
        request->http.content_length = strlen(text);
        cJSON_free(text);
        cJSON_Delete(body);
    }

    OpenAPI_list_free(Triggers);
    ogs_free(message.h.uri);
    for (i = 0; i < report->num_of_rule; i++)
        ogs_free(report->rule_id[i]);
    ogs_free(report);

    return request;
}

/*
 * qosMonReports of SmPolicyUpdateContextData is an array of the QosMonitoringReport of TS 29.512 (refPccRuleIds, ulDelays, dlDelays,
 * rtDelays, pdmf); the generated model of that name is the one of TS 29.514 (flows), both YAML files using one schema name, so the
 * report is added to the JSON of the request.
 */
ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_qos_mon_update(
        smf_sess_t *sess, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    smf_npcf_smpolicycontrol_qos_mon_report_t *report = data;
    OpenAPI_sm_policy_update_context_data_t UpdateData;
    OpenAPI_list_t *Triggers = NULL;
    cJSON *body = NULL, *reports = NULL, *info = NULL, *ids = NULL, *arr = NULL;
    char *text = NULL;

    ogs_assert(sess);
    ogs_assert(sess->policy_association.resource_uri);
    ogs_assert(report);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = ogs_msprintf("%s/%s", sess->policy_association.resource_uri, OGS_SBI_RESOURCE_NAME_UPDATE);

    memset(&UpdateData, 0, sizeof(UpdateData));
    Triggers = OpenAPI_list_create();
    ogs_assert(Triggers);
    OpenAPI_list_add(Triggers, (void *)OpenAPI_policy_control_request_trigger_QOS_MONITORING);
    UpdateData.rep_policy_ctrl_req_triggers = Triggers;
    message.SmPolicyUpdateContextData = &UpdateData;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    if (request && request->http.content) {
        body = cJSON_Parse(request->http.content);
        ogs_assert(body);
        reports = cJSON_AddArrayToObject(body, "qosMonReports");
        info = cJSON_CreateObject();
        ogs_assert(reports && info);
        ids = cJSON_AddArrayToObject(info, "refPccRuleIds");
        ogs_assert(ids);
        cJSON_AddItemToArray(ids, cJSON_CreateString(report->rule_id));
        if (report->measurement.ul_presence) {
            arr = cJSON_AddArrayToObject(info, "ulDelays");
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(report->measurement.ul));
        }
        if (report->measurement.dl_presence) {
            arr = cJSON_AddArrayToObject(info, "dlDelays");
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(report->measurement.dl));
        }
        if (report->measurement.rt_presence) {
            arr = cJSON_AddArrayToObject(info, "rtDelays");
            cJSON_AddItemToArray(arr, cJSON_CreateNumber(report->measurement.rt));
        }
        /* PacketDelayFailureReport: no measurement result within the measurement period */
        if (report->measurement.failure)
            cJSON_AddBoolToObject(info, "pdmf", 1);
        cJSON_AddItemToArray(reports, info);
        text = cJSON_PrintUnformatted(body);
        ogs_assert(text);
        ogs_free(request->http.content);
        request->http.content = ogs_strdup(text);
        request->http.content_length = strlen(text);
        cJSON_free(text);
        cJSON_Delete(body);
    }

    OpenAPI_list_free(Triggers);
    ogs_free(message.h.uri);
    ogs_free(report->rule_id);
    ogs_free(report);

    return request;
}

ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_update(
        smf_sess_t *sess, void *data)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    smf_npcf_smpolicycontrol_rule_report_t *report = data;

    OpenAPI_sm_policy_update_context_data_t UpdateData;
    OpenAPI_rule_report_t RuleReport;
    OpenAPI_list_t *Triggers = NULL, *RuleReports = NULL, *Ids = NULL;
    int i;

    ogs_assert(sess);
    ogs_assert(sess->policy_association.resource_uri);
    ogs_assert(report);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = ogs_msprintf("%s/%s",
            sess->policy_association.resource_uri,
            OGS_SBI_RESOURCE_NAME_UPDATE);

    memset(&UpdateData, 0, sizeof(UpdateData));
    memset(&RuleReport, 0, sizeof(RuleReport));

    /* TS 29.512 V19.7.0 clause 4.2.4.14: SUCC_RES_ALLO in repPolicyCtrlReqTriggers with the ruleReports of the rules */
    if (report->status == OpenAPI_rule_status_ACTIVE) {
        Triggers = OpenAPI_list_create();
        ogs_assert(Triggers);
        OpenAPI_list_add(Triggers,
                (void *)OpenAPI_policy_control_request_trigger_SUCC_RES_ALLO);
        UpdateData.rep_policy_ctrl_req_triggers = Triggers;
    }

    Ids = OpenAPI_list_create();
    ogs_assert(Ids);
    for (i = 0; i < report->num_of_rule; i++)
        OpenAPI_list_add(Ids, report->rule_id[i]);
    RuleReport.pcc_rule_ids = Ids;
    RuleReport.rule_status = report->status;
    RuleReport.failure_code = report->failure_code;
    RuleReport.alt_qos_param_id = report->alt_qos_param_id;

    RuleReports = OpenAPI_list_create();
    ogs_assert(RuleReports);
    OpenAPI_list_add(RuleReports, &RuleReport);
    UpdateData.rule_reports = RuleReports;

    message.SmPolicyUpdateContextData = &UpdateData;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

    OpenAPI_list_free(RuleReports);
    OpenAPI_list_free(Ids);
    if (Triggers)
        OpenAPI_list_free(Triggers);
    ogs_free(message.h.uri);

    for (i = 0; i < report->num_of_rule; i++)
        ogs_free(report->rule_id[i]);
    if (report->alt_qos_param_id)
        ogs_free(report->alt_qos_param_id);
    ogs_free(report);

    return request;
}

ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_delete(
        smf_sess_t *sess, void *data)
{
    smf_ue_t *smf_ue = NULL;

    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    OpenAPI_sm_policy_delete_data_t SmPolicyDeleteData;
    OpenAPI_list_t *ranNasRelCauseList = NULL;
    OpenAPI_ran_nas_rel_cause_t *ranNasRelCause = NULL;
    OpenAPI_user_location_t ueLocation;
    OpenAPI_lnode_t *node = NULL;

    ogs_assert(sess);
    ogs_assert(sess->sm_context_ref);
    smf_ue = smf_ue_find_by_id(sess->smf_ue_id);
    ogs_assert(smf_ue);
    ogs_assert(sess->policy_association.resource_uri);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.uri = ogs_msprintf("%s/%s",
            sess->policy_association.resource_uri,
            OGS_SBI_RESOURCE_NAME_DELETE);

    memset(&SmPolicyDeleteData, 0, sizeof(SmPolicyDeleteData));

    memset(&ueLocation, 0, sizeof(ueLocation));

    if (sess->nsmf_param.gmm_cause ||
        sess->nsmf_param.gsm_cause ||
        sess->nsmf_param.ngap_cause.group) {

        ranNasRelCauseList = OpenAPI_list_create();
        if (!ranNasRelCauseList) {
            ogs_error("No ranNasRelCauseList");
            goto end;
        }

        ranNasRelCause = ogs_calloc(1, sizeof(*ranNasRelCause));
        if (!ranNasRelCause) {
            ogs_error("No ranNasRelCause");
            goto end;
        }

        if (sess->nsmf_param.ngap_cause.group) {
            OpenAPI_ng_ap_cause_t *ngApCause = NULL;

            ranNasRelCause->ng_ap_cause = ngApCause =
                ogs_calloc(1, sizeof(*ngApCause));
            if (!ranNasRelCause->ng_ap_cause) {
                ogs_error("No ranNasRelCause->ng_ap_cause");
                if (ranNasRelCause)
                    ogs_free(ranNasRelCause);
                goto end;
            }

            ngApCause->group = sess->nsmf_param.ngap_cause.group;
            ngApCause->value = sess->nsmf_param.ngap_cause.value;
        }

        if (sess->nsmf_param.gmm_cause) {
            ranNasRelCause->is__5g_mm_cause = true;
            ranNasRelCause->_5g_mm_cause = sess->nsmf_param.gmm_cause;
        }
        if (sess->nsmf_param.gsm_cause) {
            ranNasRelCause->is__5g_sm_cause = true;
            ranNasRelCause->_5g_sm_cause = sess->nsmf_param.gsm_cause;
        }

        OpenAPI_list_add(ranNasRelCauseList, ranNasRelCause);
    }

    if (sess->nsmf_param.ue_location) {
        ueLocation.nr_location = ogs_sbi_build_nr_location(
                &sess->nr_tai, &sess->nr_cgi);
        if (!ueLocation.nr_location) {
            ogs_error("ueLocation.nr_location");
            goto end;
        }
        if (sess->ue_location_timestamp)
            ueLocation.nr_location->ue_location_timestamp =
                ogs_sbi_gmtime_string(sess->ue_location_timestamp);

        SmPolicyDeleteData.user_location_info = &ueLocation;
    }
    if (sess->nsmf_param.ue_timezone) {
        SmPolicyDeleteData.ue_time_zone =
            ogs_sbi_timezone_string(ogs_timezone());
        if (!SmPolicyDeleteData.ue_time_zone) {
            ogs_error("SmPolicyDeleteData.ue_time_zone");
            goto end;
        }
    }

    SmPolicyDeleteData.serving_network =
        ogs_sbi_build_plmn_id_nid(&sess->serving_plmn_id);
    if (!SmPolicyDeleteData.serving_network) {
        ogs_error("SmPolicyDeleteData.serving_network");
        goto end;
    }

    SmPolicyDeleteData.ran_nas_rel_causes = ranNasRelCauseList;

    message.SmPolicyDeleteData = &SmPolicyDeleteData;

    request = ogs_sbi_build_request(&message);
    ogs_expect(request);

end:

    if (message.h.uri)
        ogs_free(message.h.uri);

    if (ueLocation.nr_location) {
        if (ueLocation.nr_location->ue_location_timestamp)
            ogs_free(ueLocation.nr_location->ue_location_timestamp);
        ogs_sbi_free_nr_location(ueLocation.nr_location);
    }
    if (SmPolicyDeleteData.ue_time_zone)
        ogs_free(SmPolicyDeleteData.ue_time_zone);

    OpenAPI_list_for_each(ranNasRelCauseList, node) {
        ranNasRelCause = node->data;
        if (ranNasRelCause) {
            if (ranNasRelCause->ng_ap_cause) {
                ogs_free(ranNasRelCause->ng_ap_cause);
            }
            ogs_free(ranNasRelCause);
        }
    }

    OpenAPI_list_free(ranNasRelCauseList);

    if (SmPolicyDeleteData.serving_network)
        ogs_sbi_free_plmn_id_nid(SmPolicyDeleteData.serving_network);

    return request;
}
