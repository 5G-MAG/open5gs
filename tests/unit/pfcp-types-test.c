/*
 * Parsing of the PFCP information elements whose octets come from a peer must report an error, not abort.
 */

#include "ogs-pfcp.h"
#include "core/abts.h"

/* Multicast Transport Information (TS 29.244 cl. 8.2.207): spare, C-TEID, destination address, source address;
 * each address is one octet (Address Type in bits 8-7, Address Length in bits 6-1) and the address. */
static void multicast_transport_information_func(abts_case *tc, void *data)
{
    static const uint8_t good4[] = {
        0x00, 0x00, 0x00, 0x00, 0x2a,
        0x04, 232, 1, 1, 1,
        0x04, 10, 1, 2, 3 };
    static const uint8_t good6[] = {
        0x00, 0x00, 0x00, 0x00, 0x2a,
        0x50, 0xff, 0x3e, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x12, 0x34,
        0x50, 0xfd, 0x99, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x02 };
    ogs_pfcp_multicast_transport_information_t mti;
    ogs_tlv_octet_t octet;
    uint8_t buf[sizeof(good6) + 1];
    size_t n;

    ogs_log_install_domain(&__ogs_pfcp_domain, "pfcp", ogs_core()->log.level);

    /* well formed: IPv4 and IPv6 */
    octet.presence = 1;
    octet.data = (void *)good4;
    octet.len = sizeof(good4);
    ABTS_INT_EQUAL(tc, (int)sizeof(good4), ogs_pfcp_parse_multicast_transport_information(&mti, &octet));
    ABTS_INT_EQUAL(tc, 0, mti.ip_multicast_distribution_address.address_type);
    octet.data = (void *)good6;
    octet.len = sizeof(good6);
    ABTS_INT_EQUAL(tc, (int)sizeof(good6), ogs_pfcp_parse_multicast_transport_information(&mti, &octet));
    ABTS_INT_EQUAL(tc, 1, mti.ip_source_address.address_type);

    /* every truncation of a well formed string is an error */
    for (n = 0; n < sizeof(good4); n++) {
        octet.data = buf;
        octet.len = n;
        memcpy(buf, good4, n);
        ABTS_INT_EQUAL(tc, -1, ogs_pfcp_parse_multicast_transport_information(&mti, &octet));
    }
    for (n = 0; n < sizeof(good6); n++) {
        octet.data = buf;
        octet.len = n;
        memcpy(buf, good6, n);
        ABTS_INT_EQUAL(tc, -1, ogs_pfcp_parse_multicast_transport_information(&mti, &octet));
    }

    /* octets left over */
    memcpy(buf, good4, sizeof(good4));
    buf[sizeof(good4)] = 0;
    octet.data = buf;
    octet.len = sizeof(good4) + 1;
    ABTS_INT_EQUAL(tc, -1, ogs_pfcp_parse_multicast_transport_information(&mti, &octet));

    /* the length does not match the type */
    memcpy(buf, good4, sizeof(good4));
    buf[5] = 0x08;      /* IPv4, length 8 */
    octet.len = sizeof(good4);
    ABTS_INT_EQUAL(tc, -1, ogs_pfcp_parse_multicast_transport_information(&mti, &octet));
    memcpy(buf, good4, sizeof(good4));
    buf[10] = 0x44;     /* a type that is neither IPv4 nor IPv6 */
    ABTS_INT_EQUAL(tc, -1, ogs_pfcp_parse_multicast_transport_information(&mti, &octet));

    /* no octets at all */
    octet.data = NULL;
    octet.len = 0;
    ABTS_INT_EQUAL(tc, -1, ogs_pfcp_parse_multicast_transport_information(&mti, &octet));
}

abts_suite *test_pfcp_types(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, multicast_transport_information_func, NULL);

    return suite;
}
