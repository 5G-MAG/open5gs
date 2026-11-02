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
#include "path-management.h"
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
static int _decode_addressing_info(
        const ogs_pfcp_tlv_ip_multicast_addressing_info_within_pfcp_session_establishment_request_t *info,
        bool *has_dest, ogs_pfcp_decoded_ip_multicast_address_t *dest,
        bool *has_src, ogs_pfcp_decoded_source_ip_address_t *src, uint16_t *offending_ie_value)
{
    *has_dest = false;
    *has_src = false;

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
 * IP Multicast Addressing Info on N4mb (TS 29.244 Table 7.5.2.2-4): the IP Multicast Address is "M" ("For N4mb, the IP Multicast Address
 * shall be set to the value of IP Multicast Address in the SSM for the MBS Session") and so is the Source IP Address in the N4mb row of
 * the table ("This IE shall be set to the value of the IP Source Address in the SSM for the MBS Session"); the "If this IE is not present,
 * this indicates "any" source IP address" of the N4 row does not apply. A member that is missing is answered with cause 66 and its type
 * (cl. 7.6.6). Both members are absent when the PDI carries no information at all.
 */
static int _check_addressing_members(bool has_dest, bool has_src, uint8_t *cause_value, uint16_t *offending_ie_value)
{
    if (has_dest && !has_src) {
        *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        *offending_ie_value = OGS_PFCP_SOURCE_IP_ADDRESS_TYPE;
        return OGS_ERROR;
    }
    if (has_src && !has_dest) {
        *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        *offending_ie_value = OGS_PFCP_IP_MULTICAST_ADDRESS_TYPE;
        return OGS_ERROR;
    }
    return OGS_OK;
}

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

    return _decode_addressing_info(info, has_dest, dest, has_src, src, offending_ie_value);
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
        mbs_sess->udp_tunnel_pooled_port = ntohs(lit->ipv4 ? bind_address->sin.sin_port : bind_address->sin6.sin6_port);
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
 * The Multicast Transport Information IE of the request (TS 29.244 cl. 8.2.207, Figure 8.2.207-1): octet 5 spare,
 * octets 6 to 9 the C-TEID, then the IP multicast distribution address and the IP source address, each behind an
 * octet with the Address Type (bits 8 and 7) and the Address Length (bits 6 to 1). "Other combinations of values are
 * not valid." Returns the cause for an IE that is not valid, else OGS_PFCP_CAUSE_REQUEST_ACCEPTED.
 */
static uint8_t _decode_multicast_transport_information(const ogs_tlv_octet_t *ie,
        uint32_t *c_teid, ogs_ip_t *dest, ogs_ip_t *src)
{
    const uint8_t *d = ie->data;
    size_t len = ie->len, pos = 5;
    ogs_ip_t *out[2] = { dest, src };
    int i;

    if (len < 5 + 1)
        return OGS_PFCP_CAUSE_INVALID_LENGTH;
    *c_teid = (uint32_t)d[1] << 24 | (uint32_t)d[2] << 16 | (uint32_t)d[3] << 8 | d[4];

    for (i = 0; i < 2; i++) {
        unsigned int type, alen;

        if (pos >= len)
            return OGS_PFCP_CAUSE_INVALID_LENGTH;
        type = d[pos] >> 6;
        alen = d[pos] & 0x3f;
        pos++;
        if (!((type == 0 && alen == OGS_IPV4_LEN) || (type == 1 && alen == OGS_IPV6_LEN)))
            return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
        if (pos + alen > len)
            return OGS_PFCP_CAUSE_INVALID_LENGTH;
        memset(out[i], 0, sizeof(*out[i]));
        if (type == 0) {
            out[i]->ipv4 = 1;
            out[i]->len = OGS_IPV4_LEN;
            memcpy(&out[i]->addr, d + pos, OGS_IPV4_LEN);
        } else {
            out[i]->ipv6 = 1;
            out[i]->len = OGS_IPV6_LEN;
            memcpy(out[i]->addr6, d + pos, OGS_IPV6_LEN);
        }
        pos += alen;
    }
    if (pos != len)
        return OGS_PFCP_CAUSE_INVALID_LENGTH;

    /* the destination is an IP multicast address and the source a unicast address of its family */
    if (dest->ipv4 != src->ipv4)
        return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
    if (dest->ipv4 ? !IN_MULTICAST(be32toh(dest->addr)) : dest->addr6[0] != 0xff)
        return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
    if (src->ipv4 ? (src->addr == 0 || IN_MULTICAST(be32toh(src->addr))) :
            (src->addr6[0] == 0xff || memcmp(src->addr6, &in6addr_any, OGS_IPV6_LEN) == 0))
        return OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;

    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}

/*
 * Whether the MB-UPF can use the low layer SSM that the MB-SMF asks for in an MBS restoration (TS 29.244 Table 7.5.2.1-4
 * NOTE: "If one requested address is not available at the MB-UPF, the MB-UPF shall reject the PFCP Session
 * Establishment Request with the cause "PFCP session restoration failure due to requested resource not available""):
 * the group is not held by another session, and the source is an address the MB-UPF sends GTP-U from (a configured
 * upf.gtpu address; for IPv6 there has to be an IPv6 GTP-U socket).
 */
static bool _requested_llssm_available(const ogs_ip_t *dest, const ogs_ip_t *src)
{
    ogs_socknode_t *node = NULL;
    ogs_sockaddr_t *addr = NULL;

    if (ogs_pfcp_llssm_dest_in_use(dest))
        return false;
    if (!src->ipv4 && !ogs_gtp_self()->gtpu_sock6)
        return false;

    ogs_list_for_each(&ogs_gtp_self()->gtpu_list, node) {
        for (addr = node->addr; addr; addr = addr->next) {
            if (src->ipv4 && addr->ogs_sa_family == AF_INET && addr->sin.sin_addr.s_addr == src->addr)
                return true;
            if (!src->ipv4 && addr->ogs_sa_family == AF_INET6 &&
                    memcmp(&addr->sin6.sin6_addr, src->addr6, OGS_IPV6_LEN) == 0)
                return true;
        }
    }
    return false;
}

/*
 * The destination of a FAR with the low layer SSM: the group, the C-TEID and the GTP node. The address is the one of
 * the session's low layer SSM, the C-TEID is the TEID of the G-PDUs a FSSM FAR sends; a restoration gives it in the
 * request, so it is set here and not when the response is built.
 */
static int _setup_far_node(upf_mbs_sess_t *mbs_sess, ogs_pfcp_far_t *far)
{
    ogs_assert(mbs_sess->ll_ssm);

    if (mbs_sess->ll_ssm->dest_ip_addr.ipv4) {
        far->outer_header_creation.addr = mbs_sess->ll_ssm->dest_ip_addr.addr;
        far->outer_header_creation_len += OGS_IPV4_LEN;
    } else if (mbs_sess->ll_ssm->dest_ip_addr.ipv6) {
        memcpy(&far->outer_header_creation.addr6, mbs_sess->ll_ssm->dest_ip_addr.addr6, OGS_IPV6_LEN);
        far->outer_header_creation_len += OGS_IPV6_LEN;
        /* The description of an FSSM FAR carries no address (cl. 8.2.56 NOTE 7); the FAR kept here names the family of
         * its low layer SSM by the IPv6 bit, which the node of the FAR and its hash read */
        far->outer_header_creation.ip6 = 1;
    }

    if (far->mbs_multicast_parameters)
        far->outer_header_creation.teid = mbs_sess->c_teid;

    if (OGS_ERROR == ogs_pfcp_setup_far_gtpu_node(far)) {
        ogs_fatal("CHECK CONFIGURATION: upf.gtpu");
        ogs_fatal("ogs_pfcp_setup_far_gtpu_node() failed");
        return OGS_ERROR;
    }

    if (far->gnode)
        ogs_pfcp_far_f_teid_hash_set(far);

    return OGS_OK;
}

/*
 * MBS Multicast Parameters (TS 29.244 Table 7.5.2.3-5): Destination Interface "M" ("This IE shall identify the destination
 * interface of the outgoing packet.") and Outer Header Creation "M" ("This IE shall instruct the MB-UPF to create a GTP-U header
 * using the Low Layer SSM and the C-TEID allocated to the MBS session."). A missing mandatory member is answered with cause 66
 * and its type (cl. 7.6.6). The Outer Header Creation is also refused if it is empty, which the FAR handler would not survive.
 */
static int _check_mbs_multicast_parameters(const ogs_pfcp_tlv_mbs_multicast_parameters_t *mp,
        uint8_t *cause_value, uint16_t *offending_ie_value)
{
    if (!mp->presence)
        return OGS_OK;
    if (!mp->destination_interface.presence) {
        *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        *offending_ie_value = OGS_PFCP_DESTINATION_INTERFACE_TYPE;
        return OGS_ERROR;
    }
    if (!mp->outer_header_creation.presence) {
        *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        *offending_ie_value = OGS_PFCP_OUTER_HEADER_CREATION_TYPE;
        return OGS_ERROR;
    }
    if (mp->outer_header_creation.len == 0 || !mp->outer_header_creation.data) {
        *cause_value = OGS_PFCP_CAUSE_INVALID_LENGTH;
        *offending_ie_value = OGS_PFCP_OUTER_HEADER_CREATION_TYPE;
        return OGS_ERROR;
    }
    return OGS_OK;
}

/*
 * F-SEID (TS 29.244 cl. 8.2.37): "At least one of V4 and V6 shall be set to "1", and both may be set to "1"." (otherwise
 * "Mandatory IE incorrect", cl. 7.6.8) and the length is the flags, the SEID and each address the flags announce (cl. 7.6.7,
 * "Invalid length"). The address is the part of the key of the session, so it has to be there before the session is added.
 */
static bool _cp_f_seid_valid(const ogs_pfcp_tlv_f_seid_t *ie_, uint8_t *cause_value)
{
    const ogs_pfcp_f_seid_t *f_seid = ie_->data;
    uint32_t want;

    if (!f_seid || ie_->len < 1 + sizeof(uint64_t)) {
        *cause_value = OGS_PFCP_CAUSE_INVALID_LENGTH;
        return false;
    }
    if (!f_seid->ipv4 && !f_seid->ipv6) {
        *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
        return false;
    }
    want = 1 + sizeof(uint64_t) + (f_seid->ipv4 ? OGS_IPV4_LEN : 0) + (f_seid->ipv6 ? OGS_IPV6_LEN : 0);
    if (ie_->len < want) {
        *cause_value = OGS_PFCP_CAUSE_INVALID_LENGTH;
        return false;
    }
    return true;
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
    } else if (!_cp_f_seid_valid(&req->cp_f_seid, &cause_value)) {
        ogs_error("CP F-SEID of the N4mb Session Establishment Request is not valid");
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
    } else if (req->mbs_session_n4mb_control_information.mbs_session_identifier.presence == 0) {
        /* Table 7.5.2.1-4: "MBS Session Identifier", M */
        ogs_error("No MBS Session Identifier in the MBS Session N4mb Control Information");
        cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        offending_ie_value = OGS_PFCP_MBS_SESSION_IDENTIFIER_TYPE;
    } else {
        int i;

        /* Precedence is mandatory in a Create PDR on N4mb. The shared PDR handler cannot require it: its
         * other callers (Sxa) carry no Precedence. */
        for (i = 0; i < OGS_MAX_NUM_OF_PDR && req->create_pdr[i].presence; i++) {
            if (ogs_pfcp_check_sdf_filters(&req->create_pdr[i].pdi) != OGS_OK) {
                ogs_error("Invalid Flow Description in the SDF Filter of a Create PDR of the N4mb Session Establishment Request");
                cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
                offending_ie_value = OGS_PFCP_SDF_FILTER_TYPE;
                break;
            }
            /* Table 7.5.2.2-1, FAR ID: "This IE shall be present if the Activate Predefined Rules IE is not included or if it is
             * included but it does not result in activating a predefined FAR, and if the MAR ID is not included." The
             * Activate Predefined Rules IE and the MAR ID are not applicable to N4mb ("-"): a PDR has its FAR ID. */
            if (req->create_pdr[i].far_id.presence == 0) {
                ogs_error("No FAR ID in Create PDR of N4mb Session Establishment Request");
                cause_value = OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING;
                offending_ie_value = OGS_PFCP_FAR_ID_TYPE;
                break;
            }
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
            if (_check_mbs_multicast_parameters(&req->create_far[i].mbs_multicast_parameters,
                        &cause_value, &offending_ie_value) != OGS_OK) {
                ogs_error("MBS Multicast Parameters of the N4mb Session Establishment Request are not valid");
                break;
            }
            /* Table 7.5.2.3-1, Add MBS Unicast Parameters: "This IE shall be present when the Apply Action is set to "MBSU"." */
            if (req->create_far[i].apply_action.presence &&
                    (req->create_far[i].apply_action.u16 & OGS_PFCP_APPLY_ACTION_MBSU) &&
                    req->create_far[i].add_mbs_unicast_parameters[0].presence == 0) {
                ogs_error("Apply Action MBSU without Add MBS Unicast Parameters");
                cause_value = OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING;
                offending_ie_value = OGS_PFCP_ADD_MBS_UNICAST_PARAMETERS_TYPE;
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
            if (_check_addressing_members(has_dest, has_src, &cause_value, &offending_ie_value) != OGS_OK) {
                ogs_error("IP Multicast Addressing Info of PDR[%d] misses its Source IP Address or IP Multicast Address", i);
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

    /* MBS restoration: the low layer SSM and the C-TEID that the MB-SMF asks for */
    if (cause_value == OGS_PFCP_CAUSE_REQUEST_ACCEPTED &&
            req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.presence &&
            req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.len >= 1 &&
            ((ogs_pfcp_mbsn4mbreq_flags_t *)req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.data)->
                restoration_indication &&
            req->mbs_session_n4mb_control_information.multicast_transport_information_for_n3mb_and_or_n19mb.presence) {
        uint32_t c_teid;
        ogs_ip_t dest, src;

        cause_value = _decode_multicast_transport_information(
                &req->mbs_session_n4mb_control_information.multicast_transport_information_for_n3mb_and_or_n19mb,
                &c_teid, &dest, &src);
        if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
            ogs_error("Multicast Transport Information of the restoration is not valid (cause %u)", cause_value);
            offending_ie_value = OGS_PFCP_MULTICAST_TRANSPORT_INFORMATION_TYPE;
        } else if (!_requested_llssm_available(&dest, &src)) {
            ogs_error("The low layer SSM of the restoration is not available");
            cause_value = OGS_PFCP_CAUSE_RESTORATION_FAILURE_RESOURCE_NOT_AVAILABLE;
            offending_ie_value = OGS_PFCP_MULTICAST_TRANSPORT_INFORMATION_TYPE;
        }
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
    ogs_ssm_t id_ssm;
    int num_of_created_pdr = 0;
    bool resti = false, requested_llssm = false;
    uint32_t requested_c_teid = 0;
    ogs_ip_t requested_dest, requested_src;

    ogs_assert(xact);
    ogs_assert(req);

    ogs_debug("N4mb Session Establishment Request");

    /* MBS RESTI: the PFCP session restores an existing one, with the low layer SSM and the C-TEID the MB-SMF gives
     * (already decoded and checked by upf_n4mb_check_session_establishment_request()) */
    if (req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.presence &&
            req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.len >= 1)
        resti = ((ogs_pfcp_mbsn4mbreq_flags_t *)
                req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.data)->restoration_indication;
    if (resti &&
            req->mbs_session_n4mb_control_information.multicast_transport_information_for_n3mb_and_or_n19mb.presence) {
        ogs_assert(OGS_PFCP_CAUSE_REQUEST_ACCEPTED == _decode_multicast_transport_information(
                &req->mbs_session_n4mb_control_information.multicast_transport_information_for_n3mb_and_or_n19mb,
                &requested_c_teid, &requested_dest, &requested_src));
        requested_llssm = true;
    }

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
        created_pdr[i] = ogs_pfcp_handle_create_pdr(&mbs_sess->pfcp,
                &req->create_pdr[i], NULL,
                &cause_value, &offending_ie_value);
        if (created_pdr[i] == NULL)
            break;
    }
    num_of_created_pdr = i;

    if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED)
        goto cleanup;

    for (i = 0; i < OGS_MAX_NUM_OF_FAR; i++) {
        if (ogs_pfcp_handle_create_far(&mbs_sess->pfcp, &req->create_far[i],
                    &cause_value, &offending_ie_value) == NULL)
            break;
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
        if (ogs_pfcp_handle_create_qer(&mbs_sess->pfcp, &req->create_qer[i],
                    &cause_value, &offending_ie_value) == NULL)
            break;
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
                _allocate_ingress_tunnel(mbs_sess, lit_ie->data, &cause_value, &offending_ie_value) != OGS_OK) {
            /* MBS restoration: the address and port are the ones the MB-SMF gave (CH=0), and the MB-UPF "shall reject
             * the PFCP Session Establishment Request with the cause "PFCP session restoration failure due to
             * requested resource not available"" if it cannot bind them (Table 7.5.2.1-4 NOTE) */
            if (resti && !((const ogs_pfcp_local_ingress_tunnel_t *)lit_ie->data)->choose &&
                    cause_value == OGS_PFCP_CAUSE_SYSTEM_FAILURE)
                cause_value = OGS_PFCP_CAUSE_RESTORATION_FAILURE_RESOURCE_NOT_AVAILABLE;
            goto cleanup;
        }

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
                // Free the old values and initialise to empty
                upf_mbs_session_id_clear(&mbs_sess->mbs_session_id);

                if (mbs_session_identifier.tmgif) {
                    mbs_sess->mbs_session_id.is_tmgi = 1;
                    mbs_sess->mbs_session_id.tmgi = (ogs_tmgi_t*)ogs_calloc(1, sizeof(*mbs_sess->mbs_session_id.tmgi));
                    mbs_sess->mbs_session_id.tmgi->mbs_service_id = ogs_msprintf("%.2X%.2X%.2X", mbs_session_identifier.tmgi.mbs_service_id[0], mbs_session_identifier.tmgi.mbs_service_id[1], mbs_session_identifier.tmgi.mbs_service_id[2]);
                    /* TS 24.008 figure 10.5.154: "bit 8 of octet 3 is the most significant bit and bit 1 of octet 5 the least significant
                     * bit" of the MBMS Service ID: the first octet is the first two digits */
                    ogs_debug("MBS Session Identifier: TMGI MBS Service ID [%s]", mbs_sess->mbs_session_id.tmgi->mbs_service_id);
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
                }
                /* the TMGI and the SSM may be there together (cl. 8.2.206: each has its flag) */
                if (mbs_session_identifier.ssmif) {
                    memset(&id_ssm, 0, sizeof(id_ssm));
                    switch (mbs_session_identifier.ssm.ip_source_address.address_type) {
                    case 0: /* IPv4 */
                        id_ssm.src_ip_addr.ipv4 = 1;
                        id_ssm.src_ip_addr.addr = mbs_session_identifier.ssm.ip_source_address.ipv4_addr;
                        id_ssm.src_ip_addr.len = mbs_session_identifier.ssm.ip_source_address.address_length;
                        break;
                    case 1: /* IPv6 */
                        id_ssm.src_ip_addr.ipv6 = 1;
                        memcpy(id_ssm.src_ip_addr.addr6, mbs_session_identifier.ssm.ip_source_address.ipv6_addr, sizeof(id_ssm.src_ip_addr.addr6));
                        id_ssm.src_ip_addr.len = mbs_session_identifier.ssm.ip_source_address.address_length;
                        break;
                    default:
                        ogs_warn("Unknown address_type %i in SSM source address", mbs_session_identifier.ssm.ip_source_address.address_type);
                        break;
                    }
                    switch (mbs_session_identifier.ssm.ip_multicast_distribution_address.address_type) {
                    case 0: /* IPv4 */
                        id_ssm.dest_ip_addr.ipv4 = 1;
                        id_ssm.dest_ip_addr.addr = mbs_session_identifier.ssm.ip_multicast_distribution_address.ipv4_addr;
                        id_ssm.dest_ip_addr.len = mbs_session_identifier.ssm.ip_multicast_distribution_address.address_length;
                        break;
                    case 1: /* IPv6 */
                        id_ssm.dest_ip_addr.ipv6 = 1;
                        memcpy(id_ssm.dest_ip_addr.addr6, mbs_session_identifier.ssm.ip_multicast_distribution_address.ipv6_addr, sizeof(id_ssm.dest_ip_addr.addr6));
                        id_ssm.dest_ip_addr.len = mbs_session_identifier.ssm.ip_multicast_distribution_address.address_length;
                        break;
                    default:
                        ogs_warn("Unknown address_type %i in SSM destination address", mbs_session_identifier.ssm.ip_multicast_distribution_address.address_type);
                        break;
                    }
                    /* The TMGI and the SSM share the field of the identifier (a union): with a TMGI the SSM is the session's, below */
                    if (!mbs_session_identifier.tmgif) {
                        mbs_sess->mbs_session_id.is_ssm = 1;
                        mbs_sess->mbs_session_id.ssm = (ogs_ssm_t *)ogs_calloc(1, sizeof(*mbs_sess->mbs_session_id.ssm));
                        ogs_assert(mbs_sess->mbs_session_id.ssm);
                        *mbs_sess->mbs_session_id.ssm = id_ssm;
                    }
                }
                if (!mbs_sess->ssm.dest_ip_addr.ipv4 && !mbs_sess->ssm.dest_ip_addr.ipv6 && mbs_session_identifier.ssmif) {
                    /* Duplicate to ID SSM to SSM settings if we don't already have an SSM destination */
                    memcpy(&mbs_sess->ssm, &id_ssm, sizeof(mbs_sess->ssm));
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
    if (requested_llssm) {
        /* The low layer SSM and the C-TEID of the session before the restart, not new ones (cl. 5.34.2.1A NOTE 1) */
        mbs_sess->ll_ssm = ogs_pfcp_llssm_add_requested(&requested_dest, &requested_src);
        if (!mbs_sess->ll_ssm) {
            ogs_error("The requested low layer SSM is not available");
            cause_value = OGS_PFCP_CAUSE_RESTORATION_FAILURE_RESOURCE_NOT_AVAILABLE;
            offending_ie_value = OGS_PFCP_MULTICAST_TRANSPORT_INFORMATION_TYPE;
            goto cleanup;
        }
        mbs_sess->c_teid = requested_c_teid;
    } else if (mbs_sess->mbs_flags.provide_lower_layer_ssm) {
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
        if (_setup_far_node(mbs_sess, far) != OGS_OK) {
            /* "Rule creation/modification Failure: This cause shall be used by the UP function if a received Rule failed to be stored and be applied in the UP function." */
            cause_value = OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE;
            offending_ie_value = OGS_PFCP_FAR_ID_TYPE;
            goto cleanup;
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
    upf_gtpu_path_sync();

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
 * The IEs of a PFCP Session Modification Request that apply to N4mb (TS 29.244 Table 7.5.4.1-1, N4mb column) and that
 * the MB-UPF does not process (it processes the flow rules, Create and Remove PDR, FAR and QER, and Update FAR). They
 * are rejected, not skipped: ignoring a rule the CP function believes installed
 * would leave the two out of step. Returns true with the IE type of the first one found.
 */
static bool _modification_ie_not_supported(const ogs_pfcp_session_modification_request_t *req, uint16_t *type)
{
    static const struct { size_t offset; uint16_t type; } ies[] = {
        { offsetof(ogs_pfcp_session_modification_request_t, cp_f_seid), OGS_PFCP_F_SEID_TYPE },
        { offsetof(ogs_pfcp_session_modification_request_t, remove_urr), OGS_PFCP_REMOVE_URR_TYPE },
        { offsetof(ogs_pfcp_session_modification_request_t, remove_traffic_endpoint),
            OGS_PFCP_REMOVE_TRAFFIC_ENDPOINT_TYPE },
        { offsetof(ogs_pfcp_session_modification_request_t, create_urr), OGS_PFCP_CREATE_URR_TYPE },
        { offsetof(ogs_pfcp_session_modification_request_t, create_traffic_endpoint),
            OGS_PFCP_CREATE_TRAFFIC_ENDPOINT_TYPE },
        { offsetof(ogs_pfcp_session_modification_request_t, update_urr), OGS_PFCP_UPDATE_URR_TYPE },
        { offsetof(ogs_pfcp_session_modification_request_t, update_traffic_endpoint),
            OGS_PFCP_UPDATE_TRAFFIC_ENDPOINT_TYPE },
        { offsetof(ogs_pfcp_session_modification_request_t, query_urr), OGS_PFCP_QUERY_URR_TYPE },
        { offsetof(ogs_pfcp_session_modification_request_t, user_plane_inactivity_timer),
            OGS_PFCP_USER_PLANE_INACTIVITY_TIMER_TYPE },
    };
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(ies); i++) {
        /* every one of these starts with the presence word (ogs_tlv_presence_t), the first of an array element too */
        if (*(const ogs_tlv_presence_t *)((const char *)req + ies[i].offset)) {
            *type = ies[i].type;
            return true;
        }
    }
    return false;
}

static bool _ip_equal(const ogs_ip_t *a, const ogs_ip_t *b)
{
    if (a->ipv4 != b->ipv4 || a->ipv6 != b->ipv6)
        return false;
    if (a->ipv4 && a->addr != b->addr)
        return false;
    return !a->ipv6 || memcmp(a->addr6, b->addr6, OGS_IPV6_LEN) == 0;
}

/* The Remove PDR, FAR or QER IEs of a request name this identifier. */
static bool _removes_pdr(const ogs_pfcp_session_modification_request_t *req, uint16_t id)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->remove_pdr) && req->remove_pdr[i].presence; i++)
        if (req->remove_pdr[i].pdr_id.presence && req->remove_pdr[i].pdr_id.u16 == id)
            return true;
    return false;
}

static bool _removes_far(const ogs_pfcp_session_modification_request_t *req, uint32_t id)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->remove_far) && req->remove_far[i].presence; i++)
        if (req->remove_far[i].far_id.presence && req->remove_far[i].far_id.u32 == id)
            return true;
    return false;
}

