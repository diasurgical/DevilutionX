#ifndef _NET_IF_H
#define _NET_IF_H 1

#define IF_NAMESIZE 16

struct if_nameindex {
	unsigned int if_index;
	char *if_name;
};

#ifdef __cplusplus
extern "C" {
#endif

unsigned int if_nametoindex(const char *__ifname);
char *if_indextoname(unsigned int __ifindex, char *__ifname);
struct if_nameindex *if_nameindex();
void if_freenameindex(struct if_nameindex *__ptr);

#ifdef __cplusplus
}
#endif

#endif
