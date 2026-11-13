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

#include "ogs-pfcp.h"

const char *ogs_pfcp_cause_get_name(uint8_t cause)
{
    switch(cause) {
    case OGS_PFCP_CAUSE_REQUEST_ACCEPTED:
        return "OGS_PFCP_CAUSE_REQUEST_ACCEPTED";
        break;
    case OGS_PFCP_CAUSE_REQUEST_REJECTED:
        return "OGS_PFCP_CAUSE_REQUEST_REJECTED";
        break;
    case OGS_PFCP_CAUSE_SESSION_CONTEXT_NOT_FOUND:
        return "OGS_PFCP_CAUSE_SESSION_CONTEXT_NOT_FOUND";
        break;
    case OGS_PFCP_CAUSE_MANDATORY_IE_MISSING:
        return "OGS_PFCP_CAUSE_MANDATORY_IE_MISSING";
        break;
    case OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING:
        return "OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING";
        break;
    case OGS_PFCP_CAUSE_INVALID_LENGTH:
        return "OGS_PFCP_CAUSE_INVALID_LENGTH";
        break;
    case OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT:
        return "OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT";
        break;
    case OGS_PFCP_CAUSE_INVALID_FORWARDING_POLICY:
        return "OGS_PFCP_CAUSE_INVALID_FORWARDING_POLICY";
        break;
    case OGS_PFCP_CAUSE_INVALID_F_TEID_ALLOCATION_OPTION:
        return "OGS_PFCP_CAUSE_INVALID_F_TEID_ALLOCATION_OPTION";
        break;
    case OGS_PFCP_CAUSE_NO_ESTABLISHED_PFCP_ASSOCIATION:
        return "OGS_PFCP_CAUSE_NO_ESTABLISHED_PFCP_ASSOCIATION";
        break;
    case OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE:
        return "OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE";
        break;
    case OGS_PFCP_CAUSE_PFCP_ENTITY_IN_CONGESTION:
        return "OGS_PFCP_CAUSE_PFCP_ENTITY_IN_CONGESTION";
        break;
    case OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE:
        return "OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE";
        break;
    case OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED:
        return "OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED";
        break;
    case OGS_PFCP_CAUSE_SYSTEM_FAILURE:
        return "OGS_PFCP_CAUSE_SYSTEM_FAILURE";
        break;
    default:
        break;
    }
    return "OGS_PFCP_CAUSE_UNKNOWN";
}


void ogs_pfcp_mpx_free(ogs_pfcp_mpx_t *mpx, int num_of_mpx)
{
    int i;

    if (!mpx)
        return;
    for (i = 0; i < num_of_mpx; i++) {
        if (mpx[i].mid)
            ogs_free(mpx[i].mid);
    }
    ogs_free(mpx);
}

ogs_pfcp_mpx_t *ogs_pfcp_mpx_copy(const ogs_pfcp_mpx_t *mpx, int num_of_mpx)
{
    ogs_pfcp_mpx_t *copy = NULL;
    int i;

    if (!mpx || num_of_mpx <= 0)
        return NULL;
    copy = ogs_calloc(num_of_mpx, sizeof(*copy));
    ogs_assert(copy);
    memcpy(copy, mpx, num_of_mpx * sizeof(*copy));
    for (i = 0; i < num_of_mpx; i++) {
        copy[i].mid = NULL;
        if (mpx[i].mid && mpx[i].mid_len) {
            copy[i].mid = ogs_malloc(mpx[i].mid_len);
            ogs_assert(copy[i].mid);
            memcpy(copy[i].mid, mpx[i].mid, mpx[i].mid_len);
        }
    }
    return copy;
}

/* octet 1 of figure 8.2.5-2: SSRCPI bit 1, PTPI bit 2, MIDPI bit 3, RHIDMPI bit 4, RPTPI bit 5 */
static uint8_t mpx_flags(const ogs_pfcp_mpx_t *m)
{
    return (m->ssrc_presence ? 0x01 : 0) | (m->pt_presence ? 0x02 : 0) | (m->mid_presence ? 0x04 : 0) |
        (m->rhid_presence ? 0x08 : 0) | (m->rtcp_presence ? 0x10 : 0);
}

static int mpx_instance_len(const ogs_pfcp_mpx_t *m)
{
    return 1 + (m->ssrc_presence ? 4 : 0) + (m->pt_presence ? 1 : 0) + (m->mid_presence ? 1 + m->mid_len : 0) +
        (m->rhid_presence ? 1 : 0) + (m->rtcp_presence ? 1 : 0);
}

int ogs_pfcp_mpx_encoded_len(const ogs_pfcp_mpx_t *mpx, int num_of_mpx)
{
    int i, len = 1;     /* the Number of instances */

    for (i = 0; i < num_of_mpx; i++)
        len += 2 + mpx_instance_len(&mpx[i]);
    return len;
}

int16_t ogs_pfcp_build_sdf_filter(
        ogs_tlv_octet_t *octet, ogs_pfcp_sdf_filter_t *filter,
        void *data, int data_len)
{
    ogs_pfcp_sdf_filter_t target;
    int16_t size = 0;

    ogs_assert(filter);
    ogs_assert(octet);
    ogs_assert(data);
    ogs_assert(data_len);

    octet->data = data;
    memcpy(&target, filter, sizeof(ogs_pfcp_sdf_filter_t));

    ogs_assert(size + sizeof(target.flags) <= data_len);
    memcpy((unsigned char *)octet->data + size,
            &target.flags, sizeof(target.flags));
    size += sizeof(target.flags);

    ogs_assert(size + sizeof(target.spare2) <= data_len);
    memcpy((unsigned char *)octet->data + size,
            &target.spare2, sizeof(target.spare2));
    size += sizeof(target.spare2);

    if (target.fd) {
        ogs_assert(size + sizeof(target.flow_description_len) <= data_len);
        target.flow_description_len = htobe16(target.flow_description_len);
        memcpy((unsigned char *)octet->data + size,
                &target.flow_description_len,
                sizeof(target.flow_description_len));
        size += sizeof(target.flow_description_len);

        ogs_assert(size + filter->flow_description_len <= data_len);
        memcpy((char *)octet->data + size,
                filter->flow_description, filter->flow_description_len);
        size += filter->flow_description_len;
    }

    if (target.ttc) {
        ogs_assert(size + sizeof(target.tos_traffic_class) <= data_len);
        target.tos_traffic_class = htobe16(target.tos_traffic_class);
        memcpy((unsigned char *)octet->data + size,
                &target.tos_traffic_class, sizeof(target.tos_traffic_class));
        size += sizeof(target.tos_traffic_class);
    }

    if (target.spi) {
        ogs_assert(size + sizeof(target.security_parameter_index) <= data_len);
        target.security_parameter_index =
            htobe32(target.security_parameter_index);
        memcpy((unsigned char *)octet->data + size,
                &target.security_parameter_index,
                sizeof(target.security_parameter_index));
        size += sizeof(target.security_parameter_index);
    }

    if (target.fl) {
        int bit24_len = 3;
        ogs_assert(size + bit24_len <= data_len);
        target.flow_label = htobe32(target.flow_label);
        memcpy((unsigned char *)octet->data + size,
                &target.flow_label, bit24_len);
        size += bit24_len;
    }

    if (target.bid) {
        ogs_assert(size + sizeof(target.sdf_filter_id) <= data_len);
        target.sdf_filter_id =
            htobe32(target.sdf_filter_id);
        memcpy((unsigned char *)octet->data + size,
                &target.sdf_filter_id, sizeof(target.sdf_filter_id));
        size += sizeof(target.sdf_filter_id);
    }

    if (target.smmii) {
        int i;
        unsigned char *p = (unsigned char *)octet->data;

        ogs_assert(target.num_of_mpx >= 1 && target.mpx);
        ogs_assert(size + ogs_pfcp_mpx_encoded_len(target.mpx, target.num_of_mpx) <= data_len);
        p[size++] = target.num_of_mpx;
        for (i = 0; i < target.num_of_mpx; i++) {
            const ogs_pfcp_mpx_t *m = &target.mpx[i];
            uint16_t ilen = htobe16(mpx_instance_len(m));

            memcpy(p + size, &ilen, sizeof(ilen));
            size += sizeof(ilen);
            p[size++] = mpx_flags(m);
            if (m->ssrc_presence) {
                uint32_t ssrc = htobe32(m->ssrc);

                memcpy(p + size, &ssrc, sizeof(ssrc));
                size += sizeof(ssrc);
            }
            if (m->pt_presence)
                p[size++] = m->pt;
            if (m->mid_presence) {
                p[size++] = m->mid_len;
                if (m->mid_len) {
                    memcpy(p + size, m->mid, m->mid_len);
                    size += m->mid_len;
                }
            }
            if (m->rhid_presence)
                p[size++] = m->rhid;
            if (m->rtcp_presence)
                p[size++] = m->rtcp_type;
        }
    }

    octet->len = size;

    return octet->len;
}