static bool _removes_qer(const ogs_pfcp_session_modification_request_t *req, uint32_t id)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->remove_qer) && req->remove_qer[i].presence; i++)
        if (req->remove_qer[i].qer_id.presence && req->remove_qer[i].qer_id.u32 == id)
            return true;
    return false;
}

static bool _creates_far(const ogs_pfcp_session_modification_request_t *req, uint32_t id)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->create_far) && req->create_far[i].presence; i++)
        if (req->create_far[i].far_id.presence && req->create_far[i].far_id.u32 == id)
            return true;
    return false;
}

static bool _creates_qer(const ogs_pfcp_session_modification_request_t *req, uint32_t id)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->create_qer) && req->create_qer[i].presence; i++)
        if (req->create_qer[i].qer_id.presence && req->create_qer[i].qer_id.u32 == id)
            return true;
    return false;
}

/* The Update PDR of a request that names this PDR, or NULL. */
static const ogs_pfcp_tlv_update_pdr_t *_updates_pdr(const ogs_pfcp_session_modification_request_t *req, uint16_t id)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->update_pdr) && req->update_pdr[i].presence; i++)
        if (req->update_pdr[i].pdr_id.presence && req->update_pdr[i].pdr_id.u16 == id)
            return &req->update_pdr[i];
    return NULL;
}

