/*
 * context_status_event_report.h
 *
 * Context Status Event Report
 */

#ifndef _OpenAPI_context_status_event_report_H_
#define _OpenAPI_context_status_event_report_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "context_status_event_type.h"
#include "mbs_security_context.h"
#include "mbs_service_area.h"
#include "mbs_service_area_info.h"
#include "mbs_session_activity_status.h"
#include "multicast_transport_address_change_info.h"
#include "qos_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_context_status_event_report_s OpenAPI_context_status_event_report_t;
typedef struct OpenAPI_context_status_event_report_s {
    OpenAPI_context_status_event_type_e event_type;
    char *time_stamp;
    struct OpenAPI_qos_info_s *qos_info;
    OpenAPI_mbs_session_activity_status_e status_info;
    struct OpenAPI_mbs_service_area_s *mbs_service_area;
    OpenAPI_list_t* mbs_service_area_info_list;
    struct OpenAPI_multicast_transport_address_change_info_s *multicast_trans_add_info;
    struct OpenAPI_mbs_security_context_s *mbs_security_context;
} OpenAPI_context_status_event_report_t;

OpenAPI_context_status_event_report_t *OpenAPI_context_status_event_report_create(
    OpenAPI_context_status_event_type_e event_type,
    char *time_stamp,
    OpenAPI_qos_info_t *qos_info,
    OpenAPI_mbs_session_activity_status_e status_info,
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_list_t* mbs_service_area_info_list,
    OpenAPI_multicast_transport_address_change_info_t *multicast_trans_add_info,
    OpenAPI_mbs_security_context_t *mbs_security_context
);
void OpenAPI_context_status_event_report_free(OpenAPI_context_status_event_report_t *context_status_event_report);
OpenAPI_context_status_event_report_t *OpenAPI_context_status_event_report_parseFromJSON(cJSON *context_status_event_reportJSON);
cJSON *OpenAPI_context_status_event_report_convertToJSON(OpenAPI_context_status_event_report_t *context_status_event_report);
OpenAPI_context_status_event_report_t *OpenAPI_context_status_event_report_copy(OpenAPI_context_status_event_report_t *dst, OpenAPI_context_status_event_report_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_context_status_event_report_H_ */

