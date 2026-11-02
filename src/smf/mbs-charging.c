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

uint8_t smf_mbs_sess_take_usage_report(smf_mbs_sess_t *mbs_sess, const char *source, const ogs_pfcp_tlv_urr_id_t *urr_id,
        const ogs_pfcp_tlv_ur_seqn_t *seqn, const ogs_pfcp_tlv_usage_report_trigger_t *trigger,
        const ogs_pfcp_tlv_start_time_t *start, const ogs_pfcp_tlv_end_time_t *end,
        const ogs_pfcp_tlv_volume_measurement_t *volume, uint16_t *offending_ie)
{
    ogs_pfcp_volume_measurement_t vm;
    ogs_pfcp_urr_t *urr;
    uint8_t octets[3] = { 0, 0, 0 };
    bool has_volume = false;

    ogs_assert(mbs_sess);
    ogs_assert(urr_id && seqn && trigger && start && end && volume);
    ogs_assert(offending_ie);

    *offending_ie = 0;
    if (!urr_id->presence) {
        *offending_ie = OGS_PFCP_URR_ID_TYPE;
        return OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    }
    if (!seqn->presence) {
        *offending_ie = OGS_PFCP_UR_SEQN_TYPE;
        return OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    }
    if (!trigger->presence) {
        *offending_ie = OGS_PFCP_USAGE_REPORT_TRIGGER_TYPE;
        return OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    }

    memset(&vm, 0, sizeof(vm));
    if (volume->presence) {
        /* TS 29.244 V18.10.0 clause 8.2.41: the flags octet (TONOP, ULNOP, DLNOP, TOVOL, ULVOL, DLVOL, the three counts of
         * packets above them) and an 8-octet counter for each flag set. ogs_pfcp_parse_volume_measurement() asserts on
         * another length: a report from a peer is checked first. */
        const uint8_t *d = volume->data;
        int expected = 1, flags;

        if (d && volume->len >= 1) {
            for (flags = d[0] & 0x3f; flags; flags >>= 1)
                expected += (flags & 1) ? 8 : 0;
        }
        if (!d || volume->len < 1 || volume->len != expected ||
                ogs_pfcp_parse_volume_measurement(&vm, (ogs_tlv_octet_t *)volume) < 0) {
            ogs_error("MBS Session mbsSessionRef[%s]: the Volume Measurement of a usage report cannot be read",
                    mbs_sess->mbs_session_ref);
            *offending_ie = OGS_PFCP_VOLUME_MEASUREMENT_TYPE;
            return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
        }
        has_volume = true;
    }

    /* a report of a URR this MB-SMF did not provision is not one of the session's usage */
    urr = ogs_list_first(&mbs_sess->pfcp.urr_list);
    if (!urr || urr->id != urr_id->u32) {
        ogs_warn("MBS Session mbsSessionRef[%s]: usage report [%s] of URR [%u], which is not a URR of the session",
                mbs_sess->mbs_session_ref, source, urr_id->u32);
        return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
    }

    octets[0] = (trigger->u24 >> 16) & 0xff;
    octets[1] = (trigger->u24 >> 8) & 0xff;
    octets[2] = trigger->u24 & 0xff;
    mbs_sess->usage_reports++;
    if (has_volume && vm.tovol)
        mbs_sess->usage_total_volume += vm.total_volume;
    if (has_volume && vm.dlvol)
        mbs_sess->usage_downlink_volume += vm.downlink_volume;
    ogs_info("MBS Session mbsSessionRef[%s]: usage report [%s] URR [%u] UR-SEQN [%u] trigger [%02x %02x %02x] "
            "start [%u] end [%u] total [%" PRIu64 "] downlink [%" PRIu64 "] octets (session so far: [%" PRIu64
            "] total, [%" PRIu64 "] downlink in [%u] reports)", mbs_sess->mbs_session_ref, source, urr_id->u32,
            seqn->u32, octets[0], octets[1], octets[2], start->presence ? start->u32 : 0, end->presence ? end->u32 : 0,
            (has_volume && vm.tovol) ? vm.total_volume : 0, (has_volume && vm.dlvol) ? vm.downlink_volume : 0,
            mbs_sess->usage_total_volume, mbs_sess->usage_downlink_volume, mbs_sess->usage_reports);
    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}

#define TAKE_REPORTS(mbs_sess, source, array, offending_ie) do { \
    int i_; \
    for (i_ = 0; i_ < (int)OGS_ARRAY_SIZE(array) && (array)[i_].presence; i_++) { \
        uint8_t cause_ = smf_mbs_sess_take_usage_report((mbs_sess), (source), &(array)[i_].urr_id, \
                &(array)[i_].ur_seqn, &(array)[i_].usage_report_trigger, &(array)[i_].start_time, \
                &(array)[i_].end_time, &(array)[i_].volume_measurement, (offending_ie)); \
        if (cause_ != OGS_PFCP_CAUSE_REQUEST_ACCEPTED) \
            return cause_; \
    } \
} while (0)

uint8_t smf_mbs_sess_take_usage_reports_of_report_request(smf_mbs_sess_t *mbs_sess,
        const ogs_pfcp_session_report_request_t *req, uint16_t *offending_ie)
{
    *offending_ie = 0;
    TAKE_REPORTS(mbs_sess, "Session Report Request", req->usage_report, offending_ie);
    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}

uint8_t smf_mbs_sess_take_usage_reports_of_deletion_response(smf_mbs_sess_t *mbs_sess,
        const ogs_pfcp_session_deletion_response_t *rsp, uint16_t *offending_ie)
{
    *offending_ie = 0;
    TAKE_REPORTS(mbs_sess, "Session Deletion Response", rsp->usage_report, offending_ie);
    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}

uint8_t smf_mbs_sess_take_usage_reports_of_modification_response(smf_mbs_sess_t *mbs_sess,
        const ogs_pfcp_session_modification_response_t *rsp, uint16_t *offending_ie)
{
    *offending_ie = 0;
    TAKE_REPORTS(mbs_sess, "Session Modification Response", rsp->usage_report, offending_ie);
    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}