/*
 * The PDI of a flow that the request creates, or of an Update PDR that replaces the PDI of a flow. The session has one
 * ingress tunnel, one group and one source (Table 7.5.2.7-1 and cl. 5.34.2.2: "a single ingress tunnel address,
 * regardless of the number of MBS QoS flows"): a flow refers to the tunnel by the Traffic Endpoint ID, and carries
 * the group and the source of the session if it carries any. Returns OGS_OK, or OGS_ERROR with the cause and the
 * Offending IE.
 */
static int _check_flow_pdi(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_pdi_t *pdi,
        uint8_t *cause_value, uint16_t *offending_ie_value)
{
    bool has_dest, has_src;
    ogs_pfcp_decoded_ip_multicast_address_t dest;
    ogs_pfcp_decoded_source_ip_address_t src;

#define FAIL(c, t) do { *cause_value = (c); *offending_ie_value = (t); return OGS_ERROR; } while (0)

    if (ogs_pfcp_check_sdf_filters(pdi) != OGS_OK)
        FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_SDF_FILTER_TYPE);
    if (pdi->local_ingress_tunnel.presence)
        FAIL(OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED, OGS_PFCP_LOCAL_INGRESS_TUNNEL_TYPE);
    /* the PDR handlers refuse an F-TEID with neither the V4 nor the V6 flag (the first octet, bits 1 and 2) */
    if (pdi->local_f_teid.presence &&
            (pdi->local_f_teid.len < 1 || (*(const uint8_t *)pdi->local_f_teid.data & 0x03) == 0))
        FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_F_TEID_TYPE);
    if (pdi->traffic_endpoint_id.presence &&
            (pdi->traffic_endpoint_id.len < 1 || !mbs_sess->traffic_endpoint_id_presence ||
             *(const uint8_t *)pdi->traffic_endpoint_id.data != mbs_sess->traffic_endpoint_id))
        FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_TRAFFIC_ENDPOINT_ID_TYPE);
    if (_decode_addressing_info(&pdi->ip_multicast_addressing_info, &has_dest, &dest, &has_src, &src,
                offending_ie_value) != OGS_OK)
        FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, *offending_ie_value);
    if (_check_addressing_members(has_dest, has_src, cause_value, offending_ie_value) != OGS_OK)
        return OGS_ERROR;
    if (has_dest && (dest.any || dest.range || !_ip_equal(&dest.start, &mbs_sess->ssm.dest_ip_addr)))
        FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_IP_MULTICAST_ADDRESS_TYPE);
    if (has_src && !_ip_equal(&src.addr, &mbs_sess->ssm.src_ip_addr))
        FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_SOURCE_IP_ADDRESS_TYPE);

