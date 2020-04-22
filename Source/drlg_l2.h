/**
 * @file drlg_l2.h
 *
 * Interface of the catacombs level generation algorithms.
 */
#ifndef __DRLG_L2_H__
#define __DRLG_L2_H__

DEVILUTION_BEGIN_NAMESPACE

#ifdef __cplusplus
extern "C" {
#endif

extern int nSx1;
extern int nSy1;
extern int nSx2;
extern int nSy2;
extern int nRoomCnt;
extern BYTE predungeon[DMAXX][DMAXY];
extern ROOMNODE RoomList[81];
extern HALLNODE *pHallList;

void InitDungeon();
void L2LockoutFix();
void L2DoorFix();
void LoadL2Dungeon(char *sFileName, int vx, int vy);
void LoadPreL2Dungeon(char *sFileName, int vx, int vy);
void CreateL2Dungeon(DWORD rseed, int entry);

/* rdata */
extern int Area_Min;
extern int Room_Max;
extern int Room_Min;
extern int Dir_Xadd[5];
extern int Dir_Yadd[5];
extern ShadowStruct SPATSL2[2];
//short word_48489A;
extern BYTE BTYPESL2[161];
extern BYTE BSTYPESL2[161];
extern BYTE VARCH1[];
extern BYTE VARCH2[];
extern BYTE VARCH3[];
extern BYTE VARCH4[];
extern BYTE VARCH5[];
extern BYTE VARCH6[];
extern BYTE VARCH7[];
extern BYTE VARCH8[];
extern BYTE VARCH9[];
extern BYTE VARCH10[];
extern BYTE VARCH11[];
extern BYTE VARCH12[];
extern BYTE VARCH13[];
extern BYTE VARCH14[];
extern BYTE VARCH15[];
extern BYTE VARCH16[];
extern BYTE VARCH17[];
extern BYTE VARCH18[];
extern BYTE VARCH19[];
extern BYTE VARCH20[];
extern BYTE VARCH21[];
extern BYTE VARCH22[];
extern BYTE VARCH23[];
extern BYTE VARCH24[];
extern BYTE VARCH25[];
extern BYTE VARCH26[];
extern BYTE VARCH27[];
extern BYTE VARCH28[];
extern BYTE VARCH29[];
extern BYTE VARCH30[];
extern BYTE VARCH31[];
extern BYTE VARCH32[];
extern BYTE VARCH33[];
extern BYTE VARCH34[];
extern BYTE VARCH35[];
extern BYTE VARCH36[];
extern BYTE VARCH37[];
extern BYTE VARCH38[];
extern BYTE VARCH39[];
extern BYTE VARCH40[];
extern BYTE HARCH1[];
extern BYTE HARCH2[];
extern BYTE HARCH3[];
extern BYTE HARCH4[];
extern BYTE HARCH5[];
extern BYTE HARCH6[];
extern BYTE HARCH7[];
extern BYTE HARCH8[];
extern BYTE HARCH9[];
extern BYTE HARCH10[];
extern BYTE HARCH11[];
extern BYTE HARCH12[];
extern BYTE HARCH13[];
extern BYTE HARCH14[];
extern BYTE HARCH15[];
extern BYTE HARCH16[];
extern BYTE HARCH17[];
extern BYTE HARCH18[];
extern BYTE HARCH19[];
extern BYTE HARCH20[];
extern BYTE HARCH21[];
extern BYTE HARCH22[];
extern BYTE HARCH23[];
extern BYTE HARCH24[];
extern BYTE HARCH25[];
extern BYTE HARCH26[];
extern BYTE HARCH27[];
extern BYTE HARCH28[];
extern BYTE HARCH29[];
extern BYTE HARCH30[];
extern BYTE HARCH31[];
extern BYTE HARCH32[];
extern BYTE HARCH33[];
extern BYTE HARCH34[];
extern BYTE HARCH35[];
extern BYTE HARCH36[];
extern BYTE HARCH37[];
extern BYTE HARCH38[];
extern BYTE HARCH39[];
extern BYTE HARCH40[];
extern BYTE USTAIRS[];
extern BYTE DSTAIRS[];
extern BYTE WARPSTAIRS[];
extern BYTE CRUSHCOL[];
extern BYTE BIG1[];
extern BYTE BIG2[];
extern BYTE BIG3[];
extern BYTE BIG4[];
extern BYTE BIG5[];
extern BYTE BIG6[];
extern BYTE BIG7[];
extern BYTE BIG8[];
extern BYTE BIG9[];
extern BYTE BIG10[];
extern BYTE RUINS1[];
extern BYTE RUINS2[];
extern BYTE RUINS3[];
extern BYTE RUINS4[];
extern BYTE RUINS5[];
extern BYTE RUINS6[];
extern BYTE RUINS7[];
extern BYTE PANCREAS1[];
extern BYTE PANCREAS2[];
extern BYTE CTRDOOR1[];
extern BYTE CTRDOOR2[];
extern BYTE CTRDOOR3[];
extern BYTE CTRDOOR4[];
extern BYTE CTRDOOR5[];
extern BYTE CTRDOOR6[];
extern BYTE CTRDOOR7[];
extern BYTE CTRDOOR8[];
extern int Patterns[100][10];

#ifdef __cplusplus
}
#endif

DEVILUTION_END_NAMESPACE

#endif /* __DRLG_L2_H__ */
