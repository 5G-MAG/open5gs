/*
 * Copyright (C) 2023 by Sukchan Lee <acetcom@gmail.com>
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

#include "ogs-sbi.h"
#include "core/abts.h"

static void proto_message_test1(abts_case *tc, void *data)
{
    char *fqdn = NULL;
    ogs_plmn_id_t plmn_id1, plmn_id2;

    ogs_plmn_id_build(&plmn_id1, 456, 123, 3);
    fqdn = ogs_home_network_domain_from_plmn_id(&plmn_id1);
    ABTS_STR_EQUAL(tc, "5gc.mnc123.mcc456.3gppnetwork.org", fqdn);
    ABTS_INT_EQUAL(tc, 456, ogs_plmn_id_mcc_from_fqdn(fqdn));
    ABTS_INT_EQUAL(tc, 123, ogs_plmn_id_mnc_from_fqdn(fqdn));
    ogs_free(fqdn);

    ogs_plmn_id_build(&plmn_id1, 1, 1, 2);
    fqdn = ogs_home_network_domain_from_plmn_id(&plmn_id1);
    ABTS_STR_EQUAL(tc, "5gc.mnc001.mcc001.3gppnetwork.org", fqdn);
    ABTS_INT_EQUAL(tc, 1, ogs_plmn_id_mcc_from_fqdn(fqdn));
    ABTS_INT_EQUAL(tc, 1, ogs_plmn_id_mnc_from_fqdn(fqdn));
    ogs_free(fqdn);

    ogs_plmn_id_build(&plmn_id1, 310, 14, 3);
    fqdn = ogs_home_network_domain_from_plmn_id(&plmn_id1);
    ABTS_STR_EQUAL(tc, "5gc.mnc014.mcc310.3gppnetwork.org", fqdn);
    ABTS_INT_EQUAL(tc, 310, ogs_plmn_id_mcc_from_fqdn(fqdn));
    ABTS_INT_EQUAL(tc, 14, ogs_plmn_id_mnc_from_fqdn(fqdn));
    ogs_free(fqdn);
}

static void proto_message_test2(abts_case *tc, void *data)
{
    char *dnn_oi = NULL;
    char *full_dnn = NULL;
    char dnn_ni[OGS_MAX_DNN_LEN+1];
    ogs_plmn_id_t plmn_id1, plmn_id2;

    ogs_plmn_id_build(&plmn_id1, 456, 123, 3);
    dnn_oi = ogs_dnn_oi_from_plmn_id(&plmn_id1);
    ABTS_STR_EQUAL(tc,
            "mnc123.mcc456.gprs", dnn_oi);
    full_dnn = ogs_msprintf("internet.realm.%s", dnn_oi);
    ABTS_STR_EQUAL(tc,
            "internet.realm.mnc123.mcc456.gprs", full_dnn);
    ABTS_STR_EQUAL(tc,
            dnn_oi, ogs_dnn_oi_from_fqdn(full_dnn));

    ogs_cpystrn(dnn_ni, full_dnn,
            ogs_min(OGS_MAX_DNN_LEN,
                ogs_dnn_oi_from_fqdn(full_dnn) - full_dnn));
    ABTS_STR_EQUAL(tc, "internet.realm", dnn_ni);

    ABTS_INT_EQUAL(tc, 456, ogs_plmn_id_mcc_from_fqdn(full_dnn));
    ABTS_INT_EQUAL(tc, 123, ogs_plmn_id_mnc_from_fqdn(full_dnn));
    ogs_free(dnn_oi);
    ogs_free(full_dnn);
}

/* A media component that names every flow of the rule and one more is not the flows of the rule (TS 29.514 V19.8.0 clause 4.2.3.2) */
static void proto_flow_test3(abts_case *tc, void *data)
{
    ogs_pcc_rule_t rule;
    ogs_media_component_t media;

    memset(&rule, 0, sizeof(rule));
    memset(&media, 0, sizeof(media));
    media.num_of_sub = 1;
    media.sub[0].num_of_flow = 1;
    media.sub[0].flow[0].description = ogs_strdup("permit out 17 from 10.1.1.1 2000 to 10.45.0.2 1000");
    ABTS_INT_EQUAL(tc, OGS_OK, ogs_pcc_rule_install_flow_from_media(&rule, &media));
    ABTS_INT_EQUAL(tc, 1, rule.num_of_flow);

    /* the same flow again: equal */
    ABTS_INT_EQUAL(tc, rule.num_of_flow, ogs_pcc_rule_num_of_flow_equal_to_media(&rule, &media));

    /* the same flow and a new one: different */
    media.sub[0].num_of_flow = 2;
    media.sub[0].flow[1].description = ogs_strdup("permit out 17 from 10.1.1.1 2001 to 10.45.0.2 1001");
    ABTS_TRUE(tc, rule.num_of_flow != ogs_pcc_rule_num_of_flow_equal_to_media(&rule, &media));

    /* only the new flow: different */
    media.sub[0].flow[0].description = media.sub[0].flow[1].description;
    media.sub[0].flow[1].description = NULL;
    media.sub[0].num_of_flow = 1;
    ABTS_TRUE(tc, rule.num_of_flow != ogs_pcc_rule_num_of_flow_equal_to_media(&rule, &media));

    ogs_free(media.sub[0].flow[0].description);
    OGS_PCC_RULE_FREE(&rule);
}

abts_suite *test_proto_message(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, proto_message_test1, NULL);
    abts_run_test(suite, proto_message_test2, NULL);
    abts_run_test(suite, proto_flow_test3, NULL);

    return suite;
}
