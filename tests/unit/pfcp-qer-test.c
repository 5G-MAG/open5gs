/*
 * Copyright (C) 2026 5G-MAG
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

#include "ogs-pfcp.h"
#include "core/abts.h"

/*
 * QER Indications of TS 29.244 V19.6.0 figure 8.2.216-1: bit 3 EML4S, bit 4 PDUSM, bit 5 DBSMI, bit 6 TNBMI (octet 5 values 0x04,
 * 0x08, 0x10, 0x20), carried in Create QER and Update QER.
 */

static ogs_pfcp_sess_t sess;
static bool initialized = false;

static void qer_indications_setup(void)
{
    if (!initialized) {
        ogs_app()->pool.sess = 4;
        ogs_app()->pool.nf = 4;
        ogs_pfcp_context_init();
        initialized = true;
    }
    memset(&sess, 0, sizeof(sess));
    ogs_pfcp_pool_init(&sess);
}

static void qer_indications_teardown(void)
{
    ogs_pfcp_sess_clear(&sess);
    ogs_pfcp_pool_final(&sess);
    ogs_pfcp_context_final();
    initialized = false;
}

static void pfcp_qer_burst_create(abts_case *tc, void *data)
{
    ogs_pfcp_qer_t *qer = NULL, *out = NULL;
    ogs_pfcp_tlv_create_qer_t tlv;
    uint8_t cause = 0, offending = 0;

    qer_indications_setup();
    qer = ogs_pfcp_qer_add(&sess);
    ABTS_PTR_NOTNULL(tc, qer);

    /* Data Burst Size alone: bit 5 */
    qer->data_burst_size_marking = true;
    memset(&tlv, 0, sizeof(tlv));
    ogs_pfcp_build_create_qer(&tlv, 0, qer);
    ABTS_INT_EQUAL(tc, 1, tlv.qer_indications.presence);
    ABTS_INT_EQUAL(tc, 0x10, ((uint8_t *)tlv.qer_indications.data)[0]);
    out = ogs_pfcp_handle_create_qer(&sess, &tlv, &cause, &offending);
    ABTS_PTR_NOTNULL(tc, out);
    ABTS_TRUE(tc, out->data_burst_size_marking);
    ABTS_TRUE(tc, !out->time_to_next_burst_marking);

    /* Time to Next Burst alone: bit 6 */
    qer->data_burst_size_marking = false;
    qer->time_to_next_burst_marking = true;
    memset(&tlv, 0, sizeof(tlv));
    ogs_pfcp_build_create_qer(&tlv, 0, qer);
    ABTS_INT_EQUAL(tc, 0x20, ((uint8_t *)tlv.qer_indications.data)[0]);
    out = ogs_pfcp_handle_create_qer(&sess, &tlv, &cause, &offending);
    ABTS_PTR_NOTNULL(tc, out);
    ABTS_TRUE(tc, !out->data_burst_size_marking);
    ABTS_TRUE(tc, out->time_to_next_burst_marking);

    /* with the other indications: EML4S 0x04, PDUSM 0x08 */
    qer->data_burst_size_marking = true;
    qer->ecn_marking_l4s = true;
    qer->pdu_set_marking = true;
    memset(&tlv, 0, sizeof(tlv));
    ogs_pfcp_build_create_qer(&tlv, 0, qer);
    ABTS_INT_EQUAL(tc, 0x3c, ((uint8_t *)tlv.qer_indications.data)[0]);
    out = ogs_pfcp_handle_create_qer(&sess, &tlv, &cause, &offending);
    ABTS_PTR_NOTNULL(tc, out);
    ABTS_TRUE(tc, out->ecn_marking_l4s && out->pdu_set_marking && out->data_burst_size_marking && out->time_to_next_burst_marking);

    /* none asked: no QER Indications IE, and a Create QER that has none clears the flags */
    qer->data_burst_size_marking = qer->time_to_next_burst_marking = qer->ecn_marking_l4s = qer->pdu_set_marking = false;
    memset(&tlv, 0, sizeof(tlv));
    ogs_pfcp_build_create_qer(&tlv, 0, qer);
    ABTS_INT_EQUAL(tc, 0, tlv.qer_indications.presence);
    out = ogs_pfcp_handle_create_qer(&sess, &tlv, &cause, &offending);
    ABTS_PTR_NOTNULL(tc, out);
    ABTS_TRUE(tc, !out->data_burst_size_marking && !out->time_to_next_burst_marking);

    qer_indications_teardown();
}

