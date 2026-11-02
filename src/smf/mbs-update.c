/*
 * 5G-MAG Reference Tools
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

#include <ctype.h>

#include "context.h"
#include "mbs-update.h"
#include "nmbsmf-handler.h"

/* ---- RFC 6902 ---- */

/* The segment of a JSON pointer (RFC 6901): "~1" is "/" and "~0" is "~". */
static char *json_pointer_unescape(const char *segment, size_t len)
{
    char *out = ogs_malloc(len + 1);
    size_t i, n = 0;

    ogs_assert(out);
    for (i = 0; i < len; i++) {
        if (segment[i] == '~' && i + 1 < len && segment[i + 1] == '1') {
            out[n++] = '/';
            i++;
        } else if (segment[i] == '~' && i + 1 < len && segment[i + 1] == '0') {
            out[n++] = '~';
            i++;
        } else {
            out[n++] = segment[i];
        }
    }
    out[n] = '\0';
    return out;
}

/* An array index of RFC 6901: "0" or digits without a leading zero. */
static bool json_array_index(const char *segment, int *index)
{
    size_t i, len = strlen(segment);
    long v = 0;

    if (len == 0 || len > 9)
        return false;
    if (len > 1 && segment[0] == '0')
        return false;
    for (i = 0; i < len; i++) {
        if (segment[i] < '0' || segment[i] > '9')
            return false;
        v = v * 10 + (segment[i] - '0');
    }
    *index = (int)v;
    return true;
}

/* The parent of the value a pointer addresses and its last segment (allocated). NULL parent: the pointer is not
 * valid, or its parent does not exist. */
static cJSON *json_pointer_parent(cJSON *doc, const char *pointer, char **last)
{
    const char *p = pointer;
    cJSON *cur = doc;

    *last = NULL;
    if (!p || p[0] != '/')
        return NULL;

    while (1) {
        const char *next = strchr(p + 1, '/');
        size_t len = next ? (size_t)(next - (p + 1)) : strlen(p + 1);
        char *segment = json_pointer_unescape(p + 1, len);

        if (!next) {
            *last = segment;
            return cur;
        }

        if (cJSON_IsObject(cur)) {
            cur = cJSON_GetObjectItemCaseSensitive(cur, segment);
        } else if (cJSON_IsArray(cur)) {
            int index;
            cur = json_array_index(segment, &index) ? cJSON_GetArrayItem(cur, index) : NULL;
        } else {
            cur = NULL;
        }
        ogs_free(segment);
        if (!cur)
            return NULL;
        p = next;
    }
}

static cJSON *json_pointer_get(cJSON *doc, const char *pointer)
{
    char *last = NULL;
    cJSON *parent = json_pointer_parent(doc, pointer, &last);
    cJSON *item = NULL;

    if (!parent)
        return NULL;
    if (cJSON_IsObject(parent)) {
        item = cJSON_GetObjectItemCaseSensitive(parent, last);
    } else if (cJSON_IsArray(parent)) {
        int index;
        if (json_array_index(last, &index))
            item = cJSON_GetArrayItem(parent, index);
    }
    ogs_free(last);
    return item;
}

/* add of RFC 6902 4.1; takes item */
static const char *json_pointer_add(cJSON *doc, const char *pointer, cJSON *item)
{
    char *last = NULL;
    cJSON *parent = json_pointer_parent(doc, pointer, &last);
    const char *err = NULL;

    if (!parent) {
        cJSON_Delete(item);
        return "the parent of the path does not exist";
    }
    if (cJSON_IsObject(parent)) {
        if (cJSON_GetObjectItemCaseSensitive(parent, last))
            cJSON_ReplaceItemInObjectCaseSensitive(parent, last, item);
        else
            cJSON_AddItemToObject(parent, last, item);
    } else if (cJSON_IsArray(parent)) {
        int index, size = cJSON_GetArraySize(parent);
        if (strcmp(last, "-") == 0) {
            cJSON_AddItemToArray(parent, item);
        } else if (json_array_index(last, &index) && index <= size) {
            if (index == size)
                cJSON_AddItemToArray(parent, item);
            else
                cJSON_InsertItemInArray(parent, index, item);
        } else {
            cJSON_Delete(item);
            err = "the index of the path is not in the array";
        }
    } else {
        cJSON_Delete(item);
        err = "the parent of the path is neither an object nor an array";
    }
    ogs_free(last);
    return err;
}

