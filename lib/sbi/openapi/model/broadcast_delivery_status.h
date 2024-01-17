/*
 * broadcast_delivery_status.h
 *
 * 
 */

#ifndef _OpenAPI_broadcast_delivery_status_H_
#define _OpenAPI_broadcast_delivery_status_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_broadcast_delivery_status_NULL = 0, OpenAPI_broadcast_delivery_status_STARTED, OpenAPI_broadcast_delivery_status_TERMINATED } OpenAPI_broadcast_delivery_status_e;

char* OpenAPI_broadcast_delivery_status_ToString(OpenAPI_broadcast_delivery_status_e broadcast_delivery_status);

OpenAPI_broadcast_delivery_status_e OpenAPI_broadcast_delivery_status_FromString(char* broadcast_delivery_status);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_broadcast_delivery_status_H_ */