#undef FAIL
    return OGS_OK;
}

/*
 * Checks the Remove and Create PDR, FAR and QER of an N4mb Session Modification Request against the session, without
 * changing it (the flows of an MBS session that came and went, TS 29.244 cl. 5.34.2.2: "for each MBS QoS flow: a
 * Create PDR IE ..., a Create QER IE ..., a Create FAR IE ..."). The Create IEs are those of the establishment
 * (cl. 7.5.4.17: "The encoding of these IEs are specified within PFCP Session Establishment Request message"), so the
 * checks are those of the establishment, and a rule is new: one that exists is not replaced.
 */
static int _check_flow_changes(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_session_modification_request_t *req,
        uint8_t *cause_value, uint16_t *offending_ie_value)
{
    ogs_pfcp_pdr_t *pdr;
    size_t i, j;

#define FAIL(c, t) do { *cause_value = (c); *offending_ie_value = (t); return OGS_ERROR; } while (0)

    for (i = 0; i < OGS_ARRAY_SIZE(req->remove_pdr) && req->remove_pdr[i].presence; i++) {
        if (!req->remove_pdr[i].pdr_id.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_PDR_ID_TYPE);
        if (!ogs_pfcp_pdr_find(&mbs_sess->pfcp, req->remove_pdr[i].pdr_id.u16))
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_PDR_ID_TYPE);
        for (j = 0; j < i; j++)
            if (req->remove_pdr[j].pdr_id.u16 == req->remove_pdr[i].pdr_id.u16)
                FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_PDR_ID_TYPE);
    }
    for (i = 0; i < OGS_ARRAY_SIZE(req->remove_far) && req->remove_far[i].presence; i++) {
        if (!req->remove_far[i].far_id.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_FAR_ID_TYPE);
        if (!ogs_pfcp_far_find(&mbs_sess->pfcp, req->remove_far[i].far_id.u32))
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_FAR_ID_TYPE);
        for (j = 0; j < i; j++)
            if (req->remove_far[j].far_id.u32 == req->remove_far[i].far_id.u32)
                FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_FAR_ID_TYPE);
    }
    for (i = 0; i < OGS_ARRAY_SIZE(req->remove_qer) && req->remove_qer[i].presence; i++) {
        if (!req->remove_qer[i].qer_id.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_QER_ID_TYPE);
        if (!ogs_pfcp_qer_find(&mbs_sess->pfcp, req->remove_qer[i].qer_id.u32))
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_QER_ID_TYPE);
        for (j = 0; j < i; j++)
            if (req->remove_qer[j].qer_id.u32 == req->remove_qer[i].qer_id.u32)
                FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_QER_ID_TYPE);
    }

    /* Update PDR (Table 7.5.4.2-1): PDR ID, M; Outer Header Removal, Precedence, PDI, FAR ID and QER ID, each "present if
     * it needs to be changed". The PDI "shall replace the PDI previously stored", so it has what the PDI of a Create PDR
     * has (Source Interface, M). The other IEs of the table are not applicable to N4mb ("-") and are not read. */
    for (i = 0; i < OGS_ARRAY_SIZE(req->update_pdr) && req->update_pdr[i].presence; i++) {
        const ogs_pfcp_tlv_update_pdr_t *u = &req->update_pdr[i];

        if (!u->pdr_id.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_PDR_ID_TYPE);
        if (!ogs_pfcp_pdr_find(&mbs_sess->pfcp, u->pdr_id.u16) || _removes_pdr(req, u->pdr_id.u16))
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_PDR_ID_TYPE);
        for (j = 0; j < i; j++)
            if (req->update_pdr[j].pdr_id.u16 == u->pdr_id.u16)
                FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_PDR_ID_TYPE);
        if (u->far_id.presence &&
                !(ogs_pfcp_far_find(&mbs_sess->pfcp, u->far_id.u32) && !_removes_far(req, u->far_id.u32)) &&
                !_creates_far(req, u->far_id.u32))
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_FAR_ID_TYPE);
        if (u->qer_id.presence &&
                !(ogs_pfcp_qer_find(&mbs_sess->pfcp, u->qer_id.u32) && !_removes_qer(req, u->qer_id.u32)) &&
                !_creates_qer(req, u->qer_id.u32))
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_QER_ID_TYPE);
        if (u->pdi.presence) {
            if (!u->pdi.source_interface.presence)
                FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_SOURCE_INTERFACE_TYPE);
            if (_check_flow_pdi(mbs_sess, &u->pdi, cause_value, offending_ie_value) != OGS_OK)
                return OGS_ERROR;
        }
    }

    /* Update QER (Table 7.5.4.5-1): QER ID, M; Gate Status, Maximum Bitrate, Guaranteed Bitrate and QFI are "X" for N4mb,
     * the others are "-" and are not read. */
    for (i = 0; i < OGS_ARRAY_SIZE(req->update_qer) && req->update_qer[i].presence; i++) {
        const ogs_pfcp_tlv_update_qer_t *u = &req->update_qer[i];

        if (!u->qer_id.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_QER_ID_TYPE);
        if (!ogs_pfcp_qer_find(&mbs_sess->pfcp, u->qer_id.u32) || _removes_qer(req, u->qer_id.u32))
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_QER_ID_TYPE);
        for (j = 0; j < i; j++)
            if (req->update_qer[j].qer_id.u32 == u->qer_id.u32)
                FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_QER_ID_TYPE);
    }

    /* A FAR or a QER that is removed is not used by a PDR that stays (after its Update PDR) */
    ogs_list_for_each(&mbs_sess->pfcp.pdr_list, pdr) {
        const ogs_pfcp_tlv_update_pdr_t *u;

        if (_removes_pdr(req, pdr->id))
            continue;
        u = _updates_pdr(req, pdr->id);
        if (!(u && u->far_id.presence) && pdr->far && _removes_far(req, pdr->far->id))
            FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_FAR_ID_TYPE);
        if (!(u && u->qer_id.presence) && pdr->qer && _removes_qer(req, pdr->qer->id))
            FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_QER_ID_TYPE);
    }

    for (i = 0; i < OGS_ARRAY_SIZE(req->create_pdr) && req->create_pdr[i].presence; i++) {
        const ogs_pfcp_tlv_create_pdr_t *c = &req->create_pdr[i];

        if (!c->pdr_id.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_PDR_ID_TYPE);
        if (ogs_pfcp_pdr_find(&mbs_sess->pfcp, c->pdr_id.u16))
            FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_PDR_ID_TYPE);
        for (j = 0; j < i; j++)
            if (req->create_pdr[j].pdr_id.u16 == c->pdr_id.u16)
                FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_PDR_ID_TYPE);
        /* Precedence is mandatory in a Create PDR on N4mb (as at the establishment) */
        if (!c->precedence.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_PRECEDENCE_TYPE);
        if (!c->pdi.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_PDI_TYPE);
        if (!c->pdi.source_interface.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_SOURCE_INTERFACE_TYPE);
        /* the data path needs the FAR; it is one that stays, or one this request creates */
        if (!c->far_id.presence)
            FAIL(OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING, OGS_PFCP_FAR_ID_TYPE);
        if (!(ogs_pfcp_far_find(&mbs_sess->pfcp, c->far_id.u32) && !_removes_far(req, c->far_id.u32)) &&
                !_creates_far(req, c->far_id.u32))
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_FAR_ID_TYPE);
        if (c->qer_id.presence &&
                !(ogs_pfcp_qer_find(&mbs_sess->pfcp, c->qer_id.u32) && !_removes_qer(req, c->qer_id.u32)) &&
                !_creates_qer(req, c->qer_id.u32))
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT, OGS_PFCP_QER_ID_TYPE);
        if (_check_flow_pdi(mbs_sess, &c->pdi, cause_value, offending_ie_value) != OGS_OK)
            return OGS_ERROR;
    }

    for (i = 0; i < OGS_ARRAY_SIZE(req->create_far) && req->create_far[i].presence; i++) {
        const ogs_pfcp_tlv_create_far_t *c = &req->create_far[i];

        if (!c->far_id.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_FAR_ID_TYPE);
        if (ogs_pfcp_far_find(&mbs_sess->pfcp, c->far_id.u32))
            FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_FAR_ID_TYPE);
        for (j = 0; j < i; j++)
            if (req->create_far[j].far_id.u32 == c->far_id.u32)
                FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_FAR_ID_TYPE);
        if (!c->apply_action.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_APPLY_ACTION_TYPE);
        if ((c->apply_action.u16 & OGS_PFCP_APPLY_ACTION_FSSM) && !c->mbs_multicast_parameters.presence)
            FAIL(OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING, OGS_PFCP_MBS_MULTICAST_PARAMETERS_TYPE);
        if (_check_mbs_multicast_parameters(&c->mbs_multicast_parameters, cause_value, offending_ie_value) != OGS_OK)
            return OGS_ERROR;
        if ((c->apply_action.u16 & OGS_PFCP_APPLY_ACTION_MBSU) && !c->add_mbs_unicast_parameters[0].presence)
            FAIL(OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING, OGS_PFCP_ADD_MBS_UNICAST_PARAMETERS_TYPE);
    }

    for (i = 0; i < OGS_ARRAY_SIZE(req->create_qer) && req->create_qer[i].presence; i++) {
        const ogs_pfcp_tlv_create_qer_t *c = &req->create_qer[i];

        if (!c->qer_id.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_QER_ID_TYPE);
        if (ogs_pfcp_qer_find(&mbs_sess->pfcp, c->qer_id.u32))
            FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_QER_ID_TYPE);
        for (j = 0; j < i; j++)
            if (req->create_qer[j].qer_id.u32 == c->qer_id.u32)
                FAIL(OGS_PFCP_CAUSE_RULE_CREATION_MODIFICATION_FAILURE, OGS_PFCP_QER_ID_TYPE);
        if (!c->gate_status.presence)
            FAIL(OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, OGS_PFCP_GATE_STATUS_TYPE);
    }

