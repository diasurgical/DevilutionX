#include <3ds.h>
#include <cstdio>
#include <cstdlib>

#include "platform/ctr/cfgu_service.hpp"
#include "platform/ctr/random.hpp"
#include "platform/ctr/sockets.hpp"
#include "platform/ctr/system.h"

using namespace devilution;

bool shouldDisableBacklight;

aptHookCookie cookie;

void aptHookFunc(APT_HookType hookType, void *param)
{
	switch (hookType) {
	case APTHOOK_ONSUSPEND:
		ctr_lcd_backlight_on();
		break;
	case APTHOOK_ONSLEEP:
		break;
	case APTHOOK_ONRESTORE:
		ctr_lcd_backlight_off();
		break;
	case APTHOOK_ONWAKEUP:
		ctr_lcd_backlight_off();
		break;
	case APTHOOK_ONEXIT:
		ctr_lcd_backlight_on();
		break;
	default:
		break;
	}
}

void ctr_lcd_backlight_on()
{
	if (!shouldDisableBacklight)
		return;
	gspLcdInit();
	GSPLCD_PowerOnBacklight(GSPLCD_SCREEN_BOTTOM);
	gspLcdExit();
}

void ctr_lcd_backlight_off()
{
	if (!shouldDisableBacklight)
		return;
	gspLcdInit();
	GSPLCD_PowerOffBacklight(GSPLCD_SCREEN_BOTTOM);
	gspLcdExit();
}

bool ctr_check_dsp()
{
	FILE *dsp = fopen("sdmc:/3ds/dspfirm.cdc", "r");
	if (dsp == NULL) {
		gfxInitDefault();
		errorConf error;
		errorInit(&error, ERROR_TEXT, CFG_LANGUAGE_EN);
		errorText(&error, "Cannot find DSP firmware!\n\n\"sdmc:/3ds/dspfirm.cdc\"\n\nRun \'DSP1\' at least once to\ndump your DSP firmware.");
		errorDisp(&error);
		gfxExit();
		return false;
	}
	fclose(dsp);
	return true;
}

bool ctr_is_n3ds()
{
	bool isN3DS;
	Result res = APT_CheckNew3DS(&isN3DS);
	return R_SUCCEEDED(res) && isN3DS;
}

bool ctr_should_disable_backlight()
{
	n3ds::CFGUService cfguService;
	if (!cfguService.IsInitialized())
		return false;

	u8 model;
	Result res = CFGU_GetSystemModel(&model);
	if (!R_SUCCEEDED(res))
		return false;

	return model != CFG_MODEL_2DS;
}

void ctr_sys_init()
{
	if (ctr_check_dsp() == false)
		exit(0);

	aptHook(&cookie, aptHookFunc, NULL);

	if (ctr_is_n3ds())
		osSetSpeedupEnable(true);

	shouldDisableBacklight = ctr_should_disable_backlight();

	ctr_lcd_backlight_off();
	atexit([]() { ctr_lcd_backlight_on(); });

	romfsInit();
	atexit([]() { romfsExit(); });

	acInit();
	atexit([]() { acExit(); });

	n3ds_socInit();
	atexit([]() { n3ds_socExit(); });

#ifdef PACKET_ENCRYPTION
	randombytes_ctrrandom_init();
	atexit([]() {
		if (psGetSessionHandle())
			psExit();
	});
#endif
}
