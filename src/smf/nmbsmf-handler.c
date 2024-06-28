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

#include "sbi-path.h"
#include "pfcp-path.h"
#include "nmbsmf-handler.h"

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

    int rv = OGS_OK;

    TmgiAllocate = message->TmgiAllocate;

    if (!TmgiAllocate) {
        ogs_error("TMGI Allocate: No TmgiAllocate");
        // Extracted from the OpenAPI spec, not the 3GPP TS
        // TmgiAllocate must be present, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Bad Request", "Requested TMGI Allocate failed, no TmgiAllocate",  NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    if (!TmgiAllocate->tmgi_number && !TmgiAllocate->tmgi_list) {
        ogs_error("TMGI Allocate: tmgi_number and tmgi_list not present");
        // tmgi_number or tmgi_list should be present, send error (403 + MANDATORY_IE_INCORRECT)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
            message, "Mandatory IE incorrect", "Requested TMGI Allocate failed, no [tmgiNumber] nor [tmgiList] present",
            NMBSMF_TMGI_MANDATORY_IE_INCORRECT);
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
                // Avoid reaching the maximum number of TMGI, send error (403)
                ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
                    message, "Forbidden", "Cannot allocate [tmgiNumber] of TMGIs", NULL);
                rv = OGS_ERROR;
                goto cleanup;
            }
        } else {
            ogs_error("TMGI Allocate: allocate error, incorrect number in tmgi_number");
            // tmgi_number needs to be between 1 and 255, send error (403 + MANDATORY_IE_INCORRECT)
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
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

    memset(&sendmsg, 0, sizeof(sendmsg));

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

    tmgi_list = message->param.tmgi_list;

    if (!tmgi_list) {
        // Extracted from the OpenAPI spec, not the 3GPP TS
        // tmgi_list not present, send error (400)
        ogs_error("TMGI Deallocate: No tmgi_list");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Bad Request", "Requested TMGI Deallocate failed, no tmgi-list", NULL);
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

    memset(&sendmsg, 0, sizeof(sendmsg));

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
    ogs_ssm_t ssm_received;
    ogs_tmgi_t *tmgi = NULL;
    ogs_ssm_t *ssm = NULL;
    smf_mbs_sess_t *mbs_sess = NULL;

    /* TODO (borieher): Move this to a different place after PFCP and the AMF request are done */
    OpenAPI_create_rsp_data_t *CreateRspData = NULL;
    OpenAPI_tmgi_t *Tmgi = NULL;
    OpenAPI_tmgi_t *Tmgi_copy = NULL;
    OpenAPI_ssm_t * Ssm = NULL;
    OpenAPI_ssm_t * Ssm_copy = NULL;
    OpenAPI_mbs_session_id_t *Mbs_session_id = NULL;
    OpenAPI_mbs_service_type_e Mbs_service_type = OpenAPI_mbs_service_type_NULL;
    OpenAPI_ext_mbs_session_t *Ext_mbs_session = NULL;
    /* */

    ogs_sbi_message_t sendmsg;
    ogs_sbi_server_t *server = NULL;
    ogs_sbi_header_t header;
    ogs_sbi_response_t *response = NULL;

    ogs_assert(stream);
    ogs_assert(message);

    char *expiration_time = NULL;
    // TODO (borieher): How to get NID?
    char *nid = NULL;
    char *service_type = NULL;

    int rv = OGS_OK;

    CreateReqData = message->CreateReqData;

    if (!CreateReqData) {
        ogs_error("MBS Session Create: No CreateReqData");
        // Extracted from the OpenAPI spec, not the 3GPP TS
        // CreateReqData must be present, send error (400)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Bad Request", "Requested MBS Session Create failed, no CreateReqData", NULL);
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
    if ((!CreateReqData->mbs_session->mbs_session_id && CreateReqData->mbs_session->tmgi_alloc_req <= 0) || \
            (!CreateReqData->mbs_session->mbs_session_id && !CreateReqData->mbs_session->tmgi_alloc_req)) {
        ogs_error("MBS Session Create: mbs_session_id or tmgi_alloc_req not present");
        // Custom error handling, not the 3GPP TS
        // mbs_session_id or tmgi_alloc_req should be present, send error (400 + ERROR_INPUT_PARAMETERS)
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Error input parameters", "MBS Session Create failed, no [mbsSessionId] nor [tmgiAllocReq] present",
            NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
        rv = OGS_ERROR;
        goto cleanup;
    }

    // Perform the TMGI allocate operation
    if (CreateReqData->mbs_session->tmgi_alloc_req > 0) {
        if (CreateReqData->mbs_session->mbs_session_id) {
            // For multicast, SSM can be provided as MBS Session ID. But TMGI must be allocated too
            if (CreateReqData->mbs_session->mbs_session_id->ssm) {
                if (ogs_strcasecmp(service_type, "MULTICAST") == 0) {
                    // TODO (borieher): Check SSM exists
                    ogs_sbi_parse_ssm(&ssm_received, CreateReqData->mbs_session->mbs_session_id->ssm);
                    ssm = ogs_malloc(sizeof(ogs_ssm_t));
                    memcpy(ssm, &ssm_received, sizeof(ogs_ssm_t));
                    // Continue with the TMGI allocate
                } else {
                    ogs_error("MBS Session Create: SSM as mbs_session_id and tmgi_alloc_req but service_type is not MULTICAST");
                    // Custom error handling, not the 3GPP TS
                    // SSM as mbs_session_id and tmgi_alloc_req should not be present if service_type is not MULTICAST, send error (400 + ERROR_INPUT_PARAMETERS)
                    ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                        message, "Error input parameters", "MBS Session Create failed, SSM as [mbsSessionId] and [tmgiAllocReq] both present but service_type is not MULTICAST",
                        NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS);
                    rv = OGS_ERROR;
                    goto cleanup;
                }
            } else {
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
    if (!CreateReqData->mbs_session->tmgi_alloc_req || CreateReqData->mbs_session->tmgi_alloc_req <= 0) {
        if (CreateReqData->mbs_session->mbs_session_id) {
            // When TMGI is already allocated, TMGI must be present either as MBS Session ID or separated
            if (CreateReqData->mbs_session->mbs_session_id->tmgi) {
                ogs_sbi_parse_tmgi(&tmgi_received, CreateReqData->mbs_session->mbs_session_id->tmgi);
                tmgi = smf_tmgi_find_by_tmgi(&tmgi_received);
            } else if (CreateReqData->mbs_session->mbs_session_id->ssm) {
                if (ogs_strcasecmp(service_type, "MULTICAST") == 0) {
                    if (CreateReqData->mbs_session->tmgi) {
                        ogs_sbi_parse_tmgi(&tmgi_received, CreateReqData->mbs_session->tmgi);
                        tmgi = smf_tmgi_find_by_tmgi(&tmgi_received);
                        // TODO (borieher): Check SSM exists
                        ogs_sbi_parse_ssm(&ssm_received, CreateReqData->mbs_session->mbs_session_id->ssm);
                        ssm = ogs_malloc(sizeof(ogs_ssm_t));
                        memcpy(ssm, &ssm_received, sizeof(ogs_ssm_t));
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

    // TODO (borieher): Check provided TMGI is not added to an existing MBS Session

    // Parse the outer SSM field in the request
    if (CreateReqData->mbs_session->ssm) {
        // TODO (borieher): Check SSM exists
        // TODO (borieher): Compare CreateReqData->mbs_session->ssm and CreateReqData->mbs_session->mbs_session_id->ssm
        ogs_sbi_parse_ssm(&ssm_received, CreateReqData->mbs_session->ssm);
        if (!ssm)
            ssm = ogs_malloc(sizeof(ogs_ssm_t));
        memcpy(ssm, &ssm_received, sizeof(ogs_ssm_t));
    }

    // MBS Session create
    mbs_sess = smf_mbs_sess_create(tmgi, ssm, service_type);

    /*********************************************************************
     * Send PFCP N4mb Session Establishment Request to the UPF
     *********************************************************************/

    smf_mbs_sess_create_mbs_data_forwarding(mbs_sess);

    smf_5gc_pfcp_n4mb_send_session_establishment_request(mbs_sess, 0);

    // NOTE (borieher): Currently the response is right after the request, but in the call flow is after the PFCP Session Establishment
    //                  separate this in request and response

    Tmgi = ogs_sbi_build_tmgi(mbs_sess->tmgi);
    if (mbs_sess->mbs_session_id.is_tmgi) {
        Tmgi_copy = OpenAPI_tmgi_copy(Tmgi_copy, Tmgi);
        Mbs_session_id = OpenAPI_mbs_session_id_create(Tmgi_copy, NULL, mbs_sess->mbs_session_id.nid);
    }

    if (mbs_sess->mbs_session_id.is_ssm) {
        Ssm = ogs_sbi_build_ssm(mbs_sess->mbs_session_id.ssm);
        Ssm_copy = OpenAPI_ssm_copy(Ssm_copy, Ssm);
        Mbs_session_id = OpenAPI_mbs_session_id_create(NULL, Ssm_copy, nid);
    }

    Mbs_service_type = OpenAPI_mbs_service_type_FromString(mbs_sess->service_type);

    Ext_mbs_session = OpenAPI_ext_mbs_session_create(Mbs_session_id, NULL, 0, Tmgi, NULL, Mbs_service_type,
        NULL, 0, NULL, 0, NULL, 0, NULL, Ssm, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        OpenAPI_mbs_session_activity_status_NULL, NULL, 0, NULL, NULL, NULL, 0);

    CreateRspData = OpenAPI_create_rsp_data_create(Ext_mbs_session, NULL);

    // TODO (borieher): Check the TMGIs in the already created MBS Sessions to avoid collisions

    /*********************************************************************
     * Send OGS_SBI_HTTP_STATUS_CREATED (/nmbsmf-mbssession/v1/mbs-sessions) to the consumer NF
     *********************************************************************/

    memset(&sendmsg, 0, sizeof(sendmsg));

    server = ogs_sbi_server_from_stream(stream);
    ogs_assert(server);

    // Adding the mbsSessionRef in the headers for the created resource
    memset(&header, 0, sizeof(header));
    header.service.name = (char *) OGS_SBI_SERVICE_NAME_NMBSMF_MBS_SESSION;
    header.api.version = (char *) OGS_SBI_API_V1;
    header.resource.component[0] =
        (char *) OGS_SBI_RESOURCE_NAME_MBS_SESSIONS;
    header.resource.component[1] = mbs_sess->mbs_session_ref;

    sendmsg.http.location = ogs_sbi_server_uri(server, &header);
    ogs_assert(sendmsg.http.location);

    sendmsg.CreateRspData = CreateRspData;

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_CREATED);

    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

cleanup:
    if (expiration_time)
        ogs_free(expiration_time);

    if (nid)
        ogs_free(nid);

    if (service_type)
        ogs_free(service_type);

    if (CreateRspData)
        OpenAPI_create_rsp_data_free(CreateRspData);

    if (sendmsg.http.location)
        ogs_free(sendmsg.http.location);

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

    // TODO (borieher): Send here the MBS Broadcast Context Release (handle the response and perform the PFCP Release)

    // NOTE (borieher): Currently the response is right after the request, but in the call flow is after the PFCP Session Deletion
    //                  separate this in request and response

    // MBS Session release
    smf_mbs_sess_release(mbs_sess);

    /*********************************************************************
     * Send HTTP_STATUS_NO_CONTENT (/nmbsmf-mbssession/v1/mbs-sessions) to the consumer NF
     *********************************************************************/

    memset(&sendmsg, 0, sizeof(sendmsg));

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);

    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(stream, response));

    return true;
}