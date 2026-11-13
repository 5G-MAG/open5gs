/*
 * nr_red_cap_ue_info.h
 *
 * Indicates whether the broadcast MBS session is intended only for NR (e)RedCap UEs,  only for UEs that are neither NR RedCap UEs nor NR RedCap UEs, or for any kind of UE.  
 */

#ifndef _OpenAPI_nr_red_cap_ue_info_H_
#define _OpenAPI_nr_red_cap_ue_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { OpenAPI_nr_red_cap_ue_info_NULL = 0, OpenAPI_nr_red_cap_ue_info_NR_REDCAP_UE_ONLY, OpenAPI_nr_red_cap_ue_info_BOTH_NR_REDCAP_UE_AND_NON_REDCAP_UE, OpenAPI_nr_red_cap_ue_info_NON_REDCAP_UE_ONLY } OpenAPI_nr_red_cap_ue_info_e;

char* OpenAPI_nr_red_cap_ue_info_ToString(OpenAPI_nr_red_cap_ue_info_e nr_red_cap_ue_info);

OpenAPI_nr_red_cap_ue_info_e OpenAPI_nr_red_cap_ue_info_FromString(char* nr_red_cap_ue_info);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_nr_red_cap_ue_info_H_ */

