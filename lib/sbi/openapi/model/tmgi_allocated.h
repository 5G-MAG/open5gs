/*
 * tmgi_allocated.h
 *
 * Data within TMGI Allocate Response
 */

#ifndef _OpenAPI_tmgi_allocated_H_
#define _OpenAPI_tmgi_allocated_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "tmgi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_tmgi_allocated_s OpenAPI_tmgi_allocated_t;
typedef struct OpenAPI_tmgi_allocated_s {
    OpenAPI_list_t *tmgi_list;
    char *expiration_time;
    char *nid;
} OpenAPI_tmgi_allocated_t;

OpenAPI_tmgi_allocated_t *OpenAPI_tmgi_allocated_create(
    OpenAPI_list_t *tmgi_list,
    char *expiration_time,
    char *nid
);
void OpenAPI_tmgi_allocated_free(OpenAPI_tmgi_allocated_t *tmgi_allocated);
OpenAPI_tmgi_allocated_t *OpenAPI_tmgi_allocated_parseFromJSON(cJSON *tmgi_allocatedJSON);
cJSON *OpenAPI_tmgi_allocated_convertToJSON(OpenAPI_tmgi_allocated_t *tmgi_allocated);
OpenAPI_tmgi_allocated_t *OpenAPI_tmgi_allocated_copy(OpenAPI_tmgi_allocated_t *dst, OpenAPI_tmgi_allocated_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_tmgi_allocated_H_ */

