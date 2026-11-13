#include "context.h"
#include "event.h"
#include "timer.h"
#include "path-management.h"

/*
 * A path is a remote GTP-U peer of the unicast N3mb transport of an MBS session (Add MBS Unicast Parameters). TS 29.281
 * cl. 7.2.1: "A GTP-U peer may send an Echo Request on a path to the other GTP-U peer to find out if it is alive ... A path
 * is considered to be in use if at least one ... PDU Session ... uses the path to the other GTP-U peer. When and how often
 * an Echo Request message may be sent is implementation specific but an Echo Request shall not be sent more often than every
 * 60 s on each path. This doesn't prevent resending an Echo Request with the same sequence number according to the
 * T3-RESPONSE timer." Multicast transport has no path: "the path management messages Echo Request and Echo Response ...
 * shall not be used" (cl. 7.1).
 *
 * The Echo Request goes out when the path comes into use and then every upf.mbs.gtpu_path.echo_interval seconds (at least
 * 60). cl. 11: "At the expiry of the timer the request is retransmitted if the total number of request attempts is less
 * than N3REQUESTS times"; cl. 12: T3-RESPONSE is the maximum wait for a response (no value is given), N3-REQUESTS "The
 * recommended value is 5". A request that is not answered after N3-REQUESTS attempts is the failure of the path: the
 * definition of the failure of a path is in TS 23.007 cl. 20.3.1 (cl. 9.2 of TS 29.281 and cl. 8.1B of TS 23.527 refer to
 * it), a document this tree has no copy of.
 */

#define MAX_NUM_OF_PATH 256

typedef struct upf_gtpu_path_s {
    ogs_lnode_t lnode;
    ogs_pool_id_t id;

    ogs_gtp_node_t *gnode;
    ogs_ip_t ip;

    ogs_timer_t *t_echo;        /* the next Echo Request */
    ogs_timer_t *t3;            /* T3-RESPONSE */
    uint16_t sequence_number;   /* of the Echo Request that is not answered yet */
    int attempts;
    bool outstanding;
    bool failed;                /* reported, and not answered since */
    bool in_use;                /* mark of upf_gtpu_path_sync() */
} upf_gtpu_path_t;

static OGS_POOL(path_pool, upf_gtpu_path_t);
static ogs_list_t path_list;
static bool path_initialized;

bool upf_gtpu_path_enabled(void)
{
    return upf_self()->mbs_path_echo_interval != 0;
}

static void path_init(void)
{
    if (path_initialized)
        return;
    ogs_pool_init(&path_pool, MAX_NUM_OF_PATH);
    ogs_list_init(&path_list);
    path_initialized = true;
}

static bool sockaddr_to_ip(const ogs_sockaddr_t *addr, ogs_ip_t *ip)
{
    memset(ip, 0, sizeof(*ip));
    if (addr->ogs_sa_family == AF_INET) {
        ip->ipv4 = 1;
        ip->addr = addr->sin.sin_addr.s_addr;
        ip->len = OGS_IPV4_LEN;
    } else if (addr->ogs_sa_family == AF_INET6) {
        ip->ipv6 = 1;
        memcpy(ip->addr6, &addr->sin6.sin6_addr, OGS_IPV6_LEN);
        ip->len = OGS_IPV6_LEN;
    } else {
        return false;
    }
    return true;
}

static bool ip_equal(const ogs_ip_t *a, const ogs_ip_t *b)
{
    if (a->ipv4 != b->ipv4 || a->ipv6 != b->ipv6)
        return false;
    if (a->ipv4 && a->addr != b->addr)
        return false;
    return !a->ipv6 || memcmp(a->addr6, b->addr6, OGS_IPV6_LEN) == 0;
}

static upf_gtpu_path_t *path_find_by_gnode(const ogs_gtp_node_t *gnode)
{
    upf_gtpu_path_t *path;

    ogs_list_for_each(&path_list, path)
        if (path->gnode == gnode)
            return path;
    return NULL;
}