/* remove of RFC 6902 4.2 */
static const char *json_pointer_remove(cJSON *doc, const char *pointer)
{
    char *last = NULL;
    cJSON *parent = json_pointer_parent(doc, pointer, &last);
    const char *err = NULL;

    if (!parent)
        return "the parent of the path does not exist";
    if (cJSON_IsObject(parent)) {
        if (cJSON_GetObjectItemCaseSensitive(parent, last))
            cJSON_DeleteItemFromObjectCaseSensitive(parent, last);
        else
            err = "the path does not exist";
    } else if (cJSON_IsArray(parent)) {
        int index;
        if (json_array_index(last, &index) && index < cJSON_GetArraySize(parent))
            cJSON_DeleteItemFromArray(parent, index);
        else
            err = "the index of the path is not in the array";
    } else {
        err = "the parent of the path is neither an object nor an array";
    }
    ogs_free(last);
    return err;
}

/* replace of RFC 6902 4.3, in place; takes item */
static const char *json_pointer_replace(cJSON *doc, const char *pointer, cJSON *item)
{
    char *last = NULL;
    cJSON *parent = json_pointer_parent(doc, pointer, &last);
    const char *err = NULL;

    if (!parent) {
        cJSON_Delete(item);
        return "the path to replace does not exist";
    }
    if (cJSON_IsObject(parent)) {
        if (cJSON_GetObjectItemCaseSensitive(parent, last))
            cJSON_ReplaceItemInObjectCaseSensitive(parent, last, item);
        else
            err = "the path to replace does not exist";
    } else if (cJSON_IsArray(parent)) {
        int index;
        if (json_array_index(last, &index) && index < cJSON_GetArraySize(parent))
            cJSON_ReplaceItemInArray(parent, index, item);
        else
            err = "the path to replace does not exist";
    } else {
        err = "the path to replace does not exist";
    }
    if (err)
        cJSON_Delete(item);
    ogs_free(last);
    return err;
}

/* Whether the pointer a names the value pointer b names or a part of it. */
static bool json_pointer_is_prefix(const char *a, const char *b)
{
    size_t len = strlen(a);

    return strncmp(a, b, len) == 0 && (b[len] == '\0' || b[len] == '/');
}

const char *smf_json_patch_apply(cJSON *doc, OpenAPI_list_t *patch_items)
{
    OpenAPI_lnode_t *node;

    ogs_assert(doc);

    if (!patch_items)
        return NULL;

    OpenAPI_list_for_each(patch_items, node) {
        OpenAPI_patch_item_t *item = node->data;
        const char *err = NULL;
        cJSON *value = NULL;

        if (!item || !item->path)
            return "a patch operation has no path";

        switch (item->op) {
        case OpenAPI_patch_operation_add:
        case OpenAPI_patch_operation_replace:
        case OpenAPI_patch_operation_test:
            if (item->is_value_null)
                value = cJSON_CreateNull();
            else if (item->value && item->value->json)
                value = cJSON_Duplicate(item->value->json, 1);
            if (!value)
                return "a patch operation has no value";
            break;
        default:
            break;
        }

        switch (item->op) {
        case OpenAPI_patch_operation_add:
            err = json_pointer_add(doc, item->path, value);
            break;
        case OpenAPI_patch_operation_replace:
            /* RFC 6902 4.3: "The target location MUST exist for the operation to be successful." */
            err = json_pointer_replace(doc, item->path, value);
            break;
        case OpenAPI_patch_operation__remove:
            err = json_pointer_remove(doc, item->path);
            break;
        case OpenAPI_patch_operation_test: {
            cJSON *target = json_pointer_get(doc, item->path);
            bool equal = target && cJSON_Compare(target, value, true);
            cJSON_Delete(value);
            if (!equal)
                return "a test operation does not hold";
            break;
        }
        case OpenAPI_patch_operation_move:
        case OpenAPI_patch_operation_copy: {
            cJSON *source, *copy;

            if (!item->from)
                return "a move or copy operation has no from";
            source = json_pointer_get(doc, item->from);
            if (!source)
                return "the from of a move or copy does not exist";
            if (item->op == OpenAPI_patch_operation_move &&
                    strcmp(item->from, item->path) != 0 && json_pointer_is_prefix(item->from, item->path))
                return "a value cannot be moved into itself";
            copy = cJSON_Duplicate(source, 1);
            if (item->op == OpenAPI_patch_operation_move) {
                err = json_pointer_remove(doc, item->from);
                if (err) {
                    cJSON_Delete(copy);
                    break;
                }
            }
            err = json_pointer_add(doc, item->path, copy);
            break;
        }
        default:
            return "a patch operation is not add, remove, replace, move, copy or test";
        }

        if (err)
            return err;
    }

    return NULL;
}

