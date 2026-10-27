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

/* The RTP header of a UDP packet over IPv4 or IPv6 without extension headers, NULL if the packet is not RTP (version 2 and at least the fixed header); rtp_len is the length from there to the end of the packet */
static const uint8_t *rtp_header(const ogs_pkbuf_t *pkbuf, int *rtp_len)
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
    *rtp_len = len - offset;
    return p + offset;
}

/*
 * The RTP Header Extension for PDU Set Marking (TS 26.522 V19.4.0 clause 4.2), in the one-byte (0xBEDE) or two-byte (0x100) format of
 * RFC 8285. Returns false if the packet has no such element with the ID of the Protocol Description.
 */
static bool rtp_pdu_set_header_extension(const uint8_t *rtp, int rtp_len, uint8_t id, ogs_pfcp_qer_t *qer)
{
    int offset = 12 + 4 * (rtp[0] & 0x0f), end;
    uint16_t profile;

    if (!(rtp[0] & 0x10) || rtp_len < offset + 4)
        return false;
    profile = (rtp[offset] << 8) | rtp[offset + 1];
    end = offset + 4 + 4 * ((rtp[offset + 2] << 8) | rtp[offset + 3]);
    if (end > rtp_len)
        return false;
    offset += 4;

    while (offset < end) {
        const uint8_t *e;
        int element_id, len;

        if (rtp[offset] == 0) {         /* padding */
            offset++;
            continue;
        }
        if (profile == 0xbede) {
            element_id = rtp[offset] >> 4;
            if (element_id == 15)       /* the reserved ID: no element follows */
                return false;
            len = (rtp[offset] & 0x0f) + 1;
            e = rtp + offset + 1;
            offset += 1 + len;
        } else if ((profile & 0xfff0) == 0x1000) {
            if (offset + 2 > end)
                return false;
            element_id = rtp[offset];
            len = rtp[offset + 1];
            e = rtp + offset + 2;
            offset += 2 + len;
        } else
            return false;
        if (offset > end)
            return false;
        if (element_id != id)
            continue;

        /*
         * E, R, D and PSI in one octet, then PSSN (10 bits) and PSN (6 bits); the PDU Set Size (3 octets) and the Number of PDUs (2 octets)
         * may follow, so the element has 3, 5 (Number of PDUs), 6 (PDU Set Size) or 8 octets
         */
        if (len != 3 && len != 5 && len != 6 && len != 8)
            return false;
        qer->pdu_set_mark.end_pdu = (e[0] & 0x80) != 0;
        qer->pdu_set_mark.psi = e[0] & 0x0f;
        qer->pdu_set_mark.pssn = ((e[1] << 2) | (e[2] >> 6)) & 0x3ff;
        qer->pdu_set_mark.psn = e[2] & 0x3f;
        qer->pdu_set_mark.pssize_presence = false;
        if (len == 6 || len == 8) {
            /* 0 is a sender that could not determine the size */
            qer->pdu_set_mark.pssize = ((uint32_t)e[3] << 16) | (e[4] << 8) | e[5];
            qer->pdu_set_mark.pssize_presence = qer->pdu_set_mark.pssize != 0;
        }
        return true;
    }
    return false;
}

static bool payload_type_listed(const ogs_pfcp_pdr_t *pdr, uint8_t payload_type)
{
    int i;

    if (!pdr->protocol_description.num_of_payload_type)
        return true;
    for (i = 0; i < pdr->protocol_description.num_of_payload_type; i++)
        if (pdr->protocol_description.payload_type[i] == payload_type)
            return true;
    return false;
}

/* a PDU that belongs to no PDU Set that can be seen is a PDU Set of one PDU */
static void mark_single_pdu_set(ogs_pfcp_qer_t *qer)
{
    qer->pdu_set.pssn = qer->pdu_set.rtp_timestamp_known ? (qer->pdu_set.pssn + 1) & 0x3ff : 0;
    qer->pdu_set.rtp_timestamp_known = true;
    qer->pdu_set.active = false;
    qer->pdu_set.psn = 0;
    qer->pdu_set_mark.pssn = qer->pdu_set.pssn;
    qer->pdu_set_mark.psn = 0;
    qer->pdu_set_mark.end_pdu = true;
}

void upf_pdu_set_mark(ogs_pfcp_pdr_t *pdr, const ogs_pkbuf_t *pkbuf)
{
    ogs_pfcp_qer_t *qer;
    const uint8_t *rtp = NULL;
    int rtp_len = 0;
    bool described;

    ogs_assert(pdr);
    qer = pdr->qer;
    ogs_assert(qer);
    ogs_assert(pkbuf);

    memset(&qer->pdu_set_mark, 0, sizeof(qer->pdu_set_mark));
    /* MoQ Transport is identified from its metadata, which this UPF does not read: its packets are not RTP */
    if (!pdr->protocol_description.presence || pdr->protocol_description.transport_protocol != 3 /* MOQT, TS 29.244 table 8.2.233-1 */)
        rtp = rtp_header(pkbuf, &rtp_len);
    /* a Protocol Description with payload types limits the RTP packets that are parsed to them (table 7.5.2.2-7, note 1) */
    if (rtp && pdr->protocol_description.presence && !payload_type_listed(pdr, rtp[1] & 0x7f))
        rtp = NULL;
    described = pdr->protocol_description.presence && pdr->protocol_description.rtp_he_type == 1 /* PDU Set Marking, table 8.2.234-1 */;

    if (rtp && described) {
        /* the sender marks its PDU Sets: PDU Set Sequence Number, PDU Sequence Number, End PDU, Importance and, if enabled, the Size */
        if (rtp_pdu_set_header_extension(rtp, rtp_len, pdr->protocol_description.rtp_he_id, qer)) {
            qer->pdu_set.pssn = qer->pdu_set_mark.pssn;
            qer->pdu_set.psn = qer->pdu_set_mark.psn;
            qer->pdu_set.rtp_timestamp_known = true;
            qer->pdu_set.active = false;
        } else {
            /* an RTP packet without the extension does not belong to a PDU Set based on the Protocol Description */
            memset(&qer->pdu_set_mark, 0, sizeof(qer->pdu_set_mark));
            mark_single_pdu_set(qer);
            qer->pdu_set_mark.psi = 0;
        }
    } else if (rtp) {
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
        /* the importance of the PDU Set is not known: 0 (TS 38.415 V19.1.0 clause 6.5.3.9) */
        qer->pdu_set_mark.psi = 0;
    } else {
        mark_single_pdu_set(qer);
        qer->pdu_set_mark.psi = 0;
    }
    qer->pdu_set_mark.presence = true;
}
