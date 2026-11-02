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

/* A PFCP Node Report Request without SEID: the header (version 1, type 12, length, sequence number) and the IEs. */
static ogs_pkbuf_t *node_report_request_pkbuf(const uint8_t *ies, size_t len)
{
    ogs_pkbuf_t *pkbuf = ogs_pkbuf_alloc(NULL, OGS_MAX_SDU_LEN);
    uint8_t hdr[8];

    ogs_assert(pkbuf);
    hdr[0] = 0x20;
    hdr[1] = OGS_PFCP_NODE_REPORT_REQUEST_TYPE;
    hdr[2] = (4 + len) >> 8;
    hdr[3] = (4 + len) & 0xff;
    hdr[4] = hdr[5] = 0;
    hdr[6] = 1;
    hdr[7] = 0;
    ogs_pkbuf_put_data(pkbuf, hdr, sizeof(hdr));
    ogs_pkbuf_put_data(pkbuf, ies, len);
    return pkbuf;
}

/* User Plane Path Failure Report (TS 29.244 Table 7.4.5.1.2-1): "More than one IE with this type may be included to
 * represent multiple remote GTP-U peers towards which a user plane path failure has been detected." */
static void path_failure_report_peers_func(abts_case *tc, void *data)
{
    static const uint8_t ies[] = {
        0x00, 0x3c, 0x00, 0x05, 0x00, 127, 0, 0, 1,              /* Node ID */
        0x00, 0x65, 0x00, 0x01, 0x01,                            /* Node Report Type: UPFR */
        0x00, 0x66, 0x00, 0x27,                                  /* User Plane Path Failure Report */
          0x00, 0x67, 0x00, 0x09, 0x02, 0x00, 0x00, 10, 0, 0, 1, 0x00, 0x00,   /* flags V4, 10.0.0.1, no DI, no NI */
          0x00, 0x67, 0x00, 0x09, 0x02, 0x00, 0x00, 10, 0, 0, 2, 0x00, 0x00,
          0x00, 0x67, 0x00, 0x09, 0x02, 0x00, 0x00, 10, 0, 0, 3, 0x00, 0x00 };
    ogs_pfcp_message_t *message;
    ogs_pkbuf_t *pkbuf, *built, *with_header;
    ogs_pfcp_message_t *out;
    ogs_pfcp_node_id_t node_id;
    uint8_t peer_data[3][7];
    int i;

    ogs_log_install_domain(&__ogs_pfcp_domain, "pfcp", ogs_core()->log.level);

    pkbuf = node_report_request_pkbuf(ies, sizeof(ies));
    message = ogs_pfcp_parse_msg(pkbuf);
    ABTS_PTR_NOTNULL(tc, message);
    if (!message) {
        ogs_pkbuf_free(pkbuf);
        return;
    }
    for (i = 0; i < 3; i++) {
        ABTS_INT_EQUAL(tc, 1, message->pfcp_node_report_request.user_plane_path_failure_report.remote_gtp_u_peers[i].presence);
        if (message->pfcp_node_report_request.user_plane_path_failure_report.remote_gtp_u_peers[i].presence)
            ABTS_INT_EQUAL(tc, i + 1,
                ((uint8_t *)message->pfcp_node_report_request.user_plane_path_failure_report.remote_gtp_u_peers[i].data)[6]);
    }
    ABTS_INT_EQUAL(tc, 0, message->pfcp_node_report_request.user_plane_path_failure_report.remote_gtp_u_peers[3].presence);
    ogs_pfcp_message_free(message);
    ogs_pkbuf_free(pkbuf);

    /* the builder writes every peer */
    out = ogs_calloc(1, sizeof(*out));
    ogs_assert(out);
    memset(&node_id, 0, sizeof(node_id));
    out->pfcp_node_report_request.node_id.presence = 1;
    out->pfcp_node_report_request.node_id.data = &node_id;
    out->pfcp_node_report_request.node_id.len = 5;
    out->pfcp_node_report_request.node_report_type.presence = 1;
    out->pfcp_node_report_request.node_report_type.data = "\x01";
    out->pfcp_node_report_request.node_report_type.len = 1;
    for (i = 0; i < 3; i++) {
        memcpy(peer_data[i], "\x02\x00\x00\x0a\x00\x00\x00", 7);
        peer_data[i][6] = i + 1;
        out->pfcp_node_report_request.user_plane_path_failure_report.remote_gtp_u_peers[i].presence = 1;
        out->pfcp_node_report_request.user_plane_path_failure_report.remote_gtp_u_peers[i].data = peer_data[i];
        out->pfcp_node_report_request.user_plane_path_failure_report.remote_gtp_u_peers[i].len = 7;
    }
    out->pfcp_node_report_request.user_plane_path_failure_report.presence = 1;
    out->h.type = OGS_PFCP_NODE_REPORT_REQUEST_TYPE;
    built = ogs_pfcp_build_msg(out);
    ABTS_PTR_NOTNULL(tc, built);
    ogs_free(out);
    if (!built)
        return;
    with_header = node_report_request_pkbuf(built->data, built->len);
    ogs_pkbuf_free(built);
    message = ogs_pfcp_parse_msg(with_header);
    ABTS_PTR_NOTNULL(tc, message);
    if (message) {
        for (i = 0; i < 3; i++)
            ABTS_INT_EQUAL(tc, 1, message->pfcp_node_report_request.user_plane_path_failure_report.remote_gtp_u_peers[i].presence);
        ogs_pfcp_message_free(message);
    }
    ogs_pkbuf_free(with_header);
}

/* UP Function Features (TS 29.244 cl. 8.2.25): the octets up to 15 are read and kept: "14/1 MBSCH", "12/8 EML4S", "13/1 PDUSM", "15/3 ULM" */
static void up_function_features_func(abts_case *tc, void *data)
{
    ogs_pfcp_up_function_features_t f;
    uint8_t octets[11];     /* octets 5 to 15 of the IE */

    ABTS_INT_EQUAL(tc, 11, (int)sizeof(f));

    memset(octets, 0, sizeof(octets));
    octets[14 - 5] = 0x01;
    memcpy(&f, octets, sizeof(f));
    ABTS_INT_EQUAL(tc, 1, f.mbsch);
    ABTS_INT_EQUAL(tc, 0, f.octet12 | f.octet13 | f.octet15);

    memset(octets, 0, sizeof(octets));
    octets[12 - 5] = 0x80;
    octets[13 - 5] = 0x01;
    octets[15 - 5] = 0x04;
    memcpy(&f, octets, sizeof(f));
    ABTS_INT_EQUAL(tc, 1, f.eml4s);
    ABTS_INT_EQUAL(tc, 1, f.pdusm);
    ABTS_INT_EQUAL(tc, 1, f.ulm);
    ABTS_INT_EQUAL(tc, 0, f.mbsch);

    /* the flag set in the structure is the bit of the octet */
    memset(&f, 0, sizeof(f));
    f.mbsch = 1;
    ABTS_INT_EQUAL(tc, 0x01, f.octet14);
    ABTS_INT_EQUAL(tc, 0, f.octet5 | f.octet11 | f.octet12 | f.octet13 | f.octet15);
}

abts_suite *test_pfcp_types(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, multicast_transport_information_func, NULL);
    abts_run_test(suite, path_failure_report_peers_func, NULL);
    abts_run_test(suite, up_function_features_func, NULL);

    return suite;
}
