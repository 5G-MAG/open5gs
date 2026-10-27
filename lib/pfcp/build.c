/*
 * Copyright (C) 2019-2024 by Sukchan Lee <acetcom@gmail.com>
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

ogs_pkbuf_t *ogs_pfcp_build_heartbeat_request(uint8_t type)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_heartbeat_request_t *req = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_debug("Heartbeat Request");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    req = &pfcp_message->pfcp_heartbeat_request;

    req->recovery_time_stamp.presence = 1;
    req->recovery_time_stamp.u32 = ogs_pfcp_self()->local_recovery;

    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_free(pfcp_message);

    return pkbuf;
}

ogs_pkbuf_t *ogs_pfcp_build_heartbeat_response(uint8_t type)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_heartbeat_response_t *rsp = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_debug("Heartbeat Response");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    rsp = &pfcp_message->pfcp_heartbeat_response;

    rsp->recovery_time_stamp.presence = 1;
    rsp->recovery_time_stamp.u32 = ogs_pfcp_self()->local_recovery;

    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_free(pfcp_message);

    return pkbuf;
}

ogs_pkbuf_t *ogs_pfcp_cp_build_association_setup_request(uint8_t type)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_association_setup_request_t *req = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_pfcp_node_id_t node_id;
    int node_id_len = 0, rv;

    ogs_debug("Association Setup Request");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    req = &pfcp_message->pfcp_association_setup_request;

    rv = ogs_pfcp_sockaddr_to_node_id(&node_id, &node_id_len);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_sockaddr_to_node_id() failed");
        ogs_free(pfcp_message);
        return NULL;
    }
    req->node_id.presence = 1;
    req->node_id.data = &node_id;
    req->node_id.len = node_id_len;

    req->recovery_time_stamp.presence = 1;
    req->recovery_time_stamp.u32 = ogs_pfcp_self()->local_recovery;

    req->cp_function_features.presence = 1;
    req->cp_function_features.u8 = ogs_pfcp_self()->cp_function_features.octet5;

    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_free(pfcp_message);

    return pkbuf;
}

ogs_pkbuf_t *ogs_pfcp_cp_build_association_setup_response(uint8_t type,
        uint8_t cause)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_association_setup_response_t *rsp = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_pfcp_node_id_t node_id;
    int node_id_len = 0, rv;

    ogs_debug("Association Setup Response");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    rsp = &pfcp_message->pfcp_association_setup_response;

    rv = ogs_pfcp_sockaddr_to_node_id(&node_id, &node_id_len);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_sockaddr_to_node_id() failed");
        ogs_free(pfcp_message);
        return NULL;
    }
    rsp->node_id.presence = 1;
    rsp->node_id.data = &node_id;
    rsp->node_id.len = node_id_len;

    rsp->cause.presence = 1;
    rsp->cause.u8 = cause;

    rsp->recovery_time_stamp.presence = 1;
    rsp->recovery_time_stamp.u32 = ogs_pfcp_self()->local_recovery;

    rsp->cp_function_features.presence = 1;
    rsp->cp_function_features.u8 = ogs_pfcp_self()->cp_function_features.octet5;

    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_free(pfcp_message);

    return pkbuf;
}

ogs_pkbuf_t *ogs_pfcp_up_build_association_setup_request(uint8_t type)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_association_setup_request_t *req = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_pfcp_node_id_t node_id;
    int node_id_len = 0, rv;

    ogs_debug("Association Setup Request");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    req = &pfcp_message->pfcp_association_setup_request;

    rv = ogs_pfcp_sockaddr_to_node_id(&node_id, &node_id_len);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_sockaddr_to_node_id() failed");
        ogs_free(pfcp_message);
        return NULL;
    }
    req->node_id.presence = 1;
    req->node_id.data = &node_id;
    req->node_id.len = node_id_len;

    req->recovery_time_stamp.presence = 1;
    req->recovery_time_stamp.u32 = ogs_pfcp_self()->local_recovery;

    ogs_assert(ogs_pfcp_self()->up_function_features_len);
    req->up_function_features.presence = 1;
    req->up_function_features.data = &ogs_pfcp_self()->up_function_features;
    req->up_function_features.len = ogs_pfcp_self()->up_function_features_len;

    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_free(pfcp_message);

    return pkbuf;
}

ogs_pkbuf_t *ogs_pfcp_up_build_association_setup_response(uint8_t type,
        uint8_t cause)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_association_setup_response_t *rsp = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_pfcp_node_id_t node_id;
    int node_id_len = 0, rv;

    ogs_debug("Association Setup Response");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    rsp = &pfcp_message->pfcp_association_setup_response;

    rv = ogs_pfcp_sockaddr_to_node_id(&node_id, &node_id_len);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_sockaddr_to_node_id() failed");
        ogs_free(pfcp_message);
        return NULL;
    }
    rsp->node_id.presence = 1;
    rsp->node_id.data = &node_id;
    rsp->node_id.len = node_id_len;

    rsp->cause.presence = 1;
    rsp->cause.u8 = cause;

    rsp->recovery_time_stamp.presence = 1;
    rsp->recovery_time_stamp.u32 = ogs_pfcp_self()->local_recovery;

    ogs_assert(ogs_pfcp_self()->up_function_features_len);
    rsp->up_function_features.presence = 1;
    rsp->up_function_features.data = &ogs_pfcp_self()->up_function_features;
    rsp->up_function_features.len = ogs_pfcp_self()->up_function_features_len;

    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_free(pfcp_message);

    return pkbuf;
}

static struct {
    ogs_pfcp_f_teid_t f_teid;
    char dnn[OGS_MAX_DNN_LEN+1];
    char *sdf_filter[OGS_MAX_NUM_OF_FLOW_IN_PDR];
    struct {
        uint8_t transport_protocol;
        uint8_t rtp_he_type;
        uint8_t rtp_he_id;
        uint8_t payload_type[OGS_PFCP_MAX_RTP_PAYLOAD_TYPE];
        uint8_t payload_format;
    } protocol_description;
} pdrbuf[OGS_MAX_NUM_OF_PDR];

void ogs_pfcp_pdrbuf_init(void)
{
    memset(pdrbuf, 0, sizeof(pdrbuf));
}

void ogs_pfcp_pdrbuf_clear(void)
{
    int i, j;
    for (i = 0; i < OGS_MAX_NUM_OF_PDR; i++) {
        for (j = 0; j < OGS_MAX_NUM_OF_FLOW_IN_PDR; j++) {
            if (pdrbuf[i].sdf_filter[j])
                ogs_free(pdrbuf[i].sdf_filter[j]);
        }
    }
}

/* The Protocol Description IE of a PDR (TS 29.244 V19.6.0 table 7.5.2.2-7) */
static void build_protocol_description(
    ogs_pfcp_tlv_protocol_description_t *message, int i, ogs_pfcp_pdr_t *pdr)
{
    int j;

    if (!pdr->protocol_description.presence)
        return;

    pdrbuf[i].protocol_description.transport_protocol = pdr->protocol_description.transport_protocol;
    pdrbuf[i].protocol_description.rtp_he_type = pdr->protocol_description.rtp_he_type;
    pdrbuf[i].protocol_description.rtp_he_id = pdr->protocol_description.rtp_he_id;
    pdrbuf[i].protocol_description.payload_format = pdr->protocol_description.payload_format;
    memcpy(pdrbuf[i].protocol_description.payload_type, pdr->protocol_description.payload_type,
            sizeof(pdrbuf[i].protocol_description.payload_type));

    message->presence = 1;
    if (pdr->protocol_description.transport_protocol) {
        message->transport_protocol.presence = 1;
        message->transport_protocol.data = &pdrbuf[i].protocol_description.transport_protocol;
        message->transport_protocol.len = 1;
    }
    if (pdr->protocol_description.rtp_he_type) {
        message->rtp_header_extension_information.presence = 1;
        message->rtp_header_extension_information.rtp_header_extension_type.presence = 1;
        message->rtp_header_extension_information.rtp_header_extension_type.data = &pdrbuf[i].protocol_description.rtp_he_type;
        message->rtp_header_extension_information.rtp_header_extension_type.len = 1;
        message->rtp_header_extension_information.rtp_header_extension_id.presence = 1;
        message->rtp_header_extension_information.rtp_header_extension_id.data = &pdrbuf[i].protocol_description.rtp_he_id;
        message->rtp_header_extension_information.rtp_header_extension_id.len = 1;
    }
    if (pdr->protocol_description.num_of_payload_type || pdr->protocol_description.payload_format) {
        message->rtp_payload_information.presence = 1;
        for (j = 0; j < pdr->protocol_description.num_of_payload_type; j++) {
            message->rtp_payload_information.rtp_payload_type[j].presence = 1;
            message->rtp_payload_information.rtp_payload_type[j].data = &pdrbuf[i].protocol_description.payload_type[j];
            message->rtp_payload_information.rtp_payload_type[j].len = 1;
        }
        if (pdr->protocol_description.payload_format) {
            message->rtp_payload_information.rtp_payload_format.presence = 1;
            message->rtp_payload_information.rtp_payload_format.data = &pdrbuf[i].protocol_description.payload_format;
            message->rtp_payload_information.rtp_payload_format.len = 1;
        }
    }
}

