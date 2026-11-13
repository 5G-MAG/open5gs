/*
 * mbs_charging_data.h
 *
 * Represents the MBS Charging Data.
 */

#ifndef _OpenAPI_mbs_charging_data_H_
#define _OpenAPI_mbs_charging_data_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_charging_data_s OpenAPI_mbs_charging_data_t;
#include "metering_method.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_charging_data_s {
    char *mbs_chg_id;
    bool is_mbs_rating_grp;
    int mbs_rating_grp;
    OpenAPI_metering_method_e metering_method;
};

OpenAPI_mbs_charging_data_t *OpenAPI_mbs_charging_data_create(
    char *mbs_chg_id,
    bool is_mbs_rating_grp,
    int mbs_rating_grp,
    OpenAPI_metering_method_e metering_method
);
void OpenAPI_mbs_charging_data_free(OpenAPI_mbs_charging_data_t *mbs_charging_data);
OpenAPI_mbs_charging_data_t *OpenAPI_mbs_charging_data_parseFromJSON(cJSON *mbs_charging_dataJSON);
cJSON *OpenAPI_mbs_charging_data_convertToJSON(OpenAPI_mbs_charging_data_t *mbs_charging_data);
OpenAPI_mbs_charging_data_t *OpenAPI_mbs_charging_data_copy(OpenAPI_mbs_charging_data_t *dst, OpenAPI_mbs_charging_data_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_charging_data_H_ */

