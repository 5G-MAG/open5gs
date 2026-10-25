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

#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <time.h>
#include <ctype.h>

#include "bdt.h"

/*
 * Npcf_BDTPolicyControl (TS 29.554 V19.2.1). The bodies are read and written as JSON: the generated models of BdtPolicyData and
 * TransferPolicy are those of the Nudr_DataRepository, which share the names but not the attributes. The transfer policies are
 * held locally (clause 4.2.2.2, NOTE 1: with one PCF the interaction with the UDR is not required).
 */

#define BDT_MAX_TRANSFER_POLICIES 16

typedef struct bdt_transfer_conf_s {
    int         rating_group;
    uint64_t    max_bit_rate_dl;
    uint64_t    max_bit_rate_ul;
    bool        has_window;
    int         start_min;      /* of the day, UTC */
    int         end_min;
} bdt_transfer_conf_t;

static struct {
    bdt_transfer_conf_t conf[BDT_MAX_TRANSFER_POLICIES];
    int                 num_of_conf;
} bdt_self;

typedef struct bdt_policy_s {
    ogs_lnode_t lnode;
    char        *id;            /* bdtPolicyId of the resource URI */
    cJSON       *doc;           /* the BdtPolicy: bdtPolData and bdtReqData */
    uint64_t    features;       /* negotiated, TS 29.554 V19.2.1 table 5.8-1 */
} bdt_policy_t;

static OGS_LIST(bdt_policy_list);
static uint32_t bdt_policy_seq;

/* Features of table 5.8-1 that this PCF supports: only PatchCorrection (3) */
#define BDT_FEATURE_PATCH_CORRECTION 3
#define BDT_SUPPORTED_FEATURES ((uint64_t)1 << (BDT_FEATURE_PATCH_CORRECTION - 1))

static int parse_time_of_day(const char *s)
{
    int hh, mm;
    char tail;

    if (!s || sscanf(s, "%2d:%2d%c", &hh, &mm, &tail) != 2 || hh < 0 || hh > 23 || mm < 0 || mm > 59)
        return -1;
    return hh * 60 + mm;
}

int pcf_bdt_parse_config(ogs_yaml_iter_t *parent)
{
    ogs_yaml_iter_t bdt_iter;

    ogs_assert(parent);

    ogs_yaml_iter_recurse(parent, &bdt_iter);
    while (ogs_yaml_iter_next(&bdt_iter)) {
        const char *key = ogs_yaml_iter_key(&bdt_iter);
        ogs_yaml_iter_t array, item;

        ogs_assert(key);
        if (strcmp(key, "transfer_policies")) {
            ogs_warn("unknown bdt key `%s`", key);
            continue;
        }

        ogs_yaml_iter_recurse(&bdt_iter, &array);
        do {
            bdt_transfer_conf_t c;
            bool has_rating_group = false;

            memset(&c, 0, sizeof(c));
            OGS_YAML_ARRAY_NEXT(&array, &item);
            while (ogs_yaml_iter_next(&item)) {
                const char *k = ogs_yaml_iter_key(&item);
                const char *v = NULL;
                ogs_yaml_iter_t value;

                ogs_assert(k);
                /* the value of the pair is a scalar, or a mapping for window */
                ogs_yaml_iter_recurse(&item, &value);
                if (ogs_yaml_iter_type(&value) == YAML_SCALAR_NODE)
                    v = ogs_yaml_iter_value(&item);
                if (!strcmp(k, "rating_group") && v) {
                    c.rating_group = (int)strtol(v, NULL, 10);
                    has_rating_group = true;
                } else if (!strcmp(k, "max_bit_rate_dl") && v) {
                    c.max_bit_rate_dl = strtoull(v, NULL, 10);
                } else if (!strcmp(k, "max_bit_rate_ul") && v) {
                    c.max_bit_rate_ul = strtoull(v, NULL, 10);
                } else if (!strcmp(k, "window")) {
                    ogs_yaml_iter_t w;
                    const char *start = NULL, *end = NULL;

                    ogs_yaml_iter_recurse(&item, &w);
                    while (ogs_yaml_iter_next(&w)) {
                        const char *wk = ogs_yaml_iter_key(&w);

                        if (!strcmp(wk, "start"))
                            start = ogs_yaml_iter_value(&w);
                        else if (!strcmp(wk, "end"))
                            end = ogs_yaml_iter_value(&w);
                        else
                            ogs_warn("unknown bdt window key `%s`", wk);
                    }
                    c.start_min = parse_time_of_day(start);
                    c.end_min = parse_time_of_day(end);
                    c.has_window = c.start_min >= 0 && c.end_min >= 0 && c.start_min != c.end_min;
                    if (!c.has_window)
                        ogs_warn("Ignore the bdt window: start and end are different times of the day as hh:mm");
                } else {
                    ogs_warn("unknown bdt transfer_policies key `%s`", k);
                }
            }

            if (!has_rating_group || c.rating_group < 0) {
                ogs_warn("Ignore the bdt transfer policy without a rating_group");
                continue;
            }
            if (bdt_self.num_of_conf >= BDT_MAX_TRANSFER_POLICIES) {
                ogs_warn("Ignore the bdt transfer policy beyond max [%d]", BDT_MAX_TRANSFER_POLICIES);
                continue;
            }
            bdt_self.conf[bdt_self.num_of_conf++] = c;
        } while (ogs_yaml_iter_type(&array) == YAML_SEQUENCE_NODE);
    }

    return OGS_OK;
}

