/*
 * ext_mbs_session.h
 *
 * MbsSession common data type with MB-SMF API specific extensions
 */

#ifndef _OpenAPI_ext_mbs_session_H_
#define _OpenAPI_ext_mbs_session_H_

#include <string.h>
#include "../external/cJSON.h"
#include "../include/list.h"
#include "../include/keyValuePair.h"
#include "../include/binary.h"
#include "external_mbs_service_area.h"
#include "mbs_security_context.h"
#include "mbs_service_area.h"
#include "mbs_service_info.h"
#include "mbs_service_type.h"
#include "mbs_session_activity_status.h"
#include "mbs_session_id.h"
#include "mbs_session_subscription.h"
#include "snssai.h"
#include "ssm.h"
#include "tmgi.h"
#include "tunnel_address.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpenAPI_ext_mbs_session_s OpenAPI_ext_mbs_session_t;
typedef struct OpenAPI_ext_mbs_session_s {
    struct OpenAPI_mbs_session_id_s *mbs_session_id;
    bool is_tmgi_alloc_req;
    int tmgi_alloc_req;
    struct OpenAPI_tmgi_s *tmgi;
    char *expiration_time;
    OpenAPI_mbs_service_type_e service_type;
    bool is_location_dependent;
    int location_dependent;
    bool is_area_session_id;
    int area_session_id;
    bool is_ingress_tun_addr_req;
    int ingress_tun_addr_req;
    OpenAPI_list_t *ingress_tun_addr;
    struct OpenAPI_ssm_s *ssm;
    struct OpenAPI_mbs_service_area_s *mbs_service_area;
    struct OpenAPI_external_mbs_service_area_s *ext_mbs_service_area;
    char *dnn;
    struct OpenAPI_snssai_s *snssai;
    char *activation_time;
    char *start_time;
    char *termination_time;
    struct OpenAPI_mbs_service_info_s *mbs_serv_info;
    struct OpenAPI_mbs_session_subscription_s *mbs_session_subsc;
    OpenAPI_mbs_session_activity_status_e activity_status;
    bool is_any_ue_ind;
    int any_ue_ind;
    OpenAPI_list_t *mbs_fsa_id_list;
    struct OpenAPI_mbs_security_context_s *mbs_security_context;
    bool is_contact_pcf_ind;
    int contact_pcf_ind;
} OpenAPI_ext_mbs_session_t;

OpenAPI_ext_mbs_session_t *OpenAPI_ext_mbs_session_create(
    OpenAPI_mbs_session_id_t *mbs_session_id,
    bool is_tmgi_alloc_req,
    int tmgi_alloc_req,
    OpenAPI_tmgi_t *tmgi,
    char *expiration_time,
    OpenAPI_mbs_service_type_e service_type,
    bool is_location_dependent,
    int location_dependent,
    bool is_area_session_id,
    int area_session_id,
    bool is_ingress_tun_addr_req,
    int ingress_tun_addr_req,
    OpenAPI_list_t *ingress_tun_addr,
    OpenAPI_ssm_t *ssm,
    OpenAPI_mbs_service_area_t *mbs_service_area,
    OpenAPI_external_mbs_service_area_t *ext_mbs_service_area,
    char *dnn,
    OpenAPI_snssai_t *snssai,
    char *activation_time,
    char *start_time,
    char *termination_time,
    OpenAPI_mbs_service_info_t *mbs_serv_info,
    OpenAPI_mbs_session_subscription_t *mbs_session_subsc,
    OpenAPI_mbs_session_activity_status_e activity_status,
    bool is_any_ue_ind,
    int any_ue_ind,
    OpenAPI_list_t *mbs_fsa_id_list,
    OpenAPI_mbs_security_context_t *mbs_security_context,
    bool is_contact_pcf_ind,
    int contact_pcf_ind
);
void OpenAPI_ext_mbs_session_free(OpenAPI_ext_mbs_session_t *ext_mbs_session);
OpenAPI_ext_mbs_session_t *OpenAPI_ext_mbs_session_parseFromJSON(cJSON *ext_mbs_sessionJSON);
cJSON *OpenAPI_ext_mbs_session_convertToJSON(OpenAPI_ext_mbs_session_t *ext_mbs_session);
OpenAPI_ext_mbs_session_t *OpenAPI_ext_mbs_session_copy(OpenAPI_ext_mbs_session_t *dst, OpenAPI_ext_mbs_session_t *src);

#ifdef __cplusplus
}
#endif

#endif /* _OpenAPI_ext_mbs_session_H_ */

