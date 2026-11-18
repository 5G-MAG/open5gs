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

#include "ogs-pfcp.h"

ogs_sock_t *ogs_pfcp_server(ogs_socknode_t *node)
{
    char buf[OGS_ADDRSTRLEN];
    ogs_sock_t *pfcp;
    ogs_assert(node);

    pfcp = ogs_udp_server(node->addr, node->option);
    if (pfcp) {
        ogs_info("pfcp_server() [%s]:%d",
                OGS_ADDR(node->addr, buf), OGS_PORT(node->addr));

        node->sock = pfcp;
    }

    return pfcp;
}

/* Minimum PFCP header length (e.g., 12 bytes) */
#define MIN_PFCP_HEADER_LENGTH 12

/*
 * ogs_pfcp_recvfrom
 *
 * Receives a PFCP message from the socket 'fd'. It allocates a pkbuf,
 * receives the message, trims the pkbuf, and verifies the header.
 * If any error occurs (e.g., too short message, unsupported version, or
 * incomplete message), the function frees the pkbuf and returns NULL.
 *
 * The sender's address is stored in 'from'.
 *
 * Returns a pointer to ogs_pkbuf_t on success, or NULL on failure.
 */
ogs_pkbuf_t *ogs_pfcp_recvfrom(ogs_socket_t fd, ogs_sockaddr_t *from)
{
    ogs_pkbuf_t *pkbuf;
    ssize_t size;
    ogs_pfcp_header_t *h;
    uint16_t pfcp_body_length;
    size_t expected_total_length;

    ogs_assert(fd != INVALID_SOCKET);
    ogs_assert(from);

    /* Allocate buffer for maximum SDU length */
    pkbuf = ogs_pkbuf_alloc(NULL, OGS_MAX_SDU_LEN);
    if (pkbuf == NULL) {
        ogs_error("ogs_pkbuf_alloc() failed");
        return NULL;
    }
    ogs_pkbuf_put(pkbuf, OGS_MAX_SDU_LEN);

    size = ogs_recvfrom(fd, pkbuf->data, pkbuf->len, 0, from);
    if (size <= 0) {
        ogs_log_message(OGS_LOG_ERROR, ogs_socket_errno,
            "ogs_recvfrom() failed");
        ogs_pkbuf_free(pkbuf);
        return NULL;
    }
    ogs_pkbuf_trim(pkbuf, size);

    /* Check that the data is at least as long as the header */
    if (size < MIN_PFCP_HEADER_LENGTH) {
        ogs_error("Received PFCP message too short: %ld bytes (min %d)",
            (long)size, MIN_PFCP_HEADER_LENGTH);
        ogs_pkbuf_free(pkbuf);
        return NULL;
    }

    h = (ogs_pfcp_header_t *)pkbuf->data;

    /* Verify PFCP version */
    if (h->version != OGS_PFCP_VERSION) {
        ogs_pfcp_header_t rsp;
        memset(&rsp, 0, sizeof(rsp));
        ogs_error("Not supported version[%d]", h->version);
        rsp.flags = (OGS_PFCP_VERSION << 5);
        rsp.type = OGS_PFCP_VERSION_NOT_SUPPORTED_RESPONSE_TYPE;
        rsp.length = htobe16(4);
        rsp.sqn_only = h->sqn_only;
        if (ogs_sendto(fd, &rsp, 8, 0, from) < 0) {
            ogs_log_message(OGS_LOG_ERROR, ogs_socket_errno,
                "ogs_sendto() failed");
        }
        ogs_pkbuf_free(pkbuf);
        return NULL;
    }

    /* Check total PFCP message length.
       Assume the header's length field indicates the body length,
       excluding the first 4 bytes. */
    pfcp_body_length = be16toh(h->length);
    expected_total_length = pfcp_body_length + 4;
    if ((size_t)size != expected_total_length) {
        ogs_error("Invalid PFCP Header Length: expected %zu bytes, "
            "received %ld bytes", expected_total_length, (long)size);
        ogs_pkbuf_free(pkbuf);
        return NULL;
    }

    return pkbuf;
}