static void pfcp_qer_burst_update(abts_case *tc, void *data)
{
    ogs_pfcp_qer_t *qer = NULL, *out = NULL;
    ogs_pfcp_tlv_update_qer_t tlv;
    uint8_t cause = 0, offending = 0;

    qer_indications_setup();
    qer = ogs_pfcp_qer_add(&sess);
    ABTS_PTR_NOTNULL(tc, qer);

    qer->data_burst_size_marking = true;
    qer->time_to_next_burst_marking = true;
    memset(&tlv, 0, sizeof(tlv));
    ogs_pfcp_build_update_qer(&tlv, 0, qer, OGS_PFCP_MODIFY_QOS_MODIFY);
    ABTS_INT_EQUAL(tc, 0x30, ((uint8_t *)tlv.qer_indications.data)[0]);
    qer->data_burst_size_marking = qer->time_to_next_burst_marking = false;
    out = ogs_pfcp_handle_update_qer(&sess, &tlv, &cause, &offending);
    ABTS_PTR_EQUAL(tc, qer, out);
    ABTS_TRUE(tc, out->data_burst_size_marking && out->time_to_next_burst_marking);

    /* the Update QER that stops one of them: bits are on or off each time */
    qer->data_burst_size_marking = false;
    qer->time_to_next_burst_marking = true;
    memset(&tlv, 0, sizeof(tlv));
    ogs_pfcp_build_update_qer(&tlv, 0, qer, OGS_PFCP_MODIFY_QOS_MODIFY);
    ABTS_INT_EQUAL(tc, 0x20, ((uint8_t *)tlv.qer_indications.data)[0]);
    qer->data_burst_size_marking = true;
    out = ogs_pfcp_handle_update_qer(&sess, &tlv, &cause, &offending);
    ABTS_PTR_EQUAL(tc, qer, out);
    ABTS_TRUE(tc, !out->data_burst_size_marking && out->time_to_next_burst_marking);

    qer_indications_teardown();
}


/*
 * Maximum bit rate of a QER (TS 29.244 V19.6.0 clause 7.5.2.5, Maximum Bitrate; clause 8.2.115, Averaging Window) in the Create QER and
 * Update QER the UPF receives.
 */
static void pfcp_qer_rate_parse(abts_case *tc, void *data)
{
    ogs_pfcp_qer_t *qer = NULL, *out = NULL;
    ogs_pfcp_tlv_create_qer_t create;
    ogs_pfcp_tlv_update_qer_t update;
    uint8_t cause = 0, offending = 0;

    qer_indications_setup();
    qer = ogs_pfcp_qer_add(&sess);
    ABTS_PTR_NOTNULL(tc, qer);

    qer->mbr.uplink = 1000000;
    qer->mbr.downlink = 3000000;
    memset(&create, 0, sizeof(create));
    ogs_pfcp_build_create_qer(&create, 0, qer);
    qer->mbr.uplink = qer->mbr.downlink = 0;
    out = ogs_pfcp_handle_create_qer(&sess, &create, &cause, &offending);
    ABTS_PTR_NOTNULL(tc, out);
    ABTS_INT_EQUAL(tc, 1000000, (int)out->mbr.uplink);
    ABTS_INT_EQUAL(tc, 3000000, (int)out->mbr.downlink);
    ABTS_INT_EQUAL(tc, 0, (int)out->averaging_window);

    /* Averaging Window in milliseconds */
    create.averaging_window.presence = 1;
    create.averaging_window.u32 = 500;
    out = ogs_pfcp_handle_create_qer(&sess, &create, &cause, &offending);
    ABTS_PTR_NOTNULL(tc, out);
    ABTS_INT_EQUAL(tc, 500, (int)out->averaging_window);

    /* an Update QER changes the rate and the window, and keeps what it does not carry */
    qer->mbr.uplink = 2000000;
    qer->mbr.downlink = 5000000;
    memset(&update, 0, sizeof(update));
    ogs_pfcp_build_update_qer(&update, 0, qer, OGS_PFCP_MODIFY_QOS_MODIFY);
    qer->mbr.uplink = qer->mbr.downlink = 0;
    update.averaging_window.presence = 1;
    update.averaging_window.u32 = 200;
    out = ogs_pfcp_handle_update_qer(&sess, &update, &cause, &offending);
    ABTS_PTR_EQUAL(tc, qer, out);
    ABTS_INT_EQUAL(tc, 2000000, (int)out->mbr.uplink);
    ABTS_INT_EQUAL(tc, 5000000, (int)out->mbr.downlink);
    ABTS_INT_EQUAL(tc, 200, (int)out->averaging_window);

    qer_indications_teardown();
}

