
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "geographic_area.h"

OpenAPI_geographic_area_t *OpenAPI_geographic_area_create(
    OpenAPI_supported_gad_shapes_e shape,
    OpenAPI_geographical_coordinates_t *point,
    float uncertainty,
    OpenAPI_uncertainty_ellipse_t *uncertainty_ellipse,
    int confidence,
    OpenAPI_list_t *point_list,
    double altitude,
    float uncertainty_altitude,
    bool is_v_confidence,
    int v_confidence,
    int inner_radius,
    float uncertainty_radius,
    int offset_angle,
    int included_angle
)
{
    OpenAPI_geographic_area_t *geographic_area_local_var = ogs_malloc(sizeof(OpenAPI_geographic_area_t));
    ogs_assert(geographic_area_local_var);

    geographic_area_local_var->shape = shape;
    geographic_area_local_var->point = point;
    geographic_area_local_var->uncertainty = uncertainty;
    geographic_area_local_var->uncertainty_ellipse = uncertainty_ellipse;
    geographic_area_local_var->confidence = confidence;
    geographic_area_local_var->point_list = point_list;
    geographic_area_local_var->altitude = altitude;
    geographic_area_local_var->uncertainty_altitude = uncertainty_altitude;
    geographic_area_local_var->is_v_confidence = is_v_confidence;
    geographic_area_local_var->v_confidence = v_confidence;
    geographic_area_local_var->inner_radius = inner_radius;
    geographic_area_local_var->uncertainty_radius = uncertainty_radius;
    geographic_area_local_var->offset_angle = offset_angle;
    geographic_area_local_var->included_angle = included_angle;

    return geographic_area_local_var;
}

void OpenAPI_geographic_area_free(OpenAPI_geographic_area_t *geographic_area)
{
    OpenAPI_lnode_t *node = NULL;

    if (NULL == geographic_area) {
        return;
    }
    if (geographic_area->point) {
        OpenAPI_geographical_coordinates_free(geographic_area->point);
        geographic_area->point = NULL;
    }
    if (geographic_area->uncertainty_ellipse) {
        OpenAPI_uncertainty_ellipse_free(geographic_area->uncertainty_ellipse);
        geographic_area->uncertainty_ellipse = NULL;
    }
    if (geographic_area->point_list) {
        OpenAPI_list_for_each(geographic_area->point_list, node) {
            OpenAPI_geographical_coordinates_free(node->data);
        }
        OpenAPI_list_free(geographic_area->point_list);
        geographic_area->point_list = NULL;
    }
    ogs_free(geographic_area);
}

/*
 * TS 29.572 defines GeographicArea as an anyOf of seven shapes, each requiring only its own
 * attributes. This struct holds the attributes of all of them, so which of its members are meaningful
 * depends on the shape: the functions below read and write only the members of the shape, and
 * require only the attributes that shape requires.
 */
enum {
    GA_POINT = 1 << 0,
    GA_UNCERTAINTY = 1 << 1,
    GA_UNCERTAINTY_ELLIPSE = 1 << 2,
    GA_CONFIDENCE = 1 << 3,
    GA_POINT_LIST = 1 << 4,
    GA_ALTITUDE = 1 << 5,
    GA_UNCERTAINTY_ALTITUDE = 1 << 6,
    GA_INNER_RADIUS = 1 << 7,
    GA_UNCERTAINTY_RADIUS = 1 << 8,
    GA_OFFSET_ANGLE = 1 << 9,
    GA_INCLUDED_ANGLE = 1 << 10
};

/* The attributes the shape requires; 0 for a shape this struct cannot hold. */
static unsigned int geographic_area_required(OpenAPI_supported_gad_shapes_e shape)
{
    switch (shape) {
    case OpenAPI_supported_gad_shapes_POINT:
        return GA_POINT;
    case OpenAPI_supported_gad_shapes_POINT_UNCERTAINTY_CIRCLE:
        return GA_POINT | GA_UNCERTAINTY;
    case OpenAPI_supported_gad_shapes_POINT_UNCERTAINTY_ELLIPSE:
        return GA_POINT | GA_UNCERTAINTY_ELLIPSE | GA_CONFIDENCE;
    case OpenAPI_supported_gad_shapes_POLYGON:
        return GA_POINT_LIST;
    case OpenAPI_supported_gad_shapes_POINT_ALTITUDE:
        return GA_POINT | GA_ALTITUDE;
    case OpenAPI_supported_gad_shapes_POINT_ALTITUDE_UNCERTAINTY:
        return GA_POINT | GA_ALTITUDE | GA_UNCERTAINTY_ELLIPSE | GA_UNCERTAINTY_ALTITUDE | GA_CONFIDENCE;
    case OpenAPI_supported_gad_shapes_ELLIPSOID_ARC:
        return GA_POINT | GA_INNER_RADIUS | GA_UNCERTAINTY_RADIUS | GA_OFFSET_ANGLE | GA_INCLUDED_ANGLE | GA_CONFIDENCE;
    default:
        return 0;
    }
}

