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
#include "mbs-restore.h"
#include "mbs-n3mb.h"
#include "sbi-path.h"
#include "event.h"
#include "mbs-subsc.h"
#include "pfcp-path.h"
#include "smf-sm.h"

static ogs_list_t pending_restorations;

/* the ContextUpdate sender, replaceable for a test that has no SBI */
static int (*context_update_sender)(smf_mbs_sess_t *, smf_namf_context_update_param_t *) =
        smf_sbi_send_mbs_broadcast_context_update;

void smf_mbs_set_context_update_sender(int (*sender)(smf_mbs_sess_t *, smf_namf_context_update_param_t *))
{
    context_update_sender = sender ? sender : smf_sbi_send_mbs_broadcast_context_update;
}

/* the ContextCreate sender, replaceable for a test that has no SBI */
static int (*context_create_sender)(smf_mbs_sess_t *) = smf_sbi_send_mbs_broadcast_context_create;

void smf_mbs_set_context_create_sender(int (*sender)(smf_mbs_sess_t *))
{
    context_create_sender = sender ? sender : smf_sbi_send_mbs_broadcast_context_create;
}

/* Whether an AMF is registered at the NRF (the NF instances this MB-SMF knows), replaceable for a test */
static bool real_amf_available(void)
{
    ogs_sbi_nf_instance_t *nf_instance = NULL;

    ogs_list_for_each(&ogs_sbi_self()->nf_instance_list, nf_instance)
        if (nf_instance->nf_type == OpenAPI_nf_type_AMF && nf_instance->id)
            return true;
    return false;
}

static bool (*amf_available_check)(void) = real_amf_available;

void smf_mbs_set_amf_available_check(bool (*check)(void))
{
    amf_available_check = check ? check : real_amf_available;
}

/* The AMF set of an NF instance, and an alternative AMF of the same set, replaceable for a test that has no NRF */
static bool real_amf_set_of(const char *nf_instance_id, smf_mbs_amf_set_t *set)
{
    ogs_sbi_nf_instance_t *nf_instance = ogs_sbi_nf_instance_find((char *)nf_instance_id);
    ogs_sbi_nf_info_t *nf_info = NULL;

    memset(set, 0, sizeof(*set));
    if (!nf_instance || nf_instance->nf_type != OpenAPI_nf_type_AMF)
        return false;
    nf_info = ogs_sbi_nf_info_find(&nf_instance->nf_info_list, OpenAPI_nf_type_AMF);
    if (!nf_info)
        return false;
    set->known = true;
    set->set_id = nf_info->amf.amf_set_id;
    set->region_id = nf_info->amf.amf_region_id;
    if (nf_info->amf.num_of_guami > 0) {
        set->has_plmn = true;
        set->plmn_id = nf_info->amf.guami[0].plmn_id;
    }
    return true;
}

bool smf_mbs_amf_set_equal(const smf_mbs_amf_set_t *a, const smf_mbs_amf_set_t *b)
{
    return a->known && b->known && a->set_id == b->set_id && a->region_id == b->region_id &&
        (!a->has_plmn || !b->has_plmn || memcmp(&a->plmn_id, &b->plmn_id, sizeof(a->plmn_id)) == 0);
}

static const char *real_alternative_amf(const smf_mbs_amf_set_t *set, const char *not_instance_id)
{
    ogs_sbi_nf_instance_t *nf_instance = NULL;

    ogs_list_for_each(&ogs_sbi_self()->nf_instance_list, nf_instance) {
        smf_mbs_amf_set_t other;

        if (nf_instance->nf_type != OpenAPI_nf_type_AMF || !nf_instance->id ||
                nf_instance->nf_status != OpenAPI_nf_status_REGISTERED ||
                strcmp(nf_instance->id, not_instance_id) == 0)
            continue;
        if (real_amf_set_of(nf_instance->id, &other) && smf_mbs_amf_set_equal(set, &other))
            return nf_instance->id;
    }
    return NULL;
}

static bool (*amf_set_of)(const char *, smf_mbs_amf_set_t *) = real_amf_set_of;
static const char *(*alternative_amf)(const smf_mbs_amf_set_t *, const char *) = real_alternative_amf;

void smf_mbs_set_amf_set_lookup(bool (*set_of)(const char *, smf_mbs_amf_set_t *),
        const char *(*alternative)(const smf_mbs_amf_set_t *, const char *))
{
    amf_set_of = set_of ? set_of : real_amf_set_of;
    alternative_amf = alternative ? alternative : real_alternative_amf;
}

