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

#include "qos-monitoring.h"
#include "pfcp-path.h"
#include "gtp-path.h"

/*
 * TS 29.244 V19.6.0 clause 5.24.4.3: the UPF sends a downlink packet with the QMP flag and a time stamp for each requested QoS flow
 * and measures the delay from the time stamps and delays in the uplink packet of the NG-RAN. TS 23.501 V19.9.0 clause 5.33.3.2 leaves
 * the measurement frequency to the PSA UPF; the UPF probes once a second, or once per Measurement Period when that is shorter.
 */
#define UPF_QOSMON_PROBE_INTERVAL_SEC 1

static struct upf_qosmon_s *qosmon_find(upf_sess_t *sess, uint8_t srr_id)
{
    int i;

    for (i = 0; i < UPF_MAX_NUM_OF_QOSMON; i++)
        if (sess->qosmon[i].used && sess->qosmon[i].srr_id == srr_id)
            return &sess->qosmon[i];
    return NULL;
}

/* The downlink PDR of the QoS flow: where the packet with QMP goes */
static ogs_pfcp_pdr_t *qosmon_dl_pdr(upf_sess_t *sess, uint8_t qfi)
{
    ogs_pfcp_pdr_t *pdr = NULL;

    ogs_list_for_each(&sess->pfcp.pdr_list, pdr) {
        if (pdr->src_if != OGS_PFCP_INTERFACE_CORE || !pdr->far || !pdr->qer)
            continue;
        if (pdr->qer->qfi != qfi || !pdr->far->gnode)
            continue;
        if (pdr->far->dst_if != OGS_PFCP_INTERFACE_ACCESS || !(pdr->far->apply_action & OGS_PFCP_APPLY_ACTION_FORW))
            continue;
        return pdr;
    }
    return NULL;
}

static void qosmon_report(upf_sess_t *sess, struct upf_qosmon_s *mon, ogs_pfcp_srr_t *srr,
        const ogs_pfcp_qos_monitoring_measurement_t *result)
{
    ogs_pfcp_user_plane_report_t report;

    memset(&report, 0, sizeof(report));
    report.type.session_report = 1;
    report.qos_monitoring.srr_id = srr->id;
    report.qos_monitoring.qfi = srr->qos_monitoring.qfi;
    memcpy(&report.qos_monitoring.measurement, result, sizeof(*result));
    mon->last_report = ogs_time_now();
    ogs_assert(OGS_OK == upf_pfcp_send_session_report_request(sess, &report));
}

/* What was requested of a measurement */
static void qosmon_requested(const ogs_pfcp_qos_monitoring_control_t *c,
        const ogs_pfcp_qos_monitoring_measurement_t *in, ogs_pfcp_qos_monitoring_measurement_t *out)
{
    memset(out, 0, sizeof(*out));
    if (c->dl_delay && in->dl_presence) { out->dl_presence = true; out->dl = in->dl; }
    if (c->ul_delay && in->ul_presence) { out->ul_presence = true; out->ul = in->ul; }
    if (c->rt_delay && in->rt_presence) { out->rt_presence = true; out->rt = in->rt; }
    out->failure = in->failure;
}

static void qosmon_probe(upf_sess_t *sess, struct upf_qosmon_s *mon, ogs_pfcp_srr_t *srr)
{
    ogs_pfcp_pdr_t *pdr = qosmon_dl_pdr(sess, srr->qos_monitoring.qfi);
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_time_t now = ogs_time_now();

    if (!pdr) {
        ogs_debug("QoS monitoring: no downlink PDR for QFI[%d] yet", srr->qos_monitoring.qfi);
        return;
    }
    mon->t1 = now;
    mon->t1_ntp = ogs_gtpu_time_to_ntp64(now);
    pkbuf = ogs_gtpu_build_qos_monitoring_dl(pdr->far->outer_header_creation.teid, srr->qos_monitoring.qfi, mon->t1_ntp);
    ogs_pfcp_send_gtpu(pdr, pkbuf);
}

