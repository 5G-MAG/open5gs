/*
 * 5G-MAG Reference Tools
 * Copyright (C) 2024 iTEAM UPV <borieher@iteam.upv.es>
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

#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/udp.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <unistd.h>

#include "context.h"
#include "gtp-path.h"
#include "pfcp-path.h"
#include "n4mb-handler.h"
#include "multicastrouter/multicastrouter.h"

static void _mbs_tunnel_poll_handler(short when, ogs_socket_t fd, void *data);
static uint16_t _get_next_udp_tunnel_port(upf_context_t *ctx);
static int _allocate_ingress_tunnel(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_local_ingress_tunnel_t *lit,
        uint8_t *cause_value, uint16_t *offending_ie_value);
static uint8_t _check_local_ingress_tunnel(const ogs_pfcp_tlv_local_ingress_tunnel_t *ie_);
static int _join_session_ssm(upf_mbs_sess_t *mbs_sess, uint8_t *cause_value, uint16_t *offending_ie_value);
static int _open_udp_tunnel(upf_mbs_sess_t *mbs_sess, ogs_sockaddr_t *bind_address,
        uint8_t *cause_value, uint16_t *offending_ie_value);
static ogs_pkbuf_config_t *_udp_tunnel_make_pool_config(size_t max_buf_size, size_t max_buffers);
static int get_mtu_for_address(const struct sockaddr *addr, socklen_t addr_len);

/*
 * Opens the UDP socket that receives the N6mb / Nmb9 tunnel of the session at bind_address (freed here) and
 * registers it for input. On failure the cause and the Offending IE are set and nothing stays open.
 */
static int _open_udp_tunnel(upf_mbs_sess_t *mbs_sess, ogs_sockaddr_t *bind_address,
        uint8_t *cause_value, uint16_t *offending_ie_value)
{
    int mtu;
    socklen_t name_len;
    ogs_pkbuf_config_t *config;
    int family = bind_address->ogs_sa_family;

    *offending_ie_value = OGS_PFCP_LOCAL_INGRESS_TUNNEL_TYPE;

    mbs_sess->udp_tunnel = ogs_sock_socket(family, SOCK_DGRAM, IPPROTO_UDP);
    // ogs_sock_socket() and ogs_sock_bind() are checked, and a failure fails the whole
    // session establishment (rule 12). Unchecked, a socket() failure feeds a NULL
    // mbs_sess->udp_tunnel into ogs_sock_bind() and getsockname(), and a bind() failure
    // leaves the session apparently established on a tunnel never bound to the address and
    // port reported to the SMF and CU-UP.
    if (!mbs_sess->udp_tunnel) {
        ogs_error("ogs_sock_socket() failed for MBS UDP tunnel");
        *cause_value = OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE;
        ogs_freeaddrinfo(bind_address);
        return OGS_ERROR;
    }
    if (ogs_sock_bind(mbs_sess->udp_tunnel, bind_address) != OGS_OK) {
        ogs_error("ogs_sock_bind() failed for MBS UDP tunnel");
        *cause_value = OGS_PFCP_CAUSE_SYSTEM_FAILURE;
        ogs_freeaddrinfo(bind_address);
        ogs_sock_destroy(mbs_sess->udp_tunnel);
        mbs_sess->udp_tunnel = NULL;
        return OGS_ERROR;
    }
    // get the true local address to fill in ephemeral ports.
    name_len = sizeof(mbs_sess->udp_tunnel->local_addr.ss);
    getsockname(mbs_sess->udp_tunnel->fd, (struct sockaddr*)&mbs_sess->udp_tunnel->local_addr.ss, &name_len);
    ogs_freeaddrinfo(bind_address);
    /* find the MTU of the interface the UDP tunnel is bound to */
    mtu = get_mtu_for_address((const struct sockaddr*)&mbs_sess->udp_tunnel->local_addr.ss, name_len);
    mbs_sess->udp_tunnel_mtu = mtu - sizeof(struct ether_header) -
        (family == AF_INET ? sizeof(struct iphdr) : sizeof(struct ip6_hdr)) - sizeof(struct udphdr);
    ogs_debug("UDP tunnel using MTU of %i (%i after overheads)", mtu, mbs_sess->udp_tunnel_mtu);
    config = _udp_tunnel_make_pool_config(mbs_sess->udp_tunnel_mtu, 32); /* 32 buffers */
    if (!config) {
        ogs_error("Failed to allocate packet buffers configuration for UDP tunnel");
        *cause_value = OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE;
        return OGS_ERROR;
    }
    mbs_sess->udp_tunnel_pkbuf_pool = ogs_pkbuf_pool_create(config);
    ogs_free(config);
#if OGS_USE_TALLOC == 0
    // ogs_pkbuf_pool_create() is checked: a failure would leave udp_tunnel_pkbuf_pool NULL
    // while the tunnel socket is already bound and about to be registered for POLLIN, so the
    // first packet received would dereference it.
    if (!mbs_sess->udp_tunnel_pkbuf_pool) {
        ogs_error("ogs_pkbuf_pool_create() failed for MBS UDP tunnel");
        *cause_value = OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE;
        ogs_sock_destroy(mbs_sess->udp_tunnel);
        mbs_sess->udp_tunnel = NULL;
        return OGS_ERROR;
    }
#endif
    mbs_sess->udp_tunnel_poll = ogs_pollset_add(ogs_app()->pollset, OGS_POLLIN, mbs_sess->udp_tunnel->fd, _mbs_tunnel_poll_handler, mbs_sess);
    if (!mbs_sess->udp_tunnel_poll) {
        ogs_error("Failed to add UDP tunnel to pollset");
        *cause_value = OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE;
        return OGS_ERROR;
    }

    return OGS_OK;
}

/*
 * The IP Multicast Addressing Info of a downlink PDR: carried in the PDI, or, when the PDI names a
 * Traffic Endpoint ID, in that Create Traffic Endpoint (TS 29.244 Table 7.5.2.2-2: "This IE shall be present over N4mb, if multicast transport is used over N6mb and if Traffic Endpoint ID is not present.";
 * Table 7.5.2.7-1: "This IE shall be present over N4mb, if multicast transport is used over N6mb, if Traffic Endpoint ID is present in the DL PDR.").
 * Returns OGS_ERROR with the Offending IE set if an IE is not valid or the Traffic Endpoint ID is unknown.
 */