void ogs_pfcp_build_create_pdr(
    ogs_pfcp_tlv_create_pdr_t *message, int i, ogs_pfcp_pdr_t *pdr)
{
    ogs_pfcp_far_t *far = NULL;
    ogs_pfcp_sdf_filter_t pfcp_sdf_filter[OGS_MAX_NUM_OF_FLOW_IN_PDR];
    int j = 0;
    int len = 0;

    ogs_assert(message);

    ogs_assert(pdr);

    far = pdr->far;
    ogs_assert(far);

    message->presence = 1;
    message->pdr_id.presence = 1;
    message->pdr_id.u16 = pdr->id;

    if (pdr->precedence) { /* No precedence in Sxa */
        message->precedence.presence = 1;
        message->precedence.u32 = pdr->precedence;
    }

    message->pdi.presence = 1;
    message->pdi.source_interface.presence = 1;
    message->pdi.source_interface.u8 = pdr->src_if;

    if (pdr->src_if_type_presence) {
        message->pdi.source_interface_type.presence = 1;
        message->pdi.source_interface_type.u8 = pdr->src_if_type;
    }

    if (pdr->dnn) {
        message->pdi.network_instance.presence = 1;
        message->pdi.network_instance.len = ogs_fqdn_build(
            pdrbuf[i].dnn, pdr->dnn, strlen(pdr->dnn));
        message->pdi.network_instance.data = pdrbuf[i].dnn;
    }

    memset(pfcp_sdf_filter, 0, sizeof(pfcp_sdf_filter));
    for (j = 0; j < pdr->num_of_flow && j < OGS_MAX_NUM_OF_FLOW_IN_PDR; j++) {
        ogs_assert(pdr->flow[j].fd || pdr->flow[j].bid);

        if (pdr->flow[j].fd) {
            pfcp_sdf_filter[j].fd = 1;
            pfcp_sdf_filter[j].flow_description_len =
                    strlen(pdr->flow[j].description);
            pfcp_sdf_filter[j].flow_description = pdr->flow[j].description;
        }
        if (pdr->flow[j].bid) {
            pfcp_sdf_filter[j].bid = 1;
            pfcp_sdf_filter[j].sdf_filter_id = pdr->flow[j].sdf_filter_id;
        }

        if (pdr->flow[j].smmii && pdr->flow[j].num_of_mpx) {
            pfcp_sdf_filter[j].smmii = 1;
            pfcp_sdf_filter[j].num_of_mpx = pdr->flow[j].num_of_mpx;
            pfcp_sdf_filter[j].mpx = pdr->flow[j].mpx;
        }

        len = sizeof(ogs_pfcp_sdf_filter_t) +
                pfcp_sdf_filter[j].flow_description_len +
                (pfcp_sdf_filter[j].smmii ?
                    ogs_pfcp_mpx_encoded_len(pfcp_sdf_filter[j].mpx, pfcp_sdf_filter[j].num_of_mpx) : 0);

        message->pdi.sdf_filter[j].presence = 1;
        pdrbuf[i].sdf_filter[j] = ogs_calloc(1, len);
        ogs_assert(pdrbuf[i].sdf_filter[j]);
        ogs_pfcp_build_sdf_filter(&message->pdi.sdf_filter[j],
                &pfcp_sdf_filter[j], pdrbuf[i].sdf_filter[j], len);
    }

    if (pdr->ue_ip_addr_len) {
        message->pdi.ue_ip_address.presence = 1;
        message->pdi.ue_ip_address.data = &pdr->ue_ip_addr;
        message->pdi.ue_ip_address.len = pdr->ue_ip_addr_len;
    }

    for (j = 0; j < OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI; j++) {
        if (!pdr->ipv4_framed_routes || !pdr->ipv4_framed_routes[j])
            break;
        message->pdi.framed_route[j].presence = 1;
        message->pdi.framed_route[j].data = pdr->ipv4_framed_routes[j];
        message->pdi.framed_route[j].len = strlen(pdr->ipv4_framed_routes[j]);
    }

    for (j = 0; j < OGS_MAX_NUM_OF_FRAMED_ROUTES_IN_PDI; j++) {
        if (!pdr->ipv6_framed_routes || !pdr->ipv6_framed_routes[j])
            break;
        message->pdi.framed_ipv6_route[j].presence = 1;
        message->pdi.framed_ipv6_route[j].data = pdr->ipv6_framed_routes[j];
        message->pdi.framed_ipv6_route[j].len = strlen(pdr->ipv6_framed_routes[j]);
    }

    if (pdr->f_teid_len) {
        memcpy(&pdrbuf[i].f_teid, &pdr->f_teid, pdr->f_teid_len);
        pdrbuf[i].f_teid.teid = htobe32(pdr->f_teid.teid);

        message->pdi.local_f_teid.presence = 1;
        message->pdi.local_f_teid.data = &pdrbuf[i].f_teid;
        message->pdi.local_f_teid.len = pdr->f_teid_len;
    }

    if (pdr->qfi) {
        message->pdi.qfi.presence = 1;
        message->pdi.qfi.u8 = pdr->qfi;
    }

    build_protocol_description(&message->pdi.protocol_description, i, pdr);

    if (pdr->outer_header_removal_len) {
        message->outer_header_removal.presence = 1;
        message->outer_header_removal.data = &pdr->outer_header_removal;
        message->outer_header_removal.len = pdr->outer_header_removal_len;
    }

    if (pdr->far) {
        message->far_id.presence = 1;
        message->far_id.u32 = pdr->far->id;
    }

    ogs_assert(pdr->num_of_urr <= OGS_ARRAY_SIZE(message->urr_id));
    for (i = 0; i < pdr->num_of_urr; i++) {
        message->urr_id[i].presence = 1;
        message->urr_id[i].u32 = pdr->urr[i]->id;
    }

    if (pdr->qer) {
        message->qer_id.presence = 1;
        message->qer_id.u32 = pdr->qer->id;
    }
}

