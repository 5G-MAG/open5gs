/*
 * mbs_key_info.h
 *
 * MBS Security Key Data Structure
 */

#ifndef _OpenAPI_mbs_key_info_H_
#define _OpenAPI_mbs_key_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_key_info_s OpenAPI_mbs_key_info_t;
typedef struct OpenAPI_mbs_key_info_s {
    char *key_domain_id;
    char *msk_id;
    char *msk;
    char *msk_lifetime;
    char *mtk_id;
    char *mtk;
} OpenAPI_mbs_key_info_t;

OpenAPI_mbs_key_info_t *OpenAPI_mbs_key_info_create(
    char *key_domain_id,
    char *msk_id,
    char *msk,
    char *msk_lifetime,
    char *mtk_id,
    char *mtk
);
void OpenAPI_mbs_key_info_free(OpenAPI_mbs_key_info_t *mbs_key_info);
OpenAPI_mbs_key_info_t *OpenAPI_mbs_key_info_parseFromJSON(cJSON *mbs_key_infoJSON);
cJSON *OpenAPI_mbs_key_info_convertToJSON(OpenAPI_mbs_key_info_t *mbs_key_info);
OpenAPI_mbs_key_info_t *OpenAPI_mbs_key_info_copy(OpenAPI_mbs_key_info_t *dst, OpenAPI_mbs_key_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_key_info_H_ */

