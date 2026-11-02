/*
 * 5G-MAG Reference Tools
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
#include "mbs-n3mb.h"
#include "mbs-subsc.h"
#include "n4mb-build.h"
#include "ngap-build.h"
#include "pfcp-path.h"

char *smf_mbs_ran_id_text(OpenAPI_global_ran_node_id_t *ran_id)
{
    cJSON *json;
    char *text;

    if (!ran_id)
        return ogs_strdup("(none)");
    json = OpenAPI_global_ran_node_id_convertToJSON(ran_id);
    if (!json)
        return ogs_strdup("(none)");
    text = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    /* cJSON allocates with its own allocator, which is ogs's */
    return text;
}

static bool tunnel_of(NGAP_UPTransportLayerInformation_t *info, bool *has_tunnel, ogs_ip_t *addr, uint32_t *teid)
{
    NGAP_GTPTunnel_t *gtp;

    if (!info || info->present != NGAP_UPTransportLayerInformation_PR_gTPTunnel || !info->choice.gTPTunnel)
        return true;    /* no tunnel offered */

    gtp = info->choice.gTPTunnel;
    memset(addr, 0, sizeof(*addr));
    if (ogs_asn_BIT_STRING_to_ip(&gtp->transportLayerAddress, addr) != OGS_OK) {
        ogs_error("MBS Session Setup or Modification Response Transfer: the transport layer address cannot be read");
        return false;
    }
    if (gtp->gTP_TEID.size != 4) {
        ogs_error("MBS Session Setup or Modification Response Transfer: the GTP-TEID has %d octets",
                (int)gtp->gTP_TEID.size);
        return false;
    }
    ogs_asn_OCTET_STRING_to_uint32(&gtp->gTP_TEID, teid);
    *has_tunnel = true;
    return true;
}

/* The shared NG-U unicast tunnel of an MBS Session TNL Information NG-RAN (TS 38.413 clause 9.3.2.17); for a location
 * dependent session the item of the Area Session ID. *has_tunnel stays false when it carries none. */
static bool tunnel_of_tnl_info(NGAP_MBS_SessionTNLInfoNGRAN_t *tnl, bool has_area_session_id,
        uint16_t area_session_id, bool *has_tunnel, ogs_ip_t *addr, uint32_t *teid)
{
    bool ok = true;
    int i;

    if (tnl && tnl->present == NGAP_MBS_SessionTNLInfoNGRAN_PR_locationindependent) {
        ok = tunnel_of(tnl->choice.locationindependent, has_tunnel, addr, teid);
    } else if (tnl && tnl->present == NGAP_MBS_SessionTNLInfoNGRAN_PR_locationdependent &&
            tnl->choice.locationdependent && has_area_session_id) {
        /* one item for each Area Session ID: the item of this session's part */
        for (i = 0; i < tnl->choice.locationdependent->list.count; i++) {
            NGAP_MBS_SessionTNLInfoNGRANItem_t *item = tnl->choice.locationdependent->list.array[i];

            if (item && item->mBS_AreaSessionID == area_session_id) {
                ok = tunnel_of(item->sharedNGU_UnicastTNLInformation, has_tunnel, addr, teid);
                break;
            }
        }
    }
    return ok;
}

bool smf_mbs_n3mb_parse_response_transfer(ogs_pkbuf_t *pkbuf, bool has_area_session_id, uint16_t area_session_id,
        bool *has_tunnel, ogs_ip_t *addr, uint32_t *teid)
{
    NGAP_MBSSessionSetupOrModResponseTransfer_t transfer;
    bool ok = true;

    ogs_assert(pkbuf);
    ogs_assert(has_tunnel);
    ogs_assert(addr);
    ogs_assert(teid);

    *has_tunnel = false;
    memset(&transfer, 0, sizeof(transfer));
    if (ogs_asn_decode(&asn_DEF_NGAP_MBSSessionSetupOrModResponseTransfer, &transfer, sizeof(transfer), pkbuf) !=
            OGS_OK) {
        ogs_error("MBS Session Setup or Modification Response Transfer: cannot decode");
        return false;
    }

    ok = tunnel_of_tnl_info(transfer.mBS_SessionTNLInfoNGRAN, has_area_session_id, area_session_id, has_tunnel, addr,
            teid);

    ogs_asn_free(&asn_DEF_NGAP_MBSSessionSetupOrModResponseTransfer, &transfer);
    return ok;
}

