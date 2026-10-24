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

#include "sbi-path.h"
#include "nbsf-handler.h"

/* Replace the PCF IP end points of a binding with those of a request. */
static void set_pcf_ip_end_points(bsf_sess_t *sess, OpenAPI_list_t *PcfIpEndPointList)
{
    OpenAPI_lnode_t *node = NULL;
    int i;

    for (i = 0; i < sess->num_of_pcf_ip; i++) {
        if (sess->pcf_ip[i].addr) {
            ogs_free(sess->pcf_ip[i].addr);
            sess->pcf_ip[i].addr = NULL;
        }
        if (sess->pcf_ip[i].addr6) {
            ogs_free(sess->pcf_ip[i].addr6);
            sess->pcf_ip[i].addr6 = NULL;
        }
        sess->pcf_ip[i].is_port = false;
        sess->pcf_ip[i].port = 0;
    }
    sess->num_of_pcf_ip = 0;

    OpenAPI_list_for_each(PcfIpEndPointList, node) {
        OpenAPI_ip_end_point_t *IpEndPoint = node->data;

        if (!IpEndPoint) continue;
        if (sess->num_of_pcf_ip >= OGS_SBI_MAX_NUM_OF_IP_ADDRESS) break;
        if (!IpEndPoint->ipv4_address && !IpEndPoint->ipv6_address) continue;

        if (IpEndPoint->ipv4_address)
            sess->pcf_ip[sess->num_of_pcf_ip].addr =
                ogs_strdup(IpEndPoint->ipv4_address);
        if (IpEndPoint->ipv6_address)
            sess->pcf_ip[sess->num_of_pcf_ip].addr6 =
                ogs_strdup(IpEndPoint->ipv6_address);
        if (IpEndPoint->is_port) {
            sess->pcf_ip[sess->num_of_pcf_ip].is_port = IpEndPoint->is_port;
            sess->pcf_ip[sess->num_of_pcf_ip].port = IpEndPoint->port;
        }
        sess->num_of_pcf_ip++;
    }
}

/*
 * The properties of the binding that the BSF keeps only as registered. They are borrowed from the stored copy, so the
 * binding that is built is not to be freed through OpenAPI_pcf_binding_free().
 */
static void set_registered_properties(
        OpenAPI_pcf_binding_t *dst, const bsf_sess_t *sess)
{
    const OpenAPI_pcf_binding_t *reg = sess->registered;

    if (!reg) return;
    dst->add_ipv6_prefixes = reg->add_ipv6_prefixes;
    dst->mac_addr48 = reg->mac_addr48;
    dst->add_mac_addrs = reg->add_mac_addrs;
    dst->pcf_diam_host = reg->pcf_diam_host;
    dst->pcf_diam_realm = reg->pcf_diam_realm;
    dst->pcf_sm_fqdn = reg->pcf_sm_fqdn;
    dst->pcf_sm_ip_end_points = reg->pcf_sm_ip_end_points;
    dst->pcf_id = reg->pcf_id;
    dst->pcf_set_id = reg->pcf_set_id;
    dst->recovery_time = reg->recovery_time;
    dst->para_com = reg->para_com;
    dst->bind_level = reg->bind_level;
}