int16_t ogs_pfcp_parse_sdf_filter(
        ogs_pfcp_sdf_filter_t *filter, ogs_tlv_octet_t *octet)
{
    uint32_t size = 0;

    ogs_assert(filter);
    ogs_assert(octet);

    memset(filter, 0, sizeof(ogs_pfcp_sdf_filter_t));

    if (size + sizeof(filter->flags) > octet->len) {
        ogs_error("size[%d]+sizeof(filter->flags)[%d] > IE Length[%d]",
                size, (int)sizeof(filter->flags), octet->len);
        return 0;
    }
    memcpy(&filter->flags,
            (unsigned char *)octet->data + size, sizeof(filter->flags));
    size++;

    if (size + sizeof(filter->spare2) > octet->len) {
        ogs_error("size[%d]+sizeof(filter->spare2)[%d] > IE Length[%d]",
                size, (int)sizeof(filter->spare2), octet->len);
        return 0;
    }
    memcpy(&filter->spare2,
            (unsigned char *)octet->data + size, sizeof(filter->flags));
    size++;

    if (filter->fd) {
        if (size + sizeof(filter->flow_description_len) > octet->len) {
            ogs_error("size[%d]+sizeof(filter->flow_description_len)[%d] "
                    "> IE Length[%d]",
                    size, (int)sizeof(filter->flow_description_len),
                    octet->len);
            return 0;
        }
        memcpy(&filter->flow_description_len,
                (unsigned char *)octet->data + size,
                sizeof(filter->flow_description_len));
        filter->flow_description_len = be16toh(filter->flow_description_len);
        size += sizeof(filter->flow_description_len);

        filter->flow_description = (char *)octet->data + size;
        size += filter->flow_description_len;
    }

    if (filter->ttc) {
        if (size + sizeof(filter->tos_traffic_class) > octet->len) {
            ogs_error("size[%d]+sizeof(filter->tos_traffic_class)[%d] "
                    "> IE Length[%d]",
                    size, (int)sizeof(filter->tos_traffic_class), octet->len);
            return 0;
        }
        memcpy(&filter->tos_traffic_class,
                (unsigned char *)octet->data + size,
                sizeof(filter->tos_traffic_class));
        filter->tos_traffic_class = be32toh(filter->tos_traffic_class);
        size += sizeof(filter->tos_traffic_class);
    }

    if (filter->spi) {
        if (size + sizeof(filter->security_parameter_index) > octet->len) {
            ogs_error("size[%d]+sizeof(filter->security_parameter_index)[%d] "
                    "> IE Length[%d]",
                    size, (int)sizeof(filter->security_parameter_index),
                    octet->len);
            return 0;
        }
        memcpy(&filter->security_parameter_index,
                (unsigned char *)octet->data + size,
                sizeof(filter->security_parameter_index));
        filter->security_parameter_index =
            be32toh(filter->security_parameter_index);
        size += sizeof(filter->security_parameter_index);
    }

    if (filter->fl) {
        int bit24_len = 3;
        if (size + bit24_len > octet->len) {
            ogs_error("size[%d]+bit24_len[%d] > IE Length[%d]",
                    size, bit24_len, octet->len);
            return 0;
        }
        memcpy(&filter->flow_label,
                (unsigned char *)octet->data + size, bit24_len);
        filter->flow_label = be32toh(filter->flow_label);
        size += bit24_len;
    }

    if (filter->bid) {
        if (size + sizeof(filter->sdf_filter_id) > octet->len) {
            ogs_error("size[%d]+sizeof(filter->sdf_filter_id)[%d]"
                    "> IE Length[%d]",
                    size, (int)sizeof(filter->sdf_filter_id), octet->len);
            return 0;
        }
        memcpy(&filter->sdf_filter_id, (unsigned char *)octet->data + size,
                sizeof(filter->sdf_filter_id));
        filter->sdf_filter_id = be32toh(filter->sdf_filter_id);
        size += sizeof(filter->sdf_filter_id);
    }

    if (filter->smmii) {
        const unsigned char *p = (const unsigned char *)octet->data;
        int i, n;

        if (size + 1 > octet->len) {
            ogs_error("No Number of (S)RTP Multiplexed Media Identification Information [IE Length:%d]", octet->len);
            return 0;
        }
        n = p[size++];
        if (n < 1) {
            ogs_error("Number of (S)RTP Multiplexed Media Identification Information is 0");
            return 0;
        }
        filter->mpx = ogs_calloc(n, sizeof(ogs_pfcp_mpx_t));
        ogs_assert(filter->mpx);
        for (i = 0; i < n; i++) {
            ogs_pfcp_mpx_t *m = &filter->mpx[i];
            uint16_t ilen;
            uint32_t end;
            uint8_t f;

            if (size + 3 > octet->len) {
                ogs_error("Truncated (S)RTP Multiplexed Media Identification Information %d", i);
                goto mpx_error;
            }
            memcpy(&ilen, p + size, sizeof(ilen));
            ilen = be16toh(ilen);
            size += sizeof(ilen);
            if (ilen < 1 || size + ilen > octet->len) {
                ogs_error("Invalid length %d of (S)RTP Multiplexed Media Identification Information %d", ilen, i);
                goto mpx_error;
            }
            end = size + ilen;
            f = p[size++];
            if (f & 0x01) {
                uint32_t ssrc;

                if (size + 4 > end) goto mpx_short;
                memcpy(&ssrc, p + size, sizeof(ssrc));
                m->ssrc = be32toh(ssrc);
                m->ssrc_presence = true;
                size += 4;
            }
            if (f & 0x02) {
                if (size + 1 > end) goto mpx_short;
                m->pt = p[size++] & 0x7f;
                m->pt_presence = true;
            }
            if (f & 0x04) {
                if (size + 1 > end) goto mpx_short;
                m->mid_len = p[size++];
                if (size + m->mid_len > end) goto mpx_short;
                m->mid_presence = true;
                if (m->mid_len) {
                    m->mid = ogs_malloc(m->mid_len);
                    ogs_assert(m->mid);
                    memcpy(m->mid, p + size, m->mid_len);
                }
                size += m->mid_len;
            }
            if (f & 0x08) {
                if (size + 1 > end) goto mpx_short;
                m->rhid = p[size++];
                m->rhid_presence = true;
            }
            if (f & 0x10) {
                if (size + 1 > end) goto mpx_short;
                m->rtcp_type = p[size++];
                m->rtcp_presence = true;
            }
            size = end;     /* octets that are present only if explicitly specified are skipped */
            filter->num_of_mpx = i + 1;
            continue;
mpx_short:
            ogs_error("(S)RTP Multiplexed Media Identification Information %d is shorter than its flags say", i);
mpx_error:
            ogs_pfcp_mpx_free(filter->mpx, i + 1);
            filter->mpx = NULL;
            filter->num_of_mpx = 0;
            return 0;
        }
    }

    if (size != octet->len)
        ogs_error("Mismatch IE Length[%d] != Decoded[%d]", octet->len, size);

    return size;
}

