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
#define NMBSMF_TMGI_MANDATORY_IE_INCORRECT  "MANDATORY_IE_INCORRECT"
#define NMBSMF_TMGI_UNKNOWN_TMGI            "UNKNOWN_TMGI"

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

/* Nmbsmf_MBSSession Service - MBS Session Create service operation */
bool smf_nmbsmf_handle_mbs_session_create(
    ogs_sbi_stream_t *stream, ogs_sbi_message_t *message);

#ifdef __cplusplus
}
#endif

#endif /* SMF_NMBSMF_HANDLER_H */
