/*
 * external_mbs_service_area.h
 *
 * List of geographic area or list of civic address info for MBS Service Area
 */

#ifndef _OpenAPI_external_mbs_service_area_H_
#define _OpenAPI_external_mbs_service_area_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "civic_address.h"
#include "geographic_area.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_external_mbs_service_area_s OpenAPI_external_mbs_service_area_t;
typedef struct OpenAPI_external_mbs_service_area_s {
    OpenAPI_list_t *geographic_area_list;
    OpenAPI_list_t *civic_address_list;
} OpenAPI_external_mbs_service_area_t;

OpenAPI_external_mbs_service_area_t *OpenAPI_external_mbs_service_area_create(
    OpenAPI_list_t *geographic_area_list,
    OpenAPI_list_t *civic_address_list
);
void OpenAPI_external_mbs_service_area_free(OpenAPI_external_mbs_service_area_t *external_mbs_service_area);
OpenAPI_external_mbs_service_area_t *OpenAPI_external_mbs_service_area_parseFromJSON(cJSON *external_mbs_service_areaJSON);
cJSON *OpenAPI_external_mbs_service_area_convertToJSON(OpenAPI_external_mbs_service_area_t *external_mbs_service_area);
OpenAPI_external_mbs_service_area_t *OpenAPI_external_mbs_service_area_copy(OpenAPI_external_mbs_service_area_t *dst, OpenAPI_external_mbs_service_area_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_external_mbs_service_area_H_ */

