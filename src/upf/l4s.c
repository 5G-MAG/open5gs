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


#include "l4s.h"

#include <netinet/ip.h>
#include <netinet/ip6.h>

void upf_l4s_congestion(upf_sess_t *sess, const ogs_gtp2_header_desc_t *header_desc)
{
    ogs_pfcp_qer_t *qer = NULL;

    ogs_assert(sess);
    ogs_assert(header_desc);

    ogs_list_for_each(&sess->pfcp.qer_list, qer) {
        if (qer->qfi != header_desc->qos_flow_identifier || !qer->ecn_marking_l4s)
            continue;
        /* a value of the NG-RAN is a percentage in hundredths, 0 to 10000 */
        if (header_desc->congestion.ul_presence)
            qer->ecn_ul = header_desc->congestion.ul > 10000 ? 10000 : header_desc->congestion.ul;
        if (header_desc->congestion.dl_presence)
            qer->ecn_dl = header_desc->congestion.dl > 10000 ? 10000 : header_desc->congestion.dl;
    }
}

void upf_l4s_mark(ogs_pkbuf_t *pkbuf, uint16_t percent)
{
    uint8_t version;

    ogs_assert(pkbuf);
    if (!percent || pkbuf->len < 1)
        return;
    /* the percentage of the packets: one packet in 10000 is chosen with the probability */
    if (percent < 10000 && (uint16_t)(rand() % 10000) >= percent)
        return;

    version = pkbuf->data[0] >> 4;
    if (version == 4 && pkbuf->len >= sizeof(struct ip)) {
        struct ip *ip_h = (struct ip *)pkbuf->data;

        /* the ECN field is the two low bits of the ToS octet: 00 is not ECN capable, and is not marked */
        if ((ip_h->ip_tos & 0x03) == 0x00 || (ip_h->ip_tos & 0x03) == 0x03)
            return;
        ip_h->ip_tos |= 0x03;
        ip_h->ip_sum = 0;
        ip_h->ip_sum = ogs_in_cksum((uint16_t *)ip_h, ip_h->ip_hl * 4);
    } else if (version == 6 && pkbuf->len >= sizeof(struct ip6_hdr)) {
        /* the traffic class spans the low nibble of the first octet and the high nibble of the second; its two low bits are the ECN field */
        uint8_t *p = pkbuf->data;
        uint8_t ecn = (p[1] >> 4) & 0x03;

        if (ecn == 0x00 || ecn == 0x03)
            return;
        p[1] |= 0x30;
    }
}
