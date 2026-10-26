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


#ifndef UPF_PDU_SET_H
#define UPF_PDU_SET_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PDU Set marking of a downlink packet of a QoS flow (TS 29.244 V19.6.0 clause 5.38.3): the PDU Set information that the packet gets in the PDU
 * Set Information Container is left in the QER for the encapsulation. The PDU Sets are identified by implementation specific means: the
 * RTP packets of one PDU Set have the same RTP timestamp and the packet with the marker bit is the last one; any other packet is a PDU Set of its own.
 */
void upf_pdu_set_mark(ogs_pfcp_qer_t *qer, const ogs_pkbuf_t *pkbuf);

#ifdef __cplusplus
}
#endif

#endif /* UPF_PDU_SET_H */