static void qosmon_timer_cb(void *data)
{
    struct upf_qosmon_s *mon = data;
    upf_sess_t *sess = NULL;
    ogs_pfcp_srr_t *srr = NULL;
    ogs_pfcp_qos_monitoring_control_t *c = NULL;
    ogs_pfcp_qos_monitoring_measurement_t out;
    ogs_time_t now = ogs_time_now();
    ogs_time_t interval = ogs_time_from_sec(UPF_QOSMON_PROBE_INTERVAL_SEC);

    ogs_assert(mon);
    sess = upf_sess_find_by_id(mon->sess_id);
    if (!sess || !mon->used)
        return;
    srr = ogs_pfcp_srr_find(&sess->pfcp, mon->srr_id);
    if (!srr || !srr->qos_monitoring_presence)
        return;
    c = &srr->qos_monitoring;
    if (c->period_presence && ogs_time_from_sec(c->period) < interval)
        interval = ogs_time_from_sec(c->period);

    /* no uplink packet with the time stamps came within the Measurement Period: a measurement failure (clause 5.24.4.3) */
    if (mon->t1 && c->period_presence && now - mon->t1 >= ogs_time_from_sec(c->period)) {
        memset(&out, 0, sizeof(out));
        out.failure = true;
        mon->t1 = 0;
        qosmon_report(sess, mon, srr, &out);
    }
    /* periodic reporting: the last result when the measurement period is over */
    if (c->periodic && c->period_presence && mon->have_result &&
            now - mon->last_periodic >= ogs_time_from_sec(c->period)) {
        qosmon_requested(c, &mon->result, &out);
        mon->last_periodic = now;
        qosmon_report(sess, mon, srr, &out);
    }
    if (!mon->t1)
        qosmon_probe(sess, mon, srr);
    ogs_timer_start(mon->timer, interval);
}

void upf_qos_monitoring_configure(upf_sess_t *sess, ogs_pfcp_srr_t *srr)
{
    struct upf_qosmon_s *mon = NULL;
    int i;

    ogs_assert(sess);
    ogs_assert(srr);
    if (!srr->qos_monitoring_presence)
        return;
    if (!(srr->qos_monitoring.dl_delay || srr->qos_monitoring.ul_delay || srr->qos_monitoring.rt_delay)) {
        ogs_warn("QoS monitoring of SRR[%d] requests no packet delay", srr->id);
        return;
    }

    mon = qosmon_find(sess, srr->id);
    if (!mon) {
        for (i = 0; i < UPF_MAX_NUM_OF_QOSMON && sess->qosmon[i].used; i++);
        if (i == UPF_MAX_NUM_OF_QOSMON) {
            ogs_error("Too many QoS monitoring rules [%d]", UPF_MAX_NUM_OF_QOSMON);
            return;
        }
        mon = &sess->qosmon[i];
        memset(mon, 0, sizeof(*mon));
        mon->used = true;
        mon->sess_id = sess->id;
        mon->srr_id = srr->id;
        mon->timer = ogs_timer_add(ogs_app()->timer_mgr, qosmon_timer_cb, mon);
        ogs_assert(mon->timer);
    }
    /* a new threshold applies to the measurements that are ongoing (clause 5.24.4.3): the state is kept */
    mon->last_periodic = ogs_time_now();
    ogs_timer_start(mon->timer, ogs_time_from_msec(1));
}

void upf_qos_monitoring_stop(upf_sess_t *sess, uint8_t srr_id)
{
    struct upf_qosmon_s *mon = qosmon_find(sess, srr_id);
    ogs_pfcp_srr_t *srr = ogs_pfcp_srr_find(&sess->pfcp, srr_id);
    ogs_pfcp_qos_monitoring_measurement_t out;

    if (!mon)
        return;
    ogs_timer_stop(mon->timer);
    /* the instruction to stop: the delays that were detected are reported (clause 5.24.4.3) */
    if (srr && srr->qos_monitoring_presence && mon->have_result) {
        qosmon_requested(&srr->qos_monitoring, &mon->result, &out);
        qosmon_report(sess, mon, srr, &out);
    }
    ogs_timer_delete(mon->timer);
    memset(mon, 0, sizeof(*mon));
}

