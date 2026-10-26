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


#include "pdu-set.h"

#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>

/* The RTP header of a UDP packet over IPv4 or IPv6 without extension headers, NULL if the packet is not RTP (version 2 and at least the fixed header) */
static const uint8_t *rtp_header(const ogs_pkbuf_t *pkbuf)
{
    const uint8_t *p = pkbuf->data;
    int len = pkbuf->len, offset;

    if (len < 1)
        return NULL;
    if ((p[0] >> 4) == 4 && len >= (int)sizeof(struct ip)) {
        const struct ip *ip_h = (const struct ip *)p;

        if (ip_h->ip_p != IPPROTO_UDP)
            return NULL;
        offset = ip_h->ip_hl * 4;
    } else if ((p[0] >> 4) == 6 && len >= (int)sizeof(struct ip6_hdr)) {
        const struct ip6_hdr *ip6_h = (const struct ip6_hdr *)p;

        if (ip6_h->ip6_nxt != IPPROTO_UDP)
            return NULL;
        offset = sizeof(struct ip6_hdr);
    } else
        return NULL;
    offset += sizeof(struct udphdr);
    if (len < offset + 12 || (p[offset] >> 6) != 2)
        return NULL;
    return p + offset;
}

void upf_pdu_set_mark(ogs_pfcp_qer_t *qer, const ogs_pkbuf_t *pkbuf)
{
    const uint8_t *rtp;

    ogs_assert(qer);
    ogs_assert(pkbuf);

    memset(&qer->pdu_set_mark, 0, sizeof(qer->pdu_set_mark));
    rtp = rtp_header(pkbuf);
    if (rtp) {
        uint32_t timestamp = ((uint32_t)rtp[4] << 24) | ((uint32_t)rtp[5] << 16) | ((uint32_t)rtp[6] << 8) | rtp[7];
        bool marker = (rtp[1] & 0x80) != 0;

        if (!qer->pdu_set.active || qer->pdu_set.rtp_timestamp != timestamp) {
            /* the first packet of a PDU Set: the sequence number counts the sets of the QoS flow, 10 bits */
            qer->pdu_set.pssn = qer->pdu_set.rtp_timestamp_known ? (qer->pdu_set.pssn + 1) & 0x3ff : 0;
            qer->pdu_set.psn = 0;
            qer->pdu_set.rtp_timestamp = timestamp;
            qer->pdu_set.rtp_timestamp_known = true;
            qer->pdu_set.active = true;
        } else
            qer->pdu_set.psn++;
        qer->pdu_set_mark.pssn = qer->pdu_set.pssn;
        qer->pdu_set_mark.psn = qer->pdu_set.psn;
        qer->pdu_set_mark.end_pdu = marker;
        if (marker)
            qer->pdu_set.active = false;
    } else {
        /* a packet that is not RTP belongs to no PDU Set that can be seen: it is a PDU Set of one PDU */
        qer->pdu_set.pssn = qer->pdu_set.rtp_timestamp_known ? (qer->pdu_set.pssn + 1) & 0x3ff : 0;
        qer->pdu_set.rtp_timestamp_known = true;
        qer->pdu_set.active = false;
        qer->pdu_set.psn = 0;
        qer->pdu_set_mark.pssn = qer->pdu_set.pssn;
        qer->pdu_set_mark.psn = 0;
        qer->pdu_set_mark.end_pdu = true;
    }
    /* the importance of the PDU Set is not known: 0 (TS 38.415 V19.1.0 clause 6.5.3.9) */
    qer->pdu_set_mark.psi = 0;
    qer->pdu_set_mark.presence = true;
}
