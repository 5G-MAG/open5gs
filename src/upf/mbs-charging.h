/*
 * MBS charging of the MB-UPF (TS 29.244 V18.10.0 cl. 5.34.2.5): URRs of an MBS session that measure the volume of the MBS data
 * and the Usage Reports of the Session Deletion Response, the Session Modification Response (Query URR, QAURR, Remove URR) and the
 * Session Report Request (periodic reporting, volume threshold, linked usage reporting).
 */

#ifndef UPF_MBS_CHARGING_H
#define UPF_MBS_CHARGING_H

#include "ogs-core.h"
#include "ogs-pfcp.h"
#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Checks the Create URR IEs of a request (n of them); the session as it is, and the IDs of the request itself */
uint8_t upf_mbs_urr_check_create(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_create_urr_t *create, int n,
        const ogs_pfcp_tlv_remove_urr_t *remove, int num_remove, uint16_t *offending_ie_value);
uint8_t upf_mbs_urr_check_remove_query(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_remove_urr_t *remove, int n_remove,
        const ogs_pfcp_tlv_query_urr_t *query, int n_query, uint16_t *offending_ie_value);

/* The Create URR IEs that have been checked: the URRs start measuring */
void upf_mbs_urr_create(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_create_urr_t *create, int n);

/* A modification that has been checked: the reports of Query URR and QAURR (before any update), of Remove URR, then the removals; the
 * Usage Reports go to report, whose type has the usage report bit if there is one */
void upf_mbs_urr_query_and_remove(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_query_urr_t *query, int n_query, bool query_all,
        const ogs_pfcp_tlv_remove_urr_t *remove, int n_remove, ogs_pfcp_user_plane_report_t *report);

/* The Usage Reports of a session that ends: one for each URR (TERMR) */
void upf_mbs_urr_terminate(upf_mbs_sess_t *mbs_sess, ogs_pfcp_user_plane_report_t *report);

/* The session is removed: the timers go */
void upf_mbs_urr_free_all(upf_mbs_sess_t *mbs_sess);

/* The data path forwarded a packet of the session of this size (poll thread) */
void upf_mbs_urr_account(upf_mbs_sess_t *mbs_sess, uint32_t octets);

/* In the state machine */
void upf_mbs_charging_handle_period(uint64_t seid, uint32_t urr_id);
void upf_mbs_charging_handle_check(uint64_t seid, uint32_t urr_id);

/* Whether the FAR sends to a destination (multicast group or remote GTP-U peer) */
bool upf_mbs_far_has_destination(const ogs_pfcp_far_t *far);

#ifdef __cplusplus
}
#endif

#endif /* UPF_MBS_CHARGING_H */
