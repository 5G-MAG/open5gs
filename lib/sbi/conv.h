/*
 * Copyright (C) 2019-2023 by Sukchan Lee <acetcom@gmail.com>
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

#if !defined(OGS_SBI_INSIDE) && !defined(OGS_SBI_COMPILATION)
#error "This header cannot be included directly."
#endif

#ifndef OGS_SBI_CONV_H
#define OGS_SBI_CONV_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ogs_sbi_server_s ogs_sbi_server_t;
typedef struct ogs_sbi_client_s ogs_sbi_client_t;
typedef struct ogs_sbi_header_s ogs_sbi_header_t;

char *ogs_supi_from_suci(char *suci);
char *ogs_supi_from_supi_or_suci(char *supi_or_suci);

char *ogs_uridup(
        OpenAPI_uri_scheme_e scheme,
        char *fqdn, ogs_sockaddr_t *addr, ogs_sockaddr_t *addr6, uint16_t port,
        ogs_sbi_header_t *h);
uint16_t ogs_sbi_uri_port_from_scheme_and_addr(
        OpenAPI_uri_scheme_e scheme, ogs_sockaddr_t *addr);
char *ogs_sbi_sockaddr_uri(
        OpenAPI_uri_scheme_e scheme,
        ogs_sockaddr_t *sa_list, ogs_sbi_header_t *h);
char *ogs_sbi_server_uri(ogs_sbi_server_t *server, ogs_sbi_header_t *h);
char *ogs_sbi_client_apiroot(ogs_sbi_client_t *client);
char *ogs_sbi_client_uri(ogs_sbi_client_t *client, ogs_sbi_header_t *h);

char *ogs_sbi_url_encode(const char *str);
char *ogs_sbi_url_decode(const char *str);

char *ogs_sbi_parse_uri(char *uri, const char *delim, char **saveptr);

bool ogs_sbi_getaddr_from_uri(
        OpenAPI_uri_scheme_e *scheme,
        char **fqdn, uint16_t *fqdn_port,
        ogs_sockaddr_t **addr, ogs_sockaddr_t **addr6,
        char *uri);
bool ogs_sbi_getpath_from_uri(char **path, char *uri);

char *ogs_sbi_client_resolve(
        OpenAPI_uri_scheme_e scheme,
        char *fqdn, uint16_t fqdn_port,
        const char **resolve, int num_of_resolve);

#define OGS_SBI_BITRATE_BPS     0
#define OGS_SBI_BITRATE_KBPS    1
#define OGS_SBI_BITRATE_MBPS    2
#define OGS_SBI_BITRATE_GBPS    3
#define OGS_SBI_BITRATE_TBPS    4
char *ogs_sbi_bitrate_to_string(uint64_t bitrate, int unit);
uint64_t ogs_sbi_bitrate_from_string(char *str);

int ogs_strftimezone(char *str, size_t size, int tm_gmtoff);
char *ogs_sbi_localtime_string(ogs_time_t time);
char *ogs_sbi_gmtime_string(ogs_time_t time);
char *ogs_sbi_timezone_string(int tm_gmtoff);
bool ogs_sbi_time_from_string(ogs_time_t *time, char *str);

#define OGS_SBI_RFC7231_DATE_LEN (34)
int ogs_sbi_rfc7231_string(char *date_str, ogs_time_t time);

char *ogs_sbi_s_nssai_to_json(ogs_s_nssai_t *s_nssai);
bool ogs_sbi_s_nssai_from_json(ogs_s_nssai_t *s_nssai, char *str);

char *ogs_sbi_s_nssai_to_string(ogs_s_nssai_t *s_nssai);
bool ogs_sbi_s_nssai_from_string(ogs_s_nssai_t *s_nssai, char *str);

OpenAPI_plmn_id_t *ogs_sbi_build_plmn_id(ogs_plmn_id_t *plmn_id);
bool ogs_sbi_parse_plmn_id(
        ogs_plmn_id_t *plmn_id, OpenAPI_plmn_id_t *PlmnId);
void ogs_sbi_free_plmn_id(OpenAPI_plmn_id_t *PlmnId);

OpenAPI_list_t *ogs_sbi_build_plmn_list(
        ogs_plmn_id_t *plmn_list, int num_of_plmn_list);
int ogs_sbi_parse_plmn_list(
        ogs_plmn_id_t *plmn_list, OpenAPI_list_t *PlmnList);
void ogs_sbi_free_plmn_list(OpenAPI_list_t *PlmnList);
bool ogs_sbi_compare_plmn_list(
        ogs_plmn_id_t *plmn_id, OpenAPI_plmn_id_t *PlmnId);

OpenAPI_plmn_id_nid_t *ogs_sbi_build_plmn_id_nid(ogs_plmn_id_t *plmn_id);
bool ogs_sbi_parse_plmn_id_nid(
        ogs_plmn_id_t *plmn_id, OpenAPI_plmn_id_nid_t *PlmnIdNid);
void ogs_sbi_free_plmn_id_nid(OpenAPI_plmn_id_nid_t *PlmnIdNid);

OpenAPI_guami_t *ogs_sbi_build_guami(ogs_guami_t *guami);
bool ogs_sbi_parse_guami(ogs_guami_t *guami, OpenAPI_guami_t *Guami);
void ogs_sbi_free_guami(OpenAPI_guami_t *Guami);

OpenAPI_nr_location_t *ogs_sbi_build_nr_location(
    ogs_5gs_tai_t *tai, ogs_nr_cgi_t *nr_cgi);
bool ogs_sbi_parse_nr_location(ogs_5gs_tai_t *tai, ogs_nr_cgi_t *nr_cgi,
        OpenAPI_nr_location_t *NrLocation);
void ogs_sbi_free_nr_location(OpenAPI_nr_location_t *NrLocation);

OpenAPI_pcc_rule_t *ogs_sbi_build_pcc_rule(
        ogs_pcc_rule_t *pcc_rule, int flow_presence);
void ogs_sbi_free_pcc_rule(OpenAPI_pcc_rule_t *PccRule);
OpenAPI_qos_data_t *ogs_sbi_build_qos_data(ogs_pcc_rule_t *pcc_rule);
/* The QosData of the alternative QoS parameter set number index of the rule (TS 29.512 V19.7.0 clause 4.2.6.2.1) */
OpenAPI_qos_data_t *ogs_sbi_build_alt_qos_data(ogs_pcc_rule_t *pcc_rule, int index);