int16_t ogs_pfcp_build_bitrate(ogs_tlv_octet_t *octet,
        ogs_pfcp_bitrate_t *bitrate, void *data, int data_len)
{
    uint64_t target;
    int16_t size = 0;

    ogs_assert(bitrate);
    ogs_assert(octet);
    ogs_assert(data);
    ogs_assert(data_len >= OGS_PFCP_BITRATE_LEN);

    octet->data = data;

    /*
     * Ch 8.15 Bearer QoS in TS 29.274 v15.9.0
     *
     * The UL/DL MBR and GBR fields are encoded as kilobits
     * per second (1 kbps = 1000 bps) in binary value.
     * The UL/DL MBR and GBR fields may require converting values
     * in bits per second to kilobits per second when the UL/DL MBR
     * and GBR values are received from an interface other than GTPv2
     * interface. If such conversions result in fractions, then
     * the value of UL/DL MBR and GBR fields shall be rounded upwards.
     */
    target = (bitrate->uplink / 1000) + ((bitrate->uplink % 1000) ? 1 : 0);
    ogs_uint64_to_buffer(target, 5,
            (unsigned char *)octet->data + size);
    size += 5;

    target = (bitrate->downlink / 1000) + ((bitrate->downlink % 1000) ? 1 : 0);
    ogs_uint64_to_buffer(target, 5,
            (unsigned char *)octet->data + size);
    size += 5;

    octet->len = size;

    return octet->len;
}
int16_t ogs_pfcp_parse_bitrate(
        ogs_pfcp_bitrate_t *bitrate, ogs_tlv_octet_t *octet)
{
    int16_t size = 0;

    ogs_assert(bitrate);
    ogs_assert(octet);
    if (octet->len != OGS_PFCP_BITRATE_LEN) {
        ogs_error("Mismatch IE Length[%d] != Decoded[%d]",
                OGS_PFCP_BITRATE_LEN, octet->len);
        return 0;
    }

    memset(bitrate, 0, sizeof(ogs_pfcp_bitrate_t));

    /*
     * Ch 8.15 Bearer QoS in TS 29.274 v15.9.0
     *
     * The UL/DL MBR and GBR fields are encoded as kilobits
     * per second (1 kbps = 1000 bps) in binary value.
     */
    bitrate->uplink = ogs_buffer_to_uint64(
            (unsigned char *)octet->data + size, 5) * 1000;
    size += 5;
    bitrate->downlink = ogs_buffer_to_uint64(
            (unsigned char *)octet->data + size, 5) * 1000;
    size += 5;

    ogs_assert(size == octet->len);

    return size;
}

int16_t ogs_pfcp_build_volume(ogs_tlv_octet_t *octet,
        ogs_pfcp_volume_threshold_t *volume, void *data, int data_len)
{
    ogs_pfcp_volume_threshold_t target;
    int16_t size = 0;

    ogs_assert(volume);
    ogs_assert(octet);
    ogs_assert(data);
    ogs_assert(data_len >= sizeof(ogs_pfcp_volume_threshold_t));

    ogs_assert(volume->flags);

    octet->data = data;
    memcpy(&target, volume, sizeof(ogs_pfcp_volume_threshold_t));

    ((unsigned char *)octet->data)[size] = target.flags;
    size += sizeof(target.flags);

    if (target.tovol) {
        target.total_volume = htobe64(target.total_volume);
        memcpy((unsigned char *)octet->data + size,
                &target.total_volume, sizeof(target.total_volume));
        size += sizeof(target.total_volume);
    }
    if (target.ulvol) {
        target.uplink_volume = htobe64(target.uplink_volume);
        memcpy((unsigned char *)octet->data + size,
                &target.uplink_volume, sizeof(target.uplink_volume));
        size += sizeof(target.uplink_volume);
    }
    if (target.dlvol) {
        target.downlink_volume = htobe64(target.downlink_volume);
        memcpy((unsigned char *)octet->data + size,
                &target.downlink_volume, sizeof(target.downlink_volume));
        size += sizeof(target.downlink_volume);
    }

    octet->len = size;

    return octet->len;
}

void ogs_pfcp_parse_usage_report_trigger(
        ogs_pfcp_usage_report_trigger_t *rep_trig,
        ogs_pfcp_tlv_usage_report_trigger_t *tlv)
{
    rep_trig->reptri_5 = (tlv->u24 >> 16) & 0xff;
    rep_trig->reptri_6 = (tlv->u24 >> 8) & 0xff;
    rep_trig->reptri_7 = (tlv->u24) & 0xff;
}

int16_t ogs_pfcp_parse_volume(
        ogs_pfcp_volume_threshold_t *volume, ogs_tlv_octet_t *octet)
{
    int16_t size = 0;

    ogs_assert(volume);
    ogs_assert(octet);

    memset(volume, 0, sizeof(ogs_pfcp_volume_threshold_t));

    volume->flags = ((unsigned char *)octet->data)[size];
    size += sizeof(volume->flags);

    if (volume->tovol) {
        if (size + sizeof(volume->total_volume) > octet->len) {
            ogs_error("size[%d]+sizeof(volume->total_volume)[%d] "
                    "> IE Length[%d]",
                    size, (int)sizeof(volume->total_volume), octet->len);
            return 0;
        }
        memcpy(&volume->total_volume, (unsigned char *)octet->data + size,
                sizeof(volume->total_volume));
        volume->total_volume = be64toh(volume->total_volume);
        size += sizeof(volume->total_volume);
    }
    if (volume->ulvol) {
        if (size + sizeof(volume->uplink_volume) > octet->len) {
            ogs_error("size[%d]+sizeof(volume->uplink_volume)[%d] "
                    "> IE Length[%d]",
                    size, (int)sizeof(volume->uplink_volume), octet->len);
            return 0;
        }
        memcpy(&volume->uplink_volume, (unsigned char *)octet->data + size,
                sizeof(volume->uplink_volume));
        volume->uplink_volume = be64toh(volume->uplink_volume);
        size += sizeof(volume->uplink_volume);
    }
    if (volume->dlvol) {
        if (size + sizeof(volume->downlink_volume) > octet->len) {
            ogs_error("size[%d]+sizeof(volume->downlink_volume)[%d] "
                    "> IE Length[%d]",
                    size, (int)sizeof(volume->downlink_volume), octet->len);
            return 0;
        }
        memcpy(&volume->downlink_volume, (unsigned char *)octet->data + size,
                sizeof(volume->downlink_volume));
        volume->downlink_volume = be64toh(volume->downlink_volume);
        size += sizeof(volume->downlink_volume);
    }

    if (size != octet->len)
        ogs_error("Mismatch IE Length[%d] != Decoded[%d]", octet->len, size);

    return size;
}

