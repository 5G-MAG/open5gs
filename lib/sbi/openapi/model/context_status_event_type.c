
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_status_event_type.h"

char* OpenAPI_context_status_event_type_ToString(OpenAPI_context_status_event_type_e context_status_event_type)
{
    const char *context_status_event_typeArray[] =  { "NULL", "QOS_INFO", "STATUS_INFO", "SERVICE_AREA_INFO", "SESSION_RELEASE", "MULT_TRANS_ADD_CHANGE", "SECURITY_INFO" };
    size_t sizeofArray = sizeof(context_status_event_typeArray) / sizeof(context_status_event_typeArray[0]);
    if (context_status_event_type < sizeofArray)
        return (char *)context_status_event_typeArray[context_status_event_type];
    else
        return (char *)"Unknown";
}

OpenAPI_context_status_event_type_e OpenAPI_context_status_event_type_FromString(char* context_status_event_type)
{
    int stringToReturn = 0;
    const char *context_status_event_typeArray[] =  { "NULL", "QOS_INFO", "STATUS_INFO", "SERVICE_AREA_INFO", "SESSION_RELEASE", "MULT_TRANS_ADD_CHANGE", "SECURITY_INFO" };
    size_t sizeofArray = sizeof(context_status_event_typeArray) / sizeof(context_status_event_typeArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(context_status_event_type, context_status_event_typeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

