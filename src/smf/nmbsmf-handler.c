/*
 * 5G-MAG Reference Tools
 * Copyright (C) 2023 iTEAM UPV <borieher@iteam.upv.es>
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

#include "ogs-proto.h"

#include "sbi-path.h"
#include "pfcp-path.h"
#include "nmbsmf-handler.h"

static bool smf_nmbsmf_parse_tai(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message, ogs_tai_t *tai, OpenAPI_tai_t *api_tai);
static bool smf_nmbsmf_parse_ncgi(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message, ogs_ncgi_t *ncgi, OpenAPI_ncgi_t *api_ncgi);
static bool smf_nmbsmf_parse_geographic_area(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message, ogs_geographic_area_t *geog_area,
                                            OpenAPI_geographic_area_t *api_geog_area);
static bool smf_nmbsmf_parse_civic_address(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message, ogs_civic_address_t *civic_addr,
                                            OpenAPI_civic_address_t *api_civic_addr);

/* Nmbsmf_TMGI Service API */

/*
 * 3GPP TS 29.532 - Release 17.4.0
 * 5G System; 5G Multicast-Broadcast Session Management Services; Stage 3
 * Ch. 5.2.2.2 - Nmbsmf_TMGI Service API - TMGI Allocate service operation
 */
bool smf_nmbsmf_handle_tmgi_allocate(
    ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    // TODO (borieher): Not handling the 307 Temporary Redirect and 308 Permanent Redirect errors for now
    ogs_debug("TMGI allocate request received");

    OpenAPI_tmgi_allocate_t *TmgiAllocate = NULL;

    ogs_tmgi_t tmgi_received;
    ogs_tmgi_t *tmgi_found = NULL;
    ogs_tmgi_t *tmgi_allocated = NULL;

    OpenAPI_tmgi_allocated_t *TmgiAllocated = NULL;
    OpenAPI_list_t *tmgi_list = NULL;
    OpenAPI_lnode_t *node = NULL;
    OpenAPI_tmgi_t *Tmgi_received = NULL;
    OpenAPI_tmgi_t *Tmgi_copy = NULL;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(stream);
    ogs_assert(message);

    char *expiration_time = NULL;
    // TODO (borieher): How to get NID?
    char *nid = NULL;

    memset(&sendmsg, 0, sizeof(sendmsg));

    int rv = OGS_OK;

    TmgiAllocate = message->TmgiAllocate;

    if (!TmgiAllocate) {
        ogs_error("TMGI Allocate: No TmgiAllocate");
        // Extracted from the OpenAPI spec, not the 3GPP TS
        // TmgiAllocate must be present, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Bad Request", "Requested TMGI Allocate failed, no TmgiAllocate",  NMBSMF_TMGI_MANDATORY_IE_MISSING);
        rv = OGS_ERROR;
        goto cleanup;
    }

    if (!TmgiAllocate->tmgi_number && !TmgiAllocate->tmgi_list) {
        ogs_error("TMGI Allocate: no tmgi_number or tmgi_list present");
        // tmgi_number or tmgi_list should be present, send error (400 + MANDATORY_IE_MISSING)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Mandatory IE incorrect", "Requested TMGI Allocate failed, no [tmgiNumber] nor [tmgiList] present",
            NMBSMF_TMGI_MANDATORY_IE_MISSING);
        rv = OGS_ERROR;
        goto cleanup;
    }

    // Error checking for TmgiAllocate->tmgi_list
    if (TmgiAllocate->tmgi_list) {
        OpenAPI_list_for_each(TmgiAllocate->tmgi_list, node) {
            if (!node->data)
                continue;

            Tmgi_received = node->data;
            ogs_sbi_parse_tmgi(&tmgi_received, Tmgi_received);
            tmgi_found = smf_tmgi_find_by_tmgi(&tmgi_received);

            if (!tmgi_found) {
                ogs_error("TMGI Allocate: refresh error, TMGI not present");
                // TMGI not found, send error (404 + UNKNOWN_TMGI)
                ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
                    message, "Unknown TMGI", "Requested TMGI Allocate (refresh) failed, TMGI expired or cannot be found",
                    NMBSMF_TMGI_UNKNOWN_TMGI);
                rv = OGS_ERROR;
                goto cleanup;
            }
        }
    }

    // Error checking for TmgiAllocate->tmgi_number
    if (TmgiAllocate->tmgi_number) {
        if (TmgiAllocate->tmgi_number >= NMBSMF_TMGI_MIN_TMGI_NUMBER && \
                TmgiAllocate->tmgi_number <= NMBSMF_TMGI_MAX_TMGI_NUMBER) {

            // Check the number of TMGIs available
            if ((smf_tmgi_count() + TmgiAllocate->tmgi_number) > OGS_MAX_NUM_OF_TMGI) {
                ogs_error("TMGI Allocate: Cannot allocate %d TMGIs", TmgiAllocate->tmgi_number);
                // Custom error handling, not the 3GPP TS
                // Avoid reaching the maximum number of TMGI, send error (500)
                ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR,
                    message, "Forbidden", "Cannot allocate [tmgiNumber] of TMGIs", NMBSMF_TMGI_INSUFFICIENT_RESOURCES);
                rv = OGS_ERROR;
                goto cleanup;
            }
        } else {
            ogs_error("TMGI Allocate: allocate error, incorrect number in tmgi_number");
            // tmgi_number needs to be between 1 and 255, send error (400 + MANDATORY_IE_INCORRECT)
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                message, "Mandatory IE incorrect", "Requested TMGI Allocate failed, incorrect number in [tmgiNumber]",
                NMBSMF_TMGI_MANDATORY_IE_INCORRECT);
            rv = OGS_ERROR;
            goto cleanup;
        }
    }

    expiration_time = smf_tmgi_gen_expiration_time(OGS_DEFAULT_EXPIRATION_TIME_VALIDITY);

    tmgi_list = OpenAPI_list_create();

    // Perform the TMGI refresh operation for TmgiAllocate->tmgi_list
    if (TmgiAllocate->tmgi_list) {
        OpenAPI_list_for_each(TmgiAllocate->tmgi_list, node) {
            if (!node->data)
                continue;

            Tmgi_received = node->data;
            ogs_sbi_parse_tmgi(&tmgi_received, Tmgi_received);
            tmgi_found = smf_tmgi_find_by_tmgi(&tmgi_received);

            if (tmgi_found) {
                // TMGI present, refresh the expiration_time
                ogs_free(tmgi_found->expiration_time);
                tmgi_found->expiration_time = ogs_strdup(expiration_time);
            }
            // TODO (borieher): Add the refreshed TMGIs to the tmgi_list
        }
    }

    // Perform the TMGI allocate operation for TmgiAllocate->tmgi_number
    if (TmgiAllocate->tmgi_number) {
        int i;
        for (i = 0; i < TmgiAllocate->tmgi_number; i++) {
            tmgi_allocated = smf_tmgi_allocate(expiration_time);
            Tmgi_copy = ogs_sbi_build_tmgi(tmgi_allocated);
            OpenAPI_list_add(tmgi_list, Tmgi_copy);
        }
    }

    TmgiAllocated = OpenAPI_tmgi_allocated_create(tmgi_list, ogs_strdup(expiration_time), nid);

    /*********************************************************************
     * Send HTTP_STATUS_OK (/nmbsmf-tmgi/v1/tmgi) to the consumer NF
     *********************************************************************/

    sendmsg.TmgiAllocated = TmgiAllocated;

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_OK);

    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