#undef FAIL
    return OGS_OK;
}

/* Creates the rules of the Create PDR, FAR and QER IEs. On failure the cause is set and what was created so far is
 * still in the session: _flows_rollback() takes it out. */
static int _create_flows(upf_mbs_sess_t *mbs_sess, ogs_pfcp_session_modification_request_t *req,
        uint8_t *cause_value, uint16_t *offending_ie_value)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->create_pdr) && req->create_pdr[i].presence; i++) {
        if (!ogs_pfcp_handle_create_pdr(&mbs_sess->pfcp, &req->create_pdr[i], NULL, cause_value, offending_ie_value))
            return OGS_ERROR;
    }
    for (i = 0; i < OGS_ARRAY_SIZE(req->create_far) && req->create_far[i].presence; i++) {
        ogs_pfcp_far_t *far;

        /* The handler of the Create FAR finds the FAR that a Create PDR made; a FAR that no Create PDR of the request
         * names (an Update PDR may name it, Table 7.5.4.2-1) is made here. _check_flow_changes() saw it did not exist. */
        if (!ogs_pfcp_far_find_or_add(&mbs_sess->pfcp, req->create_far[i].far_id.u32)) {
            *cause_value = OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE;
            *offending_ie_value = OGS_PFCP_CREATE_FAR_TYPE;
            return OGS_ERROR;
        }
        far = ogs_pfcp_handle_create_far(&mbs_sess->pfcp, &req->create_far[i], cause_value, offending_ie_value);

        if (!far)
            return OGS_ERROR;
        if (mbs_sess->ll_ssm && _setup_far_node(mbs_sess, far) != OGS_OK) {
            *cause_value = OGS_PFCP_CAUSE_SYSTEM_FAILURE;
            *offending_ie_value = OGS_PFCP_CREATE_FAR_TYPE;
            return OGS_ERROR;
        }
    }
    for (i = 0; i < OGS_ARRAY_SIZE(req->create_qer) && req->create_qer[i].presence; i++) {
        if (!ogs_pfcp_qer_find_or_add(&mbs_sess->pfcp, req->create_qer[i].qer_id.u32)) {
            *cause_value = OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE;
            *offending_ie_value = OGS_PFCP_CREATE_QER_TYPE;
            return OGS_ERROR;
        }
        if (!ogs_pfcp_handle_create_qer(&mbs_sess->pfcp, &req->create_qer[i], cause_value, offending_ie_value))
            return OGS_ERROR;
    }
    return OGS_OK;
}