int16_t ogs_pfcp_build_dropped_dl_traffic_threshold(
        ogs_tlv_octet_t *octet,
        ogs_pfcp_dropped_dl_traffic_threshold_t *threshold,
        void *data, int data_len)
{
    ogs_pfcp_dropped_dl_traffic_threshold_t target;
    int16_t size = 0;

    ogs_assert(threshold);
    ogs_assert(octet);
    ogs_assert(data);
    ogs_assert(data_len >= sizeof(ogs_pfcp_dropped_dl_traffic_threshold_t));

    ogs_assert(threshold->flags);

    octet->data = data;
    memcpy(&target, threshold, sizeof(ogs_pfcp_dropped_dl_traffic_threshold_t));

    ((unsigned char *)octet->data)[size] = target.flags;
    size += sizeof(target.flags);

    if (target.dlpa) {
        target.downlink_packets = htobe64(target.downlink_packets);
        memcpy((unsigned char *)octet->data + size,
                &target.downlink_packets, sizeof(target.downlink_packets));
        size += sizeof(target.downlink_packets);
    }

    if (target.dlby) {
        target.number_of_bytes_of_downlink_data =
            htobe64(target.number_of_bytes_of_downlink_data);
        memcpy((unsigned char *)octet->data + size,
                &target.number_of_bytes_of_downlink_data,
                sizeof(target.number_of_bytes_of_downlink_data));
        size += sizeof(target.number_of_bytes_of_downlink_data);
    }

    octet->len = size;

    return octet->len;
}
int16_t ogs_pfcp_parse_dropped_dl_traffic_threshold(
        ogs_pfcp_dropped_dl_traffic_threshold_t *threshold,
        ogs_tlv_octet_t *octet)
{
    int16_t size = 0;

    ogs_assert(threshold);
    ogs_assert(octet);

    memset(threshold, 0, sizeof(ogs_pfcp_dropped_dl_traffic_threshold_t));

    threshold->flags = ((unsigned char *)octet->data)[size];
    size += sizeof(threshold->flags);

    if (threshold->dlpa) {
        memcpy(&threshold->downlink_packets,
                (unsigned char *)octet->data + size,
                sizeof(threshold->downlink_packets));
        threshold->downlink_packets = be64toh(threshold->downlink_packets);
        size += sizeof(threshold->downlink_packets);
    }
    if (threshold->dlby) {
        memcpy(&threshold->number_of_bytes_of_downlink_data,
                (unsigned char *)octet->data + size,
                sizeof(threshold->number_of_bytes_of_downlink_data));
        threshold->number_of_bytes_of_downlink_data =
            be64toh(threshold->number_of_bytes_of_downlink_data);
        size += sizeof(threshold->number_of_bytes_of_downlink_data);
    }

    if (size != octet->len)
        ogs_error("Mismatch IE Length[%d] != Decoded[%d]", octet->len, size);

    return size;
}

/* An information element inside a grouped one: type, length and value (TS 29.244 V19.6.0 clause 8.1.1) */
static int put_ie(uint8_t *p, uint16_t type, const uint8_t *value, uint16_t len)
{
    p[0] = type >> 8; p[1] = type & 0xff;
    p[2] = len >> 8; p[3] = len & 0xff;
    memcpy(p + 4, value, len);
    return 4 + len;
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = v >> 24; p[1] = (v >> 16) & 0xff; p[2] = (v >> 8) & 0xff; p[3] = v & 0xff;
}

static uint32_t get_u32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

int16_t ogs_pfcp_build_qos_monitoring_control(ogs_tlv_octet_t *octet,
        const ogs_pfcp_qos_monitoring_control_t *control, void *data, int data_len)
{
    uint8_t *p = data, v[16];
    int size = 0, n;

    ogs_assert(octet);
    ogs_assert(control);
    ogs_assert(data);
    ogs_assert(data_len >= 96);

    /* QFI, Requested QoS Monitoring (octets 5 and 6), Reporting Frequency */
    v[0] = control->qfi & 0x3f;
    size += put_ie(p + size, OGS_PFCP_QFI_TYPE, v, 1);
    v[0] = (control->dl_delay ? 0x01 : 0) | (control->ul_delay ? 0x02 : 0) | (control->rt_delay ? 0x04 : 0);
    v[1] = 0;
    size += put_ie(p + size, OGS_PFCP_REQUESTED_QOS_MONITORING_TYPE, v, 2);
    v[0] = (control->event_triggered ? 0x01 : 0) | (control->periodic ? 0x02 : 0);
    size += put_ie(p + size, OGS_PFCP_REPORTING_FREQUENCY_TYPE, v, 1);

    if (control->thr_dl_presence || control->thr_ul_presence || control->thr_rt_presence) {
        n = 1;
        v[0] = (control->thr_dl_presence ? 0x01 : 0) | (control->thr_ul_presence ? 0x02 : 0) | (control->thr_rt_presence ? 0x04 : 0);
        if (control->thr_dl_presence) { put_u32(v + n, control->thr_dl); n += 4; }
        if (control->thr_ul_presence) { put_u32(v + n, control->thr_ul); n += 4; }
        if (control->thr_rt_presence) { put_u32(v + n, control->thr_rt); n += 4; }
        size += put_ie(p + size, OGS_PFCP_PACKET_DELAY_THRESHOLDS_TYPE, v, n);
    }
    if (control->min_wait_presence) {
        put_u32(v, control->min_wait);
        size += put_ie(p + size, OGS_PFCP_MINIMUM_WAIT_TIME_TYPE, v, 4);
    }
    if (control->period_presence) {
        put_u32(v, control->period);
        size += put_ie(p + size, OGS_PFCP_MEASUREMENT_PERIOD_TYPE, v, 4);
    }

    octet->data = data;
    octet->len = size;
    return size;
}

int16_t ogs_pfcp_parse_qos_monitoring_control(
        ogs_pfcp_qos_monitoring_control_t *control, const ogs_tlv_octet_t *octet)
{
    const uint8_t *p = octet->data;
    int pos = 0;

    memset(control, 0, sizeof(*control));
    while (pos + 4 <= octet->len) {
        uint16_t type = (p[pos] << 8) | p[pos + 1];
        uint16_t len = (p[pos + 2] << 8) | p[pos + 3];
        const uint8_t *v = p + pos + 4;

        if (pos + 4 + len > octet->len) {
            ogs_error("Invalid length in QoS Monitoring per QoS flow Control Information");
            return -1;
        }
        switch (type) {
        case OGS_PFCP_QFI_TYPE:
            if (len >= 1) control->qfi = v[0] & 0x3f;
            break;
        case OGS_PFCP_REQUESTED_QOS_MONITORING_TYPE:
            if (len >= 1) {
                control->dl_delay = v[0] & 0x01;
                control->ul_delay = v[0] & 0x02;
                control->rt_delay = v[0] & 0x04;
            }
            break;
        case OGS_PFCP_REPORTING_FREQUENCY_TYPE:
            if (len >= 1) {
                control->event_triggered = v[0] & 0x01;
                control->periodic = v[0] & 0x02;
            }
            break;
        case OGS_PFCP_PACKET_DELAY_THRESHOLDS_TYPE:
            if (len >= 1) {
                int n = 1;

                if ((v[0] & 0x01) && n + 4 <= len) { control->thr_dl_presence = true; control->thr_dl = get_u32(v + n); n += 4; }
                if ((v[0] & 0x02) && n + 4 <= len) { control->thr_ul_presence = true; control->thr_ul = get_u32(v + n); n += 4; }
                if ((v[0] & 0x04) && n + 4 <= len) { control->thr_rt_presence = true; control->thr_rt = get_u32(v + n); n += 4; }
            }
            break;
        case OGS_PFCP_MINIMUM_WAIT_TIME_TYPE:
            if (len >= 4) { control->min_wait_presence = true; control->min_wait = get_u32(v); }
            break;
        case OGS_PFCP_MEASUREMENT_PERIOD_TYPE:
            if (len >= 4) { control->period_presence = true; control->period = get_u32(v); }
            break;
        default:
            break;
        }
        pos += 4 + len;
    }
    return octet->len;
}