cleanup:
    if (expiration_time)
        ogs_free(expiration_time);

    if (nid)
        ogs_free(nid);

    if (TmgiAllocated) {
        OpenAPI_tmgi_allocated_free(TmgiAllocated);
        tmgi_list = NULL;
    }

    if (rv == OGS_OK)
        return true;
    else
        return false;
}

/*
 * 3GPP TS 29.532 - Release 17.4.0
 * 5G System; 5G Multicast-Broadcast Session Management Services; Stage 3
 * Ch. 5.2.2.3 - Nmbsmf_TMGI Service API - TMGI Deallocate service operation
 */
bool smf_nmbsmf_handle_tmgi_deallocate(
    ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    // TODO (borieher): Not handling the 307 Temporary Redirect and 308 Permanent Redirect errors for now
    ogs_debug("TMGI deallocate request received");

    OpenAPI_list_t *tmgi_list = NULL;

    ogs_tmgi_t tmgi_received;
    ogs_tmgi_t *tmgi_found = NULL;

    OpenAPI_lnode_t *node = NULL;
    OpenAPI_tmgi_t *Tmgi_received = NULL;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(stream);
    ogs_assert(message);

    memset(&sendmsg, 0, sizeof(sendmsg));

    tmgi_list = message->param.tmgi_list;

    if (!tmgi_list) {
        // Extracted from the OpenAPI spec, not the 3GPP TS
        // tmgi_list not present, send error (400)
        ogs_error("TMGI Deallocate: No tmgi_list");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Bad Request", "Requested TMGI Deallocate failed, no tmgi-list", NMBSMF_TMGI_MANDATORY_IE_MISSING);
        return false;
    }

    // Error checking for tmgi_list
    OpenAPI_list_for_each(tmgi_list, node) {
        if (!node->data)
            continue;

        Tmgi_received = node->data;
        ogs_sbi_parse_tmgi(&tmgi_received, Tmgi_received);
        tmgi_found = smf_tmgi_find_by_tmgi(&tmgi_received);

        if (!tmgi_found) {
            ogs_error("TMGI Deallocate: deallocate error, TMGI not present");
            // TMGI not found, send error (404 + UNKNOWN_TMGI)
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
                message, "Unknown TMGI", "Requested TMGI Deallocate failed, TMGI expired or cannot be found",
                NMBSMF_TMGI_UNKNOWN_TMGI);
            return false;
        }
    }

    // Perform the TMGI dellocate operation for tmgi_list
    OpenAPI_list_for_each(tmgi_list, node) {
        if (!node->data)
            continue;

        Tmgi_received = node->data;
        ogs_sbi_parse_tmgi(&tmgi_received, Tmgi_received);
        tmgi_found = smf_tmgi_find_by_tmgi(&tmgi_received);

        // Protection against TMGI duplicates in tmgi_list
        if (tmgi_found)
            smf_tmgi_deallocate(tmgi_found);
    }

    /*********************************************************************
     * Send HTTP_STATUS_NO_CONTENT (/nmbsmf-tmgi/v1/tmgi) to the consumer NF
     *********************************************************************/

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);

    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return true;
}

/* Nmbsmf_MBSSession Service API */

/*
 * 3GPP TS 29.532 - Release 17.4.0
 * 5G System; 5G Multicast-Broadcast Session Management Services; Stage 3
 * Ch. 5.3.2.2 - Nmbsmf_MBSSession Service - MBS Session Create service operation
 */
