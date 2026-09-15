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
        /* The ceiling is the smaller of what the attribute may carry and what this MB-SMF could ever
         * hold: a tmgiNumber above either is a request that no amount of waiting would satisfy, which
         * is what TS 29.532 V18.6.0 table 6.1.3.2.3.1-3 answers with 403 MANDATORY_IE_INCORRECT in the
         * else branch below. Being merely unsatisfiable right now is a different answer, handled inside. */
        if (TmgiAllocate->tmgi_number >= NMBSMF_TMGI_MIN_TMGI_NUMBER &&
                TmgiAllocate->tmgi_number <=
                    ogs_min(NMBSMF_TMGI_MAX_TMGI_NUMBER, OGS_MAX_NUM_OF_TMGI)) {

            // Check the number of TMGIs available, after releasing any whose advertised
            // expiration time has passed -- see smf_tmgi_reclaim_expired().
            if ((smf_tmgi_count() + TmgiAllocate->tmgi_number) > OGS_MAX_NUM_OF_TMGI)
                smf_tmgi_reclaim_expired();

            if ((smf_tmgi_count() + TmgiAllocate->tmgi_number) > OGS_MAX_NUM_OF_TMGI) {
                ogs_error("TMGI Allocate: Cannot allocate %d TMGIs", TmgiAllocate->tmgi_number);
                /* 500, not 403: the request is within the ceiling checked above, so it is a valid request
                 * that cannot be met at this moment, and repeating it later may succeed once TMGIs are
                 * released. TS 29.532 V18.6.0 table 6.1.3.2.3.1-3 lists no application error for this
                 * case, so the status comes from the common table.
                 *
                 * TS 29.500 V18.10.0 table 5.2.7.2-1, row INSUFFICIENT_RESOURCES, gives "500 Internal
                 * Server Error" for "The request is rejected due to insufficient resources." */
                ogs_sbi_server_send_error(stream,
                    OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR,
                    message, "Insufficient resources", "Cannot allocate [tmgiNumber] of TMGIs",
                    NMBSMF_TMGI_INSUFFICIENT_RESOURCES);
                rv = OGS_ERROR;
                goto cleanup;
            }
        } else {
            ogs_error("TMGI Allocate: allocate error, incorrect number in tmgi_number");
            /* 403 Forbidden, not 400: TS 29.532 V18.6.0 table 6.1.3.2.3.1-3, the POST /tmgi response
             * table, maps MANDATORY_IE_INCORRECT, "if the required TMGI number for TMGI allocation is
             * not valid," to 403 Forbidden. */
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

                                // Refreshed TMGIs go into the outgoing tmgi_list too. TmgiAllocated.tmgiList is required
                                // with minItems: 1, and TS 29.532 Table 6.1.3.2.3.1-3 says the response "shall contain the
                                // list of the TMGI(s) and their new expiration time" for refresh flows as well, so a
                                // pure-refresh request carrying no tmgiNumber would otherwise return an empty, schema-
                                // invalid list and drop the refresh confirmation.
                Tmgi_copy = ogs_sbi_build_tmgi(tmgi_found);
                OpenAPI_list_add(tmgi_list, Tmgi_copy);
            }
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
                // A Deallocate with no tmgi-list is rejected rather than treated as "deallocate all".
                // TS 29.532 V19.3.0 Table 6.1.3.2.3.2-1 marks tmgi-list Presence "M" for this operation, and clause
                // 5.2.2.3.1's prose does not license omitting it: "Query parameters shall be used to indicate the
                // TMGI(s) to be deallocated. The NF Service Consumer may request to deallocate all previously
                // allocated TMGIs, or one or more specific TMGIs previously allocated". The first sentence requires
                // the query parameter to carry the indication; the second only chooses between "all" and "specific"
                // within it, deallocating all by enumerating every currently-allocated TMGI.
                //
                // NOTE: TS29532_Nmbsmf_TMGI.yaml, which this build's DELETE handler is generated against, has no
                // "required: true" on the parameter, contradicting its own governing table. The YAML is the defect.
        ogs_error("TMGI Deallocate: mandatory tmgi-list query parameter missing");
        ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            message, "Bad Request", "Requested TMGI Deallocate failed, no tmgi-list",
            NMBSMF_TMGI_MANDATORY_QUERY_PARAM_MISSING);
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

                // A TMGI still referenced by a live MBS session is not freed. smf_mbs_sess_create() stores the same
                // ogs_tmgi_t pointer in smf_mbs_sess_t rather than a copy, so freeing it here would leave that
                // session holding a dangling pointer that a later, unrelated TMGI allocation could recycle and
                // overwrite, corrupting the live session's identity as it feeds PFCP N4mb, Namf_MBSBroadcast and
                // NGAP signalling. See smf_mbs_sess_find_by_tmgi().
        if (smf_mbs_sess_find_by_tmgi(tmgi_found)) {
            ogs_error("TMGI Deallocate: deallocate error, TMGI still in use by an MBS session");
                        // No cause is sent with this 403. TS 29.532 defines only UNKNOWN_TMGI for this service
                        // (Table 6.1.7.3-1), which describes a TMGI that expired or cannot be found rather than one
                        // present and in use, and TS 29.500 Table 5.2.7.2-1's common 403 causes do not cover it either.
                        // ProblemDetails.cause is optional, so omitting it is conformant where inventing a value is not.
                        // 403 itself is a mandated generic status code for DELETE (TS 29.500 Table 5.2.7.1-1).
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
                message, "Forbidden", "Requested TMGI Deallocate failed, TMGI still in use by an MBS session",
                NULL);
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

        // Error checking, check the number of TMGIs available, after releasing any whose
        // advertised expiration time has passed -- see smf_tmgi_reclaim_expired().
        if (smf_tmgi_count() >= OGS_MAX_NUM_OF_TMGI)
            smf_tmgi_reclaim_expired();

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

        // An explicitly-provided TMGI is checked against MBS Sessions already using it; a freshly
        // tmgi_alloc_req'd one is unused by construction and needs no check.
        // smf_mbs_sess_create()'s smf_context_have_matching_mbs_session_id() check catches this only when the
        // TMGI is itself the MBS Session ID (is_tmgi), so a MULTICAST session using SSM as its MBS Session ID
        // alongside a separate explicit TMGI would go unchecked.
    if (tmgi && (!CreateReqData->mbs_session->is_tmgi_alloc_req ||
                CreateReqData->mbs_session->tmgi_alloc_req == 0)) {
        if (smf_mbs_sess_find_by_tmgi(tmgi)) {
            ogs_error("MBS Session Create: provided TMGI is already in use by an existing MBS Session");
            ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
                message, "Forbidden",
                "MBS Session Create failed, provided TMGI is already in use by an existing MBS Session",
                NMBSMF_MBSSESSION_MBS_SESSION_ALREADY_CREATED);
            rv = OGS_ERROR;
            goto cleanup;
        }
    }

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

        // locationDependent and areaSessionId in the response reflect what was requested rather than a fixed
        // false/0; see smf_n4mb_handle_session_establishment_response(). TS 29.532 V17.5.0 cl.5.3.2.2.1: "For a
        // location dependent MBS service, the MB-SMF shall allocate a unique Area Session ID within the MBS
        // session for the MBS Service Area." areaSessionId is readOnly in this schema, so server-allocated and
        // never client-supplied, and this northbound schema carries at most one per session, so any fixed value
        // satisfies "unique within the MBS session" when only one such ID exists in that scope.
    if (CreateReqData->mbs_session->is_location_dependent && CreateReqData->mbs_session->location_dependent) {
        mbs_sess->location_dependent = true;
        mbs_sess->area_session_id = 1;
    }

    if (is_multicast_service) {
        mbs_sess->activity_status = CreateReqData->mbs_session->activity_status;
    }

        // MBS QoS Flow information supplied by the AF is read here: TS 29.571
        // MbsSession.mbsServInfo.mbsMediaComps, each carrying an mbsQoSReq. It is the only real per-flow QoS
        // data this SMF receives, and without reading it
        // ngap_build_mbs_session_setup_or_modification_request_transfer() has nothing to build from. The AF
        // interface does supply it: rt-mbs-function's MediaComp.cc and QoSReq.cc forward exactly this data.
        // QFI is not part of the schema, so one is assigned sequentially per media component present, matching
        // this SMF's existing convention for regular-PDU-session QFIs.
    ogs_list_init(&mbs_sess->mbs_qos_flow_list);
    if (CreateReqData->mbs_session->mbs_serv_info &&
            CreateReqData->mbs_session->mbs_serv_info->mbs_media_comps) {
        // mbsMediaComps is a map (TS 29.571: "the key ... is the mbsMedCompNum attribute"); the
        // generator represents a map as a list of OpenAPI_map_t key/value pairs, not a list of the
        // value type directly -- confirmed against OpenAPI_mbs_service_info_parseFromJSON()'s own
        // parsing code before writing this, not assumed from the header alone.
        OpenAPI_lnode_t *node;
        uint8_t next_qfi = 1;
        OpenAPI_list_for_each(CreateReqData->mbs_session->mbs_serv_info->mbs_media_comps, node) {
            OpenAPI_map_t *pair = (OpenAPI_map_t *)node->data;
            OpenAPI_mbs_media_comp_rm_t *media_comp = pair ? (OpenAPI_mbs_media_comp_rm_t *)pair->value : NULL;
            if (media_comp && media_comp->mbs_qo_s_req) {
                smf_mbs_qos_flow_t *qos_flow = (smf_mbs_qos_flow_t *)ogs_calloc(1, sizeof(*qos_flow));
                ogs_assert(qos_flow);
                qos_flow->qfi = next_qfi++;
                qos_flow->five_qi = (uint8_t)media_comp->mbs_qo_s_req->_5qi;
                ogs_list_add(&mbs_sess->mbs_qos_flow_list, qos_flow);
            }
        }
    }
    if (ogs_list_count(&mbs_sess->mbs_qos_flow_list) == 0) {
        // No clause, configuration option or documented default names what a single QoS flow should
        // be when the AF supplies none at all (rule 12) -- 5QI 9 is TS 23.501 table 5.7.4-1's own
        // standardised non-GBR default, used here as a stated, labelled engineering choice rather than
        // silently repeating the old fabricated three-flow behaviour. Not claimed as spec-derived.
        smf_mbs_qos_flow_t *qos_flow = (smf_mbs_qos_flow_t *)ogs_calloc(1, sizeof(*qos_flow));
        ogs_assert(qos_flow);
        qos_flow->qfi = 1;
        qos_flow->five_qi = 9;
        ogs_list_add(&mbs_sess->mbs_qos_flow_list, qos_flow);
        ogs_warn("MBS Session Create: no mbsServInfo/mbsMediaComps QoS supplied, "
                 "using a single default QoS flow (5QI 9)");
    }

        // MBS FSA ID (MBS Frequency Selection Area Identity) is read from TS 29.532
        // MbsSession.mbsFsaIdList, which the AF supplies and rt-mbs-function's MBSMFMBSSession::setFsaId()
        // builds when it does. ngap_build_mbs_session_setup_or_modification_request_transfer() populates the
        // corresponding Optional NGAP IE whenever this list is non-empty, and omits it otherwise.
    ogs_list_init(&mbs_sess->mbs_fsa_id_list);
    if (CreateReqData->mbs_session->mbs_fsa_id_list) {
        OpenAPI_lnode_t *node;
        OpenAPI_list_for_each(CreateReqData->mbs_session->mbs_fsa_id_list, node) {
            const char *hex_id = (const char *)node->data;
            if (hex_id) {
                smf_mbs_fsa_id_t *fsa_id = (smf_mbs_fsa_id_t *)ogs_calloc(1, sizeof(*fsa_id));
                ogs_assert(fsa_id);
                fsa_id->id = ogs_uint24_from_string(hex_id);
                ogs_list_add(&mbs_sess->mbs_fsa_id_list, fsa_id);
            }
        }
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

        // Idempotent: only the first Release call triggers the AMF and UPF release chain, and a duplicate
        // replies 204 without touching mbs_sess again, which may already be mid-teardown or freed by then.
        // MBSF's delete-cascade can send up to three Nmbsmf_MBSSession Release requests for the same session
        // at the same instant, and without this they run concurrently against one still-live mbs_sess.
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

        // Fires both real release triggers, to the AMF and the UPF, while mbs_sess is still valid; releasing
        // the local smf_mbs_sess_t here instead would make the release chain local bookkeeping only and fill
        // the UPF's fixed-size MBS session pool (OGS_MAX_NUM_OF_MBS_SESSIONS) across create and delete cycles.
        // This mirrors the create path, where smf_n4mb_handle_session_establishment_response() answers its
        // northbound caller before its own follow-on Namf_MBSBroadcast create completes. The local
        // smf_mbs_sess_t is freed only once the UPF's N4mb Session Deletion Response arrives, in
        // smf_n4mb_handle_session_deletion_response() (n4mb-handler.c).

    // Release the AMF/NGAP broadcast context, if one was ever actually created for this session (a
    // Multicast session, or a Broadcast session that never got far enough to receive a mbsContextRef,
    // has nothing to release here).
    if (mbs_sess->mbs_context_ref) {
                // The cached nf_instance is cleared before this call. mbs_sess->sbi holds whatever instance the
                // first namf-mbs-bc call for this session resolved (OGS_SBI_SETUP_NF_INSTANCE in
                // ogs_sbi_discover_and_send()) and is reused for every later call to that service: ContextCreate at
                // session-create time, then this ContextDelete. By delete time that cached instance's
                // nf_service_list has no "namf-mbs-bc" entry, so ogs_sbi_client_find_by_service_name() falls back
                // to the NF-instance-level default client, which carries no port. The request then goes out with a
                // portless 3gpp-Sbi-Target-apiRoot ("http://<amf-ip>"), the SCP fails the downstream HTTP/2
                // connection ("Remote peer returned unexpected data while we expected SETTINGS frame") and returns
                // 500, so the AMF's ContextDelete, and the NGAP Broadcast Session Release it would trigger, never
                // happen and the gNB never frees the session's MRB and LCID.
                //
                // ContextCreate does not hit this because it starts from an empty cache slot, being the session's
                // first namf-mbs-bc call, and goes through the SCP's own fresh discovery, which resolves the
                // complete profile. Clearing the slot puts this call on that same path.
        mbs_sess->sbi.service_type_array[OGS_SBI_SERVICE_TYPE_NAMF_MBS_BC].nf_instance = NULL;
        mbs_sess->sbi.service_type_array[OGS_SBI_SERVICE_TYPE_NAMF_MBS_BC].validity_timeout = 0;

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

    /* code-derived, no spec claim: sendmsg was previously used uninitialized -- harmless while
     * every path here only ever built a bodyless 204, but this function now also builds a 200
     * response carrying sendmsg.UpdateRspData, and ogs_sbi_build_response()'s own serialiser
     * (lib/sbi/message.c) picks its response body by checking each OpenAPI_*_t field on this
     * struct in turn, so stack garbage in an unrelated field could otherwise be mistaken for a
     * different response type. */
    memset(&sendmsg, 0, sizeof(sendmsg));

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
        /* TS 29.532 clause 5.3.2.3.1 lists mbsSecurityContext (MSK/MTK key rotation) as modifiable for a
         * multicast session, unlike the fields above this block, which the same clause never lists as
         * updatable at all. This SMF has no multicast security-context state to update (mbsSecurityContext
         * is stubbed NULL throughout -- see this file's own Multicast-scope survey), so the honest response
         * is "not implemented", not "forbidden". */
        CASE("/mbsSecurityContext")
        CASE("/mbsSecurityContext/keyList")
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
    /* TS 29.532 V18.6.0 clause 5.3.2.3.1 step 2b: "If the MBS service area received in the
     * request cannot be entirely covered by the MB-SMF service area, the MB-SMF shall reduce the
     * MBS service area to be within the MB-SMF service area and continue the Update service
     * operation using the reduced MBS service area."  The step then returns 200 OK and requires
     * the MB-SMF to "provide in the response the representation of the updated MBS session
     * including the reduced MBS service area in the redMbsServArea attribute set to the part of
     * the requested MBS service area that is within the MB-SMF service area in which the MBS
     * session has been updated".  Step 2a, unchanged and still the default, returns 204 No
     * Content.  (Status codes are named without quotation marks here: the specification writes
     * them inside its own quotation marks, which cannot be nested inside a quoted sentence.)
     *
     * Mirrors smf_n4mb_handle_session_establishment_response()'s own Create-side check
     * (n4mb-handler.c) against the same smf_mbs_service_area_reduce() -- see that function's own
     * scope note (TAI-list coverage only, S12). `mbs_sess->mbs_service_area` is this session's own
     * requested area. PATCH `/mbsServiceArea` is answered 501 above and so never mutates it yet
     * (a separate, unimplemented item -- not this one); this branch is therefore unreachable live
     * until that is done, but is wired unconditionally on smf_mbs_service_area_reduce()'s own
     * result so the reporting is already correct once it is. */
    OpenAPI_mbs_service_area_t *Red_mbs_service_area = NULL;
    ogs_mbs_service_area_t *reduced_mbs_service_area = NULL;
    bool area_reduced = mbs_sess->mbs_service_area &&
            smf_mbs_service_area_reduce(mbs_sess->mbs_service_area, &reduced_mbs_service_area);

    if (!area_reduced) {
        response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
    } else {
        OpenAPI_mbs_session_id_t *Mbs_session_id = NULL;
        OpenAPI_tmgi_t *Tmgi = NULL;
        OpenAPI_tmgi_t *Tmgi_copy = NULL;
        OpenAPI_ssm_t *Ssm = NULL;
        OpenAPI_ssm_t *Ssm_copy = NULL;
        char *expiration_time = NULL;
        OpenAPI_mbs_service_type_e Mbs_service_type = OpenAPI_mbs_service_type_NULL;
        OpenAPI_list_t *ingress_tun_addr = NULL;
        OpenAPI_ext_mbs_session_t *Ext_mbs_session = NULL;
        OpenAPI_update_rsp_data_t *UpdateRspData = NULL;

        Red_mbs_service_area = ogs_sbi_build_mbs_service_area(reduced_mbs_service_area);
        ogs_mbs_service_area_free(reduced_mbs_service_area);

        /* Same field population as smf_n4mb_handle_session_establishment_response()'s own Create
         * response (n4mb-handler.c) -- kept identical rather than inventing new field coverage
         * this item does not need; see that function's own comments for why each field is (or is
         * not) populated. */
        Tmgi = ogs_sbi_build_tmgi(mbs_sess->tmgi);
        if (mbs_sess->mbs_session_id.is_tmgi) {
            Tmgi_copy = OpenAPI_tmgi_copy(Tmgi_copy, Tmgi);
            Mbs_session_id = OpenAPI_mbs_session_id_create(Tmgi_copy, NULL, mbs_sess->mbs_session_id.nid);
        }
        if (mbs_sess->mbs_session_id.is_ssm) {
            Ssm = ogs_sbi_build_ssm(mbs_sess->mbs_session_id.ssm);
            Ssm_copy = OpenAPI_ssm_copy(Ssm_copy, Ssm);
            Mbs_session_id = OpenAPI_mbs_session_id_create(NULL, Ssm_copy, NULL /* nid */);
        }

        Mbs_service_type = OpenAPI_mbs_service_type_FromString(mbs_sess->service_type);

        if (mbs_sess->ingress_tun_addr) {
            char *ipv4_str = NULL;
            char *ipv6_str = NULL;
            int port;
            ingress_tun_addr = OpenAPI_list_create();
            if (mbs_sess->ingress_tun_addr->ogs_sa_family == AF_INET) {
                char buf[OGS_ADDRSTRLEN];
                ipv4_str = ogs_strdup(OGS_ADDR(mbs_sess->ingress_tun_addr, buf));
            } else if (mbs_sess->ingress_tun_addr->ogs_sa_family == AF_INET6) {
                char buf[OGS_ADDRSTRLEN];
                ipv6_str = ogs_strdup(OGS_ADDR(mbs_sess->ingress_tun_addr, buf));
            }
            port = OGS_PORT(mbs_sess->ingress_tun_addr);
            OpenAPI_list_add(ingress_tun_addr, OpenAPI_tunnel_address_create(ipv4_str, ipv6_str, port));
        }

        if (mbs_sess->tmgi)
            expiration_time = ogs_strdup(mbs_sess->tmgi->expiration_time);

        Ext_mbs_session = OpenAPI_ext_mbs_session_create(
                Mbs_session_id  /* mbs_session_id */,
                false, 0        /* tmgi_alloc_req */,
                Tmgi            /* tmgi */,
                expiration_time /* expiration_time */,
                Mbs_service_type /* service_type */,
                mbs_sess->location_dependent, mbs_sess->location_dependent /* location_dependent */,
                mbs_sess->location_dependent, mbs_sess->area_session_id /* area_session_id */,
                false, 0        /* ingress_tun_addr_req */,
                ingress_tun_addr /* ingress_tun_addr */,
                Ssm             /* ssm */,
                NULL            /* mbs_service_area, writeOnly, unaffected -- see redMbsServArea's
                                   own Create-side commit for this choice */,
                Red_mbs_service_area /* red_mbs_service_area */,
                NULL            /* ext_mbs_service_area */,
                NULL            /* dnn */,
                NULL            /* snssai */,
                NULL            /* activation_time */,
                NULL            /* start_time */,
                NULL            /* termination_time */,
                NULL            /* mbs_serv_info */,
                NULL            /* mbs_session_subsc */,
                mbs_sess->activity_status /* activity_status */,
                false, 0        /* any_ue_ind */,
                NULL            /* mbs_fsa_id_list */,
                NULL            /* mbs_security_context */,
                false, 0        /* contact_pcf_ind */);

        UpdateRspData = OpenAPI_update_rsp_data_create(Ext_mbs_session);
        sendmsg.UpdateRspData = UpdateRspData;

        response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_OK);

        if (UpdateRspData)
            OpenAPI_update_rsp_data_free(UpdateRspData);
    }

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