static int _decode_pdr_addressing(const ogs_pfcp_session_establishment_request_t *req, int pdr_index,
        bool *has_dest, ogs_pfcp_decoded_ip_multicast_address_t *dest,
        bool *has_src, ogs_pfcp_decoded_source_ip_address_t *src, uint16_t *offending_ie_value)
{
    const ogs_pfcp_tlv_create_pdr_t *cpdr = &req->create_pdr[pdr_index];
    const ogs_pfcp_tlv_create_traffic_endpoint_t *te = &req->create_traffic_endpoint;
    const ogs_pfcp_tlv_ip_multicast_addressing_info_within_pfcp_session_establishment_request_t *info =
        &cpdr->pdi.ip_multicast_addressing_info;

    *has_dest = false;
    *has_src = false;

    if (cpdr->pdi.traffic_endpoint_id.presence) {
        if (cpdr->pdi.traffic_endpoint_id.len < 1 || !te->presence || !te->traffic_endpoint_id.presence ||
                te->traffic_endpoint_id.len < 1 ||
                *(uint8_t *)cpdr->pdi.traffic_endpoint_id.data != *(uint8_t *)te->traffic_endpoint_id.data) {
            ogs_error("The Traffic Endpoint ID of PDR[%d] names no Create Traffic Endpoint of the request", pdr_index);
            *offending_ie_value = OGS_PFCP_TRAFFIC_ENDPOINT_ID_TYPE;
            return OGS_ERROR;
        }
        info = &te->ip_multicast_addressing_info;
    }

    if (!info->presence)
        return OGS_OK;

    if (info->ip_multicast_address.presence) {
        if (ogs_pfcp_parse_ip_multicast_address(dest, &info->ip_multicast_address) < 0) {
            *offending_ie_value = OGS_PFCP_IP_MULTICAST_ADDRESS_TYPE;
            return OGS_ERROR;
        }
        *has_dest = true;
    }
    if (info->source_ip_address.presence) {
        if (ogs_pfcp_parse_source_ip_address(src, &info->source_ip_address) < 0) {
            *offending_ie_value = OGS_PFCP_SOURCE_IP_ADDRESS_TYPE;
            return OGS_ERROR;
        }
        *has_src = true;
    }
    return OGS_OK;
}

/*
 * Allocates the ingress tunnel of the session as the Local Ingress Tunnel IE asks (TS 29.244 cl. 8.2.209).
 * On failure the cause and the Offending IE are set and nothing stays open.
 */
static int _allocate_ingress_tunnel(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_local_ingress_tunnel_t *lit,
        uint8_t *cause_value, uint16_t *offending_ie_value)
{
    ogs_sockaddr_t *bind_address = NULL;
    upf_context_t *ctx = upf_self();

    if (lit->choose) {
        /* The UP function assigns the address and the UDP port: the configured base address, and the
         * next port of the configured pool (or an ephemeral one). */
        ogs_copyaddrinfo(&bind_address, ctx->mbs_udp_tun_base_addr);
        if (!bind_address ||
                bind_address->ogs_sa_family != (lit->ipv4 ? AF_INET : AF_INET6)) {
            ogs_error("The MBS UDP tunnel base address is not of the requested address family");
            *cause_value = OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE;
            *offending_ie_value = OGS_PFCP_LOCAL_INGRESS_TUNNEL_TYPE;
            if (bind_address)
                ogs_freeaddrinfo(bind_address);
            return OGS_ERROR;
        }
        // _get_next_udp_tunnel_port() returns a host-byte-order port, straight from the parsed
        // YAML config, while ogs_sin_port is read by the kernel in network byte order; assigning
        // it unconverted binds the byte-swapped port on a little-endian host.
        if (lit->ipv4)
            bind_address->sin.sin_port = htons(_get_next_udp_tunnel_port(ctx));
        else
            bind_address->sin6.sin6_port = htons(_get_next_udp_tunnel_port(ctx));
        if (bind_address->ogs_sin_port == 0xffff) {
            // Error already reported, abort MBS Session
            *cause_value = OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE;
            *offending_ie_value = OGS_PFCP_LOCAL_INGRESS_TUNNEL_TYPE;
            ogs_freeaddrinfo(bind_address);
            return OGS_ERROR;
        }
        mbs_sess->udp_tunnel_port_pooled = !ctx->mbs_udp_tun_ephemeral_port;
    } else {
        /* The CP function gave the address and the UDP port of the tunnel (TS 29.244 cl. 8.2.209: with the
         * CH bit clear the address field of the V4 or V6 flag is present): the tunnel is opened there. */
        bind_address = ogs_calloc(1, sizeof(*bind_address));
        ogs_assert(bind_address);
        if (lit->ipv4) {
            bind_address->sin.sin_family = AF_INET;
            bind_address->sin.sin_port = lit->port;     /* already network order */
            bind_address->sin.sin_addr.s_addr = lit->addr;
        } else {
            bind_address->sin6.sin6_family = AF_INET6;
            bind_address->sin6.sin6_port = lit->port;
            memcpy(&bind_address->sin6.sin6_addr, lit->addr6, OGS_IPV6_LEN);
        }
    }

    return _open_udp_tunnel(mbs_sess, bind_address, cause_value, offending_ie_value);
}

/* Returns the cause for a Local Ingress Tunnel IE that is not valid, or OGS_PFCP_CAUSE_REQUEST_ACCEPTED. */
static uint8_t _check_local_ingress_tunnel(const ogs_pfcp_tlv_local_ingress_tunnel_t *ie_)
{
    const ogs_pfcp_local_ingress_tunnel_t *lit = ie_->data;
    uint16_t len = ie_->len;
    uint16_t want;

    if (lit == NULL || len < 1)
        return OGS_PFCP_CAUSE_INVALID_LENGTH;
    if (lit->ipv4 == lit->ipv6) {
        /* TS 29.244 cl. 8.2.209: "Only one of the V4 and V6 flags shall be set to "1"." */
        ogs_error("Local Ingress Tunnel with V4=%d and V6=%d", lit->ipv4, lit->ipv6);
        return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
    }
    /* With CH set the port and address fields are not present, else the field of the set flag is. */
    want = 1;
    if (!lit->choose)
        want += 2 + (lit->ipv4 ? OGS_IPV4_LEN : OGS_IPV6_LEN);
    if (len != want) {
        ogs_error("Local Ingress Tunnel length %u, expected %u", len, want);
        return OGS_PFCP_CAUSE_INVALID_LENGTH;
    }
    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}

