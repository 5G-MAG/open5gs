/*
 * mbs_n2_message_transfer_rsp_data.h
 *
 * Data within MBS N2 Message Transfer Response
 */

#ifndef _OpenAPI_mbs_n2_message_transfer_rsp_data_H_
#define _OpenAPI_mbs_n2_message_transfer_rsp_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "n2_information_transfer_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_mbs_n2_message_transfer_rsp_data_s OpenAPI_mbs_n2_message_transfer_rsp_data_t;
typedef struct OpenAPI_mbs_n2_message_transfer_rsp_data_s {
    OpenAPI_n2_information_transfer_result_e result;
    char *supported_features;
} OpenAPI_mbs_n2_message_transfer_rsp_data_t;

OpenAPI_mbs_n2_message_transfer_rsp_data_t *OpenAPI_mbs_n2_message_transfer_rsp_data_create(
    OpenAPI_n2_information_transfer_result_e result,
    char *supported_features
);
void OpenAPI_mbs_n2_message_transfer_rsp_data_free(OpenAPI_mbs_n2_message_transfer_rsp_data_t *mbs_n2_message_transfer_rsp_data);
OpenAPI_mbs_n2_message_transfer_rsp_data_t *OpenAPI_mbs_n2_message_transfer_rsp_data_parseFromJSON(cJSON *mbs_n2_message_transfer_rsp_dataJSON);
cJSON *OpenAPI_mbs_n2_message_transfer_rsp_data_convertToJSON(OpenAPI_mbs_n2_message_transfer_rsp_data_t *mbs_n2_message_transfer_rsp_data);
OpenAPI_mbs_n2_message_transfer_rsp_data_t *OpenAPI_mbs_n2_message_transfer_rsp_data_copy(OpenAPI_mbs_n2_message_transfer_rsp_data_t *dst, OpenAPI_mbs_n2_message_transfer_rsp_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_n2_message_transfer_rsp_data_H_ */