int16_t ogs_pfcp_build_qos_monitoring_measurement(ogs_tlv_octet_t *octet,
        const ogs_pfcp_qos_monitoring_measurement_t *measurement, void *data, int data_len)
{
    uint8_t *p = data;
    int size = 1;

    ogs_assert(octet);
    ogs_assert(measurement);
    ogs_assert(data);
    ogs_assert(data_len >= 16);

    p[0] = (measurement->dl_presence ? 0x01 : 0) | (measurement->ul_presence ? 0x02 : 0) |
        (measurement->rt_presence ? 0x04 : 0) | (measurement->failure ? 0x08 : 0);
    if (measurement->dl_presence) { put_u32(p + size, measurement->dl); size += 4; }
    if (measurement->ul_presence) { put_u32(p + size, measurement->ul); size += 4; }
    if (measurement->rt_presence) { put_u32(p + size, measurement->rt); size += 4; }

    octet->data = data;
    octet->len = size;
    return size;
}

int16_t ogs_pfcp_parse_qos_monitoring_measurement(
        ogs_pfcp_qos_monitoring_measurement_t *measurement, const ogs_tlv_octet_t *octet)
{
    const uint8_t *p = octet->data;
    int n = 1;

    memset(measurement, 0, sizeof(*measurement));
    if (octet->len < 1)
        return -1;
    measurement->failure = p[0] & 0x08;
    if (p[0] & 0x01) { if (n + 4 > octet->len) return -1; measurement->dl_presence = true; measurement->dl = get_u32(p + n); n += 4; }
    if (p[0] & 0x02) { if (n + 4 > octet->len) return -1; measurement->ul_presence = true; measurement->ul = get_u32(p + n); n += 4; }
    if (p[0] & 0x04) { if (n + 4 > octet->len) return -1; measurement->rt_presence = true; measurement->rt = get_u32(p + n); n += 4; }
    return n;
}

int16_t ogs_pfcp_build_volume_measurement(ogs_tlv_octet_t *octet,
        ogs_pfcp_volume_measurement_t *volume, void *data, int data_len)
{
    ogs_pfcp_volume_measurement_t target;
    int16_t size = 0;

    ogs_assert(volume);
    ogs_assert(octet);
    ogs_assert(data);
    ogs_assert(data_len >= sizeof(ogs_pfcp_volume_measurement_t));

    ogs_assert(volume->flags);

    octet->data = data;
    memcpy(&target, volume, sizeof(ogs_pfcp_volume_measurement_t));

    ((unsigned char *)octet->data)[size] = target.flags;
    size += sizeof(target.flags);

    if (target.tovol) {
        target.total_volume = htobe64(target.total_volume);
        memcpy((unsigned char *)octet->data + size,
                &target.total_volume, sizeof(target.total_volume));
        size += sizeof(target.total_volume);
    }
    if (target.ulvol) {
        target.uplink_volume = htobe64(target.uplink_volume);
        memcpy((unsigned char *)octet->data + size,
                &target.uplink_volume, sizeof(target.uplink_volume));
        size += sizeof(target.uplink_volume);
    }
    if (target.dlvol) {
        target.downlink_volume = htobe64(target.downlink_volume);
        memcpy((unsigned char *)octet->data + size,
                &target.downlink_volume, sizeof(target.downlink_volume));
        size += sizeof(target.downlink_volume);
    }
    if (target.tonop) {
        target.total_n_packets = htobe64(target.total_n_packets);
        memcpy((unsigned char *)octet->data + size,
                &target.total_n_packets, sizeof(target.total_n_packets));
        size += sizeof(target.total_n_packets);
    }
    if (target.ulnop) {
        target.uplink_n_packets = htobe64(target.uplink_n_packets);
        memcpy((unsigned char *)octet->data + size,
                &target.uplink_n_packets, sizeof(target.uplink_n_packets));
        size += sizeof(target.uplink_n_packets);
    }
    if (target.dlnop) {
        target.downlink_n_packets = htobe64(target.downlink_n_packets);
        memcpy((unsigned char *)octet->data + size,
                &target.downlink_n_packets, sizeof(target.downlink_n_packets));
        size += sizeof(target.downlink_n_packets);
    }

    octet->len = size;

    return octet->len;
}

int16_t ogs_pfcp_parse_volume_measurement(
        ogs_pfcp_volume_measurement_t *volume, ogs_tlv_octet_t *octet)
{
    int16_t size = 0;

    ogs_assert(volume);
    ogs_assert(octet);

    memset(volume, 0, sizeof(ogs_pfcp_volume_measurement_t));

    volume->flags = ((unsigned char *)octet->data)[size];
    size += sizeof(volume->flags);

    if (volume->tovol) {
        memcpy(&volume->total_volume, (unsigned char *)octet->data + size,
                sizeof(volume->total_volume));
        volume->total_volume = be64toh(volume->total_volume);
        size += sizeof(volume->total_volume);
    }
    if (volume->ulvol) {
        memcpy(&volume->uplink_volume, (unsigned char *)octet->data + size,
                sizeof(volume->uplink_volume));
        volume->uplink_volume = be64toh(volume->uplink_volume);
        size += sizeof(volume->uplink_volume);
    }
    if (volume->dlvol) {
        memcpy(&volume->downlink_volume, (unsigned char *)octet->data + size,
                sizeof(volume->downlink_volume));
        volume->downlink_volume = be64toh(volume->downlink_volume);
        size += sizeof(volume->downlink_volume);
    }
    if (volume->tonop) {
        memcpy(&volume->total_n_packets, (unsigned char *)octet->data + size,
                sizeof(volume->total_n_packets));
        volume->total_n_packets = be64toh(volume->total_n_packets);
        size += sizeof(volume->total_n_packets);
    }
    if (volume->ulnop) {
        memcpy(&volume->uplink_n_packets, (unsigned char *)octet->data + size,
                sizeof(volume->uplink_n_packets));
        volume->uplink_n_packets = be64toh(volume->uplink_n_packets);
        size += sizeof(volume->uplink_n_packets);
    }
    if (volume->dlnop) {
        memcpy(&volume->downlink_n_packets, (unsigned char *)octet->data + size,
                sizeof(volume->downlink_n_packets));
        volume->downlink_n_packets = be64toh(volume->downlink_n_packets);
        size += sizeof(volume->downlink_n_packets);
    }

    if (size != octet->len)
        ogs_error("Mismatch IE Length[%d] != Decoded[%d]", octet->len, size);

    return size;
}

int16_t ogs_pfcp_build_user_id(
        ogs_tlv_octet_t *octet, ogs_pfcp_user_id_t *user_id,
        void *data, int data_len)
{
    ogs_pfcp_user_id_t target;
    int16_t size = 0;

    ogs_assert(user_id);
    ogs_assert(octet);
    ogs_assert(data);
    ogs_assert(data_len);

    octet->data = data;
    memcpy(&target, user_id, sizeof(ogs_pfcp_user_id_t));

    ogs_assert(size + sizeof(target.flags) <= data_len);
    memcpy((unsigned char *)octet->data + size,
            &target.flags, sizeof(target.flags));
    size += sizeof(target.flags);

    if (target.imsif) {
        ogs_assert(size + sizeof(target.imsi_len) <= data_len);
        memcpy((unsigned char *)octet->data + size,
                &target.imsi_len, sizeof(target.imsi_len));
        size += sizeof(target.imsi_len);

        ogs_assert(size + user_id->imsi_len <= data_len);
        memcpy((char *)octet->data + size, user_id->imsi, user_id->imsi_len);
        size += user_id->imsi_len;
    }
    if (target.imeif) {
        ogs_assert(size + sizeof(target.imeisv_len) <= data_len);
        memcpy((unsigned char *)octet->data + size,
                &target.imeisv_len, sizeof(target.imeisv_len));
        size += sizeof(target.imeisv_len);

        ogs_assert(size + user_id->imeisv_len <= data_len);
        memcpy((char *)octet->data + size,
                user_id->imeisv, user_id->imeisv_len);
        size += user_id->imeisv_len;
    }
    if (target.msisdnf) {
        ogs_assert(size + sizeof(target.msisdn_len) <= data_len);
        memcpy((unsigned char *)octet->data + size,
                &target.msisdn_len, sizeof(target.msisdn_len));
        size += sizeof(target.msisdn_len);

        ogs_assert(size + user_id->msisdn_len <= data_len);
        memcpy((char *)octet->data + size,
                user_id->msisdn, user_id->msisdn_len);
        size += user_id->msisdn_len;
    }

    octet->len = size;

    return octet->len;
}

