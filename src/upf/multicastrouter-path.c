#include <net/if.h>

#include "context.h"
#include "multicastrouter-path.h"
#include "multicastrouter/multicastrouter.h"

#define ZERO_INTERFACES     0
#define ONE_INTERFACE       1
#define MULTIPLE_INTERFACES 2

// References: https://stackoverflow.com/a/45796495, man if_nameindex
//
// Returns: - OGS_ERROR: if if_nameindex fails
//          - OGS_OK: otherwise
// Return parameters: - *interfaces_name: returns the name of the last interface at the list (other than 'lo' and 'ogstun')
//                    - *number_of_interfaces: returns the number of interfaces in the host. 'lo' and 'ogstun' are note counted.
static int find_interface_name(char *interface_name, int *number_of_interfaces){
    int interface_count = 0;

    // we use if_nameindex since we only need the name of the interfaces
    struct if_nameindex *interface_array = if_nameindex();
    if(interface_array == NULL){
        ogs_error("errno: %d", errno);
        ogs_error("Could not find interface names");
        return OGS_ERROR;
    }else{
        for(struct if_nameindex *interface = interface_array; !(interface->if_index == 0 && interface->if_name == NULL); interface++){
            if((strcmp("lo", interface->if_name) == 0) || (strcmp("ogstun", interface->if_name) == 0)){
                // dont take into account lo and ogstun
                ogs_debug("Skipping %s", interface->if_name);
            }else{
                ogs_debug("%s", interface->if_name);
                strcpy(interface_name, interface->if_name);
                interface_count++;
            }
        }
        if_freenameindex(interface_array);
    }

    if(interface_count == 0){
        *number_of_interfaces = ZERO_INTERFACES;
    }else if(interface_count == 1){
         *number_of_interfaces = ONE_INTERFACE;
    }else if(interface_count > 1){
        *number_of_interfaces = MULTIPLE_INTERFACES;
    }

    return OGS_OK;
}

// Tries to determine the name of the input interface of the multicast router.
// If the interface name is present in the configuration (context), use that value.
// Otherwise, if the host has only one interface (other than lo or ogstun), use that interface
//
// Returns: - OGS_ERROR: if the host has zero interface or the number of interface can't be determined
//          - OGS_OK: otherwise
// Return parameters: - *input_interface: string with the name of the input interface
static int determine_input_interface(char *input_interface){
    // if interface in configuration, use that
    if(strcmp(upf_self()->mbs_multicastrouter_input_interface, "") != 0){
        strcpy(input_interface, upf_self()->mbs_multicastrouter_input_interface);
        return OGS_OK;
    }

    // otherwise, use the one available if there is only one in the host (other than lo or ogstun)
    int number_of_interfaces;
    int error = find_interface_name(input_interface, &number_of_interfaces);
    if(error == OGS_ERROR){
        ogs_error("Could not list network interfaces errno: %d", errno);
        return OGS_ERROR;
    }else{
        if(number_of_interfaces == ZERO_INTERFACES){
            // if this happens there are other problems with the host, so we can just fail
            ogs_error("The host doesn't have any network interface");
            return OGS_ERROR;
        }else if(number_of_interfaces == ONE_INTERFACE){
            // interface name already in input_interface, we don't need to do anything else
        }else if(number_of_interfaces == MULTIPLE_INTERFACES){
            ogs_debug("There are multiple interfaces in the host (other than lo or ogstun)");
            if(strcmp(upf_self()->mbs_multicastrouter_input_interface, "") == 0){
                ogs_error("The input interface of the host needs to be specified in the configuration yaml in the section mbs/multicast_router/input_interface");
            return OGS_ERROR;
            }
        }
    }
    return OGS_OK;
}

// Determine the name of the output interface of the multicast router.
// If it is present in the configuration , use that, otherwise default to ogstun
//
// Return parameters: - *output_interface: string with the name of the output interface
static void determine_output_interface(char *output_interface){
    if(strcmp(upf_self()->mbs_multicastrouter_output_interface, "") != 0){
        strcpy(output_interface, upf_self()->mbs_multicastrouter_output_interface);
    }else{
        strcpy(output_interface, "ogstun");
    }
}

int upf_multicastrouter_init(void){
    int error;

    char input_interface[IFNAMSIZ];
    char output_interface[IFNAMSIZ];

    error = determine_input_interface(input_interface);
    if(error == OGS_ERROR){
        ogs_error("Could not determine input interface for the multicast router");
        return OGS_ERROR;
    }

    determine_output_interface(output_interface);

    ogs_info("Initializing multicast router using %s as input interface and %s as output interface", input_interface, output_interface);
    multicastrouter_init(input_interface, output_interface);

    return OGS_OK;
}

void upf_multicastrouter_final(void){
    multicastrouter_final();
}
