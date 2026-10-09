/*
 * mbs_comm_n2_mbs_sm_info.h
 *
 * N2 MBS Session Management information
 */

#ifndef _OpenAPI_mbs_comm_n2_mbs_sm_info_H_
#define _OpenAPI_mbs_comm_n2_mbs_sm_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_ngap_ie_type.h"
#include "ref_to_binary_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_comm_n2_mbs_sm_info_s OpenAPI_mbs_comm_n2_mbs_sm_info_t;
typedef struct OpenAPI_mbs_comm_n2_mbs_sm_info_s {
    OpenAPI_mbs_ngap_ie_type_e ngap_ie_type;
    struct OpenAPI_ref_to_binary_data_s *ngap_data;
} OpenAPI_mbs_comm_n2_mbs_sm_info_t;

OpenAPI_mbs_comm_n2_mbs_sm_info_t *OpenAPI_mbs_comm_n2_mbs_sm_info_create(
    OpenAPI_mbs_ngap_ie_type_e ngap_ie_type,
    OpenAPI_ref_to_binary_data_t *ngap_data
);
void OpenAPI_mbs_comm_n2_mbs_sm_info_free(OpenAPI_mbs_comm_n2_mbs_sm_info_t *mbs_comm_n2_mbs_sm_info);
OpenAPI_mbs_comm_n2_mbs_sm_info_t *OpenAPI_mbs_comm_n2_mbs_sm_info_parseFromJSON(cJSON *mbs_comm_n2_mbs_sm_infoJSON);
cJSON *OpenAPI_mbs_comm_n2_mbs_sm_info_convertToJSON(OpenAPI_mbs_comm_n2_mbs_sm_info_t *mbs_comm_n2_mbs_sm_info);
OpenAPI_mbs_comm_n2_mbs_sm_info_t *OpenAPI_mbs_comm_n2_mbs_sm_info_copy(OpenAPI_mbs_comm_n2_mbs_sm_info_t *dst, OpenAPI_mbs_comm_n2_mbs_sm_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_comm_n2_mbs_sm_info_H_ */