void pcf_bdt_final(void)
{
    bdt_policy_t *p = NULL, *next = NULL;

    ogs_list_for_each_safe(&bdt_policy_list, next, p) {
        ogs_list_remove(&bdt_policy_list, p);
        ogs_free(p->id);
        cJSON_Delete(p->doc);
        ogs_free(p);
    }
}

/* RFC 3339 date-time, as the DateTime of TS 29.571 */
static bool bdt_time_parse(const char *s, time_t *out)
{
    struct tm tm;
    const char *p;
    long off = 0;

    if (!s)
        return false;
    memset(&tm, 0, sizeof(tm));
    p = strptime(s, "%Y-%m-%dT%H:%M:%S", &tm);
    if (!p)
        return false;
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p))
            p++;
    }
    if (*p == 'Z' || *p == 'z') {
        p++;
    } else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1, hh, mm;

        if (sscanf(p + 1, "%2d:%2d", &hh, &mm) != 2)
            return false;
        off = sign * (hh * 3600L + mm * 60L);
        p += 6;
    } else {
        return false;
    }
    if (*p)
        return false;
    *out = timegm(&tm) - off;
    return true;
}

static void bdt_time_format(time_t t, char *buf, size_t len)
{
    struct tm tm;

    gmtime_r(&t, &tm);
    strftime(buf, len, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

/* TimeWindow of TS 29.122: startTime and stopTime */
static bool bdt_window_read(const cJSON *w, time_t *start, time_t *stop)
{
    const cJSON *s = NULL, *e = NULL;

    if (!cJSON_IsObject(w))
        return false;
    s = cJSON_GetObjectItem((cJSON *)w, "startTime");
    e = cJSON_GetObjectItem((cJSON *)w, "stopTime");
    return cJSON_IsString(s) && cJSON_IsString(e) &&
        bdt_time_parse(s->valuestring, start) && bdt_time_parse(e->valuestring, stop) && *stop > *start;
}

static cJSON *bdt_window_json(time_t start, time_t stop)
{
    cJSON *w = cJSON_CreateObject();
    char buf[32];

    ogs_assert(w);
    bdt_time_format(start, buf, sizeof(buf));
    cJSON_AddStringToObject(w, "startTime", buf);
    bdt_time_format(stop, buf, sizeof(buf));
    cJSON_AddStringToObject(w, "stopTime", buf);
    return w;
}

/*
 * The recommended time window of a transfer policy: the desired window itself, or, for a policy that is configured with a daily window,
 * the first occurrence of that window that overlaps the desired one. A daily window recurs every 24 hours, so the first overlap, if
 * there is one, starts within the three days that begin at midnight (UTC) of the day before the desired window.
 */
static bool bdt_recommend(const bdt_transfer_conf_t *c, time_t dstart, time_t dstop, time_t *rstart, time_t *rstop)
{
    time_t day;
    int i;

    if (!c->has_window) {
        *rstart = dstart;
        *rstop = dstop;
        return true;
    }
    day = dstart - (dstart % 86400) - 86400;
    for (i = 0; i < 3; i++, day += 86400) {
        time_t ws = day + c->start_min * 60L, we = day + c->end_min * 60L, s, e;

        if (c->end_min <= c->start_min)
            we += 86400;
        s = ws > dstart ? ws : dstart;
        e = we < dstop ? we : dstop;
        if (e > s) {
            *rstart = s;
            *rstop = e;
            return true;
        }
    }
    return false;
}

/* A JSON number that is an integer */
static bool bdt_is_integer(const cJSON *n)
{
    char *text = NULL;
    bool integer;

    if (!cJSON_IsNumber(n))
        return false;
    text = cJSON_PrintUnformatted(n);
    ogs_assert(text);
    integer = strpbrk(text, ".eE") == NULL;
    cJSON_free(text);
    return integer;
}

static void bdt_send(ogs_sbi_stream_t *stream, int status, const char *location, cJSON *body)
{
    ogs_sbi_message_t sendmsg;
    ogs_sbi_response_t *response = NULL;
    char *text = NULL;

    memset(&sendmsg, 0, sizeof(sendmsg));
    sendmsg.http.location = (char *)location;
    response = ogs_sbi_build_response(&sendmsg, status);
    ogs_assert(response);
    if (body) {
        text = cJSON_PrintUnformatted(body);
        ogs_assert(text);
        ogs_sbi_header_set(response->http.headers, OGS_SBI_CONTENT_TYPE, OGS_SBI_CONTENT_JSON_TYPE);
        response->http.content = ogs_strdup(text);
        response->http.content_length = strlen(text);
        cJSON_free(text);
    }
    ogs_assert(true == ogs_sbi_server_send_response(stream, response));
}

static char *bdt_location(ogs_sbi_stream_t *stream, const char *id)
{
    ogs_sbi_server_t *server = ogs_sbi_server_from_stream(stream);
    ogs_sbi_header_t header;

    ogs_assert(server);
    memset(&header, 0, sizeof(header));
    header.service.name = (char *)OpenAPI_service_name_ToString(OpenAPI_service_name_npcf_bdtpolicycontrol);
    header.api.version = (char *)OGS_SBI_API_V1;
    header.resource.component[0] = (char *)OGS_SBI_RESOURCE_NAME_BDT_POLICIES;
    header.resource.component[1] = (char *)id;
    return ogs_sbi_server_uri(server, &header);
}

static bdt_policy_t *bdt_policy_find(const char *id)
{
    bdt_policy_t *p = NULL;

    ogs_list_for_each(&bdt_policy_list, p)
        if (!strcmp(p->id, id))
            return p;
    return NULL;
}

/* A request is the same as an existing resource when the service requirements are the same (table 5.3.2.3.1-3, 303 See Other) */
static bool bdt_equivalent(const cJSON *a, const cJSON *b)
{
    static const char *names[] = { "aspId", "desTimeInt", "numOfUes", "volPerUe", "nwAreaInfo", "dnn", "snssai", "interGroupId", "trafficDes" };
    int i;

    for (i = 0; i < (int)OGS_ARRAY_SIZE(names); i++) {
        cJSON *x = cJSON_GetObjectItem((cJSON *)a, names[i]), *y = cJSON_GetObjectItem((cJSON *)b, names[i]);

        if ((x == NULL) != (y == NULL) || (x && !cJSON_Compare(x, y, 1)))
            return false;
    }
    return true;
}

/* The transfer policies of the PCF for a desired time window; NULL if there is none */
static cJSON *bdt_transfer_policies(time_t dstart, time_t dstop)
{
    cJSON *list = cJSON_CreateArray();
    int i, id = 0;

    ogs_assert(list);
    if (bdt_self.num_of_conf == 0) {
        /* default: one policy that gives the desired window, rating group 1 and no limit on the bit rate */
        cJSON *t = cJSON_CreateObject();

        ogs_assert(t);
        cJSON_AddNumberToObject(t, "transPolicyId", 1);
        cJSON_AddNumberToObject(t, "ratingGroup", 1);
        cJSON_AddItemToObject(t, "recTimeInt", bdt_window_json(dstart, dstop));
        cJSON_AddItemToArray(list, t);
        return list;
    }
    for (i = 0; i < bdt_self.num_of_conf; i++) {
        const bdt_transfer_conf_t *c = &bdt_self.conf[i];
        time_t rstart, rstop;
        cJSON *t = NULL;
        char *rate = NULL;

        id++;
        if (!bdt_recommend(c, dstart, dstop, &rstart, &rstop))
            continue;
        t = cJSON_CreateObject();
        ogs_assert(t);
        cJSON_AddNumberToObject(t, "transPolicyId", id);
        cJSON_AddNumberToObject(t, "ratingGroup", c->rating_group);
        cJSON_AddItemToObject(t, "recTimeInt", bdt_window_json(rstart, rstop));
        if (c->max_bit_rate_dl) {
            rate = ogs_sbi_bitrate_to_string(c->max_bit_rate_dl, OGS_SBI_BITRATE_BPS);
            cJSON_AddStringToObject(t, "maxBitRateDl", rate);
            ogs_free(rate);
        }
        if (c->max_bit_rate_ul) {
            rate = ogs_sbi_bitrate_to_string(c->max_bit_rate_ul, OGS_SBI_BITRATE_BPS);
            cJSON_AddStringToObject(t, "maxBitRateUl", rate);
            ogs_free(rate);
        }
        cJSON_AddItemToArray(list, t);
    }
    if (cJSON_GetArraySize(list) == 0) {
        cJSON_Delete(list);
        return NULL;
    }
    return list;
}

static void bdt_create(ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg, const char *content)
{
    cJSON *body = content ? cJSON_Parse(content) : NULL, *item = NULL, *req = NULL, *pol = NULL, *transf = NULL;
    bdt_policy_t *p = NULL;
    time_t dstart, dstop;
    uint64_t features = 0;
    ogs_uuid_t uuid;
    char ref[OGS_UUID_FORMATTED_LENGTH + 1], *location = NULL;
    const char *detail = NULL;

    /* table 5.6.2.3-1: aspId, desTimeInt, numOfUes and volPerUe are mandatory */
    if (!cJSON_IsObject(body)) {
        detail = "No BdtReqData";
    } else if (!cJSON_IsString(cJSON_GetObjectItem(body, "aspId"))) {
        detail = "No aspId";
    } else if (!bdt_window_read(cJSON_GetObjectItem(body, "desTimeInt"), &dstart, &dstop)) {
        detail = "No valid desTimeInt";
    } else if (!bdt_is_integer(item = cJSON_GetObjectItem(body, "numOfUes")) || item->valueint < 1) {
        detail = "No valid numOfUes";
    } else if (!cJSON_IsObject(cJSON_GetObjectItem(body, "volPerUe"))) {
        detail = "No volPerUe";
    }
    if (detail) {
        if (body)
            cJSON_Delete(body);
        ogs_assert(true == ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "Invalid BdtReqData", detail, NULL));
        return;
    }

    /* clause 5.8: the features that both support */
    item = cJSON_GetObjectItem(body, "suppFeat");
    if (cJSON_IsString(item))
        features = ogs_uint64_from_string_hexadecimal(item->valuestring) & BDT_SUPPORTED_FEATURES;

    /* the service requirements as the PCF keeps them: what a feature that is not supported would carry is not kept */
    req = cJSON_Duplicate(body, 1);
    ogs_assert(req);
    cJSON_DeleteItemFromObject(req, "suppFeat");
    cJSON_DeleteItemFromObject(req, "warnNotifReq");
    cJSON_DeleteItemFromObject(req, "notifUri");
    cJSON_DeleteItemFromObject(req, "energyInd");

    ogs_list_for_each(&bdt_policy_list, p) {
        if (bdt_equivalent(req, cJSON_GetObjectItem(p->doc, "bdtReqData"))) {
            location = bdt_location(stream, p->id);
            bdt_send(stream, OGS_SBI_HTTP_STATUS_SEE_OTHER, location, NULL);
            ogs_free(location);
            cJSON_Delete(req);
            cJSON_Delete(body);
            return;
        }
    }

    transf = bdt_transfer_policies(dstart, dstop);
    if (!transf) {
        cJSON_Delete(req);
        cJSON_Delete(body);
        ogs_assert(true == ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_FORBIDDEN,
                recvmsg, "No transfer policy", "No transfer policy is acceptable for the desired time window", NULL));
        return;
    }

    p = ogs_calloc(1, sizeof(*p));
    ogs_assert(p);
    p->id = ogs_msprintf("%u", ++bdt_policy_seq);
    ogs_assert(p->id);
    p->features = features;

    /* the BDT Reference ID is unique */
    ogs_uuid_get(&uuid);
    ogs_uuid_format(ref, &uuid);

    p->doc = cJSON_CreateObject();
    pol = cJSON_AddObjectToObject(p->doc, "bdtPolData");
    ogs_assert(p->doc && pol);
    cJSON_AddStringToObject(pol, "bdtRefId", ref);
    cJSON_AddItemToObject(pol, "transfPolicies", transf);
    if (cJSON_IsString(cJSON_GetObjectItem(body, "suppFeat"))) {
        char *f = ogs_uint64_to_string(features);

        cJSON_AddStringToObject(pol, "suppFeat", f);
        ogs_free(f);
    }
    cJSON_AddItemToObject(p->doc, "bdtReqData", req);
    ogs_list_add(&bdt_policy_list, p);

    location = bdt_location(stream, p->id);
    bdt_send(stream, OGS_SBI_HTTP_STATUS_CREATED, location, p->doc);
    ogs_free(location);
    cJSON_Delete(body);
}

