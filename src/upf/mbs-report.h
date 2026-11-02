/*
 * PFCP Session Report Requests of the MB-UPF for an MBS session (TS 29.244 cl. 7.5.8, N4mb): the Downlink Data Report of a FAR that
 * buffers and notifies (cl. 5.2.3, 5.34.2.3, 5.34.2.4) and the User Plane Inactivity Report (cl. 5.11.2).
 */

#ifndef UPF_MBS_REPORT_H
#define UPF_MBS_REPORT_H

#include "ogs-core.h"
#include "ogs-pfcp.h"
#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The User Plane Inactivity Timer IE of an establishment or a modification (seconds, cl. 8.2.83) */
void upf_mbs_set_inactivity_timer(upf_mbs_sess_t *mbs_sess, uint32_t seconds);
void upf_mbs_inactivity_stop(upf_mbs_sess_t *mbs_sess);

/* The data path got a packet for the session (poll thread: a time stamp only) */
void upf_mbs_note_activity(upf_mbs_sess_t *mbs_sess);

/* In the state machine */
void upf_mbs_report_handle_timer(int timer_id, uint64_t seid);
void upf_mbs_report_handle_dl_data(uint64_t seid, uint16_t pdr_id, uint8_t qfi);

/* From the data path (poll thread): the first packet of the PDR was buffered by a FAR that notifies the CP function */
void upf_mbs_report_dl_data_event(upf_mbs_sess_t *mbs_sess, uint16_t pdr_id, uint8_t qfi);

#ifdef __cplusplus
}
#endif

#endif /* UPF_MBS_REPORT_H */