bool ogs_pfcp_build_created_pdr(
    ogs_pfcp_tlv_created_pdr_t *message, int i, ogs_pfcp_pdr_t *pdr)
{
    bool pdr_presence = false;

    ogs_assert(message);

    ogs_assert(pdr);

    if (pdr->f_teid_len) {
        memcpy(&pdrbuf[i].f_teid, &pdr->f_teid, pdr->f_teid_len);
        pdrbuf[i].f_teid.teid = htobe32(pdr->f_teid.teid);

        message->local_f_teid.presence = 1;
        message->local_f_teid.data = &pdrbuf[i].f_teid;
        message->local_f_teid.len = pdr->f_teid_len;

        pdr_presence = true;
    }

    if (pdr_presence == true) {
        message->presence = 1;
        message->pdr_id.presence = 1;
        message->pdr_id.u16 = pdr->id;
    }

    return pdr_presence;
}

void ogs_pfcp_build_update_pdr(
    ogs_pfcp_tlv_update_pdr_t *message, int i,
    ogs_pfcp_pdr_t *pdr, uint64_t modify_flags)
{
    ogs_pfcp_sdf_filter_t pfcp_sdf_filter[OGS_MAX_NUM_OF_FLOW_IN_PDR];
    int j = 0;
    int len = 0;

    ogs_assert(message);
    ogs_assert(pdr);

    ogs_assert(modify_flags &
            (OGS_PFCP_MODIFY_TFT_NEW|OGS_PFCP_MODIFY_TFT_ADD|
             OGS_PFCP_MODIFY_TFT_REPLACE|OGS_PFCP_MODIFY_TFT_DELETE|
             OGS_PFCP_MODIFY_EPC_TFT_UPDATE|
             OGS_PFCP_MODIFY_OUTER_HEADER_REMOVAL|
             OGS_PFCP_MODIFY_PDR_URR|OGS_PFCP_MODIFY_PDR_PROTO_DESC));

    message->presence = 1;
    message->pdr_id.presence = 1;
    message->pdr_id.u16 = pdr->id;

    /* the Protocol Description of the PDR (TS 29.244 V19.6.0 table 7.5.4.2-1, PDI) */
    if (modify_flags & OGS_PFCP_MODIFY_PDR_PROTO_DESC) {
        message->pdi.presence = 1;
        message->pdi.source_interface.presence = 1;
        message->pdi.source_interface.u8 = pdr->src_if;
        build_protocol_description(&message->pdi.protocol_description, i, pdr);
    }

    /* the URRs of the PDR (TS 29.244 table 7.5.4.2-1, URR ID) */
    if (modify_flags & OGS_PFCP_MODIFY_PDR_URR) {
        ogs_assert(pdr->num_of_urr <= OGS_ARRAY_SIZE(message->urr_id));
        for (j = 0; j < pdr->num_of_urr; j++) {
            message->urr_id[j].presence = 1;
            message->urr_id[j].u32 = pdr->urr[j]->id;
        }
    }

    if (modify_flags &
            (OGS_PFCP_MODIFY_TFT_NEW|OGS_PFCP_MODIFY_TFT_ADD|
             OGS_PFCP_MODIFY_TFT_REPLACE|OGS_PFCP_MODIFY_TFT_DELETE|
             OGS_PFCP_MODIFY_EPC_TFT_UPDATE)) {
        message->pdi.presence = 1;
        message->pdi.source_interface.presence = 1;
        message->pdi.source_interface.u8 = pdr->src_if;

        if (pdr->src_if_type_presence) {
            message->pdi.source_interface_type.presence = 1;
            message->pdi.source_interface_type.u8 = pdr->src_if_type;
        }

        memset(pfcp_sdf_filter, 0, sizeof(pfcp_sdf_filter));
        for (j = 0; j < pdr->num_of_flow && j < OGS_MAX_NUM_OF_FLOW_IN_PDR; j++) {
            ogs_assert(pdr->flow[j].fd || pdr->flow[j].bid);

            if (pdr->flow[j].fd) {
                pfcp_sdf_filter[j].fd = 1;
                pfcp_sdf_filter[j].flow_description_len =
                        strlen(pdr->flow[j].description);
                pfcp_sdf_filter[j].flow_description = pdr->flow[j].description;
            }
            if (pdr->flow[j].bid) {
                pfcp_sdf_filter[j].bid = 1;
                pfcp_sdf_filter[j].sdf_filter_id = pdr->flow[j].sdf_filter_id;
            }

            if (pdr->flow[j].smmii && pdr->flow[j].num_of_mpx) {
                pfcp_sdf_filter[j].smmii = 1;
                pfcp_sdf_filter[j].num_of_mpx = pdr->flow[j].num_of_mpx;
                pfcp_sdf_filter[j].mpx = pdr->flow[j].mpx;
            }

            len = sizeof(ogs_pfcp_sdf_filter_t) +
                    pfcp_sdf_filter[j].flow_description_len +
                    (pfcp_sdf_filter[j].smmii ?
                        ogs_pfcp_mpx_encoded_len(pfcp_sdf_filter[j].mpx, pfcp_sdf_filter[j].num_of_mpx) : 0);

            message->pdi.sdf_filter[j].presence = 1;
            pdrbuf[i].sdf_filter[j] = ogs_calloc(1, len);
            ogs_assert(pdrbuf[i].sdf_filter[j]);
            ogs_pfcp_build_sdf_filter(&message->pdi.sdf_filter[j],
                    &pfcp_sdf_filter[j], pdrbuf[i].sdf_filter[j], len);
        }
    }
    if (modify_flags & OGS_PFCP_MODIFY_OUTER_HEADER_REMOVAL) {
        if (pdr->outer_header_removal_len) {
            message->outer_header_removal.presence = 1;
            message->outer_header_removal.data = &pdr->outer_header_removal;
            message->outer_header_removal.len = pdr->outer_header_removal_len;
        }
    }
}

