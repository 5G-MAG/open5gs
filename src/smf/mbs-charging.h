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

#ifdef __cplusplus
}
#endif

#endif /* SMF_MBS_CHARGING_H */
