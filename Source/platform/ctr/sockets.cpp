#include "platform/ctr/sockets.hpp"

#include <cstdint>
#include <malloc.h>

#include <3ds.h>

#include "utils/log.hpp"

namespace devilution {

constexpr auto SOC_ALIGN = 0x1000;
constexpr auto SOC_BUFFERSIZE = 0x100000;
static u32 *socBuffer;
static bool initialized;

static bool waitForWifi()
{
	// 100 ms
	constexpr s64 sleepNano = 100 * 1000 * 1000;

	// 5 sec
	constexpr int loopCount = 5 * 1000 / 100;

	uint32_t wifi = 0;
	for (int i = 0; i < loopCount; ++i) {
		if (R_SUCCEEDED(ACU_GetWifiStatus(&wifi)) && wifi)
			return true;

		svcSleepThread(sleepNano);
	}

	return false;
}

void n3ds_socExit()
{
	if (socBuffer == nullptr)
		return;

	socExit();
	free(socBuffer);
	socBuffer = nullptr;
}

void n3ds_socInit()
{
	if (!waitForWifi()) {
		LogError("n3ds_socInit: Wifi off");
		return;
	}

	socBuffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	if (socBuffer == nullptr) {
		LogError("n3ds_socInit: memalign() failed");
		return;
	}

	Result result = socInit(socBuffer, SOC_BUFFERSIZE);
	if (!R_SUCCEEDED(result)) {
		LogError("n3ds_socInit: socInit() failed");
		free(socBuffer);
		return;
	}

	if (!initialized)
		atexit([]() { n3ds_socExit(); });
	initialized = true;
}

} // namespace devilution