/* ---- the models the Update modifies ---- */

char *smf_mbs_service_area_to_json(OpenAPI_mbs_service_area_t *api_area)
{
    cJSON *json;
    char *text;

    if (!api_area)
        return NULL;
    json = OpenAPI_mbs_service_area_convertToJSON(api_area);
    if (!json)
        return NULL;
    text = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return text;
}

char *smf_mbs_service_info_to_json(OpenAPI_mbs_service_info_t *mbs_serv_info)
{
    cJSON *json;
    char *text;

    if (!mbs_serv_info)
        return NULL;
    json = OpenAPI_mbs_service_info_convertToJSON(mbs_serv_info);
    if (!json)
        return NULL;
    text = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return text;
}

static bool parse_tai(ogs_tai_t *tai, OpenAPI_tai_t *api_tai)
{
    if (!api_tai || !ogs_sbi_parse_plmn_id(&tai->plmn_id, api_tai->plmn_id))
        return false;
    if (api_tai->tac)
        tai->tac = ogs_strdup(api_tai->tac);
    if (api_tai->nid)
        tai->nid = ogs_strdup(api_tai->nid);
    return true;
}

bool smf_mbs_service_area_from_openapi(OpenAPI_mbs_service_area_t *api_area, ogs_mbs_service_area_t **out)
{
    OpenAPI_lnode_t *node, *cell_node;
    ogs_mbs_service_area_t *area = NULL;

    ogs_assert(api_area);
    ogs_assert(out);

    *out = NULL;

    if (api_area->ncgi_list) {
        OpenAPI_list_for_each(api_area->ncgi_list, node) {
            OpenAPI_ncgi_tai_t *api_ncgi_tai = node->data;
            ogs_ncgi_tai_t *ncgi_tai;

            if (!area)
                area = ogs_calloc(1, sizeof(*area));
            if (!area->ncgi_tai_list)
                area->ncgi_tai_list = ogs_calloc(1, sizeof(*area->ncgi_tai_list));
            ncgi_tai = ogs_calloc(1, sizeof(*ncgi_tai));
            if (!api_ncgi_tai || !parse_tai(&ncgi_tai->tai, api_ncgi_tai->tai)) {
                ogs_ncgi_tai_free(ncgi_tai);
                ogs_mbs_service_area_free(area);
                return false;
            }
            OpenAPI_list_for_each(api_ncgi_tai->cell_list, cell_node) {
                OpenAPI_ncgi_t *api_ncgi = cell_node->data;
                ogs_ncgi_t *ncgi = ogs_calloc(1, sizeof(*ncgi));

                if (!api_ncgi || !ogs_sbi_parse_plmn_id(&ncgi->plmn_id, api_ncgi->plmn_id)) {
                    ogs_free(ncgi);
                    ogs_ncgi_tai_free(ncgi_tai);
                    ogs_mbs_service_area_free(area);
                    return false;
                }
                if (api_ncgi->nr_cell_id)
                    ncgi->nr_cell_id = ogs_strdup(api_ncgi->nr_cell_id);
                if (api_ncgi->nid)
                    ncgi->nid = ogs_strdup(api_ncgi->nid);
                ogs_list_add(&ncgi_tai->cell_list, ncgi);
            }
            ogs_list_add(area->ncgi_tai_list, ncgi_tai);
        }
    }

    if (api_area->tai_list) {
        OpenAPI_list_for_each(api_area->tai_list, node) {
            ogs_tai_t *tai;

            if (!area)
                area = ogs_calloc(1, sizeof(*area));
            if (!area->tai_list)
                area->tai_list = ogs_calloc(1, sizeof(*area->tai_list));
            tai = ogs_calloc(1, sizeof(*tai));
            if (!parse_tai(tai, node->data)) {
                ogs_tai_free(tai);
                ogs_mbs_service_area_free(area);
                return false;
            }
            ogs_list_add(area->tai_list, tai);
        }
    }

    *out = area;
    return true;
}

