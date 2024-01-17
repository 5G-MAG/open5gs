/*
 * mbs_context_info.h
 *
 * MBS context information
 */

#ifndef _OpenAPI_mbs_context_info_H_
#define _OpenAPI_mbs_context_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_service_area.h"
#include "mbs_service_area_info.h"
#include "ssm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_context_info_s OpenAPI_mbs_context_info_t;
typedef struct OpenAPI_mbs_context_info_s {
    char *start_time;
    bool is_any_ue_ind;
    int any_ue_ind;
    struct OpenAPI_ssm_s *ll_ssm;
    bool is_c_teid;
    int c_teid;
    struct OpenAPI_mbs_service_area_s *mbs_service_area;
    OpenAPI_list_t* mbs_service_area_info_list;
} OpenAPI_mbs_context_info_t;

OpenAPI_mbs_context_info_t *OpenAPI_mbs_context_info_create(
    char *start_time,
    bool is_any_ue_ind,
    int any_ue_ind,
    OpenAPI_ssm_t *ll_ssm,
    bool is_c_teid,
    int c_teid,
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_list_t* mbs_service_area_info_list
);
void OpenAPI_mbs_context_info_free(OpenAPI_mbs_context_info_t *mbs_context_info);
OpenAPI_mbs_context_info_t *OpenAPI_mbs_context_info_parseFromJSON(cJSON *mbs_context_infoJSON);
cJSON *OpenAPI_mbs_context_info_convertToJSON(OpenAPI_mbs_context_info_t *mbs_context_info);
OpenAPI_mbs_context_info_t *OpenAPI_mbs_context_info_copy(OpenAPI_mbs_context_info_t *dst, OpenAPI_mbs_context_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_context_info_H_ */

