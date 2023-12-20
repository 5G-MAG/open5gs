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

#include "nmbsmf-handler.h"

/* Nmbsmf_TMGI Service API */

/*
 * 3GPP TS 29.532 - Release 17.4.0
 * 5G System; 5G Multicast-Broadcast Session Management Services; Stage 3
 * Ch. 5.2.2.2 - Nmbsmf_TMGI Service API - TMGI Allocate service operation
 */
bool smf_nmbsmf_handle_allocate_tmgi(
    ogs_sbi_stream_t *stream, ogs_sbi_message_t *message)
{
    // TODO (borieher): Implement TMGI Allocate service operation

    ogs_warn("TMGI allocate request received");

    return true;
}