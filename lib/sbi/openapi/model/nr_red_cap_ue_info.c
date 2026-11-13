
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "nr_red_cap_ue_info.h"

char* OpenAPI_nr_red_cap_ue_info_ToString(OpenAPI_nr_red_cap_ue_info_e nr_red_cap_ue_info)
{
    const char *nr_red_cap_ue_infoArray[] =  { "NULL", "NR_REDCAP_UE_ONLY", "BOTH_NR_REDCAP_UE_AND_NON_REDCAP_UE", "NON_REDCAP_UE_ONLY" };
    size_t sizeofArray = sizeof(nr_red_cap_ue_infoArray) / sizeof(nr_red_cap_ue_infoArray[0]);
    if (nr_red_cap_ue_info < sizeofArray)
        return (char *)nr_red_cap_ue_infoArray[nr_red_cap_ue_info];
    else
        return (char *)"Unknown";
}

OpenAPI_nr_red_cap_ue_info_e OpenAPI_nr_red_cap_ue_info_FromString(char* nr_red_cap_ue_info)
{
    int stringToReturn = 0;
    const char *nr_red_cap_ue_infoArray[] =  { "NULL", "NR_REDCAP_UE_ONLY", "BOTH_NR_REDCAP_UE_AND_NON_REDCAP_UE", "NON_REDCAP_UE_ONLY" };
    size_t sizeofArray = sizeof(nr_red_cap_ue_infoArray) / sizeof(nr_red_cap_ue_infoArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(nr_red_cap_ue_info, nr_red_cap_ue_infoArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

