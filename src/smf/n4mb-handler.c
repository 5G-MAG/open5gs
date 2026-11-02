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
#include "mbs-subsc.h"
#include "pfcp-path.h"

// NOTE (borieher): Needed for the SBI request
#include "sbi-path.h"
#include "mbs-restore.h"

static const char *_pfcp_cause_to_problem_cause(uint8_t pfcp_cause);

/* Sets the ingress tunnel of the session from a Local Ingress Tunnel IE. Returns whether it is another one than the
 * session had (false for the first one: nothing changed from a tunnel that was not there). */
static bool _set_ingress_tunnel(smf_mbs_sess_t *mbs_sess, ogs_pfcp_local_ingress_tunnel_t *lit)
{
    ogs_sockaddr_t *old = NULL;
    bool changed = false;

    ogs_assert(mbs_sess);
    ogs_assert(lit);

    old = mbs_sess->ingress_tun_addr;
    mbs_sess->ingress_tun_addr = NULL;

    if (lit->ipv4) {
        char address[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &lit->addr, address, sizeof(address));
        ogs_addaddrinfo(&mbs_sess->ingress_tun_addr, AF_INET, address, htons(lit->port), 0);
    } else if (lit->ipv6) {
        char address[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &lit->addr6, address, sizeof(address));
        ogs_addaddrinfo(&mbs_sess->ingress_tun_addr, AF_INET6, address, htons(lit->port), 0);
    }

    if (old) {
        changed = !mbs_sess->ingress_tun_addr ||
            !ogs_sockaddr_is_equal(old, mbs_sess->ingress_tun_addr) ||
            OGS_PORT(old) != OGS_PORT(mbs_sess->ingress_tun_addr);
        ogs_freeaddrinfo(old);
    }

    return changed;
}

// The northbound request of a session that cannot be established has been answered with an error: the session must
// not stay in the tables. A MB-UPF that accepted the request holds a session the MB-SMF will not use, so it is deleted
// (the deletion response releases the local session); otherwise the session is released now.
static void _abandon_failed_establishment(smf_mbs_sess_t *mbs_sess,
        ogs_pfcp_session_establishment_response_t *rsp)
{
    ogs_assert(mbs_sess);
    ogs_assert(rsp);

    if (rsp->cause.presence && rsp->cause.u8 == OGS_PFCP_CAUSE_REQUEST_ACCEPTED &&
            rsp->up_f_seid.presence) {
        ogs_pfcp_f_seid_t *up_f_seid = rsp->up_f_seid.data;
        ogs_assert(up_f_seid);
        mbs_sess->upf_n4mb_seid = be64toh(up_f_seid->seid);
        if (smf_5gc_pfcp_n4mb_send_session_deletion_request(mbs_sess) == OGS_OK)
            return;
        ogs_error("Cannot delete the unused MB-UPF session");
    }

    smf_mbs_sess_release(mbs_sess);
}

