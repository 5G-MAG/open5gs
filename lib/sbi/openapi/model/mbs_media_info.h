/*
 * mbs_media_info.h
 *
 * Represent MBS Media Information.
 */

#ifndef _OpenAPI_mbs_media_info_H_
#define _OpenAPI_mbs_media_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "media_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_media_info_s OpenAPI_mbs_media_info_t;
typedef struct OpenAPI_mbs_media_info_s {
    OpenAPI_media_type_e mbs_med_type;
    char *max_req_mbs_bw_dl;
    char *min_req_mbs_bw_dl;
    OpenAPI_list_t *codecs;
} OpenAPI_mbs_media_info_t;

OpenAPI_mbs_media_info_t *OpenAPI_mbs_media_info_create(
    OpenAPI_media_type_e mbs_med_type,
    char *max_req_mbs_bw_dl,
    char *min_req_mbs_bw_dl,
    OpenAPI_list_t *codecs
);
void OpenAPI_mbs_media_info_free(OpenAPI_mbs_media_info_t *mbs_media_info);
OpenAPI_mbs_media_info_t *OpenAPI_mbs_media_info_parseFromJSON(cJSON *mbs_media_infoJSON);
cJSON *OpenAPI_mbs_media_info_convertToJSON(OpenAPI_mbs_media_info_t *mbs_media_info);
OpenAPI_mbs_media_info_t *OpenAPI_mbs_media_info_copy(OpenAPI_mbs_media_info_t *dst, OpenAPI_mbs_media_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_media_info_H_ */

