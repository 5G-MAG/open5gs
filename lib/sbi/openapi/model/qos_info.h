/*
 * qos_info.h
 *
 * QoS Information
 */

#ifndef _OpenAPI_qos_info_H_
#define _OpenAPI_qos_info_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "qos_flow_add_modify_request_item.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_qos_info_s OpenAPI_qos_info_t;
typedef struct OpenAPI_qos_info_s {
    OpenAPI_list_t *qos_flows_add_mod_request_list;
    OpenAPI_list_t *qos_flows_rel_request_list;
} OpenAPI_qos_info_t;

OpenAPI_qos_info_t *OpenAPI_qos_info_create(
    OpenAPI_list_t *qos_flows_add_mod_request_list,
    OpenAPI_list_t *qos_flows_rel_request_list
);
void OpenAPI_qos_info_free(OpenAPI_qos_info_t *qos_info);
OpenAPI_qos_info_t *OpenAPI_qos_info_parseFromJSON(cJSON *qos_infoJSON);
cJSON *OpenAPI_qos_info_convertToJSON(OpenAPI_qos_info_t *qos_info);
OpenAPI_qos_info_t *OpenAPI_qos_info_copy(OpenAPI_qos_info_t *dst, OpenAPI_qos_info_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_qos_info_H_ */

