// This module allows setting up multicast routers in the kernel and join multicast groups.
// The implementation is based on what smcroute does.
// It only supports routes from one input interface to one output interface (defined when initializing the module).
//
// References:
// https://github.com/troglobit/smcroute
// https://tldp.org/HOWTO/Multicast-HOWTO-7.html
// https://man.cx/multicast(4)
// https://manpages.ubuntu.com/manpages/xenial//man4/multicast.4freebsd.html
// manpages
// chatgpt

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <netinet/ip.h>
#include <sys/socket.h>
#include <linux/mroute.h>
#include <net/if.h>

#include "core/ogs-core.h"
#include "multicastrouter.h"

// this socket is used for configuring the multicast routes in the kernel
// and for joining the multicast groups, not for receiving traffic
static int sockfd;

// variable to avoid running initialization code twice
static int initialized = 0;

// Only one program can use the MRT interface, so we should always be able to create the virtual interfaces with indexes 0 and 1
static const int interface_input_vifi = 0;
static const int interface_output_vifi = 1;

// indexes of output interface and input interface
static int interface_input_index;
static int interface_output_index;

int multicastrouter_init(const char *interface_input, const char *interface_output){
    if(initialized == 0){
        int error;

        // OPEN SOCKET
        sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP);
        if(sockfd == -1){
            ogs_error("Could not open socket");
            return OGS_ERROR;
        }else{
            ogs_debug("Opening IGMP RAW socket with sockfd %d", sockfd);
        }

        // ENABLE MULTICAST ROUTING
        int val = 1; // 1 means enable
        error = setsockopt(sockfd, IPPROTO_IP, MRT_INIT, &val, sizeof(val));
        if(error == 0){
            ogs_debug("Multicast routing initialized");
        }else{
            ogs_error("Could not initialize multicast routing");
            ogs_error("ERROR CODE errno %d", errno);
            return OGS_ERROR;
        }

        // FIND INTERFACE INDEX FOR THE TWO INTERFACES
        interface_input_index = if_nametoindex(interface_input);
        if(interface_input_index == 0){
            ogs_error("Could not find interface input");
            ogs_error("errno %d", errno);
            return OGS_ERROR;
        }else{
            ogs_debug("Interface input  %s index: %d", interface_input , interface_input_index);
        }

        interface_output_index = if_nametoindex(interface_output);
        if(interface_output_index == 0){
            ogs_error("Could not find interface output");
            ogs_error("errno %d", errno);
            return OGS_ERROR;
        }else{
            ogs_debug("Interface output  %s index: %d", interface_output , interface_output_index);
        }

        // CREATE VIF FOR THE TWO INTERFACES
        // Warning: the struct vifctl in the documentation seems to be outdated, see smcroute/src/kern.c:kern_vif_add()
        struct vifctl interface_input_vifctl;
        memset(&interface_input_vifctl, 0, sizeof interface_input_vifctl);
        interface_input_vifctl.vifc_vifi = interface_input_vifi;
        interface_input_vifctl.vifc_flags = VIFF_USE_IFINDEX;
        interface_input_vifctl.vifc_lcl_ifindex = interface_input_index;
        interface_input_vifctl.vifc_threshold = 1;
        ogs_debug("interface_input_vifctl.vifc_vifi: %d", interface_input_vifctl.vifc_vifi);
        error = setsockopt(sockfd, IPPROTO_IP, MRT_ADD_VIF, &interface_input_vifctl, sizeof interface_input_vifctl);
        if(error == 0){
            ogs_debug("vif for interface_input created");
        }else{
            ogs_error("ERROR: vif for interface_input not created");
            ogs_error("error code %d, errno %d", error, errno);
        }

        struct vifctl interface_output_vifctl;
        memset(&interface_output_vifctl, 0, sizeof interface_output_vifctl);
        interface_output_vifctl.vifc_vifi = interface_output_vifi;
        interface_output_vifctl.vifc_flags = VIFF_USE_IFINDEX;
        interface_output_vifctl.vifc_lcl_ifindex = interface_output_index;
        interface_output_vifctl.vifc_threshold = 1;
        ogs_debug("interface_output_vifctl.vifc_vifi: %d", interface_output_vifctl.vifc_vifi);
        error = setsockopt(sockfd, IPPROTO_IP, MRT_ADD_VIF, &interface_output_vifctl, sizeof interface_output_vifctl);
        if(error == 0){
            ogs_debug("vif for interface_output created");
        }else{
            ogs_error("ERROR: vif for interface_output not created");
            ogs_error("error code %d, errno %d", error, errno);
        }

        // MULTICAST ROUTER INITIALIZED
        initialized = 1;
    }
    return OGS_OK;
}

void multicastrouter_final(void){
    ogs_debug("Closing the multicast router socket");
    int error = close(sockfd);
    if(error != 0){
        ogs_error("Could not close the multicast router socket, errno %d", errno);
    }
}

int multicastrouter_add_route_and_join(const char *ip_source_string, const char *ip_multicast_string){
    int error;

    // ADD MULTICAST ROUTE
    // we need source ip address, multicast ip address, input interface(IIF), output interface (OIF)
    in_addr_t source = inet_addr(ip_source_string);
    in_addr_t group = inet_addr(ip_multicast_string);
    if(source == INADDR_NONE){
        ogs_error("ERROR: source is INADDR_NONE");
        return OGS_ERROR;
    }
    if(group == INADDR_NONE){
        ogs_error("ERROR: group is INADDR_NONE");
        return OGS_ERROR;
    }

    struct mfcctl route;
    memset(&route, 0, sizeof route);
    route.mfcc_origin.s_addr = source;
    route.mfcc_mcastgrp.s_addr = group;
    route.mfcc_parent = interface_input_vifi; //inbound interface
    route.mfcc_ttls[interface_output_vifi] = 1; // outbound interface

    error = setsockopt(sockfd, IPPROTO_IP, MRT_ADD_MFC, &route, sizeof route);
    if(error == 0){
        ogs_debug("route created");
    }else{
        ogs_error("ERROR: route not created");
        ogs_error("error code %d, errno %d", error, errno);
        return OGS_ERROR;
    }

    // JOIN MULTICAST GROUP
    // only works in machines with struct ip_mreqn (since Linux 2.2)
    struct ip_mreqn imr;
    memset(&imr, 0, sizeof imr);
    imr.imr_multiaddr.s_addr = inet_addr(ip_multicast_string);
    imr.imr_ifindex   = interface_input_index;
    error = setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imr, sizeof imr);
    if(error == 0){
        ogs_debug("Successfully join multicast group %s", ip_multicast_string);
    }else{
        // errno 98: Address already in use.
        // In this case it means we have already joined the group
        if (errno == 98){
            // we dont fail if we already joined the group
        }else{
            ogs_error("Error joining multicast group %s", ip_multicast_string);
            ogs_error("error code %d, errno %d", error, errno);
            return OGS_ERROR;
        }
    }

    return OGS_OK;
}
