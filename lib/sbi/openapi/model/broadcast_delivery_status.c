
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "broadcast_delivery_status.h"

char* OpenAPI_broadcast_delivery_status_ToString(OpenAPI_broadcast_delivery_status_e broadcast_delivery_status)
{
    const char *broadcast_delivery_statusArray[] =  { "NULL", "STARTED", "TERMINATED" };
    size_t sizeofArray = sizeof(broadcast_delivery_statusArray) / sizeof(broadcast_delivery_statusArray[0]);
    if (broadcast_delivery_status < sizeofArray)
        return (char *)broadcast_delivery_statusArray[broadcast_delivery_status];
    else
        return (char *)"Unknown";
}

OpenAPI_broadcast_delivery_status_e OpenAPI_broadcast_delivery_status_FromString(char* broadcast_delivery_status)
{
    int stringToReturn = 0;
    const char *broadcast_delivery_statusArray[] =  { "NULL", "STARTED", "TERMINATED" };
    size_t sizeofArray = sizeof(broadcast_delivery_statusArray) / sizeof(broadcast_delivery_statusArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(broadcast_delivery_status, broadcast_delivery_statusArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