/*
 * Joins the multicast tree of the session's AF multicast (TS 29.244 cl. 5.34.2.1A, JMBSSM): the channel (source,
 * group), or the group for any source when the session has no source address. The multicast router has to be
 * active (upf.mbs.multicast_router.activate), as it is what routes the group to the UPF's TUN device.
 */
static int _join_session_ssm(upf_mbs_sess_t *mbs_sess, uint8_t *cause_value, uint16_t *offending_ie_value)
{
    const ogs_ssm_t *ssm = &mbs_sess->ssm;
    int family, rv;
    const void *group, *source = NULL;
    size_t len;

    if (!upf_self()->mbs_multicastrouter_activate) {
        ogs_error("JMBSSM requested but the multicast router is not active (upf.mbs.multicast_router.activate)");
        *cause_value = OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED;
        *offending_ie_value = OGS_PFCP_MBSN4MBREQ_FLAGS_TYPE;
        return OGS_ERROR;
    }
    if (mbs_sess->ssm_dest_any || mbs_sess->ssm_dest_range) {
        ogs_error("JMBSSM requested for a range of groups or any group, not one SSM address");
        *cause_value = OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED;
        *offending_ie_value = OGS_PFCP_IP_MULTICAST_ADDRESS_TYPE;
        return OGS_ERROR;
    }
    if (!ssm->dest_ip_addr.ipv4 && !ssm->dest_ip_addr.ipv6) {
        ogs_error("JMBSSM requested without SSM address information");
        *cause_value = OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING;
        *offending_ie_value = OGS_PFCP_IP_MULTICAST_ADDRESSING_INFO_WITHIN_PFCP_SESSION_ESTABLISHMENT_REQUEST_TYPE;
        return OGS_ERROR;
    }

    if ((ssm->dest_ip_addr.ipv4 && !IN_MULTICAST(be32toh(ssm->dest_ip_addr.addr))) ||
            (!ssm->dest_ip_addr.ipv4 && ssm->dest_ip_addr.addr6[0] != 0xff)) {
        ogs_error("JMBSSM requested for an address that is not an IP multicast address");
        *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
        *offending_ie_value = OGS_PFCP_IP_MULTICAST_ADDRESS_TYPE;
        return OGS_ERROR;
    }

    if (ssm->dest_ip_addr.ipv4) {
        family = AF_INET;
        group = &ssm->dest_ip_addr.addr;
        len = OGS_IPV4_LEN;
        if (ssm->src_ip_addr.ipv4)
            source = &ssm->src_ip_addr.addr;
    } else {
        family = AF_INET6;
        group = ssm->dest_ip_addr.addr6;
        len = OGS_IPV6_LEN;
        if (ssm->src_ip_addr.ipv6)
            source = ssm->src_ip_addr.addr6;
    }
    if (!source && (ssm->src_ip_addr.ipv4 || ssm->src_ip_addr.ipv6)) {
        ogs_error("The source and the group of the SSM address are of different address families");
        *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
        *offending_ie_value = OGS_PFCP_SOURCE_IP_ADDRESS_TYPE;
        return OGS_ERROR;
    }
    if (source && mbs_sess->ssm_src_has_prefix && mbs_sess->ssm_src_prefix_len != 8 * len) {
        ogs_error("JMBSSM requested for a source subnet, not one source");
        *cause_value = OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED;
        *offending_ie_value = OGS_PFCP_SOURCE_IP_ADDRESS_TYPE;
        return OGS_ERROR;
    }

    rv = multicastrouter_join(family, source, group);
    if (rv != OGS_OK) {
        *cause_value = rv == MULTICASTROUTER_UNSUPPORTED ?
            OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED : OGS_PFCP_CAUSE_SYSTEM_FAILURE;
        *offending_ie_value = OGS_PFCP_MBSN4MBREQ_FLAGS_TYPE;
        return OGS_ERROR;
    }

    mbs_sess->mc_joined = true;
    mbs_sess->mc_family = family;
    mbs_sess->mc_has_source = source != NULL;
    if (source)
        memcpy(mbs_sess->mc_source, source, len);
    memcpy(mbs_sess->mc_group, group, len);
    ogs_info("Joined the multicast channel of the MBS session (%s)", source ? "source specific" : "any source");
    return OGS_OK;
}

/*
 * Checks that the mandatory IEs of an N4mb Session Establishment Request are present. On a failure the
 * response has been sent and false is returned, and no session has been allocated.
 */
