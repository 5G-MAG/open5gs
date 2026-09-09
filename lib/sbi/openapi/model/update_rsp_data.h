/*
 * update_rsp_data.h
 *
 * Data within Update Response
 */

#ifndef _OpenAPI_update_rsp_data_H_
#define _OpenAPI_update_rsp_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "ext_mbs_session.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Not generated: the SBI codegen this tree runs is pinned at Rel-17 (see ext_mbs_session.h's own
 * red_mbs_service_area comment) and never produced an UpdateRspData model at all -- Update's own
 * handler previously always answered "204 No Content". Hand-patched here, mirroring
 * create_rsp_data.h's own create/free/convertToJSON/parseFromJSON pattern, matching the vendored
 * TS29532_Nmbsmf_MBSSession.yaml's own UpdateRspData definition: one required field, mbsSession
 * (ExtMbsSession). TS 29.532 V18.6.0 table 6.2.6.2.24-1 (Type: UpdateRspData); clause 5.3.2.3.1
 * step 2b returns 200 OK and requires the MB-SMF to "provide in the response the representation
 * of the updated MBS session including the reduced MBS service area in the redMbsServArea
 * attribute". */
typedef struct OpenAPI_update_rsp_data_s OpenAPI_update_rsp_data_t;
typedef struct OpenAPI_update_rsp_data_s {
    struct OpenAPI_ext_mbs_session_s *mbs_session;
} OpenAPI_update_rsp_data_t;

OpenAPI_update_rsp_data_t *OpenAPI_update_rsp_data_create(
    OpenAPI_ext_mbs_session_t *mbs_session
);
void OpenAPI_update_rsp_data_free(OpenAPI_update_rsp_data_t *update_rsp_data);
OpenAPI_update_rsp_data_t *OpenAPI_update_rsp_data_parseFromJSON(cJSON *update_rsp_dataJSON);
cJSON *OpenAPI_update_rsp_data_convertToJSON(OpenAPI_update_rsp_data_t *update_rsp_data);
OpenAPI_update_rsp_data_t *OpenAPI_update_rsp_data_copy(OpenAPI_update_rsp_data_t *dst, OpenAPI_update_rsp_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_update_rsp_data_H_ */
