#include "context.h"
#include "event.h"
#include "timer.h"
#include "pfcp-path.h"
#include "mbs-charging.h"

/*
 * TS 29.244 cl. 5.34.2.5: "MBS charging is an optional feature for the MB-SMF and the MB-UPF. ... the MB-SMF may request the MB-UPF to
 * measure and report the volume of MBS data transmitted for the entire MBS session (or location dependent part of the MBS session, for a
 * location dependent MBS service), for a broadcast or a multicast MBS session ... If so, the MB-SMF shall provision URR(s) for the related
 * PFCP session in the MB-UPF as defined in clause 5.4.10 with the following modifications: no charging quotas shall be used for MBS charging
 * over N4mb, i.e. Volume (Time/Event) Quota shall not be provisioned in a URR for MBS charging; and Only volume-based measurement shall be used
 * for MBS data usage reporting over N4mb (i.e. no time nor event based measurement)."
 * NOTE: "The measured volume corresponds to the volume of MBS data received from the AF or MBSTF that is transmitted downstreams towards at
 * least one NG-RAN or UPF. MBS data is counted only once when it is transmitted towards multiple NG-RANs and/or UPFs."
 * A URR is therefore one of the session, not of a PDR (the URR ID of the Create PDR is "-" for N4mb), and the bytes are the length of the
 * packet that the data path sent to at least one destination.
 */

/* the Linked URR ID IE: four octets, the URR ID (cl. 8.2.82) */
static bool linked_urr_id(const ogs_pfcp_tlv_linked_urr_id_t *ie_, ogs_pfcp_urr_id_t *id)
{
    uint32_t v;

    if (!ie_->presence || ie_->len != sizeof(v))
        return false;
    memcpy(&v, ie_->data, sizeof(v));
    *id = be32toh(v);
    return true;
}

static upf_mbs_urr_acc_t *acc_find(upf_mbs_sess_t *mbs_sess, ogs_pfcp_urr_id_t id)
{
    int i;

    for (i = 0; i < OGS_MAX_NUM_OF_URR; i++)
        if (mbs_sess->urr_acc[i].used && mbs_sess->urr_acc[i].id == id)
            return &mbs_sess->urr_acc[i];
    return NULL;
}

bool upf_mbs_far_has_destination(const ogs_pfcp_far_t *far)
{
    ogs_pfcp_mbs_unicast_t *mu;

    if (!(far->apply_action & (OGS_PFCP_APPLY_ACTION_FSSM | OGS_PFCP_APPLY_ACTION_MBSU)))
        return false;
    if ((far->apply_action & OGS_PFCP_APPLY_ACTION_FSSM) && far->gnode && far->dst_if != OGS_PFCP_INTERFACE_UNKNOWN)
        return true;
    if (far->apply_action & OGS_PFCP_APPLY_ACTION_MBSU)
        ogs_list_for_each(&((ogs_pfcp_far_t *)far)->mbs_unicast_list, mu)
            if (mu->gnode)
                return true;
    return false;
}

/*
 * Create URR on N4mb (Table 7.5.2.4-1): URR ID (M), Measurement Method (M), Reporting Triggers (M), Measurement Period, Volume Threshold and Linked
 * URR ID (C); the other IEs are not applicable ("-") and are not read. Measurement Method: volume only (cl. 5.34.2.5). Reporting Triggers: the
 * triggers of a volume measurement that the N4mb IEs support, periodic reporting (Measurement Period), volume threshold (Volume Threshold) and linked
 * usage reporting (Linked URR ID); any other is "Service not supported". Returns the cause, 0x01 if the IEs are valid.
 */
