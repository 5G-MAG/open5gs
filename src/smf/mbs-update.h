/*
 * 5G-MAG Reference Tools
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SMF_MBS_UPDATE_H
#define SMF_MBS_UPDATE_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The modification of an MBS session by an Nmbsmf_MBSSession Update (TS 29.532 V18.6.0 cl. 5.3.2.3.1): the
 * attributes of the representation the clause lets the consumer modify and this MB-SMF models are the MBS
 * service area, the MBS service information, the list of MBS FSA IDs, contactPcfInd and nrRedCapUeInfo. */

typedef enum smf_mbs_update_result_e {
    SMF_MBS_UPDATE_OK = 0,
    SMF_MBS_UPDATE_PATCH_FAILED,                /* a patch operation cannot be applied: 400 INVALID_MSG_FORMAT */
    SMF_MBS_UPDATE_ERROR_INPUT_PARAMETERS,      /* the result is erroneous: 400 ERROR_INPUT_PARAMETERS */
    SMF_MBS_UPDATE_INVALID_MBS_SERVICE_INFO,    /* 400 INVALID_MBS_SERVICE_INFO */
    SMF_MBS_UPDATE_FILTER_RESTRICTIONS_NOT_RESPECTED, /* 400 FILTER_RESTRICTIONS_NOT_RESPECTED */
    SMF_MBS_UPDATE_INSUFFICIENT_RESOURCES,      /* 500 INSUFFICIENT_RESOURCES */
} smf_mbs_update_result_e;

typedef struct smf_mbs_update_s {
    bool area_in_request;       /* the request modifies the MBS service area */
    bool area_changed;          /* the area the session runs with is another one */
    bool area_reduced;          /* the area of the request was reduced to the coverage of this MB-SMF */
    bool service_info_changed;
    char *serv_info_json;       /* smf_mbs_sess_update_check() only: the service information the request leaves, to be freed */
    bool fsa_changed;
    bool contact_pcf_ind_changed;
    smf_mbs_flow_change_t flow_change;
    bool n2_info_changed;       /* the N2 MBS Session Management information is another one: QoS flows, FSA IDs */
} smf_mbs_update_t;

/* Whether \p id is an MBS FSA ID: six hexadecimal digits (TS 29.571 V18.12.0 MbsFsaId, pattern '^[A-Fa-f0-9]{6}$') */
bool smf_mbs_fsa_id_is_valid(const char *id);

/* Whether the patch path names an attribute smf_mbs_sess_update() applies: the first segment is mbsServiceArea,
 * mbsServInfo, mbsFsaIdList, contactPcfInd or nrRedCapUeInfo. */
bool smf_mbs_update_path_is_modifiable(const char *path);

/* Applies the patch items to the representation of the MBS session and, if the result is acceptable, to the
 * session (area, flows with their user plane rules, FSA IDs, contactPcfInd). Nothing is changed when the result
 * is not SMF_MBS_UPDATE_OK; *detail then says why. patch_items is a list of OpenAPI_patch_item_t; those whose
 * path is not one smf_mbs_update_path_is_modifiable() accepts are left out. */
smf_mbs_update_result_e smf_mbs_sess_update(smf_mbs_sess_t *mbs_sess, OpenAPI_list_t *patch_items,
        smf_mbs_update_t *update, const char **detail);

/* As smf_mbs_sess_update(); with \p check_only nothing is applied (neither the session nor the user plane rules), and
 * of \p update only service_info_changed and serv_info_json are set. */
smf_mbs_update_result_e smf_mbs_sess_update_check(smf_mbs_sess_t *mbs_sess, OpenAPI_list_t *patch_items,
        smf_mbs_update_t *update, const char **detail, bool check_only);

/* RFC 6902 on a cJSON document. Returns NULL when every operation was applied, else a description of the first
 * that was not (the document is then partly modified). */
const char *smf_json_patch_apply(cJSON *doc, OpenAPI_list_t *patch_items);

/* The MBS service area of the OpenAPI model as the session holds it. Returns false when a PLMN is malformed. */
bool smf_mbs_service_area_from_openapi(OpenAPI_mbs_service_area_t *api_area, ogs_mbs_service_area_t **out);

/* The MBS QoS flows of the MBS service information: one for each media component with a QoS requirement, keyed
 * by the media component number, QFI and PDR not set; with no such component the default flow of 5QI 9. */
void smf_mbs_qos_flows_from_service_info(OpenAPI_mbs_service_info_t *mbs_serv_info, ogs_list_t *flows);

/* The cJSON document of a model and its text, NULL for no model. */
char *smf_mbs_service_area_to_json(OpenAPI_mbs_service_area_t *api_area);
char *smf_mbs_service_info_to_json(OpenAPI_mbs_service_info_t *mbs_serv_info);

#ifdef __cplusplus
}
#endif

#endif /* SMF_MBS_UPDATE_H */
