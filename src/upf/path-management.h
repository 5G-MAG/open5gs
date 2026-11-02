/*
 * GTP-U path management of the MB-UPF for unicast N3mb transport: Echo Request and Echo Response (TS 29.281 cl. 7.2), the
 * retransmission of an Echo Request (cl. 11, 12: T3-RESPONSE and N3-REQUESTS) and the report of a path that does not
 * answer, in a PFCP Node Report Request with a User Plane Path Failure Report (TS 29.244 cl. 5.10A, 6.2.9, 7.4.5.1.2;
 * TS 23.527 cl. 8.1B, 8.5).
 */

#ifndef UPF_PATH_MANAGEMENT_H
#define UPF_PATH_MANAGEMENT_H

#include "ogs-core.h"
#include "ogs-pfcp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Whether the configuration asks for Echo Requests (upf.mbs.gtpu_path.echo_interval) */
bool upf_gtpu_path_enabled(void);

/* The remote GTP-U peers of the MBS sessions changed: start the paths that are now in use, end those that are not */
void upf_gtpu_path_sync(void);

/* Timers and the Echo Response, in the event loop of the state machine */
void upf_gtpu_path_handle_timer(int timer_id, ogs_pool_id_t id);
void upf_gtpu_path_handle_echo_response(const ogs_ip_t *from, uint16_t sequence_number);

void upf_gtpu_path_final(void);

/* Builds the PFCP Node Report Request body for the remote GTP-U peers that failed (test hook, also used to send) */
ogs_pkbuf_t *upf_gtpu_path_build_failure_report(const ogs_ip_t *peers, int num_of_peers);

#ifdef __cplusplus
}
#endif

#endif /* UPF_PATH_MANAGEMENT_H */