uint8_t upf_mbs_urr_check_create(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_create_urr_t *create, int n,
        const ogs_pfcp_tlv_remove_urr_t *remove, int num_remove, uint16_t *off)
{
    int i, j, used = 0;

#define FAIL(c, t) do { *off = (t); return (c); } while (0)

    for (i = 0; i < OGS_MAX_NUM_OF_URR; i++)
        used += mbs_sess->urr_acc[i].used;

    for (i = 0; i < n; i++) {
        const ogs_pfcp_tlv_create_urr_t *c = &create[i];
        ogs_pfcp_reporting_triggers_t trig;
        ogs_pfcp_volume_threshold_t vt;
        bool exists_after;

        if (!c->presence)
            continue;
        if (!c->urr_id.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_URR_ID_TYPE);
        if (!c->measurement_method.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_MEASUREMENT_METHOD_TYPE);
        if (!c->reporting_triggers.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_REPORTING_TRIGGERS_TYPE);
        /* the URR ID is new: a URR of the session stays, unless this request removes it */
        exists_after = acc_find(mbs_sess, c->urr_id.u32) != NULL;
        for (j = 0; j < num_remove; j++)
            if (remove[j].presence && remove[j].urr_id.presence && remove[j].urr_id.u32 == c->urr_id.u32)
                exists_after = false;
        if (exists_after)
            FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_URR_ID_TYPE);
        for (j = 0; j < i; j++)
            if (create[j].presence && create[j].urr_id.presence && create[j].urr_id.u32 == c->urr_id.u32)
                FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_URR_ID_TYPE);

        if (c->measurement_method.u8 != OGS_PFCP_MEASUREMENT_METHOD_VOLUME)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_MEASUREMENT_METHOD_TYPE);

        memset(&trig, 0, sizeof(trig));
        trig.reptri_5 = (c->reporting_triggers.u24 >> 16) & 0xff;
        trig.reptri_6 = (c->reporting_triggers.u24 >> 8) & 0xff;
        trig.reptri_7 = c->reporting_triggers.u24 & 0xff;
        {
            ogs_pfcp_reporting_triggers_t supported;

            memset(&supported, 0, sizeof(supported));
            supported.periodic_reporting = 1;
            supported.volume_threshold = 1;
            supported.linked_usage_reporting = 1;
            if ((trig.reptri_5 & ~supported.reptri_5) || trig.reptri_6 || trig.reptri_7)
                FAIL(OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED, OGS_PFCP_REPORTING_TRIGGERS_TYPE);
        }
        if (trig.periodic_reporting && (!c->measurement_period.presence || c->measurement_period.u32 == 0))
            FAIL(OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING, OGS_PFCP_MEASUREMENT_PERIOD_TYPE);
        if (trig.volume_threshold) {
            if (!c->volume_threshold.presence)
                FAIL(OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING, OGS_PFCP_VOLUME_THRESHOLD_TYPE);
            memset(&vt, 0, sizeof(vt));
            if (ogs_pfcp_parse_volume(&vt, (ogs_tlv_octet_t *)&c->volume_threshold) < 0 || (!vt.tovol && !vt.dlvol))
                FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_VOLUME_THRESHOLD_TYPE);
        }
        if (trig.linked_usage_reporting) {
            ogs_pfcp_urr_id_t lid;

            if (!c->linked_urr_id.presence)
                FAIL(OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING, OGS_PFCP_LINKED_URR_ID_TYPE);
            if (!linked_urr_id(&c->linked_urr_id, &lid))
                FAIL(OGS_PFCP_CAUSE_INVALID_LENGTH, OGS_PFCP_LINKED_URR_ID_TYPE);
        }
        used++;
        if (used > OGS_MAX_NUM_OF_URR)
            FAIL(OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE, OGS_PFCP_URR_ID_TYPE);
    }

#undef FAIL
    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}

/* Remove URR and Query URR name a URR of the session (URR ID, M) */
uint8_t upf_mbs_urr_check_remove_query(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_remove_urr_t *remove, int n_remove,
        const ogs_pfcp_tlv_query_urr_t *query, int n_query, uint16_t *off)
{
    int i, j;

    for (i = 0; i < n_remove; i++) {
        if (!remove[i].presence)
            continue;
        if (!remove[i].urr_id.presence) {
            *off = OGS_PFCP_URR_ID_TYPE;
            return OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        }
        if (!acc_find(mbs_sess, remove[i].urr_id.u32)) {
            *off = OGS_PFCP_URR_ID_TYPE;
            return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
        }
        for (j = 0; j < i; j++)
            if (remove[j].presence && remove[j].urr_id.presence && remove[j].urr_id.u32 == remove[i].urr_id.u32) {
                *off = OGS_PFCP_URR_ID_TYPE;
                return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
            }
    }
    for (i = 0; i < n_query; i++) {
        if (!query[i].presence)
            continue;
        if (!query[i].urr_id.presence) {
            *off = OGS_PFCP_URR_ID_TYPE;
            return OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        }
        if (!acc_find(mbs_sess, query[i].urr_id.u32)) {
            *off = OGS_PFCP_URR_ID_TYPE;
            return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
        }
    }
    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}

static uint64_t load(const uint64_t *p)
{
    return __atomic_load_n(p, __ATOMIC_RELAXED);
}

static void period_arm(upf_mbs_urr_acc_t *acc)
{
    if (acc->triggers.periodic_reporting && acc->period && acc->t_period)
        ogs_timer_start(acc->t_period, ogs_time_from_sec(acc->period));
}