cJSON *OpenAPI_geographic_area_convertToJSON(OpenAPI_geographic_area_t *geographic_area)
{
    cJSON *item = NULL;
    OpenAPI_lnode_t *node = NULL;
    unsigned int members;

    if (geographic_area == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [GeographicArea]");
        return NULL;
    }

    if (geographic_area->shape == OpenAPI_supported_gad_shapes_NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [shape]");
        return NULL;
    }
    members = geographic_area_required(geographic_area->shape);
    if (members == 0) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [shape %s is not held by this structure]",
                OpenAPI_supported_gad_shapes_ToString(geographic_area->shape));
        return NULL;
    }

    item = cJSON_CreateObject();
    if (cJSON_AddStringToObject(item, "shape", OpenAPI_supported_gad_shapes_ToString(geographic_area->shape)) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [shape]");
        goto end;
    }

    if (members & GA_POINT) {
        cJSON *point_local_JSON;
        if (!geographic_area->point) {
            ogs_error("OpenAPI_geographic_area_convertToJSON() failed [point]");
            goto end;
        }
        point_local_JSON = OpenAPI_geographical_coordinates_convertToJSON(geographic_area->point);
        if (point_local_JSON == NULL) {
            ogs_error("OpenAPI_geographic_area_convertToJSON() failed [point]");
            goto end;
        }
        cJSON_AddItemToObject(item, "point", point_local_JSON);
    }

    if ((members & GA_UNCERTAINTY) &&
            cJSON_AddNumberToObject(item, "uncertainty", geographic_area->uncertainty) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [uncertainty]");
        goto end;
    }

    if (members & GA_UNCERTAINTY_ELLIPSE) {
        cJSON *uncertainty_ellipse_local_JSON;
        if (!geographic_area->uncertainty_ellipse) {
            ogs_error("OpenAPI_geographic_area_convertToJSON() failed [uncertainty_ellipse]");
            goto end;
        }
        uncertainty_ellipse_local_JSON = OpenAPI_uncertainty_ellipse_convertToJSON(geographic_area->uncertainty_ellipse);
        if (uncertainty_ellipse_local_JSON == NULL) {
            ogs_error("OpenAPI_geographic_area_convertToJSON() failed [uncertainty_ellipse]");
            goto end;
        }
        cJSON_AddItemToObject(item, "uncertaintyEllipse", uncertainty_ellipse_local_JSON);
    }

    if ((members & GA_CONFIDENCE) &&
            cJSON_AddNumberToObject(item, "confidence", geographic_area->confidence) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [confidence]");
        goto end;
    }

    if (members & GA_POINT_LIST) {
        cJSON *point_listList;
        if (!geographic_area->point_list) {
            ogs_error("OpenAPI_geographic_area_convertToJSON() failed [point_list]");
            goto end;
        }
        point_listList = cJSON_AddArrayToObject(item, "pointList");
        if (point_listList == NULL) {
            ogs_error("OpenAPI_geographic_area_convertToJSON() failed [point_list]");
            goto end;
        }
        OpenAPI_list_for_each(geographic_area->point_list, node) {
            cJSON *itemLocal = OpenAPI_geographical_coordinates_convertToJSON(node->data);
            if (itemLocal == NULL) {
                ogs_error("OpenAPI_geographic_area_convertToJSON() failed [point_list]");
                goto end;
            }
            cJSON_AddItemToArray(point_listList, itemLocal);
        }
    }

    if ((members & GA_ALTITUDE) &&
            cJSON_AddNumberToObject(item, "altitude", geographic_area->altitude) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [altitude]");
        goto end;
    }

    if ((members & GA_UNCERTAINTY_ALTITUDE) &&
            cJSON_AddNumberToObject(item, "uncertaintyAltitude", geographic_area->uncertainty_altitude) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [uncertainty_altitude]");
        goto end;
    }

    if ((members & GA_UNCERTAINTY_ALTITUDE) && geographic_area->is_v_confidence &&
            cJSON_AddNumberToObject(item, "vConfidence", geographic_area->v_confidence) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [v_confidence]");
        goto end;
    }

    if ((members & GA_INNER_RADIUS) &&
            cJSON_AddNumberToObject(item, "innerRadius", geographic_area->inner_radius) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [inner_radius]");
        goto end;
    }

    if ((members & GA_UNCERTAINTY_RADIUS) &&
            cJSON_AddNumberToObject(item, "uncertaintyRadius", geographic_area->uncertainty_radius) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [uncertainty_radius]");
        goto end;
    }

    if ((members & GA_OFFSET_ANGLE) &&
            cJSON_AddNumberToObject(item, "offsetAngle", geographic_area->offset_angle) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [offset_angle]");
        goto end;
    }

    if ((members & GA_INCLUDED_ANGLE) &&
            cJSON_AddNumberToObject(item, "includedAngle", geographic_area->included_angle) == NULL) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed [included_angle]");
        goto end;
    }

    return item;
