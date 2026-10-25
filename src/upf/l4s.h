/*
 * Copyright (C) 2019-2025 by Sukchan Lee <acetcom@gmail.com>
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


#ifndef UPF_L4S_H
#define UPF_L4S_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ECN marking for L4S at the UPF (TS 29.244 V19.6.0 clause 5.38.1) */

/* The congestion information of the NG-RAN for a QoS flow (UL PDU SESSION INFORMATION frame): what the UPF marks from now on */
void upf_l4s_congestion(upf_sess_t *sess, const ogs_gtp2_header_desc_t *header_desc);

/* Mark the percentage (hundredths of a percent) of the IP packets that are ECN capable: the congestion experienced code point */
void upf_l4s_mark(ogs_pkbuf_t *pkbuf, uint16_t percent);

#ifdef __cplusplus
}
#endif

#endif /* UPF_L4S_H */