bool smf_nmbsmf_handle_mbs_session_create(
    ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    // TODO (borieher): Not handling the 307 Temporary Redirect and 308 Permanent Redirect errors for now
    ogs_debug("MBS Session create request received");

    OpenAPI_create_req_data_t *CreateReqData = NULL;

    ogs_tmgi_t tmgi_received;
    ogs_tmgi_t *tmgi = NULL;
    ogs_ssm_t *ssm = NULL;
    smf_mbs_sess_t *mbs_sess = NULL;
    ogs_mbs_service_area_t *mbs_service_area = NULL;
    ogs_ext_mbs_service_area_t *ext_mbs_service_area = NULL;
    ogs_ncgi_t *ncgi = NULL;
    ogs_tai_t *tai = NULL;
    ogs_ncgi_tai_t *ncgi_tai = NULL;
    ogs_geographic_area_t *geog_area = NULL;
    ogs_civic_address_t *civic_addr = NULL;

    ogs_assert(stream);
    ogs_assert(message);

    char *expiration_time = NULL;
    // TODO (borieher): How to get NID?
    char *nid = NULL;
    char *service_type = NULL;

    bool is_multicast_service = false;

    int rv = OGS_OK;

    CreateReqData = message->CreateReqData;

    if (!CreateReqData) {
        ogs_error("MBS Session Create: No CreateReqData");
        // Extracted from the OpenAPI spec, not the 3GPP TS
        // CreateReqData must be present, send error (400 + MANDATORY_IE_MISSING)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Bad Request", "Requested MBS Session Create failed, no CreateReqData", NMBSMF_MBSSESSION_MANDATORY_IE_MISSING);
        rv = OGS_ERROR;
        goto cleanup;
    }

    if (!CreateReqData->mbs_session->service_type) {
        ogs_error("MBS Session Create: service_type not present");
        // service_type should be present, send error (400 + ERROR_INPUT_PARAMETERS)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Error input parameters", "MBS Session Create failed, no [serviceType] present",
            NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        rv = OGS_ERROR;
        goto cleanup;
    }

    service_type = ogs_strdup(OpenAPI_mbs_service_type_ToString(CreateReqData->mbs_session->service_type));

    // Check service_type is not NULL
    if (ogs_strcasecmp(service_type, "NULL") == 0) {
        ogs_error("MBS Session Create: service_type should be MULTICAST or BROADCAST");
        // Custom error handling, not the 3GPP TS
        // service_type should be MULTICAST or BROADCAST, send error (400 + ERROR_INPUT_PARAMETERS)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Error input parameters", "MBS Session Create failed, [serviceType] should be MULTICAST or BROADCAST",
            NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        rv = OGS_ERROR;
        goto cleanup;
    }

    // Error when mbs_session_id is not provided and tmgi_alloc_req is not provided or set to false
    if (!CreateReqData->mbs_session->mbs_session_id && (!CreateReqData->mbs_session->is_tmgi_alloc_req || CreateReqData->mbs_session->tmgi_alloc_req == 0)) {
        ogs_error("MBS Session Create: mbs_session_id or tmgi_alloc_req not present");
        // Custom error handling, not the 3GPP TS
        // mbs_session_id or tmgi_alloc_req should be present, send error (400 + ERROR_INPUT_PARAMETERS)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Error input parameters", "MBS Session Create failed, no [mbsSessionId] nor [tmgiAllocReq] present",
            NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        rv = OGS_ERROR;
        goto cleanup;
    }

    // Extract the SSM
    is_multicast_service = (ogs_strcasecmp(service_type, "MULTICAST") == 0);
    if (CreateReqData->mbs_session->mbs_session_id && CreateReqData->mbs_session->mbs_session_id->ssm && is_multicast_service) {
        ssm = ogs_malloc(sizeof(*ssm));
        ogs_assert(ssm);
        ogs_sbi_parse_ssm(ssm, CreateReqData->mbs_session->mbs_session_id->ssm);
    } else if (CreateReqData->mbs_session->ssm) {
        ssm = ogs_malloc(sizeof(*ssm));
        ogs_assert(ssm);
        ogs_sbi_parse_ssm(ssm, CreateReqData->mbs_session->ssm);
    }

    // Perform the TMGI allocate operation
    if (CreateReqData->mbs_session->is_tmgi_alloc_req && CreateReqData->mbs_session->tmgi_alloc_req > 0) {
        if (CreateReqData->mbs_session->mbs_session_id) {
            // For multicast, SSM can be provided as MBS Session ID. But TMGI must be allocated too
            if (CreateReqData->mbs_session->mbs_session_id->ssm) {
                if (!is_multicast_service) {
                    ogs_error("MBS Session Create: SSM as mbs_session_id and tmgi_alloc_req but service_type is not MULTICAST");
                    // Custom error handling, not the 3GPP TS
                    // SSM as mbs_session_id and tmgi_alloc_req should not be present if service_type is not MULTICAST, send error (400 + ERROR_INPUT_PARAMETERS)
                    ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                        message, "Error input parameters", "MBS Session Create failed, SSM as [mbsSessionId] and [tmgiAllocReq] both present but service_type is not MULTICAST",
                        NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
                    rv = OGS_ERROR;
                    goto cleanup;
                }
            } else if (CreateReqData->mbs_session->mbs_session_id->tmgi) {
                ogs_error("MBS Session Create: TMGI as mbs_session_id and tmgi_alloc_req both present");
                // Custom error handling, not the 3GPP TS
                // TMGI as mbs_session_id and tmgi_alloc_req should not be present at the same time, send error (400 + ERROR_INPUT_PARAMETERS)
                ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                    message, "Error input parameters", "MBS Session Create failed, TMGI as [mbsSessionId] and [tmgiAllocReq] both present",
                    NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
                rv = OGS_ERROR;
                goto cleanup;
            }
        }

        // Error checking, check the number of TMGIs available
        if (smf_tmgi_count() >= OGS_MAX_NUM_OF_TMGI) {
            ogs_error("MBS Session Create: Cannot allocate TMGI");
            // Custom error handling, not the 3GPP TS
            // Avoid reaching the maximum number of TMGI, send error (403)
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
                message, "Forbidden", "Cannot allocate TMGIs", NULL);
            rv = OGS_ERROR;
            goto cleanup;
        }

        // TMGI allocate
        expiration_time = smf_tmgi_gen_expiration_time(OGS_DEFAULT_EXPIRATION_TIME_VALIDITY);
        tmgi = smf_tmgi_allocate(expiration_time);
    }

    // Grab the provided TMGI as MBS Session ID
    if (!CreateReqData->mbs_session->is_tmgi_alloc_req || CreateReqData->mbs_session->tmgi_alloc_req == 0) {
        if (CreateReqData->mbs_session->mbs_session_id) {
            // When TMGI is already allocated, TMGI must be present either as MBS Session ID or separated
            if (CreateReqData->mbs_session->mbs_session_id->tmgi) {
                ogs_sbi_parse_tmgi(&tmgi_received, CreateReqData->mbs_session->mbs_session_id->tmgi);
                tmgi = smf_tmgi_find_by_tmgi(&tmgi_received);
            } else if (CreateReqData->mbs_session->mbs_session_id->ssm) {
                if (is_multicast_service) {
                    if (CreateReqData->mbs_session->tmgi) {
                        ogs_sbi_parse_tmgi(&tmgi_received, CreateReqData->mbs_session->tmgi);
                        tmgi = smf_tmgi_find_by_tmgi(&tmgi_received);
                    } else {
                        // Custom error handling, not the 3GPP TS
                        // No TMGI provided, send error (400 + ERROR_INPUT_PARAMETERS)
                        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                            message, "Error input parameters", "MBS Session Create failed, no TMGI provided",
                            NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
                        rv = OGS_ERROR;
                        goto cleanup;
                    }
                } else {
                    // Error SSM provided but service type is not MULTICAST (no TMGI provided)
                    ogs_error("MBS Session Create: SSM as mbs_session_id but service-type is not MULTICAST");
                    // Custom error handling, not the 3GPP TS
                    // SSM as mbs_session_id is only for MULTICAST service_type, send error (400 + ERROR_INPUT_PARAMETERS)
                    ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                        message, "Error input parameters", "MBS Session Create failed, SSM as [mbsSessionId] but [serviceType] is not MULTICAST",
                        NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
                    rv = OGS_ERROR;
                    goto cleanup;
                }
            }

            // Error checking, TMGI not found
            if (!tmgi) {
                // TMGI not found, send error (404 + UNKNOWN_TMGI)
                ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
                    message, "Unknown TMGI", "Requested MBS Session Create failed, TMGI provided expired or cannot be found",
                    NMBSMF_MBSSESSION_UNKNOWN_TMGI);
                rv = OGS_ERROR;
                goto cleanup;
            }
        }
    }

    // Extract MBS Service Area if present
    if (CreateReqData->mbs_session->mbs_service_area) {
        if (CreateReqData->mbs_session->mbs_service_area->ncgi_list) {
            OpenAPI_lnode_t *node;
            OpenAPI_list_for_each(CreateReqData->mbs_session->mbs_service_area->ncgi_list, node) {
                OpenAPI_lnode_t *cell_node;
                if (!mbs_service_area) mbs_service_area = (__typeof__(mbs_service_area))ogs_calloc(1, sizeof(*mbs_service_area));
                if (!mbs_service_area->ncgi_tai_list)
                    mbs_service_area->ncgi_tai_list =
                            (__typeof__(mbs_service_area->ncgi_tai_list))ogs_calloc(1, sizeof(*mbs_service_area->ncgi_tai_list));
                OpenAPI_ncgi_tai_t *api_ncgi_tai = (OpenAPI_ncgi_tai_t*)node->data;
                ncgi_tai = (ogs_ncgi_tai_t*)ogs_calloc(1, sizeof(*ncgi_tai));
                if (!smf_nmbsmf_parse_tai(stream, message, &ncgi_tai->tai, api_ncgi_tai->tai)) {
                    rv = OGS_ERROR;
                    goto cleanup;
                }
                OpenAPI_list_for_each(api_ncgi_tai->cell_list, cell_node) {
                    OpenAPI_ncgi_t *api_ncgi = (OpenAPI_ncgi_t*)node->data;
                    ncgi = (ogs_ncgi_t*)ogs_calloc(1, sizeof(*ncgi));
                    if (!smf_nmbsmf_parse_ncgi(stream, message, ncgi, api_ncgi)) {
                        rv = OGS_ERROR;
                        goto cleanup;
                    }
                    ogs_list_add(&ncgi_tai->cell_list, ncgi);
                    ncgi = NULL;
                }
                ogs_list_add(mbs_service_area->ncgi_tai_list, ncgi_tai);
                ncgi_tai = NULL;
            }
        }
        if (CreateReqData->mbs_session->mbs_service_area->tai_list) {
            OpenAPI_lnode_t *node;
            OpenAPI_list_for_each(CreateReqData->mbs_session->mbs_service_area->tai_list, node) {
                if (!mbs_service_area) mbs_service_area = (__typeof__(mbs_service_area))ogs_calloc(1, sizeof(*mbs_service_area));
                if (!mbs_service_area->tai_list)
                    mbs_service_area->tai_list =
                            (__typeof__(mbs_service_area->tai_list))ogs_calloc(1, sizeof(*mbs_service_area->tai_list));
                OpenAPI_tai_t *api_tai = (OpenAPI_tai_t*)node->data;
                tai = (ogs_tai_t*)ogs_calloc(1, sizeof(*tai));
                if (!smf_nmbsmf_parse_tai(stream, message, tai, api_tai)) {
                    rv = OGS_ERROR;
                    goto cleanup;
                }
                ogs_list_add(mbs_service_area->tai_list, tai);
                tai = NULL;
            }
        }
    }

    // Extract External MBS Service Area if present
    if (CreateReqData->mbs_session->ext_mbs_service_area) {
        if (CreateReqData->mbs_session->ext_mbs_service_area->geographic_area_list) {
            OpenAPI_lnode_t *node;
            OpenAPI_list_for_each(CreateReqData->mbs_session->ext_mbs_service_area->geographic_area_list, node) {
                if (!ext_mbs_service_area)
                    ext_mbs_service_area = (__typeof__(ext_mbs_service_area))ogs_calloc(1, sizeof(*ext_mbs_service_area));
                if (!ext_mbs_service_area->geographic_area_list)
                    ext_mbs_service_area->geographic_area_list =
                            (__typeof__(ext_mbs_service_area->geographic_area_list))ogs_calloc(1,
                                        sizeof(*ext_mbs_service_area->geographic_area_list));
                OpenAPI_geographic_area_t *api_geog_area = (OpenAPI_geographic_area_t*)node->data;
                geog_area = (ogs_geographic_area_t*)ogs_calloc(1, sizeof(*geog_area));
                if (!smf_nmbsmf_parse_geographic_area(stream, message, geog_area, api_geog_area)) {
                    rv = OGS_ERROR;
                    goto cleanup;
                }
                ogs_list_add(ext_mbs_service_area->geographic_area_list, geog_area);
                geog_area = NULL;
            }
        }
        if (CreateReqData->mbs_session->ext_mbs_service_area->civic_address_list) {
            OpenAPI_lnode_t *node;
            OpenAPI_list_for_each(CreateReqData->mbs_session->ext_mbs_service_area->civic_address_list, node) {
                if (!ext_mbs_service_area)
                    ext_mbs_service_area = (__typeof__(ext_mbs_service_area))ogs_calloc(1, sizeof(*ext_mbs_service_area));
                if (!ext_mbs_service_area->civic_address_list)
                    ext_mbs_service_area->civic_address_list =
                            (__typeof__(ext_mbs_service_area->civic_address_list))ogs_calloc(1,
                                        sizeof(*ext_mbs_service_area->civic_address_list));
                OpenAPI_civic_address_t *api_civic_addr = (OpenAPI_civic_address_t*)node->data;
                civic_addr = (ogs_civic_address_t*)ogs_calloc(1, sizeof(*civic_addr));
                if (!smf_nmbsmf_parse_civic_address(stream, message, civic_addr, api_civic_addr)) {
                    rv = OGS_ERROR;
                    goto cleanup;
                }
                ogs_list_add(ext_mbs_service_area->civic_address_list, civic_addr);
                civic_addr = NULL;
            }
        }
    }

    // TODO (borieher): Check provided TMGI is not added to an existing MBS Session

    // MBS Session create
    mbs_sess = smf_mbs_sess_create(tmgi, ssm, service_type, mbs_service_area, ext_mbs_service_area);
    tmgi = NULL; // tmgi passed to mbs_sess
    ssm = NULL; // ssm passed to mbs_sess
    mbs_service_area = NULL; // mbs_service_area passed to mbs_sess
    ext_mbs_service_area = NULL; // ext_mbs_service_area passed to mbs_sess

    if (!mbs_sess) {
        ogs_error("MBS Session Create: MBS Session Id collides with existing MBS Session");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
                        message, "Forbidden", "MBS Session Create failed, [mbsSessionId] has already been used in the same area",
                        NMBSMF_MBSSESSION_MBS_SESSION_ALREADY_CREATED);
        rv = OGS_ERROR;
        goto cleanup;
    }

    mbs_sess->ingress_tun_addr_req = (CreateReqData->mbs_session->is_ingress_tun_addr_req &&
                                      CreateReqData->mbs_session->ingress_tun_addr_req != 0);

    if (is_multicast_service) {
        mbs_sess->activity_status = CreateReqData->mbs_session->activity_status;
    }

    /*********************************************************************
     * Send PFCP N4mb Session Establishment Request to the UPF
     *********************************************************************/

    smf_mbs_sess_create_mbs_data_forwarding(mbs_sess);

    smf_5gc_pfcp_n4mb_send_session_establishment_request(mbs_sess, 0, stream);

