/*
 * tmgi_allocate.h
 *
 * Data within TMGI Allocate Request
 */

#ifndef _OpenAPI_tmgi_allocate_H_
#define _OpenAPI_tmgi_allocate_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "tmgi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_tmgi_allocate_s OpenAPI_tmgi_allocate_t;
typedef struct OpenAPI_tmgi_allocate_s {
    bool is_tmgi_number;
    int tmgi_number;
    OpenAPI_list_t *tmgi_list;
} OpenAPI_tmgi_allocate_t;

OpenAPI_tmgi_allocate_t *OpenAPI_tmgi_allocate_create(
    bool is_tmgi_number,
    int tmgi_number,
    OpenAPI_list_t *tmgi_list
);
void OpenAPI_tmgi_allocate_free(OpenAPI_tmgi_allocate_t *tmgi_allocate);
OpenAPI_tmgi_allocate_t *OpenAPI_tmgi_allocate_parseFromJSON(cJSON *tmgi_allocateJSON);
cJSON *OpenAPI_tmgi_allocate_convertToJSON(OpenAPI_tmgi_allocate_t *tmgi_allocate);
OpenAPI_tmgi_allocate_t *OpenAPI_tmgi_allocate_copy(OpenAPI_tmgi_allocate_t *dst, OpenAPI_tmgi_allocate_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_tmgi_allocate_H_ */