int ogs_pfcp_sendto(ogs_pfcp_node_t *node, ogs_pkbuf_t *pkbuf)
{
    ssize_t sent;
    ogs_sock_t *sock = NULL;
    ogs_sockaddr_t *addr = NULL;

    ogs_assert(node);
    ogs_assert(pkbuf);
    ogs_assert(node->addr_list);

    /* Initialize round-robin iterator if needed */
    if (node->current_addr == NULL) {
        node->current_addr = node->addr_list;
    }
    addr = node->current_addr;
    ogs_assert(addr);

    if (addr->ogs_sa_family == AF_INET) {
        sock = ogs_pfcp_self()->pfcp_sock;
        if (!sock) {
            ogs_error("IPv4 socket (pfcp_sock) is not available. "
                    "Ensure that 'pfcp.server.address: 127.0.0.1' "
                    "is set in the YAML configuration file.");
            return OGS_ERROR;
        }
    } else if (addr->ogs_sa_family == AF_INET6) {
        sock = ogs_pfcp_self()->pfcp_sock6;
        if (!sock) {
            ogs_error("IPv6 socket (pfcp_sock) is not available. "
                    "Ensure that 'pfcp.server.address: [::1]' "
                    "is set in the YAML configuration file.");
            return OGS_ERROR;
        }
    } else
        ogs_assert_if_reached();

    sent = ogs_sendto(sock->fd, pkbuf->data, pkbuf->len, 0, addr);
    if (sent < 0 || sent != pkbuf->len) {
        if (ogs_socket_errno != OGS_EAGAIN) {
            char buf[OGS_ADDRSTRLEN];
            int err = ogs_socket_errno;
            ogs_log_message(OGS_LOG_ERROR, err,
                    "ogs_sendto(%u, %p, %u, 0, %s:%u) failed",
                    sock->fd, pkbuf->data, pkbuf->len,
                    OGS_ADDR(addr, buf), OGS_PORT(addr));
        }
        return OGS_ERROR;
    }

    /* Move to next address in round-robin sequence */
    if (node->current_addr->next)
        node->current_addr = node->current_addr->next;
    else
        /* If end of list reached, wrap around to the start */
        node->current_addr = node->addr_list;

    return OGS_OK;
}

