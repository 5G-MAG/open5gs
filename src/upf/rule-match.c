/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
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

#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE     1

#include "rule-match.h"

#if HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#if HAVE_NETINET_IP6_H
#include <netinet/ip6.h>
#endif

upf_sess_t *upf_sess_find_by_ue_ip_address(ogs_pkbuf_t *pkbuf)
{
    upf_sess_t *sess = NULL;

    char buf[OGS_ADDRSTRLEN];

    struct ip *ip_h = NULL;
    struct ip6_hdr *ip6_h = NULL;

    ogs_assert(pkbuf);
    ogs_assert(pkbuf->len);
    ogs_assert(pkbuf->data);

    ip_h = (struct ip *)pkbuf->data;
    if (ip_h->ip_v == 4) {
        ip_h = (struct ip *)pkbuf->data;
        sess = upf_sess_find_by_ipv4(ip_h->ip_dst.s_addr);
    } else if (ip_h->ip_v == 6) {
        ip6_h = (struct ip6_hdr *)pkbuf->data;
        sess = upf_sess_find_by_ipv6((uint32_t *)ip6_h->ip6_dst.s6_addr);
    } else {
        ogs_error("Invalid packet [IP version:%d, Packet Length:%d]",
                ip_h->ip_v, pkbuf->len);
        ogs_log_hexdump(OGS_LOG_ERROR, pkbuf->data, pkbuf->len);
    }

    if (sess) {
        if (ip_h && sess->ipv4)
            ogs_trace("PAA IPv4:%s", OGS_INET_NTOP(&sess->ipv4->addr, buf));
        if (ip6_h && sess->ipv6)
            ogs_trace("PAA IPv6:%s", OGS_INET6_NTOP(&sess->ipv6->addr, buf));
    }

    return sess;
}

/* True if the first prefix_len bits of a and b are equal (the whole of both when prefix_len is 8 * len). */
static bool prefix_equal(const uint8_t *a, const uint8_t *b, unsigned int len, unsigned int prefix_len)
{
    unsigned int whole = prefix_len / 8, rest = prefix_len % 8;

    if (whole > len)
        whole = len;
    if (memcmp(a, b, whole) != 0)
        return false;
    if (rest && whole < len) {
        uint8_t mask = (uint8_t)(0xff << (8 - rest));
        if ((a[whole] & mask) != (b[whole] & mask))
            return false;
    }
    return true;
}

/*
 * True if the source address of the packet is one the session receives from: the session's source address of the
 * packet's family, with its mask or prefix length if it has one; a session with no source address receives from
 * any source (TS 29.244 Table 7.5.2.2-4, Source IP Address: "If this IE is not present, this indicates "any" source IP address.").
 */
static bool source_matches(const upf_mbs_sess_t *mbs_sess, int ip_version, const uint8_t *src)
{
    const ogs_ip_t *want = &mbs_sess->ssm.src_ip_addr;

    if (!want->ipv4 && !want->ipv6)
        return true;
    if (ip_version == 4) {
        return want->ipv4 && prefix_equal((const uint8_t *)&want->addr, src, OGS_IPV4_LEN,
                mbs_sess->ssm_src_has_prefix ? mbs_sess->ssm_src_prefix_len : 8 * OGS_IPV4_LEN);
    }
    return want->ipv6 && prefix_equal(want->addr6, src, OGS_IPV6_LEN,
            mbs_sess->ssm_src_has_prefix ? mbs_sess->ssm_src_prefix_len : 8 * OGS_IPV6_LEN);
}

