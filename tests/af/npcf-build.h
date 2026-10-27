/*
 * Copyright (C) 2019,2020 by Sukchan Lee <acetcom@gmail.com>
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

#ifndef AF_NPCF_BUILD_H
#define AF_NPCF_BUILD_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct af_npcf_policyauthorization_param_s {
    OpenAPI_media_type_e med_type;
    int flow_type;
    int qos_type;
    const char *qos_reference;
    bool omit_med_type;
    const char *af_charg_id;       /* the AF charging identifier (afChargId), NULL for none */
    bool ev_resources_allocation;  /* subscribe to SUCCESSFUL_RESOURCES_ALLOCATION and FAILED_RESOURCES_ALLOCATION */
    const char *sponsor_id;        /* sponId and aspId: sponsored data connectivity (TS 29.514 V19.8.0 clause 4.2.2.5), NULL for none */
    bool ev_qos_mon;               /* subscribe to QOS_MONITORING for the packet delay (TS 29.514 V19.8.0 clause 4.2.2.23) */
    int qos_mon_threshold;         /* repThreshDl, repThreshUl and repThreshRp in ms (event triggered reporting) */
    int qos_mon_wait_time;         /* waitTime in seconds */
    int l4s_ind;                   /* l4sInd of the media component: 1 UL, 2 DL, 3 UL_DL, 0 none (TS 29.514 V19.8.0 clause 4.2.2.43) */
    bool ev_l4s;                   /* subscribe to L4S_SUPP */
    bool ev_qos_notif;             /* subscribe to QOS_NOTIF (TS 29.514 V19.8.0 clause 4.2.2.6) */
    long usage_threshold;          /* with the event USAGE_REPORT: the totalVolume of usgThres in bytes, 0 for none */
    const char *alt_ser_req1;      /* altSerReqs of the media component (TS 29.514 V19.8.0 clause 4.2.2.32), highest priority first, NULL for none */
    const char *alt_ser_req2;
    bool proto_desc;               /* protoDescDl of the media component: RTP (clause 4.2.2.39) */
    int proto_desc_he_id;          /* with proto_desc: the ID of the RTP header extension for PDU Set marking in rtpHeaderExtInfo, 0 for none */
    int proto_desc_pt;             /* with proto_desc: the one RTP payload type of rtpPayloadInfoList, 0 for none */
    bool burst_inds;               /* datBurstSizeInd and timetoNextBurstInd (clause 4.2.2.48) */
    bool exp_tran;                 /* expTranInd (clause 4.2.2.48) */
    bool mpx;                      /* mpxMediaUlInfos and mpxMediaDlInfos of the first sub component (clause 4.2.2.47) */
    bool no_med_components;        /* create: no medComponents in the AscReqData, only the events subscription (TS 29.514 V19.8.0 table 5.6.2.3-1: medComponents is optional) */
    int mpx_dl_ssrc, mpx_dl_pt;    /* with mpx: the SSRC and the payload type of the downlink information instead of 2222 and 97, 0 for the default */
    bool pdu_set_qos;              /* pduSetQosDl of the media component (clause 4.2.2.39): a delay budget, an error rate and the integrated handling information */
} af_npcf_policyauthorization_param_t;

ogs_sbi_request_t *af_npcf_policyauthorization_build_create(
        af_sess_t *sess, void *data);
ogs_sbi_request_t *af_npcf_policyauthorization_build_update(
        af_sess_t *sess, void *data);
ogs_sbi_request_t *af_npcf_policyauthorization_build_remove_media(
        af_sess_t *sess, void *data);
/* A request with the given HTTP method, the path after the application session and a JSON body, as the NF service consumer may send them */
typedef struct af_npcf_policyauthorization_raw_s {
    const char *method;
    const char *path;  /* appended to the URI of the application session, e.g. "/events-subscription" */
    const char *body;  /* JSON text, or NULL */
} af_npcf_policyauthorization_raw_t;

/* A request to the Npcf_BDTPolicyControl service of the PCF of the application session (TS 29.554 V19.2.1 clause 5.3) */
typedef struct af_npcf_bdtpolicycontrol_raw_s {
    const char *method;
    const char *path;  /* appended to .../bdtpolicies, e.g. "/1" */
    const char *body;  /* JSON text, or NULL */
} af_npcf_bdtpolicycontrol_raw_t;

ogs_sbi_request_t *af_npcf_bdtpolicycontrol_build_raw(
        af_sess_t *sess, void *data);

ogs_sbi_request_t *af_npcf_policyauthorization_build_raw(
        af_sess_t *sess, void *data);
ogs_sbi_request_t *af_npcf_policyauthorization_build_get(
        af_sess_t *sess, void *data);
ogs_sbi_request_t *af_npcf_policyauthorization_build_delete(
        af_sess_t *sess, void *data);

ogs_sbi_request_t *af_npcf_policyauthorization_build_create_video(
        af_sess_t *sess, void *data);

#ifdef __cplusplus
}
#endif

#endif /* AF_NPCF_BUILD_H */
