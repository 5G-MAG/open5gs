#ifndef MULTICASTROUTER
#define MULTICASTROUTER

#ifdef __cplusplus
extern "C" {
#endif

/* Returned by multicastrouter_join() when the kernel has no multicast routing for the address family. */
#define MULTICASTROUTER_UNSUPPORTED (-2)

int multicastrouter_init(const char *interface_input, const char *interface_output);
void multicastrouter_final(void);

/*
 * Receives the group (IPv4: AF_INET, 4 octets; IPv6: AF_INET6, 16 octets, network order) on the input interface
 * and routes it to the output interface: joins the source specific channel (source, group), or the group for any
 * source if source is NULL, and adds the multicast route. Joining an already joined channel counts a user: the
 * channel is left when the same number of multicastrouter_leave() calls were made.
 * Returns OGS_OK, OGS_ERROR, or MULTICASTROUTER_UNSUPPORTED.
 */
int multicastrouter_join(int family, const void *source, const void *group);
void multicastrouter_leave(int family, const void *source, const void *group);

#ifdef __cplusplus
}
#endif

#endif /* MULTICASTROUTER */
