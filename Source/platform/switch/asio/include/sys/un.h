#ifndef _SYS_UN_H
#define _SYS_UN_H 1

#include <sys/types.h>

#ifndef _SA_FAMILY_T_DECLARED
typedef __sa_family_t sa_family_t;
#define _SA_FAMILY_T_DECLARED
#endif

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

#endif