void upf_qos_monitoring_remove_all(upf_sess_t *sess)
{
    int i;

    for (i = 0; i < UPF_MAX_NUM_OF_QOSMON; i++) {
        if (!sess->qosmon[i].used)
            continue;
        ogs_timer_delete(sess->qosmon[i].timer);
        memset(&sess->qosmon[i], 0, sizeof(sess->qosmon[i]));
    }
}

/* (T2-T1+T4-T3)/2 for each direction between the NG-RAN and the UPF, which are not assumed to be time synchronised (TS 23.501 clause 5.33.3.2) */
void upf_qos_monitoring_uplink(upf_sess_t *sess, ogs_gtp2_header_desc_t *header_desc, ogs_time_t t4)
{
    struct upf_qosmon_s *mon = NULL;
    ogs_pfcp_srr_t *srr = NULL;
    ogs_pfcp_qos_monitoring_control_t *c = NULL;
    ogs_pfcp_qos_monitoring_measurement_t result, out;
    ogs_time_t t1, t2, t3;
    int64_t transport_us;
    int i;
    bool exceeded = false;

    ogs_assert(sess);
    ogs_assert(header_desc);

    ogs_list_for_each(&sess->pfcp.srr_list, srr) {
        if (!srr->qos_monitoring_presence || srr->qos_monitoring.qfi != header_desc->qos_flow_identifier)
            continue;
        mon = qosmon_find(sess, srr->id);
        if (!mon)
            continue;
        c = &srr->qos_monitoring;

        t1 = ogs_gtpu_ntp64_to_time(header_desc->qm.dl_sending_ts);
        t2 = ogs_gtpu_ntp64_to_time(header_desc->qm.dl_received_ts);
        t3 = ogs_gtpu_ntp64_to_time(header_desc->qm.ul_sending_ts);
        transport_us = ((int64_t)(t2 - t1) + (int64_t)(t4 - t3)) / 2;
        if (transport_us < 0)
            transport_us = 0;

        memset(&result, 0, sizeof(result));
        result.dl_presence = true;
        result.dl = (header_desc->qm.dl_delay_ind ? header_desc->qm.dl_delay_result : 0) + (uint32_t)((transport_us + 500) / 1000);
        result.ul_presence = true;
        result.ul = (header_desc->qm.ul_delay_ind ? header_desc->qm.ul_delay_result : 0) + (uint32_t)((transport_us + 500) / 1000);
        result.rt_presence = true;
        result.rt = result.dl + result.ul;

        memcpy(&mon->result, &result, sizeof(result));
        mon->have_result = true;
        if (mon->t1_ntp == header_desc->qm.dl_sending_ts)
            mon->t1 = 0; /* the response to the packet that was sent: the next one may go */

        /* event triggered: a delay that matches or exceeds its threshold, and not within the Minimum Wait Time */
        if (c->event_triggered) {
            if (!(c->thr_dl_presence || c->thr_ul_presence || c->thr_rt_presence))
                exceeded = true; /* no threshold: every measurement */
            if (c->dl_delay && c->thr_dl_presence && result.dl >= c->thr_dl) exceeded = true;
            if (c->ul_delay && c->thr_ul_presence && result.ul >= c->thr_ul) exceeded = true;
            if (c->rt_delay && c->thr_rt_presence && result.rt >= c->thr_rt) exceeded = true;
            if (exceeded && (!c->min_wait_presence || !mon->last_report ||
                    ogs_time_now() - mon->last_report >= ogs_time_from_sec(c->min_wait))) {
                qosmon_requested(c, &result, &out);
                qosmon_report(sess, mon, srr, &out);
            }
        }
        (void)i;
    }
}