bool upf_n4mb_check_session_establishment_request(
        ogs_pfcp_xact_t *xact, ogs_pfcp_session_establishment_request_t *req)
{
    uint8_t cause_value = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
    uint16_t offending_ie_value = 0;
    uint64_t cp_seid = 0;

    ogs_assert(xact);
    ogs_assert(req);

    if (req->cp_f_seid.presence == 0 || req->cp_f_seid.data == NULL) {
        ogs_error("No CP F-SEID in N4mb Session Establishment Request");
        cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        offending_ie_value = OGS_PFCP_F_SEID_TYPE;
    } else if (req->node_id.presence == 0) {
        ogs_error("No Node ID in N4mb Session Establishment Request");
        cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        offending_ie_value = OGS_PFCP_NODE_ID_TYPE;
    } else if (req->create_pdr[0].presence == 0) {
        ogs_error("No Create PDR in N4mb Session Establishment Request");
        cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        offending_ie_value = OGS_PFCP_CREATE_PDR_TYPE;
    } else if (req->create_far[0].presence == 0) {
        ogs_error("No Create FAR in N4mb Session Establishment Request");
        cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        offending_ie_value = OGS_PFCP_CREATE_FAR_TYPE;
    } else {
        int i;

        /* Precedence is mandatory in a Create PDR on N4mb. The shared PDR handler cannot require it: its
         * other callers (Sxa) carry no Precedence. */
        for (i = 0; i < OGS_MAX_NUM_OF_PDR && req->create_pdr[i].presence; i++) {
            if (req->create_pdr[i].precedence.presence == 0) {
                ogs_error("No Precedence in Create PDR of N4mb Session Establishment Request");
                cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
                offending_ie_value = OGS_PFCP_PRECEDENCE_TYPE;
                break;
            }
        }
    }

    if (cause_value == OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
        int i;

        /* Here, not in ogs_pfcp_handle_create_far(): the Offending IE type of that function is one octet, and
         * the type of MBS Multicast Parameters is 301. */
        for (i = 0; i < OGS_MAX_NUM_OF_FAR && req->create_far[i].presence; i++) {
            if (req->create_far[i].apply_action.presence &&
                    (req->create_far[i].apply_action.u16 & OGS_PFCP_APPLY_ACTION_FSSM) &&
                    req->create_far[i].mbs_multicast_parameters.presence == 0) {
                ogs_error("Apply Action FSSM without MBS Multicast Parameters");
                cause_value = OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING;
                offending_ie_value = OGS_PFCP_MBS_MULTICAST_PARAMETERS_TYPE;
                break;
            }
        }
    }

    if (cause_value == OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
        int i;

        for (i = 0; i < OGS_MAX_NUM_OF_PDR && req->create_pdr[i].presence; i++) {
            bool has_dest, has_src;
            ogs_pfcp_decoded_ip_multicast_address_t dest;
            ogs_pfcp_decoded_source_ip_address_t src;

            if (_decode_pdr_addressing(req, i, &has_dest, &dest, &has_src, &src, &offending_ie_value) != OGS_OK) {
                cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
                break;
            }
        }
    }

    if (cause_value == OGS_PFCP_CAUSE_REQUEST_ACCEPTED && req->create_traffic_endpoint.local_ingress_tunnel.presence) {
        cause_value = _check_local_ingress_tunnel(&req->create_traffic_endpoint.local_ingress_tunnel);
        if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED)
            offending_ie_value = OGS_PFCP_LOCAL_INGRESS_TUNNEL_TYPE;
    }

    if (cause_value == OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
        int i;

        for (i = 0; i < OGS_MAX_NUM_OF_PDR && req->create_pdr[i].presence; i++) {
            if (!req->create_pdr[i].pdi.local_ingress_tunnel.presence)
                continue;
            cause_value = _check_local_ingress_tunnel(&req->create_pdr[i].pdi.local_ingress_tunnel);
            if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
                offending_ie_value = OGS_PFCP_LOCAL_INGRESS_TUNNEL_TYPE;
                break;
            }
        }
    }

    if (cause_value == OGS_PFCP_CAUSE_REQUEST_ACCEPTED &&
            req->mbs_session_n4mb_control_information.area_session_id.presence &&
            req->mbs_session_n4mb_control_information.area_session_id.len != 2) {
        /* TS 29.244 cl. 8.2.213: "Length = 2" */
        ogs_error("Area Session ID length %u", req->mbs_session_n4mb_control_information.area_session_id.len);
        cause_value = OGS_PFCP_CAUSE_INVALID_LENGTH;
        offending_ie_value = OGS_PFCP_AREA_SESSION_ID_TYPE;
    }

    if (cause_value == OGS_PFCP_CAUSE_REQUEST_ACCEPTED &&
            req->mbs_session_n4mb_control_information.mbs_session_identifier.presence) {
        ogs_pfcp_mbs_session_identifier_t mbs_session_identifier;

        if (ogs_pfcp_parse_mbs_session_identifier(&mbs_session_identifier,
                &req->mbs_session_n4mb_control_information.mbs_session_identifier) < 0) {
            cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
            offending_ie_value = OGS_PFCP_MBS_SESSION_IDENTIFIER_TYPE;
        }
    }

    if (cause_value == OGS_PFCP_CAUSE_REQUEST_ACCEPTED)
        return true;

    /* The response header carries the SEID the CP function allocated, when it gave one. */
    if (req->cp_f_seid.presence && req->cp_f_seid.data &&
            req->cp_f_seid.len >= 1 + sizeof(uint64_t)) {  /* flags, SEID */
        ogs_pfcp_f_seid_t *f_seid = req->cp_f_seid.data;
        cp_seid = be64toh(f_seid->seid);
    }
    ogs_pfcp_send_error_message(xact, cp_seid,
            OGS_PFCP_SESSION_ESTABLISHMENT_RESPONSE_TYPE,
            cause_value, offending_ie_value);
    return false;
}

