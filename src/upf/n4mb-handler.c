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
#include "pfcp-path.h"
#include "n4mb-handler.h"

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
    //ogs_pfcp_mbs_session_identifier_t mbs_session_identifier;
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
    }

    // TODO (borieher): Handle the reception of MBS Session N4mb Control Information IE
    if (req->mbs_session_n4mb_control_information.presence) {

        // MBS Session Identifier IE
        if (req->mbs_session_n4mb_control_information.mbs_session_identifier.presence) {
            // TODO (borieher): Continue parsing MBS Session Identifier, after fixing it
            //ogs_pfcp_parse_mbs_session_identifier(&mbs_session_identifier, req->mbs_session_n4mb_control_information.mbs_session_identifier.data);
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
        if (mbs_sess->ll_ssm.dest_ip_addr.ipv4) {
            far->outer_header_creation.addr = mbs_sess->ll_ssm.dest_ip_addr.addr;
            far->outer_header_creation_len += OGS_IPV4_LEN;
        } else if (mbs_sess->ll_ssm.dest_ip_addr.ipv6) {
            memcpy(&far->outer_header_creation.addr6, mbs_sess->ll_ssm.dest_ip_addr.addr6, OGS_IPV6_LEN);
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

    return;

cleanup:
    ogs_pfcp_sess_clear(&mbs_sess->pfcp);
    ogs_pfcp_send_error_message(xact, mbs_sess ? mbs_sess->smf_n4mb_f_seid.seid : 0,
            OGS_PFCP_SESSION_ESTABLISHMENT_RESPONSE_TYPE,
            cause_value, offending_ie_value);
}