cleanup:
    if (mbs_service_area)
        ogs_mbs_service_area_free(mbs_service_area);

    if (ext_mbs_service_area)
        ogs_ext_mbs_service_area_free(ext_mbs_service_area);

    if (ncgi)
        ogs_ncgi_free(ncgi);

    if (tai)
        ogs_tai_free(tai);

    if (ncgi_tai)
        ogs_ncgi_tai_free(ncgi_tai);

    if (geog_area)
        ogs_geographic_area_free(geog_area);

    if (civic_addr)
        ogs_civic_address_free(civic_addr);

    if (expiration_time)
        ogs_free(expiration_time);

    if (nid)
        ogs_free(nid);

    if (ssm)
        ogs_free(ssm);

    if (service_type)
        ogs_free(service_type);

    if (rv == OGS_OK)
        return true;
    else
        return false;
}

/*
 * 3GPP TS 29.532 - Release 17.4.0
 * 5G System; 5G Multicast-Broadcast Session Management Services; Stage 3
 * Ch. 5.3.2.4 - Nmbsmf_MBSSession Service - MBS Session Release service operation
 */
bool smf_nmbsmf_handle_mbs_session_release(
    smf_mbs_sess_t *mbs_sess, ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    // TODO (borieher): Not handling the 307 Temporary Redirect and 308 Permanent Redirect errors for now
    ogs_debug("MBS Session release request received");

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(stream);
    ogs_assert(message);
    ogs_assert(mbs_sess);

    // BUG FIX: confirmed live this session -- MBSF's own delete-cascade sends up to three duplicate
    // Nmbsmf_MBSSession Release requests for the exact same session (same instant, same resource), which
    // made everything below fire multiple times concurrently for the same still-live mbs_sess and is the
    // likely cause of a real SMF segfault observed live (confirmed via dmesg). Make this idempotent: only
    // the first release call actually triggers the AMF/UPF release chain; any duplicate just replies
    // 204 without touching mbs_sess again (it may already be mid-teardown, or freed, by the time a
    // duplicate is dispatched).
    if (mbs_sess->release_triggered) {
        ogs_warn("MBS Session release requested again for an already-releasing session -- ignoring "
                "duplicate (mbsSessionRef[%s])", mbs_sess->mbs_session_ref);
        memset(&sendmsg, 0, sizeof(sendmsg));
        response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
        ogs_assert(response);
        ogs_assert(true == ogs_sbi_server_send_response(stream, response));
        return true;
    }
    mbs_sess->release_triggered = true;

    // BUG FIX: this used to release the local smf_mbs_sess_t immediately (smf_mbs_sess_release()) without
    // ever telling the AMF or the UPF -- the release chain was a no-op beyond this process's own
    // bookkeeping. This is confirmed to be the root cause of the UPF's fixed-size MBS session pool
    // (OGS_MAX_NUM_OF_MBS_SESSIONS=20) filling up after repeated test session creation/deletion cycles.
    //
    // Fire both real release triggers now, while mbs_sess is still valid, mirroring how the create path
    // (smf_n4mb_handle_session_establishment_response()) already responds to its northbound caller before
    // its own follow-on Namf_MBSBroadcast create call completes. The local smf_mbs_sess_t itself is now
    // only freed once the UPF's N4mb Session Deletion Response actually arrives
    // (smf_n4mb_handle_session_deletion_response() in n4mb-handler.c), not eagerly here.

    // Release the AMF/NGAP broadcast context, if one was ever actually created for this session (a
    // Multicast session, or a Broadcast session that never got far enough to receive a mbsContextRef,
    // has nothing to release here).
    if (mbs_sess->mbs_context_ref) {
        int r = smf_sbi_old_discover_and_send(
                OGS_SBI_SERVICE_TYPE_NAMF_MBS_BC, NULL,
                smf_namf_build_mbs_broadcast_context_delete_request,
                mbs_sess, NULL, 0, NULL);
        if (r != OGS_OK)
            ogs_error("Failed to send MBS Broadcast ContextDelete for mbsContextRef[%s]",
                    mbs_sess->mbs_context_ref);
    }

    // Release the UPF-side N4mb PFCP session. If no PFCP peer was ever associated (e.g. the session
    // never got far enough to be established), there is nothing to tell the UPF -- release local state
    // directly instead of waiting for a response that will never come.
    if (mbs_sess->pfcp_node) {
        int r = smf_5gc_pfcp_n4mb_send_session_deletion_request(mbs_sess);
        if (r != OGS_OK) {
            ogs_error("Failed to send N4mb Session Deletion Request, releasing local state anyway");
            smf_mbs_sess_release(mbs_sess);
        }
    } else {
        smf_mbs_sess_release(mbs_sess);
    }

    /*********************************************************************
     * Send HTTP_STATUS_NO_CONTENT (/nmbsmf-mbssession/v1/mbs-sessions) to the consumer NF
     *********************************************************************/

    memset(&sendmsg, 0, sizeof(sendmsg));

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);

    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return true;
}

