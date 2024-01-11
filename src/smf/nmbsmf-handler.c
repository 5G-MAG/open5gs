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
    ogs_warn("TMGI allocate request received");
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
        smf_sbi_send_nmbsmf_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
            "Bad Request", "Requested TMGI Allocate failed, no TmgiAllocate", NULL);
        rv = OGS_ERROR;
        goto cleanup;
    }

    if (!TmgiAllocate->tmgi_number && !TmgiAllocate->tmgi_list) {
        ogs_error("TMGI Allocate: tmgi_number and tmgi_list not present");
        // tmgi_number or tmgi_list should be present, send error (403 + MANDATORY_IE_INCORRECT)
        smf_sbi_send_nmbsmf_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
            "Mandatory IE incorrect",
            "Requested TMGI Allocate failed, no [tmgiNumber] nor [tmgiList] present",
            NMBSMF_TMGI_MANDATORY_IE_INCORRECT);
        rv = OGS_ERROR;
        goto cleanup;
    }

    // Error checking for TmgiAllocate->tmgi_list
    if (TmgiAllocate->tmgi_list) {
        // Check for errors in TmgiAllocate->tmgi_list
        OpenAPI_list_for_each(TmgiAllocate->tmgi_list, node) {
            if (!node->data)
                continue;

            Tmgi_received = node->data;

            ogs_sbi_parse_tmgi(&tmgi_received, Tmgi_received);

            tmgi_found = smf_tmgi_find_by_tmgi(&tmgi_received);

            if (!tmgi_found) {
                ogs_error("TMGI Allocate: refresh error, TMGI not present");
                // TMGI not found, send error (404 + UNKNOWN_TMGI)
                smf_sbi_send_nmbsmf_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
                    "Unknown TMGI",
                    "Requested TMGI Allocate (refresh) failed, TMGI expired or cannot be found",
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
                smf_sbi_send_nmbsmf_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
                    "Forbidden", "Cannot allocate [tmgiNumber] of TMGIs", NULL);
                rv = OGS_ERROR;
                goto cleanup;
            }
        } else {
            ogs_error("TMGI Allocate: allocate error, incorrect number in tmgi_number");
            // tmgi_number needs to be between 1 and 255, send error (403 + MANDATORY_IE_INCORRECT)
            smf_sbi_send_nmbsmf_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
                "Mandatory IE incorrect",
                "Requested TMGI Allocate failed, incorrect number in [tmgiNumber]",
                NMBSMF_TMGI_MANDATORY_IE_INCORRECT);
            rv = OGS_ERROR;
            goto cleanup;
        }
    }

    expiration_time = smf_tmgi_gen_expiration_time(OGS_DEFAULT_EXPIRATION_TIME_VALIDITY);

    // Include a list, even if its empty
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

    // TODO (borieher): Add the NID parameter
    TmgiAllocated = OpenAPI_tmgi_allocated_create(tmgi_list, ogs_strdup(expiration_time), NULL);

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
    // TODO (borieher): Implement TMGI Dellocate service operation

    ogs_warn("TMGI deallocate request received");

    return true;
}
