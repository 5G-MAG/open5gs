
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "op_event_type_any_of.h"

char* OpenAPI_op_event_type_any_of_ToString(OpenAPI_op_event_type_any_of_e op_event_type_any_of)
{
    const char *op_event_type_any_ofArray[] =  { "NULL", "AMF_CHANGE", "NG_RAN_EVENT" };
    size_t sizeofArray = sizeof(op_event_type_any_ofArray) / sizeof(op_event_type_any_ofArray[0]);
    if (op_event_type_any_of < sizeofArray)
        return (char *)op_event_type_any_ofArray[op_event_type_any_of];
    else
        return (char *)"Unknown";
}

OpenAPI_op_event_type_any_of_e OpenAPI_op_event_type_any_of_FromString(char* op_event_type_any_of)
{
    int stringToReturn = 0;
    const char *op_event_type_any_ofArray[] =  { "NULL", "AMF_CHANGE", "NG_RAN_EVENT" };
    size_t sizeofArray = sizeof(op_event_type_any_ofArray) / sizeof(op_event_type_any_ofArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(op_event_type_any_of, op_event_type_any_ofArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