void upf_mbs_urr_create(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_create_urr_t *create, int n)
{
    int i, k;

    for (i = 0; i < n; i++) {
        const ogs_pfcp_tlv_create_urr_t *c = &create[i];
        upf_mbs_urr_acc_t *acc = NULL;
        ogs_pfcp_volume_threshold_t vt;

        if (!c->presence)
            continue;
        for (k = 0; k < OGS_MAX_NUM_OF_URR; k++)
            if (!mbs_sess->urr_acc[k].used) {
                acc = &mbs_sess->urr_acc[k];
                break;
            }
        ogs_assert(acc);    /* checked */
        memset(acc, 0, sizeof(*acc));
        acc->used = true;
        acc->seid = mbs_sess->upf_n4mb_seid;
        acc->id = c->urr_id.u32;
        acc->triggers.reptri_5 = (c->reporting_triggers.u24 >> 16) & 0xff;
        acc->triggers.reptri_6 = (c->reporting_triggers.u24 >> 8) & 0xff;
        acc->triggers.reptri_7 = c->reporting_triggers.u24 & 0xff;
        if (acc->triggers.periodic_reporting)
            acc->period = c->measurement_period.u32;
        if (acc->triggers.volume_threshold) {
            memset(&vt, 0, sizeof(vt));
            ogs_pfcp_parse_volume(&vt, (ogs_tlv_octet_t *)&c->volume_threshold);
            acc->thr_total = vt.tovol;
            acc->thr_downlink = vt.dlvol;
            acc->thr_total_volume = vt.total_volume;
            acc->thr_downlink_volume = vt.downlink_volume;
        }
        if (acc->triggers.linked_usage_reporting) {
            acc->linked = linked_urr_id(&c->linked_urr_id, &acc->linked_id);
        }
        acc->period_start = ogs_time_now();
        acc->t_period = ogs_timer_add(ogs_app()->timer_mgr, upf_timer_mbs_period, acc);
        ogs_assert(acc->t_period);
        period_arm(acc);
    }
}

static void free_acc(upf_mbs_urr_acc_t *acc)
{
    if (acc->t_period)
        ogs_timer_delete(acc->t_period);
    memset(acc, 0, sizeof(*acc));
}

void upf_mbs_urr_free_all(upf_mbs_sess_t *mbs_sess)
{
    int i;

    for (i = 0; i < OGS_MAX_NUM_OF_URR; i++)
        if (mbs_sess->urr_acc[i].used)
            free_acc(&mbs_sess->urr_acc[i]);
}

static bool acc_measured_nothing(const upf_mbs_urr_acc_t *acc)
{
    return load(&acc->total_octets) == acc->rep_total_octets && load(&acc->dl_octets) == acc->rep_dl_octets;
}

/*
 * The Usage Report of a URR (cl. 5.2.2.3.1): the UR-SEQN "set to "0" for the first Usage Report and incremented for every subsequent Usage
 * Report", the trigger, the measurement since the last Usage Report: "the UP function shall include all the counters (Total, Uplink and
 * Downlink) of the URR in the Volume Measurement IE" (the data is downlink only), the start and the end of the measurement and the time of the
 * first and the last packet. The counts are then reset; the threshold is re-applied if the threshold caused the report and is adjusted by the
 * reported usage otherwise.
 */
static void fill_report(upf_mbs_urr_acc_t *acc, ogs_pfcp_user_plane_report_t *report, unsigned int idx,
        const ogs_pfcp_usage_report_trigger_t *trigger)
{
    uint64_t total = load(&acc->total_octets), dl = load(&acc->dl_octets);
    ogs_time_t now = ogs_time_now();

    report->type.usage_report = 1;
    report->usage_report[idx].id = acc->id;
    report->usage_report[idx].seqn = acc->seqn++;
    report->usage_report[idx].rep_trigger = *trigger;
    report->usage_report[idx].start_time = ogs_time_to_ntp32(acc->period_start);
    report->usage_report[idx].end_time = ogs_time_to_ntp32(now);
    report->usage_report[idx].vol_measurement.tovol = 1;
    report->usage_report[idx].vol_measurement.ulvol = 1;
    report->usage_report[idx].vol_measurement.dlvol = 1;
    report->usage_report[idx].vol_measurement.total_volume = total - acc->rep_total_octets;
    report->usage_report[idx].vol_measurement.uplink_volume = 0;
    report->usage_report[idx].vol_measurement.downlink_volume = dl - acc->rep_dl_octets;
    if (acc->time_of_first_packet)
        report->usage_report[idx].time_of_first_packet = ogs_time_to_ntp32(acc->time_of_first_packet);
    if (acc->time_of_last_packet)
        report->usage_report[idx].time_of_last_packet = ogs_time_to_ntp32(acc->time_of_last_packet);

    acc->rep_total_octets = total;
    acc->rep_dl_octets = dl;
    acc->period_start = now;
    acc->time_of_first_packet = 0;
    acc->time_of_last_packet = 0;
    if (trigger->volume_threshold) {
        acc->thr_total_octets = total;
        acc->thr_dl_octets = dl;
    }
}