/* True if dst is within the range or any-group session's destinations. */
static bool range_dest_matches(const upf_mbs_sess_t *mbs_sess, int ip_version, const uint8_t *dst)
{
    if (ip_version == 4) {
        if (!IN_MULTICAST(be32toh(*(const uint32_t *)dst)))
            return false;
        if (mbs_sess->ssm_dest_any)
            return true;
        return mbs_sess->ssm.dest_ip_addr.ipv4 && mbs_sess->ssm_dest_end.ipv4 &&
            be32toh(*(const uint32_t *)dst) >= be32toh(mbs_sess->ssm.dest_ip_addr.addr) &&
            be32toh(*(const uint32_t *)dst) <= be32toh(mbs_sess->ssm_dest_end.addr);
    }
    if (dst[0] != 0xff)
        return false;
    if (mbs_sess->ssm_dest_any)
        return true;
    return mbs_sess->ssm.dest_ip_addr.ipv6 && mbs_sess->ssm_dest_end.ipv6 &&
        memcmp(dst, mbs_sess->ssm.dest_ip_addr.addr6, OGS_IPV6_LEN) >= 0 &&
        memcmp(dst, mbs_sess->ssm_dest_end.addr6, OGS_IPV6_LEN) <= 0;
}

upf_mbs_sess_t *upf_mbs_sess_find_by_ssm(ogs_pkbuf_t *pkbuf)
{
    upf_mbs_sess_t *mbs_sess = NULL, *it = NULL;

    char buf[OGS_ADDRSTRLEN];

    struct ip *ip_h = NULL;
    struct ip6_hdr *ip6_h = NULL;
    int ip_version = 0;
    const uint8_t *src = NULL, *dst = NULL;

    ogs_assert(pkbuf);
    ogs_assert(pkbuf->len);
    ogs_assert(pkbuf->data);

    ip_h = (struct ip *)pkbuf->data;
    if (ip_h->ip_v == 4) {
        ip_version = 4;
        src = (const uint8_t *)&ip_h->ip_src.s_addr;
        dst = (const uint8_t *)&ip_h->ip_dst.s_addr;
        mbs_sess = upf_mbs_sess_find_by_ipv4(ip_h->ip_dst.s_addr);
    } else if (ip_h->ip_v == 6) {
        ip_version = 6;
        ip6_h = (struct ip6_hdr *)pkbuf->data;
        src = ip6_h->ip6_src.s6_addr;
        dst = ip6_h->ip6_dst.s6_addr;
        mbs_sess = upf_mbs_sess_find_by_ipv6((uint32_t *)ip6_h->ip6_dst.s6_addr);
    } else {
        ogs_error("Invalid packet [IP version:%d, Packet Length:%d]",
                ip_h->ip_v, pkbuf->len);
        ogs_log_hexdump(OGS_LOG_ERROR, pkbuf->data, pkbuf->len);
        return NULL;
    }

    /* A hit of the hash is a session of that group, and the group is the whole address: a session of another group is not one */
    if (mbs_sess && !upf_mbs_sess_same_group(mbs_sess, ip_version, dst))
        mbs_sess = NULL;

    /* The group is only half of the channel: the packet has to come from the session's source, too. */
    if (mbs_sess && !source_matches(mbs_sess, ip_version, src)) {
        ogs_debug("Multicast packet to a group of an MBS session, from a source it does not receive from");
        mbs_sess = NULL;
    }

    if (!mbs_sess) {
        /* Another session on the same group, for another source: the destination hash holds one session per group */
        ogs_list_for_each(&upf_self()->upf_mbs_sess_list, it) {
            if (upf_mbs_sess_same_group(it, ip_version, dst) && source_matches(it, ip_version, src)) {
                mbs_sess = it;
                break;
            }
        }
    }

    if (!mbs_sess) {
        /* Sessions for a range of groups, or for any group */
        ogs_list_for_each(&upf_self()->upf_mbs_sess_list, it) {
            if ((it->ssm_dest_any || it->ssm_dest_range) &&
                    range_dest_matches(it, ip_version, dst) && source_matches(it, ip_version, src)) {
                mbs_sess = it;
                break;
            }
        }
    }

    if (mbs_sess) {
        if (ip_h && mbs_sess->ssm.dest_ip_addr.ipv4)
            ogs_trace("PAA IPv4:%s", OGS_INET_NTOP(&mbs_sess->ssm.dest_ip_addr.addr, buf));
        if (ip6_h && mbs_sess->ssm.dest_ip_addr.ipv6)
            ogs_trace("PAA IPv6:%s", OGS_INET6_NTOP(&mbs_sess->ssm.dest_ip_addr.addr6, buf));
    }

    return mbs_sess;
}