void smf_mbs_amf_note_set(smf_mbs_amf_t *amf)
{
    ogs_assert(amf);

    memset(&amf->amf_set, 0, sizeof(amf->amf_set));
    if (amf->nf_instance_id)
        amf_set_of(amf->nf_instance_id, &amf->amf_set);
}

/* The wait before a pending session asks for its context again (smf.mbsContextRetryDelay) */
static ogs_timer_t *context_retry_timer;

static void context_retry_timeout(void *data)
{
    smf_event_t *e = smf_event_new(SMF_EVT_MBS_CONTEXT_RETRY);

    ogs_assert(e);
    if (ogs_queue_push(ogs_app()->queue, e) != OGS_OK) {
        ogs_error("ogs_queue_push() failed in the retry of a broadcast context");
        ogs_event_free(e);
    }
}

bool smf_mbs_context_retry_scheduled(void)
{
    return context_retry_timer && context_retry_timer->running;
}

bool smf_mbs_schedule_context_retry(void)
{
    int seconds = 0;

    if (!smf_mbs_context_retry_delay(&seconds))
        return false;
    if (!context_retry_timer) {
        context_retry_timer = ogs_timer_add(ogs_app()->timer_mgr, context_retry_timeout, NULL);
        ogs_assert(context_retry_timer);
    }
    if (!context_retry_timer->running) {
        ogs_timer_start(context_retry_timer, ogs_time_from_sec(seconds));
        ogs_info("MBS broadcast contexts: asking for the contexts that are still missing again in %d s", seconds);
    }
    return true;
}

void smf_mbs_context_retry_expired(void)
{
    smf_mbs_create_pending_contexts();
}

/* Creates the broadcast context of \p mbs_sess again unless it has an entry (a Create waiting for its answer, or a context
 * of another AMF). The session stays pending until an AMF answers 201 Created. */
static int create_context_again(smf_mbs_sess_t *mbs_sess)
{
    if (!ogs_list_empty(&mbs_sess->amf_list))
        return OGS_OK;

    ogs_info("MBS session mbsSessionRef[%s]: creating its broadcast context in an AMF again", mbs_sess->mbs_session_ref);
    if (context_create_sender(mbs_sess) != OGS_OK) {
        ogs_error("Failed to send MBS Broadcast ContextCreate again");
        smf_mbs_schedule_context_retry();
        return OGS_ERROR;
    }
    return OGS_OK;
}

int smf_mbs_amf_context_lost(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf)
{
    ogs_assert(mbs_sess);
    ogs_assert(amf);

    if (!amf->nf_instance_id) {
        /* A 404 from an AMF that is not known to be the one holding the context says nothing about the context. */
        ogs_warn("MBS session mbsSessionRef[%s]: an AMF has no context mbsContextRef[%s], but the context is not bound "
                "to an AMF instance: kept", mbs_sess->mbs_session_ref, amf->mbs_context_ref ? amf->mbs_context_ref : "(none)");
        return OGS_OK;
    }

    ogs_warn("MBS session mbsSessionRef[%s]: the AMF instance [%s] holds no context mbsContextRef[%s] of it any more",
            mbs_sess->mbs_session_ref, amf->nf_instance_id, amf->mbs_context_ref ? amf->mbs_context_ref : "(none)");
    smf_mbs_sess_amf_remove(mbs_sess, amf);
    mbs_sess->broadcast_context_pending = true;
    return create_context_again(mbs_sess);
}

/* TS 23.527 V18.6.0 cl. 8.3.2.4, alternative B, steps 7 and 8: "The MB-SMF selects an alternative AMF pertaining to the
 * same AMF set using the Binding Indication provided by the old AMF or using the NF profile of the old AMF." and "The
 * MB-SMF sends a Namf_MBSBroadcast_ContextUpdate Request including a MBS Session ID, the corresponding MBS Service
 * Area, a MBS Session Setup or Modification Request Transfer, and sets the "noNgapSignallingInd" to "true" to request
 * the AMF2 to be the AMF for the Broadcast MBS Session". The entry is bound to the alternative AMF, which keeps the
 * mbsContextRef (the context of the set). No clause gives a time to wait for a Namf_MBSBroadcast_ContextStatusNotify of
 * alternative A: none is waited for, and one that arrives later binds the entry to the AMF that sent it. Returns
 * OGS_OK when the request was handed to the SBI. */
