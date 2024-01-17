/*
 * acceptable_mbs_serv_info.h
 *
 * Contains the MBS Service Information that can be accepted by the PCF. 
 */

#ifndef _OpenAPI_acceptable_mbs_serv_info_H_
#define _OpenAPI_acceptable_mbs_serv_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "mbs_media_comp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_acceptable_mbs_serv_info_s OpenAPI_acceptable_mbs_serv_info_t;
typedef struct OpenAPI_acceptable_mbs_serv_info_s {
    OpenAPI_list_t* acc_mbs_serv_info;
    char *acc_max_mbs_bw;
} OpenAPI_acceptable_mbs_serv_info_t;

OpenAPI_acceptable_mbs_serv_info_t *OpenAPI_acceptable_mbs_serv_info_create(
    OpenAPI_list_t* acc_mbs_serv_info,
    char *acc_max_mbs_bw
);
void OpenAPI_acceptable_mbs_serv_info_free(OpenAPI_acceptable_mbs_serv_info_t *acceptable_mbs_serv_info);
OpenAPI_acceptable_mbs_serv_info_t *OpenAPI_acceptable_mbs_serv_info_parseFromJSON(cJSON *acceptable_mbs_serv_infoJSON);
cJSON *OpenAPI_acceptable_mbs_serv_info_convertToJSON(OpenAPI_acceptable_mbs_serv_info_t *acceptable_mbs_serv_info);
OpenAPI_acceptable_mbs_serv_info_t *OpenAPI_acceptable_mbs_serv_info_copy(OpenAPI_acceptable_mbs_serv_info_t *dst, OpenAPI_acceptable_mbs_serv_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_acceptable_mbs_serv_info_H_ */