int16_t ogs_pfcp_build_mbs_session_identifier(ogs_tlv_octet_t *octet,
        ogs_pfcp_mbs_session_identifier_t *mbs_session_identifier, void *data, int data_len)
{
    ogs_pfcp_mbs_session_identifier_t target;
    int16_t size = 0;

    ogs_assert(mbs_session_identifier);
    ogs_assert(octet);
    ogs_assert(data);
    ogs_assert(data_len);

    octet->data = data;
    memcpy(&target, mbs_session_identifier, sizeof(ogs_pfcp_mbs_session_identifier_t));

    ogs_assert(size + sizeof(target.octet5) <= data_len);
    memcpy((uint8_t *)octet->data + size,
            &target.octet5, sizeof(target.octet5));
    size += sizeof(target.octet5);

    if (target.tmgif) {
        ogs_assert(size + sizeof(target.tmgi.mbs_service_id) <= data_len);
        memcpy((uint8_t *)octet->data + size, &target.tmgi.mbs_service_id,
            sizeof(target.tmgi.mbs_service_id));
        size += sizeof(target.tmgi.mbs_service_id);

        ogs_assert(size + sizeof(target.tmgi.plmn_id) <= data_len);
        memcpy((uint8_t *)octet->data + size, &target.tmgi.plmn_id,
            sizeof(target.tmgi.plmn_id));
        size += sizeof(target.tmgi.plmn_id);
#if 0
    } else {
        size += 6;
#endif
    }

    if (target.ssmif) {
        // TODO (borieher): Add SSM
        ((uint8_t *)octet->data)[size] = target.ssm.ip_multicast_distribution_address.flags;
        size += 1;

        memcpy((uint8_t *)octet->data + size, target.ssm.ip_multicast_distribution_address.ipv6_addr, target.ssm.ip_multicast_distribution_address.address_length);
        size += target.ssm.ip_multicast_distribution_address.address_length;

        ((uint8_t *)octet->data)[size] = target.ssm.ip_source_address.flags;
        size += 1;

        memcpy((uint8_t *)octet->data + size, target.ssm.ip_source_address.ipv6_addr, target.ssm.ip_source_address.address_length);
        size += target.ssm.ip_source_address.address_length;
    }

    if (target.nidif) {
        // TODO (borieher): Add NID
        memcpy((uint8_t *)octet->data + size, target.nid, sizeof(target.nid));
	size += sizeof(target.nid);
    }

    octet->len = size;

    return octet->len;
}

/* Copies an IPv4 or IPv6 address of the given length into ip (kept in network byte order, as ogs_ip_t does). */
static void set_ip_from_octets(ogs_ip_t *ip, bool v6, const uint8_t *data)
{
    if (v6) {
        ip->ipv6 = 1;
        ip->len = OGS_IPV6_LEN;
        memcpy(ip->addr6, data, OGS_IPV6_LEN);
    } else {
        ip->ipv4 = 1;
        ip->len = OGS_IPV4_LEN;
        memcpy(&ip->addr, data, OGS_IPV4_LEN);
    }
}

int16_t ogs_pfcp_build_ip_multicast_address(
        void *buf, const ogs_pfcp_ip_multicast_address_t *address)
{
    uint8_t *p = buf;
    int flags;

    ogs_assert(buf);
    ogs_assert(address);

    /* "One and only one of the V6, V4 and A flags shall be set to "1". The R flag may be set if the V6 or the V4
     * flag is set to "1"." (cl. 8.2.137) */
    flags = (address->v6 ? 1 : 0) + (address->v4 ? 1 : 0) + (address->a ? 1 : 0);
    if (flags != 1 || (address->r && address->a)) {
        ogs_error("IP Multicast Address flags 0x%02x are not valid", address->octet5);
        return -1;
    }

    /* octet 5: spare "0", A, R, V4, V6 */
    *p++ = (address->octet5 & 0x0f);
    if (address->a)
        return 1;

    if (address->v4) {
        memcpy(p, &address->s_ipv4_addr, OGS_IPV4_LEN);
        p += OGS_IPV4_LEN;
    }
    if (address->v6) {
        memcpy(p, address->s_ipv6_addr, OGS_IPV6_LEN);
        p += OGS_IPV6_LEN;
    }
    if (address->r) {
        if (address->v4) {
            memcpy(p, &address->e_ipv4_addr, OGS_IPV4_LEN);
            p += OGS_IPV4_LEN;
        }
        if (address->v6) {
            memcpy(p, address->e_ipv6_addr, OGS_IPV6_LEN);
            p += OGS_IPV6_LEN;
        }
    }

    return p - (uint8_t *)buf;
}

int16_t ogs_pfcp_build_source_ip_address(
        void *buf, const ogs_pfcp_source_ip_address_t *address)
{
    uint8_t *p = buf;

    ogs_assert(buf);
    ogs_assert(address);

    if (!address->v4 && !address->v6) {
        ogs_error("Source IP Address flags 0x%02x carry no address", address->octet5);
        return -1;
    }

    /* octet 5: spare "0", MPL, V4, V6; then the IPv4 address, the IPv6 address, the mask/prefix length */
    *p++ = (address->octet5 & 0x07);
    if (address->v4) {
        memcpy(p, &address->ipv4_addr, OGS_IPV4_LEN);
        p += OGS_IPV4_LEN;
    }
    if (address->v6) {
        memcpy(p, address->ipv6_addr, OGS_IPV6_LEN);
        p += OGS_IPV6_LEN;
    }
    if (address->mpl)
        *p++ = address->mask_prefix_len;

    return p - (uint8_t *)buf;
}

int16_t ogs_pfcp_parse_ip_multicast_address(
        ogs_pfcp_decoded_ip_multicast_address_t *decoded, const ogs_tlv_octet_t *octet)
{
    const uint8_t *data;
    uint32_t len, want, pos;
    bool v6, v4, r, a;

    ogs_assert(decoded);
    ogs_assert(octet);

    memset(decoded, 0, sizeof(*decoded));
    data = octet->data;
    len = octet->len;
    if (data == NULL || len < 1) {
        ogs_error("IP Multicast Address without its flags octet");
        return -1;
    }

    v6 = data[0] & 0x01;    /* bit 1 */
    v4 = data[0] & 0x02;    /* bit 2 */
    r  = data[0] & 0x04;    /* bit 3 */
    a  = data[0] & 0x08;    /* bit 4 */

    /* TS 29.244 cl. 8.2.137: "One and only one of the V6, V4 and A flags shall be set to "1". The R flag may be
     * set if the V6 or the V4 flag is set to "1"." */
    if ((v6 + v4 + a) != 1 || (r && a)) {
        ogs_error("IP Multicast Address flags 0x%02x are not valid", data[0]);
        return -1;
    }

    want = 1;
    if (v4) want += OGS_IPV4_LEN * (r ? 2 : 1);
    if (v6) want += OGS_IPV6_LEN * (r ? 2 : 1);
    if (len != want) {
        ogs_error("IP Multicast Address length %u, flags 0x%02x need %u", len, data[0], want);
        return -1;
    }

    decoded->any = a;
    decoded->range = r;
    pos = 1;
    if (v4 || v6) {
        set_ip_from_octets(&decoded->start, v6, data + pos);
        pos += v6 ? OGS_IPV6_LEN : OGS_IPV4_LEN;
        if (r)
            set_ip_from_octets(&decoded->end, v6, data + pos);
    }
    return len;
}