static int hand_context_to_alternative(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, const char *alternative_id)
{
    smf_namf_context_update_param_t param;
    char *instance_id = ogs_strdup(alternative_id);

    ogs_assert(instance_id);
    if (amf->nf_instance_id)
        ogs_free(amf->nf_instance_id);
    amf->nf_instance_id = instance_id;
    amf->takeover = true;

    memset(&param, 0, sizeof(param));
    param.amf = amf;
    param.service_area = true;
    param.no_n2_info_change_ind = true;
    param.no_ngap_signalling = true;

    ogs_info("MBS session mbsSessionRef[%s]: the context mbsContextRef[%s] goes to the alternative AMF instance [%s]",
            mbs_sess->mbs_session_ref, amf->mbs_context_ref, alternative_id);
    if (context_update_sender(mbs_sess, &param) != OGS_OK) {
        ogs_error("Failed to send MBS Broadcast ContextUpdate to the alternative AMF");
        amf->takeover = false;
        return OGS_ERROR;
    }
    return OGS_OK;
}

int smf_mbs_amf_instance_lost(const char *nf_instance_id)
{
    smf_mbs_sess_t *mbs_sess = NULL;
    int lost = 0;

    ogs_assert(nf_instance_id);

    /* TS 23.527 V18.6.0 cl. 8.3.2.4: "the MB-SMF detects that the AMF1 has failed ... via notifications from the NRF for
     * the NF Status Change" */
    ogs_list_for_each(&smf_self()->smf_mbs_sess_list, mbs_sess) {
        smf_mbs_amf_t *amf = NULL, *next = NULL;
        bool held = false, dropped = false;

        ogs_list_for_each_safe(&mbs_sess->amf_list, next, amf) {
            const char *alternative_id = NULL;

            if (!amf->nf_instance_id || strcmp(amf->nf_instance_id, nf_instance_id) != 0)
                continue;
            held = true;
            /* a context of an AMF of an AMF set goes to another AMF of the set when one is registered */
            if (amf->mbs_context_ref && amf->amf_set.known)
                alternative_id = alternative_amf(&amf->amf_set, nf_instance_id);
            if (alternative_id && hand_context_to_alternative(mbs_sess, amf, alternative_id) == OGS_OK)
                continue;
            smf_mbs_sess_amf_remove(mbs_sess, amf);
            dropped = true;
        }
        if (held) {
            ogs_warn("MBS session mbsSessionRef[%s]: the AMF instance [%s] is gone, with its broadcast context",
                    mbs_sess->mbs_session_ref, nf_instance_id);
            if (dropped)
                mbs_sess->broadcast_context_pending = true;
            lost++;
        }
    }
    /* An alternative AMF is registered: the contexts are created in it now (TS 23.527 cl. 8.3.2.4 "selects an alternative
     * AMF"). Otherwise they wait for an AMF to register: a Create sent now would only fail. */
    if (lost && amf_available_check())
        smf_mbs_create_pending_contexts();
    return lost;
}

void smf_mbs_amf_takeover_answered(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, bool accepted)
{
    ogs_assert(mbs_sess);
    ogs_assert(amf);

    if (!amf->takeover)
        return;
    if (accepted) {
        amf->takeover = false;
        return;
    }
    /* the alternative AMF does not hold the context, or did not take it: the entry is dropped and the context created
     * again in an AMF */
    ogs_warn("MBS session mbsSessionRef[%s]: the alternative AMF instance [%s] did not take the context mbsContextRef[%s]",
            mbs_sess->mbs_session_ref, amf->nf_instance_id ? amf->nf_instance_id : "(none)",
            amf->mbs_context_ref ? amf->mbs_context_ref : "(none)");
    smf_mbs_amf_context_lost(mbs_sess, amf);
}

void smf_mbs_namf_request_timed_out(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf)
{
    ogs_assert(mbs_sess);

    /* A hand over to an alternative AMF that nothing answered: that AMF holds no context of the session as far as is
     * known, so the context is created again in an AMF. */
    if (amf && amf->takeover) {
        smf_mbs_amf_takeover_answered(mbs_sess, amf, false);
        return;
    }

    /* A Create that nothing answered leaves no context: the entry waiting for it is dropped, as for any other failed
     * Create, and a session that waits for its context asks again; the time between the attempts is the SBI client
     * wait time of the operator. A request about an existing context keeps its entry. */
    if (amf && !amf->mbs_context_ref)
        smf_mbs_sess_amf_remove(mbs_sess, amf);

    if (!mbs_sess->broadcast_context_pending)
        return;
    if (!smf_mbs_schedule_context_retry() && amf_available_check())
        smf_mbs_create_pending_contexts();
}

