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
#include "n4mb-build.h"

ogs_pkbuf_t *upf_n4mb_build_session_establishment_response(uint8_t type,
    upf_mbs_sess_t *mbs_sess, ogs_pfcp_pdr_t *created_pdr[], int num_of_created_pdr)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_session_establishment_response_t *rsp = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    int i = 0, j = 0;

    ogs_pfcp_node_id_t node_id;
    ogs_pfcp_f_seid_t f_seid;
    ogs_pfcp_multicast_transport_information_t multicast_transport_information;
    ogs_pfcp_local_ingress_tunnel_t local_ingress_tunnel;
    uint8_t multicast_transport_information_buf[39];
    int multicast_transport_information_len = 0;
    int len = 0;

    ogs_debug("N4mb Session Establishment Response");

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    rsp = &pfcp_message->pfcp_session_establishment_response;

    /* Node ID */
    ogs_pfcp_sockaddr_to_node_id(&node_id, &len);
    rsp->node_id.presence = 1;
    rsp->node_id.data = &node_id;
    rsp->node_id.len = len;

    /* Cause */
    rsp->cause.presence = 1;
    rsp->cause.u8 = OGS_PFCP_CAUSE_REQUEST_ACCEPTED;

    /* F-SEID */
    ogs_pfcp_sockaddr_to_f_seid(&f_seid, &len);
    f_seid.seid = htobe64(mbs_sess->upf_n4mb_seid);
    rsp->up_f_seid.presence = 1;
    rsp->up_f_seid.data = &f_seid;
    rsp->up_f_seid.len = len;

    ogs_pfcp_pdrbuf_init();

    /* Created PDR */
    for (i = 0, j = 0; i < num_of_created_pdr; i++) {
        bool pdr_presence = ogs_pfcp_build_created_pdr(
                &rsp->created_pdr[j], i, created_pdr[i]);
        if (pdr_presence == true) j++;
    }

    /* Created Traffic Endpoint */
    if (mbs_sess->udp_tunnel) {
	if (ogs_pfcp_sockaddr_to_local_ingress_tunnel(&mbs_sess->udp_tunnel->local_addr, &local_ingress_tunnel, &len) == OGS_OK) {
            rsp->created_traffic_endpoint.presence = 1;
            rsp->created_traffic_endpoint.local_ingress_tunnel.presence = 1;
	    rsp->created_traffic_endpoint.local_ingress_tunnel.data = &local_ingress_tunnel;
            rsp->created_traffic_endpoint.local_ingress_tunnel.len = len;
        } else {
            ogs_warn("Failed to create PFCP Local Ingress Tunnel address");
        }
    }

    /* MBS Session N4mb Information */
    // TODO (borieher): Should I check the outer_header_creation_description low_layer_ssm_and_c_teid flag too?
    if (mbs_sess->mbs_flags.provide_lower_layer_ssm) {
        upf_mbs_sess_set_llssm(mbs_sess);

        rsp->mbs_session_n4mb_information.presence = 1;
        rsp->mbs_session_n4mb_information.multicast_transport_information.presence = 1;

        memset(&multicast_transport_information, 0, sizeof(ogs_pfcp_multicast_transport_information_t));

        multicast_transport_information.spare = 0;

        multicast_transport_information.c_teid = htobe32(mbs_sess->c_teid);
                // Loops over the PDRs rather than indexing created_pdr[0]. One Common TEID is allocated per MBS
                // session and used, through Apply Action FSSM, to forward that session's data for every PDR and FAR
                // of the session (TS 29.244 cl.8.2.207 and cl.5.34.2.2), so indexing only the first leaves any
                // further FAR with outer_header_creation.teid == 0, which ogs_pfcp_send_g_pdu() copies verbatim
                // into outgoing GTP-U headers. Latent while the SMF creates one PDR and FAR per session, but this
                // is the structurally correct form.
        for (i = 0; i < num_of_created_pdr; i++)
            created_pdr[i]->far->outer_header_creation.teid = mbs_sess->c_teid;

        multicast_transport_information_len += 5;

        if (mbs_sess->ll_ssm->dest_ip_addr.ipv4) {
            multicast_transport_information.ip_multicast_distribution_address.address_type = 0;
            multicast_transport_information.ip_multicast_distribution_address.address_length = OGS_IPV4_LEN;
            multicast_transport_information.ip_multicast_distribution_address.ipv4_addr = mbs_sess->ll_ssm->dest_ip_addr.addr;
            multicast_transport_information_len += 5;
        } else if (mbs_sess->ll_ssm->dest_ip_addr.ipv6) {
            multicast_transport_information.ip_multicast_distribution_address.address_type = 1;
            multicast_transport_information.ip_multicast_distribution_address.address_length = OGS_IPV6_LEN;
            memcpy(&multicast_transport_information.ip_multicast_distribution_address.ipv6_addr, mbs_sess->ll_ssm->dest_ip_addr.addr6,
                OGS_IPV6_LEN);
            multicast_transport_information_len += 17;
        }

        if (mbs_sess->ll_ssm->src_ip_addr.ipv4) {
            multicast_transport_information.ip_source_address.address_type = 0;
            multicast_transport_information.ip_source_address.address_length = OGS_IPV4_LEN;
            multicast_transport_information.ip_source_address.ipv4_addr = mbs_sess->ll_ssm->src_ip_addr.addr;
            multicast_transport_information_len += 5;
        } else if (mbs_sess->ll_ssm->src_ip_addr.ipv6) {
            multicast_transport_information.ip_source_address.address_type = 1;
            multicast_transport_information.ip_source_address.address_length = OGS_IPV6_LEN;
            memcpy(&multicast_transport_information.ip_source_address.ipv6_addr, mbs_sess->ll_ssm->src_ip_addr.addr6,
                OGS_IPV6_LEN);
            multicast_transport_information_len += 17;
        }

        ogs_pfcp_build_multicast_transport_information(
            &rsp->mbs_session_n4mb_information.multicast_transport_information,
            &multicast_transport_information, multicast_transport_information_buf,
            multicast_transport_information_len);
    }

    pfcp_message->h.type = type;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_pfcp_pdrbuf_clear();
    ogs_free(pfcp_message);

    return pkbuf;
}

/*
 * BUG FIX: no N4mb Session Deletion Response builder existed at all -- mirrors the non-MBS
 * upf_n4_build_session_deletion_response(), minus the URR usage-report accumulation that function does
 * (MBS sessions in this codebase don't populate urr_list the way regular sessions do), via the same
 * generic ogs_pfcp_build_session_deletion_response() helper both use.
 */
ogs_pkbuf_t *upf_n4mb_build_session_deletion_response(uint8_t type, upf_mbs_sess_t *mbs_sess)
{
    ogs_pfcp_user_plane_report_t report;

    ogs_debug("N4mb Session Deletion Response");
    ogs_assert(mbs_sess);

    memset(&report, 0, sizeof(report));

    return ogs_pfcp_build_session_deletion_response(type, OGS_PFCP_CAUSE_REQUEST_ACCEPTED, &report);
}
