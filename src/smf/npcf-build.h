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

#ifndef SMF_NPCF_BUILD_H
#define SMF_NPCF_BUILD_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_create(
        smf_sess_t *sess, void *data);
/* The outcome of the resource allocation of PCC rules (TS 29.512 V19.7.0 clauses 4.2.4.14 and 4.2.4.15); the builder frees it */
typedef struct smf_npcf_smpolicycontrol_rule_report_s {
    OpenAPI_rule_status_e status;
    OpenAPI_failure_code_e failure_code;
    int num_of_rule;
    char *rule_id[OGS_MAX_NUM_OF_PCC_RULE];
} smf_npcf_smpolicycontrol_rule_report_t;

/* An accumulated usage report of a usage monitoring control instance (TS 29.512 V19.7.0 clause 4.2.4.10) */
typedef struct smf_npcf_smpolicycontrol_usage_report_s {
    char *um_id;
    bool trigger; /* US_RE: the thresholds were reached or the last rule is gone; false when the PCF asked for the usage */
    uint64_t total;
    uint64_t uplink;
    uint64_t downlink;
    uint32_t time;
} smf_npcf_smpolicycontrol_usage_report_t;

/* QoS notification control (TS 29.512 V19.7.0 clause 4.2.4.20): the GBR QoS targets of PCC rules are not guaranteed, or guaranteed again */
typedef struct smf_npcf_smpolicycontrol_qnc_report_s {
    OpenAPI_qos_notif_type_e notif_type;
    int num_of_rule;
    char *rule_id[OGS_MAX_NUM_OF_PCC_RULE];
} smf_npcf_smpolicycontrol_qnc_report_t;

/* A QoS monitoring report of the packet delay of the flow of a PCC rule (TS 29.512 V19.7.0 clause 4.2.4.24); the builder frees it */
typedef struct smf_npcf_smpolicycontrol_qos_mon_report_s {
    char *rule_id;
    ogs_pfcp_qos_monitoring_measurement_t measurement;
} smf_npcf_smpolicycontrol_qos_mon_report_t;

ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_qos_mon_update(
        smf_sess_t *sess, void *data);
ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_qnc_update(
        smf_sess_t *sess, void *data);
ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_usage_update(
        smf_sess_t *sess, void *data);
ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_update(
        smf_sess_t *sess, void *data);
ogs_sbi_request_t *smf_npcf_smpolicycontrol_build_delete(
        smf_sess_t *sess, void *data);

#ifdef __cplusplus
}
#endif

#endif /* SMF_NPCF_BUILD_H */
