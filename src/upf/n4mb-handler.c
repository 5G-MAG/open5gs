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
static ogs_pkbuf_config_t *_udp_tunnel_make_pool_config(size_t max_buf_size, size_t max_buffers);
static int get_mtu_for_address(const struct sockaddr *addr, socklen_t addr_len);

void upf_n4mb_handle_session_establishment_request(
        upf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_establishment_request_t *req)
{

    int i;
    uint8_t cause_value = 0;
    uint8_t offending_ie_value = 0;

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
        ogs_error("No Context");
        ogs_pfcp_send_error_message(xact, 0,
                OGS_PFCP_SESSION_ESTABLISHMENT_RESPONSE_TYPE,
                OGS_PFCP_CAUSE_MANDATORY_IE_MISSING, 0);
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

        ogs_assert(0 < ogs_fqdn_parse(apn_dnn, req->apn_dnn.data,
                ogs_min(req->apn_dnn.len, OGS_MAX_DNN_LEN)));


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

    if (req->create_traffic_endpoint.presence) {
        if (req->create_traffic_endpoint.local_ingress_tunnel.presence) {
            ogs_pfcp_local_ingress_tunnel_t *lit = (ogs_pfcp_local_ingress_tunnel_t*)req->create_traffic_endpoint.local_ingress_tunnel.data;
            if (lit->choose) {
                int mtu = 1500;
                if (lit->ipv4) {
                    // Create IPv4 UDP tunnel endpoint and return the address & port in the session response
                    ogs_sockaddr_t *bind_address = NULL;
                    upf_context_t *ctx = upf_self();
                    ogs_copyaddrinfo(&bind_address, &ctx->mbs_udp_tun_base_addr);
                    bind_address->ogs_sin_port = _get_next_udp_tunnel_port(ctx);
                    mbs_sess->udp_tunnel = ogs_sock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    ogs_sock_bind(mbs_sess->udp_tunnel, bind_address);
                    // get the true local address to fill in ephemeral ports.
                    socklen_t name_len = sizeof(mbs_sess->udp_tunnel->local_addr.ss);
                    getsockname(mbs_sess->udp_tunnel->fd, (struct sockaddr*)&mbs_sess->udp_tunnel->local_addr.ss, &name_len);
                    ogs_freeaddrinfo(bind_address);
                    /* find the MTU of the interface the UDP tunnel is bound to */
                    mtu = get_mtu_for_address((const struct sockaddr*)&mbs_sess->udp_tunnel->local_addr.ss, name_len);
                    mbs_sess->udp_tunnel_mtu = mtu - sizeof(struct ether_header) - sizeof(struct iphdr) - sizeof(struct udphdr);
                    ogs_debug("UDP tunnel using MTU of %i (%i after overheads)", mtu, mbs_sess->udp_tunnel_mtu);
                    ogs_pkbuf_config_t *config = _udp_tunnel_make_pool_config(mbs_sess->udp_tunnel_mtu, 32); /* 32 buffers */
                    mbs_sess->udp_tunnel_pkbuf_pool = ogs_pkbuf_pool_create(config);
                    ogs_free(config);
                    ogs_pollset_add(ogs_app()->pollset, OGS_POLLIN, mbs_sess->udp_tunnel->fd, _mbs_tunnel_poll_handler, mbs_sess);
                } else if (lit->ipv6) {
                    // Create IPv6 UDP tunnel endpoint and return the address & port in the session response
                    ogs_sockaddr_t *bind_address = NULL;
                    upf_context_t *ctx = upf_self();
                    ogs_copyaddrinfo(&bind_address, &ctx->mbs_udp_tun_base_addr);
                    bind_address->sin6.sin6_port = _get_next_udp_tunnel_port(ctx);
                    mbs_sess->udp_tunnel = ogs_sock_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
                    ogs_sock_bind(mbs_sess->udp_tunnel, bind_address);
                    // get the true local address to fill in ephemeral ports.
                    socklen_t name_len = sizeof(mbs_sess->udp_tunnel->local_addr.ss);
                    getsockname(mbs_sess->udp_tunnel->fd, (struct sockaddr*)&mbs_sess->udp_tunnel->local_addr.ss, &name_len);
                    ogs_freeaddrinfo(bind_address);
                    /* find the MTU of the interface the UDP tunnel is bound to */
                    mtu = get_mtu_for_address((const struct sockaddr*)&mbs_sess->udp_tunnel->local_addr.ss, name_len);
                    mbs_sess->udp_tunnel_mtu = mtu - sizeof(struct ether_header) - sizeof(struct ip6_hdr) - sizeof(struct udphdr);
                    ogs_debug("UDP tunnel using MTU of %i (%i after overheads)", mtu, mbs_sess->udp_tunnel_mtu);
                    ogs_pkbuf_config_t *config = _udp_tunnel_make_pool_config(mbs_sess->udp_tunnel_mtu, 32); /* 32 buffers */
                    mbs_sess->udp_tunnel_pkbuf_pool = ogs_pkbuf_pool_create(config);
                    ogs_free(config);
                    ogs_pollset_add(ogs_app()->pollset, OGS_POLLIN, mbs_sess->udp_tunnel->fd, _mbs_tunnel_poll_handler, mbs_sess);
                }
            } else {
                if (lit->ipv4) {
                    // TODO: Open IPv4 UDP tunnel endpoint at the address and port requested
                }
                if (lit->ipv6) {
                    // TODO: Open IPv6 UDP tunnel endpoint at the address and port requested
                }
            }
        }
    }

    for (i = 0; i < num_of_created_pdr; i++) {
        pdr = created_pdr[i];
        ogs_assert(pdr);

        // TODO (borieher): Modify this to work with IP ranges
        /* Setup AF SSM IP address */
        if (pdr->ip_multicast_addressing_info_len) {
            if (pdr->ip_multicast_addressing_info.source_ip_address.v4) {
                mbs_sess->ssm.src_ip_addr.ipv4 = 1;
                mbs_sess->ssm.src_ip_addr.len = OGS_IPV4_LEN;
                mbs_sess->ssm.src_ip_addr.addr = pdr->ip_multicast_addressing_info.source_ip_address.ipv4_addr;
            } else if (pdr->ip_multicast_addressing_info.source_ip_address.v6) {
                mbs_sess->ssm.src_ip_addr.ipv6 = 1;
                mbs_sess->ssm.src_ip_addr.len = OGS_IPV6_LEN;
                memcpy(&mbs_sess->ssm.src_ip_addr.addr6, pdr->ip_multicast_addressing_info.source_ip_address.ipv6_addr, OGS_IPV6_LEN);
            }

            if (pdr->ip_multicast_addressing_info.ip_multicast_address.v4) {
                mbs_sess->ssm.dest_ip_addr.ipv4 = 1;
                mbs_sess->ssm.dest_ip_addr.len = OGS_IPV4_LEN;
                mbs_sess->ssm.dest_ip_addr.addr = pdr->ip_multicast_addressing_info.ip_multicast_address.s_ipv4_addr;
            } else if (pdr->ip_multicast_addressing_info.ip_multicast_address.v6) {
                mbs_sess->ssm.dest_ip_addr.ipv6 = 1;
                mbs_sess->ssm.dest_ip_addr.len = OGS_IPV6_LEN;
                memcpy(&mbs_sess->ssm.dest_ip_addr.addr6, pdr->ip_multicast_addressing_info.ip_multicast_address.s_ipv6_addr, OGS_IPV6_LEN);
            }

            upf_mbs_sess_set_ssm(mbs_sess);
        }

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
            if (ogs_pfcp_parse_mbs_session_identifier(&mbs_session_identifier, &req->mbs_session_n4mb_control_information.mbs_session_identifier)) {
                // Free old values
                if (mbs_sess->mbs_session_id.is_tmgi && mbs_sess->mbs_session_id.tmgi) ogs_free(mbs_sess->mbs_session_id.tmgi);
                if (mbs_sess->mbs_session_id.is_ssm && mbs_sess->mbs_session_id.ssm) ogs_free(mbs_sess->mbs_session_id.ssm);
                if (mbs_sess->mbs_session_id.nid) ogs_free(mbs_sess->mbs_session_id.nid);

                // Initialise to empty
                mbs_sess->mbs_session_id.is_tmgi = 0;
                mbs_sess->mbs_session_id.is_ssm = 0;
                mbs_sess->mbs_session_id.tmgi = NULL;
                mbs_sess->mbs_session_id.nid = NULL;

                if (mbs_session_identifier.tmgif) {
                    mbs_sess->mbs_session_id.is_tmgi = 1;
                    mbs_sess->mbs_session_id.tmgi = (ogs_tmgi_t*)ogs_calloc(1, sizeof(*mbs_sess->mbs_session_id.tmgi));
                    mbs_sess->mbs_session_id.tmgi->mbs_service_id = ogs_msprintf("%.2X%.2X%.2X", mbs_session_identifier.tmgi.mbs_service_id[2], mbs_session_identifier.tmgi.mbs_service_id[1], mbs_session_identifier.tmgi.mbs_service_id[0]);
                    mbs_sess->mbs_session_id.tmgi->plmn_id.mcc1 = mbs_session_identifier.tmgi.mcc1;
                    mbs_sess->mbs_session_id.tmgi->plmn_id.mcc2 = mbs_session_identifier.tmgi.mcc2;
                    mbs_sess->mbs_session_id.tmgi->plmn_id.mcc3 = mbs_session_identifier.tmgi.mcc3;
                    mbs_sess->mbs_session_id.tmgi->plmn_id.mnc1 = mbs_session_identifier.tmgi.mnc1;
                    mbs_sess->mbs_session_id.tmgi->plmn_id.mnc2 = mbs_session_identifier.tmgi.mnc2;
                    mbs_sess->mbs_session_id.tmgi->plmn_id.mnc3 = mbs_session_identifier.tmgi.mnc3;
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
                }
                if (mbs_session_identifier.nidif) {
                    mbs_sess->mbs_session_id.nid = (char*)ogs_calloc(1, 12);
                }
            }
        }

        // MBSN4mbReq-Flags IE
        if (req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.presence)
            memcpy(&mbs_sess->mbs_flags, req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.data,
                sizeof(ogs_pfcp_mbsn4mbreq_flags_t));
    }

    ogs_assert(OGS_OK ==
            upf_pfcp_n4mb_send_session_establishment_response(
                xact, mbs_sess, created_pdr, num_of_created_pdr));

    /* Setup GTP Node */
    ogs_list_for_each(&mbs_sess->pfcp.far_list, far) {
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


    if(upf_self()->mbs_multicastrouter_activate && !req->create_traffic_endpoint.presence){
        for (i = 0; i < num_of_created_pdr; i++) {
            pdr = created_pdr[i];
            ogs_assert(pdr);

            if (pdr->ip_multicast_addressing_info.source_ip_address.v4 || pdr->ip_multicast_addressing_info.ip_multicast_address.v4) {
                // get the ips from the PDR
                uint32_t ip_multicast = pdr->ip_multicast_addressing_info.ip_multicast_address.s_ipv4_addr;
                uint32_t ip_source    = pdr->ip_multicast_addressing_info.source_ip_address.ipv4_addr;

                char ip_multicast_string[INET_ADDRSTRLEN];
                char ip_source_string[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ip_multicast, ip_multicast_string, sizeof ip_multicast_string);
                inet_ntop(AF_INET, &ip_source, ip_source_string, sizeof ip_source_string);

                if(OGS_ERROR == multicastrouter_add_route_and_join(ip_source_string, ip_multicast_string)){
                    ogs_error("Could not create multicast route: %s to %s", ip_source_string, ip_multicast_string);
                }else{
                    ogs_info("Multicast route created: %s to %s", ip_source_string, ip_multicast_string);
                    ogs_info("Join multicast group: %s", ip_multicast_string);
                }
            }else if (pdr->ip_multicast_addressing_info.source_ip_address.v6 || pdr->ip_multicast_addressing_info.ip_multicast_address.v6) {
                ogs_warn("Multicast router for ipv6 not implemented yet");
                continue;
            }
        }
    }

    return;

cleanup:
    ogs_pfcp_sess_clear(&mbs_sess->pfcp);
    ogs_pfcp_send_error_message(xact, mbs_sess ? mbs_sess->smf_n4mb_f_seid.seid : 0,
            OGS_PFCP_SESSION_ESTABLISHMENT_RESPONSE_TYPE,
            cause_value, offending_ie_value);
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
