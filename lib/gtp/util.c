/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
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

#include "ogs-gtp.h"

/* TS 38.415 V19.1.0 clauses 5.5.2.1 and 5.5.2.2: the frames of the PDU Session Container that carry QoS monitoring. len: the octets of the content */
static void parse_qos_monitoring(ogs_gtp2_header_desc_t *header_desc, const uint8_t *c, int len)
{
    int pos = 2;
    uint8_t pdu_type;
    uint64_t v;
    int k;

    if (len < 2)
        return;
    pdu_type = c[0] >> 4;
    header_desc->pdu_type = pdu_type;
    header_desc->qos_flow_identifier = c[1] & 0x3f;
    header_desc->qm.qmp = (c[0] & 0x08) ? true : false;
    if (!header_desc->qm.qmp)
        return;

#define GET64(__dst) do { \
        if (pos + 8 > len) { header_desc->qm.qmp = false; return; } \
        v = 0; for (k = 0; k < 8; k++) v = (v << 8) | c[pos + k]; \
        (__dst) = v; pos += 8; \
    } while (0)
#define GET32(__dst) do { \
        if (pos + 4 > len) { header_desc->qm.qmp = false; return; } \
        (__dst) = ((uint32_t)c[pos] << 24) | ((uint32_t)c[pos + 1] << 16) | ((uint32_t)c[pos + 2] << 8) | c[pos + 3]; \
        pos += 4; \
    } while (0)

    if (pdu_type == OGS_GTP2_EXTENSION_HEADER_PDU_TYPE_DL_PDU_SESSION_INFORMATION) {
        if (c[1] & 0x80) /* PPP: the PPI octet */
            pos++;
        GET64(header_desc->qm.dl_sending_ts);
    } else if (pdu_type == OGS_GTP2_EXTENSION_HEADER_PDU_TYPE_UL_PDU_SESSION_INFORMATION) {
        header_desc->qm.dl_delay_ind = (c[0] & 0x04) ? true : false;
        header_desc->qm.ul_delay_ind = (c[0] & 0x02) ? true : false;
        GET64(header_desc->qm.dl_sending_ts);
        GET64(header_desc->qm.dl_received_ts);
        GET64(header_desc->qm.ul_sending_ts);
        if (header_desc->qm.dl_delay_ind)
            GET32(header_desc->qm.dl_delay_result);
        if (header_desc->qm.ul_delay_ind)
            GET32(header_desc->qm.ul_delay_result);
    }
#undef GET64
#undef GET32
}

uint64_t ogs_gtpu_time_to_ntp64(ogs_time_t time)
{
    uint64_t sec = (uint64_t)(time / OGS_USEC_PER_SEC) + 2208988800ULL;
    uint64_t usec = (uint64_t)(time % OGS_USEC_PER_SEC);

    return (sec << 32) | ((usec << 32) / OGS_USEC_PER_SEC);
}

ogs_time_t ogs_gtpu_ntp64_to_time(uint64_t ntp)
{
    uint64_t sec = ntp >> 32;
    uint64_t frac = ntp & 0xffffffffULL;

    return (ogs_time_t)(((int64_t)sec - 2208988800LL) * OGS_USEC_PER_SEC) +
        (ogs_time_t)((frac * OGS_USEC_PER_SEC) >> 32);
}

static void put64(uint8_t *p, uint64_t v)
{
    int k;

    for (k = 7; k >= 0; k--) { p[k] = v & 0xff; v >>= 8; }
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24; p[1] = (v >> 16) & 0xff; p[2] = (v >> 8) & 0xff; p[3] = v & 0xff;
}

