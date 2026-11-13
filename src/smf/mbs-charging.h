/*
 * 5G-MAG Reference Tools
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

#ifndef SMF_MBS_CHARGING_H
#define SMF_MBS_CHARGING_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MBS charging, TS 29.244 V18.10.0 clause 5.34.2.5: "MBS charging is optional feature for the MB-SMF and the MB-UPF. ...
 * the MB-SMF may request the MB-UPF to measure and report the volume of MBS data transmitted for the entire MBS session
 * (or location dependent part of the MBS session, for a location dependent MBS service) ... If so, the MB-SMF shall
 * provision URR(s) for the related PFCP session in the MB-UPF ... no charging quotas shall be used ... Only
 * volume-based measurement shall be used for MBS data usage reporting over N4mb (i.e. no time nor event based
 * measurement)." */

/* The configuration smf.mbsCharging: \p enabled, \p volume_threshold (octets) and \p period (seconds) are the texts of
 * its keys (NULL when absent). Returns false, changing nothing, when they are not acceptable: enabled is "true" or
 * "false"; with true both values are positive numbers: no clause gives a default (rule 12). */
bool smf_mbs_charging_configure(const char *enabled, const char *volume_threshold, const char *period);

/* Whether the MB-SMF requests MBS charging; the volume threshold and the period of the URR are then the operator's */
bool smf_mbs_charging_enabled(uint64_t *volume_threshold, uint32_t *period);

/* The URR of the MBS session for its charging, created with the session's PFCP rules the first time: measurement
 * method volume, reporting triggers PERIO (periodic reporting) and VOLTH (volume threshold), the operator's values.
 * NULL when charging is off, or when the MB-UPF of the session does not set MBSCH ("The MB-UPF shall set the MBSCH
 * feature flag in the UP Function Features IE (see clause 8.2.25) if it supports MBS charging"): then it is logged
 * that charging was requested and the MB-UPF cannot do it. */
ogs_pfcp_urr_t *smf_mbs_sess_charging_urr(smf_mbs_sess_t *mbs_sess);

/* Whether the session has a URR at its MB-UPF (provisioned with the establishment, at an MB-UPF that sets MBSCH) */
bool smf_mbs_sess_has_charging_urr(smf_mbs_sess_t *mbs_sess);

/* A Usage Report IE of the MB-UPF, from a Session Report Request (Report Type USAR, table 7.5.8.3-1), a Session Deletion
 * Response (table 7.5.7.2-1, trigger TERMR) or a Session Modification Response (table 7.5.5.2-1, trigger IMMER of a Query
 * URR): "URR ID | M", "UR-SEQN | M", "Usage Report Trigger | M", "Start Time | C", "End Time | C" and "Volume Measurement |
 * C | ... if a volume measurement needs to be reported". The report is added to the session's usage and logged. Returns
 * the PFCP cause: 66 with *offending_ie when a mandatory IE is missing, 69 for a Volume Measurement that cannot be
 * read, else 1. */
uint8_t smf_mbs_sess_take_usage_report(smf_mbs_sess_t *mbs_sess, const char *source, const ogs_pfcp_tlv_urr_id_t *urr_id,
        const ogs_pfcp_tlv_ur_seqn_t *seqn, const ogs_pfcp_tlv_usage_report_trigger_t *trigger,
        const ogs_pfcp_tlv_start_time_t *start, const ogs_pfcp_tlv_end_time_t *end,
        const ogs_pfcp_tlv_volume_measurement_t *volume, uint16_t *offending_ie);

/* The usage reports of a message: the Usage Report IEs of the Session Report Request, Session Deletion Response and
 * Session Modification Response. Return the cause as smf_mbs_sess_take_usage_report() does for the first report that
 * is not acceptable (the reports before it are taken in). */
uint8_t smf_mbs_sess_take_usage_reports_of_report_request(smf_mbs_sess_t *mbs_sess,
        const ogs_pfcp_session_report_request_t *req, uint16_t *offending_ie);
uint8_t smf_mbs_sess_take_usage_reports_of_deletion_response(smf_mbs_sess_t *mbs_sess,
        const ogs_pfcp_session_deletion_response_t *rsp, uint16_t *offending_ie);
uint8_t smf_mbs_sess_take_usage_reports_of_modification_response(smf_mbs_sess_t *mbs_sess,
        const ogs_pfcp_session_modification_response_t *rsp, uint16_t *offending_ie);

#ifdef __cplusplus
}
#endif

#endif /* SMF_MBS_CHARGING_H */