void smf_mbs_qos_flows_from_service_info(OpenAPI_mbs_service_info_t *mbs_serv_info, ogs_list_t *flows)
{
    OpenAPI_lnode_t *node;

    ogs_assert(flows);
    ogs_list_init(flows);

    if (mbs_serv_info && mbs_serv_info->mbs_media_comps) {
        OpenAPI_list_for_each(mbs_serv_info->mbs_media_comps, node) {
            OpenAPI_map_t *pair = node->data;
            OpenAPI_mbs_media_comp_rm_t *media_comp = pair ? pair->value : NULL;
            smf_mbs_qos_flow_t *flow;

            if (!media_comp || !media_comp->mbs_qo_s_req)
                continue;
            flow = ogs_calloc(1, sizeof(*flow));
            ogs_assert(flow);
            flow->five_qi = (uint8_t)media_comp->mbs_qo_s_req->_5qi;
            if (pair->key)
                flow->med_comp_num = ogs_strdup(pair->key);
            ogs_list_add(flows, flow);
        }
    }

    if (ogs_list_empty(flows)) {
        /* No clause, configuration option or documented default names what a single QoS flow should be when
         * the AF supplies none at all (rule 12): 5QI 9 is TS 23.501 table 5.7.4-1's standardised non-GBR
         * default, a stated engineering choice, not claimed as spec-derived. */
        smf_mbs_qos_flow_t *flow = ogs_calloc(1, sizeof(*flow));

        ogs_assert(flow);
        flow->five_qi = 9;
        ogs_list_add(flows, flow);
    }
}

static void qos_flows_free(ogs_list_t *flows)
{
    smf_mbs_qos_flow_t *flow, *next;

    ogs_list_for_each_safe(flows, next, flow) {
        ogs_list_remove(flows, flow);
        if (flow->med_comp_num)
            ogs_free(flow->med_comp_num);
        ogs_free(flow);
    }
}

/* TS 29.571 V18.12.0 MbsFsaId, pattern '^[A-Fa-f0-9]{6}$' */
bool smf_mbs_fsa_id_is_valid(const char *id)
{
    int i;

    if (!id || strlen(id) != 6)
        return false;
    for (i = 0; i < 6; i++)
        if (!isxdigit((unsigned char)id[i]))
            return false;
    return true;
}

bool smf_mbs_update_path_is_modifiable(const char *path)
{
    static const char *const attributes[] = { "/mbsServiceArea", "/mbsServInfo", "/mbsFsaIdList", "/contactPcfInd",
        "/nrRedCapUeInfo" };
    size_t i;

    if (!path)
        return false;
    for (i = 0; i < OGS_ARRAY_SIZE(attributes); i++) {
        size_t len = strlen(attributes[i]);

        if (strncmp(path, attributes[i], len) == 0 && (path[len] == '\0' || path[len] == '/'))
            return true;
    }
    return false;
}

static bool fsa_ids_equal(const ogs_list_t *current, char **ids, int num)
{
    smf_mbs_fsa_id_t *fsa_id;
    int i = 0;

    ogs_list_for_each(current, fsa_id) {
        if (i >= num || ogs_uint24_from_string(ids[i]).v != fsa_id->id.v)
            return false;
        i++;
    }
    return i == num;
}

smf_mbs_update_result_e smf_mbs_sess_update(smf_mbs_sess_t *mbs_sess, OpenAPI_list_t *patch_items,
        smf_mbs_update_t *update, const char **detail)
{
    return smf_mbs_sess_update_check(mbs_sess, patch_items, update, detail, false);
}