/* The G-PDU header with the sequence number octets, the PDU Session Container header and its content of content_len octets (a multiple of four, less 3) */
static ogs_pkbuf_t *build_container_packet(uint32_t teid, uint8_t **content, int content_len)
{
    ogs_pkbuf_t *pkbuf = ogs_pkbuf_alloc(NULL, OGS_MAX_PKT_LEN);
    uint8_t *p;
    int ext_len = content_len + 2; /* the length octet, the content and the next extension header type */

    ogs_assert(pkbuf);
    ogs_assert(ext_len % 4 == 0);
    ogs_pkbuf_put(pkbuf, OGS_GTPV1U_HEADER_LEN + OGS_GTPV1U_EXTENSION_HEADER_LEN + ext_len);
    p = pkbuf->data;
    memset(p, 0, pkbuf->len);

    p[0] = OGS_GTPU_FLAGS_V | OGS_GTPU_FLAGS_PT | OGS_GTPU_FLAGS_E;
    p[1] = OGS_GTPU_MSGTYPE_GPDU;
    /* the length counts what follows the mandatory part of the header */
    p[2] = ((OGS_GTPV1U_EXTENSION_HEADER_LEN + ext_len) >> 8) & 0xff;
    p[3] = (OGS_GTPV1U_EXTENSION_HEADER_LEN + ext_len) & 0xff;
    put32(p + 4, teid);
    p[11] = OGS_GTP2_EXTENSION_HEADER_TYPE_PDU_SESSION_CONTAINER;
    p[12] = ext_len / 4;
    /* the next extension header type (the last octet) is zero */
    *content = p + 13;
    return pkbuf;
}

ogs_pkbuf_t *ogs_gtpu_build_qos_monitoring_dl(
        uint32_t teid, uint8_t qfi, uint64_t dl_sending_ts)
{
    uint8_t *c = NULL;
    /* octet 1, the QFI octet and the time stamp: 10 octets, in a container of 16 octets in all */
    ogs_pkbuf_t *pkbuf = build_container_packet(teid, &c, 14);

    c[0] = (OGS_GTP2_EXTENSION_HEADER_PDU_TYPE_DL_PDU_SESSION_INFORMATION << 4) | 0x08;
    c[1] = qfi & 0x3f;
    put64(c + 2, dl_sending_ts);
    return pkbuf;
}

ogs_pkbuf_t *ogs_gtpu_build_qos_monitoring_ul(uint32_t teid, uint8_t qfi,
        uint64_t dl_sending_ts, uint64_t dl_received_ts, uint64_t ul_sending_ts,
        bool dl_delay_ind, uint32_t dl_delay_result,
        bool ul_delay_ind, uint32_t ul_delay_result)
{
    uint8_t *c = NULL;
    int n = 2 + 24 + (dl_delay_ind ? 4 : 0) + (ul_delay_ind ? 4 : 0);
    int content_len = (n + 2 + 3) / 4 * 4 - 2; /* content and padding up to a whole number of four octets, as the extension header */
    ogs_pkbuf_t *pkbuf = build_container_packet(teid, &c, content_len);
    int pos = 2;

    c[0] = (OGS_GTP2_EXTENSION_HEADER_PDU_TYPE_UL_PDU_SESSION_INFORMATION << 4) | 0x08 |
        (dl_delay_ind ? 0x04 : 0) | (ul_delay_ind ? 0x02 : 0);
    c[1] = qfi & 0x3f;
    put64(c + pos, dl_sending_ts); pos += 8;
    put64(c + pos, dl_received_ts); pos += 8;
    put64(c + pos, ul_sending_ts); pos += 8;
    if (dl_delay_ind) { put32(c + pos, dl_delay_result); pos += 4; }
    if (ul_delay_ind) { put32(c + pos, ul_delay_result); pos += 4; }
    return pkbuf;
}