int ogs_pfcp_send_heartbeat_request(ogs_pfcp_node_t *node,
        void (*cb)(ogs_pfcp_xact_t *xact, void *data))
{
    int rv;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_pfcp_header_t h;
    ogs_pfcp_xact_t *xact = NULL;

    ogs_assert(node);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_HEARTBEAT_REQUEST_TYPE;
    h.seid = 0;

    xact = ogs_pfcp_xact_local_create(node, cb, node);
    if (!xact) {
        ogs_error("ogs_pfcp_xact_local_create() failed");
        return OGS_ERROR;
    }

    pkbuf = ogs_pfcp_build_heartbeat_request(h.type);
    if (!pkbuf) {
        ogs_error("ogs_pfcp_build_heartbeat_request() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, pkbuf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    return rv;
}

int ogs_pfcp_send_heartbeat_response(ogs_pfcp_xact_t *xact)
{
    int rv;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_pfcp_header_t h;

    ogs_assert(xact);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_HEARTBEAT_RESPONSE_TYPE;
    h.seid = 0;

    pkbuf = ogs_pfcp_build_heartbeat_response(h.type);
    if (!pkbuf) {
        ogs_error("ogs_pfcp_build_heartbeat_response() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, pkbuf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    /*
     * Force delete the PFCP transaction to check the PFCP recovery timestamp.
     *
     * Otherwise, duplicated request (lib/pfcp/xact.c:384) prevents the message
     * from being passed to the state machine, so the PFCP recovery timestamp
     * cannot be delivered in the handler routine.
     */
    ogs_pfcp_xact_delete(xact);

    return rv;
}

int ogs_pfcp_cp_send_association_setup_request(ogs_pfcp_node_t *node,
        void (*cb)(ogs_pfcp_xact_t *xact, void *data))
{
    int rv;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_pfcp_header_t h;
    ogs_pfcp_xact_t *xact = NULL;

    ogs_assert(node);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_ASSOCIATION_SETUP_REQUEST_TYPE;
    h.seid = 0;

    xact = ogs_pfcp_xact_local_create(node, cb, node);
    if (!xact) {
        ogs_error("ogs_pfcp_xact_local_create() failed");
        return OGS_ERROR;
    }

    pkbuf = ogs_pfcp_cp_build_association_setup_request(h.type);
    if (!pkbuf) {
        ogs_error("ogs_pfcp_cp_build_association_setup_request() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, pkbuf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    return rv;
}

int ogs_pfcp_cp_send_association_setup_response(ogs_pfcp_xact_t *xact,
        uint8_t cause)
{
    int rv;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_pfcp_header_t h;

    ogs_assert(xact);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_ASSOCIATION_SETUP_RESPONSE_TYPE;
    h.seid = 0;

    pkbuf = ogs_pfcp_cp_build_association_setup_response(h.type, cause);
    if (!pkbuf) {
        ogs_error("ogs_pfcp_cp_build_association_setup_response() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, pkbuf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    return rv;
}

int ogs_pfcp_up_send_association_setup_request(ogs_pfcp_node_t *node,
        void (*cb)(ogs_pfcp_xact_t *xact, void *data))
{
    int rv;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_pfcp_header_t h;
    ogs_pfcp_xact_t *xact = NULL;

    ogs_assert(node);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_ASSOCIATION_SETUP_REQUEST_TYPE;
    h.seid = 0;

    xact = ogs_pfcp_xact_local_create(node, cb, node);
    if (!xact) {
        ogs_error("ogs_pfcp_xact_local_create() failed");
        return OGS_ERROR;
    }

    pkbuf = ogs_pfcp_up_build_association_setup_request(h.type);
    if (!pkbuf) {
        ogs_error("ogs_pfcp_build_heartbeat_request() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, pkbuf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    return rv;
}

int ogs_pfcp_up_send_association_setup_response(ogs_pfcp_xact_t *xact,
        uint8_t cause)
{
    int rv;
    ogs_pkbuf_t *pkbuf = NULL;
    ogs_pfcp_header_t h;

    ogs_assert(xact);

    memset(&h, 0, sizeof(ogs_pfcp_header_t));
    h.type = OGS_PFCP_ASSOCIATION_SETUP_RESPONSE_TYPE;
    h.seid = 0;

    pkbuf = ogs_pfcp_up_build_association_setup_response(h.type, cause);
    if (!pkbuf) {
        ogs_error("ogs_pfcp_up_build_association_setup_response() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &h, pkbuf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return OGS_ERROR;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);

    return rv;
}

void ogs_pfcp_send_gtpu(ogs_pfcp_pdr_t *pdr, ogs_pkbuf_t *sendbuf)
{
    ogs_gtp_node_t *gnode = NULL;
    ogs_pfcp_far_t *far = NULL;

    ogs_assert(pdr);
    ogs_assert(sendbuf);

    far = pdr->far;
    if (!far) {
        ogs_error("No FAR");
        ogs_pkbuf_free(sendbuf);
        return;
    }

    if (far->dst_if == OGS_PFCP_INTERFACE_UNKNOWN) {
        ogs_error("No Destination Interface");
        ogs_pkbuf_free(sendbuf);
        return;
    }

    gnode = far->gnode;
    if (!gnode) {
        ogs_error("No GTP Node Setup");
        ogs_pkbuf_free(sendbuf);
        return;
    }
    if (!gnode->sock) {
        ogs_error("No GTP Socket Setup");
        ogs_pkbuf_free(sendbuf);
        return;
    }

    ogs_gtp_send_with_teid(
            gnode->sock,
            sendbuf, far->outer_header_creation.teid,
            &gnode->addr);

    ogs_pkbuf_free(sendbuf);
}

/*
 * TS 29.244 V19.6.0 clause 5.4.13: "The UP function shall perform the transport level marking for the detected traffic and sends the marked
 * packet to the peer entity." The DSCP is the first octet of the Transport Level Marking IE (clause 8.2.12), the second its mask, which "shall be
 * set to "0xFC"": the two ECN bits of the outer IP header are not part of the marking (the NOTE of clause 8.2.12).
 */
static int send_g_pdu_mbs_copy(ogs_sock_t *sock, ogs_pkbuf_t *buf, uint32_t teid, ogs_sockaddr_t *to,
        bool transport_level_marking, uint8_t tos, uint8_t tos_mask)
{
    if (transport_level_marking)
        return ogs_gtp_send_with_teid_tos(sock, buf, teid, to, tos & tos_mask & 0xFC);
    return ogs_gtp_send_with_teid(sock, buf, teid, to);
}

void ogs_pfcp_send_g_pdu_mbs(ogs_pfcp_pdr_t *pdr, ogs_pkbuf_t *sendbuf)
{
    ogs_pfcp_far_t *far = NULL;
    ogs_pfcp_mbs_unicast_t *mu = NULL;
    int num_of_destinations = 0, sent = 0;
    bool multicast;

    ogs_assert(pdr);
    ogs_assert(sendbuf);

    far = pdr->far;
    if (!far) {
        ogs_error("No FAR");
        ogs_pkbuf_free(sendbuf);
        return;
    }

    /* FSSM: the low layer SSM; MBSU: each remote GTP-U peer (TS 29.244 cl. 5.34.2.2: "Both the "FSSM" and "MBSU"
     * flags shall be set in the Apply Action IE if the MB-UPF is requested to forward MBS data using both
     * multicast and unicast transport") */
    multicast = (far->apply_action & OGS_PFCP_APPLY_ACTION_FSSM) && far->gnode &&
        far->dst_if != OGS_PFCP_INTERFACE_UNKNOWN;
    if (multicast)
        num_of_destinations++;
    if (far->apply_action & OGS_PFCP_APPLY_ACTION_MBSU) {
        ogs_list_for_each(&far->mbs_unicast_list, mu) {
            if (mu->gnode)
                num_of_destinations++;
        }
    }
    if (num_of_destinations == 0) {
        ogs_error("No destination for the MBS data of FAR %u", far->id);
        ogs_pkbuf_free(sendbuf);
        return;
    }

    /* One header for every copy: the DL MBS QFI Sequence Number identifies the packet of the flow, not the copy */
    if (multicast) {
        ogs_gtp_node_t *gnode = far->gnode;
        ogs_pkbuf_t *buf;

        ogs_assert(gnode->sock);
        sent++;
        buf = sent == num_of_destinations ? sendbuf : ogs_pkbuf_copy(sendbuf);
        if (buf) {
            send_g_pdu_mbs_copy(gnode->sock, buf, far->outer_header_creation.teid, &gnode->addr,
                    far->transport_level_marking, far->tos, far->tos_mask);
            ogs_pkbuf_free(buf);
        } else
            ogs_error("ogs_pkbuf_copy() failed");
    }

    if (far->apply_action & OGS_PFCP_APPLY_ACTION_MBSU) {
        ogs_list_for_each(&far->mbs_unicast_list, mu) {
            ogs_gtp_node_t *gnode = mu->gnode;
            ogs_pkbuf_t *buf;

            if (!gnode)
                continue;
            ogs_assert(gnode->sock);

            /* The packet itself goes to the last destination, a copy to each of the others */
            sent++;
            buf = sent == num_of_destinations ? sendbuf : ogs_pkbuf_copy(sendbuf);
            if (!buf) {
                ogs_error("ogs_pkbuf_copy() failed");
                continue;
            }
            send_g_pdu_mbs_copy(gnode->sock, buf, mu->outer_header_creation.teid, &gnode->addr,
                    mu->transport_level_marking, mu->tos, mu->tos_mask);
            ogs_pkbuf_free(buf);
        }
    }
}

void ogs_pfcp_send_buffered_gtpu(ogs_pfcp_pdr_t *pdr)
{
    ogs_pfcp_far_t *far = NULL;
    int i;

    ogs_assert(pdr);
    far = pdr->far;

    if (far && far->gnode) {
        if (far->apply_action & OGS_PFCP_APPLY_ACTION_FORW) {
            for (i = 0; i < far->num_of_buffered_gtpu; i++) {
                ogs_pfcp_send_gtpu(pdr, far->buffered_gtpu[i]);
            }
            far->num_of_buffered_gtpu = 0;
        }
    }
}

int ogs_pfcp_send_end_marker(ogs_pfcp_pdr_t *pdr)
{
    ogs_pkbuf_t *sendbuf = NULL;
    ogs_gtp2_header_desc_t header_desc;

    ogs_assert(pdr);

    sendbuf = ogs_pkbuf_alloc(NULL, OGS_GTPV1U_5GC_HEADER_LEN);
    if (!sendbuf) {
        ogs_error("ogs_pkbuf_alloc() failed");
        return OGS_ERROR;
    }
    ogs_pkbuf_reserve(sendbuf, OGS_GTPV1U_5GC_HEADER_LEN);

    memset(&header_desc, 0, sizeof(header_desc));
    header_desc.type = OGS_GTPU_MSGTYPE_END_MARKER;

    if (pdr->qer && pdr->qer->qfi) {
        header_desc.pdu_type =
            OGS_GTP2_EXTENSION_HEADER_PDU_TYPE_DL_PDU_SESSION_INFORMATION;
        header_desc.qos_flow_identifier = pdr->qer->qfi;
    }

    ogs_gtp2_encapsulate_header(&header_desc, sendbuf);

    ogs_pfcp_send_gtpu(pdr, sendbuf);

    return OGS_OK;
}

/* failed_pdr_id: the PDR that failed to be created or modified (a negative value if no rule is named); only the Session
 * Establishment and Session Modification Responses carry it. */
static void send_error_message(
    ogs_pfcp_xact_t *xact, uint64_t seid, uint8_t type,
    uint8_t cause_value, uint16_t offending_ie_value, int32_t failed_pdr_id)
{
    int rv;
    ogs_pfcp_message_t errmsg;
    uint8_t failed_rule_id[3];
    ogs_pfcp_tlv_cause_t *cause = NULL;
    ogs_pfcp_tlv_offending_ie_t *offending_ie = NULL;
    ogs_pfcp_node_id_t node_id;
    int node_id_len = 0;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_assert(xact);

    memset(&errmsg, 0, sizeof(ogs_pfcp_message_t));
    errmsg.h.seid = seid;
    errmsg.h.type = type;

    switch (type) {
    case OGS_PFCP_PFD_MANAGEMENT_RESPONSE_TYPE:
        cause = &errmsg.pfcp_pfd_management_response.cause;
        offending_ie = &errmsg.pfcp_pfd_management_response.offending_ie;
        break;
    case OGS_PFCP_ASSOCIATION_SETUP_RESPONSE_TYPE:
        cause = &errmsg.pfcp_association_setup_response.cause;
        break;
    case OGS_PFCP_ASSOCIATION_UPDATE_RESPONSE_TYPE:
        cause = &errmsg.pfcp_association_update_response.cause;
        break;
    case OGS_PFCP_ASSOCIATION_RELEASE_RESPONSE_TYPE:
        cause = &errmsg.pfcp_association_release_response.cause;
        break;
    case OGS_PFCP_NODE_REPORT_RESPONSE_TYPE:
        cause = &errmsg.pfcp_node_report_response.cause;
        offending_ie = &errmsg.pfcp_node_report_response.offending_ie;
        break;
    case OGS_PFCP_SESSION_SET_DELETION_RESPONSE_TYPE:
        cause = &errmsg.pfcp_session_set_deletion_response.cause;
        offending_ie = &errmsg.pfcp_session_set_deletion_response.offending_ie;
        break;
    case OGS_PFCP_SESSION_ESTABLISHMENT_RESPONSE_TYPE:
        cause = &errmsg.pfcp_session_establishment_response.cause;
        offending_ie = &errmsg.pfcp_session_establishment_response.offending_ie;

        /* Node ID is mandatory in a Session Establishment Response of any Cause */
        if (ogs_pfcp_sockaddr_to_node_id(&node_id, &node_id_len) == OGS_OK) {
            errmsg.pfcp_session_establishment_response.node_id.presence = 1;
            errmsg.pfcp_session_establishment_response.node_id.data =
                &node_id;
            errmsg.pfcp_session_establishment_response.node_id.len =
                node_id_len;
        } else {
            ogs_error("ogs_pfcp_sockaddr_to_node_id() failed");
        }
        if (failed_pdr_id >= 0) {
            /* TS 29.244 Figure 8.2.80-1: Rule ID Type (PDR is 0) in octet 5, the Rule ID value after it (the PDR ID is 2 octets) */
            failed_rule_id[0] = 0;
            failed_rule_id[1] = (failed_pdr_id >> 8) & 0xff;
            failed_rule_id[2] = failed_pdr_id & 0xff;
            errmsg.pfcp_session_establishment_response.failed_rule_id.presence = 1;
            errmsg.pfcp_session_establishment_response.failed_rule_id.data = failed_rule_id;
            errmsg.pfcp_session_establishment_response.failed_rule_id.len = sizeof(failed_rule_id);
        }
        break;
    case OGS_PFCP_SESSION_MODIFICATION_RESPONSE_TYPE:
        cause = &errmsg.pfcp_session_modification_response.cause;
        offending_ie = &errmsg.pfcp_session_modification_response.offending_ie;
        if (failed_pdr_id >= 0) {
            failed_rule_id[0] = 0;
            failed_rule_id[1] = (failed_pdr_id >> 8) & 0xff;
            failed_rule_id[2] = failed_pdr_id & 0xff;
            errmsg.pfcp_session_modification_response.failed_rule_id.presence = 1;
            errmsg.pfcp_session_modification_response.failed_rule_id.data = failed_rule_id;
            errmsg.pfcp_session_modification_response.failed_rule_id.len = sizeof(failed_rule_id);
        }
        break;
    case OGS_PFCP_SESSION_DELETION_RESPONSE_TYPE:
        cause = &errmsg.pfcp_session_deletion_response.cause;
        offending_ie = &errmsg.pfcp_session_deletion_response.offending_ie;
        break;
    case OGS_PFCP_SESSION_REPORT_RESPONSE_TYPE:
        cause = &errmsg.pfcp_session_report_response.cause;
        offending_ie = &errmsg.pfcp_session_report_response.offending_ie;
        break;
    default:
        ogs_assert_if_reached();
        return;
    }

    ogs_assert(cause);

    cause->presence = 1;
    cause->u8 = cause_value;

    if (offending_ie && offending_ie_value) {
        offending_ie->presence = 1;
        offending_ie->u16 = offending_ie_value;
    }

    pkbuf = ogs_pfcp_build_msg(&errmsg);
    if (!pkbuf) {
        ogs_error("ogs_pfcp_build_msg() failed");
        return;
    }

    rv = ogs_pfcp_xact_update_tx(xact, &errmsg.h, pkbuf);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return;
    }

    rv = ogs_pfcp_xact_commit(xact);
    ogs_expect(rv == OGS_OK);
}

void ogs_pfcp_send_error_message(
    ogs_pfcp_xact_t *xact, uint64_t seid, uint8_t type,
    uint8_t cause_value, uint16_t offending_ie_value)
{
    send_error_message(xact, seid, type, cause_value, offending_ie_value, -1);
}

void ogs_pfcp_send_error_message_failed_pdr(
    ogs_pfcp_xact_t *xact, uint64_t seid, uint8_t type,
    uint8_t cause_value, uint16_t failed_pdr_id)
{
    send_error_message(xact, seid, type, cause_value, 0, failed_pdr_id);
}
