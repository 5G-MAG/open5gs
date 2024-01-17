/*
 * n2_mbs_sm_info.h
 *
 * N2 MBS Session Management information
 */

#ifndef _OpenAPI_n2_mbs_sm_info_H_
#define _OpenAPI_n2_mbs_sm_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "ngap_ie_type.h"
#include "ref_to_binary_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_n2_mbs_sm_info_s OpenAPI_n2_mbs_sm_info_t;
typedef struct OpenAPI_n2_mbs_sm_info_s {
    OpenAPI_ngap_ie_type_e ngap_ie_type;
    struct OpenAPI_ref_to_binary_data_s *ngap_data;
} OpenAPI_n2_mbs_sm_info_t;

OpenAPI_n2_mbs_sm_info_t *OpenAPI_n2_mbs_sm_info_create(
    OpenAPI_ngap_ie_type_e ngap_ie_type,
    OpenAPI_ref_to_binary_data_t *ngap_data
);
void OpenAPI_n2_mbs_sm_info_free(OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info);
OpenAPI_n2_mbs_sm_info_t *OpenAPI_n2_mbs_sm_info_parseFromJSON(cJSON *n2_mbs_sm_infoJSON);
cJSON *OpenAPI_n2_mbs_sm_info_convertToJSON(OpenAPI_n2_mbs_sm_info_t *n2_mbs_sm_info);
OpenAPI_n2_mbs_sm_info_t *OpenAPI_n2_mbs_sm_info_copy(OpenAPI_n2_mbs_sm_info_t *dst, OpenAPI_n2_mbs_sm_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_n2_mbs_sm_info_H_ */

