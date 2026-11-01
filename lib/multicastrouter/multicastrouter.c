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
#include <linux/mroute6.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

// the same for IPv6: a second routing socket, with its own multicast interfaces (MIFs)
static int sockfd6 = -1;

// The channels joined, with the number of users of each
typedef struct channel_s {
    ogs_lnode_t lnode;
    int family;
    bool has_source;
    uint8_t source[16];
    uint8_t group[16];
    int users;
} channel_t;
static OGS_LIST(channel_list);

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

        // IPv6: the same, on a second socket. Without it the IPv4 router still works and an IPv6 channel is refused.
        sockfd6 = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
        if (sockfd6 == -1) {
            ogs_warn("No IPv6 raw socket (errno %d): IPv6 multicast is not routed", errno);
        } else if (setsockopt(sockfd6, IPPROTO_IPV6, MRT6_INIT, &val, sizeof(val)) != 0) {
            ogs_warn("Could not initialize IPv6 multicast routing (errno %d): IPv6 multicast is not routed", errno);
            close(sockfd6);
            sockfd6 = -1;
        } else {
            struct mif6ctl mif;
            memset(&mif, 0, sizeof mif);
            mif.mif6c_mifi = interface_input_vifi;
            mif.mif6c_pifi = interface_input_index;
            error = setsockopt(sockfd6, IPPROTO_IPV6, MRT6_ADD_MIF, &mif, sizeof mif);
            memset(&mif, 0, sizeof mif);
            mif.mif6c_mifi = interface_output_vifi;
            mif.mif6c_pifi = interface_output_index;
            error |= setsockopt(sockfd6, IPPROTO_IPV6, MRT6_ADD_MIF, &mif, sizeof mif);
            if (error != 0) {
                ogs_warn("Could not add the IPv6 multicast interfaces (errno %d): IPv6 multicast is not routed", errno);
                close(sockfd6);
                sockfd6 = -1;
            } else {
                ogs_debug("IPv6 multicast routing initialized");
            }
        }

        // MULTICAST ROUTER INITIALIZED
        initialized = 1;
    }
    return OGS_OK;
}

void multicastrouter_final(void){
    channel_t *channel = NULL, *next = NULL;

    ogs_list_for_each_safe(&channel_list, next, channel) {
        ogs_list_remove(&channel_list, channel);
        ogs_free(channel);
    }
    if (sockfd6 != -1) {
        close(sockfd6);
        sockfd6 = -1;
    }
    ogs_debug("Closing the multicast router socket");
    int error = close(sockfd);
    if(error != 0){
        ogs_error("Could not close the multicast router socket, errno %d", errno);
    }
}

static channel_t *channel_find(int family, const void *source, const void *group)
{
    channel_t *channel = NULL;
    size_t len = family == AF_INET ? 4 : 16;

    ogs_list_for_each(&channel_list, channel) {
        if (channel->family == family && memcmp(channel->group, group, len) == 0 &&
                channel->has_source == (source != NULL) &&
                (source == NULL || memcmp(channel->source, source, len) == 0))
            return channel;
    }
    return NULL;
}

// Joins (source, group) or (*, group) on the input interface, with the protocol independent options
// (MCAST_JOIN_SOURCE_GROUP, MCAST_JOIN_GROUP) of RFC 3678, which take an interface index.
static int channel_join(int fd, int level, int family, const void *source, const void *group)
{
    struct sockaddr_storage grp, src;
    memset(&grp, 0, sizeof grp);
    memset(&src, 0, sizeof src);

    if (family == AF_INET) {
        struct sockaddr_in *g = (struct sockaddr_in *)&grp, *sr = (struct sockaddr_in *)&src;
        g->sin_family = sr->sin_family = AF_INET;
        memcpy(&g->sin_addr, group, 4);
        if (source)
            memcpy(&sr->sin_addr, source, 4);
    } else {
        struct sockaddr_in6 *g = (struct sockaddr_in6 *)&grp, *sr = (struct sockaddr_in6 *)&src;
        g->sin6_family = sr->sin6_family = AF_INET6;
        memcpy(&g->sin6_addr, group, 16);
        if (source)
            memcpy(&sr->sin6_addr, source, 16);
    }

    if (source) {
        struct group_source_req req;
        memset(&req, 0, sizeof req);
        req.gsr_interface = interface_input_index;
        memcpy(&req.gsr_group, &grp, sizeof grp);
        memcpy(&req.gsr_source, &src, sizeof src);
        return setsockopt(fd, level, MCAST_JOIN_SOURCE_GROUP, &req, sizeof req);
    } else {
        struct group_req req;
        memset(&req, 0, sizeof req);
        req.gr_interface = interface_input_index;
        memcpy(&req.gr_group, &grp, sizeof grp);
        return setsockopt(fd, level, MCAST_JOIN_GROUP, &req, sizeof req);
    }
}

