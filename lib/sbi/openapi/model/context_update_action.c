
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "context_update_action.h"

char* OpenAPI_context_update_action_ToString(OpenAPI_context_update_action_e context_update_action)
{
    const char *context_update_actionArray[] =  { "NULL", "START", "TERMINATE" };
    size_t sizeofArray = sizeof(context_update_actionArray) / sizeof(context_update_actionArray[0]);
    if (context_update_action < sizeofArray)
        return (char *)context_update_actionArray[context_update_action];
    else
        return (char *)"Unknown";
}

OpenAPI_context_update_action_e OpenAPI_context_update_action_FromString(char* context_update_action)
{
    int stringToReturn = 0;
    const char *context_update_actionArray[] =  { "NULL", "START", "TERMINATE" };
    size_t sizeofArray = sizeof(context_update_actionArray) / sizeof(context_update_actionArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(context_update_action, context_update_actionArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

