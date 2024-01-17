/*
 * mbs_session_event_type.h
 *
 * 
 */

#ifndef _OpenAPI_mbs_session_event_type_H_
#define _OpenAPI_mbs_session_event_type_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_mbs_session_event_type_NULL = 0, OpenAPI_mbs_session_event_type_MBS_REL_TMGI_EXPIRY, OpenAPI_mbs_session_event_type_BROADCAST_DELIVERY_STATUS, OpenAPI_mbs_session_event_type_INGRESS_TUNNEL_ADD_CHANGE } OpenAPI_mbs_session_event_type_e;

char* OpenAPI_mbs_session_event_type_ToString(OpenAPI_mbs_session_event_type_e mbs_session_event_type);

OpenAPI_mbs_session_event_type_e OpenAPI_mbs_session_event_type_FromString(char* mbs_session_event_type);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_session_event_type_H_ */

