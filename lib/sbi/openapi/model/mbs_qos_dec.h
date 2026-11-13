/*
 * mbs_qos_dec.h
 *
 * Represents the parameters constituting an MBS QoS Decision.
 */

#ifndef _OpenAPI_mbs_qos_dec_H_
#define _OpenAPI_mbs_qos_dec_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_qos_dec_s OpenAPI_mbs_qos_dec_t;
#include "arp.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_qos_dec_s {
    char *mbs_qos_id;
    bool is__5qi;
    int _5qi;
    bool is_priority_level;
    int priority_level;
    char *mbr_dl;
    char *gbr_dl;
    struct OpenAPI_arp_s *arp;
    bool is_aver_window;
    int aver_window;
    bool is_mbs_max_data_burst_vol;
    int mbs_max_data_burst_vol;
};

OpenAPI_mbs_qos_dec_t *OpenAPI_mbs_qos_dec_create(
    char *mbs_qos_id,
    bool is__5qi,
    int _5qi,
    bool is_priority_level,
    int priority_level,
    char *mbr_dl,
    char *gbr_dl,
    OpenAPI_arp_t *arp,
    bool is_aver_window,
    int aver_window,
    bool is_mbs_max_data_burst_vol,
    int mbs_max_data_burst_vol
);
void OpenAPI_mbs_qos_dec_free(OpenAPI_mbs_qos_dec_t *mbs_qos_dec);
OpenAPI_mbs_qos_dec_t *OpenAPI_mbs_qos_dec_parseFromJSON(cJSON *mbs_qos_decJSON);
cJSON *OpenAPI_mbs_qos_dec_convertToJSON(OpenAPI_mbs_qos_dec_t *mbs_qos_dec);
OpenAPI_mbs_qos_dec_t *OpenAPI_mbs_qos_dec_copy(OpenAPI_mbs_qos_dec_t *dst, OpenAPI_mbs_qos_dec_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_qos_dec_H_ */