void smf_mbs_context_create_failed(smf_mbs_sess_t *mbs_sess)
{
    ogs_assert(mbs_sess);

    /* An AMF refused the Create (the mandatory 500: no NG-RAN could carry it yet, TS 29.518 has no code of its own for
     * it): the session keeps waiting for its context and asks again after the operator's time. */
    if (mbs_sess->broadcast_context_pending)
        smf_mbs_schedule_context_retry();
}

int smf_mbs_create_pending_contexts(void)
{
    smf_mbs_sess_t *mbs_sess = NULL;

    /* One Create at a time: after an AMF restarted, requests sent together to it can all meet the connection that died
     * with the old process and fail, while the first one opens a connection the next can use. The next Create goes out
     * when the previous one is answered (smf_namf_handle_mbs_broadcast_context_create_response()) or timed out. */
    ogs_list_for_each(&smf_self()->smf_mbs_sess_list, mbs_sess)
        if (mbs_sess->broadcast_context_pending && !ogs_list_empty(&mbs_sess->amf_list))
            return 0;

    ogs_list_for_each(&smf_self()->smf_mbs_sess_list, mbs_sess) {
        if (mbs_sess->broadcast_context_pending && ogs_list_empty(&mbs_sess->amf_list))
            return create_context_again(mbs_sess) == OGS_OK ? 1 : 0;
    }
    return 0;
}

/* The DL tunnel of the NG-RAN node \p ran_id taken away, the change recorded for the MB-UPF. Returns 1 when there
 * was one. */
static int remove_dl_tunnel_of(smf_mbs_sess_t *mbs_sess, const char *ran_id, smf_mbs_tunnel_changes_t *changes)
{
    smf_mbs_dl_tunnel_t *tunnel = smf_mbs_sess_dl_tunnel_find(mbs_sess, ran_id);

    if (!tunnel)
        return 0;

    if (changes && changes->count < SMF_MBS_MAX_TUNNEL_CHANGES) {
        smf_mbs_tunnel_change_t *tc = &changes->change[changes->count++];

        memset(tc, 0, sizeof(*tc));
        tc->has_remove = true;
        tc->remove_id = tunnel->unicast_params_id;
    }
    smf_mbs_sess_dl_tunnel_remove(mbs_sess, tunnel);
    return 1;
}

/* TS 29.518 V18.14.0 table 6.5.6.2.8-1, amfId: "This IE shall be present to contain the NF Instance ID of the AMF sending
 * the Context Status Notify Request message if the operation event type indicates an AMF change event."; TS 23.527
 * V18.6.0 cl. 8.3.2.4 step 5: "The MB-SMF acknowledges the notification and will send subsequent signalling message
 * for this Broadcast MBS Session via the AMF2." */
bool smf_mbs_sess_take_amf_change_event(smf_mbs_amf_t *amf, const OpenAPI_operation_event_t *event)
{
    char *instance_id = NULL;

    ogs_assert(event);

    if (!amf || !event->amf_id || !*event->amf_id)
        return false;
    instance_id = ogs_strdup(event->amf_id);
    ogs_assert(instance_id);
    if (amf->nf_instance_id)
        ogs_free(amf->nf_instance_id);
    amf->nf_instance_id = instance_id;
    /* the AMF set is that of the AMF that took over; an entry that has one keeps it (the AMF is "in the same AMF set") */
    if (!amf->amf_set.known)
        smf_mbs_amf_note_set(amf);
    return true;
}

