
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ngran_failure_indication_any_of.h"

char* OpenAPI_ngran_failure_indication_any_of_ToString(OpenAPI_ngran_failure_indication_any_of_e ngran_failure_indication_any_of)
{
    const char *ngran_failure_indication_any_ofArray[] =  { "NULL", "NG_RAN_RESTART_OR_START", "NG_RAN_FAILURE_WITHOUT_RESTART", "NG_RAN_NOT_REACHABLE", "NG_RAN_REQUIRED_RELEASE" };
    size_t sizeofArray = sizeof(ngran_failure_indication_any_ofArray) / sizeof(ngran_failure_indication_any_ofArray[0]);
    if (ngran_failure_indication_any_of < sizeofArray)
        return (char *)ngran_failure_indication_any_ofArray[ngran_failure_indication_any_of];
    else
        return (char *)"Unknown";
}

OpenAPI_ngran_failure_indication_any_of_e OpenAPI_ngran_failure_indication_any_of_FromString(char* ngran_failure_indication_any_of)
{
    int stringToReturn = 0;
    const char *ngran_failure_indication_any_ofArray[] =  { "NULL", "NG_RAN_RESTART_OR_START", "NG_RAN_FAILURE_WITHOUT_RESTART", "NG_RAN_NOT_REACHABLE", "NG_RAN_REQUIRED_RELEASE" };
    size_t sizeofArray = sizeof(ngran_failure_indication_any_ofArray) / sizeof(ngran_failure_indication_any_ofArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(ngran_failure_indication_any_of, ngran_failure_indication_any_ofArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