/* Takes out the rules the request creates: _check_flow_changes() saw that none of them existed. */
static void _flows_rollback(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_session_modification_request_t *req)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->create_pdr) && req->create_pdr[i].presence; i++) {
        ogs_pfcp_pdr_t *pdr = ogs_pfcp_pdr_find(&mbs_sess->pfcp, req->create_pdr[i].pdr_id.u16);

        if (pdr)
            ogs_pfcp_pdr_remove(pdr);
    }
    for (i = 0; i < OGS_ARRAY_SIZE(req->create_far) && req->create_far[i].presence; i++) {
        ogs_pfcp_far_t *far = ogs_pfcp_far_find(&mbs_sess->pfcp, req->create_far[i].far_id.u32);

        if (far)
            ogs_pfcp_far_remove(far);
    }
    for (i = 0; i < OGS_ARRAY_SIZE(req->create_qer) && req->create_qer[i].presence; i++) {
        ogs_pfcp_qer_t *qer = ogs_pfcp_qer_find(&mbs_sess->pfcp, req->create_qer[i].qer_id.u32);

        if (qer)
            ogs_pfcp_qer_remove(qer);
    }
}

/*
 * The Update PDR of a request that has been checked: the PDI replaces the PDI (the SDF filters, the Network Instance and
 * the QFI of a PDI that does not carry them are gone), and the Precedence, FAR and QER are replaced. The Outer Header
 * Removal is stored as at the establishment.
 */
static void _update_pdrs(upf_mbs_sess_t *mbs_sess, ogs_pfcp_session_modification_request_t *req)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->update_pdr) && req->update_pdr[i].presence; i++) {
        ogs_pfcp_tlv_update_pdr_t *u = &req->update_pdr[i];
        ogs_pfcp_pdr_t *pdr = ogs_pfcp_pdr_find(&mbs_sess->pfcp, u->pdr_id.u16);
        uint8_t cause_value = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
        uint16_t offending_ie_value = 0;

        ogs_assert(pdr);

        if (u->pdi.presence) {
            if (pdr->dnn) {
                ogs_free(pdr->dnn);
                pdr->dnn = NULL;
            }
            pdr->qfi = 0;
            /* checked: the Flow Descriptions compile, the Source Interface is there */
            ogs_assert(ogs_pfcp_handle_update_pdr(&mbs_sess->pfcp, u, &cause_value, &offending_ie_value));
        }
        if (u->precedence.presence)
            ogs_pfcp_pdr_reorder_by_precedence(pdr, u->precedence.u32);
        if (u->far_id.presence)
            ogs_pfcp_pdr_associate_far(pdr, ogs_pfcp_far_find(&mbs_sess->pfcp, u->far_id.u32));
        if (u->qer_id.presence)
            ogs_pfcp_pdr_associate_qer(pdr, ogs_pfcp_qer_find(&mbs_sess->pfcp, u->qer_id.u32));
        if (u->outer_header_removal.presence) {
            memset(&pdr->outer_header_removal, 0, sizeof(pdr->outer_header_removal));
            pdr->outer_header_removal_len = ogs_min(u->outer_header_removal.len, sizeof(pdr->outer_header_removal));
            memcpy(&pdr->outer_header_removal, u->outer_header_removal.data, pdr->outer_header_removal_len);
        }
    }
}

/*
 * The packets that the FAR buffered when its Apply Action stops being BUFF. TS 29.244 cl. 5.2.4.2: "The UP function shall
 * stop applying buffering when new instruction is received from the CP function. The buffered packets shall be either
 * dropped or forwarded following the packet forwarding model specified in clause 5.2.1 and taking into consideration
 * that the buffered Packets have been already processed earlier." The PDR that matched a packet is the one that held it
 * when it came: with FSSM or MBSU the packet is forwarded by it, with DROP it is discarded (as it is when it arrives).
 */