int smf_mbs_sess_take_ngran_failure_event(smf_mbs_sess_t *mbs_sess, const OpenAPI_ngran_failure_event_t *event,
        smf_mbs_tunnel_changes_t *changes, smf_mbs_ngran_restore_t *restore)
{
    char *ran_id = NULL;
    int removed = 0;

    ogs_assert(mbs_sess);
    ogs_assert(event);
    ogs_assert(restore);

    memset(restore, 0, sizeof(*restore));

    if (!event->ngran_id) {
        /* table 6.5.6.2.9-1: "ngranId | GlobalRanNodeId | M | 1" */
        ogs_error("MBS Broadcast ContextStatusNotify: an NG-RAN event without the NG-RAN ID");
        return 0;
    }
    ran_id = smf_mbs_ran_id_text((OpenAPI_global_ran_node_id_t *)event->ngran_id);

    switch (event->ngran_failure_indication) {
    case OpenAPI_ngran_failure_indication_NG_RAN_RESTART_OR_START:
        /* TS 23.527 V18.6.0 clause 8.3.2.3, step 5: "The MB-SMF modifies the PFCP sessions corresponding to the
         * affected MBS sessions to remove the NG-RAN DL F-TEID for unicast transport over N3mb." and step 6: "The
         * MB-SMF sends Namf_MBSBroadcast_ContextUpdate Request message to the AMF to (re)start the MBS session in
         * the related NG-RAN including the MBS Session Setup or Modification Request Transfer IE and the NG-RAN ID
         * received in the step 3." */
        removed = remove_dl_tunnel_of(mbs_sess, ran_id, changes);
        restore->restore = true;
        restore->ran_id = ran_id;
        ran_id = NULL;
        break;

    case OpenAPI_ngran_failure_indication_NG_RAN_FAILURE_WITHOUT_RESTART:
        /* clause 8.3.3, step 5: "The MB-SMF modifies the PFCP sessions corresponding to the affected MBS sessions to
         * remove the NG-RAN DL F-TEID in case NG-RAN failure without restart for unicast transport over N3mb." */
        removed = remove_dl_tunnel_of(mbs_sess, ran_id, changes);
        break;

    case OpenAPI_ngran_failure_indication_NG_RAN_REQUIRED_RELEASE:
        /* TS 29.518 V18.14.0 clause 5.6.2.5: "Upon receiving such a notification, per local policies, the MB-SMF
         * may attempt to re-establish the MBS session after some operator configurable time in these NG-RANs by
         * performing the Broadcast MBS session restoration by MB-SMF procedure specified in clause 8.3.2.3 of
         * TS 23.527". */
        restore->restore_later = true;
        restore->ran_id = ran_id;
        ran_id = NULL;
        break;

    default:
        /* NG_RAN_NOT_REACHABLE: no clause of TS 23.527 clause 8 read names an action of the MB-SMF for it */
        ogs_warn("MBS Broadcast ContextStatusNotify: NG-RAN [%s] event [%s]", ran_id,
                OpenAPI_ngran_failure_indication_ToString(event->ngran_failure_indication));
        break;
    }

    if (ran_id)
        ogs_free(ran_id);
    return removed;
}

int smf_mbs_sess_send_ngran_restoration(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, const char *ran_id)
{
    smf_namf_context_update_param_t param;
    const char *ran_ids[1] = { ran_id };

    ogs_assert(mbs_sess);
    ogs_assert(ran_id);

    if (!amf || !amf->mbs_context_ref) {
        ogs_error("MBS session mbsSessionRef[%s]: NG-RAN [%s] cannot be restored, the AMF holds no context of the "
                "session", mbs_sess->mbs_session_ref, ran_id);
        return OGS_ERROR;
    }

    memset(&param, 0, sizeof(param));
    param.amf = amf;
    param.no_n2_info_change_ind = true;
    param.ran_ids = ran_ids;
    param.num_ran_ids = 1;

    ogs_info("MBS session mbsSessionRef[%s]: restoring it in NG-RAN [%s]", mbs_sess->mbs_session_ref, ran_id);

    /* ogs_sbi_xact_add() builds the request before it returns (lib/sbi/context.c), and keeps no pointer to param */
    if (smf_sbi_send_mbs_broadcast_context_update(mbs_sess, &param) != OGS_OK) {
        ogs_error("Failed to send MBS Broadcast ContextUpdate to restore an NG-RAN");
        return OGS_ERROR;
    }
    return OGS_OK;
}

static void restoration_remove(smf_mbs_ngran_restoration_t *restoration)
{
    ogs_list_remove(&pending_restorations, restoration);
    if (restoration->timer)
        ogs_timer_delete(restoration->timer);
    if (restoration->ran_id)
        ogs_free(restoration->ran_id);
    ogs_free(restoration);
}

/* the timer thread: the work is done by the SMF's own thread */
static void restoration_timeout(void *data)
{
    smf_event_t *e = smf_event_new(SMF_EVT_MBS_NGRAN_RESTORE);

    ogs_assert(e);
    e->mbs_ngran_restoration = data;
    if (ogs_queue_push(ogs_app()->queue, e) != OGS_OK) {
        ogs_error("ogs_queue_push() failed in the restoration of an NG-RAN");
        ogs_event_free(e);
    }
}

