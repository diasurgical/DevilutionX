#ifndef _SYS_UIO_H
#define _SYS_UIO_H 1

#include <sys/types.h>

struct iovec {
	void *iov_base;
	size_t iov_len;
};

ssize_t readv(int __fd, const struct iovec *__iovec, int __count);
ssize_t writev(int __fd, const struct iovec *__iovec, int __count);

#endif
