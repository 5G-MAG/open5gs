
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "reporting_mode.h"

char* OpenAPI_reporting_mode_ToString(OpenAPI_reporting_mode_e reporting_mode)
{
    const char *reporting_modeArray[] =  { "NULL", "CONTINUOUS", "ONE_TIME" };
    size_t sizeofArray = sizeof(reporting_modeArray) / sizeof(reporting_modeArray[0]);
    if (reporting_mode < sizeofArray)
        return (char *)reporting_modeArray[reporting_mode];
    else
        return (char *)"Unknown";
}

OpenAPI_reporting_mode_e OpenAPI_reporting_mode_FromString(char* reporting_mode)
{
    int stringToReturn = 0;
    const char *reporting_modeArray[] =  { "NULL", "CONTINUOUS", "ONE_TIME" };
    size_t sizeofArray = sizeof(reporting_modeArray) / sizeof(reporting_modeArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(reporting_mode, reporting_modeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