static void pfcp_qer_rate_allow(abts_case *tc, void *data)
{
    ogs_pfcp_qer_t qer;
    ogs_time_t now = 1000000; /* microseconds */
    int i, allowed;
    uint64_t bytes;

    /* no maximum bit rate: everything passes */
    memset(&qer, 0, sizeof(qer));
    for (i = 0; i < 1000; i++)
        ABTS_TRUE(tc, ogs_pfcp_qer_rate_allow(&qer, true, 1500, now, 2000));

    /* 1 Mbit/s downlink, window 1000 ms: a burst of 125000 bytes, then nothing until time passes; uplink has no maximum */
    memset(&qer, 0, sizeof(qer));
    qer.mbr.downlink = 1000000;
    allowed = 0;
    for (i = 0; i < 200; i++)
        if (ogs_pfcp_qer_rate_allow(&qer, true, 1250, now, 1000)) allowed++;
    ABTS_INT_EQUAL(tc, 100, allowed);
    ABTS_TRUE(tc, ogs_pfcp_qer_rate_allow(&qer, false, 1500, now, 1000));

    /* 100 ms later 12500 bytes were added: ten packets */
    now += 100000;
    allowed = 0;
    for (i = 0; i < 20; i++)
        if (ogs_pfcp_qer_rate_allow(&qer, true, 1250, now, 1000)) allowed++;
    ABTS_INT_EQUAL(tc, 10, allowed);

    /* the Averaging Window of the QER wins over the default: 100 ms gives a burst of 12500 bytes */
    memset(&qer, 0, sizeof(qer));
    qer.mbr.downlink = 1000000;
    qer.averaging_window = 100;
    allowed = 0;
    for (i = 0; i < 200; i++)
        if (ogs_pfcp_qer_rate_allow(&qer, true, 1250, now, 2000)) allowed++;
    ABTS_INT_EQUAL(tc, 10, allowed);

    /* at exactly the rate every packet passes: 1250 bytes every 10 ms for 10 s at 1 Mbit/s */
    memset(&qer, 0, sizeof(qer));
    qer.mbr.downlink = 1000000;
    allowed = 0;
    for (i = 0; i < 1000; i++) {
        now += 10000;
        if (ogs_pfcp_qer_rate_allow(&qer, true, 1250, now, 200)) allowed++;
    }
    ABTS_INT_EQUAL(tc, 1000, allowed);

    /* at twice the rate for 10 s no more than the rate and the first burst pass: 1250000 plus 25000 bytes (window 200 ms) */
    memset(&qer, 0, sizeof(qer));
    qer.mbr.downlink = 1000000;
    bytes = 0;
    for (i = 0; i < 2000; i++) {
        now += 5000;
        if (ogs_pfcp_qer_rate_allow(&qer, true, 1250, now, 200)) bytes += 1250;
    }
    ABTS_TRUE(tc, bytes <= 1275000);
    ABTS_TRUE(tc, bytes >= 1240000);
}

abts_suite *test_pfcp_qer(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, pfcp_qer_burst_create, NULL);
    abts_run_test(suite, pfcp_qer_burst_update, NULL);
    abts_run_test(suite, pfcp_qer_rate_parse, NULL);
    abts_run_test(suite, pfcp_qer_rate_allow, NULL);

    return suite;
}
