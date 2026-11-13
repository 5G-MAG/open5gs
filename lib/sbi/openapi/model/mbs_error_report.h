/*
 * mbs_error_report.h
 *
 * Represents the reporting of MBS Policy decision level failure(s) and/or MBS PCC rule level failure(s). 
 */

#ifndef _OpenAPI_mbs_error_report_H_
#define _OpenAPI_mbs_error_report_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
typedef struct OpenAPI_mbs_error_report_s OpenAPI_mbs_error_report_t;
#include "mbs_report.h"

#ifdef __cplusplus
extern "C" {
#endif

struct OpenAPI_mbs_error_report_s {
    OpenAPI_list_t *mbs_reports;
};

OpenAPI_mbs_error_report_t *OpenAPI_mbs_error_report_create(
    OpenAPI_list_t *mbs_reports
);
void OpenAPI_mbs_error_report_free(OpenAPI_mbs_error_report_t *mbs_error_report);
OpenAPI_mbs_error_report_t *OpenAPI_mbs_error_report_parseFromJSON(cJSON *mbs_error_reportJSON);
cJSON *OpenAPI_mbs_error_report_convertToJSON(OpenAPI_mbs_error_report_t *mbs_error_report);
OpenAPI_mbs_error_report_t *OpenAPI_mbs_error_report_copy(OpenAPI_mbs_error_report_t *dst, OpenAPI_mbs_error_report_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_mbs_error_report_H_ */

