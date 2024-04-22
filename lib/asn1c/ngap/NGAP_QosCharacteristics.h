/*
 * Generated by asn1c-0.9.29 (http://lionet.info/asn1c)
 * From ASN.1 module "NGAP-IEs"
 * 	found in "../support/ngap-r17.3.0/38413-h30.asn"
 * 	`asn1c -pdu=all -fcompound-names -findirect-choice -fno-include-deps -no-gen-BER -no-gen-XER -no-gen-OER -no-gen-UPER -no-gen-JER`
 */

#ifndef	_NGAP_QosCharacteristics_H_
#define	_NGAP_QosCharacteristics_H_


#include <asn_application.h>

/* Including external dependencies */
#include <constr_CHOICE.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dependencies */
typedef enum NGAP_QosCharacteristics_PR {
	NGAP_QosCharacteristics_PR_NOTHING,	/* No components present */
	NGAP_QosCharacteristics_PR_nonDynamic5QI,
	NGAP_QosCharacteristics_PR_dynamic5QI,
	NGAP_QosCharacteristics_PR_choice_Extensions
} NGAP_QosCharacteristics_PR;

/* Forward declarations */
struct NGAP_NonDynamic5QIDescriptor;
struct NGAP_Dynamic5QIDescriptor;
struct NGAP_ProtocolIE_SingleContainer;

/* NGAP_QosCharacteristics */
typedef struct NGAP_QosCharacteristics {
	NGAP_QosCharacteristics_PR present;
	union NGAP_QosCharacteristics_u {
		struct NGAP_NonDynamic5QIDescriptor	*nonDynamic5QI;
		struct NGAP_Dynamic5QIDescriptor	*dynamic5QI;
		struct NGAP_ProtocolIE_SingleContainer	*choice_Extensions;
	} choice;
	
	/* Context for parsing across buffer boundaries */
	asn_struct_ctx_t _asn_ctx;
} NGAP_QosCharacteristics_t;

/* Implementation */
extern asn_TYPE_descriptor_t asn_DEF_NGAP_QosCharacteristics;
extern asn_CHOICE_specifics_t asn_SPC_NGAP_QosCharacteristics_specs_1;
extern asn_TYPE_member_t asn_MBR_NGAP_QosCharacteristics_1[3];
extern asn_per_constraints_t asn_PER_type_NGAP_QosCharacteristics_constr_1;

#ifdef __cplusplus
}
#endif

#endif	/* _NGAP_QosCharacteristics_H_ */
#include <asn_internal.h>