static upf_gtpu_path_t *path_find_by_ip(const ogs_ip_t *ip)
{
    upf_gtpu_path_t *path;

    ogs_list_for_each(&path_list, path)
        if (ip_equal(&path->ip, ip))
            return path;
    return NULL;
}

/* An Echo Request (cl. 4.4.2.1: destination port 2152; cl. 5.1: S flag set, TEID 0, version 1, PT 1) */
static void path_send_echo_request(upf_gtpu_path_t *path)
{
    ogs_pkbuf_t *pkbuf;
    uint8_t *p;

    pkbuf = ogs_pkbuf_alloc(NULL, 16);
    if (!pkbuf) {
        ogs_error("ogs_pkbuf_alloc() failed");
        return;
    }
    p = ogs_pkbuf_put(pkbuf, 12);
    p[0] = 0x32;                        /* version 1, PT 1, S flag */
    p[1] = OGS_GTPU_MSGTYPE_ECHO_REQ;
    p[2] = 0; p[3] = 4;                 /* the sequence number, the N-PDU number and the next extension header type */
    p[4] = p[5] = p[6] = p[7] = 0;      /* TEID 0 */
    p[8] = path->sequence_number >> 8;
    p[9] = path->sequence_number & 0xff;
    p[10] = 0;
    p[11] = 0;

    if (ogs_gtp_sendto(path->gnode, pkbuf) != OGS_OK)
        ogs_error("Echo Request to a remote GTP-U peer failed");
    ogs_pkbuf_free(pkbuf);
}

static void path_start_echo(upf_gtpu_path_t *path)
{
    path->sequence_number++;
    path->attempts = 1;
    path->outstanding = true;
    path_send_echo_request(path);
    ogs_timer_start(path->t3, ogs_time_from_sec(upf_self()->mbs_path_t3_response));
    ogs_timer_start(path->t_echo, ogs_time_from_sec(upf_self()->mbs_path_echo_interval));
}

static upf_gtpu_path_t *path_add(ogs_gtp_node_t *gnode)
{
    upf_gtpu_path_t *path = NULL;

    ogs_pool_id_calloc(&path_pool, &path);
    if (!path) {
        ogs_error("Maximum number of GTP-U paths [%d] reached", MAX_NUM_OF_PATH);
        return NULL;
    }
    path->gnode = gnode;
    if (!sockaddr_to_ip(&gnode->addr, &path->ip)) {
        ogs_pool_id_free(&path_pool, path);
        return NULL;
    }
    path->t_echo = ogs_timer_add(ogs_app()->timer_mgr, upf_timer_gtpu_echo, OGS_UINT_TO_POINTER(path->id));
    path->t3 = ogs_timer_add(ogs_app()->timer_mgr, upf_timer_gtpu_t3_response, OGS_UINT_TO_POINTER(path->id));
    ogs_assert(path->t_echo && path->t3);
    ogs_list_add(&path_list, path);
    return path;
}

static void path_remove(upf_gtpu_path_t *path)
{
    ogs_list_remove(&path_list, path);
    ogs_timer_delete(path->t_echo);
    ogs_timer_delete(path->t3);
    ogs_pool_id_free(&path_pool, path);
}

void upf_gtpu_path_sync(void)
{
    upf_gtpu_path_t *path, *next;
    upf_mbs_sess_t *mbs_sess;
    ogs_pfcp_far_t *far;
    ogs_pfcp_mbs_unicast_t *mu;

    if (!upf_gtpu_path_enabled())
        return;
    path_init();

    ogs_list_for_each(&path_list, path)
        path->in_use = false;

    ogs_list_for_each(&upf_self()->upf_mbs_sess_list, mbs_sess) {
        ogs_list_for_each(&mbs_sess->pfcp.far_list, far) {
            ogs_list_for_each(&far->mbs_unicast_list, mu) {
                if (!mu->gnode)
                    continue;
                path = path_find_by_gnode(mu->gnode);
                if (!path) {
                    path = path_add(mu->gnode);
                    if (!path)
                        continue;
                    path->in_use = true;
                    path_start_echo(path);
                }
                path->in_use = true;
            }
        }
    }

    ogs_list_for_each_safe(&path_list, next, path)
        if (!path->in_use)
            path_remove(path);
}