static struct {
    ogs_pfcp_outer_header_creation_t outer_header_creation;
    char dnn[OGS_MAX_DNN_LEN+1];
} farbuf[OGS_MAX_NUM_OF_FAR];

void ogs_pfcp_build_create_far(
    ogs_pfcp_tlv_create_far_t *message, int i, ogs_pfcp_far_t *far)
{
    ogs_pfcp_sess_t *sess = NULL;

    ogs_assert(message);
    ogs_assert(far);
    sess = far->sess;
    ogs_assert(sess);

    message->presence = 1;
    message->far_id.presence = 1;
    message->far_id.u32 = far->id;

    message->apply_action.presence = 1;
    message->apply_action.u16 = far->apply_action;

    if (far->apply_action & OGS_PFCP_APPLY_ACTION_FORW) {
        message->forwarding_parameters.presence = 1;
        message->forwarding_parameters.destination_interface.presence = 1;
        message->forwarding_parameters.destination_interface.u8 =
            far->dst_if;

        if (far->dst_if_type_presence) {
            message->forwarding_parameters.destination_interface_type.
                presence = 1;
            message->forwarding_parameters.destination_interface_type.
                u8 = far->dst_if_type;
        }

        if (far->dnn) {
            message->forwarding_parameters.network_instance.presence = 1;
            message->forwarding_parameters.network_instance.len =
                ogs_fqdn_build(farbuf[i].dnn, far->dnn, strlen(far->dnn));
            message->forwarding_parameters.network_instance.data =
                farbuf[i].dnn;
        }

        if (far->outer_header_creation_len) {
            memcpy(&farbuf[i].outer_header_creation,
                &far->outer_header_creation, far->outer_header_creation_len);
            farbuf[i].outer_header_creation.teid =
                    htobe32(far->outer_header_creation.teid);

            message->forwarding_parameters.outer_header_creation.presence = 1;
            message->forwarding_parameters.outer_header_creation.data =
                    &farbuf[i].outer_header_creation;
            message->forwarding_parameters.outer_header_creation.len =
                    far->outer_header_creation_len;
        }
    } else if (far->apply_action & OGS_PFCP_APPLY_ACTION_BUFF) {
        ogs_assert(sess->bar);
        message->bar_id.presence = 1;
        message->bar_id.u8 = sess->bar->id;
    }
}

void ogs_pfcp_build_update_far_deactivate(
        ogs_pfcp_tlv_update_far_t *message, int i, ogs_pfcp_far_t *far)
{
    ogs_pfcp_sess_t *sess = NULL;

    ogs_assert(message);
    ogs_assert(far);
    sess = far->sess;
    ogs_assert(sess);

    message->presence = 1;
    message->far_id.presence = 1;
    message->far_id.u32 = far->id;

    far->apply_action =
        OGS_PFCP_APPLY_ACTION_BUFF | OGS_PFCP_APPLY_ACTION_NOCP;
    message->apply_action.presence = 1;
    message->apply_action.u16 = far->apply_action;

    ogs_assert(sess->bar);
    message->bar_id.presence = 1;
    message->bar_id.u8 = sess->bar->id;
}

void ogs_pfcp_build_update_far_activate(
        ogs_pfcp_tlv_update_far_t *message, int i, ogs_pfcp_far_t *far)
{
    ogs_assert(message);
    ogs_assert(far);

    message->presence = 1;
    message->far_id.presence = 1;
    message->far_id.u32 = far->id;

    ogs_assert(far->apply_action & OGS_PFCP_APPLY_ACTION_FORW);

    message->apply_action.presence = 1;
    message->apply_action.u16 = far->apply_action;

    message->update_forwarding_parameters.presence = 1;
    message->update_forwarding_parameters.destination_interface.presence = 1;
    message->update_forwarding_parameters.
        destination_interface.u8 = far->dst_if;

    if (far->dst_if_type_presence) {
        message->update_forwarding_parameters.destination_interface_type.
            presence = 1;
        message->update_forwarding_parameters.destination_interface_type.
            u8 = far->dst_if_type;
    }

    if (far->dnn) {
        message->update_forwarding_parameters.network_instance.presence = 1;
        message->update_forwarding_parameters.network_instance.len =
            ogs_fqdn_build(farbuf[i].dnn, far->dnn, strlen(far->dnn));
        message->update_forwarding_parameters.network_instance.data =
            farbuf[i].dnn;
    }

    if (far->outer_header_creation_len || far->smreq_flags.value) {

        if (far->outer_header_creation_len) {
            memcpy(&farbuf[i].outer_header_creation,
                &far->outer_header_creation, far->outer_header_creation_len);
            farbuf[i].outer_header_creation.teid =
                    htobe32(far->outer_header_creation.teid);

            message->update_forwarding_parameters.
                outer_header_creation.presence = 1;
            message->update_forwarding_parameters.
                outer_header_creation.data = &farbuf[i].outer_header_creation;
            message->update_forwarding_parameters.
                outer_header_creation.len = far->outer_header_creation_len;

        }

        if (far->smreq_flags.value) {
            message->update_forwarding_parameters.pfcpsmreq_flags.presence = 1;
            message->update_forwarding_parameters.pfcpsmreq_flags.u8 =
                far->smreq_flags.value;
        }
    }
}

