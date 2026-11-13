
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_pcc_rule_status.h"

char* OpenAPI_mbs_pcc_rule_status_ToString(OpenAPI_mbs_pcc_rule_status_e mbs_pcc_rule_status)
{
    const char *mbs_pcc_rule_statusArray[] =  { "NULL", "ACTIVE", "INACTIVE" };
    size_t sizeofArray = sizeof(mbs_pcc_rule_statusArray) / sizeof(mbs_pcc_rule_statusArray[0]);
    if (mbs_pcc_rule_status < sizeofArray)
        return (char *)mbs_pcc_rule_statusArray[mbs_pcc_rule_status];
    else
        return (char *)"Unknown";
}

OpenAPI_mbs_pcc_rule_status_e OpenAPI_mbs_pcc_rule_status_FromString(char* mbs_pcc_rule_status)
{
    int stringToReturn = 0;
    const char *mbs_pcc_rule_statusArray[] =  { "NULL", "ACTIVE", "INACTIVE" };
    size_t sizeofArray = sizeof(mbs_pcc_rule_statusArray) / sizeof(mbs_pcc_rule_statusArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(mbs_pcc_rule_status, mbs_pcc_rule_statusArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