/*
 * 3GPP TS 29.532 - Release 17.4.0
 * 5G System; 5G Multicast-Broadcast Session Management Services; Stage 3
 * Ch. 5.3.2.3 - Nmbsmf_MBSSession Service - MBS Session Update service operation
 */
bool smf_nmbsmf_handle_mbs_session_patch(smf_mbs_sess_t *mbs_sess,
                                         ogs_sbi_stream_t *stream,
                                         ogs_sbi_message_t *message)
{
    // TODO (davidjw): Not handling the 307 Temporary Redirect and 308 Permanent Redirect errors for now
    ogs_debug("MBS Session update request received");

    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(stream);
    ogs_assert(message);
    ogs_assert(mbs_sess);

    if (!message->PatchItemList) {
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                    message, "Missing patch list", "Requested MBS Session Update failed, no patch list provided",
                    "INVALID_MSG_FORMAT");
        return false;
    }

    OpenAPI_lnode_t *node;
    OpenAPI_list_for_each(message->PatchItemList, node) {
        OpenAPI_patch_item_t *patch_item = (OpenAPI_patch_item_t*)node->data;
        if (!patch_item->path || !patch_item->op) {
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                    message, "Malformed patch list", "Requested MBS Session Update failed, patch list malformed",
                    "INVALID_MSG_FORMAT");
            return false;
        }
        SWITCH(patch_item->path)
        CASE("/activityStatus")
            if (strcmp(mbs_sess->service_type, "MULTICAST")) {
                ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN, message, "Update forbidden",
                        "Requested MBS Session Update failed, activityStatus can only be updated for multicast services",
                        "MODIFICATION_NOT_ALLOWED");
                return false;
            }
            mbs_sess->activity_status = OpenAPI_mbs_session_activity_status_FromString(cJSON_GetStringValue(patch_item->value->json));
            break;
        CASE("/mbsSessionId")
        CASE("/mbsSessionId/tmgi")
        CASE("/mbsSessionId/tmgi/mbsServiceId")
        CASE("/mbsSessionId/tmgi/plmnId")
        CASE("/mbsSessionId/tmgi/plmnId/mcc")
        CASE("/mbsSessionId/tmgi/plmnId/mnc")
        CASE("/mbsSessionId/ssm/sourceIpAddr")
        CASE("/mbsSessionId/ssm/sourceIpAddr/ipv4Addr")
        CASE("/mbsSessionId/ssm/sourceIpAddr/ipv6Addr")
        CASE("/mbsSessionId/ssm/sourceIpAddr/ipv6Prefix")
        CASE("/mbsSessionId/ssm/destIpAddr")
        CASE("/mbsSessionId/ssm/destIpAddr/ipv4Addr")
        CASE("/mbsSessionId/ssm/destIpAddr/ipv6Addr")
        CASE("/mbsSessionId/ssm/destIpAddr/ipv6Prefix")
        CASE("/mbsSessionId/nid")
        CASE("/tmgiAllocReq")
        CASE("/tmgi")
        CASE("/tmgi/mbsServiceId")
        CASE("/tmgi/plmnId")
        CASE("/tmgi/plmnId/mcc")
        CASE("/tmgi/plmnId/mnc")
        CASE("/expirationTime")
        CASE("/serviceType")
        CASE("/locationDependent")
        CASE("/areaSessionId")
        CASE("/ingressTunReqAddr")
        CASE("/ingressTunAddr")
        CASE("/ssm")
        CASE("/ssm/sourceIpAddr")
        CASE("/ssm/sourceIpAddr/ipv4Addr")
        CASE("/ssm/sourceIpAddr/ipv6Addr")
        CASE("/ssm/sourceIpAddr/ipv6Prefix")
        CASE("/ssm/destIpAddr")
        CASE("/ssm/destIpAddr/ipv4Addr")
        CASE("/ssm/destIpAddr/ipv6Addr")
        CASE("/ssm/destIpAddr/ipv6Prefix")
        CASE("/dnn")
        CASE("/snssai")
        CASE("/snssai/sst")
        CASE("/snssai/sd")
        CASE("/startTime")
        CASE("/terminationTime")
        CASE("/mbsSessionSubsc")
        CASE("/mbsSessionSubsc/mbsSessionId")
        CASE("/mbsSessionSubsc/mbsSessionId/tmgi")
        CASE("/mbsSessionSubsc/mbsSessionId/tmgi/mbsServiceId")
        CASE("/mbsSessionSubsc/mbsSessionId/tmgi/plmnId")
        CASE("/mbsSessionSubsc/mbsSessionId/tmgi/plmnId/mcc")
        CASE("/mbsSessionSubsc/mbsSessionId/tmgi/plmnId/mnc")
        CASE("/mbsSessionSubsc/mbsSessionId/ssm/sourceIpAddr")
        CASE("/mbsSessionSubsc/mbsSessionId/ssm/sourceIpAddr/ipv4Addr")
        CASE("/mbsSessionSubsc/mbsSessionId/ssm/sourceIpAddr/ipv6Addr")
        CASE("/mbsSessionSubsc/mbsSessionId/ssm/sourceIpAddr/ipv6Prefix")
        CASE("/mbsSessionSubsc/mbsSessionId/ssm/destIpAddr")
        CASE("/mbsSessionSubsc/mbsSessionId/ssm/destIpAddr/ipv4Addr")
        CASE("/mbsSessionSubsc/mbsSessionId/ssm/destIpAddr/ipv6Addr")
        CASE("/mbsSessionSubsc/mbsSessionId/ssm/destIpAddr/ipv6Prefix")
        CASE("/anyUeInd")
        CASE("/mbsSecurityContext")
        CASE("/mbsSecurityContext/keyList")
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN, message, "Update forbidden",
                    "Requested MBS Session Update failed, update not allowed for one or more field paths",
                    "MODIFICATION_NOT_ALLOWED");
            return false;
        CASE("/mbsServiceArea")
        CASE("/mbsServiceArea/ncgiTaiList")
        CASE("/mbsServiceArea/taiList")
        CASE("/extMbsServiceArea")
        CASE("/extMbsServiceArea/geographicAreaList")
        CASE("/extMbsServiceArea/civicAddressList")
        CASE("/mbsServInfo")
        CASE("/mbsServInfo/mbsMediaComps")
        CASE("/mbsServInfo/mbsSdfResPrio")
        CASE("/mbsServInfo/afAppId")
        CASE("/mbsServInfo/mbsSessionAmbr")
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_IMPLEMENTED, message, "Update not implemented",
                    "Requested MBS Session Update failed, update for requested field path is not implemented", NULL);
            return false;
        DEFAULT
            /* handle array paths */
            if (!strncmp(patch_item->path, "/mbsServiceArea/ncgiTaiList/", 28) ||
                !strncmp(patch_item->path, "/mbsServiceArea/taiList/", 24) ||
                !strncmp(patch_item->path, "/extMbsServiceArea/geographicAreaList/", 38) ||
                !strncmp(patch_item->path, "/extMbsServiceArea/civicAddressList", 35) ||
                !strncmp(patch_item->path, "/mbsServInfo/mbsMediaComps/", 27)) {
                ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_IMPLEMENTED, message, "Update not implemented",
                        "Requested MBS Session Update failed, update for requested field path is not implemented", NULL);
                return false;
            } else {
                ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST, message, "Update path not found",
                        "Requested MBS Session Update failed, patch path not recognised", "INVALID_MSG_FORMAT");
                return false;
            }
        END
    }
