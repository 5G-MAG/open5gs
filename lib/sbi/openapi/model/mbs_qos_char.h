/*
 * mbs_qos_char.h
 *
 * Represents the parameters constituting a set of explicitly signalled QoS characteristics. 
 */

#ifndef _OpenAPI_mbs_qos_char_H_
#define _OpenAPI_mbs_qos_char_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_qos_char_s OpenAPI_mbs_qos_char_t;
#include "qos_resource_type.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_qos_char_s {
    int _5qi;
    int priority_level;
    OpenAPI_qos_resource_type_e resource_type;
    int packet_delay_budget;
    char *packet_error_rate;
    bool is_aver_window;
    int aver_window;
    int mbs_max_data_burst_vol;
};

OpenAPI_mbs_qos_char_t *OpenAPI_mbs_qos_char_create(
    int _5qi,
    int priority_level,
    OpenAPI_qos_resource_type_e resource_type,
    int packet_delay_budget,
    char *packet_error_rate,
    bool is_aver_window,
    int aver_window,
    int mbs_max_data_burst_vol
);
void OpenAPI_mbs_qos_char_free(OpenAPI_mbs_qos_char_t *mbs_qos_char);
OpenAPI_mbs_qos_char_t *OpenAPI_mbs_qos_char_parseFromJSON(cJSON *mbs_qos_charJSON);
cJSON *OpenAPI_mbs_qos_char_convertToJSON(OpenAPI_mbs_qos_char_t *mbs_qos_char);
OpenAPI_mbs_qos_char_t *OpenAPI_mbs_qos_char_copy(OpenAPI_mbs_qos_char_t *dst, OpenAPI_mbs_qos_char_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_qos_char_H_ */

