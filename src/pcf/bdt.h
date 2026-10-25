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

#ifndef PCF_BDT_H
#define PCF_BDT_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The bdt section of the configuration: the transfer policies that the PCF offers (TS 29.554 V19.2.1 clause 4.2.2.2) */
int pcf_bdt_parse_config(ogs_yaml_iter_t *parent);
void pcf_bdt_final(void);

/* Npcf_BDTPolicyControl: POST /bdtpolicies, GET, PATCH and DELETE /bdtpolicies/{bdtPolicyId}; content is the request body */
void pcf_npcf_bdtpolicycontrol_handle(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg, const char *content);

#ifdef __cplusplus
}
#endif

#endif /* PCF_BDT_H */