#if 1
    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
#else /* for Rel 18 we can return 200 + UpdateRspData JSON */
    OpenAPI_mbs_session_id_t *mbs_session_id = NULL;
    OpenAPI_tmgi_t *tmgi = NULL;
    char *expiration_time = NULL;
    OpenAPI_mbs_service_type_e service_type = OpenAPI_mbs_service_type_NULL;
    OpenAPI_list_t *ingress_tun_addr = NULL;
    OpenAPI_ssm_t *ssm = NULL;
    char *dnn = NULL;
    OpenAPI_snssai_t *snssai = NULL;
    OpenAPI_mbs_session_activity_status_e activity_status = OpenAPI_mbs_session_activity_status_NULL;
    bool is_any_ue_ind = false;
    int any_ue_ind = 0;

    /* TODO (davidjw): fill in mbs session fields from mbs_sess */

    sendmsg.UpdateRspData = OpenAPI_update_rsp_data_create(
            OpenAPI_ext_mbs_session_create(mbs_session_id, /* mbsSessionId */
                                           false, 0, /* tmgiAllocReq */
                                           tmgi, /* tmgi */
                                           expiration_time, /* expirationTime */
                                           service_type, /* serviceType */
                                           false, 0, /* locationDependent */
                                           false, 0, /* areaSessionId */
                                           false, 0, /* ingressTunAddrReq */
                                           ingress_tun_addr, /* ingressTunAddr */
                                           ssm, /* ssm */
                                           NULL, /* mbsServiceArea */
                                           NULL, /* extMbsServiceArea */
                                           dnn, /* dnn */
                                           snssai, /* snssai */
                                           NULL, /* activationTime */
                                           NULL, /* startTime */
                                           NULL, /* terminationTime */
                                           NULL, /* mbsServInfo */
                                           NULL, /* mbsSessionSubsc */
                                           activity_status, /* activityStatus */
                                           is_any_ue_ind, any_ue_ind,
                                           NULL, /* mbsFsaIdList */
                                           NULL, /* mbsSecurityContext */
                                           false, 0 /* contactPcfInd */
                                          ));
    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_OK);
