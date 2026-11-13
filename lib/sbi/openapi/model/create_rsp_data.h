/*
 * create_rsp_data.h
 *
 * Data within Create Response
 */

#ifndef _OpenAPI_create_rsp_data_H_
#define _OpenAPI_create_rsp_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_create_rsp_data_s OpenAPI_create_rsp_data_t;
#include "ext_mbs_session.h"
#include "mbs_session_event_report_list.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_create_rsp_data_s {
    struct OpenAPI_ext_mbs_session_s *mbs_session;
    struct OpenAPI_mbs_session_event_report_list_s *event_list;
    char *supported_features;
};

OpenAPI_create_rsp_data_t *OpenAPI_create_rsp_data_create(
    OpenAPI_ext_mbs_session_t *mbs_session,
    OpenAPI_mbs_session_event_report_list_t *event_list,
    char *supported_features
);
void OpenAPI_create_rsp_data_free(OpenAPI_create_rsp_data_t *create_rsp_data);
OpenAPI_create_rsp_data_t *OpenAPI_create_rsp_data_parseFromJSON(cJSON *create_rsp_dataJSON);
cJSON *OpenAPI_create_rsp_data_convertToJSON(OpenAPI_create_rsp_data_t *create_rsp_data);
OpenAPI_create_rsp_data_t *OpenAPI_create_rsp_data_copy(OpenAPI_create_rsp_data_t *dst, OpenAPI_create_rsp_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_create_rsp_data_H_ */

