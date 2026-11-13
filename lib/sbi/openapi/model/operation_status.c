
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "operation_status.h"

char* OpenAPI_operation_status_ToString(OpenAPI_operation_status_e operation_status)
{
    const char *operation_statusArray[] =  { "NULL", "MBS_SESSION_START_COMPLETE", "MBS_SESSION_START_INCOMPLETE", "MBS_SESSION_UPDATE_COMPLETE", "MBS_SESSION_UPDATE_INCOMPLETE" };
    size_t sizeofArray = sizeof(operation_statusArray) / sizeof(operation_statusArray[0]);
    if (operation_status < sizeofArray)
        return (char *)operation_statusArray[operation_status];
    else
        return (char *)"Unknown";
}

OpenAPI_operation_status_e OpenAPI_operation_status_FromString(char* operation_status)
{
    int stringToReturn = 0;
    const char *operation_statusArray[] =  { "NULL", "MBS_SESSION_START_COMPLETE", "MBS_SESSION_START_INCOMPLETE", "MBS_SESSION_UPDATE_COMPLETE", "MBS_SESSION_UPDATE_INCOMPLETE" };
    size_t sizeofArray = sizeof(operation_statusArray) / sizeof(operation_statusArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(operation_status, operation_statusArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

