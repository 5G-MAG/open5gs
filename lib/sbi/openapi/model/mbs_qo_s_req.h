/*
 * mbs_qo_s_req.h
 *
 * Represent MBS QoS requirements.
 */

#ifndef _OpenAPI_mbs_qo_s_req_H_
#define _OpenAPI_mbs_qo_s_req_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "arp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_qo_s_req_s OpenAPI_mbs_qo_s_req_t;
typedef struct OpenAPI_mbs_qo_s_req_s {
    int _5qi;
    char *guar_bit_rate;
    char *max_bit_rate;
    bool is_aver_window;
    int aver_window;
    struct OpenAPI_arp_s *req_mbs_arp;
} OpenAPI_mbs_qo_s_req_t;

OpenAPI_mbs_qo_s_req_t *OpenAPI_mbs_qo_s_req_create(
    int _5qi,
    char *guar_bit_rate,
    char *max_bit_rate,
    bool is_aver_window,
    int aver_window,
    OpenAPI_arp_t *req_mbs_arp
);
void OpenAPI_mbs_qo_s_req_free(OpenAPI_mbs_qo_s_req_t *mbs_qo_s_req);
OpenAPI_mbs_qo_s_req_t *OpenAPI_mbs_qo_s_req_parseFromJSON(cJSON *mbs_qo_s_reqJSON);
cJSON *OpenAPI_mbs_qo_s_req_convertToJSON(OpenAPI_mbs_qo_s_req_t *mbs_qo_s_req);
OpenAPI_mbs_qo_s_req_t *OpenAPI_mbs_qo_s_req_copy(OpenAPI_mbs_qo_s_req_t *dst, OpenAPI_mbs_qo_s_req_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_qo_s_req_H_ */