static struct {
    ogs_pfcp_volume_threshold_t vol_threshold;
    ogs_pfcp_volume_quota_t vol_quota;
    ogs_pfcp_dropped_dl_traffic_threshold_t dropped_dl_traffic_threshold;
} urrbuf[OGS_MAX_NUM_OF_URR];

void ogs_pfcp_build_create_urr(
    ogs_pfcp_tlv_create_urr_t *message, int i, ogs_pfcp_urr_t *urr)
{
    ogs_assert(message);
    ogs_assert(urr);

    message->presence = 1;
    message->urr_id.presence = 1;
    message->urr_id.u32 = urr->id;
    message->measurement_method.presence = 1;
    message->measurement_method.u8 = urr->meas_method;
    message->reporting_triggers.presence = 1;
    message->reporting_triggers.u24 = (urr->rep_triggers.reptri_5 << 16)
                                    | (urr->rep_triggers.reptri_6 << 8)
                                    | (urr->rep_triggers.reptri_7);
    if (urr->meas_period) {
        message->measurement_period.presence = 1;
        message->measurement_period.u32 = urr->meas_period;
    }

    if (urr->vol_threshold.flags) {
        message->volume_threshold.presence = 1;
        ogs_pfcp_build_volume(
                &message->volume_threshold, &urr->vol_threshold,
                &urrbuf[i].vol_threshold, sizeof(urrbuf[i].vol_threshold));
    }

    if (urr->vol_quota.flags) {
        message->volume_quota.presence = 1;
        ogs_pfcp_build_volume(
                &message->volume_quota, &urr->vol_quota,
                &urrbuf[i].vol_quota, sizeof(urrbuf[i].vol_quota));
    }

    if (urr->event_threshold) {
        message->event_threshold.presence = 1;
        message->event_threshold.u32 = urr->event_threshold;
    }

    if (urr->event_quota) {
        message->event_quota.presence = 1;
        message->event_quota.u32 = urr->event_quota;
    }

    if (urr->time_threshold) {
        message->time_threshold.presence = 1;
        message->time_threshold.u32 = urr->time_threshold;
    }

    if (urr->time_quota) {
        message->time_quota.presence = 1;
        message->time_quota.u32 = urr->time_quota;
    }

    if (urr->quota_holding_time) {
        message->quota_holding_time.presence = 1;
        message->quota_holding_time.u32 = urr->quota_holding_time;
    }

    if (urr->dropped_dl_traffic_threshold.flags) {
        message->dropped_dl_traffic_threshold.presence = 1;
        ogs_pfcp_build_dropped_dl_traffic_threshold(
                &message->dropped_dl_traffic_threshold,
                &urr->dropped_dl_traffic_threshold,
                &urrbuf[i].dropped_dl_traffic_threshold,
                sizeof(urrbuf[i].dropped_dl_traffic_threshold));
    }

    if (urr->quota_validity_time) {
        message->quota_validity_time.presence = 1;
        message->quota_validity_time.u32 = urr->quota_validity_time;
    }

    if (urr->meas_info.octet5) {
        message->measurement_information.presence = 1;
        message->measurement_information.data = &urr->meas_info.octet5;
        message->measurement_information.len = 1;
    }
}

void ogs_pfcp_build_update_urr(
    ogs_pfcp_tlv_update_urr_t *message, int i,
    ogs_pfcp_urr_t *urr, uint64_t modify_flags)
{
    ogs_assert(message);
    ogs_assert(urr);

    ogs_assert(modify_flags &
            (OGS_PFCP_MODIFY_URR_MEAS_METHOD|
             OGS_PFCP_MODIFY_URR_REPORT_TRIGGER|
             OGS_PFCP_MODIFY_URR_VOLUME_THRESH|
             OGS_PFCP_MODIFY_URR_VOLUME_QUOTA|
             OGS_PFCP_MODIFY_URR_TIME_THRESH|
             OGS_PFCP_MODIFY_URR_TIME_QUOTA|
             OGS_PFCP_MODIFY_URR_QUOTA_VALIDITY_TIME));

    /* Change request: Send only changed IEs */
    message->presence = 1;
    message->urr_id.presence = 1;
    message->urr_id.u32 = urr->id;

    if (modify_flags & OGS_PFCP_MODIFY_URR_MEAS_METHOD) {
        message->measurement_method.presence = 1;
        message->measurement_method.u8 = urr->meas_method;
    }

    if (modify_flags & OGS_PFCP_MODIFY_URR_REPORT_TRIGGER) {
        message->reporting_triggers.presence = 1;
        message->reporting_triggers.u24 = (urr->rep_triggers.reptri_5 << 16)
                                        | (urr->rep_triggers.reptri_6 << 8)
                                        | (urr->rep_triggers.reptri_7);
    }

    if (modify_flags & OGS_PFCP_MODIFY_URR_VOLUME_THRESH) {
        if (urr->vol_threshold.flags) {
            message->volume_threshold.presence = 1;
            ogs_pfcp_build_volume(
                    &message->volume_threshold, &urr->vol_threshold,
                    &urrbuf[i].vol_threshold, sizeof(urrbuf[i].vol_threshold));
        }
    }

    if (modify_flags & OGS_PFCP_MODIFY_URR_VOLUME_QUOTA) {
        if (urr->vol_quota.flags) {
            message->volume_quota.presence = 1;
            ogs_pfcp_build_volume(
                    &message->volume_quota, &urr->vol_quota,
                    &urrbuf[i].vol_quota, sizeof(urrbuf[i].vol_quota));
        }
    }

    if (modify_flags & OGS_PFCP_MODIFY_URR_TIME_THRESH) {
        if (urr->time_threshold) {
            message->time_threshold.presence = 1;
            message->time_threshold.u32 = urr->time_threshold;
        }
    }

    if (modify_flags & OGS_PFCP_MODIFY_URR_TIME_QUOTA) {
        if (urr->time_quota) {
            message->time_quota.presence = 1;
            message->time_quota.u32 = urr->time_quota;
        }
    }

    if (modify_flags & OGS_PFCP_MODIFY_URR_QUOTA_VALIDITY_TIME) {
        if (urr->quota_validity_time) {
            message->quota_validity_time.presence = 1;
            message->quota_validity_time.u32 = urr->quota_validity_time;
        }
    }
}

