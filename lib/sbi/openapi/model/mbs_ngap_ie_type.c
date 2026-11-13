
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_ngap_ie_type.h"

char* OpenAPI_mbs_ngap_ie_type_ToString(OpenAPI_mbs_ngap_ie_type_e mbs_ngap_ie_type)
{
    const char *mbs_ngap_ie_typeArray[] =  { "NULL", "MBS_SES_ACT_REQ", "MBS_SES_DEACT_REQ", "MBS_SES_UPD_REQ" };
    size_t sizeofArray = sizeof(mbs_ngap_ie_typeArray) / sizeof(mbs_ngap_ie_typeArray[0]);
    if (mbs_ngap_ie_type < sizeofArray)
        return (char *)mbs_ngap_ie_typeArray[mbs_ngap_ie_type];
    else
        return (char *)"Unknown";
}

OpenAPI_mbs_ngap_ie_type_e OpenAPI_mbs_ngap_ie_type_FromString(char* mbs_ngap_ie_type)
{
    int stringToReturn = 0;
    const char *mbs_ngap_ie_typeArray[] =  { "NULL", "MBS_SES_ACT_REQ", "MBS_SES_DEACT_REQ", "MBS_SES_UPD_REQ" };
    size_t sizeofArray = sizeof(mbs_ngap_ie_typeArray) / sizeof(mbs_ngap_ie_typeArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(mbs_ngap_ie_type, mbs_ngap_ie_typeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

