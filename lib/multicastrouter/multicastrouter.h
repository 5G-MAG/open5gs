#ifndef MULTICASTROUTER
#define MULTICASTROUTER

#ifdef __cplusplus
extern "C" {
#endif

int multicastrouter_init(const char *interface_input, const char *interface_output);
void multicastrouter_final(void);
int multicastrouter_add_route_and_join(const char *ip_source_string, const char *ip_multicast_string);

#ifdef __cplusplus
}
#endif

#endif /* MULTICASTROUTER */