static struct {
    char mbr[OGS_PFCP_BITRATE_LEN];
    char gbr[OGS_PFCP_BITRATE_LEN];
    uint8_t indications; /* QER Indications, octet 5 */
} create_qer_buf[OGS_MAX_NUM_OF_QER], update_qer_buf[OGS_MAX_NUM_OF_QER];

void ogs_pfcp_build_create_qer(
    ogs_pfcp_tlv_create_qer_t *message, int i, ogs_pfcp_qer_t *qer)
{
    ogs_assert(message);
    ogs_assert(qer);

    message->presence = 1;
    message->qer_id.presence = 1;
    message->qer_id.u32 = qer->id;

    message->gate_status.presence = 1;
    message->gate_status.u8 = qer->gate_status.value;

    if (qer->mbr.uplink || qer->mbr.downlink) {
        message->maximum_bitrate.presence = 1;
        ogs_pfcp_build_bitrate(
                &message->maximum_bitrate,
                &qer->mbr, create_qer_buf[i].mbr, OGS_PFCP_BITRATE_LEN);
    }
    if (qer->gbr.uplink || qer->gbr.downlink) {
        message->guaranteed_bitrate.presence = 1;
        ogs_pfcp_build_bitrate(
                &message->guaranteed_bitrate,
                &qer->gbr, create_qer_buf[i].gbr, OGS_PFCP_BITRATE_LEN);
    }

    if (qer->qfi) {
        message->qos_flow_identifier.presence = 1;
        message->qos_flow_identifier.u8 = qer->qfi;
    }

    /*
     * EML4S: the UPF performs ECN marking for L4S (bit 3); PDUSM: the UPF performs PDU Set marking (bit 4); DBSMI: the UPF marks the
     * Data Burst Size (bit 5); TNBMI: the UPF marks the Time to Next Burst (bit 6)
     */
    if (qer->ecn_marking_l4s || qer->pdu_set_marking || qer->data_burst_size_marking || qer->time_to_next_burst_marking) {
        create_qer_buf[i].indications = (qer->ecn_marking_l4s ? 0x04 : 0) | (qer->pdu_set_marking ? 0x08 : 0) |
            (qer->data_burst_size_marking ? 0x10 : 0) | (qer->time_to_next_burst_marking ? 0x20 : 0);
        message->qer_indications.presence = 1;
        message->qer_indications.data = &create_qer_buf[i].indications;
        message->qer_indications.len = 1;
    }
}

void ogs_pfcp_build_update_qer(
    ogs_pfcp_tlv_update_qer_t *message, int i,
    ogs_pfcp_qer_t *qer, uint64_t modify_flags)
{
    ogs_assert(message);
    ogs_assert(qer);

    ogs_assert(modify_flags &
            (OGS_PFCP_MODIFY_QOS_MODIFY|
             OGS_PFCP_MODIFY_EPC_QOS_UPDATE));

    message->presence = 1;
    message->qer_id.presence = 1;
    message->qer_id.u32 = qer->id;

    if (modify_flags &
            (OGS_PFCP_MODIFY_QOS_MODIFY|
             OGS_PFCP_MODIFY_EPC_QOS_UPDATE)) {
        /* the gate status of the QER (TS 29.244 clause 8.2.7) goes along with the QoS */
        message->gate_status.presence = 1;
        message->gate_status.u8 = qer->gate_status.value;

        if (qer->mbr.uplink || qer->mbr.downlink) {
            message->maximum_bitrate.presence = 1;
            ogs_pfcp_build_bitrate(
                    &message->maximum_bitrate,
                    &qer->mbr, update_qer_buf[i].mbr, OGS_PFCP_BITRATE_LEN);
        }
        if (qer->gbr.uplink || qer->gbr.downlink) {
            message->guaranteed_bitrate.presence = 1;
            ogs_pfcp_build_bitrate(
                    &message->guaranteed_bitrate,
                    &qer->gbr, update_qer_buf[i].gbr, OGS_PFCP_BITRATE_LEN);
        }

        /*
         * the ECN marking for L4S, the PDU Set marking and the burst markings are on or off (QER Indications EML4S, bit 3, PDUSM, bit 4,
         * DBSMI, bit 5, and TNBMI, bit 6)
         */
        update_qer_buf[i].indications = (qer->ecn_marking_l4s ? 0x04 : 0x00) | (qer->pdu_set_marking ? 0x08 : 0x00) |
            (qer->data_burst_size_marking ? 0x10 : 0x00) | (qer->time_to_next_burst_marking ? 0x20 : 0x00);
        message->qer_indications.presence = 1;
        message->qer_indications.data = &update_qer_buf[i].indications;
        message->qer_indications.len = 1;
    }
}

void ogs_pfcp_build_create_bar(
    ogs_pfcp_tlv_create_bar_t *message, ogs_pfcp_bar_t *bar)
{
    ogs_assert(message);
    ogs_assert(bar);

    message->presence = 1;
    message->bar_id.presence = 1;
    message->bar_id.u8 = bar->id;
}

static struct {
    ogs_pfcp_volume_measurement_t vol_meas;
} usage_report_buf;

/* Create SRR and Update SRR carry the QoS monitoring control of the rule (TS 29.244 V19.6.0 tables 7.5.2.9-1 and 7.5.4.9-1); buf outlives the building of the message */
void ogs_pfcp_build_create_srr(
        ogs_pfcp_tlv_create_srr_t *message, ogs_pfcp_srr_t *srr, void *buf, int buf_len)
{
    ogs_assert(message);
    ogs_assert(srr);

    message->presence = 1;
    message->srr_id.presence = 1;
    message->srr_id.data = &srr->id;
    message->srr_id.len = 1;
    if (srr->qos_monitoring_presence) {
        message->qos_monitoring_per_qos_flow_control_information.presence = 1;
        ogs_pfcp_build_qos_monitoring_control(
                &message->qos_monitoring_per_qos_flow_control_information,
                &srr->qos_monitoring, buf, buf_len);
    }
}