static void bdt_not_found(ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg, bool with_cause)
{
    ogs_assert(true == ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_NOT_FOUND,
            recvmsg, "Not found", recvmsg->h.uri, with_cause ? "BDT_POLICY_NOT_FOUND" : NULL));
}

/*
 * Clause 4.2.3.2: with PatchCorrection the body is a PatchBdtPolicy (bdtPolData.selTransPolicyId, and bdtReqData); without it the body is
 * a BdtPolicyDataPatch (selTransPolicyId). A transfer policy that is selected has to be one of the policies that were offered.
 */
static void bdt_patch(ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg, bdt_policy_t *p, const char *content)
{
    cJSON *body = content ? cJSON_Parse(content) : NULL, *sel = NULL, *data = NULL, *pol = NULL, *t = NULL;
    bool patch_correction = (p->features & ((uint64_t)1 << (BDT_FEATURE_PATCH_CORRECTION - 1))) ? true : false;
    const char *detail = NULL;
    bool found = false;

    if (!cJSON_IsObject(body)) {
        detail = "No PatchBdtPolicy";
    } else if (patch_correction) {
        data = cJSON_GetObjectItem(body, "bdtPolData");
        if (cJSON_GetObjectItem(body, "selTransPolicyId"))
            detail = "selTransPolicyId is in bdtPolData of a PatchBdtPolicy";
        else if (data && !cJSON_IsObject(data))
            detail = "Invalid bdtPolData";
        else if (data)
            sel = cJSON_GetObjectItem(data, "selTransPolicyId");
        if (!detail && !sel && !cJSON_IsObject(cJSON_GetObjectItem(body, "bdtReqData")))
            detail = "A PatchBdtPolicy has bdtPolData or bdtReqData";
    } else {
        sel = cJSON_GetObjectItem(body, "selTransPolicyId");
        if (!sel)
            detail = "No selTransPolicyId";
    }
    if (!detail && sel && !bdt_is_integer(sel))
        detail = "Invalid selTransPolicyId";
    if (!detail && sel) {
        /* the value 0 means no policy selected, with BdtNotification_5G, which this PCF does not support */
        cJSON_ArrayForEach(t, cJSON_GetObjectItem(cJSON_GetObjectItem(p->doc, "bdtPolData"), "transfPolicies")) {
            cJSON *id = cJSON_GetObjectItem(t, "transPolicyId");

            if (cJSON_IsNumber(id) && id->valueint == sel->valueint)
                found = true;
        }
        if (!found)
            detail = "selTransPolicyId is not the transPolicyId of an offered transfer policy";
    }
    if (detail) {
        if (body)
            cJSON_Delete(body);
        ogs_assert(true == ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "Invalid PatchBdtPolicy", detail, NULL));
        return;
    }

    if (sel) {
        pol = cJSON_GetObjectItem(p->doc, "bdtPolData");
        cJSON_DeleteItemFromObject(pol, "selTransPolicyId");
        cJSON_AddNumberToObject(pol, "selTransPolicyId", sel->valueint);
    }
    cJSON_Delete(body);
    bdt_send(stream, OGS_SBI_HTTP_STATUS_OK, NULL, p->doc);
}