void upf_n4mb_handle_session_establishment_request(
        upf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_establishment_request_t *req)
{

    int i;
    uint8_t cause_value = 0;
    uint16_t offending_ie_value = 0;

    ogs_pfcp_pdr_t *pdr = NULL;
    ogs_pfcp_far_t *far = NULL;
    ogs_pfcp_pdr_t *created_pdr[OGS_MAX_NUM_OF_PDR];
    ogs_pfcp_mbs_session_identifier_t mbs_session_identifier;
    int num_of_created_pdr = 0;

    ogs_assert(xact);
    ogs_assert(req);

    ogs_debug("N4mb Session Establishment Request");

    cause_value = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;

    if (!mbs_sess) {
        /* The mandatory IEs are checked by upf_n4mb_check_session_establishment_request() before the
         * session is allocated, so a missing session here is an allocation failure. */
        ogs_error("No MBS Session Context");
        ogs_pfcp_send_error_message(xact, 0,
                OGS_PFCP_SESSION_ESTABLISHMENT_RESPONSE_TYPE,
                OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE, 0);
        return;
    }

    for (i = 0; i < OGS_MAX_NUM_OF_PDR; i++) {
        // NOTE (borieher): Put sereq_flags to NULL?
        uint8_t offending_ie_value_tmp = 0;
        created_pdr[i] = ogs_pfcp_handle_create_pdr(&mbs_sess->pfcp,
                &req->create_pdr[i], NULL,
                &cause_value, &offending_ie_value_tmp);
        offending_ie_value = offending_ie_value_tmp;
        if (created_pdr[i] == NULL)
            break;
    }
    num_of_created_pdr = i;

    if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED)
        goto cleanup;

    for (i = 0; i < OGS_MAX_NUM_OF_FAR; i++) {
        uint8_t offending_ie_value_tmp = 0;
        if (ogs_pfcp_handle_create_far(&mbs_sess->pfcp, &req->create_far[i],
                    &cause_value, &offending_ie_value_tmp) == NULL) {
            offending_ie_value = offending_ie_value_tmp;
            break;
        }
        offending_ie_value = offending_ie_value_tmp;
    }
    if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED)
        goto cleanup;

    if (req->apn_dnn.presence) {
        char apn_dnn[OGS_MAX_DNN_LEN+1];

        if (ogs_fqdn_parse(apn_dnn, req->apn_dnn.data,
                ogs_min(req->apn_dnn.len, OGS_MAX_DNN_LEN)) <= 0) {
            ogs_error("Malformed APN/DNN in N4mb Session Establishment Request");
            cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
            offending_ie_value = OGS_PFCP_APN_DNN_TYPE;
            goto cleanup;
        }

        if (mbs_sess->dnn)
            ogs_free(mbs_sess->dnn);
        mbs_sess->dnn = ogs_strdup(apn_dnn);
        ogs_assert(mbs_sess->dnn);
    }

    for (i = 0; i < OGS_MAX_NUM_OF_QER; i++) {
        uint8_t offending_ie_value_tmp = 0;
        if (ogs_pfcp_handle_create_qer(&mbs_sess->pfcp, &req->create_qer[i],
                    &cause_value, &offending_ie_value_tmp) == NULL) {
            offending_ie_value = offending_ie_value_tmp;
            break;
        }
        offending_ie_value = offending_ie_value_tmp;
    }
    if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED)
        goto cleanup;

    if (req->create_traffic_endpoint.presence &&
            req->create_traffic_endpoint.traffic_endpoint_id.presence &&
            req->create_traffic_endpoint.traffic_endpoint_id.len >= 1) {
        mbs_sess->traffic_endpoint_id_presence = true;
        mbs_sess->traffic_endpoint_id = *(uint8_t *)req->create_traffic_endpoint.traffic_endpoint_id.data;
    }

    /* The single ingress tunnel of the session: asked for in the Create Traffic Endpoint, or in the PDI of the
     * PDRs that name no Traffic Endpoint (the MB-UPF "shall assign a single ingress tunnel address, regardless of
     * the number of MBS QoS flows", TS 29.244 cl. 5.34.2.2). */
    {
        const ogs_pfcp_tlv_local_ingress_tunnel_t *lit_ie = NULL;

        if (req->create_traffic_endpoint.presence && req->create_traffic_endpoint.local_ingress_tunnel.presence) {
            lit_ie = &req->create_traffic_endpoint.local_ingress_tunnel;
            mbs_sess->ingress_in_traffic_endpoint = true;
        }
        for (i = 0; !lit_ie && i < num_of_created_pdr; i++) {
            if (req->create_pdr[i].pdi.local_ingress_tunnel.presence &&
                    !req->create_pdr[i].pdi.traffic_endpoint_id.presence)
                lit_ie = &req->create_pdr[i].pdi.local_ingress_tunnel;
        }
        if (lit_ie &&
                _allocate_ingress_tunnel(mbs_sess, lit_ie->data, &cause_value, &offending_ie_value) != OGS_OK)
            goto cleanup;

        for (i = 0; i < num_of_created_pdr; i++)
            mbs_sess->pdr_has_ingress_tunnel[i] = mbs_sess->udp_tunnel &&
                req->create_pdr[i].pdi.local_ingress_tunnel.presence &&
                !req->create_pdr[i].pdi.traffic_endpoint_id.presence;
    }

    /* Setup AF SSM IP address */
    for (i = 0; i < num_of_created_pdr; i++) {
        int rv;
        bool has_dest, has_src;
        ogs_pfcp_decoded_ip_multicast_address_t dest;
        ogs_pfcp_decoded_source_ip_address_t src;

        /* Already validated by the request check */
        rv = _decode_pdr_addressing(req, i, &has_dest, &dest, &has_src, &src, &offending_ie_value);
        ogs_assert(rv == OGS_OK);
        if (!has_dest && !has_src)
            continue;

        // TODO (borieher): Modify this to work with IP ranges
        if (has_src) {
            mbs_sess->ssm.src_ip_addr = src.addr;
            mbs_sess->ssm_src_has_prefix = src.has_mask_prefix_len;
            mbs_sess->ssm_src_prefix_len = src.mask_prefix_len;
        }
        if (has_dest) {
            mbs_sess->ssm_dest_any = dest.any;
            mbs_sess->ssm_dest_range = dest.range;
            mbs_sess->ssm_dest_end = dest.end;
            if (!dest.any)
                mbs_sess->ssm.dest_ip_addr = dest.start;
        }
        {
            char dst_s[OGS_ADDRSTRLEN] = "-", src_s[OGS_ADDRSTRLEN] = "-";

            if (mbs_sess->ssm.dest_ip_addr.ipv4)
                OGS_INET_NTOP(&mbs_sess->ssm.dest_ip_addr.addr, dst_s);
            else if (mbs_sess->ssm.dest_ip_addr.ipv6)
                OGS_INET6_NTOP(mbs_sess->ssm.dest_ip_addr.addr6, dst_s);
            if (mbs_sess->ssm.src_ip_addr.ipv4)
                OGS_INET_NTOP(&mbs_sess->ssm.src_ip_addr.addr, src_s);
            else if (mbs_sess->ssm.src_ip_addr.ipv6)
                OGS_INET6_NTOP(mbs_sess->ssm.src_ip_addr.addr6, src_s);
            ogs_debug("MBS session AF multicast: destination [%s] source [%s]", dst_s, src_s);
        }
        upf_mbs_sess_set_ssm(mbs_sess);
    }

    for (i = 0; i < num_of_created_pdr; i++) {
        pdr = created_pdr[i];
        ogs_assert(pdr);

        /* Setup UPF-N3-TEID & QFI Hash */
        // NOTE (borieher): restoration_indication set to NULL
        if (pdr->f_teid_len)
            ogs_pfcp_object_teid_hash_set(
                    OGS_PFCP_OBJ_SESS_TYPE, pdr, NULL);

        // Setup C-TEID
        if (pdr->teid)
            mbs_sess->c_teid = pdr->teid;
    }

    // TODO (borieher): Handle the reception of MBS Session N4mb Control Information IE
    if (req->mbs_session_n4mb_control_information.presence) {

        // MBS Session Identifier IE
        if (req->mbs_session_n4mb_control_information.mbs_session_identifier.presence) {
            // TODO (borieher): Continue parsing MBS Session Identifier, after fixing it
            if (ogs_pfcp_parse_mbs_session_identifier(&mbs_session_identifier, &req->mbs_session_n4mb_control_information.mbs_session_identifier) < 0) {
                cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
                offending_ie_value = OGS_PFCP_MBS_SESSION_IDENTIFIER_TYPE;
                goto cleanup;
            } else {
                // Free old values
                if (mbs_sess->mbs_session_id.is_tmgi && mbs_sess->mbs_session_id.tmgi) ogs_free(mbs_sess->mbs_session_id.tmgi);
                if (mbs_sess->mbs_session_id.is_ssm && mbs_sess->mbs_session_id.ssm) ogs_free(mbs_sess->mbs_session_id.ssm);
                if (mbs_sess->mbs_session_id.nid) ogs_free(mbs_sess->mbs_session_id.nid);

                // Initialise to empty
                mbs_sess->mbs_session_id.is_tmgi = 0;
                mbs_sess->mbs_session_id.is_ssm = 0;
                mbs_sess->mbs_session_id.tmgi = NULL;
                                // NULLed after the free just above (the is_ssm/ssm check); left dangling it is freed a
                                // second time by upf_mbs_sess_remove() (context.c) at teardown, if the session updates away
                                // from SSM form before it is deleted.
                mbs_sess->mbs_session_id.ssm = NULL;
                mbs_sess->mbs_session_id.nid = NULL;

                if (mbs_session_identifier.tmgif) {
                    mbs_sess->mbs_session_id.is_tmgi = 1;
                    mbs_sess->mbs_session_id.tmgi = (ogs_tmgi_t*)ogs_calloc(1, sizeof(*mbs_sess->mbs_session_id.tmgi));
                    mbs_sess->mbs_session_id.tmgi->mbs_service_id = ogs_msprintf("%.2X%.2X%.2X", mbs_session_identifier.tmgi.mbs_service_id[2], mbs_session_identifier.tmgi.mbs_service_id[1], mbs_session_identifier.tmgi.mbs_service_id[0]);
                    /* The three PLMN octets are those of TS 24.008 figure 10.5.154 (TS 29.244 cl. 8.2.206: "The
                     * TMGI field shall be encoded as octets 3 to octet 8 in the figure 10.5.154"), the layout of
                     * ogs_nas_plmn_id_t: converted, not copied digit by digit, since ogs_plmn_id_t names the
                     * digits differently. */
                    ogs_nas_to_plmn_id(&mbs_sess->mbs_session_id.tmgi->plmn_id,
                            (ogs_nas_plmn_id_t *)mbs_session_identifier.tmgi.plmn_id);
                    ogs_debug("MBS Session Identifier: TMGI PLMN [%03d/%0*d]",
                            ogs_plmn_id_mcc(&mbs_sess->mbs_session_id.tmgi->plmn_id),
                            ogs_plmn_id_mnc_len(&mbs_sess->mbs_session_id.tmgi->plmn_id),
                            ogs_plmn_id_mnc(&mbs_sess->mbs_session_id.tmgi->plmn_id));
                    /* mbs_sess->mbs_session_id.tmgi->expiration_time = ; */
                } else if (mbs_session_identifier.ssmif) {
                    mbs_sess->mbs_session_id.is_ssm = 1;
                    mbs_sess->mbs_session_id.ssm = (ogs_ssm_t*)ogs_calloc(1, sizeof(*mbs_sess->mbs_session_id.ssm));
                    switch (mbs_session_identifier.ssm.ip_source_address.address_type) {
                    case 0: /* IPv4 */
                        mbs_sess->mbs_session_id.ssm->src_ip_addr.ipv4 = 1;
                        mbs_sess->mbs_session_id.ssm->src_ip_addr.addr = mbs_session_identifier.ssm.ip_source_address.ipv4_addr;
                        mbs_sess->mbs_session_id.ssm->src_ip_addr.len = mbs_session_identifier.ssm.ip_source_address.address_length;
                        break;
                    case 1: /* IPv6 */
                        mbs_sess->mbs_session_id.ssm->src_ip_addr.ipv6 = 1;
                        memcpy(mbs_sess->mbs_session_id.ssm->src_ip_addr.addr6, mbs_session_identifier.ssm.ip_source_address.ipv6_addr, sizeof(mbs_sess->mbs_session_id.ssm->src_ip_addr.addr6));
                        mbs_sess->mbs_session_id.ssm->src_ip_addr.len = mbs_session_identifier.ssm.ip_source_address.address_length;
                        break;
                    default:
                        ogs_warn("Unknown address_type %i in SSM source address", mbs_session_identifier.ssm.ip_source_address.address_type);
                        break;
                    }
                    switch (mbs_session_identifier.ssm.ip_multicast_distribution_address.address_type) {
                    case 0: /* IPv4 */
                        mbs_sess->mbs_session_id.ssm->dest_ip_addr.ipv4 = 1;
                        mbs_sess->mbs_session_id.ssm->dest_ip_addr.addr = mbs_session_identifier.ssm.ip_multicast_distribution_address.ipv4_addr;
                        mbs_sess->mbs_session_id.ssm->dest_ip_addr.len = mbs_session_identifier.ssm.ip_multicast_distribution_address.address_length;
                        break;
                    case 1: /* IPv6 */
                        mbs_sess->mbs_session_id.ssm->dest_ip_addr.ipv6 = 1;
                        memcpy(mbs_sess->mbs_session_id.ssm->dest_ip_addr.addr6, mbs_session_identifier.ssm.ip_multicast_distribution_address.ipv6_addr, sizeof(mbs_sess->mbs_session_id.ssm->dest_ip_addr.addr6));
                        mbs_sess->mbs_session_id.ssm->dest_ip_addr.len = mbs_session_identifier.ssm.ip_multicast_distribution_address.address_length;
                        break;
                    default:
                        ogs_warn("Unknown address_type %i in SSM destination address", mbs_session_identifier.ssm.ip_multicast_distribution_address.address_type);
                        break;
                    }
                }
                if (!mbs_sess->ssm.dest_ip_addr.ipv4 && !mbs_sess->ssm.dest_ip_addr.ipv6 && mbs_session_identifier.ssmif) {
                    /* Duplicate to ID SSM to SSM settings if we don't already have an SSM destination */
                    memcpy(&mbs_sess->ssm, mbs_sess->mbs_session_id.ssm, sizeof(mbs_sess->ssm));
                    upf_mbs_sess_set_ssm(mbs_sess);
                }
                if (mbs_session_identifier.nidif) {
                    /* TS 29.244 cl. 8.2.206: eleven hexadecimal digits, the first in bits 8 to 5 of the first octet
                     * and the last in bits 8 to 5 of the sixth; bits 4 to 1 of the sixth octet are 0. */
                    char *nid = (char*)ogs_calloc(1, 12);
                    int d;

                    ogs_assert(nid);
                    for (d = 0; d < 11; d++)
                        nid[d] = "0123456789abcdef"[(d % 2 == 0) ?
                            (mbs_session_identifier.nid[d / 2] >> 4) : (mbs_session_identifier.nid[d / 2] & 0x0f)];
                    mbs_sess->mbs_session_id.nid = nid;
                    ogs_debug("MBS Session Identifier: NID [%s]", nid);
                }
            }
        }

        // Area Session ID IE: with the MBS Session Identifier it identifies the service area part of a
        // location dependent MBS service (TS 29.244 cl. 8.2.213); this PFCP session is that part.
        if (req->mbs_session_n4mb_control_information.area_session_id.presence) {
            uint8_t *v = req->mbs_session_n4mb_control_information.area_session_id.data;

            mbs_sess->area_session_id_presence = true;
            mbs_sess->area_session_id = (uint16_t)((v[0] << 8) | v[1]);
            ogs_debug("MBS session Area Session ID [%u]", mbs_sess->area_session_id);
        }

        // MBSN4mbReq-Flags IE
        if (req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.presence)
            memcpy(&mbs_sess->mbs_flags, req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.data,
                sizeof(ogs_pfcp_mbsn4mbreq_flags_t));
    }

    // The MB-SMF asks for a low-layer SSM with PLLSSM (MBSN4mbReq-Flags). Without it the session
    // has no N3mb multicast transport and the FARs are left as received.
    if (mbs_sess->mbs_flags.provide_lower_layer_ssm) {
        upf_mbs_sess_set_llssm(mbs_sess);
        if (!mbs_sess->ll_ssm) {
            ogs_error("No low-layer SSM available for the MBS session");
            cause_value = OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE;
            offending_ie_value = OGS_PFCP_MBS_SESSION_N4MB_CONTROL_INFORMATION_TYPE;
            goto cleanup;
        }
    }

    /* Setup GTP Node */
    ogs_list_for_each(&mbs_sess->pfcp.far_list, far) {
        if (!mbs_sess->ll_ssm)
            break;
        if (mbs_sess->ll_ssm->dest_ip_addr.ipv4) {
            far->outer_header_creation.addr = mbs_sess->ll_ssm->dest_ip_addr.addr;
            far->outer_header_creation_len += OGS_IPV4_LEN;
        } else if (mbs_sess->ll_ssm->dest_ip_addr.ipv6) {
            memcpy(&far->outer_header_creation.addr6, mbs_sess->ll_ssm->dest_ip_addr.addr6, OGS_IPV6_LEN);
            far->outer_header_creation_len += OGS_IPV6_LEN;
        }

        if (OGS_ERROR == ogs_pfcp_setup_far_gtpu_node(far)) {
            ogs_fatal("CHECK CONFIGURATION: upf.gtpu");
            ogs_fatal("ogs_pfcp_setup_far_gtpu_node() failed");
            goto cleanup;
        }

        if (far->gnode) {
            ogs_pfcp_far_f_teid_hash_set(far);
        }
    }

    /* JMBSSM: the MB-UPF "shall join the multicast tree towards the Source Specific Multicast (SSM) address
     * information", so the join is done, and its failure answered, before the success response. */
    if (mbs_sess->mbs_flags.join_mbs_session_ssm &&
            _join_session_ssm(mbs_sess, &cause_value, &offending_ie_value) != OGS_OK)
        goto cleanup;

    ogs_assert(OGS_OK ==
            upf_pfcp_n4mb_send_session_establishment_response(
                xact, mbs_sess, created_pdr, num_of_created_pdr));


    return;