uint8_t smf_n4mb_handle_session_establishment_response(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_establishment_response_t *rsp)
{
    int i;
    // TODO (borieher): Just a place to test the request
    int r;

    uint8_t cause_value = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
    uint16_t offending_ie_value = 0;

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

    if (!sbi_stream) {
        // The consumer closed its connection while the MB-UPF was answering: nobody can be told the
        // outcome and nobody holds the session reference, so the session is not kept.
        ogs_warn("The consumer of the MBS Session Create is gone, abandoning the session");
        _abandon_failed_establishment(mbs_sess, rsp);
        return OGS_PFCP_CAUSE_REQUEST_REJECTED;
    }

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
        // NOTE: _pfcp_cause_to_problem_cause() (below) already maps the specific PFCP cause the UPF
        // returned to the correct TS 29.500 value (MANDATORY_IE_MISSING/INCORRECT_LENGTH/
        // INSUFFICIENT_RESOURCES/SYSTEM_FAILURE) -- more precise than a blanket SYSTEM_FAILURE for
        // every rejection, so kept in preference to that when the two approaches collided here.
        ogs_assert(true == ogs_sbi_server_send_error(sbi_stream, 500, NULL, "Unable to establish MBS session", "MBS Session establishment with UPF rejected", _pfcp_cause_to_problem_cause(cause_value)));
        _abandon_failed_establishment(mbs_sess, rsp);
        return cause_value;
    }

    for (i = 0; i < OGS_MAX_NUM_OF_PDR; i++) {
        pdr = ogs_pfcp_handle_created_pdr(
                &mbs_sess->pfcp, &rsp->created_pdr[i],
                &cause_value, &offending_ie_value);

        if (!pdr)
            break;
    }

    /* TS 29.244 V18.10.0 tables 7.5.3.2 and 7.5.3.5: "If the UP function allocates the Local Ingress Tunnel, this
     * IE shall be present and shall contain the Local Ingress Tunnel to be used for this PDR." (Created PDR) and
     * the same for the Created Traffic Endpoint: it is read from the endpoint when the request used one, else
     * from a Created PDR. */
    if (rsp->created_traffic_endpoint.presence &&
            rsp->created_traffic_endpoint.local_ingress_tunnel.presence) {
        ogs_debug("Got Local Ingress Tunnel in the Created Traffic Endpoint");
        _set_ingress_tunnel(mbs_sess, rsp->created_traffic_endpoint.local_ingress_tunnel.data);
    } else {
        for (i = 0; i < OGS_MAX_NUM_OF_PDR && rsp->created_pdr[i].presence; i++) {
            if (rsp->created_pdr[i].local_ingress_tunnel.presence) {
                ogs_debug("Got Local Ingress Tunnel in a Created PDR");
                _set_ingress_tunnel(mbs_sess, rsp->created_pdr[i].local_ingress_tunnel.data);
                break;
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
        ogs_assert(true == ogs_sbi_server_send_error(sbi_stream, 500, NULL, "Unable to establish MBS session", "MBS Session establishment with UPF failed", _pfcp_cause_to_problem_cause(cause_value)));
        _abandon_failed_establishment(mbs_sess, rsp);
        return cause_value;
    }

    if (mbs_sess->upf_n3mb_addr == NULL && mbs_sess->upf_n3mb_addr6 == NULL) {
        ogs_error("No UP F-TEID");
        ogs_assert(true == ogs_sbi_server_send_error(sbi_stream, 500, NULL, "No UP F-TEID", "Session Context not found", N4MB_CAUSE_CONTEXT_NOT_FOUND));
        _abandon_failed_establishment(mbs_sess, rsp);
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
                // The parser leaves the field as received (network order, like the addresses next to it); the
                // session holds the TEID as a number, as the NGAP and PFCP builders expect.
                mbs_sess->c_teid = be32toh(multicast_transport_information.c_teid);
            }
        }
    }

    // Send response to SBI session
    memset(&sendmsg, 0, sizeof(sendmsg));
    memset(&header, 0, sizeof(header));

    Tmgi = ogs_sbi_build_tmgi(mbs_sess->tmgi);
    if (mbs_sess->mbs_session_id.is_tmgi) {
        Tmgi_copy = OpenAPI_tmgi_copy(Tmgi_copy, Tmgi);
        Mbs_session_id = OpenAPI_mbs_session_id_create(Tmgi_copy, NULL,
                mbs_sess->mbs_session_id.nid ? ogs_strdup(mbs_sess->mbs_session_id.nid) : NULL);
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

    /* TS 29.532 V18.6.0 cl. 5.3.2.2.1 step 2a: the response contains "a representation of the created MBS session
     * status subscription, including the list of MBS session events successfully subscribed, the URI of the created
     * subscription and the expiry time after which the subscription becomes invalid, if the Create request includes
     * the subscription to events about the MBS session and the subscription was created successfully." The
     * subscription of the request is the session's only one: nobody holds the session's reference yet. */
    OpenAPI_mbs_session_subscription_t *mbs_session_subsc = NULL;
    {
        smf_mbs_subsc_t *subsc = NULL;
        ogs_list_for_each(&smf_self()->smf_mbs_subsc_list, subsc) {
            if (subsc->mbs_sess_id == mbs_sess->id) {
                char *subsc_uri = smf_mbs_subsc_uri(ogs_sbi_server_from_stream(sbi_stream), subsc);

                mbs_session_subsc = smf_mbs_subsc_build(subsc, mbs_sess, subsc_uri);
                ogs_free(subsc_uri);
                break;
            }
        }
    }

    // TS 29.532 V18.6.0 cl.5.3.2.2.1 step 2b: if this session's own requested MBS service area is
    // not entirely covered by this MB-SMF's own configured coverage (smf.yaml mbsServiceArea,
    // unconfigured means no restriction -- rule 12), report the reduced area back in
    // redMbsServArea. See smf_mbs_service_area_reduce()'s own scope note (TAI-list coverage only).
    OpenAPI_mbs_service_area_t *Red_mbs_service_area = NULL;
    if (mbs_sess->service_area_reduced && mbs_sess->mbs_service_area)
        Red_mbs_service_area = ogs_sbi_build_mbs_service_area(mbs_sess->mbs_service_area);

    Ext_mbs_session = OpenAPI_ext_mbs_session_create(Mbs_session_id   /* mbs_session_id */,
                                                     false            /* is_tmgi_alloc_req */, 0 /* tmgi_alloc_req */,
                                                     Tmgi             /* tmgi */,
                                                     expiration_time  /* expiration_time */,
                                                     Mbs_service_type /* service_type */,
                                                                                                          // These report the allocation actually made, not a
                                                                                                          // fixed false/0; see
                                                                                                          // smf_nmbsmf_handle_mbs_session_create()
                                                                                                          // (TS 29.532 cl.5.3.2.2.1).
                                                     mbs_sess->location_dependent /* is_location_dependent */, mbs_sess->location_dependent /* location_dependent */,
                                                     mbs_sess->location_dependent /* is_area_session_id */, mbs_sess->area_session_id /* area_session_id */,
                                                     false            /* is_ingress_tun_addr_req */, 0 /* ingress_tun_addr_req */,
                                                     ingress_tunnel_list /* ingress_tun_addr */,
                                                     Ssm              /* ssm */,
                                                     NULL             /* mbs_service_area */,
                                                     Red_mbs_service_area /* red_mbs_service_area */,
                                                     NULL             /* ext_mbs_service_area */,
                                                     mbs_sess->dnn ? ogs_strdup(mbs_sess->dnn) : NULL /* dnn */,
                                                     smf_mbs_sess_build_snssai(mbs_sess) /* snssai */,
                                                     NULL             /* activation_time */,
                                                     mbs_sess->start_time ? ogs_strdup(mbs_sess->start_time) : NULL /* start_time */,
                                                     mbs_sess->termination_time ? ogs_strdup(mbs_sess->termination_time) : NULL /* termination_time */,
                                                     NULL             /* mbs_serv_info */,
                                                     mbs_session_subsc /* mbs_session_subsc */,
                                                     mbs_sess->activity_status /* activity_status */,
                                                     false            /* is_any_ue_ind */, 0 /* any_ue_ind */,
                                                     smf_mbs_sess_build_fsa_id_list(mbs_sess) /* mbs_fsa_id_list */,
                                                     NULL             /* mbs_security_context */,
                                                     false            /* is_contact_pcf_ind */, 0 /* contact_pcf_ind */,
                                                     NULL             /* nr_red_cap_ue_info */);

    CreateRspData = OpenAPI_create_rsp_data_create(Ext_mbs_session, NULL);

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


        // Gated on service_type. Namf_MBSBroadcast is a Broadcast-only service (TS 23.247 cl.7.3.1 step 2 sends
        // it "if the service type is broadcast service"); a Multicast session has no TMGI-centric broadcast
        // context to create here and instead uses Namf_MBSCommunication at UE-join time, a separate procedure
        // that is NOT IMPLEMENTED.
    if (ogs_strcasecmp(mbs_sess->service_type, "BROADCAST") == 0) {
        r = smf_sbi_send_mbs_broadcast_context_create(mbs_sess);
        ogs_expect(r == OGS_OK);
    }

    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}

