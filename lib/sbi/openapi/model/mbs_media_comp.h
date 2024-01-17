/*
 * mbs_media_comp.h
 *
 * Represents an MBS Media Component.
 */

#ifndef _OpenAPI_mbs_media_comp_H_
#define _OpenAPI_mbs_media_comp_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_media_info.h"
#include "mbs_qo_s_req.h"
#include "reserv_priority.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_media_comp_s OpenAPI_mbs_media_comp_t;
typedef struct OpenAPI_mbs_media_comp_s {
    int mbs_med_comp_num;
    OpenAPI_list_t *mbs_flow_descs;
    OpenAPI_reserv_priority_e mbs_sdf_res_prio;
    struct OpenAPI_mbs_media_info_s *mbs_media_info;
    char *qos_ref;
    struct OpenAPI_mbs_qo_s_req_s *mbs_qo_s_req;
} OpenAPI_mbs_media_comp_t;

OpenAPI_mbs_media_comp_t *OpenAPI_mbs_media_comp_create(
    int mbs_med_comp_num,
    OpenAPI_list_t *mbs_flow_descs,
    OpenAPI_reserv_priority_e mbs_sdf_res_prio,
    OpenAPI_mbs_media_info_t *mbs_media_info,
    char *qos_ref,
    OpenAPI_mbs_qo_s_req_t *mbs_qo_s_req
);
void OpenAPI_mbs_media_comp_free(OpenAPI_mbs_media_comp_t *mbs_media_comp);
OpenAPI_mbs_media_comp_t *OpenAPI_mbs_media_comp_parseFromJSON(cJSON *mbs_media_compJSON);
cJSON *OpenAPI_mbs_media_comp_convertToJSON(OpenAPI_mbs_media_comp_t *mbs_media_comp);
OpenAPI_mbs_media_comp_t *OpenAPI_mbs_media_comp_copy(OpenAPI_mbs_media_comp_t *dst, OpenAPI_mbs_media_comp_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_media_comp_H_ */