cleanup:
    ogs_pfcp_sess_clear(&mbs_sess->pfcp);
    ogs_pfcp_send_error_message(xact, mbs_sess ? mbs_sess->smf_n4mb_f_seid.seid : 0,
            OGS_PFCP_SESSION_ESTABLISHMENT_RESPONSE_TYPE,
            cause_value, offending_ie_value);
    // The establishment was rejected: the MB-SMF will not delete a session it was told was not established, so the
    // session must not stay in the pool and the hashes. Removal also releases the UDP tunnel (poll, port, socket, buffer
    // pool) in the order the poll must be removed before its socket is closed.
    if (mbs_sess)
        upf_mbs_sess_remove(mbs_sess);
}

/*
 * BUG FIX: this handler did not exist before -- pfcp-sm.c's Session Deletion Request dispatch had no
 * MBS-aware branch at all (see the matching fix there), so an MBS session's N4mb Session Deletion Request
 * was never routed anywhere sensible. Mirrors the non-MBS upf_n4_handle_session_deletion_request(): reply
 * then free the local session. This is what actually frees the entry in the fixed-size MBS session pool
 * (OGS_MAX_NUM_OF_MBS_SESSIONS=20) that was observed exhausting after repeated test session
 * creation/deletion cycles this session ("Maximum number of MBS Sessions[20] reached").
 */
