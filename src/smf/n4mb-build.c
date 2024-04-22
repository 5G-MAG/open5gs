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

ogs_pkbuf_t *smf_n4mb_build_session_establishment_request(
        smf_mbs_sess_t *mbs_sess, ogs_pfcp_xact_t *xact)
{
    ogs_pfcp_message_t *pfcp_message = NULL;
    ogs_pfcp_session_establishment_request_t *req = NULL;
    ogs_pkbuf_t *pkbuf = NULL;

    ogs_pfcp_pdr_t *pdr = NULL;
    ogs_pfcp_far_t *far = NULL;
    ogs_pfcp_qer_t *qer = NULL;
    int i, rv;

    ogs_pfcp_node_id_t node_id;
    ogs_pfcp_f_seid_t f_seid;
    char apn_dnn[OGS_MAX_DNN_LEN+1];
    int len;
    ogs_pfcp_mbs_session_identifier_t mbs_session_identifier;
    uint8_t mbs_session_identifier_buf[sizeof(ogs_pfcp_mbs_session_identifier_t)];
    ogs_pfcp_mbsn4mbreq_flags_t mbsn4mbreq_flags;

    ogs_debug("N4mb Session Establishment Request");

    ogs_assert(mbs_sess);
    ogs_assert(xact);

    pfcp_message = ogs_calloc(1, sizeof(*pfcp_message));
    if (!pfcp_message) {
        ogs_error("ogs_calloc() failed");
        return NULL;
    }

    req = &pfcp_message->pfcp_session_establishment_request;

    /* Node ID */
    rv = ogs_pfcp_sockaddr_to_node_id(&node_id, &len);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_sockaddr_to_node_id() failed");
        ogs_free(pfcp_message);
        return NULL;
    }
    req->node_id.presence = 1;
    req->node_id.data = &node_id;
    req->node_id.len = len;

    /* F-SEID */
    rv = ogs_pfcp_sockaddr_to_f_seid(&f_seid, &len);
    if (rv != OGS_OK) {
        ogs_error("ogs_pfcp_sockaddr_to_f_seid() failed");
        ogs_free(pfcp_message);
        return NULL;
    }
    f_seid.seid = htobe64(mbs_sess->smf_n4mb_seid);
    req->cp_f_seid.presence = 1;
    req->cp_f_seid.data = &f_seid;
    req->cp_f_seid.len = len;

    ogs_pfcp_pdrbuf_init();

    /* Create PDR (IP Multicast Addressing Info inside PDI) */
    i = 0;
    ogs_list_for_each(&mbs_sess->pfcp.pdr_list, pdr) {
        ogs_pfcp_build_create_pdr(&req->create_pdr[i], i, pdr);
        i++;
    }

    /* Create FAR (MBS Multicast Parameters inside) */
    i = 0;
    ogs_list_for_each(&mbs_sess->pfcp.far_list, far) {
        // TODO (borieher): Avoid setting the TEID in the create_far
        ogs_pfcp_build_create_far(&req->create_far[i], i, far);
        i++;
    }

    // NOTE (borieher): For now QER is not being used
    /* Create QER */
    i = 0;
    ogs_list_for_each(&mbs_sess->pfcp.qer_list, qer) {
        ogs_pfcp_build_create_qer(&req->create_qer[i], i, qer);
        i++;
    }

    /* APN/DNN */
    if (mbs_sess->dnn) {
        len = ogs_fqdn_build(apn_dnn, mbs_sess->dnn, strlen(mbs_sess->dnn));
        req->apn_dnn.presence = 1;
        req->apn_dnn.len = len;
        req->apn_dnn.data = apn_dnn;
    }

    /* S-NSSAI */
    if (mbs_sess->s_nssai.sst) {
        req->s_nssai.presence = 1;
        req->s_nssai.len = 4;
        req->s_nssai.data = &mbs_sess->s_nssai;
    }

    // NOTE (borieher): (C), create_traffic_endpoint if UP has PDI optimization

    // NOTE (borieher): (O), user_plane_inactivity_timer

    /* MBS Session N4mb Control Information */
    req->mbs_session_n4mb_control_information.presence = 1;

    // MBS Session ID for MBS Session N4mb Control Information
    memset(&mbs_session_identifier, 0, sizeof(ogs_pfcp_mbs_session_identifier_t));

    if (mbs_sess->mbs_session_id.is_tmgi) {
        mbs_session_identifier.tmgif = 1;
        memcpy(mbs_session_identifier.tmgi.mbs_service_id,
            mbs_sess->mbs_session_id.tmgi->mbs_service_id, OGS_MAX_MBS_SERVICE_ID_LEN);

        // TODO (borieher): Check if I can copy plmn_id[3] directly
        mbs_session_identifier.tmgi.mcc3 = mbs_sess->mbs_session_id.tmgi->plmn_id.mcc3;
        mbs_session_identifier.tmgi.mcc2 = mbs_sess->mbs_session_id.tmgi->plmn_id.mcc2;
        mbs_session_identifier.tmgi.mcc1 = mbs_sess->mbs_session_id.tmgi->plmn_id.mcc1;
        mbs_session_identifier.tmgi.mnc3 = mbs_sess->mbs_session_id.tmgi->plmn_id.mnc3;
        mbs_session_identifier.tmgi.mnc2 = mbs_sess->mbs_session_id.tmgi->plmn_id.mnc2;
        mbs_session_identifier.tmgi.mnc1 = mbs_sess->mbs_session_id.tmgi->plmn_id.mnc1;
    } else if (mbs_sess->mbs_session_id.is_ssm) {
        mbs_session_identifier.ssmif = 1;
        // TODO (borieher): Build SSM as MBS Session ID
    }

    if (mbs_sess->mbs_session_id.nid) {
        mbs_session_identifier.nidif = 1;
        // TODO (borieher): Build NID with MBS Session ID
    }

    req->mbs_session_n4mb_control_information.mbs_session_identifier.presence = 1;
    ogs_pfcp_build_mbs_session_identifier(&req->mbs_session_n4mb_control_information.mbs_session_identifier,
            &mbs_session_identifier, mbs_session_identifier_buf, sizeof(mbs_session_identifier_buf));

    // MBSN4mbReq flags for MBS Session N4mb Control Information
    memset(&mbsn4mbreq_flags, 0, sizeof(ogs_pfcp_mbsn4mbreq_flags_t));

    // If SSM is present, add JMBSSM flag
    if (mbs_sess->mbs_session_id.is_ssm || mbs_sess->ssm) {
        mbsn4mbreq_flags.join_mbs_session_ssm = 1;
    }

    mbsn4mbreq_flags.provide_lower_layer_ssm = 1;

    req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.presence = 1;
    req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.data = &mbsn4mbreq_flags.value;
    req->mbs_session_n4mb_control_information.mbsn4mbreq_flags.len = sizeof(ogs_pfcp_mbsn4mbreq_flags_t);


    pfcp_message->h.type = OGS_PFCP_SESSION_ESTABLISHMENT_REQUEST_TYPE;
    pkbuf = ogs_pfcp_build_msg(pfcp_message);
    ogs_expect(pkbuf);

    ogs_pfcp_pdrbuf_clear();
    ogs_free(pfcp_message);

    return pkbuf;
}
