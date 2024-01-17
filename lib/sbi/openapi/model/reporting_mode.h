/*
 * reporting_mode.h
 *
 * 
 */

#ifndef _OpenAPI_reporting_mode_H_
#define _OpenAPI_reporting_mode_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_reporting_mode_NULL = 0, OpenAPI_reporting_mode_CONTINUOUS, OpenAPI_reporting_mode_ONE_TIME } OpenAPI_reporting_mode_e;

char* OpenAPI_reporting_mode_ToString(OpenAPI_reporting_mode_e reporting_mode);

OpenAPI_reporting_mode_e OpenAPI_reporting_mode_FromString(char* reporting_mode);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_reporting_mode_H_ */