end:
    cJSON_Delete(item);
    return NULL;
}

OpenAPI_geographic_area_t *OpenAPI_geographic_area_parseFromJSON(cJSON *geographic_areaJSON)
{
    OpenAPI_geographic_area_t *geographic_area_local_var = NULL;
    OpenAPI_lnode_t *node = NULL;
    cJSON *shape = NULL;
    OpenAPI_supported_gad_shapes_e shapeVariable = 0;
    cJSON *point = NULL;
    OpenAPI_geographical_coordinates_t *point_local_nonprim = NULL;
    cJSON *uncertainty = NULL;
    cJSON *uncertainty_ellipse = NULL;
    OpenAPI_uncertainty_ellipse_t *uncertainty_ellipse_local_nonprim = NULL;
    cJSON *confidence = NULL;
    cJSON *point_list = NULL;
    OpenAPI_list_t *point_listList = NULL;
    cJSON *altitude = NULL;
    cJSON *uncertainty_altitude = NULL;
    cJSON *v_confidence = NULL;
    cJSON *inner_radius = NULL;
    cJSON *uncertainty_radius = NULL;
    cJSON *offset_angle = NULL;
    cJSON *included_angle = NULL;
    unsigned int members;

    shape = cJSON_GetObjectItemCaseSensitive(geographic_areaJSON, "shape");
    if (!shape) {
        ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [shape]");
        goto end;
    }
    if (!cJSON_IsString(shape)) {
        ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [shape]");
        goto end;
    }
    shapeVariable = OpenAPI_supported_gad_shapes_FromString(shape->valuestring);
    members = geographic_area_required(shapeVariable);
    if (members == 0) {
        ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [shape %s is not held by this structure]",
                shape->valuestring);
        goto end;
    }