smf_mbs_update_result_e smf_mbs_sess_update_check(smf_mbs_sess_t *mbs_sess, OpenAPI_list_t *patch_items,
        smf_mbs_update_t *update, const char **detail, bool check_only)
{
    smf_mbs_update_result_e result = SMF_MBS_UPDATE_OK;
    cJSON *doc = NULL, *member = NULL;
    OpenAPI_lnode_t *node;
    OpenAPI_list_t *modifications = NULL;
    const char *err = NULL;

    OpenAPI_mbs_service_area_t *api_area = NULL;
    OpenAPI_mbs_service_info_t *api_serv_info = NULL;
    ogs_mbs_service_area_t *area = NULL, *reduced = NULL;
    ogs_list_t new_flows;
    char **fsa_ids = NULL;
    int num_fsa_ids = 0, i;
    bool has_contact_pcf_ind = false, contact_pcf_ind = false;
    smf_mbs_nr_red_cap_e nr_red_cap = SMF_MBS_NR_RED_CAP_NONE;
    char *area_json = NULL, *serv_info_json = NULL;
    bool has_area = false, has_serv_info = false;
    bool flows_replaced = false;

    ogs_assert(mbs_sess);
    ogs_assert(update);

    memset(update, 0, sizeof(*update));
    ogs_list_init(&new_flows);
    if (detail)
        *detail = NULL;

    /* the representation the consumer holds, restricted to what an Update modifies */
    doc = cJSON_CreateObject();
    ogs_assert(doc);
    if (mbs_sess->mbs_service_area_json)
        cJSON_AddItemToObject(doc, "mbsServiceArea", cJSON_Parse(mbs_sess->mbs_service_area_json));
    if (mbs_sess->mbs_serv_info_json)
        cJSON_AddItemToObject(doc, "mbsServInfo", cJSON_Parse(mbs_sess->mbs_serv_info_json));
    {
        smf_mbs_fsa_id_t *fsa_id;
        cJSON *array = NULL;

        ogs_list_for_each(&mbs_sess->mbs_fsa_id_list, fsa_id) {
            char *text = ogs_uint24_to_0string(fsa_id->id);

            if (!array)
                array = cJSON_CreateArray();
            cJSON_AddItemToArray(array, cJSON_CreateString(text));
            ogs_free(text);
        }
        if (array)
            cJSON_AddItemToObject(doc, "mbsFsaIdList", array);
    }
    if (mbs_sess->has_contact_pcf_ind)
        cJSON_AddItemToObject(doc, "contactPcfInd", cJSON_CreateBool(mbs_sess->contact_pcf_ind));
    if (mbs_sess->nr_red_cap_ue_info != SMF_MBS_NR_RED_CAP_NONE) {
        static const char *const values[] = { NULL, "NR_REDCAP_UE_ONLY", "BOTH_NR_REDCAP_UE_AND_NON_REDCAP_UE",
            "NON_REDCAP_UE_ONLY" };

        cJSON_AddItemToObject(doc, "nrRedCapUeInfo", cJSON_CreateString(values[mbs_sess->nr_red_cap_ue_info]));
    }

    /* the items that name what an Update modifies, in their order */
    modifications = OpenAPI_list_create();
    OpenAPI_list_for_each(patch_items, node) {
        OpenAPI_patch_item_t *item = node->data;

        if (!item || !smf_mbs_update_path_is_modifiable(item->path))
            continue;
        OpenAPI_list_add(modifications, item);
        if (strncmp(item->path, "/mbsServiceArea", 15) == 0)
            update->area_in_request = true;
    }

    err = smf_json_patch_apply(doc, modifications);
    if (err) {
        if (detail)
            *detail = err;
        result = SMF_MBS_UPDATE_PATCH_FAILED;
        goto done;
    }

    /* the service area */
    member = cJSON_GetObjectItemCaseSensitive(doc, "mbsServiceArea");
    if (member && !cJSON_IsNull(member)) {
        has_area = true;
        api_area = OpenAPI_mbs_service_area_parseFromJSON(member);
        if (!api_area || !smf_mbs_service_area_from_openapi(api_area, &area)) {
            if (detail)
                *detail = "[mbsServiceArea] is not an MBS service area";
            result = SMF_MBS_UPDATE_ERROR_INPUT_PARAMETERS;
            goto done;
        }
        area_json = cJSON_PrintUnformatted(member);
    }

    /* the MBS service information: valid, and the flows it gives fit the session */
    member = cJSON_GetObjectItemCaseSensitive(doc, "mbsServInfo");
    if (member && !cJSON_IsNull(member)) {
        has_serv_info = true;
        api_serv_info = OpenAPI_mbs_service_info_parseFromJSON(member);
        if (!api_serv_info) {
            if (detail)
                *detail = "[mbsServInfo] is not MBS service information";
            result = SMF_MBS_UPDATE_INVALID_MBS_SERVICE_INFO;
            goto done;
        }
        {
            const char *fault = NULL;

            if (!smf_nmbsmf_mbs_service_info_is_valid(api_serv_info, &fault)) {
                if (detail)
                    *detail = fault;
                result = SMF_MBS_UPDATE_INVALID_MBS_SERVICE_INFO;
                goto done;
            }
            switch (smf_nmbsmf_mbs_flow_descriptions_check(api_serv_info, &fault)) {
            case SMF_NMBSMF_FLOW_DESC_OK:
                break;
            case SMF_NMBSMF_FLOW_DESC_RESTRICTION:
                if (detail)
                    *detail = fault;
                result = SMF_MBS_UPDATE_FILTER_RESTRICTIONS_NOT_RESPECTED;
                goto done;
            default:
                if (detail)
                    *detail = fault;
                result = SMF_MBS_UPDATE_INVALID_MBS_SERVICE_INFO;
                goto done;
            }
        }
        serv_info_json = cJSON_PrintUnformatted(member);
    }
    smf_mbs_qos_flows_from_service_info(api_serv_info, &new_flows);

    /* the FSA IDs */
    member = cJSON_GetObjectItemCaseSensitive(doc, "mbsFsaIdList");
    if (member && !cJSON_IsNull(member)) {
        cJSON *entry;

        if (!cJSON_IsArray(member)) {
            if (detail)
                *detail = "[mbsFsaIdList] is not an array";
            result = SMF_MBS_UPDATE_ERROR_INPUT_PARAMETERS;
            goto done;
        }
        num_fsa_ids = cJSON_GetArraySize(member);
        fsa_ids = ogs_calloc(num_fsa_ids ? num_fsa_ids : 1, sizeof(*fsa_ids));
        ogs_assert(fsa_ids);
        i = 0;
        cJSON_ArrayForEach(entry, member) {
            if (!cJSON_IsString(entry) || !smf_mbs_fsa_id_is_valid(entry->valuestring)) {
                if (detail)
                    *detail = "an entry of [mbsFsaIdList] is not 6 hexadecimal digits";
                result = SMF_MBS_UPDATE_ERROR_INPUT_PARAMETERS;
                goto done;
            }
            fsa_ids[i++] = entry->valuestring;
        }
    }

    member = cJSON_GetObjectItemCaseSensitive(doc, "contactPcfInd");
    if (member && !cJSON_IsNull(member)) {
        if (!cJSON_IsBool(member)) {
            if (detail)
                *detail = "[contactPcfInd] is not a boolean";
            result = SMF_MBS_UPDATE_ERROR_INPUT_PARAMETERS;
            goto done;
        }
        has_contact_pcf_ind = true;
        contact_pcf_ind = cJSON_IsTrue(member);
    }

    member = cJSON_GetObjectItemCaseSensitive(doc, "nrRedCapUeInfo");
    if (member && !cJSON_IsNull(member)) {
        if (!cJSON_IsString(member)) {
            if (detail)
                *detail = "[nrRedCapUeInfo] is not a string";
            result = SMF_MBS_UPDATE_ERROR_INPUT_PARAMETERS;
            goto done;
        }
        nr_red_cap = smf_mbs_nr_red_cap_from_string(member->valuestring);
    }

    /* Only asked whether the result is acceptable: nothing is applied, and the service information the request leaves
     * is given */
    if (check_only) {
        bool same_json = (!mbs_sess->mbs_serv_info_json && !serv_info_json) ||
            (mbs_sess->mbs_serv_info_json && serv_info_json &&
             strcmp(mbs_sess->mbs_serv_info_json, serv_info_json) == 0);

        update->service_info_changed = !same_json;
        update->serv_info_json = serv_info_json;
        serv_info_json = NULL;
        goto done;
    }

    /* From here the result is accepted: the flows are the first thing that can still fail (more rules than a
     * PFCP session holds), so they are applied first. */
    if (!smf_mbs_sess_replace_flows(mbs_sess, &new_flows, &update->flow_change)) {
        if (detail)
            *detail = "the MBS QoS flows do not fit the user plane rules of one MBS session";
        result = SMF_MBS_UPDATE_INSUFFICIENT_RESOURCES;
        goto done;
    }
    flows_replaced = true;

    /* TS 29.532 V18.6.0 cl. 5.3.2.3.1 step 2b: "If the MBS service area received in the request cannot be
     * entirely covered by the MB-SMF service area, the MB-SMF shall reduce the MBS service area to be within the
     * MB-SMF service area and continue the Update service operation using the reduced MBS service area." */
    if (area && smf_mbs_service_area_reduce(area, &reduced)) {
        ogs_mbs_service_area_free(area);
        area = reduced;
        reduced = NULL;
        if (update->area_in_request)
            update->area_reduced = true;
        mbs_sess->service_area_reduced = true;
    } else if (update->area_in_request) {
        mbs_sess->service_area_reduced = false;
    }

    {
        bool same_json = (!mbs_sess->mbs_service_area_json && !area_json) ||
            (mbs_sess->mbs_service_area_json && area_json && strcmp(mbs_sess->mbs_service_area_json, area_json) == 0);

        update->area_changed = !same_json;
    }
    {
        bool same_json = (!mbs_sess->mbs_serv_info_json && !serv_info_json) ||
            (mbs_sess->mbs_serv_info_json && serv_info_json &&
             strcmp(mbs_sess->mbs_serv_info_json, serv_info_json) == 0);

        update->service_info_changed = !same_json;
    }
    update->fsa_changed = !fsa_ids_equal(&mbs_sess->mbs_fsa_id_list, fsa_ids, num_fsa_ids);
    update->contact_pcf_ind_changed =
            has_contact_pcf_ind != mbs_sess->has_contact_pcf_ind ||
            (has_contact_pcf_ind && contact_pcf_ind != mbs_sess->contact_pcf_ind);
    update->n2_info_changed = update->flow_change.num_added || update->flow_change.num_removed ||
            update->flow_change.qos_changed || update->fsa_changed || nr_red_cap != mbs_sess->nr_red_cap_ue_info;

    if (update->area_changed) {
        if (mbs_sess->mbs_service_area)
            ogs_mbs_service_area_free(mbs_sess->mbs_service_area);
        mbs_sess->mbs_service_area = area;
        area = NULL;
        if (mbs_sess->mbs_service_area_json)
            ogs_free(mbs_sess->mbs_service_area_json);
        mbs_sess->mbs_service_area_json = area_json;
        area_json = NULL;
    }
    (void)has_area;
    if (update->service_info_changed) {
        if (mbs_sess->mbs_serv_info_json)
            ogs_free(mbs_sess->mbs_serv_info_json);
        mbs_sess->mbs_serv_info_json = serv_info_json;
        serv_info_json = NULL;
    }
    (void)has_serv_info;
    if (update->fsa_changed) {
        smf_mbs_fsa_id_t *fsa_id, *next;

        ogs_list_for_each_safe(&mbs_sess->mbs_fsa_id_list, next, fsa_id) {
            ogs_list_remove(&mbs_sess->mbs_fsa_id_list, fsa_id);
            ogs_free(fsa_id);
        }
        for (i = 0; i < num_fsa_ids; i++) {
            fsa_id = ogs_calloc(1, sizeof(*fsa_id));
            ogs_assert(fsa_id);
            fsa_id->id = ogs_uint24_from_string(fsa_ids[i]);
            ogs_list_add(&mbs_sess->mbs_fsa_id_list, fsa_id);
        }
    }
    mbs_sess->has_contact_pcf_ind = has_contact_pcf_ind;
    mbs_sess->contact_pcf_ind = contact_pcf_ind;
    mbs_sess->nr_red_cap_ue_info = nr_red_cap;

done:
    (void)flows_replaced;
    qos_flows_free(&new_flows);
    if (api_area)
        OpenAPI_mbs_service_area_free(api_area);
    if (api_serv_info)
        OpenAPI_mbs_service_info_free(api_serv_info);
    if (area)
        ogs_mbs_service_area_free(area);
    if (area_json)
        ogs_free(area_json);
    if (serv_info_json)
        ogs_free(serv_info_json);
    if (fsa_ids)
        ogs_free(fsa_ids);
    if (doc)
        cJSON_Delete(doc);
    if (modifications)
        OpenAPI_list_free(modifications);

    if (result != SMF_MBS_UPDATE_OK)
        memset(update, 0, sizeof(*update));

    return result;
}
