/*
 * Copyright (C) 2019-2023 by Sukchan Lee <acetcom@gmail.com>
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

#ifndef AF_CONTEXT_H
#define AF_CONTEXT_H

#include "ogs-sbi.h"
#include "ogs-app.h"

#include "event.h"
#include "local.h"
#include "af-sm.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int __af_log_domain;

#undef OGS_LOG_DOMAIN
#define OGS_LOG_DOMAIN __af_log_domain

typedef struct af_context_s {
    ogs_hash_t      *supi_hash;     /* hash table (SUPI) */
    ogs_hash_t      *ipv4_hash;     /* hash table (IPv4 Address) */
    ogs_hash_t      *ipv6_hash;     /* hash table (IPv6 Address) */
    ogs_hash_t      *pcf_app_session_id_hash; /* hash table (AppSessionId) */

    ogs_list_t      sess_list;
} af_context_t;

typedef struct af_sess_s af_sess_t;

typedef struct af_sess_s {
    ogs_sbi_object_t sbi;

    uint64_t policyauthorization_features;
    int last_pcf_status;
    char last_pcf_asc_af_app_id[64]; /* afAppId and the qosRef of the first media component of the last AppSessionContext that the PCF returned */
    char last_pcf_asc_qos_ref[64];
    char last_pcf_asc_events[160];   /* the events of the evSubsc of the last AppSessionContext, separated by commas */
    char last_pcf_cause[64];
    int notify_count;                 /* the Npcf_PolicyAuthorization_Notify requests of the PCF (TS 29.514 V19.8.0 clause 4.2.5.2) */
    char notify_event[64];            /* the event of the first evNotifs entry of the last one */
    int notify_med_comp_n;            /* the medCompN of its first flow, -1 if none */
    int notify_l4s_count;             /* the notifications of the event L4S_SUPP, notifType and medCompN of the first flow of the last one */
    char notify_l4s_type[24];
    int notify_l4s_med_comp_n;
    int notify_qm_count;              /* the notifications of the event QOS_MONITORING */
    long notify_qm_dl, notify_qm_ul, notify_qm_rt; /* the first delay of the last one, -1 if none */
    int notify_qm_med_comp_n;
    int notify_qnc_count;             /* the notifications of the event QOS_NOTIF */
    char notify_qnc_type[24];         /* notifType of the last, medCompN of its first flow */
    int notify_qnc_med_comp_n;
    long notify_usg_total;            /* usgRep.totalVolume of the last notification that carried a usgRep, -1 if none yet */
    int notify_usg_count;             /* the notifications of the event USAGE_REPORT */
    long delete_usg_total;            /* usgRep.totalVolume in the answer of the last deletion, -1 if none */
    char notify_failed_status[16];    /* mcResourcStatus of its first failedResourcAllocReports entry, empty if none */  /* the cause of the ProblemDetails of the last answer of the PCF, empty if none */

#define PCF_APP_SESSION_ASSOCIATED(__sESS) \
    ((__sESS) && ((__sESS)->app_session.pcf_id))
#define PCF_APP_SESSION_CLEAR(__sESS) \
    do { \
        ogs_assert((__sESS)); \
        if ((__sESS)->app_session.pcf.resource_uri) \
            ogs_free((__sESS)->app_session.pcf.resource_uri); \
        (__sESS)->app_session.pcf.resource_uri = NULL; \
        if ((__sESS)->app_session.pcf.id) \
            ogs_free((__sESS)->app_session.pcf.id); \
        (__sESS)->app_session.pcf.id = NULL; \
    } while(0)
#define PCF_APP_SESSION_STORE(__sESS, __rESOURCE_URI, __iD) \
    do { \
        ogs_assert((__sESS)); \
        ogs_assert((__rESOURCE_URI)); \
        ogs_assert((__iD)); \
        PCF_APP_SESSION_CLEAR(__sESS); \
        (__sESS)->app_session.pcf.resource_uri = ogs_strdup(__rESOURCE_URI); \
        ogs_assert((__sESS)->app_session.pcf.resource_uri); \
        (__sESS)->app_session.pcf.id = ogs_strdup(__iD); \
        ogs_assert((__sESS)->app_session.pcf.id); \
    } while(0)
    struct {
        struct {
            char *id;
        } af;
        struct {
            char *resource_uri;
            char *id;
            ogs_sbi_client_t *client;
        } pcf;
    } app_session;

    char *ipv4addr;
    char *ipv6addr;
    char *ipv6prefix;

    char *supi;
    char *gpsi;

    ogs_s_nssai_t s_nssai;
    char *dnn;

    struct {
        char *fqdn;

        int num_of_ip;
        struct {
            ogs_sockaddr_t *addr;
            ogs_sockaddr_t *addr6;
            bool is_port;
            int port;
        } ip[OGS_SBI_MAX_NUM_OF_IP_ADDRESS];

        ogs_sbi_client_t *client;
    } pcf;

} af_sess_t;

void af_context_init(void);
void af_context_final(void);
af_context_t *af_self(void);

int af_context_parse_config(void);

af_sess_t *af_sess_add_by_ue_address(ogs_ip_t *ue_address);
void af_sess_remove(af_sess_t *sess);
void af_sess_remove_all(void);

af_sess_t *af_sess_find(uint32_t index);
af_sess_t *af_sess_find_by_af_app_session_id(char *af_app_session_id);

void af_sess_associate_pcf_client(af_sess_t *sess);

#ifdef __cplusplus
}
#endif

#endif /* AF_CONTEXT_H */