int smf_mbs_sess_schedule_ngran_restoration(smf_mbs_sess_t *mbs_sess, smf_mbs_amf_t *amf, const char *ran_id)
{
    smf_mbs_ngran_restoration_t *restoration = NULL;
    int seconds = 0;

    ogs_assert(mbs_sess);
    ogs_assert(amf);
    ogs_assert(ran_id);

    if (!smf_mbs_ngran_restore_delay(&seconds)) {
        ogs_info("MBS session mbsSessionRef[%s]: NG-RAN [%s] required the release; no smf.ngranRestoreDelay is "
                "configured, so the session is not restored there", mbs_sess->mbs_session_ref, ran_id);
        return 0;
    }

    ogs_list_for_each(&pending_restorations, restoration)
        if (restoration->mbs_sess_id == mbs_sess->id && restoration->amf_id == amf->id &&
                strcmp(restoration->ran_id, ran_id) == 0)
            return 0;

    restoration = ogs_calloc(1, sizeof(*restoration));
    ogs_assert(restoration);
    restoration->mbs_sess_id = mbs_sess->id;
    restoration->amf_id = amf->id;
    restoration->ran_id = ogs_strdup(ran_id);
    ogs_assert(restoration->ran_id);
    restoration->timer = ogs_timer_add(ogs_app()->timer_mgr, restoration_timeout, restoration);
    ogs_assert(restoration->timer);
    ogs_list_add(&pending_restorations, restoration);
    ogs_timer_start(restoration->timer, ogs_time_from_sec(seconds));

    ogs_info("MBS session mbsSessionRef[%s]: NG-RAN [%s] required the release; restoring it in %d s",
            mbs_sess->mbs_session_ref, ran_id, seconds);
    return 1;
}

void smf_mbs_ngran_restoration_expired(smf_mbs_ngran_restoration_t *restoration)
{
    smf_mbs_sess_t *mbs_sess = NULL;
    smf_mbs_amf_t *amf = NULL;

    ogs_assert(restoration);

    mbs_sess = smf_mbs_sess_find_by_id(restoration->mbs_sess_id);
    if (!mbs_sess) {
        ogs_info("Restoration of NG-RAN [%s]: the MBS session is gone", restoration->ran_id);
    } else if (mbs_sess->release_triggered) {
        ogs_info("Restoration of NG-RAN [%s]: the MBS session mbsSessionRef[%s] is being released",
                restoration->ran_id, mbs_sess->mbs_session_ref);
    } else {
        amf = smf_mbs_sess_amf_find(mbs_sess, restoration->amf_id);
        if (amf)
            smf_mbs_sess_send_ngran_restoration(mbs_sess, amf, restoration->ran_id);
        else
            ogs_info("Restoration of NG-RAN [%s]: the AMF of the MBS session mbsSessionRef[%s] is gone",
                    restoration->ran_id, mbs_sess->mbs_session_ref);
    }

    restoration_remove(restoration);
}

int smf_mbs_ngran_restorations_pending(void)
{
    return ogs_list_count(&pending_restorations);
}

smf_mbs_ngran_restoration_t *smf_mbs_ngran_restoration_first(void)
{
    return ogs_list_first(&pending_restorations);
}

void smf_mbs_ngran_restoration_remove_all(void)
{
    smf_mbs_ngran_restoration_t *restoration = NULL, *next = NULL;

    ogs_list_for_each_safe(&pending_restorations, next, restoration)
        restoration_remove(restoration);
}

bool smf_mbs_decode_remote_gtp_u_peer(const uint8_t *data, int len, ogs_ip_t *peer)
{
    int offset = 1;
    bool v6, v4;

    ogs_assert(peer);
    memset(peer, 0, sizeof(*peer));

    if (!data || len < 1)
        return false;
    v6 = data[0] & 0x01;
    v4 = data[0] & 0x02;
    if (v4) {
        if (len < offset + OGS_IPV4_LEN)
            return false;
        peer->ipv4 = 1;
        memcpy(&peer->addr, data + offset, OGS_IPV4_LEN);
        offset += OGS_IPV4_LEN;
    }
    if (v6) {
        if (len < offset + OGS_IPV6_LEN)
            return false;
        peer->ipv6 = 1;
        memcpy(peer->addr6, data + offset, OGS_IPV6_LEN);
    }
    if (!peer->ipv4 && !peer->ipv6)
        return false;
    peer->len = (peer->ipv4 ? OGS_IPV4_LEN : 0) + (peer->ipv6 ? OGS_IPV6_LEN : 0);
    return true;
}

bool smf_mbs_node_has_sessions(ogs_pfcp_node_t *node)
{
    smf_mbs_sess_t *mbs_sess = NULL;

    ogs_assert(node);

    ogs_list_for_each(&smf_self()->smf_mbs_sess_list, mbs_sess)
        if (mbs_sess->pfcp_node == node)
            return true;
    return false;
}

