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

#ifndef SMF_NMBSMF_HANDLER_H
#define SMF_NMBSMF_HANDLER_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Nmbsmf_TMGI Service API */

/* Application error codes */
#define NMBSMF_TMGI_MANDATORY_IE_MISSING    "MANDATORY_IE_MISSING"
#define NMBSMF_TMGI_MANDATORY_IE_INCORRECT  "MANDATORY_IE_INCORRECT"
#define NMBSMF_TMGI_INSUFFICIENT_RESOURCES  "INSUFFICIENT_RESOURCES"
#define NMBSMF_TMGI_UNKNOWN_TMGI            "UNKNOWN_TMGI"
/* The generic cause for a missing mandatory query parameter, which is this operation's tmgi-list on
 * DELETE (see smf_nmbsmf_handle_tmgi_deallocate()).
 *
 * TS 29.500 V18.10.0 table 5.2.7.2-1, row MANDATORY_QUERY_PARAM_MISSING: "Query parameter which is
 * defined as mandatory, or as conditional but mandatory required, for an HTTP method is not included
 * in the URI of the request."
 *
 * It is deliberately not from TS 29.532's own operation-specific list: table 6.1.3.2.3.2-3 defines
 * one application error for this operation, UNKNOWN_TMGI with 404, and none for a malformed
 * request, so the common table above is where this case is answered from. */
#define NMBSMF_TMGI_MANDATORY_QUERY_PARAM_MISSING "MANDATORY_QUERY_PARAM_MISSING"
/* There is deliberately no cause here for deallocating a TMGI that is still referenced. TS 29.532
 * V18.6.0 table 6.1.3.2.3.2-3 defines one application error for this operation, UNKNOWN_TMGI with
 * 404, and no 403 row; the generic 403 causes in TS 29.500 V18.10.0 table 5.2.7.2-1 describe
 * authorisation and modification failures, not a resource still in use. Neither level defines one,
 * so none is invented. The call site in nmbsmf-handler.c says what it does instead. */

/* TMGI API constants */
#define NMBSMF_TMGI_MIN_TMGI_NUMBER 1
#define NMBSMF_TMGI_MAX_TMGI_NUMBER 255

/* Nmbsmf_TMGI Service - TMGI Allocate service operation */
bool smf_nmbsmf_handle_tmgi_allocate(
    ogs_sbi_stream_t *stream, ogs_sbi_message_t *message);
/* Nmbsmf_TMGI Service - TMGI Deallocate service operation */
bool smf_nmbsmf_handle_tmgi_deallocate(
    ogs_sbi_stream_t *stream, ogs_sbi_message_t *message);

/* Nmbsmf_MBSSession Service API */

/* Application error codes */
#define NMBSMF_MBSSESSION_MANDATORY_IE_MISSING        "MANDATORY_IE_MISSING"
#define NMBSMF_MBSSESSION_MANDATORY_IE_INCORRECT      "MANDATORY_IE_INCORRECT"
#define NMBSMF_MBSSESSION_ERROR_INPUT_PARAMETERS      "ERROR_INPUT_PARAMETERS"
#define NMBSMF_MBSSESSION_MBS_SESSION_ALREADY_CREATED "MBS_SESSION_ALREADY_CREATED"
#define NMBSMF_MBSSESSION_UNKNOWN_TMGI                "UNKNOWN_TMGI"
#define NMBSMF_MBSESSION_UNKNOWN_MBS_SESSION          "UNKNOWN_MBS_SESSION"

/* Nmbsmf_MBSSession Service - MBS Session Create service operation */
bool smf_nmbsmf_handle_mbs_session_create(
    ogs_sbi_stream_t *stream, ogs_sbi_message_t *message);

/* Nmbsmf_MBSSession Service - MBS Session Release service operation */
bool smf_nmbsmf_handle_mbs_session_release(
    smf_mbs_sess_t *mbs_sess, ogs_sbi_stream_t *stream, ogs_sbi_message_t *message);

/* Nmbsmf_MBSSession Service - MBS Session Update/Patch service operation */
bool smf_nmbsmf_handle_mbs_session_patch(
    smf_mbs_sess_t *mbs_sess, ogs_sbi_stream_t *stream, ogs_sbi_message_t *message);

#ifdef __cplusplus
}
#endif

#endif /* SMF_NMBSMF_HANDLER_H */
