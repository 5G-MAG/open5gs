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

int main(void)
{
    ogs_core_initialize();
    /* the UPF's log domain, which the error of a refusal is written to; upf_context_init() installs it in the UPF */
    ogs_log_install_domain(&__upf_log_domain, "upf", ogs_core()->log.level);
    test_tunnel_recv();
    test_far_drop();
    printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