smf_mbs_dl_tunnel_t *smf_mbs_sess_dl_tunnel_find_by_peer(smf_mbs_sess_t *mbs_sess, const ogs_ip_t *peer)
{
    smf_mbs_dl_tunnel_t *tunnel = NULL;

    ogs_assert(mbs_sess);
    ogs_assert(peer);

    ogs_list_for_each(&mbs_sess->dl_tunnel_list, tunnel) {
        if ((peer->ipv4 && tunnel->addr.ipv4 && peer->addr == tunnel->addr.addr) ||
            (peer->ipv6 && tunnel->addr.ipv6 && memcmp(peer->addr6, tunnel->addr.addr6, OGS_IPV6_LEN) == 0))
            return tunnel;
    }
    return NULL;
}

int smf_mbs_node_report_path_failure(ogs_pfcp_node_t *node, const ogs_ip_t *peer)
{
    smf_mbs_sess_t *mbs_sess = NULL, *next = NULL;
    int sent = 0;

    ogs_assert(node);
    ogs_assert(peer);

    ogs_list_for_each_safe(&smf_self()->smf_mbs_sess_list, next, mbs_sess) {
        smf_mbs_dl_tunnel_t *tunnel = NULL;
        smf_mbs_amf_t *amf = NULL;
        smf_namf_context_update_param_t param;
        const char *ran_ids[1];

        if (mbs_sess->pfcp_node != node || mbs_sess->release_triggered)
            continue;
        tunnel = smf_mbs_sess_dl_tunnel_find_by_peer(mbs_sess, peer);
        if (!tunnel)
            continue;

        amf = smf_mbs_sess_amf_find(mbs_sess, tunnel->amf_id);
        if (!amf || !amf->mbs_context_ref) {
            ogs_error("MBS session mbsSessionRef[%s]: N3mb path failure towards NG-RAN [%s], but the AMF that gave "
                    "the tunnel holds no context of the session", mbs_sess->mbs_session_ref, tunnel->ran_id);
            continue;
        }

        ran_ids[0] = tunnel->ran_id;
        memset(&param, 0, sizeof(param));
        param.amf = amf;
        param.no_n2_info_change_ind = true;
        param.ran_ids = ran_ids;
        param.num_ran_ids = 1;
        param.n3mb_path_failure = true;

        ogs_warn("MBS session mbsSessionRef[%s]: N3mb path failure towards NG-RAN [%s]", mbs_sess->mbs_session_ref,
                tunnel->ran_id);
        if (context_update_sender(mbs_sess, &param) == OGS_OK)
            sent++;
        else
            ogs_error("Failed to send MBS Broadcast ContextUpdate for an N3mb path failure");
    }

    return sent;
}

void smf_mbs_sess_release_lost(smf_mbs_sess_t *mbs_sess)
{
    ogs_assert(mbs_sess);

    if (mbs_sess->release_triggered)
        return;     /* an explicit release is already in flight */
    mbs_sess->release_triggered = true;
    mbs_sess->restoring = false;

    ogs_error("MBS Session mbsSessionRef[%s] lost at the MB-UPF -- releasing the SMF-side state so its TMGI/SSM can "
            "be reused", mbs_sess->mbs_session_ref);

    /* TS 29.532 V18.6.0 cl. 5.3.2.8.1: "the new broadcast delivery status (e.g. the MBS session has been STARTED or
     * TERMINATED)" */
    if (ogs_strcasecmp(mbs_sess->service_type, "BROADCAST") == 0)
        smf_mbs_sess_report_broadcast_delivery_status(mbs_sess, OpenAPI_broadcast_delivery_status_TERMINATED);

    smf_sbi_send_mbs_broadcast_context_delete_all(mbs_sess);
    smf_mbs_sess_release(mbs_sess);
}

int smf_mbs_sess_restore_n4mb(smf_mbs_sess_t *mbs_sess)
{
    ogs_assert(mbs_sess);
    ogs_assert(mbs_sess->pfcp_node);

    /* a new PFCP session at the MB-UPF: the request goes with SEID 0 (TS 29.244 V18.10.0 clause 7.2.2.4.2) */
    mbs_sess->restoring = true;
    mbs_sess->upf_n4mb_seid = 0;
    ogs_info("MBS Session mbsSessionRef[%s]: restoring its PFCP session at the MB-UPF", mbs_sess->mbs_session_ref);

    if (smf_5gc_pfcp_n4mb_send_session_establishment_request(mbs_sess, 0, NULL) != OGS_OK) {
        mbs_sess->restoring = false;
        return OGS_ERROR;
    }
    return OGS_OK;
}