/* The URRs that report with the URR "Y" (cl. 5.2.2.4): those with the trigger LIUSA whose Linked URR ID is Y */
static void add_linked(upf_mbs_sess_t *mbs_sess, ogs_pfcp_urr_id_t y, bool non_null_only, ogs_pfcp_user_plane_report_t *report,
        unsigned int *num)
{
    int i;
    unsigned int k;

    for (i = 0; i < OGS_MAX_NUM_OF_URR; i++) {
        upf_mbs_urr_acc_t *x = &mbs_sess->urr_acc[i];
        ogs_pfcp_usage_report_trigger_t t;

        if (!x->used || !x->linked || x->linked_id != y)
            continue;
        for (k = 0; k < *num; k++)
            if (report->usage_report[k].id == x->id)
                break;
        if (k < *num || (non_null_only && acc_measured_nothing(x)) || *num >= OGS_MAX_NUM_OF_URR)
            continue;
        memset(&t, 0, sizeof(t));
        t.linked_usage_reporting = 1;
        fill_report(x, report, (*num)++, &t);
    }
}

/* The reports of a modification: Query URR and QAURR before any update, then Remove URR (cl. 5.2.2.3.1: "generate usage report(s) ... for
 * the URR(s) being queried and for any associated linked usage reports for which there are non-null measurements"; "When being instructed to
 * remove a URR ... the UP function shall stop its ongoing measurements for the URR and include a Usage Report in the PFCP Session Modification
 * Response ... if there are non-null measurements to report for the URR") */
void upf_mbs_urr_query_and_remove(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_query_urr_t *query, int n_query, bool query_all,
        const ogs_pfcp_tlv_remove_urr_t *remove, int n_remove, ogs_pfcp_user_plane_report_t *report)
{
    unsigned int num = report->num_of_usage_report;
    int i, k;

    if (query_all) {
        for (k = 0; k < OGS_MAX_NUM_OF_URR; k++) {
            upf_mbs_urr_acc_t *acc = &mbs_sess->urr_acc[k];
            ogs_pfcp_usage_report_trigger_t t;

            if (!acc->used || num >= OGS_MAX_NUM_OF_URR)
                continue;
            memset(&t, 0, sizeof(t));
            t.immediate_report = 1;
            fill_report(acc, report, num++, &t);
        }
        for (k = 0; k < OGS_MAX_NUM_OF_URR; k++)
            if (mbs_sess->urr_acc[k].used)
                add_linked(mbs_sess, mbs_sess->urr_acc[k].id, true, report, &num);
    } else {
        for (i = 0; i < n_query; i++) {
            upf_mbs_urr_acc_t *acc;
            ogs_pfcp_usage_report_trigger_t t;
            unsigned int j;
            bool done = false;

            if (!query[i].presence || !(acc = acc_find(mbs_sess, query[i].urr_id.u32)) || num >= OGS_MAX_NUM_OF_URR)
                continue;
            for (j = 0; j < num; j++)
                if (report->usage_report[j].id == acc->id)
                    done = true;
            if (done)
                continue;
            memset(&t, 0, sizeof(t));
            t.immediate_report = 1;
            fill_report(acc, report, num++, &t);
            add_linked(mbs_sess, acc->id, true, report, &num);
        }
    }

    for (i = 0; i < n_remove; i++) {
        upf_mbs_urr_acc_t *acc;

        if (!remove[i].presence || !(acc = acc_find(mbs_sess, remove[i].urr_id.u32)))
            continue;
        if (!acc_measured_nothing(acc) && num < OGS_MAX_NUM_OF_URR) {
            ogs_pfcp_usage_report_trigger_t t;
            unsigned int j;
            bool done = false;

            for (j = 0; j < num; j++)
                if (report->usage_report[j].id == acc->id)
                    done = true;
            if (!done) {
                memset(&t, 0, sizeof(t));
                t.immediate_report = 1;
                fill_report(acc, report, num++, &t);
            }
        }
        free_acc(acc);
    }
    report->num_of_usage_report = num;
}

/* cl. 5.2.2.3.1: "At the PFCP session termination, the UP function shall indicate to the CP function, in the PFCP Session Deletion Response, the
 * resources that have been consumed for each URR that was provisioned in the PFCP session since the last usage report" */
