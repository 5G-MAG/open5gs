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

#include "context.h"
#include "n4mb-handler.h"

// NOTE (borieher): Needed for the SBI request
#include "sbi-path.h"



uint8_t smf_n4mb_handle_session_establishment_response(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_establishment_response_t *rsp)
{
    int i;
    // TODO (borieher): Just a place to test the request
    int r;

    uint8_t cause_value = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
    uint8_t offending_ie_value = 0;

    /* SBI response structures */
    OpenAPI_create_rsp_data_t *CreateRspData = NULL;
    OpenAPI_tmgi_t *Tmgi = NULL;
    OpenAPI_tmgi_t *Tmgi_copy = NULL;
    OpenAPI_ssm_t * Ssm = NULL;
    OpenAPI_ssm_t * Ssm_copy = NULL;
    OpenAPI_mbs_session_id_t *Mbs_session_id = NULL;
    OpenAPI_mbs_service_type_e Mbs_service_type = OpenAPI_mbs_service_type_NULL;
    OpenAPI_ext_mbs_session_t *Ext_mbs_session = NULL;
    ogs_sbi_server_t *server = NULL;
    ogs_sbi_response_t *response = NULL;
    ogs_sbi_message_t sendmsg;
    ogs_sbi_header_t header;
    /* */

    ogs_pfcp_f_seid_t *up_f_seid = NULL;

    ogs_pfcp_pdr_t *pdr = NULL;
    ogs_pfcp_far_t *far = NULL;

    ogs_assert(mbs_sess);
    ogs_assert(xact);
    ogs_assert(rsp);

    ogs_debug("N4mb Session Establishment Response");

    ogs_sbi_stream_t *sbi_stream = ogs_sbi_stream_find_by_id(xact->assoc_stream_id);

    ogs_pfcp_xact_commit(xact);

    if (rsp->up_f_seid.presence == 0) {
        ogs_error("No UP F-SEID");
        cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    }

    if (rsp->cause.presence) {
        if (rsp->cause.u8 != OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
            ogs_error("PFCP Cause [%d] : Not Accepted", rsp->cause.u8);
            cause_value = rsp->cause.u8;
        }
    } else {
        ogs_error("No Cause");
        cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    }

    if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
        ogs_assert(true == ogs_sbi_server_send_error(sbi_stream, 500, NULL, "Unable to establish MBS session", "MBS Session establishment with UPF rejected", "Unknown"));
        return cause_value;
    }

    for (i = 0; i < OGS_MAX_NUM_OF_PDR; i++) {
        pdr = ogs_pfcp_handle_created_pdr(
                &mbs_sess->pfcp, &rsp->created_pdr[i],
                &cause_value, &offending_ie_value);

        if (!pdr)
            break;
    }

    if (rsp->created_traffic_endpoint.presence) {
        ogs_debug("Got Created Traffic Endpoint");
        if (rsp->created_traffic_endpoint.local_ingress_tunnel.presence) {
            ogs_debug("Got Local Ingress Tunnel");
            ogs_pfcp_local_ingress_tunnel_t *lit =
                        (ogs_pfcp_local_ingress_tunnel_t*)rsp->created_traffic_endpoint.local_ingress_tunnel.data;
            if (mbs_sess->ingress_tun_addr) {
                ogs_freeaddrinfo(mbs_sess->ingress_tun_addr);
                mbs_sess->ingress_tun_addr = NULL;
            }
            if (lit->ipv4) {
                char address[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &lit->addr, address, sizeof(address));
                ogs_addaddrinfo(&mbs_sess->ingress_tun_addr, AF_INET, address, htons(lit->port), 0);
            } else if (lit->ipv6) {
                char address[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &lit->addr6, address, sizeof(address));
                ogs_addaddrinfo(&mbs_sess->ingress_tun_addr, AF_INET6, address, htons(lit->port), 0);
            }
        }
    }

    ogs_list_for_each(&mbs_sess->pfcp.pdr_list, pdr) {
        far = pdr->far;
        ogs_assert(far);

        if (pdr->src_if == OGS_PFCP_INTERFACE_ACCESS) {
            if (far->dst_if == OGS_PFCP_INTERFACE_CP_FUNCTION)
                ogs_pfcp_far_teid_hash_set(far);

            ogs_assert(mbs_sess->pfcp_node);
            if (mbs_sess->pfcp_node->up_function_features.ftup &&
                pdr->f_teid_len) {
                if (mbs_sess->upf_n3mb_addr)
                    ogs_freeaddrinfo(mbs_sess->upf_n3mb_addr);
                if (mbs_sess->upf_n3mb_addr6)
                    ogs_freeaddrinfo(mbs_sess->upf_n3mb_addr6);

                ogs_assert(OGS_OK ==
                    ogs_pfcp_f_teid_to_sockaddr(
                        &pdr->f_teid, pdr->f_teid_len,
                        &mbs_sess->upf_n3mb_addr, &mbs_sess->upf_n3mb_addr6));
            }
        } else if (pdr->src_if == OGS_PFCP_INTERFACE_CP_FUNCTION) {
            ogs_assert(OGS_ERROR != ogs_pfcp_setup_pdr_gtpu_node(pdr));
        }
    }

    if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
        ogs_error("PFCP Cause [%d] : Not Accepted", cause_value);
        return cause_value;
    }

    if (mbs_sess->upf_n3mb_addr == NULL && mbs_sess->upf_n3mb_addr6 == NULL) {
        ogs_error("No UP F-TEID");
        ogs_assert(true == ogs_sbi_server_send_error(sbi_stream, 500, NULL, "No UP F-TEID", "Session Context not found", "Unknown"));
        return OGS_PFCP_CAUSE_SESSION_CONTEXT_NOT_FOUND;
    }

    /* UP F-SEID */
    up_f_seid = rsp->up_f_seid.data;
    ogs_assert(up_f_seid);
    mbs_sess->upf_n4mb_seid = be64toh(up_f_seid->seid);

    /* MBS Session N4mb Information IE */
    if (rsp->mbs_session_n4mb_information.presence) {
        // Multicast Transport Information for MBS Session N4mb Information
        if (rsp->mbs_session_n4mb_information.multicast_transport_information.presence) {
            ogs_pfcp_multicast_transport_information_t multicast_transport_information;
            ogs_pfcp_parse_multicast_transport_information(&multicast_transport_information,
                    &rsp->mbs_session_n4mb_information.multicast_transport_information);

            if (multicast_transport_information.ip_multicast_distribution_address.address_type == 0) {
                mbs_sess->ll_ssm.dest_ip_addr.ipv4 = 1;
                mbs_sess->ll_ssm.dest_ip_addr.len = OGS_IPV4_LEN;
                mbs_sess->ll_ssm.dest_ip_addr.addr = multicast_transport_information.ip_multicast_distribution_address.ipv4_addr;
            } else if (multicast_transport_information.ip_multicast_distribution_address.address_type == 1) {
                mbs_sess->ll_ssm.dest_ip_addr.ipv6 = 1;
                mbs_sess->ll_ssm.dest_ip_addr.len = OGS_IPV6_LEN;
                memcpy(mbs_sess->ll_ssm.dest_ip_addr.addr6, multicast_transport_information.ip_multicast_distribution_address.ipv6_addr, OGS_IPV6_LEN);
            }

            if (multicast_transport_information.ip_source_address.address_type == 0) {
                mbs_sess->ll_ssm.src_ip_addr.ipv4 = 1;
                mbs_sess->ll_ssm.src_ip_addr.len = OGS_IPV4_LEN;
                mbs_sess->ll_ssm.src_ip_addr.addr = multicast_transport_information.ip_source_address.ipv4_addr;
            } else if (multicast_transport_information.ip_source_address.address_type == 1) {
                mbs_sess->ll_ssm.src_ip_addr.ipv6 = 1;
                mbs_sess->ll_ssm.src_ip_addr.len = OGS_IPV6_LEN;
                memcpy(mbs_sess->ll_ssm.src_ip_addr.addr6, multicast_transport_information.ip_source_address.ipv6_addr, OGS_IPV6_LEN);
            }

            if (multicast_transport_information.c_teid) {
                mbs_sess->c_teid = multicast_transport_information.c_teid;
            }
        }
    }

    // Send response to SBI session
    memset(&sendmsg, 0, sizeof(sendmsg));
    memset(&header, 0, sizeof(header));

    Tmgi = ogs_sbi_build_tmgi(mbs_sess->tmgi);
    if (mbs_sess->mbs_session_id.is_tmgi) {
        Tmgi_copy = OpenAPI_tmgi_copy(Tmgi_copy, Tmgi);
        Mbs_session_id = OpenAPI_mbs_session_id_create(Tmgi_copy, NULL, mbs_sess->mbs_session_id.nid);
    }

    if (mbs_sess->mbs_session_id.is_ssm) {
        Ssm = ogs_sbi_build_ssm(mbs_sess->mbs_session_id.ssm);
        Ssm_copy = OpenAPI_ssm_copy(Ssm_copy, Ssm);
        Mbs_session_id = OpenAPI_mbs_session_id_create(NULL, Ssm_copy, NULL /* nid */);
    }

    Mbs_service_type = OpenAPI_mbs_service_type_FromString(mbs_sess->service_type);

    OpenAPI_list_t *ingress_tunnel_list = NULL;
    if (mbs_sess->ingress_tun_addr) {
        char *ipv4_str = NULL;
        char *ipv6_str = NULL;
        int port;
        ingress_tunnel_list = OpenAPI_list_create();
        if (mbs_sess->ingress_tun_addr->ogs_sa_family == AF_INET) {
            char buf[OGS_ADDRSTRLEN];
            ipv4_str = ogs_strdup(OGS_ADDR(mbs_sess->ingress_tun_addr, buf));
	} else if (mbs_sess->ingress_tun_addr->ogs_sa_family == AF_INET6) {
            char buf[OGS_ADDRSTRLEN];
            ipv6_str = ogs_strdup(OGS_ADDR(mbs_sess->ingress_tun_addr, buf));
	}
        port = OGS_PORT(mbs_sess->ingress_tun_addr);
        OpenAPI_list_add(ingress_tunnel_list, OpenAPI_tunnel_address_create(ipv4_str, ipv6_str, port));
    }

    char *expiration_time = NULL;
    if (mbs_sess->tmgi) {
        expiration_time = ogs_strdup(mbs_sess->tmgi->expiration_time);
    }

    Ext_mbs_session = OpenAPI_ext_mbs_session_create(Mbs_session_id   /* mbs_session_id */,
                                                     false            /* is_tmgi_alloc_req */, 0 /* tmgi_alloc_req */,
                                                     Tmgi             /* tmgi */,
                                                     expiration_time  /* expiration_time */,
                                                     Mbs_service_type /* service_type */,
                                                     false            /* is_location_dependent */, 0 /* location_dependent */,
                                                     false            /* is_area_session_id */, 0 /* area_session_id */,
                                                     false            /* is_ingress_tun_addr_req */, 0 /* ingress_tun_addr_req */,
                                                     ingress_tunnel_list /* ingress_tun_addr */,
                                                     Ssm              /* ssm */,
                                                     NULL             /* mbs_service_area */,
                                                     NULL             /* ext_mbs_service_area */,
                                                     NULL             /* dnn */,
                                                     NULL             /* snssai */,
                                                     NULL             /* activation_time */,
                                                     NULL             /* start_time */,
                                                     NULL             /* termination_time */,
                                                     NULL             /* mbs_serv_info */,
                                                     NULL             /* mbs_session_subsc */,
                                                     OpenAPI_mbs_session_activity_status_NULL /* activity_status */,
                                                     false            /* is_any_ue_ind */, 0 /* any_ue_ind */,
                                                     NULL             /* mbs_fsa_id_list */,
                                                     NULL             /* mbs_security_context */,
                                                     false            /* is_contact_pcf_ind */, 0 /* contact_pcf_ind */);

    CreateRspData = OpenAPI_create_rsp_data_create(Ext_mbs_session, NULL);

    // TODO (borieher): Check the TMGIs in the already created MBS Sessions to avoid collisions

    /*********************************************************************
     * Send OGS_SBI_HTTP_STATUS_CREATED (/nmbsmf-mbssession/v1/mbs-sessions) to the consumer NF
     *********************************************************************/

    server = ogs_sbi_server_from_stream(sbi_stream);
    ogs_assert(server);

    // Adding the mbsSessionRef in the headers for the created resource
    header.service.name = (char *) OGS_SBI_SERVICE_NAME_NMBSMF_MBS_SESSION;
    header.api.version = (char *) OGS_SBI_API_V1;
    header.resource.component[0] =
        (char *) OGS_SBI_RESOURCE_NAME_MBS_SESSIONS;
    header.resource.component[1] = mbs_sess->mbs_session_ref;

    sendmsg.http.location = ogs_sbi_server_uri(server, &header);
    ogs_assert(sendmsg.http.location);

    sendmsg.CreateRspData = CreateRspData;

    response = ogs_sbi_build_response(&sendmsg, OGS_SBI_HTTP_STATUS_CREATED);

    ogs_assert(response);

    ogs_assert(true == ogs_sbi_server_send_response(sbi_stream, response));

    if (CreateRspData)
        OpenAPI_create_rsp_data_free(CreateRspData);

    if (sendmsg.http.location)
        ogs_free(sendmsg.http.location);


    // TODO (borieher): Remove this after testing
    r = smf_sbi_old_discover_and_send(
    OGS_SBI_SERVICE_TYPE_NAMF_MBS_BC, NULL,
    smf_namf_build_mbs_broadcast_context_create_request,
    mbs_sess, NULL, 0, (char *) OGS_SBI_RESOURCE_NAME_MBS_CONTEXTS);

    ogs_expect(r == OGS_OK);
    ogs_assert(r != OGS_ERROR);

    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}