bool smf_mbs_sess_restoration_response(smf_mbs_sess_t *mbs_sess, uint8_t cause)
{
    ogs_assert(mbs_sess);

    mbs_sess->restoring = false;
    mbs_sess->relocating = false;

    if (cause == OGS_PFCP_CAUSE_REQUEST_ACCEPTED) {
        ogs_info("MBS Session mbsSessionRef[%s] restored at the MB-UPF", mbs_sess->mbs_session_ref);
        return true;
    }

    if (cause == OGS_PFCP_CAUSE_RESTORATION_FAILURE_RESOURCE_NOT_AVAILABLE)
        ogs_error("MBS Session mbsSessionRef[%s]: the MB-UPF cannot restore it, the requested resource is not "
                "available (the restoration for MB-UPF failure without restart is not implemented)",
                mbs_sess->mbs_session_ref);
    else
        ogs_error("MBS Session mbsSessionRef[%s]: restoration at the MB-UPF refused, cause [%d]",
                mbs_sess->mbs_session_ref, cause);

    smf_mbs_sess_release_lost(mbs_sess);
    return false;
}

ogs_pfcp_node_t *smf_mbs_alternative_upf(ogs_pfcp_node_t *failed)
{
    ogs_pfcp_node_t *node = NULL;

    ogs_list_for_each(&ogs_pfcp_self()->pfcp_peer_list, node)
        if (node != failed && OGS_FSM_CHECK(&node->sm, smf_pfcp_state_associated))
            return node;
    return NULL;
}

int smf_mbs_sess_relocate_n4mb(smf_mbs_sess_t *mbs_sess, ogs_pfcp_node_t *alternative)
{
    ogs_assert(mbs_sess);
    ogs_assert(alternative);

    /* "the MB-SMF shall request the alternative MB-UPF to allocate a new N3mb and/or N19mb LL SSM address and C-TEID,
     * if multicast transport is used over N3mb and/or N19mb" (cl. 8.2.3): what the failed MB-UPF gave is forgotten */
    memset(&mbs_sess->ll_ssm, 0, sizeof(mbs_sess->ll_ssm));
    mbs_sess->c_teid = 0;
    if (mbs_sess->upf_n3mb_addr) {
        ogs_freeaddrinfo(mbs_sess->upf_n3mb_addr);
        mbs_sess->upf_n3mb_addr = NULL;
    }
    if (mbs_sess->upf_n3mb_addr6) {
        ogs_freeaddrinfo(mbs_sess->upf_n3mb_addr6);
        mbs_sess->upf_n3mb_addr6 = NULL;
    }
    mbs_sess->upf_n4mb_seid = 0;
    OGS_SETUP_PFCP_NODE(mbs_sess, alternative);
    /* the N3mb address of the session is the address of its MB-UPF, as at the creation */
    {
        ogs_sockaddr_t alternative_addr = smf_pfcp_node_addr(alternative);

        if (alternative_addr.ogs_sa_family == AF_INET)
            ogs_assert(OGS_OK == ogs_copyaddrinfo(&mbs_sess->upf_n3mb_addr, &alternative_addr));
        else if (alternative_addr.ogs_sa_family == AF_INET6)
            ogs_assert(OGS_OK == ogs_copyaddrinfo(&mbs_sess->upf_n3mb_addr6, &alternative_addr));
    }
    mbs_sess->relocating = true;

    ogs_info("MBS Session mbsSessionRef[%s]: establishing its PFCP session at an alternative MB-UPF",
            mbs_sess->mbs_session_ref);
    if (smf_5gc_pfcp_n4mb_send_session_establishment_request(mbs_sess, 0, NULL) != OGS_OK) {
        mbs_sess->relocating = false;
        return OGS_ERROR;
    }
    return OGS_OK;
}

int smf_mbs_node_failed(ogs_pfcp_node_t *failed)
{
    smf_mbs_sess_t *mbs_sess = NULL, *next = NULL;
    ogs_pfcp_node_t *alternative = NULL;
    int sent = 0;

    ogs_assert(failed);

    ogs_list_for_each_safe(&smf_self()->smf_mbs_sess_list, next, mbs_sess) {
        if (mbs_sess->pfcp_node != failed || mbs_sess->release_triggered || mbs_sess->restoring ||
                mbs_sess->relocating)
            continue;
        if (ogs_strcasecmp(mbs_sess->service_type, "BROADCAST") != 0)
            continue;

        alternative = smf_mbs_alternative_upf(failed);
        if (!alternative) {
            ogs_warn("MBS Session mbsSessionRef[%s]: its MB-UPF failed and there is no alternative MB-UPF",
                    mbs_sess->mbs_session_ref);
            continue;
        }
        if (smf_mbs_sess_relocate_n4mb(mbs_sess, alternative) == OGS_OK)
            sent++;
        else
            smf_mbs_sess_release_lost(mbs_sess);
    }

    return sent;
}
