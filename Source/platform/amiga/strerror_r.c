/*
 * GCC 15's libstdc++ (system_error.o) calls the XSI-compliant `strerror_r`,
 * but libnix only ships the GNU-flavoured one, so linking fails with an
 * undefined `__xpg_strerror_r`.
 */
#include <errno.h>
#include <stddef.h>
#include <string.h>

int __xpg_strerror_r(int errnum, char *buf, size_t buflen);

int __xpg_strerror_r(int errnum, char *buf, size_t buflen)
{
	if (buf == NULL || buflen == 0)
		return ERANGE;

	const char *msg = strerror(errnum);
	if (msg == NULL)
		return EINVAL;

	const size_t len = strlen(msg);
	if (len >= buflen) {
		memcpy(buf, msg, buflen - 1);
		buf[buflen - 1] = '\0';
		return ERANGE;
	}

	memcpy(buf, msg, len + 1);
	return 0;
}