void ogs_pfcp_build_update_srr(
        ogs_pfcp_tlv_update_srr_t *message, ogs_pfcp_srr_t *srr, void *buf, int buf_len)
{
    ogs_assert(message);
    ogs_assert(srr);

    message->presence = 1;
    message->srr_id.presence = 1;
    message->srr_id.data = &srr->id;
    message->srr_id.len = 1;
    if (srr->qos_monitoring_presence) {
        message->qos_monitoring_per_qos_flow_control_information.presence = 1;
        ogs_pfcp_build_qos_monitoring_control(
                &message->qos_monitoring_per_qos_flow_control_information,
                &srr->qos_monitoring, buf, buf_len);
    }
}

void ogs_pfcp_build_remove_srr(ogs_pfcp_tlv_remove_srr_t *message, ogs_pfcp_srr_t *srr)
{
    ogs_assert(message);
    ogs_assert(srr);

    message->presence = 1;
    message->srr_id.presence = 1;
    message->srr_id.data = &srr->id;
    message->srr_id.len = 1;
}

ogs_pkbuf_t *ogs_pfcp_build_session_report_request(
        uint8_t type, ogs_pfcp_user_plane_report_t *report)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_session_report_request_t *req = NULL;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_pfcp_downlink_data_service_information_t info;
    unsigned int i;

    ogs_assert(report);

    ogs_debug("PFCP session report request");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    req = &pfcp_message->pfcp_session_report_request;

    req->report_type.presence = 1;
    req->report_type.u8 = report->type.value;

    if (report->type.downlink_data_report) {
        int info_len = 0;

        req->downlink_data_report.presence = 1;
        req->downlink_data_report.pdr_id.presence = 1;
        req->downlink_data_report.pdr_id.u16 = report->downlink_data.pdr_id;

        memset(&info, 0, sizeof(info));
        if (report->downlink_data.qfi &&
                report->downlink_data.paging_policy_indication_value) {

            info_len = 3;

            info.qfii = 1;
            info.qfi = report->downlink_data.qfi;
            info.ppi = 1;
            info.paging_policy_indication_value =
                report->downlink_data.paging_policy_indication_value;

        } else if (report->downlink_data.qfi) {

            info_len = 2;

            info.qfii = 1;
            info.qfi = report->downlink_data.qfi;
        } else if (report->downlink_data.paging_policy_indication_value) {

            info_len = 2;

            info.ppi = 1;
            info.paging_policy_indication_value =
                report->downlink_data.paging_policy_indication_value;
        }

        if (info_len) {
            req->downlink_data_report.
                downlink_data_service_information.presence = 1;
            req->downlink_data_report.
                downlink_data_service_information.data = &info;
            req->downlink_data_report.
                downlink_data_service_information.len = info_len;
        }
    }

    if (report->type.usage_report) {
        ogs_assert(report->num_of_usage_report > 0);
        for (i = 0; i < report->num_of_usage_report; i++) {
            req->usage_report[i].presence = 1;
            req->usage_report[i].urr_id.presence = 1;
            req->usage_report[i].urr_id.u32 = report->usage_report[i].id;
            req->usage_report[i].ur_seqn.presence = 1;
            req->usage_report[i].ur_seqn.u32 = report->usage_report[i].seqn;
            req->usage_report[i].usage_report_trigger.presence = 1;
            req->usage_report[i].usage_report_trigger.u24 =
                (report->usage_report[i].rep_trigger.reptri_5 << 16)
                | (report->usage_report[i].rep_trigger.reptri_6 << 8)
                | (report->usage_report[i].rep_trigger.reptri_7);

            if (report->usage_report[i].start_time) {
                req->usage_report[i].start_time.presence = 1;
                req->usage_report[i].start_time.u32 = report->usage_report[i].start_time;
            }

            if (report->usage_report[i].end_time) {
                req->usage_report[i].end_time.presence = 1;
                req->usage_report[i].end_time.u32 = report->usage_report[i].end_time;
            }

            if (report->usage_report[i].vol_measurement.flags) {
                req->usage_report[i].volume_measurement.presence = 1;
                ogs_pfcp_build_volume_measurement(
                        &req->usage_report[i].volume_measurement,
                        &report->usage_report[i].vol_measurement,
                        &usage_report_buf.vol_meas,
                        sizeof(usage_report_buf.vol_meas));
            }

            if (report->usage_report[i].dur_measurement) {
                req->usage_report[i].duration_measurement.presence = 1;
                req->usage_report[i].duration_measurement.u32 =
                    report->usage_report[i].dur_measurement;
            }

            if (report->usage_report[i].time_of_first_packet) {
                req->usage_report[i].time_of_first_packet.presence = 1;
                req->usage_report[i].time_of_first_packet.u32 =
                    report->usage_report[i].time_of_first_packet;
            }

            if (report->usage_report[i].time_of_last_packet) {
                req->usage_report[i].time_of_last_packet.presence = 1;
                req->usage_report[i].time_of_last_packet.u32 =
                    report->usage_report[i].time_of_last_packet;
            }
        }
    }

    if (report->type.session_report) {
        /* the QoS Monitoring Report of an SRR (TS 29.244 V19.6.0 clause 7.5.8.6) */
        uint8_t srr_id = report->qos_monitoring.srr_id;
        uint8_t measurement_buf[16];

        req->session_report.presence = 1;
        req->session_report.srr_id.presence = 1;
        req->session_report.srr_id.data = &srr_id;
        req->session_report.srr_id.len = 1;
        req->session_report.qos_monitoring_report.presence = 1;
        req->session_report.qos_monitoring_report.qfi.presence = 1;
        req->session_report.qos_monitoring_report.qfi.u8 = report->qos_monitoring.qfi;
        req->session_report.qos_monitoring_report.qos_monitoring_measurement.presence = 1;
        ogs_pfcp_build_qos_monitoring_measurement(
                &req->session_report.qos_monitoring_report.qos_monitoring_measurement,
                &report->qos_monitoring.measurement, measurement_buf, sizeof(measurement_buf));

        pfcp_message->h.type = type;
        pkbuf = ogs_pfcp_build_msg(pfcp_message);
        ogs_expect(pkbuf);
        ogs_free(pfcp_message);
        return pkbuf;
    }

    if (report->error_indication.remote_f_teid_len) {
        req->error_indication_report.presence = 1;
        req->error_indication_report.remote_f_teid.presence = 1;
        req->error_indication_report.remote_f_teid.data =
            &report->error_indication.remote_f_teid;
        req->error_indication_report.remote_f_teid.len =
            report->error_indication.remote_f_teid_len;
    }

    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_free(pfcp_message);

    return pkbuf;
}