static int channel_leave(int fd, int level, int family, const void *source, const void *group)
{
    struct sockaddr_storage grp, src;
    memset(&grp, 0, sizeof grp);
    memset(&src, 0, sizeof src);

    if (family == AF_INET) {
        struct sockaddr_in *g = (struct sockaddr_in *)&grp, *sr = (struct sockaddr_in *)&src;
        g->sin_family = sr->sin_family = AF_INET;
        memcpy(&g->sin_addr, group, 4);
        if (source)
            memcpy(&sr->sin_addr, source, 4);
    } else {
        struct sockaddr_in6 *g = (struct sockaddr_in6 *)&grp, *sr = (struct sockaddr_in6 *)&src;
        g->sin6_family = sr->sin6_family = AF_INET6;
        memcpy(&g->sin6_addr, group, 16);
        if (source)
            memcpy(&sr->sin6_addr, source, 16);
    }

    if (source) {
        struct group_source_req req;
        memset(&req, 0, sizeof req);
        req.gsr_interface = interface_input_index;
        memcpy(&req.gsr_group, &grp, sizeof grp);
        memcpy(&req.gsr_source, &src, sizeof src);
        return setsockopt(fd, level, MCAST_LEAVE_SOURCE_GROUP, &req, sizeof req);
    } else {
        struct group_req req;
        memset(&req, 0, sizeof req);
        req.gr_interface = interface_input_index;
        memcpy(&req.gr_group, &grp, sizeof grp);
        return setsockopt(fd, level, MCAST_LEAVE_GROUP, &req, sizeof req);
    }
}

// Adds (add) or deletes the multicast route of the channel, from the input to the output interface.
// A channel for any source has the unspecified address as the origin.
static int channel_route(int family, const void *source, const void *group, bool add)
{
    if (family == AF_INET) {
        struct mfcctl route;
        memset(&route, 0, sizeof route);
        if (source)
            memcpy(&route.mfcc_origin.s_addr, source, 4);
        memcpy(&route.mfcc_mcastgrp.s_addr, group, 4);
        route.mfcc_parent = interface_input_vifi;       // inbound interface
        route.mfcc_ttls[interface_output_vifi] = 1;     // outbound interface
        // A (*,G) entry, with no origin, is a proxy entry for the kernel
        if (!source)
            return setsockopt(sockfd, IPPROTO_IP, add ? MRT_ADD_MFC_PROXY : MRT_DEL_MFC_PROXY, &route, sizeof route);
        return setsockopt(sockfd, IPPROTO_IP, add ? MRT_ADD_MFC : MRT_DEL_MFC, &route, sizeof route);
    } else {
        struct mf6cctl route;
        memset(&route, 0, sizeof route);
        route.mf6cc_origin.sin6_family = AF_INET6;
        if (source)
            memcpy(&route.mf6cc_origin.sin6_addr, source, 16);
        route.mf6cc_mcastgrp.sin6_family = AF_INET6;
        memcpy(&route.mf6cc_mcastgrp.sin6_addr, group, 16);
        route.mf6cc_parent = interface_input_vifi;
        IF_SET(interface_output_vifi, &route.mf6cc_ifset);
        return setsockopt(sockfd6, IPPROTO_IPV6, add ? MRT6_ADD_MFC : MRT6_DEL_MFC, &route, sizeof route);
    }
}

int multicastrouter_join(int family, const void *source, const void *group)
{
    channel_t *channel = NULL;
    int fd = family == AF_INET ? sockfd : sockfd6;
    int level = family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6;

    ogs_assert(family == AF_INET || family == AF_INET6);
    ogs_assert(group);

    if (!initialized || fd == -1) {
        ogs_error("No multicast routing for address family %d", family);
        return MULTICASTROUTER_UNSUPPORTED;
    }

    channel = channel_find(family, source, group);
    if (channel) {
        channel->users++;
        return OGS_OK;
    }

    // ADD MULTICAST ROUTE: the origin (source or any), the group, the input interface(IIF), the output interface (OIF)
    if (channel_route(family, source, group, true) != 0) {
        ogs_error("Multicast route not created, errno %d", errno);
        return OGS_ERROR;
    }

    // JOIN MULTICAST GROUP
    if (channel_join(fd, level, family, source, group) != 0) {
        ogs_error("Error joining the multicast channel, errno %d", errno);
        channel_route(family, source, group, false);
        return OGS_ERROR;
    }

    channel = ogs_calloc(1, sizeof *channel);
    ogs_assert(channel);
    channel->family = family;
    channel->has_source = source != NULL;
    if (source)
        memcpy(channel->source, source, family == AF_INET ? 4 : 16);
    memcpy(channel->group, group, family == AF_INET ? 4 : 16);
    channel->users = 1;
    ogs_list_add(&channel_list, channel);

    return OGS_OK;
}

void multicastrouter_leave(int family, const void *source, const void *group)
{
    channel_t *channel = channel_find(family, source, group);
    int fd = family == AF_INET ? sockfd : sockfd6;
    int level = family == AF_INET ? IPPROTO_IP : IPPROTO_IPV6;

    if (!channel)
        return;
    if (--channel->users > 0)
        return;

    if (channel_leave(fd, level, family, source, group) != 0)
        ogs_error("Error leaving the multicast channel, errno %d", errno);
    if (channel_route(family, source, group, false) != 0)
        ogs_error("Multicast route not deleted, errno %d", errno);
    ogs_list_remove(&channel_list, channel);
    ogs_free(channel);
}