#endif

    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return true;
}

static bool smf_nmbsmf_parse_tai(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message, ogs_tai_t *tai, OpenAPI_tai_t *api_tai)
{
    if (!ogs_sbi_parse_plmn_id(&tai->plmn_id, api_tai->plmn_id)) {
        ogs_error("Parse TAI: Unable to parse the PLMN Id");
        // bad plmn id, send error (400 + ERROR_INPUT_PARAMETERS)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Bad PLMN Id", "MBS Session: TAI: Unable to parse the PLMN Id",
            NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        return false;
    }
    if (api_tai->tac) tai->tac = ogs_strdup(api_tai->tac);
    if (api_tai->nid) tai->nid = ogs_strdup(api_tai->nid);
    return true;
}

static bool smf_nmbsmf_parse_ncgi(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message, ogs_ncgi_t *ncgi, OpenAPI_ncgi_t *api_ncgi)
{
    if (!ogs_sbi_parse_plmn_id(&ncgi->plmn_id, api_ncgi->plmn_id)) {
        ogs_error("Parse NCGI: Unable to parse the PLMN Id");
        // bad plmn id, send error (400 + ERROR_INPUT_PARAMETERS)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Bad PLMN Id", "MBS Session: NCGI: Unable to parse the PLMN Id",
            NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        return false;
    }
    if (api_ncgi->nr_cell_id) ncgi->nr_cell_id = ogs_strdup(api_ncgi->nr_cell_id);
    if (api_ncgi->nid) ncgi->nid = ogs_strdup(api_ncgi->nid);
    return true;
}