bool smf_mbs_n3mb_parse_release_response_transfer(ogs_pkbuf_t *pkbuf, bool has_area_session_id,
        uint16_t area_session_id, bool *has_tunnel, ogs_ip_t *addr, uint32_t *teid)
{
    NGAP_MBSSessionReleaseResponseTransfer_t transfer;
    bool ok = true;

    ogs_assert(pkbuf);
    ogs_assert(has_tunnel);
    ogs_assert(addr);
    ogs_assert(teid);

    *has_tunnel = false;
    memset(&transfer, 0, sizeof(transfer));
    if (ogs_asn_decode(&asn_DEF_NGAP_MBSSessionReleaseResponseTransfer, &transfer, sizeof(transfer), pkbuf) !=
            OGS_OK) {
        ogs_error("MBS Session Release Response Transfer: cannot decode");
        return false;
    }

    /* "MBS Session TNL Information NG-RAN | O" (TS 38.413 V18.8.0 clause 9.3.5.14) */
    if (transfer.mBS_SessionTNLInfoNGRAN)
        ok = tunnel_of_tnl_info(transfer.mBS_SessionTNLInfoNGRAN, has_area_session_id, area_session_id, has_tunnel,
                addr, teid);

    ogs_asn_free(&asn_DEF_NGAP_MBSSessionReleaseResponseTransfer, &transfer);
    return ok;
}

bool smf_mbs_n3mb_parse_failure_transfer(ogs_pkbuf_t *pkbuf, int *group, long *value)
{
    NGAP_MBSSessionSetupOrModFailureTransfer_t transfer;

    ogs_assert(pkbuf);
    ogs_assert(group);
    ogs_assert(value);

    memset(&transfer, 0, sizeof(transfer));
    if (ogs_asn_decode(&asn_DEF_NGAP_MBSSessionSetupOrModFailureTransfer, &transfer, sizeof(transfer), pkbuf) !=
            OGS_OK) {
        ogs_error("MBS Session Setup or Modification Failure Transfer: cannot decode");
        return false;
    }

    *group = transfer.cause.present;
    switch (transfer.cause.present) {
    case NGAP_Cause_PR_radioNetwork: *value = transfer.cause.choice.radioNetwork; break;
    case NGAP_Cause_PR_transport: *value = transfer.cause.choice.transport; break;
    case NGAP_Cause_PR_nas: *value = transfer.cause.choice.nas; break;
    case NGAP_Cause_PR_protocol: *value = transfer.cause.choice.protocol; break;
    case NGAP_Cause_PR_misc: *value = transfer.cause.choice.misc; break;
    default: *value = -1; break;
    }

    ogs_asn_free(&asn_DEF_NGAP_MBSSessionSetupOrModFailureTransfer, &transfer);
    return true;
}

/* The N3mb DL tunnel of the NG-RAN node \p ran_id, taken in; the change for the MB-UPF is recorded in
 * \p tunnel_changes when there is room. Returns 1 when the tunnel is new or another one, else 0. */
static int set_dl_tunnel(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, const char *ran_id, const ogs_ip_t *addr,
        uint32_t teid, smf_mbs_tunnel_changes_t *tunnel_changes)
{
    smf_mbs_dl_tunnel_t *old = smf_mbs_sess_dl_tunnel_find(mbs_sess, ran_id);
    smf_mbs_dl_tunnel_t *tunnel = NULL;
    smf_mbs_tunnel_change_t *tc = NULL;
    uint16_t old_id = old ? old->unicast_params_id : 0;
    bool changed = false;

    tunnel = smf_mbs_sess_dl_tunnel_set(mbs_sess, ran_id, addr, teid, &changed);
    /* the AMF through which the NG-RAN node serves the session is the one a later ContextUpdate for it goes to */
    tunnel->amf_id = amf ? amf->id : 0;
    ogs_info("NG-RAN [%s] N3mb DL tunnel TEID[0x%x]%s", ran_id, teid, changed ? "" : " (unchanged)");
    if (!changed)
        return 0;

    /* another tunnel of a node that had one is a new one for the MB-UPF: it gets a new ID, and the old one is
     * removed with it */
    if (old)
        tunnel->unicast_params_id = ++mbs_sess->next_dl_tunnel_id;
    if (tunnel_changes && tunnel_changes->count < SMF_MBS_MAX_TUNNEL_CHANGES) {
        tc = &tunnel_changes->change[tunnel_changes->count++];
        tc->has_add = true;
        tc->add_addr = tunnel->addr;
        tc->add_teid = tunnel->teid;
        tc->add_id = tunnel->unicast_params_id;
        if (old_id) {
            tc->has_remove = true;
            tc->remove_id = old_id;
        }
    }
    return 1;
}

