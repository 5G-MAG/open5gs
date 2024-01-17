/*
 * mbs_service_info.h
 *
 * Represent MBS Service Information.
 */

#ifndef _OpenAPI_mbs_service_info_H_
#define _OpenAPI_mbs_service_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_media_comp_rm.h"
#include "reserv_priority.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_service_info_s OpenAPI_mbs_service_info_t;
typedef struct OpenAPI_mbs_service_info_s {
    OpenAPI_list_t* mbs_media_comps;
    OpenAPI_reserv_priority_e mbs_sdf_res_prio;
    char *af_app_id;
    char *mbs_session_ambr;
} OpenAPI_mbs_service_info_t;

OpenAPI_mbs_service_info_t *OpenAPI_mbs_service_info_create(
    OpenAPI_list_t* mbs_media_comps,
    OpenAPI_reserv_priority_e mbs_sdf_res_prio,
    char *af_app_id,
    char *mbs_session_ambr
);
void OpenAPI_mbs_service_info_free(OpenAPI_mbs_service_info_t *mbs_service_info);
OpenAPI_mbs_service_info_t *OpenAPI_mbs_service_info_parseFromJSON(cJSON *mbs_service_infoJSON);
cJSON *OpenAPI_mbs_service_info_convertToJSON(OpenAPI_mbs_service_info_t *mbs_service_info);
OpenAPI_mbs_service_info_t *OpenAPI_mbs_service_info_copy(OpenAPI_mbs_service_info_t *dst, OpenAPI_mbs_service_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_service_info_H_ */