static bool smf_nmbsmf_parse_civic_address(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message, ogs_civic_address_t *civic_addr,
                                            OpenAPI_civic_address_t *api_civic_addr)
{
    if (api_civic_addr->country) civic_addr->country = ogs_strdup(api_civic_addr->country);
    if (api_civic_addr->a1) civic_addr->a[0] = ogs_strdup(api_civic_addr->a1);
    if (api_civic_addr->a2) civic_addr->a[1] = ogs_strdup(api_civic_addr->a2);
    if (api_civic_addr->a3) civic_addr->a[2] = ogs_strdup(api_civic_addr->a3);
    if (api_civic_addr->a4) civic_addr->a[3] = ogs_strdup(api_civic_addr->a4);
    if (api_civic_addr->a5) civic_addr->a[4] = ogs_strdup(api_civic_addr->a5);
    if (api_civic_addr->a6) civic_addr->a[5] = ogs_strdup(api_civic_addr->a6);
    if (api_civic_addr->prd) civic_addr->prd = ogs_strdup(api_civic_addr->prd);
    if (api_civic_addr->pod) civic_addr->pod = ogs_strdup(api_civic_addr->pod);
    if (api_civic_addr->sts) civic_addr->sts = ogs_strdup(api_civic_addr->sts);
    if (api_civic_addr->hno) civic_addr->hno = ogs_strdup(api_civic_addr->hno);
    if (api_civic_addr->hns) civic_addr->hns = ogs_strdup(api_civic_addr->hns);
    if (api_civic_addr->lmk) civic_addr->lmk = ogs_strdup(api_civic_addr->lmk);
    if (api_civic_addr->loc) civic_addr->loc = ogs_strdup(api_civic_addr->loc);
    if (api_civic_addr->nam) civic_addr->nam = ogs_strdup(api_civic_addr->nam);
    if (api_civic_addr->pc) civic_addr->pc = ogs_strdup(api_civic_addr->pc);
    if (api_civic_addr->bld) civic_addr->bld = ogs_strdup(api_civic_addr->bld);
    if (api_civic_addr->unit) civic_addr->unit = ogs_strdup(api_civic_addr->unit);
    if (api_civic_addr->flr) civic_addr->flr = ogs_strdup(api_civic_addr->flr);
    if (api_civic_addr->room) civic_addr->room = ogs_strdup(api_civic_addr->room);
    if (api_civic_addr->plc) civic_addr->plc = ogs_strdup(api_civic_addr->plc);
    if (api_civic_addr->pcn) civic_addr->pcn = ogs_strdup(api_civic_addr->pcn);
    if (api_civic_addr->pobox) civic_addr->pobox = ogs_strdup(api_civic_addr->pobox);
    if (api_civic_addr->addcode) civic_addr->addcode = ogs_strdup(api_civic_addr->addcode);
    if (api_civic_addr->seat) civic_addr->seat = ogs_strdup(api_civic_addr->seat);
    if (api_civic_addr->rd) civic_addr->rd = ogs_strdup(api_civic_addr->rd);
    if (api_civic_addr->rdsec) civic_addr->rdsec = ogs_strdup(api_civic_addr->rdsec);
    if (api_civic_addr->rdbr) civic_addr->rdbr = ogs_strdup(api_civic_addr->rdbr);
    if (api_civic_addr->rdsubbr) civic_addr->rdsubbr = ogs_strdup(api_civic_addr->rdsubbr);
    if (api_civic_addr->prm) civic_addr->prm = ogs_strdup(api_civic_addr->prm);
    if (api_civic_addr->pom) civic_addr->pom = ogs_strdup(api_civic_addr->pom);
    if (api_civic_addr->usage_rules) civic_addr->usage_rules = ogs_strdup(api_civic_addr->usage_rules);
    if (api_civic_addr->method) civic_addr->method = ogs_strdup(api_civic_addr->method);
    if (api_civic_addr->provided_by) civic_addr->provided_by = ogs_strdup(api_civic_addr->provided_by);

    return true;
}

static bool smf_nmbsmf_parse_geographic_area(ogs_sbi_stream_t *stream, ogs_sbi_message_t *message, ogs_geographic_area_t *geog_area,
                                            OpenAPI_geographic_area_t *api_geog_area)
{
    /* TODO (David Waring): OpenAPI type for geographic area is incomplete, so ignore until the templates are fixed */
    /* For now just create a point at 0,0 */
    geog_area->shape = ogs_supported_gad_shape_POINT;
    geog_area->point.point.lon = 0.0;
    geog_area->point.point.lat = 0.0;

    return true;
}
