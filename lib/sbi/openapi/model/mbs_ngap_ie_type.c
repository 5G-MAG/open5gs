
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mbs_ngap_ie_type.h"

char* OpenAPI_mbs_ngap_ie_type_ToString(OpenAPI_mbs_ngap_ie_type_e mbs_ngap_ie_type)
{
    const char *mbs_ngap_ie_typeArray[] =  { "NULL", "PDU_RES_SETUP_REQ", "PDU_RES_REL_CMD", "PDU_RES_MOD_REQ", "HANDOVER_CMD", "HANDOVER_REQUIRED", "HANDOVER_PREP_FAIL", "SRC_TO_TAR_CONTAINER", "TAR_TO_SRC_CONTAINER", "TAR_TO_SRC_FAIL_CONTAINER", "RAN_STATUS_TRANS_CONTAINER", "SON_CONFIG_TRANSFER", "NRPPA_PDU", "UE_RADIO_CAPABILITY", "RIM_INFO_TRANSFER", "SECONDARY_RAT_USAGE", "PC5_QOS_PARA", "EARLY_STATUS_TRANS_CONTAINER", "UE_RADIO_CAPABILITY_FOR_PAGING", "MBS_DIS_SETUP_REQ", "MBS_DIS_SETUP_RSP", "MBS_DIS_SETUP_FAIL", "MBS_DIS_REL_REQ", "MBS_SES_ACT_REQ", "MBS_SES_DEACT_REQ", "MBS_SES_UPD_REQ", "MBS_SES_REQ", "MBS_SES_RSP", "MBS_SES_FAIL", "MBS_SES_REL_RSP" };
    size_t sizeofArray = sizeof(mbs_ngap_ie_typeArray) / sizeof(mbs_ngap_ie_typeArray[0]);
    if (mbs_ngap_ie_type < sizeofArray)
        return (char *)mbs_ngap_ie_typeArray[mbs_ngap_ie_type];
    else
        return (char *)"Unknown";
}

OpenAPI_mbs_ngap_ie_type_e OpenAPI_mbs_ngap_ie_type_FromString(char* mbs_ngap_ie_type)
{
    int stringToReturn = 0;
    const char *mbs_ngap_ie_typeArray[] =  { "NULL", "PDU_RES_SETUP_REQ", "PDU_RES_REL_CMD", "PDU_RES_MOD_REQ", "HANDOVER_CMD", "HANDOVER_REQUIRED", "HANDOVER_PREP_FAIL", "SRC_TO_TAR_CONTAINER", "TAR_TO_SRC_CONTAINER", "TAR_TO_SRC_FAIL_CONTAINER", "RAN_STATUS_TRANS_CONTAINER", "SON_CONFIG_TRANSFER", "NRPPA_PDU", "UE_RADIO_CAPABILITY", "RIM_INFO_TRANSFER", "SECONDARY_RAT_USAGE", "PC5_QOS_PARA", "EARLY_STATUS_TRANS_CONTAINER", "UE_RADIO_CAPABILITY_FOR_PAGING", "MBS_DIS_SETUP_REQ", "MBS_DIS_SETUP_RSP", "MBS_DIS_SETUP_FAIL", "MBS_DIS_REL_REQ", "MBS_SES_ACT_REQ", "MBS_SES_DEACT_REQ", "MBS_SES_UPD_REQ", "MBS_SES_REQ", "MBS_SES_RSP", "MBS_SES_FAIL", "MBS_SES_REL_RSP" };
    size_t sizeofArray = sizeof(mbs_ngap_ie_typeArray) / sizeof(mbs_ngap_ie_typeArray[0]);
    while (stringToReturn < sizeofArray) {
        if (strcmp(mbs_ngap_ie_type, mbs_ngap_ie_typeArray[stringToReturn]) == 0) {
            return stringToReturn;
        }
        stringToReturn++;
    }
    return 0;
}

