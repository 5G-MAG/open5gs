/*
 * Unit test of the MB-UPF's read of its MBS UDP tunnel: a datagram that fits is read whole, one that is longer than the
 * buffer is refused and counted, not cut and passed on.
 */
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "ogs-core.h"
#include "ogs-app.h"
#include "ogs-pfcp.h"
#include "ogs-gtp.h"
#include "context.h"
#include "n4mb-handler.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond) do { \
    g_checks++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_failures++; \
    } \
} while (0)

#define MTU 1458u

static void send_datagram(int tx, const struct sockaddr_in *to, size_t len)
{
    unsigned char buf[4000];
    memset(buf, 0x45, sizeof(buf));
    CHECK(sendto(tx, buf, len, 0, (const struct sockaddr *)to, sizeof(*to)) == (ssize_t)len);
}

static void test_tunnel_recv(void)
{
    int rx = socket(AF_INET, SOCK_DGRAM, 0);
    int tx = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    unsigned char buf[MTU];
    unsigned int oversize = 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(rx >= 0 && tx >= 0);
    CHECK(bind(rx, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    CHECK(getsockname(rx, (struct sockaddr *)&addr, &addr_len) == 0);

    /* nothing to read */
    CHECK(upf_mbs_tunnel_recv(rx, buf, MTU, &oversize) == 0);
    CHECK(oversize == 0);

    /* a datagram that fits is read whole, at several sizes up to the limit */
    send_datagram(tx, &addr, 100);
    CHECK(upf_mbs_tunnel_recv(rx, buf, MTU, &oversize) == 100);
    send_datagram(tx, &addr, 1450);
    CHECK(upf_mbs_tunnel_recv(rx, buf, MTU, &oversize) == 1450);
    send_datagram(tx, &addr, MTU);
    CHECK(upf_mbs_tunnel_recv(rx, buf, MTU, &oversize) == (ssize_t)MTU);
    CHECK(oversize == 0);

    /* one octet over is refused, not cut to the limit, and counted */
    send_datagram(tx, &addr, MTU + 1);
    CHECK(upf_mbs_tunnel_recv(rx, buf, MTU, &oversize) == -1);
    CHECK(oversize == 1);
    /* and it is gone: the next read finds nothing, not a rest of it */
    CHECK(upf_mbs_tunnel_recv(rx, buf, MTU, &oversize) == 0);

    /* the sizes the MBSTF used to send for the largest FLUTE packets, and a full Ethernet payload */
    send_datagram(tx, &addr, 1470);
    CHECK(upf_mbs_tunnel_recv(rx, buf, MTU, &oversize) == -1);
    send_datagram(tx, &addr, 1500);
    CHECK(upf_mbs_tunnel_recv(rx, buf, MTU, &oversize) == -1);
    CHECK(oversize == 3);

    /* a refusal does not spoil the next datagram */
    send_datagram(tx, &addr, 1000);
    CHECK(upf_mbs_tunnel_recv(rx, buf, MTU, &oversize) == 1000);
    CHECK(oversize == 3);

    close(rx);
    close(tx);
}

/* TS 29.244 V19.6.0 clause 8.2.26: the Apply Action bit DROP is a request to drop the packets; forwarding and buffering are not */
static void test_far_drop(void)
{
    ogs_pfcp_far_t far;

    memset(&far, 0, sizeof(far));
    CHECK(!upf_mbs_far_drops_packets(NULL));
    CHECK(!upf_mbs_far_drops_packets(&far));
    far.apply_action = OGS_PFCP_APPLY_ACTION_DROP;
    CHECK(upf_mbs_far_drops_packets(&far));
    far.apply_action = OGS_PFCP_APPLY_ACTION_FSSM;
    CHECK(!upf_mbs_far_drops_packets(&far));
    far.apply_action = OGS_PFCP_APPLY_ACTION_MBSU;
    CHECK(!upf_mbs_far_drops_packets(&far));
    far.apply_action = OGS_PFCP_APPLY_ACTION_BUFF | OGS_PFCP_APPLY_ACTION_NOCP;
    CHECK(!upf_mbs_far_drops_packets(&far));
}

/* TS 29.244 V19.6.0 clause 8.2.7: a Gate value of 2 or 3 "If received, shall be interpreted as the value "1"" */
static void test_gate_status(void)
{
    ogs_pfcp_gate_status_t g;
    int ul, dl;

    for (ul = 0; ul < 4; ul++) {
        for (dl = 0; dl < 4; dl++) {
            memset(&g, 0, sizeof(g));
            g.uplink = ul;
            g.downlink = dl;
            ogs_pfcp_gate_status_interpret(&g);
            CHECK(g.uplink == (ul == 0 ? OGS_PFCP_GATE_OPEN : OGS_PFCP_GATE_CLOSE));
            CHECK(g.downlink == (dl == 0 ? OGS_PFCP_GATE_OPEN : OGS_PFCP_GATE_CLOSE));
        }
    }
}

/* The DSCP (the ToS or Traffic Class octet) of the last datagram read from fd, -1 when none, -2 when no marking was reported */
static int recv_tos(int fd, int family)
{
    char data[256], control[256];
    struct iovec iov = { data, sizeof(data) };
    struct msghdr msg;
    struct cmsghdr *c;
    int tos = -2;

    memset(&msg, 0, sizeof(msg));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    if (recvmsg(fd, &msg, MSG_DONTWAIT) < 0)
        return -1;
    for (c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
        if (family == AF_INET && c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_TOS)
            tos = *(unsigned char *)CMSG_DATA(c);
        if (family == AF_INET6 && c->cmsg_level == IPPROTO_IPV6 && c->cmsg_type == IPV6_TCLASS)
            memcpy(&tos, CMSG_DATA(c), sizeof(int));
    }
    return tos;
}

/* A receiver on the loopback of the family, which reports the marking of what it reads; -1 when the host has no such loopback */
static int open_rx(int family, ogs_sockaddr_t *addr)
{
    int fd = socket(family, SOCK_DGRAM, 0), on = 1;
    socklen_t len;

    if (fd < 0)
        return -1;
    memset(addr, 0, sizeof(*addr));
    if (family == AF_INET) {
        struct sockaddr_in a;

        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(fd, (struct sockaddr *)&a, sizeof(a)) != 0 || setsockopt(fd, IPPROTO_IP, IP_RECVTOS, &on, sizeof(on)) != 0)
            goto fail;
        len = sizeof(a);
        getsockname(fd, (struct sockaddr *)&a, &len);
        memcpy(&addr->sa, &a, sizeof(a));
    } else {
        struct sockaddr_in6 a;

        memset(&a, 0, sizeof(a));
        a.sin6_family = AF_INET6;
        a.sin6_addr = in6addr_loopback;
        if (bind(fd, (struct sockaddr *)&a, sizeof(a)) != 0 ||
                setsockopt(fd, IPPROTO_IPV6, IPV6_RECVTCLASS, &on, sizeof(on)) != 0)
            goto fail;
        len = sizeof(a);
        getsockname(fd, (struct sockaddr *)&a, &len);
        memcpy(&addr->sa, &a, sizeof(a));
    }
    return fd;
fail:
    close(fd);
    return -1;
}

static void forward(ogs_pfcp_pdr_t *pdr)
{
    ogs_pkbuf_t *buf = ogs_pkbuf_alloc(NULL, 128);

    ogs_pkbuf_put(buf, 64);
    memset(buf->data, 0, buf->len);
    ogs_pfcp_send_g_pdu_mbs(pdr, buf);
}

/*
 * TS 29.244 V19.6.0 clause 5.4.13: "The UP function shall perform the transport level marking for the detected traffic and sends the marked
 * packet to the peer entity." The Transport Level Marking of the MBS Multicast Parameters is on the copy to the low layer SSM, the one of an
 * Add MBS Unicast Parameters on the copy to its peer; a FAR without one sends an unmarked datagram. The ECN bits are not part of the marking.
 */
static void test_transport_level_marking(int family)
{
    ogs_sockaddr_t rx_addr, ux_addr;
    ogs_sock_t sock;
    ogs_gtp_node_t ll_node, u_node;
    ogs_pfcp_pdr_t pdr;
    ogs_pfcp_far_t far;
    ogs_pfcp_mbs_unicast_t mu;
    int rx = open_rx(family, &rx_addr), ux = open_rx(family, &ux_addr);
    int tx = socket(family, SOCK_DGRAM, 0);

    if (rx < 0 || ux < 0 || tx < 0) {
        printf("no %s loopback, marking test skipped\n", family == AF_INET ? "IPv4" : "IPv6");
        return;
    }
    memset(&sock, 0, sizeof(sock));
    sock.fd = tx;
    memset(&ll_node, 0, sizeof(ll_node));
    ll_node.sock = &sock;
    ll_node.addr = rx_addr;
    memset(&u_node, 0, sizeof(u_node));
    u_node.sock = &sock;
    u_node.addr = ux_addr;
    memset(&pdr, 0, sizeof(pdr));
    memset(&far, 0, sizeof(far));
    memset(&mu, 0, sizeof(mu));
    pdr.far = &far;
    far.apply_action = OGS_PFCP_APPLY_ACTION_FSSM;
    far.dst_if = OGS_PFCP_INTERFACE_ACCESS;
    far.gnode = &ll_node;
    ogs_list_init(&far.mbs_unicast_list);
    mu.far = &far;
    mu.gnode = &u_node;
    ogs_list_add(&far.mbs_unicast_list, &mu.lnode);

    /* no Transport Level Marking: the datagram is not marked */
    forward(&pdr);
    CHECK(recv_tos(rx, family) == 0);

    /* the DSCP of the IE, 0xB8 (EF) with the mask 0xFC */
    far.transport_level_marking = true;
    far.tos = 0xB8;
    far.tos_mask = 0xFC;
    forward(&pdr);
    CHECK(recv_tos(rx, family) == 0xB8);

    /* the ECN bits in the first octet, or a mask that covers them, do not reach the datagram */
    far.tos = 0xBB;
    far.tos_mask = 0xFF;
    forward(&pdr);
    CHECK(recv_tos(rx, family) == 0xB8);

    /* a mask that clears the DSCP leaves the datagram unmarked */
    far.tos = 0xB8;
    far.tos_mask = 0x00;
    forward(&pdr);
    CHECK(recv_tos(rx, family) == 0);

    /* FSSM and MBSU together: each copy carries the marking of its own parameters */
    far.apply_action = OGS_PFCP_APPLY_ACTION_FSSM | OGS_PFCP_APPLY_ACTION_MBSU;
    far.tos = 0x28;
    far.tos_mask = 0xFC;
    mu.transport_level_marking = true;
    mu.tos = 0xB8;
    mu.tos_mask = 0xFC;
    forward(&pdr);
    CHECK(recv_tos(rx, family) == 0x28);
    CHECK(recv_tos(ux, family) == 0xB8);

    /* only the multicast parameters carry a marking: the unicast copy is unmarked */
    mu.transport_level_marking = false;
    forward(&pdr);
    CHECK(recv_tos(rx, family) == 0x28);
    CHECK(recv_tos(ux, family) == 0);

    /* nothing is left to read */
    CHECK(recv_tos(rx, family) == -1);
    CHECK(recv_tos(ux, family) == -1);

    /* a failing send is reported, not taken for a sent datagram */
    {
        int dead = socket(family, SOCK_DGRAM, 0);
        ogs_sock_t dead_sock;
        ogs_pkbuf_t *buf = ogs_pkbuf_alloc(NULL, 128);

        ogs_pkbuf_put(buf, 64);
        memset(buf->data, 0, buf->len);
        close(dead);
        memset(&dead_sock, 0, sizeof(dead_sock));
        dead_sock.fd = dead;
        CHECK(ogs_sendto_tos(dead, buf->data, buf->len, 0, &rx_addr, 0xB8) == -1);
        CHECK(ogs_gtp_send_with_teid_tos(&dead_sock, buf, 0, &rx_addr, 0xB8) == OGS_ERROR);
        ogs_pkbuf_free(buf);
    }

    close(rx);
    close(ux);
    close(tx);
}

int main(void)
{
    ogs_core_initialize();
    /* the UPF's log domain, which the error of a refusal is written to; upf_context_init() installs it in the UPF */
    ogs_log_install_domain(&__upf_log_domain, "upf", ogs_core()->log.level);
    /* the GTP-U send of the marking test logs under the GTP domain */
    ogs_log_install_domain(&__ogs_gtp_domain, "gtp", ogs_core()->log.level);
    test_tunnel_recv();
    test_far_drop();
    test_gate_status();
    test_transport_level_marking(AF_INET);
    test_transport_level_marking(AF_INET6);
    printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
