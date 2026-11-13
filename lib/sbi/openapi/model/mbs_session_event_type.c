
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_session_event_type.h"

char* OpenAPI_mbs_session_event_type_ToString(OpenAPI_mbs_session_event_type_e mbs_session_event_type)
{
    const char *mbs_session_event_typeArray[] =  { "NULL", "MBS_REL_TMGI_EXPIRY", "BROADCAST_DELIVERY_STATUS", "INGRESS_TUNNEL_ADD_CHANGE" };
    size_t sizeofArray = sizeof(mbs_session_event_typeArray) / sizeof(mbs_session_event_typeArray[0]);
    if (mbs_session_event_type < sizeofArray)
        return (char *)mbs_session_event_typeArray[mbs_session_event_type];
    else
        return (char *)"Unknown";
}

OpenAPI_mbs_session_event_type_e OpenAPI_mbs_session_event_type_FromString(char* mbs_session_event_type)
{
    int stringToReturn = 0;
    const char *mbs_session_event_typeArray[] =  { "NULL", "MBS_REL_TMGI_EXPIRY", "BROADCAST_DELIVERY_STATUS", "INGRESS_TUNNEL_ADD_CHANGE" };
    size_t sizeofArray = sizeof(mbs_session_event_typeArray) / sizeof(mbs_session_event_typeArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(mbs_session_event_type, mbs_session_event_typeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

