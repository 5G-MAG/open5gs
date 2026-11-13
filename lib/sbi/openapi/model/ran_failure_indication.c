
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ran_failure_indication.h"

char* OpenAPI_ran_failure_indication_ToString(OpenAPI_ran_failure_indication_e ran_failure_indication)
{
    const char *ran_failure_indicationArray[] =  { "NULL", "NG_RAN_FAILURE_WITHOUT_RESTART", "NG_RAN_NOT_REACHABLE" };
    size_t sizeofArray = sizeof(ran_failure_indicationArray) / sizeof(ran_failure_indicationArray[0]);
    if (ran_failure_indication < sizeofArray)
        return (char *)ran_failure_indicationArray[ran_failure_indication];
    else
        return (char *)"Unknown";
}

OpenAPI_ran_failure_indication_e OpenAPI_ran_failure_indication_FromString(char* ran_failure_indication)
{
    int stringToReturn = 0;
    const char *ran_failure_indicationArray[] =  { "NULL", "NG_RAN_FAILURE_WITHOUT_RESTART", "NG_RAN_NOT_REACHABLE" };
    size_t sizeofArray = sizeof(ran_failure_indicationArray) / sizeof(ran_failure_indicationArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(ran_failure_indication, ran_failure_indicationArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