void upf_mbs_urr_terminate(upf_mbs_sess_t *mbs_sess, ogs_pfcp_user_plane_report_t *report)
{
    unsigned int num = 0;
    int k;

    for (k = 0; k < OGS_MAX_NUM_OF_URR; k++) {
        upf_mbs_urr_acc_t *acc = &mbs_sess->urr_acc[k];
        ogs_pfcp_usage_report_trigger_t t;

        if (!acc->used)
            continue;
        memset(&t, 0, sizeof(t));
        t.termination_report = 1;
        fill_report(acc, report, num++, &t);
    }
    report->num_of_usage_report = num;
}

void upf_mbs_urr_account(upf_mbs_sess_t *mbs_sess, uint32_t octets)
{
    int i;
    ogs_time_t now = ogs_time_now();

    for (i = 0; i < OGS_MAX_NUM_OF_URR; i++) {
        upf_mbs_urr_acc_t *acc = &mbs_sess->urr_acc[i];
        uint64_t total, dl;

        if (!__atomic_load_n(&acc->used, __ATOMIC_RELAXED))
            continue;
        total = __atomic_add_fetch(&acc->total_octets, octets, __ATOMIC_RELAXED);
        dl = __atomic_add_fetch(&acc->dl_octets, octets, __ATOMIC_RELAXED);
        if (!acc->time_of_first_packet)
            acc->time_of_first_packet = now;
        acc->time_of_last_packet = now;

        if (acc->triggers.volume_threshold &&
                ((acc->thr_total && total - acc->thr_total_octets >= acc->thr_total_volume) ||
                 (acc->thr_downlink && dl - acc->thr_dl_octets >= acc->thr_downlink_volume)) &&
                __atomic_exchange_n(&acc->check_pending, 1, __ATOMIC_RELAXED) == 0) {
            upf_event_t *e = upf_event_new(UPF_EVT_MBS_URR_CHECK);

            e->seid = acc->seid;
            e->urr_id = acc->id;
            if (ogs_queue_push(ogs_app()->queue, e) != OGS_OK) {
                ogs_error("ogs_queue_push() failed");
                upf_event_free(e);
                __atomic_store_n(&acc->check_pending, 0, __ATOMIC_RELAXED);
            }
        }
    }
}

/* A Session Report Request with the Usage Reports of the URR that report and of the URRs linked to them */
static void send_usage_report(upf_mbs_sess_t *mbs_sess, upf_mbs_urr_acc_t *acc, const ogs_pfcp_usage_report_trigger_t *trigger)
{
    ogs_pfcp_user_plane_report_t report;
    unsigned int num = 0;

    memset(&report, 0, sizeof(report));
    fill_report(acc, &report, num++, trigger);
    add_linked(mbs_sess, acc->id, false, &report, &num);
    report.num_of_usage_report = num;
    if (!mbs_sess->pfcp_node) {
        ogs_error("The MBS session has no PFCP node to report to");
        return;
    }
    if (upf_pfcp_n4mb_send_session_report_request(mbs_sess, &report) != OGS_OK)
        ogs_error("The PFCP Session Report Request with the Usage Report was not sent");
}

void upf_mbs_charging_handle_check(uint64_t seid, uint32_t urr_id)
{
    upf_mbs_sess_t *mbs_sess = upf_mbs_sess_find_by_seid(seid);
    upf_mbs_urr_acc_t *acc;
    uint64_t total, dl;

    if (!mbs_sess || !(acc = acc_find(mbs_sess, urr_id)))
        return;
    __atomic_store_n(&acc->check_pending, 0, __ATOMIC_RELAXED);
    total = load(&acc->total_octets);
    dl = load(&acc->dl_octets);
    if ((acc->thr_total && total - acc->thr_total_octets >= acc->thr_total_volume) ||
            (acc->thr_downlink && dl - acc->thr_dl_octets >= acc->thr_downlink_volume)) {
        ogs_pfcp_usage_report_trigger_t t;

        memset(&t, 0, sizeof(t));
        t.volume_threshold = 1;
        send_usage_report(mbs_sess, acc, &t);
    }
}

void upf_mbs_charging_handle_period(uint64_t seid, uint32_t urr_id)
{
    upf_mbs_sess_t *mbs_sess = upf_mbs_sess_find_by_seid(seid);
    upf_mbs_urr_acc_t *acc;
    ogs_pfcp_usage_report_trigger_t t;

    if (!mbs_sess || !(acc = acc_find(mbs_sess, urr_id)))
        return;
    memset(&t, 0, sizeof(t));
    t.periodic_reporting = 1;
    send_usage_report(mbs_sess, acc, &t);
    period_arm(acc);
}
