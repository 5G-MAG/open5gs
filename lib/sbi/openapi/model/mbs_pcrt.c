
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_pcrt.h"

char* OpenAPI_mbs_pcrt_ToString(OpenAPI_mbs_pcrt_e mbs_pcrt)
{
    const char *mbs_pcrtArray[] =  { "NULL", "MBS_SESSION_UPDATE" };
    size_t sizeofArray = sizeof(mbs_pcrtArray) / sizeof(mbs_pcrtArray[0]);
    if (mbs_pcrt < sizeofArray)
        return (char *)mbs_pcrtArray[mbs_pcrt];
    else
        return (char *)"Unknown";
}

OpenAPI_mbs_pcrt_e OpenAPI_mbs_pcrt_FromString(char* mbs_pcrt)
{
    int stringToReturn = 0;
    const char *mbs_pcrtArray[] =  { "NULL", "MBS_SESSION_UPDATE" };
    size_t sizeofArray = sizeof(mbs_pcrtArray) / sizeof(mbs_pcrtArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(mbs_pcrt, mbs_pcrtArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

