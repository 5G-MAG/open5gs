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

    ogs_pfcp_f_seid_t *up_f_seid = NULL;

    ogs_pfcp_pdr_t *pdr = NULL;
    ogs_pfcp_far_t *far = NULL;

    ogs_assert(mbs_sess);
    ogs_assert(xact);
    ogs_assert(rsp);

    ogs_debug("N4mb Session Establishment Response");

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

    if (cause_value != OGS_PFCP_CAUSE_REQUEST_ACCEPTED)
        return cause_value;

    for (i = 0; i < OGS_MAX_NUM_OF_PDR; i++) {
        pdr = ogs_pfcp_handle_created_pdr(
                &mbs_sess->pfcp, &rsp->created_pdr[i],
                &cause_value, &offending_ie_value);

        if (!pdr)
            break;
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

    // TODO (borieher): Remove this after testing
    r = smf_sbi_old_discover_and_send(
    OGS_SBI_SERVICE_TYPE_NAMF_MBS_BC, NULL,
    smf_namf_build_mbs_broadcast_context_create_request,
    mbs_sess, NULL, 0, (char *) OGS_SBI_RESOURCE_NAME_MBS_CONTEXTS);

    ogs_expect(r == OGS_OK);
    ogs_assert(r != OGS_ERROR);

    return OGS_PFCP_CAUSE_REQUEST_ACCEPTED;
}