bool bsf_nbsf_management_handle_pcf_binding(
        bsf_sess_t *sess, ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg)
{
    int status = 0;
    char *strerror = NULL;
    ogs_sbi_server_t *server = NULL;

    ogs_sbi_message_t sendmsg;
    ogs_sbi_header_t header;
    ogs_sbi_response_t *response = NULL;

    OpenAPI_pcf_binding_t *RecvPcfBinding = NULL;
    OpenAPI_pcf_binding_t SendPcfBinding;
    OpenAPI_snssai_t Snssai;

    ogs_assert(stream);
    ogs_assert(recvmsg);
    server = ogs_sbi_server_from_stream(stream);
    ogs_assert(server);

    if (recvmsg->h.resource.component[1]) {
        SWITCH(recvmsg->h.method)
        CASE(OGS_SBI_HTTP_METHOD_DELETE)
            memset(&sendmsg, 0, sizeof(sendmsg));

            response = ogs_sbi_build_response(
                    &sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
            ogs_assert(response);
            ogs_assert(true == ogs_sbi_server_send_response(stream, response));

            ogs_free(sendmsg.http.location);

            bsf_sess_remove(sess);
            break;

        CASE(OGS_SBI_HTTP_METHOD_PATCH)
            /*
             * TS 29.521 clause 5.3.3.3.2: the PcfBindingPatch updates the PCF address information of the binding; the
             * successful answer is 200 with the PcfBinding.
             */
            if (!recvmsg->PcfBindingPatch) {
                strerror = ogs_msprintf("No PcfBindingPatch");
                status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
                goto cleanup;
            }
            if (recvmsg->PcfBindingPatch->pcf_fqdn) {
                if (sess->pcf_fqdn)
                    ogs_free(sess->pcf_fqdn);
                sess->pcf_fqdn = ogs_strdup(recvmsg->PcfBindingPatch->pcf_fqdn);
                ogs_assert(sess->pcf_fqdn);
            }
            if (recvmsg->PcfBindingPatch->pcf_ip_end_points)
                set_pcf_ip_end_points(
                        sess, recvmsg->PcfBindingPatch->pcf_ip_end_points);

            memset(&Snssai, 0, sizeof(Snssai));
            Snssai.sst = sess->s_nssai.sst;
            Snssai.sd = ogs_s_nssai_sd_to_string(sess->s_nssai.sd);
            memset(&SendPcfBinding, 0, sizeof(SendPcfBinding));
            SendPcfBinding.dnn = sess->dnn;
            SendPcfBinding.snssai = &Snssai;
            if (sess->pcf_fqdn && strlen(sess->pcf_fqdn))
                SendPcfBinding.pcf_fqdn = sess->pcf_fqdn;
            set_registered_properties(&SendPcfBinding, sess);

            if (sess->num_of_pcf_ip) {
                int i;

                SendPcfBinding.pcf_ip_end_points = OpenAPI_list_create();
                ogs_assert(SendPcfBinding.pcf_ip_end_points);
                for (i = 0; i < sess->num_of_pcf_ip; i++) {
                    OpenAPI_ip_end_point_t *PcfIpEndPoint =
                        ogs_calloc(1, sizeof(*PcfIpEndPoint));

                    ogs_assert(PcfIpEndPoint);
                    PcfIpEndPoint->ipv4_address = sess->pcf_ip[i].addr;
                    PcfIpEndPoint->ipv6_address = sess->pcf_ip[i].addr6;
                    PcfIpEndPoint->is_port = sess->pcf_ip[i].is_port;
                    PcfIpEndPoint->port = sess->pcf_ip[i].port;
                    OpenAPI_list_add(
                            SendPcfBinding.pcf_ip_end_points, PcfIpEndPoint);
                }
            }

            memset(&sendmsg, 0, sizeof(sendmsg));
            sendmsg.PcfBinding = &SendPcfBinding;
            response = ogs_sbi_build_response(
                    &sendmsg, OGS_SBI_HTTP_STATUS_OK);
            ogs_assert(response);
            ogs_assert(true == ogs_sbi_server_send_response(stream, response));

            {
                OpenAPI_lnode_t *node = NULL;

                OpenAPI_list_for_each(SendPcfBinding.pcf_ip_end_points, node) {
                    if (node->data)
                        ogs_free(node->data);
                }
                OpenAPI_list_free(SendPcfBinding.pcf_ip_end_points);
            }
            if (Snssai.sd)
                ogs_free(Snssai.sd);
            break;

        DEFAULT
            strerror = ogs_msprintf("Invalid HTTP method [%s]",
                    recvmsg->h.method);
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            goto cleanup;
        END
    } else {
        OpenAPI_list_t *PcfIpEndPointList = NULL;
        OpenAPI_lnode_t *node = NULL;
        int i;

        SWITCH(recvmsg->h.method)
        CASE(OGS_SBI_HTTP_METHOD_POST)

            RecvPcfBinding = recvmsg->PcfBinding;
            if (!RecvPcfBinding) {
                strerror = ogs_msprintf("No PcfBinding");
                status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
                goto cleanup;
            }

            if (!RecvPcfBinding->snssai) {
                strerror = ogs_msprintf("No S-NSSAI");
                status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
                goto cleanup;
            }

            if (!RecvPcfBinding->dnn) {
                strerror = ogs_msprintf("No DNN");
                status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
                goto cleanup;
            }

            if (!RecvPcfBinding->pcf_fqdn &&
                !RecvPcfBinding->pcf_ip_end_points) {
                strerror = ogs_msprintf("No PCF address information [%p:%p]",
                            RecvPcfBinding->pcf_fqdn,
                            RecvPcfBinding->pcf_ip_end_points);
                status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
                goto cleanup;
            }

            if (sess->registered)
                OpenAPI_pcf_binding_free(sess->registered);
            sess->registered = OpenAPI_pcf_binding_copy(NULL, RecvPcfBinding);

            if (RecvPcfBinding->pcf_fqdn) {
                if (sess->pcf_fqdn)
                    ogs_free(sess->pcf_fqdn);
                sess->pcf_fqdn = ogs_strdup(RecvPcfBinding->pcf_fqdn);
                ogs_assert(sess->pcf_fqdn);
            }


            if (RecvPcfBinding->ipv4_frame_route_list) {
                OpenAPI_lnode_t *node = NULL;

                OpenAPI_clear_and_free_string_list(sess->ipv4_frame_route_list);
                sess->ipv4_frame_route_list = OpenAPI_list_create();
                OpenAPI_list_for_each(RecvPcfBinding->ipv4_frame_route_list, node) {
                    if (!node->data)
                        continue;
                    OpenAPI_list_add(sess->ipv4_frame_route_list, ogs_strdup(node->data));
                }
            }

            if (RecvPcfBinding->ipv6_frame_route_list) {
                OpenAPI_lnode_t *node = NULL;

                OpenAPI_clear_and_free_string_list(sess->ipv6_frame_route_list);
                sess->ipv6_frame_route_list = OpenAPI_list_create();
                OpenAPI_list_for_each(RecvPcfBinding->ipv6_frame_route_list, node) {
                    if (!node->data)
                        continue;
                    OpenAPI_list_add(sess->ipv6_frame_route_list, ogs_strdup(node->data));
                }
            }

            if (!bsf_sess_index_registered(sess)) {
                strerror = ogs_msprintf("Invalid MAC address");
                status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
                goto cleanup;
            }

            sess->s_nssai.sst = RecvPcfBinding->snssai->sst;
            sess->s_nssai.sd =
                ogs_s_nssai_sd_from_string(RecvPcfBinding->snssai->sd);

            sess->dnn = ogs_strdup(RecvPcfBinding->dnn);
            ogs_assert(sess->dnn);

            PcfIpEndPointList = RecvPcfBinding->pcf_ip_end_points;

            if (PcfIpEndPointList) {
                for (i = 0; i < sess->num_of_pcf_ip; i++) {
                    if (sess->pcf_ip[i].addr)
                        ogs_free(sess->pcf_ip[i].addr);
                    if (sess->pcf_ip[i].addr6)
                        ogs_free(sess->pcf_ip[i].addr6);
                }
                sess->num_of_pcf_ip = 0;

                OpenAPI_list_for_each(PcfIpEndPointList, node) {
                    OpenAPI_ip_end_point_t *IpEndPoint = node->data;

                    if (!IpEndPoint) continue;

                    if (sess->num_of_pcf_ip < OGS_SBI_MAX_NUM_OF_IP_ADDRESS) {
                        if (IpEndPoint->ipv4_address ||
                            IpEndPoint->ipv6_address) {
                            if (IpEndPoint->ipv4_address) {
                                sess->pcf_ip[sess->num_of_pcf_ip].addr =
                                    ogs_strdup(IpEndPoint->ipv4_address);
                            }
                            if (IpEndPoint->ipv6_address) {
                                sess->pcf_ip[sess->num_of_pcf_ip].addr6 =
                                    ogs_strdup(IpEndPoint->ipv6_address);
                            }
                            if (IpEndPoint->is_port) {
                                sess->pcf_ip[sess->num_of_pcf_ip].is_port =
                                    IpEndPoint->is_port;
                                sess->pcf_ip[sess->num_of_pcf_ip].port =
                                    IpEndPoint->port;
                            }
                            sess->num_of_pcf_ip++;
                        }
                    }
                }
            }

            if (RecvPcfBinding->supi) {
                if (sess->supi)
                    ogs_free(sess->supi);
                sess->supi = ogs_strdup(RecvPcfBinding->supi);
            }
            if (RecvPcfBinding->gpsi) {
                if (sess->gpsi)
                    ogs_free(sess->gpsi);
                sess->gpsi = ogs_strdup(RecvPcfBinding->gpsi);
            }
            if (RecvPcfBinding->ip_domain) {
                if (sess->ip_domain)
                    ogs_free(sess->ip_domain);
                sess->ip_domain = ogs_strdup(RecvPcfBinding->ip_domain);
            }

            memset(&header, 0, sizeof(header));
            header.service.name =
                OpenAPI_service_name_ToString(
                        OpenAPI_service_name_nbsf_management);
            header.api.version = (char *)OGS_SBI_API_V1;
            header.resource.component[0] =
                (char *)OGS_SBI_RESOURCE_NAME_PCF_BINDINGS;
            header.resource.component[1] = sess->binding_id;

            if (RecvPcfBinding->supp_feat) {
                uint64_t supported_features =
                    ogs_uint64_from_string_hexadecimal(
                            RecvPcfBinding->supp_feat);
                sess->management_features &= supported_features;

                if (sess->management_features != supported_features) {
                    ogs_free(RecvPcfBinding->supp_feat);
                    RecvPcfBinding->supp_feat =
                        ogs_uint64_to_string(sess->management_features);
                    ogs_assert(RecvPcfBinding->supp_feat);
                }
            } else {
                sess->management_features = 0;
            }

            memset(&sendmsg, 0, sizeof(sendmsg));
            sendmsg.PcfBinding = RecvPcfBinding;
            sendmsg.http.location = ogs_sbi_server_uri(server, &header);

            response = ogs_sbi_build_response(
                    &sendmsg, OGS_SBI_HTTP_STATUS_CREATED);
            ogs_assert(response);
            ogs_assert(true == ogs_sbi_server_send_response(stream, response));

            ogs_free(sendmsg.http.location);
            break;

        CASE(OGS_SBI_HTTP_METHOD_GET)
            if (sess->num_of_pcf_ip ||
                (sess->pcf_fqdn && strlen(sess->pcf_fqdn))) {
                memset(&Snssai, 0, sizeof(Snssai));
                Snssai.sst = sess->s_nssai.sst;
                Snssai.sd = ogs_s_nssai_sd_to_string(sess->s_nssai.sd);

                memset(&SendPcfBinding, 0, sizeof(SendPcfBinding));

                SendPcfBinding.dnn = sess->dnn;
                SendPcfBinding.snssai = &Snssai;

                /* TS 29.521 V19.7.0 clause 4.2.4.2: the PcfBinding as the PCF registered it */
                SendPcfBinding.supi = sess->supi;
                SendPcfBinding.gpsi = sess->gpsi;
                SendPcfBinding.ip_domain = sess->ip_domain;
                SendPcfBinding.ipv4_addr = sess->ipv4addr_string;
                SendPcfBinding.ipv6_prefix = sess->ipv6prefix_string;
                SendPcfBinding.ipv4_frame_route_list =
                    sess->ipv4_frame_route_list;
                SendPcfBinding.ipv6_frame_route_list =
                    sess->ipv6_frame_route_list;
                set_registered_properties(&SendPcfBinding, sess);
                if (sess->management_features)
                    SendPcfBinding.supp_feat =
                        ogs_uint64_to_string(sess->management_features);

                PcfIpEndPointList = OpenAPI_list_create();
                ogs_assert(PcfIpEndPointList);

                if (sess->pcf_fqdn && strlen(sess->pcf_fqdn))
                    SendPcfBinding.pcf_fqdn = ogs_strdup(sess->pcf_fqdn);

                for (i = 0; i < sess->num_of_pcf_ip; i++) {
                    OpenAPI_ip_end_point_t *PcfIpEndPoint = NULL;

                    ogs_assert(sess->pcf_ip[i].addr || sess->pcf_ip[i].addr6);

                    PcfIpEndPoint = ogs_calloc(1, sizeof(*PcfIpEndPoint));
                    ogs_assert(PcfIpEndPoint);
                    PcfIpEndPoint->ipv4_address = sess->pcf_ip[i].addr;
                    PcfIpEndPoint->ipv6_address = sess->pcf_ip[i].addr6;

                    PcfIpEndPoint->is_port = sess->pcf_ip[i].is_port;
                    PcfIpEndPoint->port = sess->pcf_ip[i].port;

                    OpenAPI_list_add(PcfIpEndPointList, PcfIpEndPoint);
                }

                if (PcfIpEndPointList->count)
                    SendPcfBinding.pcf_ip_end_points = PcfIpEndPointList;
                else
                    OpenAPI_list_free(PcfIpEndPointList);

                memset(&sendmsg, 0, sizeof(sendmsg));
                sendmsg.PcfBinding = &SendPcfBinding;

                response = ogs_sbi_build_response(
                            &sendmsg, OGS_SBI_HTTP_STATUS_OK);
                ogs_assert(response);

                ogs_assert(true ==
                        ogs_sbi_server_send_response(stream, response));

                OpenAPI_list_for_each(SendPcfBinding.pcf_ip_end_points, node) {
                    OpenAPI_ip_end_point_t *PcfIpEndPoint = node->data;
                    if (PcfIpEndPoint)
                        ogs_free(PcfIpEndPoint);
                }
                OpenAPI_list_free(SendPcfBinding.pcf_ip_end_points);

                if (SendPcfBinding.pcf_fqdn)
                    ogs_free(SendPcfBinding.pcf_fqdn);
                if (SendPcfBinding.supp_feat)
                    ogs_free(SendPcfBinding.supp_feat);
                if (Snssai.sd)
                    ogs_free(Snssai.sd);
            } else {
                memset(&sendmsg, 0, sizeof(sendmsg));
                response = ogs_sbi_build_response(
                            &sendmsg, OGS_SBI_HTTP_STATUS_NO_CONTENT);
                ogs_assert(response);

                ogs_assert(true ==
                    ogs_sbi_server_send_response(stream, response));
            }
            break;

        DEFAULT
            strerror = ogs_msprintf("Invalid HTTP method [%s]",
                    recvmsg->h.method);
            status = OGS_SBI_HTTP_STATUS_BAD_REQUEST;
            goto cleanup;
        END
    }

    return true;

cleanup:
    ogs_assert(strerror);
    ogs_assert(status);
    ogs_error("%s", strerror);
    ogs_assert(true ==
        ogs_sbi_server_send_error(stream, status, recvmsg, strerror, NULL,
                NULL));
    ogs_free(strerror);

    return false;
}
