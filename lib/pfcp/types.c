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
