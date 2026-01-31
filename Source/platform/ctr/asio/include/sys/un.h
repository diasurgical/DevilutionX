#ifndef _SYS_UN_H
#define _SYS_UN_H 1

typedef unsigned short int sa_family_t;

struct sockaddr_un {
	sa_family_t sun_family;
	char sun_path[108];
};

#endif
