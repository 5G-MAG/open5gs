/*
 * mbs_ngap_ie_type.h
 *
 * NGAP Information Element Type
 */

#ifndef _OpenAPI_mbs_ngap_ie_type_H_
#define _OpenAPI_mbs_ngap_ie_type_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_mbs_ngap_ie_type_NULL = 0, OpenAPI_mbs_ngap_ie_type_MBS_SES_ACT_REQ, OpenAPI_mbs_ngap_ie_type_MBS_SES_DEACT_REQ, OpenAPI_mbs_ngap_ie_type_MBS_SES_UPD_REQ } OpenAPI_mbs_ngap_ie_type_e;

char* OpenAPI_mbs_ngap_ie_type_ToString(OpenAPI_mbs_ngap_ie_type_e mbs_ngap_ie_type);

OpenAPI_mbs_ngap_ie_type_e OpenAPI_mbs_ngap_ie_type_FromString(char* mbs_ngap_ie_type);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_ngap_ie_type_H_ */