int16_t ogs_pfcp_parse_source_ip_address(
        ogs_pfcp_decoded_source_ip_address_t *decoded, const ogs_tlv_octet_t *octet)
{
    const uint8_t *data;
    uint32_t len, want, pos;
    bool v6, v4, mpl;

    ogs_assert(decoded);
    ogs_assert(octet);

    memset(decoded, 0, sizeof(*decoded));
    data = octet->data;
    len = octet->len;
    if (data == NULL || len < 1) {
        ogs_error("Source IP Address without its flags octet");
        return -1;
    }

    v6  = data[0] & 0x01;   /* bit 1 */
    v4  = data[0] & 0x02;   /* bit 2 */
    mpl = data[0] & 0x04;   /* bit 3 */

    if (!v4 && !v6) {
        ogs_error("Source IP Address flags 0x%02x carry no address", data[0]);
        return -1;
    }

    want = 1 + (v4 ? OGS_IPV4_LEN : 0) + (v6 ? OGS_IPV6_LEN : 0) + (mpl ? 1 : 0);
    if (len != want) {
        ogs_error("Source IP Address length %u, flags 0x%02x need %u", len, data[0], want);
        return -1;
    }

    /* Octets follow the flags in the order of figure 8.2.138-1: IPv4, IPv6, mask/prefix length. */
    pos = 1;
    if (v4) {
        decoded->addr.ipv4 = 1;
        memcpy(&decoded->addr.addr, data + pos, OGS_IPV4_LEN);
        pos += OGS_IPV4_LEN;
    }
    if (v6) {
        decoded->addr.ipv6 = 1;
        memcpy(decoded->addr.addr6, data + pos, OGS_IPV6_LEN);
        pos += OGS_IPV6_LEN;
    }
    decoded->addr.len = (v4 ? OGS_IPV4_LEN : 0) + (v6 ? OGS_IPV6_LEN : 0);
    if (mpl) {
        decoded->has_mask_prefix_len = true;
        decoded->mask_prefix_len = data[pos];
    }
    return len;
}

/*
 * Decodes one Address Type / Address Length / Address field of the Source Specific IP Multicast
 * Address (TS 29.244 cl. 8.2.206 and 8.2.207: type 0 with length 4, or type 1 with length 16; "Other
 * combinations of values are not valid."). Returns the octets consumed, or -1 if the field is not valid
 * or does not fit in the IE.
 */
static int parse_ssm_address(uint8_t *flags, void *addr, const uint8_t *data, uint32_t data_len)
{
    uint8_t type, length;

    if (data_len < 1)
        return -1;
    *flags = data[0];
    type = data[0] >> 6;
    length = data[0] & 0x3f;

    if (!((type == 0 && length == OGS_IPV4_LEN) || (type == 1 && length == OGS_IPV6_LEN))) {
        ogs_error("Invalid address type %u / length %u in the MBS Session Identifier", type, length);
        return -1;
    }
    if (data_len < 1u + length) {
        ogs_error("MBS Session Identifier too short for its address");
        return -1;
    }
    memcpy(addr, data + 1, length);
    return 1 + length;
}

int16_t ogs_pfcp_parse_mbs_session_identifier(
        ogs_pfcp_mbs_session_identifier_t *mbs_session_identifier, ogs_tlv_octet_t *octet)
{
    uint32_t size = 0;
    int rv;

    ogs_assert(mbs_session_identifier);
    ogs_assert(octet);

    uint8_t *data = (uint8_t*)octet->data;
    uint32_t data_len = octet->len;

    memset(mbs_session_identifier, 0, sizeof(ogs_pfcp_mbs_session_identifier_t));

    if (data == NULL || data_len < sizeof(mbs_session_identifier->octet5)) {
        ogs_error("MBS Session Identifier without its flags octet");
        return -1;
    }
    mbs_session_identifier->octet5 = data[size];
    size += sizeof(mbs_session_identifier->octet5);

    if (mbs_session_identifier->tmgif) {
        if (size + sizeof(mbs_session_identifier->tmgi.mbs_service_id) +
                sizeof(mbs_session_identifier->tmgi.plmn_id) > data_len) {
            ogs_error("MBS Session Identifier too short for its TMGI");
            return -1;
        }
        memcpy(mbs_session_identifier->tmgi.mbs_service_id, data + size,
                sizeof(mbs_session_identifier->tmgi.mbs_service_id));
        size += sizeof(mbs_session_identifier->tmgi.mbs_service_id);

        memcpy(&mbs_session_identifier->tmgi.plmn_id, data + size,
                sizeof(mbs_session_identifier->tmgi.plmn_id));
        size += sizeof(mbs_session_identifier->tmgi.plmn_id);
#if 0 /* Commented out as the intention is to omit the field if not indicated by the flag, but the spec shows otherwise */
    } else {
        // No TMGI so we skip to octet 12 (TS 29.244 Clause 8.2.206 shows SSM always starts at octet 12)
        size += 6;
#endif
    }

    if (mbs_session_identifier->ssmif) {
        rv = parse_ssm_address(&mbs_session_identifier->ssm.ip_multicast_distribution_address.flags,
                mbs_session_identifier->ssm.ip_multicast_distribution_address.ipv6_addr,
                data + size, data_len - size);
        if (rv < 0)
            return -1;
        size += rv;

        rv = parse_ssm_address(&mbs_session_identifier->ssm.ip_source_address.flags,
                mbs_session_identifier->ssm.ip_source_address.ipv6_addr,
                data + size, data_len - size);
        if (rv < 0)
            return -1;
        size += rv;
    }

    if (mbs_session_identifier->nidif) {
        if (size + sizeof(mbs_session_identifier->nid) > data_len) {
            ogs_error("MBS Session Identifier too short for its NID");
            return -1;
        }
        memcpy(mbs_session_identifier->nid, data + size, sizeof(mbs_session_identifier->nid));
        size += sizeof(mbs_session_identifier->nid);
    }

    if (size != data_len) {
        ogs_error("MBS Session Identifier length %u, fields decoded %u", data_len, size);
        return -1;
    }

    return size;
}

