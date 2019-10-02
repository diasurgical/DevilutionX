#include "selconn.h"

#include "devilution.h"
#include "DiabloUI/diabloui.h"
#include "DiabloUI/text.h"

namespace dvl {

char selconn_MaxPlayers[21];
char selconn_Description[64];
char selconn_Gateway[129];
bool selconn_ReturnValue = false;
bool selconn_EndMenu = false;
_SNETPROGRAMDATA *selconn_ClientInfo;
_SNETPLAYERDATA *selconn_UserInfo;
_SNETUIDATA *selconn_UiInfo;
_SNETVERSIONDATA *selconn_FileInfo;

DWORD provider;

UiArtText SELCONNECT_DIALOG_DESCRIPTION(selconn_Description, { 35, 275, 205, 66 });
UiListItem SELCONN_DIALOG_ITEMS[] = {
#ifndef NONET
	{ "Client-Server (TCP)", 0 },
	{ "Peer-to-Peer (UDP)", 1 },
	{ "Loopback", 2 },
#else
	{ "Loopback", 0 },
#endif
};
UiItem SELCONNECT_DIALOG[] = {
	UiImage(&ArtBackground, { 0, 0, 640, 480 }),
	UiArtText("Multi Player Game", { 24, 161, 590, 35 }, UIS_CENTER | UIS_BIG),
	UiArtText(selconn_MaxPlayers, { 35, 218, 205, 21 }),
	UiArtText("Requirements:", { 35, 256, 205, 21 }),
	SELCONNECT_DIALOG_DESCRIPTION,
	UiArtText("no gateway needed", { 30, 356, 220, 31 }, UIS_CENTER | UIS_MED),
	UiArtText(selconn_Gateway, { 35, 393, 205, 21 }, UIS_CENTER),
	UiArtText("Select Connection", { 300, 211, 295, 33 }, UIS_CENTER | UIS_BIG),
	UiArtTextButton("Change Gateway", nullptr, { 16, 427, 250, 35 }, UIS_CENTER | UIS_VCENTER | UIS_BIG | UIS_GOLD | UIS_HIDDEN),
	UiList(SELCONN_DIALOG_ITEMS, 305, 256, 285, 26, UIS_CENTER | UIS_VCENTER | UIS_GOLD),
	UiArtTextButton("OK", &UiFocusNavigationSelect, { 299, 427, 140, 35 }, UIS_CENTER | UIS_VCENTER | UIS_BIG | UIS_GOLD),
	UiArtTextButton("Cancel", &UiFocusNavigationEsc, { 454, 427, 140, 35 }, UIS_CENTER | UIS_VCENTER | UIS_BIG | UIS_GOLD)
};

void selconn_Load()
{
	LoadBackgroundArt("ui_art\\selconn.pcx");
	UiInitList(0, 2, selconn_Focus, selconn_Select, selconn_Esc, SELCONNECT_DIALOG, size(SELCONNECT_DIALOG));
}

void selconn_Free()
{
	ArtBackground.Unload();
}

void selconn_Esc()
{
	selconn_ReturnValue = false;
	selconn_EndMenu = true;
}

void selconn_Focus(int value)
{
	int players = MAX_PLRS;
	switch (value) {
	case 0:
		strcpy(selconn_Description, "All computers must be connected to a TCP-compatible network.");
		players = MAX_PLRS;
		break;
	case 1:
		strcpy(selconn_Description, "All computers must be connected to a UDP-compatible network.");
		players = MAX_PLRS;
		break;
	case 2:
		strcpy(selconn_Description, "Play by yourself with no network exposure.");
		players = 1;
		break;
	}

	sprintf(selconn_MaxPlayers, "Players Supported: %d", players);
	WordWrapArtStr(selconn_Description, SELCONNECT_DIALOG_DESCRIPTION.rect.w);
}

void selconn_Select(int value)
{
	switch (value) {
	case 0:
		provider = 'TCPN';
		break;
	case 1:
		provider = 'UDPN';
		break;
	case 2:
		provider = 'SCBL';
		break;
	}

	selconn_Free();
	selconn_EndMenu = SNetInitializeProvider(provider, selconn_ClientInfo, selconn_UserInfo, selconn_UiInfo, selconn_FileInfo);
	selconn_Load();
}

int UiSelectProvider(
    int a1,
    _SNETPROGRAMDATA *client_info,
    _SNETPLAYERDATA *user_info,
    _SNETUIDATA *ui_info,
    _SNETVERSIONDATA *file_info,
    int *type)
{
	selconn_ClientInfo = client_info;
	selconn_UserInfo = user_info;
	selconn_UiInfo = ui_info;
	selconn_FileInfo = file_info;
	selconn_Load();

	selconn_ReturnValue = true;
	selconn_EndMenu = false;
	while (!selconn_EndMenu) {
		UiPollAndRender();
	}
	BlackPalette();
	selconn_Free();

	return selconn_ReturnValue;
}

}