static const char *_pfcp_cause_to_problem_cause(uint8_t pfcp_cause)
{
    switch (pfcp_cause) {
    case OGS_PFCP_CAUSE_MANDATORY_IE_MISSING:
    case OGS_PFCP_CAUSE_CONDITIONAL_IE_MISSING:
	return "MANDATORY_IE_MISSING";
    case OGS_PFCP_CAUSE_INVALID_LENGTH:
        return "INCORRECT_LENGTH";
    case OGS_PFCP_CAUSE_NO_RESOURCES_AVAILABLE:
        return "INSUFFICIENT_RESOURCES";
    default:
        break;
    }
    return "SYSTEM_FAILURE";
}

uint8_t smf_n4mb_handle_session_modification_response(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_modification_response_t *rsp)
{
    uint8_t cause_value = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;

    ogs_assert(mbs_sess);
    ogs_assert(xact);
    ogs_assert(rsp);

    ogs_debug("N4mb Session Modification Response");

    ogs_pfcp_xact_commit(xact);

    if (rsp->cause.presence) {
        if (rsp->cause.u8 != OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
            cause_value = rsp->cause.u8;
            ogs_error("N4mb Session Modification of mbsSessionRef[%s] rejected by the MB-UPF: PFCP Cause [%d]",
                    mbs_sess->mbs_session_ref, cause_value);
        }
    } else {
        ogs_error("N4mb Session Modification Response: No Cause");
        cause_value = OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    }

    /* A Local Ingress Tunnel the MB-UPF allocated for the modification (TS 29.244 V18.10.0 tables 7.5.3.2 and 7.5.3.5)
     * that is not the session's is a new ingress tunnel address: TS 29.532 V18.6.0 cl. 5.3.2.8.1 reports
     * "new ingress tunnel address used over the N6mb/Nmb9 reference point" (INGRESS_TUNNEL_ADD_CHANGE). */
    if (cause_value == OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
        bool changed = false;
        int i;

        if (rsp->created_updated_traffic_endpoint.presence &&
                rsp->created_updated_traffic_endpoint.local_ingress_tunnel.presence) {
            changed = _set_ingress_tunnel(mbs_sess, rsp->created_updated_traffic_endpoint.local_ingress_tunnel.data);
        } else {
            for (i = 0; i < OGS_MAX_NUM_OF_PDR && rsp->created_pdr[i].presence; i++) {
                if (rsp->created_pdr[i].local_ingress_tunnel.presence) {
                    changed = _set_ingress_tunnel(mbs_sess, rsp->created_pdr[i].local_ingress_tunnel.data);
                    break;
                }
            }
        }
        if (changed)
            smf_mbs_sess_report_ingress_tunnel_change(mbs_sess);
    }

    return cause_value;
}