/*
 * The flow status of a PCC rule (the values of the Flow-Status AVP, 3GPP TS 29.214: 0 enabled uplink, 1 enabled downlink,
 * 2 enabled, 3 disabled, 4 removed) as the flowStatus of TrafficControlData (TS 29.512 clause 5.6.2.10) and back.
 */
OpenAPI_flow_status_e ogs_sbi_flow_status_from_pcc_rule(int flow_status);
int ogs_sbi_flow_status_to_pcc_rule(OpenAPI_flow_status_e flow_status);

/*
 * Give the SM policy decision the TrafficControlData of a PCC rule (its tcId is the PCC rule id) and make the rule refer to it.
 * The data belong to the decision until ogs_sbi_decision_free_traffic_control().
 */
void ogs_sbi_decision_add_traffic_control(
        OpenAPI_sm_policy_decision_t *decision, OpenAPI_pcc_rule_t *pcc_rule_model, ogs_pcc_rule_t *pcc_rule);
void ogs_sbi_decision_free_traffic_control(OpenAPI_sm_policy_decision_t *decision);

/*
 * Usage monitoring (TS 29.512 V19.7.0 clause 4.2.4.10) and sponsored connectivity (TS 29.514 clause 4.2.2.5) in a decision: the
 * UsageMonitoringData of a monitoring key with the thresholds (a threshold of 0 is left out) and the ChargingData with the sponsor,
 * both referred to by the PCC rule. Release them with ogs_sbi_decision_free_traffic_control(), which frees them as well.
 */
void ogs_sbi_decision_add_usage_monitoring(
        OpenAPI_sm_policy_decision_t *decision, OpenAPI_pcc_rule_t *pcc_rule_model, const char *um_id,
        long volume, long volume_uplink, long volume_downlink, int time);
/* The entry of the monitoring key with a null value ends the monitoring */
void ogs_sbi_decision_end_usage_monitoring(
        OpenAPI_sm_policy_decision_t *decision, const char *um_id);
void ogs_sbi_decision_add_sponsor(
        OpenAPI_sm_policy_decision_t *decision, OpenAPI_pcc_rule_t *pcc_rule_model, const char *chg_id,
        const char *sponsor_id, const char *asp_id);
/*
 * QoS monitoring of the packet delay (TS 29.512 V19.7.0 clause 4.2.3.25): the QosMonitoringData with the requested parameters, the
 * reporting frequencies and the thresholds (-1 leaves one out), referred to by the PCC rule. ogs_sbi_decision_free_traffic_control()
 * frees it.
 */
void ogs_sbi_decision_add_qos_monitoring(
        OpenAPI_sm_policy_decision_t *decision, OpenAPI_pcc_rule_t *pcc_rule_model, const char *qm_id,
        bool dl, bool ul, bool rt, bool event_triggered, bool periodic,
        int thr_dl, int thr_ul, int thr_rt, int wait_time, int rep_period);
/* Add QOS_MONITORING to the policy control request triggers of the decision */
void ogs_sbi_decision_add_l4s_trigger(OpenAPI_sm_policy_decision_t *decision);
void ogs_sbi_decision_add_qos_monitoring_trigger(OpenAPI_sm_policy_decision_t *decision);
/* Add US_RE to the policy control request triggers of the decision */
void ogs_sbi_decision_add_us_re_trigger(OpenAPI_sm_policy_decision_t *decision);
/* Adds the QosData of the rule to the list, and the QosData of its alternative QoS parameter sets */
void ogs_sbi_decision_add_qos_data(OpenAPI_list_t *QosDecisionList, ogs_pcc_rule_t *pcc_rule);
void ogs_sbi_free_qos_data(OpenAPI_qos_data_t *QosData);

bool ogs_sbi_parse_tmgi(ogs_tmgi_t *tmgi, OpenAPI_tmgi_t *Tmgi);
OpenAPI_tmgi_t *ogs_sbi_build_tmgi(ogs_tmgi_t *tmgi);

bool ogs_sbi_parse_ssm(ogs_ssm_t *ssm, OpenAPI_ssm_t *Ssm);
OpenAPI_ssm_t *ogs_sbi_build_ssm(ogs_ssm_t *ssm);

OpenAPI_mbs_service_area_t *ogs_sbi_build_mbs_service_area(ogs_mbs_service_area_t *area);

#ifdef __cplusplus
}
#endif

#endif /* OGS_SBI_CONV_H */
