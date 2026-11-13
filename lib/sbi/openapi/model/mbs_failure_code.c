
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_failure_code.h"

char* OpenAPI_mbs_failure_code_ToString(OpenAPI_mbs_failure_code_e mbs_failure_code)
{
    const char *mbs_failure_codeArray[] =  { "NULL", "NF_MALFUNCTION", "NF_RESOURCES_UNAVAILABLE", "RESOURCE_ALLOCATION_FAILURE", "MBS_QOS_VALIDATION_FAILURE", "NO_MBS_QOS_FLOW", "MBS_QOS_DECISION_ERROR", "MBS_POLICY_PARAM_ERROR" };
    size_t sizeofArray = sizeof(mbs_failure_codeArray) / sizeof(mbs_failure_codeArray[0]);
    if (mbs_failure_code < sizeofArray)
        return (char *)mbs_failure_codeArray[mbs_failure_code];
    else
        return (char *)"Unknown";
}

OpenAPI_mbs_failure_code_e OpenAPI_mbs_failure_code_FromString(char* mbs_failure_code)
{
    int stringToReturn = 0;
    const char *mbs_failure_codeArray[] =  { "NULL", "NF_MALFUNCTION", "NF_RESOURCES_UNAVAILABLE", "RESOURCE_ALLOCATION_FAILURE", "MBS_QOS_VALIDATION_FAILURE", "NO_MBS_QOS_FLOW", "MBS_QOS_DECISION_ERROR", "MBS_POLICY_PARAM_ERROR" };
    size_t sizeofArray = sizeof(mbs_failure_codeArray) / sizeof(mbs_failure_codeArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(mbs_failure_code, mbs_failure_codeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