int ogs_gtpu_parse_header(
        ogs_gtp2_header_desc_t *header_desc, ogs_pkbuf_t *pkbuf)
{
    ogs_gtp2_header_t *gtp_h = NULL;
    ogs_gtp2_extension_header_t ext_hdesc;
    uint8_t *ext_h = NULL;
    uint16_t len = 0;
    int i;

    ogs_assert(pkbuf);
    ogs_assert(pkbuf->data);

    gtp_h = (ogs_gtp2_header_t *)pkbuf->data;

    if (header_desc) {
        memset(header_desc, 0, sizeof(*header_desc));

        header_desc->flags = gtp_h->flags;
        header_desc->type = gtp_h->type;
        header_desc->teid = be32toh(gtp_h->teid);
    }

    len = OGS_GTPV1U_HEADER_LEN;
    if (pkbuf->len < len) {
        ogs_error("the length of the packet is insufficient[%d:%d]",
                pkbuf->len, len);
        return -1;
    }

    if (gtp_h->flags & OGS_GTPU_FLAGS_E) {

        len += OGS_GTPV1U_EXTENSION_HEADER_LEN;
        if (pkbuf->len < len) {
            ogs_error("the length of the packet is insufficient[%d:%d]",
                    pkbuf->len, len);
            return -1;
        }

        /*
         * TS29.281
         * 5.2.1 General format of the GTP-U Extension Header
         *
         * If no such Header follows,
         * then the value of the Next Extension Header Type shall be 0. */

        i = 0;
        while (*(ext_h = (((uint8_t *)gtp_h) + len - 1)) &&
                i < OGS_GTP2_NUM_OF_EXTENSION_HEADER) {
        /*
         * The length of the Extension header shall be defined
         * in a variable length of 4 octets, i.e. m+1 = n*4 octets,
         * where n is a positive integer.
         */
            len += (*(++ext_h)) * 4;
            if (*ext_h == 0) {
                ogs_error("No length in the Extension header");
                return -1;
            }

            /* the PDU Session Container of a QoS monitoring packet is longer than the other headers */
            if (((*ext_h) * 4) > OGS_GTP2_MAX_EXTENSION_HEADER_LEN &&
                !(*(ext_h - 1) == OGS_GTP2_EXTENSION_HEADER_TYPE_PDU_SESSION_CONTAINER &&
                  ((*ext_h) * 4) <= OGS_GTP2_MAX_PDU_SESSION_CONTAINER_LEN)) {
                ogs_error("Overflow length : %d", (*ext_h));
                return -1;
            }

            if (pkbuf->len < len) {
                ogs_error("the length of the packet is insufficient[%d:%d]",
                        pkbuf->len, len);
                return -1;
            }

            if (!header_desc) /* Skip to extract header content */
                continue;

            /* Copy Header Content */
            memcpy(&ext_hdesc.array[i], ext_h-1,
                    ((*ext_h) * 4) > OGS_GTP2_MAX_EXTENSION_HEADER_LEN ?
                        OGS_GTP2_MAX_EXTENSION_HEADER_LEN : (*ext_h) * 4);

            switch (ext_hdesc.array[i].type) {
            case OGS_GTP2_EXTENSION_HEADER_TYPE_PDU_SESSION_CONTAINER:
                header_desc->pdu_type = ext_hdesc.array[i].pdu_type;
                if ((*ext_h) * 4 > OGS_GTP2_MAX_EXTENSION_HEADER_LEN)
                    parse_qos_monitoring(header_desc, ext_h + 1, (*ext_h) * 4 - 2);
                else if (ext_hdesc.array[i].pdu_type ==
                            OGS_GTP2_EXTENSION_HEADER_PDU_TYPE_DL_PDU_SESSION_INFORMATION)
                    header_desc->qos_flow_identifier = ext_hdesc.array[i].qos_flow_identifier;
                if (ext_hdesc.array[i].pdu_type ==
                    OGS_GTP2_EXTENSION_HEADER_PDU_TYPE_UL_PDU_SESSION_INFORMATION) {
                        header_desc->qos_flow_identifier =
                            ext_hdesc.array[i].qos_flow_identifier;
                        ogs_trace("   QFI [0x%x]",
                                header_desc->qos_flow_identifier);
                }
                break;
            case OGS_GTP2_EXTENSION_HEADER_TYPE_UDP_PORT:
                header_desc->udp.presence = true;
                header_desc->udp.port = be16toh(ext_hdesc.array[i].udp_port);

                ogs_trace("   UDP Port [%d]", header_desc->udp.port);
                break;
            case OGS_GTP2_EXTENSION_HEADER_TYPE_PDCP_NUMBER:
                header_desc->pdcp_number_presence = true;
                header_desc->pdcp_number =
                    be16toh(ext_hdesc.array[i].pdcp_number);

                ogs_trace("   PDCP Number [%d]", header_desc->pdcp_number);
                break;
            default:
                break;
            }

            i++;
        }

        if (i >= OGS_GTP2_NUM_OF_EXTENSION_HEADER) {
            ogs_error("The number of extension headers is limited to [%d]", i);
            return -1;
        }

    } else if (gtp_h->flags & (OGS_GTPU_FLAGS_S|OGS_GTPU_FLAGS_PN)) {
        /*
         * If and only if one or more of these three flags are set,
         * the fields Sequence Number, N-PDU and Extension Header
         * shall be present. The sender shall set all the bits of
         * the unused fields to zero. The receiver shall not evaluate
         * the unused fields.
         * For example, if only the E flag is set to 1, then
         * the N-PDU Number and Sequence Number fields shall also be present,
         * but will not have meaningful values and shall not be evaluated.
         */
        len += 4;
    }

    if (pkbuf->len < len) {
        ogs_error("the length of the packet is insufficient[%d:%d]",
                pkbuf->len, len);
        return -1;
    }

    return len;
}