/* The DL tunnel \p tunnel is gone: removed from the session, and the change recorded for the MB-UPF. Returns 1. */
static int remove_dl_tunnel(smf_mbs_sess_t *mbs_sess, smf_mbs_dl_tunnel_t *tunnel,
        smf_mbs_tunnel_changes_t *tunnel_changes)
{
    if (tunnel_changes && tunnel_changes->count < SMF_MBS_MAX_TUNNEL_CHANGES) {
        smf_mbs_tunnel_change_t *tc = &tunnel_changes->change[tunnel_changes->count++];

        tc->has_remove = true;
        tc->remove_id = tunnel->unicast_params_id;
    }
    smf_mbs_sess_dl_tunnel_remove(mbs_sess, tunnel);
    return 1;
}

int smf_mbs_sess_take_amf_answer(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf,
        OpenAPI_operation_status_e operation_status, OpenAPI_list_t *n2_mbs_sm_info_list, ogs_sbi_message_t *recvmsg,
        smf_mbs_tunnel_changes_t *tunnel_changes)
{
    OpenAPI_lnode_t *node;
    int changes = 0;
    smf_mbs_tunnel_change_t *tc = NULL;

    if (tunnel_changes)
        memset(tunnel_changes, 0, sizeof(*tunnel_changes));

    ogs_assert(mbs_sess);
    ogs_assert(recvmsg);

    /* TS 29.518 V18.14.0 table 6.5.6.2.3-1, operationStatus: "This IE shall be present and indicate the completion
     * of the MBS session start operation, if the AMF received the NG-RAN responses from all involved NG-RAN(s)." The
     * session has started when every AMF that holds a context says so (TS 23.247 V18.8.0 cl. 7.3.5: "the MB-SMF may
     * wait until it has received a Namf_MBSBroadcast_ContextCreate Response or Namf_MBSBroadcast_
     * ContextStatusNotify with the indication of the completion of the operation from each AMF ... before
     * determining that the Broadcast MBS session has been started"). */
    if (amf && operation_status == OpenAPI_operation_status_MBS_SESSION_START_COMPLETE)
        amf->start_complete = true;
    if (!mbs_sess->started && amf && operation_status == OpenAPI_operation_status_MBS_SESSION_START_COMPLETE) {
        smf_mbs_amf_t *other = NULL;
        bool all = true;

        ogs_list_for_each(&mbs_sess->amf_list, other)
            if (other->mbs_context_ref && !other->start_complete)
                all = false;
        if (all) {
            mbs_sess->started = true;
            if (ogs_strcasecmp(mbs_sess->service_type, "BROADCAST") == 0)
                smf_mbs_sess_report_broadcast_delivery_status(mbs_sess, OpenAPI_broadcast_delivery_status_STARTED);
        }
    }

    if (!n2_mbs_sm_info_list)
        return 0;

    OpenAPI_list_for_each(n2_mbs_sm_info_list, node) {
        OpenAPI_n2_mbs_sm_info_t *info = node->data;
        char *ran_id = NULL;
        ogs_pkbuf_t *pkbuf = NULL;
        smf_mbs_dl_tunnel_t *tunnel = NULL;

        if (!info)
            continue;
        ran_id = smf_mbs_ran_id_text(info->ran_id);
        if (info->ngap_data && info->ngap_data->content_id)
            pkbuf = ogs_sbi_find_part_by_content_id(recvmsg, info->ngap_data->content_id);

        switch (info->ngap_ie_type) {
        case OpenAPI_ngap_ie_type_MBS_SES_RSP: {
            bool has_tunnel = false;
            ogs_ip_t addr;
            uint32_t teid = 0;

            if (!pkbuf) {
                ogs_warn("MBS Session Setup or Modification Response Transfer of NG-RAN [%s] is not in the message",
                        ran_id);
                break;
            }
            if (!smf_mbs_n3mb_parse_response_transfer(pkbuf, mbs_sess->location_dependent, mbs_sess->area_session_id,
                        &has_tunnel, &addr, &teid))
                break;
            if (!has_tunnel) {
                ogs_debug("NG-RAN [%s] offers no N3mb unicast tunnel", ran_id);
                break;
            }
            changes += set_dl_tunnel(mbs_sess, amf, ran_id, &addr, teid, tunnel_changes);
            break;
        }
        case OpenAPI_ngap_ie_type_MBS_SES_FAIL: {
            int group = 0;
            long value = 0;

            if (pkbuf && smf_mbs_n3mb_parse_failure_transfer(pkbuf, &group, &value))
                ogs_error("NG-RAN [%s] could not set up or modify the MBS session: cause group [%d] value [%ld]",
                        ran_id, group, value);
            else
                ogs_error("NG-RAN [%s] could not set up or modify the MBS session", ran_id);
            tunnel = smf_mbs_sess_dl_tunnel_find(mbs_sess, ran_id);
            if (tunnel) {
                if (tunnel_changes && tunnel_changes->count < SMF_MBS_MAX_TUNNEL_CHANGES) {
                    tc = &tunnel_changes->change[tunnel_changes->count++];
                    tc->has_remove = true;
                    tc->remove_id = tunnel->unicast_params_id;
                }
                smf_mbs_sess_dl_tunnel_remove(mbs_sess, tunnel);
                changes++;
            }
            break;
        }
        case OpenAPI_ngap_ie_type_MBS_SES_REL_RSP: {
            bool has_tunnel = false;
            ogs_ip_t addr;
            uint32_t teid = 0;
            smf_mbs_dl_tunnel_t *indicated = NULL;

            /* The NG-RAN released the session: its tunnel is gone (TS 23.247 cl. 7.3.2 step 7). TS 29.518 V18.14.0
             * clause 6.5.6.4.2.2, note of table 6.5.6.4.2.2-1: "An MBS Session Release Response Transfer IE shall
             * only be sent to the MB-SMF during a Broadcast MBS Session Release Require procedure (see clause 7.3.6
             * of TS 23.247 [55]), when unicast transport applies over N3mb, to transfer the DL F-TEID of the NG-RAN
             * node in which the MBS session has been released and towards which the delivery of MBS data shall be
             * stopped." So the tunnel the transfer names is stopped as well as the one held for that NG-RAN. */
            memset(&addr, 0, sizeof(addr));
            if (pkbuf && !smf_mbs_n3mb_parse_release_response_transfer(pkbuf, mbs_sess->location_dependent,
                        mbs_sess->area_session_id, &has_tunnel, &addr, &teid))
                ogs_warn("MBS Session Release Response Transfer of NG-RAN [%s] cannot be read", ran_id);
            if (has_tunnel) {
                ogs_list_for_each(&mbs_sess->dl_tunnel_list, tunnel) {
                    if (tunnel->teid == teid && tunnel->addr.ipv4 == addr.ipv4 && tunnel->addr.ipv6 == addr.ipv6 &&
                            (addr.ipv4 ? tunnel->addr.addr == addr.addr :
                             memcmp(tunnel->addr.addr6, addr.addr6, sizeof(addr.addr6)) == 0)) {
                        indicated = tunnel;
                        break;
                    }
                }
                if (!indicated)
                    ogs_warn("MBS Session Release Response Transfer of NG-RAN [%s] names a DL tunnel this MB-SMF "
                            "does not hold (TEID 0x%x)", ran_id, teid);
            }
            tunnel = smf_mbs_sess_dl_tunnel_find(mbs_sess, ran_id);
            if (indicated) {
                changes += remove_dl_tunnel(mbs_sess, indicated, tunnel_changes);
                if (indicated == tunnel)
                    tunnel = NULL;
            }
            if (tunnel)
                changes += remove_dl_tunnel(mbs_sess, tunnel, tunnel_changes);
            break;
        }
        default:
            ogs_warn("N2 MBS Session Management container of type [%s] from NG-RAN [%s] is not handled",
                    OpenAPI_ngap_ie_type_ToString(info->ngap_ie_type), ran_id);
            break;
        }
        ogs_free(ran_id);
    }

    return changes;
}

