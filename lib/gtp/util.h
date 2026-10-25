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

#if !defined(OGS_GTP_INSIDE) && !defined(OGS_GTP_COMPILATION)
#error "This header cannot be included directly."
#endif

#ifndef OGS_GTP_UTIL_H
#define OGS_GTP_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

int ogs_gtpu_parse_header(
        ogs_gtp2_header_desc_t *header_desc, ogs_pkbuf_t *pkbuf);
uint16_t ogs_in_cksum(uint16_t *addr, int len);

/* The 64-bit NTP time stamp (RFC 5905 clause 6) of TS 38.415 clause 5.5.3.9 and the time of this host */
uint64_t ogs_gtpu_time_to_ntp64(ogs_time_t time);
ogs_time_t ogs_gtpu_ntp64_to_time(uint64_t ntp);

/*
 * A G-PDU without a T-PDU that carries a DL PDU SESSION INFORMATION frame with QMP and the DL Sending Time Stamp (TS 29.281 V19.2.0
 * clause 5.2.2.7, TS 38.415 V19.1.0 clause 5.5.2.1): the dummy packet of the QoS monitoring per QoS flow of TS 29.244 clause 5.24.4.3.
 */
ogs_pkbuf_t *ogs_gtpu_build_qos_monitoring_dl(
        uint32_t teid, uint8_t qfi, uint64_t dl_sending_ts);
/*
 * The monitoring response of the NG-RAN: a G-PDU without a T-PDU that carries an UL PDU SESSION INFORMATION frame with QMP, the three
 * time stamps and the delay results of the RAN part (a result is carried when its indication is set).
 */
/*
 * A G-PDU without a T-PDU that carries an UL PDU SESSION INFORMATION frame with the congestion information of the NG-RAN for the ECN
 * marking at the UPF (TS 38.415 V19.1.0 clause 5.5.2.2); the values are in hundredths of a percent.
 */
ogs_pkbuf_t *ogs_gtpu_build_congestion_ul(uint32_t teid, uint8_t qfi,
        bool ul_presence, uint16_t ul, bool dl_presence, uint16_t dl);
ogs_pkbuf_t *ogs_gtpu_build_qos_monitoring_ul(uint32_t teid, uint8_t qfi,
        uint64_t dl_sending_ts, uint64_t dl_received_ts, uint64_t ul_sending_ts,
        bool dl_delay_ind, uint32_t dl_delay_result,
        bool ul_delay_ind, uint32_t ul_delay_result);

typedef struct ogs_gtp2_sender_f_teid_s {
    bool teid_presence;
    uint32_t teid;
} ogs_gtp2_sender_f_teid_t;

void ogs_gtp2_sender_f_teid(
        ogs_gtp2_sender_f_teid_t *sender_f_teid, ogs_gtp2_message_t *message);

#ifdef __cplusplus
}
#endif

#endif /* OGS_GTP_UTIL_H */
