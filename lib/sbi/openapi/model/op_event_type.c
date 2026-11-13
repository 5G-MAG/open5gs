
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "op_event_type.h"

char* OpenAPI_op_event_type_ToString(OpenAPI_op_event_type_e op_event_type)
{
    const char *op_event_typeArray[] =  { "NULL", "AMF_CHANGE", "NG_RAN_EVENT" };
    size_t sizeofArray = sizeof(op_event_typeArray) / sizeof(op_event_typeArray[0]);
    if (op_event_type < sizeofArray)
        return (char *)op_event_typeArray[op_event_type];
    else
        return (char *)"Unknown";
}

OpenAPI_op_event_type_e OpenAPI_op_event_type_FromString(char* op_event_type)
{
    int stringToReturn = 0;
    const char *op_event_typeArray[] =  { "NULL", "AMF_CHANGE", "NG_RAN_EVENT" };
    size_t sizeofArray = sizeof(op_event_typeArray) / sizeof(op_event_typeArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(op_event_type, op_event_typeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