/* The answer to one Broadcast Session Transport Request Transfer container (TS 38.413 clauses 8.17.5, 9.3.5.15 to
 * 9.3.5.17): the Response Transfer when the shared NG-U transport is taken in, the Failure Transfer with a Cause when
 * it cannot be. */
static bool transport_answer_add(smf_mbs_transport_answer_t *answers, int *count, int max,
        OpenAPI_global_ran_node_id_t *ran_id, ogs_pkbuf_t *transfer, bool failure)
{
    if (!transfer || *count >= max) {
        if (transfer)
            ogs_pkbuf_free(transfer);
        return false;
    }
    answers[*count].ran_id = ran_id;
    answers[*count].transfer = transfer;
    answers[*count].failure = failure;
    (*count)++;
    return true;
}

int smf_mbs_sess_answer_transport_requests(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf,
        OpenAPI_list_t *n2_mbs_sm_info_list, ogs_sbi_message_t *recvmsg, smf_mbs_tunnel_changes_t *tunnel_changes, smf_mbs_transport_answer_t *answers,
        int max_answers)
{
    OpenAPI_lnode_t *node;
    int count = 0;

    ogs_assert(mbs_sess);
    ogs_assert(recvmsg);
    ogs_assert(answers);

    if (tunnel_changes)
        memset(tunnel_changes, 0, sizeof(*tunnel_changes));
    if (!n2_mbs_sm_info_list)
        return 0;

    OpenAPI_list_for_each(n2_mbs_sm_info_list, node) {
        OpenAPI_n2_mbs_sm_info_t *info = node->data;
        NGAP_BroadcastTransportRequestTransfer_t transfer;
        ogs_pkbuf_t *pkbuf = NULL;
        char *ran_id = NULL;
        bool has_tunnel = false, decoded = false;
        ogs_ip_t addr;
        uint32_t teid = 0;
        NGAP_Cause_PR group = NGAP_Cause_PR_protocol;
        long cause = NGAP_CauseProtocol_transfer_syntax_error;
        ogs_tmgi_t transfer_tmgi;

        if (!info || info->ngap_ie_type != OpenAPI_ngap_ie_type_BC_TRA_REQ)
            continue;

        ran_id = smf_mbs_ran_id_text(info->ran_id);
        memset(&transfer, 0, sizeof(transfer));
        memset(&transfer_tmgi, 0, sizeof(transfer_tmgi));
        if (info->ngap_data && info->ngap_data->content_id)
            pkbuf = ogs_sbi_find_part_by_content_id(recvmsg, info->ngap_data->content_id);

        if (!pkbuf) {
            ogs_warn("Broadcast Session Transport Request Transfer of NG-RAN [%s] is not in the message", ran_id);
        } else if (ogs_asn_decode(&asn_DEF_NGAP_BroadcastTransportRequestTransfer, &transfer, sizeof(transfer),
                    pkbuf) != OGS_OK) {
            ogs_error("Broadcast Session Transport Request Transfer of NG-RAN [%s]: cannot decode", ran_id);
        } else {
            decoded = transfer.mBS_SessionID.tMGI.size == 6;   /* TMGI ::= OCTET STRING (SIZE(6)) */
            if (decoded)
                ogs_ngap_ASN_to_5gs_tmgi(&transfer.mBS_SessionID.tMGI, &transfer_tmgi);
            if (!decoded) {
                ogs_error("Broadcast Session Transport Request Transfer of NG-RAN [%s]: the TMGI is not 6 octets",
                        ran_id);
            } else if (!transfer_tmgi.mbs_service_id || !mbs_sess->tmgi ||
                    ogs_strcasecmp(transfer_tmgi.mbs_service_id, mbs_sess->tmgi->mbs_service_id) != 0 ||
                    memcmp(&transfer_tmgi.plmn_id, &mbs_sess->tmgi->plmn_id, sizeof(transfer_tmgi.plmn_id)) != 0) {
                /* TS 38.413 clause 9.3.1.2: "Unknown MBS Session ID | The action failed because the MBS Session ID
                 * is unknown." */
                ogs_warn("Broadcast Session Transport Request Transfer of NG-RAN [%s]: another MBS Session ID", ran_id);
                group = NGAP_Cause_PR_radioNetwork;
                cause = NGAP_CauseRadioNetwork_unknown_MBS_Session_ID;
                decoded = false;
            } else if (!tunnel_of_tnl_info(&transfer.mBS_SessionTNLInfoNGRAN, mbs_sess->location_dependent,
                        mbs_sess->area_session_id, &has_tunnel, &addr, &teid)) {
                decoded = false;    /* a transport layer address or TEID that cannot be read: a syntax error */
            }
        }

        if (decoded) {
            /* TS 38.413 clause 8.17.5.2: "If the Shared NG-U Unicast TNL Information IE is included in the MBS
             * Session TNL Information NG-RAN IE within the MBS Session Transport Request Transfer IE in the
             * BROADCAST SESSION TRANSPORT REQUEST message, the MB-SMF shall use the included information as the
             * downlink termination point for the shared NG-U transport." */
            if (has_tunnel)
                set_dl_tunnel(mbs_sess, amf, ran_id, &addr, teid, tunnel_changes);
            else
                ogs_debug("NG-RAN [%s] offers no N3mb unicast tunnel in its Broadcast Session Transport Request",
                        ran_id);
            transport_answer_add(answers, &count, max_answers, info->ran_id,
                    ngap_build_broadcast_transport_response_transfer(mbs_sess->tmgi), false);
        } else {
            transport_answer_add(answers, &count, max_answers, info->ran_id,
                    ngap_build_broadcast_transport_failure_transfer(mbs_sess->tmgi, group, cause), true);
        }

        if (pkbuf)
            ogs_asn_free(&asn_DEF_NGAP_BroadcastTransportRequestTransfer, &transfer);
        if (transfer_tmgi.mbs_service_id)
            ogs_free(transfer_tmgi.mbs_service_id);
        ogs_free(ran_id);
    }

    return count;
}