#define GA_NUMBER(var, key, flag) \
    var = cJSON_GetObjectItemCaseSensitive(geographic_areaJSON, key); \
    if (var) { \
        if (!cJSON_IsNumber(var)) { \
            ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [%s]", key); \
            goto end; \
        } \
    } else if (members & (flag)) { \
        ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [%s]", key); \
        goto end; \
    }

    point = cJSON_GetObjectItemCaseSensitive(geographic_areaJSON, "point");
    if (point) {
        point_local_nonprim = OpenAPI_geographical_coordinates_parseFromJSON(point);
        if (!point_local_nonprim) {
            ogs_error("OpenAPI_geographical_coordinates_parseFromJSON failed [point]");
            goto end;
        }
    } else if (members & GA_POINT) {
        ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [point]");
        goto end;
    }

    GA_NUMBER(uncertainty, "uncertainty", GA_UNCERTAINTY)

    uncertainty_ellipse = cJSON_GetObjectItemCaseSensitive(geographic_areaJSON, "uncertaintyEllipse");
    if (uncertainty_ellipse) {
        uncertainty_ellipse_local_nonprim = OpenAPI_uncertainty_ellipse_parseFromJSON(uncertainty_ellipse);
        if (!uncertainty_ellipse_local_nonprim) {
            ogs_error("OpenAPI_uncertainty_ellipse_parseFromJSON failed [uncertainty_ellipse]");
            goto end;
        }
    } else if (members & GA_UNCERTAINTY_ELLIPSE) {
        ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [uncertainty_ellipse]");
        goto end;
    }

    GA_NUMBER(confidence, "confidence", GA_CONFIDENCE)

    point_list = cJSON_GetObjectItemCaseSensitive(geographic_areaJSON, "pointList");
    if (point_list) {
        cJSON *point_list_local = NULL;
        if (!cJSON_IsArray(point_list)) {
            ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [point_list]");
            goto end;
        }

        point_listList = OpenAPI_list_create();

        cJSON_ArrayForEach(point_list_local, point_list) {
            OpenAPI_geographical_coordinates_t *point_listItem;
            if (!cJSON_IsObject(point_list_local)) {
                ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [point_list]");
                goto end;
            }
            point_listItem = OpenAPI_geographical_coordinates_parseFromJSON(point_list_local);
            if (!point_listItem) {
                ogs_error("No point_listItem");
                goto end;
            }
            OpenAPI_list_add(point_listList, point_listItem);
        }
    } else if (members & GA_POINT_LIST) {
        ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [point_list]");
        goto end;
    }

    GA_NUMBER(altitude, "altitude", GA_ALTITUDE)
    GA_NUMBER(uncertainty_altitude, "uncertaintyAltitude", GA_UNCERTAINTY_ALTITUDE)

    v_confidence = cJSON_GetObjectItemCaseSensitive(geographic_areaJSON, "vConfidence");
    if (v_confidence) {
        if (!cJSON_IsNumber(v_confidence)) {
            ogs_error("OpenAPI_geographic_area_parseFromJSON() failed [v_confidence]");
            goto end;
        }
    }

    GA_NUMBER(inner_radius, "innerRadius", GA_INNER_RADIUS)
    GA_NUMBER(uncertainty_radius, "uncertaintyRadius", GA_UNCERTAINTY_RADIUS)
    GA_NUMBER(offset_angle, "offsetAngle", GA_OFFSET_ANGLE)
    GA_NUMBER(included_angle, "includedAngle", GA_INCLUDED_ANGLE)

#undef GA_NUMBER

    geographic_area_local_var = OpenAPI_geographic_area_create (
        shapeVariable,
        point_local_nonprim,
        uncertainty ? uncertainty->valuedouble : 0,
        uncertainty_ellipse_local_nonprim,
        confidence ? confidence->valuedouble : 0,
        point_listList,
        altitude ? altitude->valuedouble : 0,
        uncertainty_altitude ? uncertainty_altitude->valuedouble : 0,
        v_confidence ? true : false,
        v_confidence ? v_confidence->valuedouble : 0,
        inner_radius ? inner_radius->valuedouble : 0,
        uncertainty_radius ? uncertainty_radius->valuedouble : 0,
        offset_angle ? offset_angle->valuedouble : 0,
        included_angle ? included_angle->valuedouble : 0
    );

    return geographic_area_local_var;
end:
    if (point_local_nonprim) {
        OpenAPI_geographical_coordinates_free(point_local_nonprim);
        point_local_nonprim = NULL;
    }
    if (uncertainty_ellipse_local_nonprim) {
        OpenAPI_uncertainty_ellipse_free(uncertainty_ellipse_local_nonprim);
        uncertainty_ellipse_local_nonprim = NULL;
    }
    if (point_listList) {
        OpenAPI_list_for_each(point_listList, node) {
            OpenAPI_geographical_coordinates_free(node->data);
        }
        OpenAPI_list_free(point_listList);
        point_listList = NULL;
    }
    return NULL;
}

OpenAPI_geographic_area_t *OpenAPI_geographic_area_copy(OpenAPI_geographic_area_t *dst, OpenAPI_geographic_area_t *src)
{
    cJSON *item = NULL;
    char *content = NULL;

    ogs_assert(src);
    item = OpenAPI_geographic_area_convertToJSON(src);
    if (!item) {
        ogs_error("OpenAPI_geographic_area_convertToJSON() failed");
        return NULL;
    }

    content = cJSON_Print(item);
    cJSON_Delete(item);

    if (!content) {
        ogs_error("cJSON_Print() failed");
        return NULL;
    }

    item = cJSON_Parse(content);
    ogs_free(content);
    if (!item) {
        ogs_error("cJSON_Parse() failed");
        return NULL;
    }

    OpenAPI_geographic_area_free(dst);
    dst = OpenAPI_geographic_area_parseFromJSON(item);
    cJSON_Delete(item);

    return dst;
}

