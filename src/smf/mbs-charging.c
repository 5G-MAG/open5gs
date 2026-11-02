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

#include "context.h"
#include "mbs-charging.h"

static bool charging_on;
static uint64_t charging_volume;
static uint32_t charging_period;

static bool parse_positive(const char *text, uint64_t max, uint64_t *out)
{
    char *end = NULL;
    unsigned long long v;

    if (!text || !*text || *text == '-')
        return false;
    v = strtoull(text, &end, 10);
    if (*end || v == 0 || v > max)
        return false;
    *out = v;
    return true;
}

bool smf_mbs_charging_configure(const char *enabled, const char *volume_threshold, const char *period)
{
    bool on = false;
    uint64_t volume = 0, seconds = 0;

    if (enabled) {
        if (!strcmp(enabled, "true"))
            on = true;
        else if (strcmp(enabled, "false"))
            return false;
    }
    if (on) {
        /* the threshold is the Total Volume of a Volume Threshold IE (8 octets), the period a Measurement Period (4) */
        if (!parse_positive(volume_threshold, UINT64_MAX, &volume) ||
                !parse_positive(period, UINT32_MAX, &seconds))
            return false;
    }

    charging_on = on;
    charging_volume = volume;
    charging_period = (uint32_t)seconds;
    return true;
}

bool smf_mbs_charging_enabled(uint64_t *volume_threshold, uint32_t *period)
{
    if (!charging_on)
        return false;
    if (volume_threshold)
        *volume_threshold = charging_volume;
    if (period)
        *period = charging_period;
    return true;
}

ogs_pfcp_urr_t *smf_mbs_sess_charging_urr(smf_mbs_sess_t *mbs_sess)
{
    ogs_pfcp_urr_t *urr;

    ogs_assert(mbs_sess);

    if (!charging_on)
        return NULL;
    if (!mbs_sess->pfcp_node || !mbs_sess->pfcp_node->up_function_features.mbsch) {
        ogs_warn("MBS Session mbsSessionRef[%s]: MBS charging is requested, but the MB-UPF does not set MBSCH; "
                "no URR is provisioned", mbs_sess->mbs_session_ref);
        return NULL;
    }

    urr = ogs_list_first(&mbs_sess->pfcp.urr_list);
    if (urr)
        return urr;

    urr = ogs_pfcp_urr_add(&mbs_sess->pfcp);
    if (!urr) {
        ogs_error("MBS Session mbsSessionRef[%s]: no URR for the charging", mbs_sess->mbs_session_ref);
        return NULL;
    }
    urr->meas_method = OGS_PFCP_MEASUREMENT_METHOD_VOLUME;
    urr->rep_triggers.periodic_reporting = 1;
    urr->rep_triggers.volume_threshold = 1;
    urr->meas_period = charging_period;
    urr->vol_threshold.tovol = 1;
    urr->vol_threshold.total_volume = charging_volume;
    return urr;
}