int smf_mbs_sess_send_tunnel_changes(smf_mbs_sess_t *mbs_sess, const smf_mbs_tunnel_changes_t *changes)
{
    int i, failures = 0;

    ogs_assert(mbs_sess);
    ogs_assert(changes);

    /* The PFCP session of an MBS session whose restoration after an MB-UPF restart is pending is not at the MB-UPF
     * yet (TS 23.527 V18.6.0 clause 8.2.2: the MB-SMF "may start restoring the PFCP sessions ... by following PFCP
     * requirements for establishing a PFCP session"), so the changes wait for it. */
    if (mbs_sess->restoring) {
        smf_mbs_tunnel_changes_t *deferred = &mbs_sess->deferred_tunnel_changes;

        for (i = 0; i < changes->count; i++) {
            if (deferred->count >= SMF_MBS_MAX_TUNNEL_CHANGES) {
                ogs_error("mbsSessionRef[%s]: more than %d N3mb DL tunnel changes while the restoration at the MB-UPF "
                        "is pending, [%d] of them are lost", mbs_sess->mbs_session_ref, SMF_MBS_MAX_TUNNEL_CHANGES,
                        changes->count - i);
                return changes->count - i;
            }
            deferred->change[deferred->count++] = changes->change[i];
        }
        return 0;
    }

    for (i = 0; i < changes->count; i++) {
        const smf_mbs_tunnel_change_t *change = &changes->change[i];
        smf_n4mb_modification_t mod;
        ogs_pfcp_far_t *far = NULL;
        bool unicast = !ogs_list_empty(&mbs_sess->dl_tunnel_list);

        memset(&mod, 0, sizeof(mod));
        mod.tunnel = change;

        /* TS 29.244 V18.10.0 cl. 5.34.2.2: "the MB-SMF shall send a PFCP Session Modification Request to change the
         * FAR by setting the Apply Action to "MBSU", together with one or more Add MBS Unicast Parameters". The flag
         * is set with the first tunnel given and cleared with the last one taken away. */
        ogs_list_for_each(&mbs_sess->pfcp.far_list, far) {
            if (change->has_add && !(far->apply_action & OGS_PFCP_APPLY_ACTION_MBSU)) {
                far->apply_action |= OGS_PFCP_APPLY_ACTION_MBSU;
                mod.update_apply_action = true;
            } else if (!change->has_add && !unicast && i == changes->count - 1 &&
                    (far->apply_action & OGS_PFCP_APPLY_ACTION_MBSU)) {
                far->apply_action &= ~OGS_PFCP_APPLY_ACTION_MBSU;
                mod.update_apply_action = true;
            }
        }

        if (smf_5gc_pfcp_n4mb_send_session_modification_request(mbs_sess, &mod) != OGS_OK) {
            ogs_error("The MB-UPF could not be told of the N3mb DL tunnel change [%d] of mbsSessionRef[%s]", i,
                    mbs_sess->mbs_session_ref);
            failures++;
        }
    }

    return failures;
}
