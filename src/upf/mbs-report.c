#include "context.h"
#include "event.h"
#include "timer.h"
#include "pfcp-path.h"
#include "mbs-report.h"

/* A PFCP Session Report Request for the MBS session, to the MB-SMF that controls it */
static void send_report(upf_mbs_sess_t *mbs_sess, ogs_pfcp_user_plane_report_t *report)
{
    if (!mbs_sess->pfcp_node) {
        ogs_error("The MBS session has no PFCP node to report to");
        return;
    }
    if (upf_pfcp_n4mb_send_session_report_request(mbs_sess, report) != OGS_OK)
        ogs_error("The PFCP Session Report Request of the MBS session was not sent");
}

void upf_mbs_note_activity(upf_mbs_sess_t *mbs_sess)
{
    mbs_sess->last_dl_packet = ogs_time_now();
}

void upf_mbs_inactivity_stop(upf_mbs_sess_t *mbs_sess)
{
    mbs_sess->up_inactivity_monitoring = false;
    if (mbs_sess->t_up_inactivity)
        ogs_timer_stop(mbs_sess->t_up_inactivity);
}

/*
 * TS 29.244 cl. 5.11.2: "Upon being provisioned with this IE, the UP function shall monitor the user plane activity of the PFCP session and
 * report any user plane inactivity exceeding the duration indicated by this IE by sending a PFCP Session Report Request with the Report Type
 * set to UPIR (User Plane Inactivity Report). When the UP function receives an updated User Plane Inactivity Timer from the CP function, the UP
 * function shall immediately update the PDU Session inactivity timer and: send a User Plane Inactivity Report if the user plane inactivity has
 * already exceeded the updated User Plane Inactivity Timer; or use the updated User Plane Inactivity Timer for monitoring user plane activity."
 */
void upf_mbs_set_inactivity_timer(upf_mbs_sess_t *mbs_sess, uint32_t seconds)
{
    ogs_time_t now = ogs_time_now();
    ogs_time_t idle;

    if (!mbs_sess->t_up_inactivity) {
        mbs_sess->t_up_inactivity = ogs_timer_add(ogs_app()->timer_mgr, upf_timer_mbs_inactivity,
                (void *)(uintptr_t)mbs_sess->upf_n4mb_seid);
        ogs_assert(mbs_sess->t_up_inactivity);
    }

    if (!mbs_sess->up_inactivity_monitoring) {
        /* provisioned (again): the inactivity counts from now */
        mbs_sess->last_dl_packet = now;
        mbs_sess->up_inactivity_monitoring = true;
    }
    mbs_sess->up_inactivity_timer = seconds;

    idle = now - mbs_sess->last_dl_packet;
    if (idle >= ogs_time_from_sec(seconds)) {
        ogs_pfcp_user_plane_report_t report;

        memset(&report, 0, sizeof(report));
        report.type.user_plane_inactivity_report = 1;
        mbs_sess->up_inactivity_monitoring = false;
        ogs_timer_stop(mbs_sess->t_up_inactivity);
        send_report(mbs_sess, &report);
        return;
    }
    ogs_timer_start(mbs_sess->t_up_inactivity, ogs_time_from_sec(seconds) - idle);
}

void upf_mbs_report_handle_timer(int timer_id, uint64_t seid)
{
    upf_mbs_sess_t *mbs_sess = upf_mbs_sess_find_by_seid(seid);

    if (!mbs_sess || timer_id != UPF_TIMER_MBS_INACTIVITY || !mbs_sess->up_inactivity_monitoring)
        return;     /* the session ended, or the monitoring stopped, while the event was queued */

    upf_mbs_set_inactivity_timer(mbs_sess, mbs_sess->up_inactivity_timer);
}

void upf_mbs_report_dl_data_event(upf_mbs_sess_t *mbs_sess, uint16_t pdr_id, uint8_t qfi)
{
    upf_event_t *e = upf_event_new(UPF_EVT_MBS_DL_DATA);

    e->seid = mbs_sess->upf_n4mb_seid;
    e->pdr_id = pdr_id;
    e->qfi = qfi;
    if (ogs_queue_push(ogs_app()->queue, e) != OGS_OK) {
        ogs_error("ogs_queue_push() failed");
        upf_event_free(e);
    }
}

void upf_mbs_report_handle_dl_data(uint64_t seid, uint16_t pdr_id, uint8_t qfi)
{
    upf_mbs_sess_t *mbs_sess = upf_mbs_sess_find_by_seid(seid);
    ogs_pfcp_user_plane_report_t report;

    if (!mbs_sess)
        return;
    memset(&report, 0, sizeof(report));
    report.type.downlink_data_report = 1;
    report.downlink_data.pdr_id = pdr_id;
    report.downlink_data.qfi = qfi;
    send_report(mbs_sess, &report);
}
