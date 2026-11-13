/*
 * mbs_pcrt.h
 *
 * 
 */

#ifndef _OpenAPI_mbs_pcrt_H_
#define _OpenAPI_mbs_pcrt_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_mbs_pcrt_NULL = 0, OpenAPI_mbs_pcrt_MBS_SESSION_UPDATE } OpenAPI_mbs_pcrt_e;

char* OpenAPI_mbs_pcrt_ToString(OpenAPI_mbs_pcrt_e mbs_pcrt);

OpenAPI_mbs_pcrt_e OpenAPI_mbs_pcrt_FromString(char* mbs_pcrt);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_pcrt_H_ */