/* The PFCP Node Report Request for the peers (cl. 6.2.9.2: the UP function sends it to the CP function) */
ogs_pkbuf_t *upf_gtpu_path_build_failure_report(const ogs_ip_t *peers, int num_of_peers)
{
    ogs_pfcp_message_t *message;
    ogs_pfcp_node_report_request_t *req;
    ogs_pfcp_node_id_t node_id;
    int node_id_len = 0, i;
    uint8_t peer_data[OGS_PFCP_MAX_NUM_OF_REMOTE_GTP_U_PEER_IN_REPORT][1 + OGS_IPV6_LEN];
    uint8_t type_data = 0x01;       /* Node Report Type, bit 1 UPFR (cl. 8.2.69) */
    ogs_pkbuf_t *pkbuf;

    ogs_assert(peers);
    ogs_assert(num_of_peers >= 1 && num_of_peers <= OGS_PFCP_MAX_NUM_OF_REMOTE_GTP_U_PEER_IN_REPORT);

    if (ogs_pfcp_sockaddr_to_node_id(&node_id, &node_id_len) != OGS_OK) {
        ogs_error("ogs_pfcp_sockaddr_to_node_id() failed");
        return NULL;
    }

    message = ogs_calloc(1, sizeof(*message));
    ogs_assert(message);
    req = &message->pfcp_node_report_request;

    req->node_id.presence = 1;
    req->node_id.data = &node_id;
    req->node_id.len = node_id_len;
    req->node_report_type.presence = 1;
    req->node_report_type.data = &type_data;
    req->node_report_type.len = 1;
    req->user_plane_path_failure_report.presence = 1;
    for (i = 0; i < num_of_peers; i++) {
        /* cl. 8.2.70: flags (bit 1 V6, bit 2 V4), then the address; neither DI, NI nor RTS */
        if (peers[i].ipv4) {
            peer_data[i][0] = 0x02;
            memcpy(&peer_data[i][1], &peers[i].addr, OGS_IPV4_LEN);
            req->user_plane_path_failure_report.remote_gtp_u_peers[i].len = 1 + OGS_IPV4_LEN;
        } else {
            peer_data[i][0] = 0x01;
            memcpy(&peer_data[i][1], peers[i].addr6, OGS_IPV6_LEN);
            req->user_plane_path_failure_report.remote_gtp_u_peers[i].len = 1 + OGS_IPV6_LEN;
        }
        req->user_plane_path_failure_report.remote_gtp_u_peers[i].presence = 1;
        req->user_plane_path_failure_report.remote_gtp_u_peers[i].data = peer_data[i];
    }

    message->h.type = OGS_PFCP_NODE_REPORT_REQUEST_TYPE;
    pkbuf = ogs_pfcp_build_msg(message);
    ogs_expect(pkbuf);
    ogs_free(message);
    return pkbuf;
}

static void node_report_timeout(ogs_pfcp_xact_t *xact, void *data)
{
    ogs_error("No PFCP Node Report Response");
}

static void send_failure_report(ogs_pfcp_node_t *node, const ogs_ip_t *peer)
{
    ogs_pkbuf_t *pkbuf;
    ogs_pfcp_header_t h;
    ogs_pfcp_xact_t *xact;

    memset(&h, 0, sizeof(h));
    h.type = OGS_PFCP_NODE_REPORT_REQUEST_TYPE;

    xact = ogs_pfcp_xact_local_create(node, node_report_timeout, NULL);
    if (!xact) {
        ogs_error("ogs_pfcp_xact_local_create() failed");
        return;
    }
    pkbuf = upf_gtpu_path_build_failure_report(peer, 1);
    if (!pkbuf) {
        ogs_pfcp_xact_delete(xact);
        return;
    }
    if (ogs_pfcp_xact_update_tx(xact, &h, pkbuf) != OGS_OK) {
        ogs_error("ogs_pfcp_xact_update_tx() failed");
        return;
    }
    ogs_expect(ogs_pfcp_xact_commit(xact) == OGS_OK);
}

