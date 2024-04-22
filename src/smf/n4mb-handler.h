/*
 * 5G-MAG Reference Tools
 * Copyright (C) 2024 iTEAM UPV <borieher@iteam.upv.es>
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

#ifndef SMF_N4MB_HANDLER_H
#define SMF_N4MB_HANDLER_H

#ifdef  __cplusplus
extern "C" {
#endif

uint8_t smf_n4mb_handle_session_establishment_response(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_establishment_response_t *rsp);

#ifdef __cplusplus
}
#endif

#endif /* SMF_N4MB_HANDLER_H */