uint16_t ogs_in_cksum(uint16_t *addr, int len)
{
    int nleft = len;
    uint32_t sum = 0;
    uint16_t *w = addr;
    uint16_t answer = 0;

    // Adding 16 bits sequentially in sum
    while (nleft > 1) {
        sum += *w;
        nleft -= 2;
        w++;
    }

    // If an odd byte is left
    if (nleft == 1) {
        *(uint8_t *) (&answer) = *(uint8_t *) w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}

void ogs_gtp2_sender_f_teid(
        ogs_gtp2_sender_f_teid_t *sender_f_teid, ogs_gtp2_message_t *message)
{
    ogs_gtp2_tlv_f_teid_t *tlv_f_teid = NULL;
    ogs_gtp2_f_teid_t *f_teid = NULL;

    ogs_assert(sender_f_teid);
    ogs_assert(message);

    memset(sender_f_teid, 0, sizeof(*sender_f_teid));

    switch (message->h.type) {
    case OGS_GTP2_CREATE_SESSION_REQUEST_TYPE:
        tlv_f_teid = &message->create_session_request.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_CREATE_SESSION_RESPONSE_TYPE:
        tlv_f_teid = &message->create_session_response.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_MODIFY_BEARER_REQUEST_TYPE:
        tlv_f_teid = &message->modify_bearer_request.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_DELETE_SESSION_REQUEST_TYPE:
        tlv_f_teid = &message->delete_session_request.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_MODIFY_BEARER_COMMAND_TYPE:
        tlv_f_teid = &message->modify_bearer_command.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_DELETE_BEARER_COMMAND_TYPE:
        tlv_f_teid = &message->delete_bearer_command.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_BEARER_RESOURCE_COMMAND_TYPE:
        tlv_f_teid = &message->bearer_resource_command.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_CREATE_INDIRECT_DATA_FORWARDING_TUNNEL_REQUEST_TYPE:
        tlv_f_teid = &message->create_indirect_data_forwarding_tunnel_request.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_CREATE_INDIRECT_DATA_FORWARDING_TUNNEL_RESPONSE_TYPE:
        tlv_f_teid = &message->create_indirect_data_forwarding_tunnel_response.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_DOWNLINK_DATA_NOTIFICATION_TYPE:
        tlv_f_teid = &message->downlink_data_notification.
            sender_f_teid_for_control_plane;
        break;
    case OGS_GTP2_MODIFY_ACCESS_BEARERS_REQUEST_TYPE:
        tlv_f_teid = &message->modify_access_bearers_request.
            sender_f_teid_for_control_plane;
    default:
        break;
    }

    if (tlv_f_teid && tlv_f_teid->presence && (f_teid = tlv_f_teid->data)) {
        sender_f_teid->teid_presence = true;
        sender_f_teid->teid = be32toh(f_teid->teid);
    }
}