static void _release_buffered_packets(upf_mbs_sess_t *mbs_sess, ogs_pfcp_far_t *far)
{
    uint32_t i;

    for (i = 0; i < far->num_of_buffered_packet; i++) {
        ogs_pkbuf_t *buf = far->buffered_packet[i];
        ogs_pfcp_pdr_t *pdr = ogs_pfcp_pdr_find(&mbs_sess->pfcp, far->buffered_packet_pdr_id[i]);

        if (pdr && pdr->far == far && (far->apply_action & (OGS_PFCP_APPLY_ACTION_FSSM | OGS_PFCP_APPLY_ACTION_MBSU))) {
            ogs_gtp2_header_desc_t sendhdr;

            memset(&sendhdr, 0, sizeof(sendhdr));
            sendhdr.type = OGS_GTPU_MSGTYPE_GPDU;
            ogs_pfcp_send_g_pdu_mbs(pdr, &sendhdr, buf);
        } else {
            ogs_pkbuf_free(buf);
        }
        far->buffered_packet[i] = NULL;
    }
    far->num_of_buffered_packet = 0;
}

/* The Update QER of a request that has been checked. */
static void _update_qers(upf_mbs_sess_t *mbs_sess, ogs_pfcp_session_modification_request_t *req)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->update_qer) && req->update_qer[i].presence; i++) {
        uint8_t cause_value = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
        uint16_t offending_ie_value = 0;

        /* checked: the QER exists */
        ogs_assert(ogs_pfcp_handle_update_qer(&mbs_sess->pfcp, &req->update_qer[i], &cause_value, &offending_ie_value));
    }
}

/* The Remove PDR, FAR and QER of a request that has been checked. */
static void _remove_flows(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_session_modification_request_t *req)
{
    size_t i;

    for (i = 0; i < OGS_ARRAY_SIZE(req->remove_pdr) && req->remove_pdr[i].presence; i++)
        ogs_pfcp_pdr_remove(ogs_pfcp_pdr_find(&mbs_sess->pfcp, req->remove_pdr[i].pdr_id.u16));
    for (i = 0; i < OGS_ARRAY_SIZE(req->remove_far) && req->remove_far[i].presence; i++)
        ogs_pfcp_far_remove(ogs_pfcp_far_find(&mbs_sess->pfcp, req->remove_far[i].far_id.u32));
    for (i = 0; i < OGS_ARRAY_SIZE(req->remove_qer) && req->remove_qer[i].presence; i++)
        ogs_pfcp_qer_remove(ogs_pfcp_qer_find(&mbs_sess->pfcp, req->remove_qer[i].qer_id.u32));
}

/*
 * The Update FAR of an N4mb Session Modification Request names an existing FAR of the session (TS 29.244
 * Table 7.5.4.3-1: FAR ID, M).
 */
static ogs_pfcp_far_t *_find_update_far(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_update_far_t *uf,
        uint8_t *cause_value, uint16_t *offending_ie_value)
{
    ogs_pfcp_far_t *far;

    if (uf->far_id.presence == 0) {
        ogs_error("No FAR ID in an Update FAR");
        *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
        *offending_ie_value = OGS_PFCP_FAR_ID_TYPE;
        return NULL;
    }
    far = ogs_pfcp_far_find(&mbs_sess->pfcp, uf->far_id.u32);
    if (!far) {
        ogs_error("Update FAR names the unknown FAR ID %u", uf->far_id.u32);
        *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
        *offending_ie_value = OGS_PFCP_FAR_ID_TYPE;
        return NULL;
    }
    return far;
}

/* Checks the Apply Action of an Update FAR against the FAR as the request leaves it. */
static int _check_apply_action(upf_mbs_sess_t *mbs_sess, const ogs_pfcp_tlv_update_far_t *uf, ogs_pfcp_far_t *far,
        uint8_t *cause_value, uint16_t *offending_ie_value)
{
    if (uf->apply_action.presence && (uf->apply_action.u16 & OGS_PFCP_APPLY_ACTION_FSSM)) {
        /* TS 29.244 cl. 5.34.2.1A: FSSM makes the MB-UPF "forward the MBS session data using the LLSSM and the
         * C-TEID, which the MB-UPF has earlier assigned to the MBS session"; the destination interface, the outer
         * header and the marking are those of the MBS Multicast Parameters of the Create FAR, which an Update FAR
         * has no IE for. */
        if (!far->mbs_multicast_parameters) {
            ogs_error("FSSM for FAR %u, created without MBS Multicast Parameters", far->id);
            *cause_value = OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING;
            *offending_ie_value = OGS_PFCP_MBS_MULTICAST_PARAMETERS_TYPE;
            return OGS_ERROR;
        }
        if (!mbs_sess->ll_ssm || !far->gnode) {
            ogs_error("FSSM for FAR %u, the session has no low layer SSM", far->id);
            *cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
            *offending_ie_value = OGS_PFCP_APPLY_ACTION_TYPE;
            return OGS_ERROR;
        }
    }

    if (uf->apply_action.presence && (uf->apply_action.u16 & OGS_PFCP_APPLY_ACTION_MBSU) &&
            ogs_list_empty(&far->mbs_unicast_list)) {
        /* Add MBS Unicast Parameters: "This IE shall be present when the Apply Action is set to "MBSU" ... if no
         * remote GTP-U peers are provisioned yet in the MB-UPF" (Table 7.5.4.3-1) */
        ogs_error("MBSU for FAR %u, no remote GTP-U peer is provisioned", far->id);
        *cause_value = OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING;
        *offending_ie_value = OGS_PFCP_ADD_MBS_UNICAST_PARAMETERS_TYPE;
        return OGS_ERROR;
    }

    return OGS_OK;
}

#define MAX_UPDATE_FAR_ADDS (OGS_MAX_NUM_OF_FAR * 8)

/*
 * The peers a request added to and removed from the FARs, kept until the whole request is known to be valid: a
 * removed peer is only unlinked, so that a request that fails can put the FARs back as they were.
 */
typedef struct {
    ogs_list_t removed;     /* ogs_pfcp_mbs_unicast_t, unlinked from their FAR */
    ogs_pfcp_mbs_unicast_t *added[MAX_UPDATE_FAR_ADDS];
    int num_of_added;
} mbs_unicast_journal_t;

static void _journal_rollback(mbs_unicast_journal_t *j)
{
    ogs_pfcp_mbs_unicast_t *mu, *next;
    int i;

    ogs_list_for_each_safe(&j->removed, next, mu) {
        ogs_list_remove(&j->removed, mu);
        ogs_list_add(&mu->far->mbs_unicast_list, mu);
    }
    for (i = 0; i < j->num_of_added; i++)
        ogs_pfcp_mbs_unicast_remove(j->added[i]);
    j->num_of_added = 0;
}

static void _journal_commit(mbs_unicast_journal_t *j)
{
    ogs_pfcp_mbs_unicast_t *mu, *next;

    ogs_list_for_each_safe(&j->removed, next, mu) {
        ogs_list_remove(&j->removed, mu);
        ogs_pfcp_mbs_unicast_free(mu);
    }
    j->num_of_added = 0;
}

/* The Remove and Add MBS Unicast Parameters of one Update FAR (removals first). */
static int _update_far_unicast(ogs_pfcp_far_t *far, ogs_pfcp_tlv_update_far_t *uf, mbs_unicast_journal_t *j,
        uint8_t *cause_value, uint16_t *offending_ie_value)
{
    size_t i;
    ogs_pfcp_mbs_unicast_t *mu;

    for (i = 0; i < OGS_ARRAY_SIZE(uf->remove_mbs_unicast_parameters) &&
            uf->remove_mbs_unicast_parameters[i].presence; i++) {
        mu = ogs_pfcp_handle_remove_mbs_unicast_parameters(far, &uf->remove_mbs_unicast_parameters[i],
                cause_value, offending_ie_value);
        if (!mu)
            return OGS_ERROR;
        ogs_list_remove(&far->mbs_unicast_list, mu);
        ogs_list_add(&j->removed, mu);
    }
    for (i = 0; i < OGS_ARRAY_SIZE(uf->add_mbs_unicast_parameters) &&
            uf->add_mbs_unicast_parameters[i].presence; i++) {
        mu = ogs_pfcp_handle_add_mbs_unicast_parameters(far, &uf->add_mbs_unicast_parameters[i],
                cause_value, offending_ie_value);
        if (!mu)
            return OGS_ERROR;
        ogs_assert(j->num_of_added < MAX_UPDATE_FAR_ADDS);
        j->added[j->num_of_added++] = mu;
    }
    return OGS_OK;
}

