#include <3ds.h>
#include <cstdint>
#include <sodium.h>
#include <sys/types.h>

static const char *randombytes_ctrrandom_implementation_name()
{
	return "ctrrandom";
}

static bool randombytes_ctrrandom_tryfill(void *const buf, const size_t size)
{
	Result res;
	if (!psGetSessionHandle()) {
		res = psInit();
		if (!R_SUCCEEDED(res))
			return false;
	}
	res = PS_GenerateRandomBytes(buf, size);
	return R_SUCCEEDED(res);
}

static uint32_t randombytes_ctrrandom()
{
	uint32_t num;
	if (!randombytes_ctrrandom_tryfill(&num, sizeof(uint32_t)))
		sodium_misuse();
	return num;
}

static void randombytes_ctrrandom_buf(void *const buf, const size_t size)
{
	if (!randombytes_ctrrandom_tryfill(buf, size))
		sodium_misuse();
}

struct randombytes_implementation randombytes_ctrrandom_implementation = {
	randombytes_ctrrandom_implementation_name,
	randombytes_ctrrandom,
	nullptr,
	nullptr,
	randombytes_ctrrandom_buf,
	nullptr
};

void randombytes_ctrrandom_init()
{
	randombytes_set_implementation(&randombytes_ctrrandom_implementation);
}