void pcf_npcf_bdtpolicycontrol_handle(
        ogs_sbi_stream_t *stream, ogs_sbi_message_t *recvmsg, const char *content)
{
    const char *id = NULL;
    bdt_policy_t *p = NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);

    if (!recvmsg->h.resource.component[0] ||
        strcmp(recvmsg->h.resource.component[0], OGS_SBI_RESOURCE_NAME_BDT_POLICIES)) {
        ogs_assert(true == ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_BAD_REQUEST,
                recvmsg, "Invalid resource name", recvmsg->h.uri, NULL));
        return;
    }
    id = recvmsg->h.resource.component[1];

    if (!id) {
        if (!strcmp(recvmsg->h.method, OGS_SBI_HTTP_METHOD_POST)) {
            bdt_create(stream, recvmsg, content);
        } else {
            ogs_assert(true == ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_METHOD_NOT_ALLOWED,
                    recvmsg, "Invalid HTTP method", recvmsg->h.method, NULL));
        }
        return;
    }

    p = bdt_policy_find(id);
    if (!strcmp(recvmsg->h.method, OGS_SBI_HTTP_METHOD_GET)) {
        if (!p) {
            bdt_not_found(stream, recvmsg, true);
            return;
        }
        bdt_send(stream, OGS_SBI_HTTP_STATUS_OK, NULL, p->doc);
    } else if (!strcmp(recvmsg->h.method, OGS_SBI_HTTP_METHOD_PATCH)) {
        if (!p) {
            bdt_not_found(stream, recvmsg, true);
            return;
        }
        bdt_patch(stream, recvmsg, p, content);
    } else if (!strcmp(recvmsg->h.method, OGS_SBI_HTTP_METHOD_DELETE)) {
        if (!p) {
            bdt_not_found(stream, recvmsg, false);
            return;
        }
        ogs_list_remove(&bdt_policy_list, p);
        ogs_free(p->id);
        cJSON_Delete(p->doc);
        ogs_free(p);
        bdt_send(stream, OGS_SBI_HTTP_STATUS_NO_CONTENT, NULL, NULL);
    } else {
        ogs_assert(true == ogs_sbi_server_send_error(stream, OGS_SBI_HTTP_STATUS_METHOD_NOT_ALLOWED,
                recvmsg, "Invalid HTTP method", recvmsg->h.method, NULL));
    }
}