int16_t ogs_pfcp_build_multicast_transport_information(ogs_tlv_octet_t *octet,
        ogs_pfcp_multicast_transport_information_t *multicast_transport_information, void *data, int data_len)
{
    ogs_pfcp_multicast_transport_information_t target;
    int16_t size = 0;

    ogs_assert(multicast_transport_information);
    ogs_assert(octet);
    ogs_assert(data);
    ogs_assert(data_len);

    octet->data = data;
    memcpy(&target, multicast_transport_information, sizeof(ogs_pfcp_multicast_transport_information_t));

    // spare
    ogs_assert(size + sizeof(target.spare) <= data_len);
    memcpy((uint8_t *)octet->data + size,
            &target.spare, sizeof(target.spare));
    size += sizeof(target.spare);

    // c_teid
    ogs_assert(size + sizeof(target.c_teid) <= data_len);
    memcpy((uint8_t *)octet->data + size,
            &target.c_teid, sizeof(target.c_teid));
    size += sizeof(target.c_teid);

    // ip_multicast_distribution_address
    ogs_assert(size + sizeof(target.ip_multicast_distribution_address.flags) <= data_len);
    memcpy((uint8_t *)octet->data + size,
            &target.ip_multicast_distribution_address.flags, sizeof(target.ip_multicast_distribution_address.flags));
    size += sizeof(target.ip_multicast_distribution_address.flags);

    if (target.ip_multicast_distribution_address.address_type == 0) {
        // IPv4
        ogs_assert(target.ip_multicast_distribution_address.address_length == sizeof(target.ip_multicast_distribution_address.ipv4_addr));
        ogs_assert(size + sizeof(target.ip_multicast_distribution_address.ipv4_addr) <= data_len);
        memcpy((uint8_t *)octet->data + size,
                &target.ip_multicast_distribution_address.ipv4_addr, sizeof(target.ip_multicast_distribution_address.ipv4_addr));
        size += sizeof(target.ip_multicast_distribution_address.ipv4_addr);
    } else if (target.ip_multicast_distribution_address.address_type == 1) {
        // IPv6
        ogs_assert(target.ip_multicast_distribution_address.address_length == OGS_IPV6_LEN);
        ogs_assert(size + OGS_IPV6_LEN <= data_len);
        memcpy((uint8_t *)octet->data + size,
                &target.ip_multicast_distribution_address.ipv6_addr, OGS_IPV6_LEN);
        size += OGS_IPV6_LEN;
    }

    // ip_source_address
    ogs_assert(size + sizeof(target.ip_source_address.flags) <= data_len);
    memcpy((uint8_t *)octet->data + size,
            &target.ip_source_address.flags, sizeof(target.ip_source_address.flags));
    size += sizeof(target.ip_source_address.flags);

    if (target.ip_source_address.address_type == 0) {
        // IPv4
        ogs_assert(target.ip_source_address.address_length == sizeof(target.ip_source_address.ipv4_addr));
        ogs_assert(size + sizeof(target.ip_source_address.ipv4_addr) <= data_len);
        memcpy((uint8_t *)octet->data + size,
                &target.ip_source_address.ipv4_addr, sizeof(target.ip_source_address.ipv4_addr));
        size += sizeof(target.ip_source_address.ipv4_addr);
    } else if (target.ip_source_address.address_type == 1) {
        // IPv6
        ogs_assert(target.ip_source_address.address_length == OGS_IPV6_LEN);
        ogs_assert(size + OGS_IPV6_LEN <= data_len);
        memcpy((uint8_t *)octet->data + size,
                &target.ip_source_address.ipv6_addr, OGS_IPV6_LEN);
        size += OGS_IPV6_LEN;
    }

    octet->len = size;

    return octet->len;
}

/*
 * Multicast Transport Information (TS 29.244 cl. 8.2.207). The octet string comes from a peer: a string that is too
 * short, an address whose length does not match its type, an address type that is not IPv4 or IPv6, or octets left
 * over are errors, reported by the return value (-1), not assertions.
 */
int16_t ogs_pfcp_parse_multicast_transport_information(
        ogs_pfcp_multicast_transport_information_t *multicast_transport_information, ogs_tlv_octet_t *octet)
{
    int size = 0;

    ogs_assert(multicast_transport_information);
    ogs_assert(octet);

    memset(multicast_transport_information, 0, sizeof(ogs_pfcp_multicast_transport_information_t));

    if (!octet->data) {
        ogs_error("No Multicast Transport Information");
        return -1;
    }

    // spare, c_teid
    if (size + sizeof(multicast_transport_information->spare) + sizeof(multicast_transport_information->c_teid) >
            octet->len) {
        ogs_error("Multicast Transport Information too short [%d]", octet->len);
        return -1;
    }
    multicast_transport_information->spare = ((uint8_t *)octet->data)[size];
    size += sizeof(multicast_transport_information->spare);
    memcpy(&multicast_transport_information->c_teid, (unsigned char *)octet->data + size,
            sizeof(multicast_transport_information->c_teid));
    size += sizeof(multicast_transport_information->c_teid);

    // ip_multicast_distribution_address
    if (size + 1 > octet->len) {
        ogs_error("Multicast Transport Information too short [%d]", octet->len);
        return -1;
    }
    multicast_transport_information->ip_multicast_distribution_address.flags = ((uint8_t *)octet->data)[size];
    size += sizeof(multicast_transport_information->ip_multicast_distribution_address.flags);
    if (multicast_transport_information->ip_multicast_distribution_address.address_type == 0) {
        // IPv4
        if (multicast_transport_information->ip_multicast_distribution_address.address_length !=
                sizeof(multicast_transport_information->ip_multicast_distribution_address.ipv4_addr) ||
                size + sizeof(multicast_transport_information->ip_multicast_distribution_address.ipv4_addr) >
                octet->len) {
            ogs_error("Multicast Transport Information: invalid IPv4 multicast distribution address");
            return -1;
        }
        memcpy(&multicast_transport_information->ip_multicast_distribution_address.ipv4_addr,
            (unsigned char *)octet->data + size,
            sizeof(multicast_transport_information->ip_multicast_distribution_address.ipv4_addr));
        size += sizeof(multicast_transport_information->ip_multicast_distribution_address.ipv4_addr);
    } else if (multicast_transport_information->ip_multicast_distribution_address.address_type == 1) {
        // IPv6
        if (multicast_transport_information->ip_multicast_distribution_address.address_length != OGS_IPV6_LEN ||
                size + OGS_IPV6_LEN > octet->len) {
            ogs_error("Multicast Transport Information: invalid IPv6 multicast distribution address");
            return -1;
        }
        memcpy(&multicast_transport_information->ip_multicast_distribution_address.ipv6_addr, (unsigned char *)octet->data + size, OGS_IPV6_LEN);
        size += OGS_IPV6_LEN;
    } else {
        ogs_error("Multicast Transport Information: address type %d of the multicast distribution address",
                multicast_transport_information->ip_multicast_distribution_address.address_type);
        return -1;
    }

    // ip_source_address
    if (size + 1 > octet->len) {
        ogs_error("Multicast Transport Information too short [%d]", octet->len);
        return -1;
    }
    multicast_transport_information->ip_source_address.flags = ((uint8_t *)octet->data)[size];
    size += sizeof(multicast_transport_information->ip_source_address.flags);
    if (multicast_transport_information->ip_source_address.address_type == 0) {
        // IPv4
        if (multicast_transport_information->ip_source_address.address_length !=
                sizeof(multicast_transport_information->ip_source_address.ipv4_addr) ||
                size + sizeof(multicast_transport_information->ip_source_address.ipv4_addr) > octet->len) {
            ogs_error("Multicast Transport Information: invalid IPv4 source address");
            return -1;
        }
        memcpy(&multicast_transport_information->ip_source_address.ipv4_addr,
            (unsigned char *)octet->data + size,
            sizeof(multicast_transport_information->ip_source_address.ipv4_addr));
        size += sizeof(multicast_transport_information->ip_source_address.ipv4_addr);
    } else if (multicast_transport_information->ip_source_address.address_type == 1) {
        // IPv6
        if (multicast_transport_information->ip_source_address.address_length != OGS_IPV6_LEN ||
                size + OGS_IPV6_LEN > octet->len) {
            ogs_error("Multicast Transport Information: invalid IPv6 source address");
            return -1;
        }
        memcpy(&multicast_transport_information->ip_source_address.ipv6_addr, (unsigned char *)octet->data + size, OGS_IPV6_LEN);
        size += OGS_IPV6_LEN;
    } else {
        ogs_error("Multicast Transport Information: address type %d of the source address",
                multicast_transport_information->ip_source_address.address_type);
        return -1;
    }

    if (size != octet->len) {
        ogs_error("Multicast Transport Information: %d octets, %d read", octet->len, size);
        return -1;
    }

    return size;
}