void upf_n4mb_handle_session_deletion_request(
        upf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_deletion_request_t *req)
{
    ogs_assert(xact);
    ogs_assert(req);

    ogs_debug("N4mb Session Deletion Request");

    if (!mbs_sess) {
        ogs_error("No MBS Session Context");
        ogs_pfcp_send_error_message(xact, 0,
                OGS_PFCP_SESSION_DELETION_RESPONSE_TYPE,
                OGS_PFCP_CAUSE_SESSION_CONTEXT_NOT_FOUND, 0);
        return;
    }

    upf_pfcp_n4mb_send_session_deletion_response(xact, mbs_sess);

    upf_mbs_sess_remove(mbs_sess);
}

static void _mbs_tunnel_poll_handler(short when, ogs_socket_t fd, void *data)
{
    upf_mbs_sess_t *mbs_sess = data;
    ogs_pkbuf_t *buf;
    void *data_buf;
    size_t read_bytes;

    buf = ogs_pkbuf_alloc(mbs_sess->udp_tunnel_pkbuf_pool, mbs_sess->udp_tunnel_mtu + OGS_GTPV1U_5GC_HEADER_LEN);
    ogs_pkbuf_reserve(buf, OGS_GTPV1U_5GC_HEADER_LEN); /* reserve space to add GTP-U headers later */
    data_buf = ogs_pkbuf_put(buf, mbs_sess->udp_tunnel_mtu); /* reserve buffer for read */

    /* TODO: use ogs_recvfrom and check that the sending IP is allowed to send to this port */
    read_bytes = ogs_recv(fd, data_buf, mbs_sess->udp_tunnel_mtu, MSG_DONTWAIT);
    if (read_bytes > 0) {
        ogs_pkbuf_trim(buf, read_bytes);
        /* call pkbuf processing routine for incoming N6mb/N9mb */
        upf_gtpv1_receive_buffer_for_mbs_sess(buf, mbs_sess);
    }
}

