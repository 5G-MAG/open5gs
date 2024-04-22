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

    /* MBS Session N4mb Information */
    // TODO (borieher): Should I check the outer_header_creation_description low_layer_ssm_and_c_teid flag too?
    if (mbs_sess->mbs_flags.provide_lower_layer_ssm) {
        upf_mbs_sess_set_llssm_and_c_teid(mbs_sess);

        rsp->mbs_session_n4mb_information.presence = 1;
        rsp->mbs_session_n4mb_information.multicast_transport_information.presence = 1;

        memset(&multicast_transport_information, 0, sizeof(ogs_pfcp_multicast_transport_information_t));

        multicast_transport_information.spare = 0;

        multicast_transport_information.c_teid = htobe32(mbs_sess->c_teid);
        created_pdr[0]->far->outer_header_creation.teid = mbs_sess->c_teid;

        multicast_transport_information_len += 5;

        if (mbs_sess->ll_ssm.dest_ip_addr.ipv4) {
            multicast_transport_information.ip_multicast_distribution_address.address_type = 0;
            multicast_transport_information.ip_multicast_distribution_address.address_length = OGS_IPV4_LEN;
            multicast_transport_information.ip_multicast_distribution_address.ipv4_addr = mbs_sess->ll_ssm.dest_ip_addr.addr;
            multicast_transport_information_len += 5;
        } else if (mbs_sess->ll_ssm.dest_ip_addr.ipv6) {
            multicast_transport_information.ip_multicast_distribution_address.address_type = 1;
            multicast_transport_information.ip_multicast_distribution_address.address_length = OGS_IPV6_LEN;
            memcpy(&multicast_transport_information.ip_multicast_distribution_address.ipv6_addr, mbs_sess->ll_ssm.dest_ip_addr.addr6,
                OGS_IPV6_LEN);
            multicast_transport_information_len += 17;
        }

        if (mbs_sess->ll_ssm.src_ip_addr.ipv4) {
            multicast_transport_information.ip_source_address.address_type = 0;
            multicast_transport_information.ip_source_address.address_length = OGS_IPV4_LEN;
            multicast_transport_information.ip_source_address.ipv4_addr = mbs_sess->ll_ssm.src_ip_addr.addr;
            multicast_transport_information_len += 5;
        } else if (mbs_sess->ll_ssm.src_ip_addr.ipv6) {
            multicast_transport_information.ip_source_address.address_type = 1;
            multicast_transport_information.ip_source_address.address_length = OGS_IPV6_LEN;
            memcpy(&multicast_transport_information.ip_source_address.ipv6_addr, mbs_sess->ll_ssm.src_ip_addr.addr6,
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