/* TS 29.244 cl. 5.10A: "Upon detection of user plane path failure ... the UP function shall initiate PFCP Node Report
 * procedure, towards the CP function controlling the affected PFCP sessions" */
static void report_path_failure(upf_gtpu_path_t *path)
{
    ogs_pfcp_node_t *nodes[64];
    int n = 0, i;
    upf_mbs_sess_t *mbs_sess;
    ogs_pfcp_far_t *far;
    ogs_pfcp_mbs_unicast_t *mu;

    ogs_list_for_each(&upf_self()->upf_mbs_sess_list, mbs_sess) {
        bool affected = false;

        if (!mbs_sess->pfcp_node)
            continue;
        ogs_list_for_each(&mbs_sess->pfcp.far_list, far)
            ogs_list_for_each(&far->mbs_unicast_list, mu)
                if (mu->gnode == path->gnode)
                    affected = true;
        if (!affected)
            continue;
        for (i = 0; i < n; i++)
            if (nodes[i] == mbs_sess->pfcp_node)
                break;
        if (i == n && n < OGS_ARRAY_SIZE(nodes))
            nodes[n++] = mbs_sess->pfcp_node;
    }

    ogs_warn("GTP-U path failure towards a remote GTP-U peer, reported to %d MB-SMF", n);
    for (i = 0; i < n; i++)
        send_failure_report(nodes[i], &path->ip);
}

void upf_gtpu_path_handle_timer(int timer_id, ogs_pool_id_t id)
{
    upf_gtpu_path_t *path;

    if (!path_initialized)
        return;
    path = ogs_pool_find_by_id(&path_pool, id);
    if (!path)
        return;     /* the path ended while the event was queued */

    switch (timer_id) {
    case UPF_TIMER_GTPU_ECHO:
        /* the sessions may have gone without a request of the MB-SMF (a lost association): paths of no session end */
        upf_gtpu_path_sync();
        path = ogs_pool_find_by_id(&path_pool, id);
        if (!path)
            break;
        if (path->outstanding) {
            /* the previous Echo Request is still being retransmitted: no second one */
            ogs_timer_start(path->t_echo, ogs_time_from_sec(upf_self()->mbs_path_echo_interval));
            break;
        }
        path_start_echo(path);
        break;
    case UPF_TIMER_GTPU_T3_RESPONSE:
        if (!path->outstanding)
            break;
        if (path->attempts < (int)upf_self()->mbs_path_n3_requests) {
            path->attempts++;
            path_send_echo_request(path);   /* the same sequence number */
            ogs_timer_start(path->t3, ogs_time_from_sec(upf_self()->mbs_path_t3_response));
            break;
        }
        path->outstanding = false;
        if (!path->failed) {
            path->failed = true;
            report_path_failure(path);
        }
        break;
    default:
        ogs_error("Unknown timer[%s:%d]", upf_timer_get_name(timer_id), timer_id);
        break;
    }
}

void upf_gtpu_path_handle_echo_response(const ogs_ip_t *from, uint16_t sequence_number)
{
    upf_gtpu_path_t *path;

    if (!path_initialized)
        return;
    path = path_find_by_ip(from);
    if (!path || !path->outstanding || path->sequence_number != sequence_number)
        return;     /* not a response to the Echo Request that is outstanding: a duplicate (cl. 11) */

    path->outstanding = false;
    path->attempts = 0;
    ogs_timer_stop(path->t3);
    if (path->failed) {
        ogs_info("GTP-U path towards a remote GTP-U peer answers again");
        path->failed = false;
    }
}

void upf_gtpu_path_final(void)
{
    upf_gtpu_path_t *path, *next;

    if (!path_initialized)
        return;
    ogs_list_for_each_safe(&path_list, next, path)
        path_remove(path);
    ogs_pool_final(&path_pool);
    path_initialized = false;
}
