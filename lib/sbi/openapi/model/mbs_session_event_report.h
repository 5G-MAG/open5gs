/*
 * mbs_session_event_report.h
 *
 * MBS session event report
 */

#ifndef _OpenAPI_mbs_session_event_report_H_
#define _OpenAPI_mbs_session_event_report_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "broadcast_delivery_status.h"
#include "ingress_tun_addr_info.h"
#include "mbs_session_event_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_session_event_report_s OpenAPI_mbs_session_event_report_t;
typedef struct OpenAPI_mbs_session_event_report_s {
    OpenAPI_mbs_session_event_type_e event_type;
    char *time_stamp;
    struct OpenAPI_ingress_tun_addr_info_s *ingress_tun_addr_info;
    OpenAPI_broadcast_delivery_status_e broadcast_del_status;
} OpenAPI_mbs_session_event_report_t;

OpenAPI_mbs_session_event_report_t *OpenAPI_mbs_session_event_report_create(
    OpenAPI_mbs_session_event_type_e event_type,
    char *time_stamp,
    OpenAPI_ingress_tun_addr_info_t *ingress_tun_addr_info,
    OpenAPI_broadcast_delivery_status_e broadcast_del_status
);
void OpenAPI_mbs_session_event_report_free(OpenAPI_mbs_session_event_report_t *mbs_session_event_report);
OpenAPI_mbs_session_event_report_t *OpenAPI_mbs_session_event_report_parseFromJSON(cJSON *mbs_session_event_reportJSON);
cJSON *OpenAPI_mbs_session_event_report_convertToJSON(OpenAPI_mbs_session_event_report_t *mbs_session_event_report);
OpenAPI_mbs_session_event_report_t *OpenAPI_mbs_session_event_report_copy(OpenAPI_mbs_session_event_report_t *dst, OpenAPI_mbs_session_event_report_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_session_event_report_H_ */