ogs_pkbuf_t *ogs_pfcp_build_session_report_response(
        uint8_t type, uint8_t cause)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_session_report_response_t *rsp = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_debug("PFCP session report response");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    rsp = &pfcp_message->pfcp_session_report_response;

    rsp->cause.presence = 1;
    rsp->cause.u8 = cause;

    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_free(pfcp_message);

    return pkbuf;
}

/* The usage reports of a user plane report as the Usage Report IEs of a Session Modification Response (TS 29.244 clause 7.5.5.4) */
void ogs_pfcp_build_usage_report_modification_response(
        ogs_pfcp_session_modification_response_t *rsp,
        ogs_pfcp_user_plane_report_t *report)
{
    unsigned int i;

    ogs_assert(rsp);
    ogs_assert(report);

    if (!report->type.usage_report) return;
    for (i = 0; i < report->num_of_usage_report && i < OGS_ARRAY_SIZE(rsp->usage_report); i++) {
        rsp->usage_report[i].presence = 1;
        rsp->usage_report[i].urr_id.presence = 1;
        rsp->usage_report[i].urr_id.u32 = report->usage_report[i].id;
        rsp->usage_report[i].ur_seqn.presence = 1;
        rsp->usage_report[i].ur_seqn.u32 = report->usage_report[i].seqn;
        rsp->usage_report[i].usage_report_trigger.presence = 1;
        rsp->usage_report[i].usage_report_trigger.u24 =
            (report->usage_report[i].rep_trigger.reptri_5 << 16)
            | (report->usage_report[i].rep_trigger.reptri_6 << 8)
            | (report->usage_report[i].rep_trigger.reptri_7);

        if (report->usage_report[i].start_time) {
            rsp->usage_report[i].start_time.presence = 1;
            rsp->usage_report[i].start_time.u32 = report->usage_report[i].start_time;
        }
        if (report->usage_report[i].end_time) {
            rsp->usage_report[i].end_time.presence = 1;
            rsp->usage_report[i].end_time.u32 = report->usage_report[i].end_time;
        }
        if (report->usage_report[i].vol_measurement.flags) {
            rsp->usage_report[i].volume_measurement.presence = 1;
            ogs_pfcp_build_volume_measurement(
                    &rsp->usage_report[i].volume_measurement,
                    &report->usage_report[i].vol_measurement,
                    &usage_report_buf.vol_meas,
                    sizeof(usage_report_buf.vol_meas));
        }
        rsp->usage_report[i].duration_measurement.presence = 1;
        rsp->usage_report[i].duration_measurement.u32 = report->usage_report[i].dur_measurement;
        if (report->usage_report[i].time_of_first_packet) {
            rsp->usage_report[i].time_of_first_packet.presence = 1;
            rsp->usage_report[i].time_of_first_packet.u32 = report->usage_report[i].time_of_first_packet;
        }
        if (report->usage_report[i].time_of_last_packet) {
            rsp->usage_report[i].time_of_last_packet.presence = 1;
            rsp->usage_report[i].time_of_last_packet.u32 = report->usage_report[i].time_of_last_packet;
        }
    }
}

ogs_pkbuf_t *ogs_pfcp_build_session_deletion_response( uint8_t type, uint8_t cause,
        ogs_pfcp_user_plane_report_t *report)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_session_deletion_response_t *rsp = NULL;
    ogs_pkbuf_t *pkbuf = NULL;
    unsigned int i;

    ogs_debug("PFCP session deletion response");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    rsp = &pfcp_message->pfcp_session_deletion_response;

    rsp->cause.presence = 1;
    rsp->cause.u8 = cause;

    if (report->type.usage_report) {
        ogs_assert(report->num_of_usage_report > 0);
        for (i = 0; i < report->num_of_usage_report; i++) {
            rsp->usage_report[i].presence = 1;
            rsp->usage_report[i].urr_id.presence = 1;
            rsp->usage_report[i].urr_id.u32 = report->usage_report[i].id;
            rsp->usage_report[i].ur_seqn.presence = 1;
            rsp->usage_report[i].ur_seqn.u32 = report->usage_report[i].seqn;
            rsp->usage_report[i].usage_report_trigger.presence = 1;
            rsp->usage_report[i].usage_report_trigger.u24 =
                (report->usage_report[i].rep_trigger.reptri_5 << 16)
                | (report->usage_report[i].rep_trigger.reptri_6 << 8)
                | (report->usage_report[i].rep_trigger.reptri_7);

            if (report->usage_report[i].start_time) {
                rsp->usage_report[i].start_time.presence = 1;
                rsp->usage_report[i].start_time.u32 = report->usage_report[i].start_time;
            }

            if (report->usage_report[i].end_time) {
                rsp->usage_report[i].end_time.presence = 1;
                rsp->usage_report[i].end_time.u32 = report->usage_report[i].end_time;
            }

            if (report->usage_report[i].vol_measurement.flags) {
                rsp->usage_report[i].volume_measurement.presence = 1;
                ogs_pfcp_build_volume_measurement(
                        &rsp->usage_report[i].volume_measurement,
                        &report->usage_report[i].vol_measurement,
                        &usage_report_buf.vol_meas,
                        sizeof(usage_report_buf.vol_meas));
            }

            rsp->usage_report[i].duration_measurement.presence = 1;
            rsp->usage_report[i].duration_measurement.u32 =
                report->usage_report[i].dur_measurement;

            if (report->usage_report[i].time_of_first_packet) {
                rsp->usage_report[i].time_of_first_packet.presence = 1;
                rsp->usage_report[i].time_of_first_packet.u32 =
                    report->usage_report[i].time_of_first_packet;
            }

            if (report->usage_report[i].time_of_last_packet) {
                rsp->usage_report[i].time_of_last_packet.presence = 1;
                rsp->usage_report[i].time_of_last_packet.u32 =
                    report->usage_report[i].time_of_last_packet;
            }
        }
    }
    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_free(pfcp_message);

    return pkbuf;
}