/*
 * BUG FIX: this response handler did not exist before -- no N4mb Session Deletion Request was ever sent
 * (see smf_5gc_pfcp_n4mb_send_session_deletion_request() in pfcp-path.c), so there was nothing to
 * respond to. This is what actually completes the release chain: it releases the local smf_mbs_sess_t
 * (freeing the local PFCP bookkeeping via smf_mbs_sess_release()) only once the UPF has genuinely
 * acknowledged (or rejected) tearing down its own session, instead of the caller
 * (smf_nmbsmf_handle_mbs_session_release()) freeing it eagerly before the UPF was ever told.
 */
uint8_t smf_n4mb_handle_session_deletion_response(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact,
        ogs_pfcp_session_deletion_response_t *rsp)
{
    uint8_t cause_value = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;

    ogs_assert(mbs_sess);
    ogs_assert(xact);
    ogs_assert(rsp);

    ogs_debug("N4mb Session Deletion Response");

    ogs_pfcp_xact_commit(xact);

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
        // An UPF-side rejection (or malformed response) shouldn't leak the SMF's own bookkeeping
        // forever -- there is no northbound caller left waiting; the 204 response was already sent by
        // smf_nmbsmf_handle_mbs_session_release() before this request was even dispatched.
        ogs_error("MBS Session N4mb deletion rejected by UPF, releasing local state anyway");
    }

    smf_mbs_sess_release(mbs_sess);

    return cause_value;
}

uint8_t smf_n4mb_handle_node_report_request(
        ogs_pfcp_node_t *node, ogs_pfcp_node_report_request_t *req, uint16_t *offending_ie)
{
    ogs_ip_t peer;

    ogs_assert(node);
    ogs_assert(req);
    ogs_assert(offending_ie);

    *offending_ie = 0;

    /* TS 29.244 V18.10.0 table 7.4.5.1.1-1: "Node Report Type | M" */
    if (!req->node_report_type.presence || !req->node_report_type.data || req->node_report_type.len < 1) {
        ogs_error("PFCP Node Report Request: No Node Report Type");
        *offending_ie = OGS_PFCP_NODE_REPORT_TYPE_TYPE;
        return OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    }

    /* "User Plane Path Failure Report | C | This IE shall be present if the Node Report Type indicates a User Plane
     * Path Failure Report." (Node Report Type octet 5, bit 1, UPFR). Reports of the other types are not
     * reports of an N3mb path: nothing in TS 23.527 clause 8 gives the MB-SMF an action for them. */
    if (!(((const uint8_t *)req->node_report_type.data)[0] & 0x01)) {
        ogs_info("PFCP Node Report Request: Node Report Type [0x%02x] needs no action of the MB-SMF",
                ((const uint8_t *)req->node_report_type.data)[0]);
        return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
    }

    if (!req->user_plane_path_failure_report.presence) {
        ogs_error("PFCP Node Report Request: the User Plane Path Failure Report is missing");
        *offending_ie = OGS_PFCP_USER_PLANE_PATH_FAILURE_REPORT_TYPE;
        return OGS_PFCP_CAUSE_MANDATORY_IE_MISSING;
    }

    /* "Remote GTP-U Peer | M" in the report */
    if (!req->user_plane_path_failure_report.remote_gtp_u_peer.presence ||
            !smf_mbs_decode_remote_gtp_u_peer(req->user_plane_path_failure_report.remote_gtp_u_peer.data,
                req->user_plane_path_failure_report.remote_gtp_u_peer.len, &peer)) {
        /* TS 29.244 V18.10.0 clause 8.2.1, cause 69: "Mandatory IE incorrect"; and 66: "Mandatory IE missing" */
        bool missing = !req->user_plane_path_failure_report.remote_gtp_u_peer.presence;

        ogs_error("PFCP Node Report Request: the Remote GTP-U Peer is %s", missing ? "missing" : "incorrect");
        *offending_ie = OGS_PFCP_REMOTE_GTP_U_PEER_TYPE;
        return missing ? OGS_PFCP_CAUSE_MANDATORY_IE_MISSING : OGS_PFCP_CAUSE_MANDATORY_IE_INCORRECT;
    }

    smf_mbs_node_report_path_failure(node, &peer);
    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}