/*
 * N4mb Session Modification Request (TS 29.244 cl. 7.5.4, N4mb column of Table 7.5.4.1-1): the Apply Action and the
 * remote GTP-U peers of the FARs of the MBS session. The request is applied as a whole or not at all.
 */
void upf_n4mb_handle_session_modification_request(
        upf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_modification_request_t *req)
{
    int i;
    uint8_t cause_value = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
    uint16_t offending_ie_value = 0;
    mbs_unicast_journal_t journal;
    ogs_pfcp_far_t *far;
    bool delete_all_f_teids = false;

    ogs_assert(xact);
    ogs_assert(req);

    ogs_debug("N4mb Session Modification Request");

    if (!mbs_sess) {
        ogs_error("No MBS Session Context");
        ogs_pfcp_send_error_message(xact, 0,
                OGS_PFCP_SESSION_MODIFICATION_RESPONSE_TYPE,
                OGS_PFCP_CAUSE_SESSION_CONTEXT_NOT_FOUND, 0);
        return;
    }

    memset(&journal, 0, sizeof(journal));

    if (_modification_ie_not_supported(req, &offending_ie_value)) {
        ogs_error("IE type %u of the N4mb Session Modification Request is not supported", offending_ie_value);
        cause_value = OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED;
        goto reject;
    }

    /* PFCPSMReq-Flags (TS 29.244 Table 7.5.4.1-1): of its flags, DETEID is the one of the MB-UPF's MBS procedures
     * (cl. 5.34.2.4); the others ask for what this handler does not do (buffered packets, usage reports) */
    if (req->pfcpsmreq_flags.presence) {
        ogs_pfcp_smreq_flags_t flags;

        flags.value = req->pfcpsmreq_flags.u8;
        if (flags.value & ~(uint8_t)(1 << 5)) {   /* bit 6, DETEID, is the only one supported */
            ogs_error("PFCPSMReq-Flags 0x%02x: only DETEID is supported", flags.value);
            cause_value = OGS_PFCP_CAUSE_SERVICE_NOT_SUPPORTED;
            offending_ie_value = OGS_PFCP_PFCPSMREQ_FLAGS_TYPE;
            goto reject;
        }
        delete_all_f_teids = flags.delete_all_dl_n3mb_and_or_n19mb_f_teids;
    }

    if (_check_flow_changes(mbs_sess, req, &cause_value, &offending_ie_value) != OGS_OK)
        goto reject;

    for (i = 0; i < OGS_MAX_NUM_OF_FAR && req->update_far[i].presence; i++) {
        if (!_find_update_far(mbs_sess, &req->update_far[i], &cause_value, &offending_ie_value))
            goto reject;
    }

    /* DETEID: "the MB-UPF shall delete all NG-RAN N3mb DL F-TEIDs and all UPF N19mb DL F-TEIDs which were provisioned in
     * Add MBS Unicast Parameters IEs for the MBS session" (cl. 8.2.31); before the additions of this request */
    if (delete_all_f_teids) {
        ogs_list_for_each(&mbs_sess->pfcp.far_list, far) {
            ogs_pfcp_mbs_unicast_t *mu, *next;

            ogs_list_for_each_safe(&far->mbs_unicast_list, next, mu) {
                ogs_list_remove(&far->mbs_unicast_list, mu);
                ogs_list_add(&journal.removed, mu);
            }
        }
    }

    for (i = 0; i < OGS_MAX_NUM_OF_FAR && req->update_far[i].presence; i++) {
        far = ogs_pfcp_far_find(&mbs_sess->pfcp, req->update_far[i].far_id.u32);
        ogs_assert(far);
        if (_update_far_unicast(far, &req->update_far[i], &journal, &cause_value, &offending_ie_value) != OGS_OK)
            goto rollback;
    }

    if (_create_flows(mbs_sess, req, &cause_value, &offending_ie_value) != OGS_OK) {
        _flows_rollback(mbs_sess, req);
        goto rollback;
    }

    for (i = 0; i < OGS_MAX_NUM_OF_FAR && req->update_far[i].presence; i++) {
        far = ogs_pfcp_far_find(&mbs_sess->pfcp, req->update_far[i].far_id.u32);
        ogs_assert(far);
        if (_check_apply_action(mbs_sess, &req->update_far[i], far, &cause_value, &offending_ie_value) != OGS_OK)
            goto rollback_flows;
    }

    _journal_commit(&journal);

    for (i = 0; i < OGS_MAX_NUM_OF_FAR && req->update_far[i].presence; i++) {
        far = ogs_pfcp_far_find(&mbs_sess->pfcp, req->update_far[i].far_id.u32);
        ogs_assert(far);
        if (req->update_far[i].apply_action.presence)
            far->apply_action = req->update_far[i].apply_action.u16;
    }

    _update_pdrs(mbs_sess, req);
    _update_qers(mbs_sess, req);

    /* buffering that this request ends: the packets held are forwarded or dropped */
    for (i = 0; i < OGS_MAX_NUM_OF_FAR && req->update_far[i].presence; i++) {
        far = ogs_pfcp_far_find(&mbs_sess->pfcp, req->update_far[i].far_id.u32);
        if (req->update_far[i].apply_action.presence && !(far->apply_action & OGS_PFCP_APPLY_ACTION_BUFF))
            _release_buffered_packets(mbs_sess, far);
    }

    _remove_flows(mbs_sess, req);

    ogs_assert(OGS_OK == upf_pfcp_n4mb_send_session_modification_response(xact, mbs_sess));
    upf_gtpu_path_sync();
    return;

rollback_flows:
    _flows_rollback(mbs_sess, req);
rollback:
    _journal_rollback(&journal);
reject:
    ogs_pfcp_send_error_message(xact, mbs_sess->smf_n4mb_f_seid.seid,
            OGS_PFCP_SESSION_MODIFICATION_RESPONSE_TYPE, cause_value, offending_ie_value);
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
    upf_gtpu_path_sync();
}

static void _mbs_tunnel_poll_handler(short when, ogs_socket_t fd, void *data)
{
    upf_mbs_sess_t *mbs_sess = data;
    ogs_pkbuf_t *buf;
    void *data_buf;
    ssize_t read_bytes;

    buf = ogs_pkbuf_alloc(mbs_sess->udp_tunnel_pkbuf_pool, mbs_sess->udp_tunnel_mtu + OGS_GTPV1U_5GC_HEADER_LEN);
    ogs_pkbuf_reserve(buf, OGS_GTPV1U_5GC_HEADER_LEN); /* reserve space to add GTP-U headers later */
    data_buf = ogs_pkbuf_put(buf, mbs_sess->udp_tunnel_mtu); /* reserve buffer for read */

    /* TODO: use ogs_recvfrom and check that the sending IP is allowed to send to this port */
    read_bytes = ogs_recv(fd, data_buf, mbs_sess->udp_tunnel_mtu, MSG_DONTWAIT);
    if (read_bytes > 0) {
        ogs_pkbuf_trim(buf, read_bytes);
        /* call pkbuf processing routine for incoming N6mb/N9mb */
        upf_gtpv1_receive_buffer_for_mbs_sess(buf, mbs_sess);
    } else {
        /* nothing to read (the datagram was dropped by the kernel after the wake-up, e.g. a wrong checksum) or an error */
        ogs_pkbuf_free(buf);
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
