
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "operation_status_any_of.h"

char* OpenAPI_operation_status_any_of_ToString(OpenAPI_operation_status_any_of_e operation_status_any_of)
{
    const char *operation_status_any_ofArray[] =  { "NULL", "MBS_SESSION_START_COMPLETE", "MBS_SESSION_START_INCOMPLETE", "MBS_SESSION_UPDATE_COMPLETE", "MBS_SESSION_UPDATE_INCOMPLETE" };
    size_t sizeofArray = sizeof(operation_status_any_ofArray) / sizeof(operation_status_any_ofArray[0]);
    if (operation_status_any_of < sizeofArray)
        return (char *)operation_status_any_ofArray[operation_status_any_of];
    else
        return (char *)"Unknown";
}

OpenAPI_operation_status_any_of_e OpenAPI_operation_status_any_of_FromString(char* operation_status_any_of)
{
    int stringToReturn = 0;
    const char *operation_status_any_ofArray[] =  { "NULL", "MBS_SESSION_START_COMPLETE", "MBS_SESSION_START_INCOMPLETE", "MBS_SESSION_UPDATE_COMPLETE", "MBS_SESSION_UPDATE_INCOMPLETE" };
    size_t sizeofArray = sizeof(operation_status_any_ofArray) / sizeof(operation_status_any_ofArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(operation_status_any_of, operation_status_any_ofArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

