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

#ifndef UPF_EVENT_H
#define UPF_EVENT_H

#include "ogs-proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ogs_pfcp_node_s ogs_pfcp_node_t;
typedef struct ogs_pfcp_xact_s ogs_pfcp_xact_t;
typedef struct ogs_pfcp_message_s ogs_pfcp_message_t;
typedef struct upf_sess_s upf_sess_t;

typedef enum {
    UPF_EVT_BASE = OGS_MAX_NUM_OF_PROTO_EVENT,

    UPF_EVT_N4_MESSAGE,
    UPF_EVT_N4_TIMER,
    UPF_EVT_N4_NO_HEARTBEAT,
    UPF_EVT_GTPU_PATH_TIMER,      /* timer_id, path_id */
    UPF_EVT_GTPU_ECHO_RESPONSE,   /* peer_ip, sequence_number */
    UPF_EVT_MBS_TIMER,            /* timer_id, seid */
    UPF_EVT_MBS_DL_DATA,          /* seid, pdr_id, qfi: the first packet buffered for a PDR of an MBS session */

    UPF_EVT_TOP,

} upf_event_e;

typedef struct upf_event_s {
    int id;
    int timer_id;

    ogs_pkbuf_t *pkbuf;

    ogs_pfcp_node_t *pfcp_node;
    ogs_pool_id_t pfcp_xact_id;
    ogs_pfcp_message_t *pfcp_message;

    ogs_pool_id_t path_id;      /* GTP-U path management */
    ogs_ip_t peer_ip;
    uint16_t sequence_number;

    uint64_t seid;              /* UPF N4mb SEID of the MBS session */
    uint16_t pdr_id;
    uint8_t qfi;
} upf_event_t;

OGS_STATIC_ASSERT(OGS_EVENT_SIZE >= sizeof(upf_event_t));

void upf_event_init(void);
void upf_event_term(void);
void upf_event_final(void);

upf_event_t *upf_event_new(upf_event_e id);
void upf_event_free(upf_event_t *e);

const char *upf_event_get_name(upf_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /* UPF_EVENT_H */
