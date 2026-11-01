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

bool smf_mbs_n3mb_parse_response_transfer(ogs_pkbuf_t *pkbuf, bool has_area_session_id, uint16_t area_session_id,
        bool *has_tunnel, ogs_ip_t *addr, uint32_t *teid)
{
    NGAP_MBSSessionSetupOrModResponseTransfer_t transfer;
    NGAP_MBS_SessionTNLInfoNGRAN_t *tnl;
    bool ok = true;
    int i;

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

    tnl = transfer.mBS_SessionTNLInfoNGRAN;
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

    ogs_asn_free(&asn_DEF_NGAP_MBSSessionSetupOrModResponseTransfer, &transfer);
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

int smf_mbs_sess_take_amf_answer(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf,
        OpenAPI_operation_status_e operation_status, OpenAPI_list_t *n2_mbs_sm_info_list, ogs_sbi_message_t *recvmsg)
{
    OpenAPI_lnode_t *node;
    int changes = 0;

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
        ran_id = smf_mbs_ran_id_text(info->rand_id);
        if (info->ngap_data && info->ngap_data->content_id)
            pkbuf = ogs_sbi_find_part_by_content_id(recvmsg, info->ngap_data->content_id);

        switch (info->ngap_ie_type) {
        case OpenAPI_ngap_ie_type_MBS_SES_RSP: {
            bool has_tunnel = false, changed = false;
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
            tunnel = smf_mbs_sess_dl_tunnel_set(mbs_sess, ran_id, &addr, teid, &changed);
            ogs_info("NG-RAN [%s] N3mb DL tunnel TEID[0x%x]%s", ran_id, teid, changed ? "" : " (unchanged)");
            if (changed)
                changes++;
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
                smf_mbs_sess_dl_tunnel_remove(mbs_sess, tunnel);
                changes++;
            }
            break;
        }
        case OpenAPI_ngap_ie_type_MBS_SES_REL_RSP:
            /* the NG-RAN released the session: its tunnel is gone (TS 23.247 cl. 7.3.2 step 7) */
            tunnel = smf_mbs_sess_dl_tunnel_find(mbs_sess, ran_id);
            if (tunnel) {
                smf_mbs_sess_dl_tunnel_remove(mbs_sess, tunnel);
                changes++;
            }
            break;
        default:
            ogs_warn("N2 MBS Session Management container of type [%s] from NG-RAN [%s] is not handled",
                    OpenAPI_ngap_ie_type_ToString(info->ngap_ie_type), ran_id);
            break;
        }
        ogs_free(ran_id);
    }

    return changes;
}