static uint16_t _get_next_udp_tunnel_port(upf_context_t *ctx)
{
    uint16_t *port_ptr = NULL;
    if (ctx->mbs_udp_tun_ephemeral_port) return 0;
    if (ctx->mbs_udp_tun_ports_next_free > 0) {
        ctx->mbs_udp_tun_ports_next_free--;
        port_ptr = ctx->mbs_udp_tun_ports_free[ctx->mbs_udp_tun_ports_next_free];
        ctx->mbs_udp_tun_ports_free[ctx->mbs_udp_tun_ports_next_free] = NULL;
    }
    if (port_ptr == NULL) {
        ogs_error("Out of MBS UDP tunnel ports, please configure more or use ephemeral ports");
        return 0xffff;
    }
    return *port_ptr;
}

static ogs_pkbuf_config_t *_udp_tunnel_make_pool_config(size_t max_buf_size, size_t max_buffers)
{
    ogs_pkbuf_config_t *config = ogs_calloc(1, sizeof(*config));
    size_t pkbuf_buffer_size = sizeof(ogs_pkbuf_t) + max_buf_size;

    if (!config) return NULL;
    if (pkbuf_buffer_size <= 128) {
        config->cluster_128_pool = 128 * max_buffers;
    } else if (pkbuf_buffer_size <= 256) {
        config->cluster_256_pool = 256 * max_buffers;
    } else if (pkbuf_buffer_size <= 512) {
        config->cluster_512_pool = 512 * max_buffers;
    } else if (pkbuf_buffer_size <= 1024) {
        config->cluster_1024_pool = 1024 * max_buffers;
    } else if (pkbuf_buffer_size <= 2048) {
        config->cluster_2048_pool = 2048 * max_buffers;
    } else if (pkbuf_buffer_size <= 8192) {
        config->cluster_8192_pool = 8192 * max_buffers;
    } else if (pkbuf_buffer_size <= 32768) {
        config->cluster_32768_pool = 32768 * max_buffers;
    } else {
        // Anything bigger would break maximum path MTU
        config->cluster_big_pool = 65536 * max_buffers;
    }
    return config;
}

static int get_mtu_for_address(const struct sockaddr *addr, socklen_t addr_len)
{
    typedef union {
        struct in_addr in;
        struct in6_addr in6;
    } address_type;

    int mtu = 1500;
    int i;
    struct ifaddrs *ifa = NULL;
    struct ifaddrs *ifa_it;
    const char *if_name = NULL;
    size_t a_len;
    size_t a_offset;
    address_type addr_with_mask;
    address_type ifc_with_mask;

    if (addr->sa_family != AF_INET && addr->sa_family != AF_INET6) {
        ogs_error("Unknown socket address type when finding MTU, using default 1500 MTU");
        return mtu;
    }

    if ((addr->sa_family == AF_INET && addr_len < sizeof(struct sockaddr_in)) ||
        (addr->sa_family == AF_INET6 && addr_len < sizeof(struct sockaddr_in6))) {
        ogs_error("Wrong address size when retrieving MTU, using default 1500 MTU");
        return mtu;
    }

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in sa;
        addr_len = sizeof(struct sockaddr_in);
        a_len = sizeof(struct in_addr);
        a_offset = ((unsigned char*)&sa.sin_addr) - ((unsigned char*)&sa);
    } else {
        struct sockaddr_in6 sa;
        addr_len = sizeof(sa);
        a_len = sizeof(sa.sin6_addr);
        a_offset = ((unsigned char*)&sa.sin6_addr) - ((unsigned char*)&sa);
    }

    if (getifaddrs(&ifa) == -1) {
        ogs_warn("getifaddrs failed: %s", strerror(errno));
        return mtu;
    }

    for (ifa_it = ifa; ifa_it; ifa_it = ifa_it->ifa_next) {
        if (!ifa_it->ifa_addr)
            continue;
        if (!ifa_it->ifa_name)
            continue;
        if (ifa_it->ifa_addr->sa_family != addr->sa_family)
            continue;

        memcpy(&addr_with_mask, ((unsigned char*)addr) + a_offset, a_len);
        memcpy(&ifc_with_mask, ((unsigned char*)ifa_it->ifa_addr) + a_offset, a_len);

        if (ifa_it->ifa_netmask) {
            for (i=0; i<a_len; i++) {
                ((unsigned char*)&addr_with_mask)[i] &= ((unsigned char*)ifa_it->ifa_netmask)[a_offset + i];
                ((unsigned char*)&ifc_with_mask)[i] &= ((unsigned char*)ifa_it->ifa_netmask)[a_offset + i];
            }
        }

#if 0
        {
            char buf1[INET6_ADDRSTRLEN], buf2[INET6_ADDRSTRLEN];
            inet_ntop(addr->sa_family, &addr_with_mask, buf1, sizeof(buf1));
            inet_ntop(addr->sa_family, &ifc_with_mask, buf2, sizeof(buf2));
            ogs_debug("Compare %s to %s...", buf1, buf2);
        }
#endif

        if (memcmp(&addr_with_mask, &ifc_with_mask, a_len) == 0) {
            /* ogs_debug("Found interface name: %s", ifa_it->ifa_name); */
            if_name = ifa_it->ifa_name;
            break;
        }
    }

    if (if_name) {
        struct ifreq ifr;
        strcpy(ifr.ifr_name, if_name);
        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            ogs_warn("Failed to open datagram socket: %s", strerror(errno));
        } else {
            if (ioctl(fd, SIOCGIFMTU, &ifr) == -1) {
                ogs_warn("Failed to get MTU: %s", strerror(errno));
            } else {
                mtu = ifr.ifr_mtu;
            }
            close(fd);
        }
    } else {
        ogs_warn("Unable to get interface name for address, using default MTU of 1500");
    }

    freeifaddrs(ifa);

    return mtu;
}
