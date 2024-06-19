
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ngran_failure_indication.h"

char* OpenAPI_ngran_failure_indication_ToString(OpenAPI_ngran_failure_indication_e ngran_failure_indication)
{
    const char *ngran_failure_indicationArray[] =  { "NULL", "NG_RAN_RESTART_OR_START", "NG_RAN_FAILURE_WITHOUT_RESTART", "NG_RAN_NOT_REACHABLE", "NG_RAN_REQUIRED_RELEASE" };
    size_t sizeofArray = sizeof(ngran_failure_indicationArray) / sizeof(ngran_failure_indicationArray[0]);
    if (ngran_failure_indication < sizeofArray)
        return (char *)ngran_failure_indicationArray[ngran_failure_indication];
    else
        return (char *)"Unknown";
}

OpenAPI_ngran_failure_indication_e OpenAPI_ngran_failure_indication_FromString(char* ngran_failure_indication)
{
    int stringToReturn = 0;
    const char *ngran_failure_indicationArray[] =  { "NULL", "NG_RAN_RESTART_OR_START", "NG_RAN_FAILURE_WITHOUT_RESTART", "NG_RAN_NOT_REACHABLE", "NG_RAN_REQUIRED_RELEASE" };
    size_t sizeofArray = sizeof(ngran_failure_indicationArray) / sizeof(ngran_failure_indicationArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(ngran_failure_indication, ngran_failure_indicationArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

