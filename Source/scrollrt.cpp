#include "diablo.h"

DEVILUTION_BEGIN_NAMESPACE

int light_table_index;
int PitchTbl[1024];
DWORD sgdwCursWdtOld;
DWORD sgdwCursX;
DWORD sgdwCursY;
BYTE *gpBufEnd;
DWORD sgdwCursHgt;
DWORD level_cel_block;
DWORD sgdwCursXOld;
DWORD sgdwCursYOld;
char arch_draw_type;
int cel_transparency_active;
int level_piece_id;
DWORD sgdwCursWdt;
void (*DrawPlrProc)(int, int, int, int, int, BYTE *, int, int, int, int);
BYTE sgSaveBack[8192];
int draw_monster_num;
DWORD sgdwCursHgtOld;

/* data */

/* used in 1.00 debug */
char *szMonModeAssert[18] = {
	"standing",
	"walking (1)",
	"walking (2)",
	"walking (3)",
	"attacking",
	"getting hit",
	"dying",
	"attacking (special)",
	"fading in",
	"fading out",
	"attacking (ranged)",
	"standing (special)",
	"attacking (special ranged)",
	"delaying",
	"charging",
	"stoned",
	"healing",
	"talking"
};

char *szPlrModeAssert[12] = {
	"standing",
	"walking (1)",
	"walking (2)",
	"walking (3)",
	"attacking (melee)",
	"attacking (ranged)",
	"blocking",
	"getting hit",
	"dying",
	"casting a spell",
	"changing levels",
	"quitting"
};

void ClearCursor() // CODE_FIX: this was supposed to be in cursor.cpp
{
	sgdwCursWdt = 0;
	sgdwCursWdtOld = 0;
}

static void scrollrt_draw_cursor_back_buffer()
{
	int i;
	BYTE *src, *dst;

	if (sgdwCursWdt == 0) {
		return;
	}

	/// ASSERT: assert(gpBuffer);
	src = sgSaveBack;
	dst = &gpBuffer[SCREENXY(sgdwCursX, sgdwCursY)];

	for (i = sgdwCursHgt; i != 0; i--, src += sgdwCursWdt, dst += BUFFER_WIDTH) {
		memcpy(dst, src, sgdwCursWdt);
	}

	sgdwCursXOld = sgdwCursX;
	sgdwCursYOld = sgdwCursY;
	sgdwCursWdtOld = sgdwCursWdt;
	sgdwCursHgtOld = sgdwCursHgt;
	sgdwCursWdt = 0;
}

static void scrollrt_draw_cursor_item()
{
	int i, mx, my, col;
	BYTE *src, *dst;

	/// ASSERT: assert(! sgdwCursWdt);

	if (pcurs <= 0 || cursW == 0 || cursH == 0) {
		return;
	}

	mx = MouseX - 1;
	if (mx < 0) {
		mx = 0;
	} else if (mx > SCREEN_WIDTH - 1) {
		return;
	}
	my = MouseY - 1;
	if (my < 0) {
		my = 0;
	} else if (my > SCREEN_HEIGHT - 1) {
		return;
	}

	sgdwCursX = mx;
	sgdwCursWdt = sgdwCursX + cursW + 1;
	if (sgdwCursWdt > SCREEN_WIDTH - 1) {
		sgdwCursWdt = SCREEN_WIDTH - 1;
	}
	sgdwCursX &= ~3;
	sgdwCursWdt |= 3;
	sgdwCursWdt -= sgdwCursX;
	sgdwCursWdt++;

	sgdwCursY = my;
	sgdwCursHgt = sgdwCursY + cursH + 1;
	if (sgdwCursHgt > SCREEN_HEIGHT - 1) {
		sgdwCursHgt = SCREEN_HEIGHT - 1;
	}
	sgdwCursHgt -= sgdwCursY;
	sgdwCursHgt++;

	/// ASSERT: assert(sgdwCursWdt * sgdwCursHgt < sizeof sgSaveBack);
	/// ASSERT: assert(gpBuffer);
	dst = sgSaveBack;
	src = &gpBuffer[SCREENXY(sgdwCursX, sgdwCursY)];

	for (i = sgdwCursHgt; i != 0; i--, dst += sgdwCursWdt, src += BUFFER_WIDTH) {
		memcpy(dst, src, sgdwCursWdt);
	}

	mx++;
	my++;
	gpBufEnd = &gpBuffer[PitchTbl[SCREEN_HEIGHT + SCREEN_Y] - cursW - 2];

	if (pcurs >= CURSOR_FIRSTITEM) {
		col = PAL16_YELLOW + 5;
		if (plr[myplr].HoldItem._iMagical != 0) {
			col = PAL16_BLUE + 5;
		}
		if (!plr[myplr].HoldItem._iStatFlag) {
			col = PAL16_RED + 5;
		}
		CelBlitOutlineSafe(col, mx + SCREEN_X, my + cursH + SCREEN_Y - 1, pCursCels, pcurs, cursW);
		if (col != PAL16_RED + 5) {
			CelClippedDrawSafe(mx + SCREEN_X, my + cursH + SCREEN_Y - 1, pCursCels, pcurs, cursW);
		} else {
			CelDrawLightRedSafe(mx + SCREEN_X, my + cursH + SCREEN_Y - 1, pCursCels, pcurs, cursW, 0, 8, 1);
		}
	} else {
		CelClippedDrawSafe(mx + SCREEN_X, my + cursH + SCREEN_Y - 1, pCursCels, pcurs, cursW);
	}
}

void DrawMissilePrivate(MissileStruct *m, int sx, int sy, BOOL pre)
{
	int mx, my, nCel, frames;
	BYTE *pCelBuff;

	if (m->_miPreFlag != pre || !m->_miDrawFlag)
		return;

	pCelBuff = m->_miAnimData;
	if (!pCelBuff) {
		// app_fatal("Draw Missile 2 type %d: NULL Cel Buffer", m->_mitype);
		return;
	}
	nCel = m->_miAnimFrame;
	frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
	if (nCel < 1 || frames > 50 || nCel > frames) {
		// app_fatal("Draw Missile 2: frame %d of %d, missile type==%d", nCel, frames, m->_mitype);
		return;
	}
	mx = sx + m->_mixoff - m->_miAnimWidth2;
	my = sy + m->_miyoff;
	if (m->_miUniqTrans)
		Cl2DrawLightTbl(mx, my, m->_miAnimData, m->_miAnimFrame, m->_miAnimWidth, 0, 8, m->_miUniqTrans + 3);
	else if (m->_miLightFlag)
		Cl2DrawLight(mx, my, m->_miAnimData, m->_miAnimFrame, m->_miAnimWidth);
	else
		Cl2Draw(mx, my, m->_miAnimData, m->_miAnimFrame, m->_miAnimWidth);
}

void DrawMissile(int x, int y, int sx, int sy, BOOL pre)
{
	int i;
	MissileStruct *m;

	if (dMissile[x][y] != -1) {
		m = &missile[dMissile[x][y] - 1];
		DrawMissilePrivate(m, sx, sy, pre);
		return;
	}

	for (i = 0; i < nummissiles; i++) {
		/// ASSERT: assert(missileactive[i] < MAXMISSILES);
		if (missileactive[i] >= MAXMISSILES)
			break;
		m = &missile[missileactive[i]];
		if (m->_mix != x || m->_miy != y)
			continue;
		DrawMissilePrivate(m, sx, sy, pre);
	}
}

static void DrawMonster(int x, int y, int mx, int my, int m)
{
	int nCel, frames;
	char trans;
	BYTE *pCelBuff;

	if ((DWORD)m >= MAXMONSTERS) {
		// app_fatal("Draw Monster: tried to draw illegal monster %d", m);
		return;
	}

	pCelBuff = monster[m]._mAnimData;
	if (!pCelBuff) {
		// app_fatal("Draw Monster \"%s\": NULL Cel Buffer", monster[m].mName);
		return;
	}

	nCel = monster[m]._mAnimFrame;
	frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
	if (nCel < 1 || frames > 50 || nCel > frames) {
		/*
		const char *szMode = "unknown action";
		if(monster[m]._mmode <= 17)
			szMode = szMonModeAssert[monster[m]._mmode];
		app_fatal(
			"Draw Monster \"%s\" %s: facing %d, frame %d of %d",
			monster[m].mName,
			szMode,
			monster[m]._mdir,
			nCel,
			frames);
		*/
		return;
	}

	if (!(dFlags[x][y] & BFLAG_LIT)) {
		Cl2DrawLightTbl(mx, my, monster[m]._mAnimData, monster[m]._mAnimFrame, monster[m].MType->width, 0, 8, 1);
	} else {
		trans = 0;
		if (monster[m]._uniqtype)
			trans = monster[m]._uniqtrans + 4;
		if (monster[m]._mmode == MM_STONE)
			trans = 2;
		if (plr[myplr]._pInfraFlag && light_table_index > 8)
			trans = 1;
		if (trans)
			Cl2DrawLightTbl(mx, my, monster[m]._mAnimData, monster[m]._mAnimFrame, monster[m].MType->width, 0, 8, trans);
		else
			Cl2DrawLight(mx, my, monster[m]._mAnimData, monster[m]._mAnimFrame, monster[m].MType->width);
	}
}

static void DrawPlayer(int pnum, int x, int y, int px, int py, BYTE *pCelBuff, int nCel, int nWidth)
{
	int l, frames;

	if (dFlags[x][y] & BFLAG_LIT || plr[myplr]._pInfraFlag || !setlevel && !currlevel) {
		if (!pCelBuff) {
			// app_fatal("Drawing player %d \"%s\": NULL Cel Buffer", pnum, plr[pnum]._pName);
			return;
		}
		frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
		if (nCel < 1 || frames > 50 || nCel > frames) {
			/*
			const char *szMode = "unknown action";
			if(plr[pnum]._pmode <= PM_QUIT)
				szMode = szPlrModeAssert[plr[pnum]._pmode];
			app_fatal(
				"Drawing player %d \"%s\" %s: facing %d, frame %d of %d",
				pnum,
				plr[pnum]._pName,
				szMode,
				plr[pnum]._pdir,
				nCel,
				frames);
			*/
			return;
		}
		if (pnum == pcursplr)
			Cl2DrawOutline(165, px, py, pCelBuff, nCel, nWidth);
		if (pnum == myplr) {
			Cl2Draw(px, py, pCelBuff, nCel, nWidth);
			if (plr[pnum].pManaShield)
				Cl2Draw(
				    px + plr[pnum]._pAnimWidth2 - misfiledata[MFILE_MANASHLD].mAnimWidth2[0],
				    py,
				    misfiledata[MFILE_MANASHLD].mAnimData[0],
				    1,
				    misfiledata[MFILE_MANASHLD].mAnimWidth[0]);
		} else if (!(dFlags[x][y] & BFLAG_LIT) || plr[myplr]._pInfraFlag && light_table_index > 8) {
			Cl2DrawLightTbl(px, py, pCelBuff, nCel, nWidth, 0, 8, 1);
			if (plr[pnum].pManaShield)
				Cl2DrawLightTbl(
				    px + plr[pnum]._pAnimWidth2 - misfiledata[MFILE_MANASHLD].mAnimWidth2[0],
				    py,
				    misfiledata[MFILE_MANASHLD].mAnimData[0],
				    1,
				    misfiledata[MFILE_MANASHLD].mAnimWidth[0],
				    0,
				    8,
				    1);
		} else {
			l = light_table_index;
			if (light_table_index < 5)
				light_table_index = 0;
			else
				light_table_index -= 5;
			Cl2DrawLight(px, py, pCelBuff, nCel, nWidth);
			if (plr[pnum].pManaShield)
				Cl2DrawLight(
				    px + plr[pnum]._pAnimWidth2 - misfiledata[MFILE_MANASHLD].mAnimWidth2[0],
				    py,
				    misfiledata[MFILE_MANASHLD].mAnimData[0],
				    1,
				    misfiledata[MFILE_MANASHLD].mAnimWidth[0],
				    0,
				    8);
			light_table_index = l;
		}
	}
}

void DrawDeadPlayer(int x, int y, int sx, int sy)
{
	int i, px, py, nCel, frames;
	PlayerStruct *p;
	BYTE *pCelBuff;

	dFlags[x][y] &= ~BFLAG_DEAD_PLAYER;

	for (i = 0; i < MAX_PLRS; i++) {
		p = &plr[i];
		if (p->plractive && !p->_pHitPoints && p->plrlevel == (BYTE)currlevel && p->WorldX == x && p->WorldY == y) {
			pCelBuff = p->_pAnimData;
			if (!pCelBuff) {
				// app_fatal("Drawing dead player %d \"%s\": NULL Cel Buffer", i, p->_pName);
				break;
			}
			nCel = p->_pAnimFrame;
			frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
			if (nCel < 1 || frames > 50 || nCel > frames) {
				// app_fatal("Drawing dead player %d \"%s\": facing %d, frame %d of %d", i, p->_pName, p->_pdir, nCel, frame);
				break;
			}
			dFlags[x][y] |= BFLAG_DEAD_PLAYER;
			px = sx + p->_pxoff - p->_pAnimWidth2;
			py = sy + p->_pyoff;
			DrawPlayer(i, x, y, px, py, p->_pAnimData, p->_pAnimFrame, p->_pAnimWidth);
		}
	}
}

static void DrawObject(int x, int y, int ox, int oy, BOOL pre)
{
	int sx, sy, xx, yy, nCel, frames;
	char bv;
	BYTE *pCelBuff;

	if (dObject[x][y] > 0) {
		bv = dObject[x][y] - 1;
		if (object[bv]._oPreFlag != pre)
			return;
		sx = ox - object[bv]._oAnimWidth2;
		sy = oy;
	} else {
		bv = -(dObject[x][y] + 1);
		if (object[bv]._oPreFlag != pre)
			return;
		xx = object[bv]._ox - x;
		yy = object[bv]._oy - y;
		sx = (xx << 5) + ox - object[bv]._oAnimWidth2 - (yy << 5);
		sy = oy + (yy << 4) + (xx << 4);
	}

	/// ASSERT: assert((unsigned char)bv < MAXOBJECTS);
	if ((BYTE)bv >= MAXOBJECTS)
		return;

	pCelBuff = object[bv]._oAnimData;
	if (!pCelBuff) {
		// app_fatal("Draw Object type %d: NULL Cel Buffer", object[bv]._otype);
		return;
	}

	nCel = object[bv]._oAnimFrame;
	frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
	if (nCel < 1 || frames > 50 || nCel > (int)frames) {
		// app_fatal("Draw Object: frame %d of %d, object type==%d", nCel, frames, object[bv]._otype);
		return;
	}

	if (bv == pcursobj)
		CelBlitOutline(194, sx, sy, object[bv]._oAnimData, object[bv]._oAnimFrame, object[bv]._oAnimWidth);
	if (object[bv]._oLight) {
		CelClippedDrawLight(sx, sy, object[bv]._oAnimData, object[bv]._oAnimFrame, object[bv]._oAnimWidth);
	} else {
		/// ASSERT: assert(object[bv]._oAnimData);
		if (object[bv]._oAnimData) // BUGFIX: _oAnimData was already checked, this is redundant
			CelClippedDraw(sx, sy, object[bv]._oAnimData, object[bv]._oAnimFrame, object[bv]._oAnimWidth);
	}
}

static void scrollrt_draw_dungeon(BYTE *pBuff, int sx, int sy, int dx, int dy, int eflag);

static void scrollrt_draw_e_flag(BYTE *pBuff, int x, int y, int sx, int sy)
{
	int i, lti_old, cta_old, lpi_old;
	BYTE *dst;
	MICROS *pMap;

	lti_old = light_table_index;
	cta_old = cel_transparency_active;
	lpi_old = level_piece_id;

	level_piece_id = dPiece[x][y];
	light_table_index = dLight[x][y];
	dst = pBuff;
	cel_transparency_active = (BYTE)(nTransTable[level_piece_id] & TransList[dTransVal[x][y]]);
	pMap = &dpiece_defs_map_1[IsometricCoord(x, y)];

	arch_draw_type = 1;
	level_cel_block = pMap->mt[0];
	if (level_cel_block != 0) {
		drawUpperScreen(dst);
	}
	arch_draw_type = 2;
	level_cel_block = pMap->mt[1];
	if (level_cel_block != 0) {
		drawUpperScreen(dst + 32);
	}

	arch_draw_type = 0;
	for (i = 2; i < MicroTileLen; i += 2) {
		dst -= BUFFER_WIDTH * 32;
		level_cel_block = pMap->mt[i];
		if (level_cel_block != 0) {
			drawLowerScreen(dst);
		}
		level_cel_block = pMap->mt[i + 1];
		if (level_cel_block != 0) {
			drawLowerScreen(dst + 32);
		}
	}

	scrollrt_draw_dungeon(pBuff, x, y, sx, sy, 0);

	light_table_index = lti_old;
	cel_transparency_active = cta_old;
	level_piece_id = lpi_old;
}

static void scrollrt_draw_dungeon(BYTE *pBuff, int sx, int sy, int dx, int dy, int eflag)
{
	int px, py, nCel, nMon, negMon, p, tx, ty, frames;
	char bFlag, bDead, bObj, bItem, bPlr, bArch, bMap, negPlr, dd;
	DeadStruct *pDeadGuy;
	ItemStruct *pItem;
	PlayerStruct *pPlayer;
	MonsterStruct *pMonster;
	BYTE *pCelBuff;

	/// ASSERT: assert((DWORD)sx < MAXDUNX);
	/// ASSERT: assert((DWORD)sy < MAXDUNY);
	bFlag = dFlags[sx][sy];
	bDead = dDead[sx][sy];
	bObj = dObject[sx][sy];
	bItem = dItem[sx][sy];
	bPlr = dPlayer[sx][sy];
	bArch = dArch[sx][sy];
	bMap = dTransVal[sx][sy];
	nMon = dMonster[sx][sy];

	/// ASSERT: assert((DWORD)(sy-1) < MAXDUNY);
	negPlr = dPlayer[sx][sy - 1];
	negMon = dMonster[sx][sy - 1];

	if (visiondebug && bFlag & BFLAG_LIT) {
		CelClippedBlit(pBuff, pSquareCel, 1, 64);
	}
	tx = dx - 96;
	ty = dy - 16;

	if (MissilePreFlag && bFlag & BFLAG_MISSILE) {
		DrawMissile(sx, sy, dx, dy, 1);
	}

	if (light_table_index < lightmax) {
		if (bDead != 0) {
			pDeadGuy = &dead[(bDead & 0x1F) - 1];
			dd = (bDead >> 5) & 7;
			px = dx - pDeadGuy->_deadWidth2;
			pCelBuff = pDeadGuy->_deadData[dd];
			/// ASSERT: assert(pDeadGuy->_deadData[dd] != NULL);
			if (pCelBuff != NULL) {
				frames = SDL_SwapLE32(*(DWORD *)pDeadGuy->_deadData[dd]);
				nCel = pDeadGuy->_deadFrame;
				if (nCel >= 1 && frames <= 50 && nCel <= frames) {
					if (pDeadGuy->_deadtrans != 0) {
						Cl2DrawLightTbl(px, dy, pCelBuff, nCel, pDeadGuy->_deadWidth, 0, 8, pDeadGuy->_deadtrans);
					} else {
						Cl2DrawLight(px, dy, pCelBuff, pDeadGuy->_deadFrame, pDeadGuy->_deadWidth);
					}
				} else {
					// app_fatal("Unclipped dead: frame %d of %d, deadnum==%d", nCel, frames, (bDead & 0x1F) - 1);
				}
			}
		}
		if (bObj != 0) {
			DrawObject(sx, sy, dx, dy, 1);
		}
	}
	if (bItem != 0) {
		pItem = &item[bItem - 1];
		if (!pItem->_iPostDraw) {
			/// ASSERT: assert((unsigned char)bItem <= MAXITEMS);
			if ((BYTE)bItem <= MAXITEMS) {
				pCelBuff = pItem->_iAnimData;
				if (pCelBuff != NULL) {
					frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
					nCel = pItem->_iAnimFrame;
					if (nCel >= 1 && frames <= 50 && nCel <= frames) {
						px = dx - pItem->_iAnimWidth2;
						if (bItem - 1 == pcursitem) {
							CelBlitOutline(181, px, dy, pCelBuff, nCel, pItem->_iAnimWidth);
						}
						CelClippedDrawLight(px, dy, pItem->_iAnimData, pItem->_iAnimFrame, pItem->_iAnimWidth);
					} else {
						// app_fatal("Draw \"%s\" Item 1: frame %d of %d, item type==%d", pItem->_iIName, nCel, frames, pItem->_itype);
					}
				} else {
					// app_fatal("Draw Item \"%s\" 1: NULL Cel Buffer", pItem->_iIName);
				}
			}
		}
	}
	if (bFlag & BFLAG_PLAYERLR) {
		p = -(negPlr + 1);
		if ((DWORD)p < MAX_PLRS) {
			pPlayer = &plr[p];
			px = dx + pPlayer->_pxoff - pPlayer->_pAnimWidth2;
			py = dy + pPlayer->_pyoff;
			DrawPlayer(p, sx, sy - 1, px, py, pPlayer->_pAnimData, pPlayer->_pAnimFrame, pPlayer->_pAnimWidth);
			if (eflag && pPlayer->_peflag != 0) {
				if (pPlayer->_peflag == 2) {
					scrollrt_draw_e_flag(pBuff - (BUFFER_WIDTH * 16 + 96), sx - 2, sy + 1, tx, ty);
				}
				scrollrt_draw_e_flag(pBuff - 64, sx - 1, sy + 1, dx - 64, dy);
			}
		} else {
			// app_fatal("draw player: tried to draw illegal player %d", p);
		}
	}
	if (bFlag & BFLAG_MONSTLR && (bFlag & BFLAG_LIT || plr[myplr]._pInfraFlag) && negMon < 0) {
		draw_monster_num = -(negMon + 1);
		if ((DWORD)draw_monster_num < MAXMONSTERS) {
			pMonster = &monster[draw_monster_num];
			if (!(pMonster->_mFlags & MFLAG_HIDDEN)) {
				if (pMonster->MType != NULL) {
					px = dx + pMonster->_mxoff - pMonster->MType->width2;
					py = dy + pMonster->_myoff;
					if (draw_monster_num == pcursmonst) {
						Cl2DrawOutline(233, px, py, pMonster->_mAnimData, pMonster->_mAnimFrame, pMonster->MType->width);
					}
					DrawMonster(sx, sy, px, py, draw_monster_num);
					if (eflag && !pMonster->_meflag) {
						scrollrt_draw_e_flag(pBuff - 64, sx - 1, sy + 1, dx - 64, dy);
					}
				} else {
					// app_fatal("Draw Monster \"%s\": uninitialized monster", pMonster->mName);
				}
			}
		} else {
			// app_fatal("Draw Monster: tried to draw illegal monster %d", draw_monster_num);
		}
	}
	if (bFlag & BFLAG_DEAD_PLAYER) {
		DrawDeadPlayer(sx, sy, dx, dy);
	}
	if (bPlr > 0) {
		p = bPlr - 1;
		if ((DWORD)p < MAX_PLRS) {
			pPlayer = &plr[p];
			px = dx + pPlayer->_pxoff - pPlayer->_pAnimWidth2;
			py = dy + pPlayer->_pyoff;
			DrawPlayer(p, sx, sy, px, py, pPlayer->_pAnimData, pPlayer->_pAnimFrame, pPlayer->_pAnimWidth);
			if (eflag && pPlayer->_peflag != 0) {
				if (pPlayer->_peflag == 2) {
					scrollrt_draw_e_flag(pBuff - (BUFFER_WIDTH * 16 + 96), sx - 2, sy + 1, dx - 96, dy - 16);
				}
				scrollrt_draw_e_flag(pBuff - 64, sx - 1, sy + 1, dx - 64, dy);
			}
		} else {
			// app_fatal("draw player: tried to draw illegal player %d", p);
		}
	}
	if (nMon > 0 && (bFlag & BFLAG_LIT || plr[myplr]._pInfraFlag)) {
		draw_monster_num = nMon - 1;
		if ((DWORD)draw_monster_num < MAXMONSTERS) {
			pMonster = &monster[draw_monster_num];
			if (!(pMonster->_mFlags & MFLAG_HIDDEN)) {
				if (pMonster->MType != NULL) {
					px = dx + pMonster->_mxoff - pMonster->MType->width2;
					py = dy + pMonster->_myoff;
					if (draw_monster_num == pcursmonst) {
						Cl2DrawOutline(233, px, py, pMonster->_mAnimData, pMonster->_mAnimFrame, pMonster->MType->width);
					}
					DrawMonster(sx, sy, px, py, draw_monster_num);
					if (eflag && !pMonster->_meflag) {
						scrollrt_draw_e_flag(pBuff - 64, sx - 1, sy + 1, dx - 64, dy);
					}
				} else {
					// app_fatal("Draw Monster \"%s\": uninitialized monster", pMonster->mName);
				}
			}
		} else {
			// app_fatal("Draw Monster: tried to draw illegal monster %d", draw_monster_num);
		}
	}
	if (bFlag & BFLAG_MISSILE) {
		DrawMissile(sx, sy, dx, dy, 0);
	}
	if (bObj != 0 && light_table_index < lightmax) {
		DrawObject(sx, sy, dx, dy, 0);
	}
	if (bItem != 0) {
		pItem = &item[bItem - 1];
		if (pItem->_iPostDraw) {
			/// ASSERT: assert((unsigned char)bItem <= MAXITEMS);
			if ((BYTE)bItem <= MAXITEMS) {
				pCelBuff = pItem->_iAnimData;
				if (pCelBuff != NULL) {
					frames = SDL_SwapLE32(*(DWORD *)pCelBuff);
					nCel = pItem->_iAnimFrame;
					if (nCel >= 1 && frames <= 50 && nCel <= frames) {
						px = dx - pItem->_iAnimWidth2;
						if (bItem - 1 == pcursitem) {
							CelBlitOutline(181, px, dy, pCelBuff, nCel, pItem->_iAnimWidth);
						}
						CelClippedDrawLight(px, dy, pItem->_iAnimData, pItem->_iAnimFrame, pItem->_iAnimWidth);
					} else {
						// app_fatal("Draw \"%s\" Item 2: frame %d of %d, item type==%d", pItem->_iIName, nCel, frames, pItem->_itype);
					}
				} else {
					// app_fatal("Draw Item \"%s\" 2: NULL Cel Buffer", pItem->_iIName);
				}
			}
		}
	}
	if (bArch != 0) {
		cel_transparency_active = TransList[bMap];
		CelClippedBlitLightTrans(pBuff, pSpecialCels, bArch, 64);
	}
}

static void scrollrt_draw(int x, int y, int sx, int sy, int chunks, int capChunks, int eflag)
{
	int i, j;
	BYTE *dst;
	MICROS *pMap;

	/// ASSERT: assert(gpBuffer);

	pMap = &dpiece_defs_map_1[IsometricCoord(x, y)];

	if (eflag) {
		if (y >= 0 && y < MAXDUNY && x >= 0 && x < MAXDUNX) {
			level_piece_id = dPiece[x][y];
			light_table_index = dLight[x][y];
			if (level_piece_id != 0) {
				dst = &gpBuffer[sx + 32 + PitchTbl[sy]];
				cel_transparency_active = (BYTE)(nTransTable[level_piece_id] & TransList[dTransVal[x][y]]);
				level_cel_block = pMap->mt[1];
				if (level_cel_block != 0) {
					arch_draw_type = 2;
					drawUpperScreen(dst);
					arch_draw_type = 0;
				}
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[3];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[5];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[7];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[9];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[11];
				if (level_cel_block != 0 && leveltype == DTYPE_HELL) {
					drawUpperScreen(dst);
				}
				scrollrt_draw_dungeon(&gpBuffer[sx + PitchTbl[sy]], x, y, sx, sy, 0);
			} else {
				world_draw_black_tile(&gpBuffer[sx + PitchTbl[sy]]);
			}
		}
		x++;
		y--;
		sx += 64;
		chunks--;
		pMap++;
	}

	for (j = 0; j < chunks; j++) {
		if (y >= 0 && y < MAXDUNY && x >= 0 && x < MAXDUNX) {
			level_piece_id = dPiece[x][y];
			light_table_index = dLight[x][y];
			if (level_piece_id != 0) {
				dst = &gpBuffer[sx + PitchTbl[sy]];
				cel_transparency_active = (BYTE)(nTransTable[level_piece_id] & TransList[dTransVal[x][y]]);
				arch_draw_type = 1;
				level_cel_block = pMap->mt[0];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				arch_draw_type = 2;
				level_cel_block = pMap->mt[1];
				if (level_cel_block != 0) {
					drawUpperScreen(dst + 32);
				}
				arch_draw_type = 0;
				for (i = 1; i < (MicroTileLen >> 1) - 1; i++) {
					dst -= BUFFER_WIDTH * 32;
					level_cel_block = pMap->mt[2 * i];
					if (level_cel_block != 0) {
						drawUpperScreen(dst);
					}
					level_cel_block = pMap->mt[2 * i + 1];
					if (level_cel_block != 0) {
						drawUpperScreen(dst + 32);
					}
				}
				scrollrt_draw_dungeon(&gpBuffer[sx + PitchTbl[sy]], x, y, sx, sy, 1);
			} else {
				world_draw_black_tile(&gpBuffer[sx + PitchTbl[sy]]);
			}
		}
		x++;
		y--;
		sx += 64;
		pMap++;
	}

	if (eflag) {
		if (y >= 0 && y < MAXDUNY && x >= 0 && x < MAXDUNX) {
			level_piece_id = dPiece[x][y];
			light_table_index = dLight[x][y];
			if (level_piece_id != 0) {
				dst = &gpBuffer[sx + PitchTbl[sy]];
				cel_transparency_active = (BYTE)(nTransTable[level_piece_id] & TransList[dTransVal[x][y]]);
				arch_draw_type = 1;
				level_cel_block = pMap->mt[0];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				arch_draw_type = 0;
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[2];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[4];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[6];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				// TODO debug thease to see if they are even on screen
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[8];
				if (level_cel_block != 0) {
					drawUpperScreen(dst);
				}
				dst -= BUFFER_WIDTH * 32;
				level_cel_block = pMap->mt[10];
				if (level_cel_block != 0 && leveltype == DTYPE_HELL) {
					drawUpperScreen(dst);
				}
				scrollrt_draw_dungeon(&gpBuffer[sx + PitchTbl[sy]], x, y, sx, sy, 0);
			} else {
				world_draw_black_tile(&gpBuffer[sx + PitchTbl[sy]]);
			}
		}
	}
}

static void DrawGame(int x, int y)
{
	int i, sx, sy, chunks, blocks;
	int wdt, nSrcOff, nDstOff;

	sx = ScrollInfo._sxoff + 64;
	if (zoomflag) {
		sy = ScrollInfo._syoff + 175;

		chunks = ceil(SCREEN_WIDTH / 64);
		// Fill screen + keep evaulating untill MicroTiles can't affect screen
		blocks = ceil(VIEWPORT_HEIGHT / 32) + ceil(MicroTileLen / 2);
	} else {
		sy = ScrollInfo._syoff + 143;

		chunks = ceil(SCREEN_WIDTH / 2 / 64) + 1; // TODO why +1?
		// Fill screen + keep evaulating untill MicroTiles can't affect screen
		blocks = ceil(VIEWPORT_HEIGHT / 2 / 32) + ceil(MicroTileLen / 2);
	}

	// Center screen
	x -= chunks;
	y--;

	if (zoomflag && SCREEN_WIDTH == PANEL_WIDTH && SCREEN_HEIGHT == VIEWPORT_HEIGHT + PANEL_HEIGHT) {
		if (chrflag || questlog) {
			x += 2;
			y -= 2;
			sx += 288;
			chunks = 6;
		}
		if (invflag || sbookflag) {
			x += 2;
			y -= 2;
			sx -= 32;
			chunks = 6;
		}
	}

	switch (ScrollInfo._sdir) {
	case SDIR_NONE:
		break;
	case SDIR_NE:
		chunks++;
	case SDIR_N:
		sy -= 32;
		x--;
		y--;
		blocks++;
		break;
	case SDIR_SE:
		blocks++;
	case SDIR_E:
		chunks++;
		break;
	case SDIR_S:
		blocks++;
		break;
	case SDIR_SW:
		blocks++;
	case SDIR_W:
		sx -= 64;
		x--;
		y++;
		chunks++;
		break;
	case SDIR_NW:
		sx -= 64;
		sy -= 32;
		x -= 2;
		chunks++;
		blocks++;
		break;
	}

	/// ASSERT: assert(gpBuffer);
	gpBufEnd = &gpBuffer[PitchTbl[VIEWPORT_HEIGHT + SCREEN_Y]];
	for (i = 0; i < blocks; i++) {
		scrollrt_draw(x, y, sx, sy, chunks, i, 0);
		y++;
		sx -= 32;
		sy += 16;
		scrollrt_draw(x, y, sx, sy, chunks, i, 1);
		x++;
		sx += 32;
		sy += 16;
	}
	if (zoomflag)
		return;

	nSrcOff = SCREENXY(32, 159);
	nDstOff = SCREENXY(0, 350);
	wdt = SCREEN_WIDTH / 2;
	if (SCREEN_WIDTH == PANEL_WIDTH && SCREEN_HEIGHT == VIEWPORT_HEIGHT + PANEL_HEIGHT) {
		if (chrflag || questlog) {
			nSrcOff = SCREENXY(112, 159);
			nDstOff = SCREENXY(320, 350);
			wdt = (SCREEN_WIDTH - 320) / 2;
		} else if (invflag || sbookflag) {
			nSrcOff = SCREENXY(112, 159);
			nDstOff = SCREENXY(0, 350);
			wdt = (SCREEN_WIDTH - 320) / 2;
		}
	}

	/// ASSERT: assert(gpBuffer);

	int hgt;
	BYTE *src, *dst1, *dst2;

	src = &gpBuffer[nSrcOff];
	dst1 = &gpBuffer[nDstOff];
	dst2 = &gpBuffer[nDstOff + BUFFER_WIDTH];

	for (hgt = 176; hgt != 0; hgt--, src -= BUFFER_WIDTH + wdt, dst1 -= 2 * (BUFFER_WIDTH + wdt), dst2 -= 2 * (BUFFER_WIDTH + wdt)) {
		for (i = wdt; i != 0; i--) {
			*dst1++ = *src;
			*dst1++ = *src;
			*dst2++ = *src;
			*dst2++ = *src;
			src++;
		}
	}
}

void DrawView(int StartX, int StartY)
{
	DrawGame(StartX, StartY);
	if (automapflag) {
		DrawAutomap();
	}
	if (invflag) {
		DrawInv();
	} else if (sbookflag) {
		DrawSpellBook();
	}

	DrawDurIcon();

	if (chrflag) {
		DrawChr();
	} else if (questlog) {
		DrawQuestLog();
	} else if (plr[myplr]._pStatPts != 0 && !spselflag) {
		DrawLevelUpIcon();
	}
	if (uitemflag) {
		DrawUniqueInfo();
	}
	if (qtextflag) {
		DrawQText();
	}
	if (spselflag) {
		DrawSpellList();
	}
	if (dropGoldFlag) {
		DrawGoldSplit(dropGoldValue);
	}
	if (helpflag) {
		DrawHelp();
	}
	if (msgflag) {
		DrawDiabloMsg();
	}
	if (deathflag) {
		RedBack();
	} else if (PauseMode != 0) {
		gmenu_draw_pause();
	}

	DrawPlrMsg();
	gmenu_draw();
	doom_draw();
	DrawInfoBox();
	DrawLifeFlask();
	DrawManaFlask();
}

void ClearScreenBuffer()
{
	lock_buf(3);

	/// ASSERT: assert(gpBuffer);

	int i;
	BYTE *dst;

	dst = &gpBuffer[SCREENXY(0, 0)];

	for (i = 0; i < SCREEN_HEIGHT; i++, dst += BUFFER_WIDTH) {
		memset(dst, 0, SCREEN_WIDTH);
	}

	unlock_buf(3);
}

#ifdef _DEBUG
void ScrollView()
{
	BOOL scroll;

	if (pcurs >= CURSOR_FIRSTITEM)
		return;

	scroll = FALSE;

	if (MouseX < 20) {
		if (dmaxy - 1 <= ViewY || dminx >= ViewX) {
			if (dmaxy - 1 > ViewY) {
				ViewY++;
				scroll = TRUE;
			}
			if (dminx < ViewX) {
				ViewX--;
				scroll = TRUE;
			}
		} else {
			ViewY++;
			ViewX--;
			scroll = TRUE;
		}
	}
	if (MouseX > SCREEN_WIDTH - 20) {
		if (dmaxx - 1 <= ViewX || dminy >= ViewY) {
			if (dmaxx - 1 > ViewX) {
				ViewX++;
				scroll = TRUE;
			}
			if (dminy < ViewY) {
				ViewY--;
				scroll = TRUE;
			}
		} else {
			ViewY--;
			ViewX++;
			scroll = TRUE;
		}
	}
	if (MouseY < 20) {
		if (dminy >= ViewY || dminx >= ViewX) {
			if (dminy < ViewY) {
				ViewY--;
				scroll = TRUE;
			}
			if (dminx < ViewX) {
				ViewX--;
				scroll = TRUE;
			}
		} else {
			ViewX--;
			ViewY--;
			scroll = TRUE;
		}
	}
	if (MouseY > SCREEN_HEIGHT - 20) {
		if (dmaxy - 1 <= ViewY || dmaxx - 1 <= ViewX) {
			if (dmaxy - 1 > ViewY) {
				ViewY++;
				scroll = TRUE;
			}
			if (dmaxx - 1 > ViewX) {
				ViewX++;
				scroll = TRUE;
			}
		} else {
			ViewX++;
			ViewY++;
			scroll = TRUE;
		}
	}

	if (scroll)
		ScrollInfo._sdir = SDIR_NONE;
}

void EnableFrameCount()
{
	frameflag = frameflag == 0;
	framestart = GetTickCount();
}

static void DrawFPS()
{
	DWORD tc, frames;
	char String[12];
	HDC hdc;

	if (frameflag && gbActive) {
		frameend++;
		tc = GetTickCount();
		frames = tc - framestart;
		if (tc - framestart >= 1000) {
			framestart = tc;
			framerate = 1000 * frameend / frames;
			frameend = 0;
		}
		if (framerate > 99)
			framerate = 99;
		wsprintf(String, "%2d", framerate);
		TextOut(hdc, 0, 400, String, strlen(String));
	}
}
#endif

static void DoBlitScreen(DWORD dwX, DWORD dwY, DWORD dwWdt, DWORD dwHgt)
{
	RECT SrcRect;

	/// ASSERT: assert(! (dwX & 3));
	/// ASSERT: assert(! (dwWdt & 3));

	SrcRect.left = dwX + SCREEN_X;
	SrcRect.top = dwY + SCREEN_Y;
	SrcRect.right = SrcRect.left + dwWdt - 1;
	SrcRect.bottom = SrcRect.top + dwHgt - 1;
	/// ASSERT: assert(! gpBuffer);
	BltFast(dwX, dwY, &SrcRect);
}

static void DrawMain(int dwHgt, BOOL draw_desc, BOOL draw_hp, BOOL draw_mana, BOOL draw_sbar, BOOL draw_btn)
{
	int ysize;
	DWORD dwTicks;
	BOOL retry;

	ysize = dwHgt;

	if (!gbActive) {
		return;
	}

	/// ASSERT: assert(ysize >= 0 && ysize <= 480); // SCREEN_HEIGHT

	if (ysize > 0) {
		DoBlitScreen(0, 0, SCREEN_WIDTH, ysize);
	}
	if (ysize < SCREEN_HEIGHT) {
		if (draw_sbar) {
			DoBlitScreen(204, 357, 232, 28);
		}
		if (draw_desc) {
			DoBlitScreen(176, 398, 288, 60);
		}
		if (draw_mana) {
			DoBlitScreen(460, 352, 88, 72);
			DoBlitScreen(564, 416, 56, 56);
		}
		if (draw_hp) {
			DoBlitScreen(96, 352, 88, 72);
		}
		if (draw_btn) {
			DoBlitScreen(8, 357, 72, 119);
			DoBlitScreen(556, 357, 72, 48);
			if (gbMaxPlayers > 1) {
				DoBlitScreen(84, 443, 36, 32);
				DoBlitScreen(524, 443, 36, 32);
			}
		}
		if (sgdwCursWdtOld != 0) {
			DoBlitScreen(sgdwCursXOld, sgdwCursYOld, sgdwCursWdtOld, sgdwCursHgtOld);
		}
		if (sgdwCursWdt != 0) {
			DoBlitScreen(sgdwCursX, sgdwCursY, sgdwCursWdt, sgdwCursHgt);
		}
	}

#ifdef _DEBUG
	DrawFPS();
#endif
}

void scrollrt_draw_game_screen(BOOL draw_cursor)
{
	int hgt;

	if (drawpanflag == 255) {
		drawpanflag = 0;
		hgt = SCREEN_HEIGHT;
	} else {
		hgt = 0;
	}

	if (draw_cursor) {
		lock_buf(0);
		scrollrt_draw_cursor_item();
		unlock_buf(0);
	}

	DrawMain(hgt, 0, 0, 0, 0, 0);

	if (draw_cursor) {
		lock_buf(0);
		scrollrt_draw_cursor_back_buffer();
		unlock_buf(0);
	}
}

void DrawAndBlit()
{
	int hgt;
	BOOL ddsdesc, ctrlPan;

	if (!gbRunGame) {
		return;
	}

	if (drawpanflag == 255) {
		drawhpflag = TRUE;
		drawmanaflag = TRUE;
		drawbtnflag = TRUE;
		drawsbarflag = TRUE;
		ddsdesc = FALSE;
		ctrlPan = TRUE;
		hgt = SCREEN_HEIGHT;
	} else if (drawpanflag == 1) {
		ddsdesc = TRUE;
		ctrlPan = FALSE;
		hgt = VIEWPORT_HEIGHT;
	} else {
		return;
	}

	drawpanflag = 0;

	lock_buf(0);
	if (leveltype != DTYPE_TOWN) {
		DrawView(ViewX, ViewY);
	} else {
		T_DrawView(ViewX, ViewY);
	}
	if (ctrlPan || 1) {
		ClearCtrlPan();
	}
	if (drawhpflag || 1) {
		UpdateLifeFlask();
	}
	if (drawmanaflag || 1) {
		UpdateManaFlask();
	}
	if (drawbtnflag || 1) {
		DrawCtrlPan();
	}
	if (drawsbarflag || 1) {
		DrawInvBelt();
	}
	if (talkflag || 1) {
		DrawTalkPan();
		hgt = SCREEN_HEIGHT;
	}
	scrollrt_draw_cursor_item();
	unlock_buf(0);

	DrawMain(hgt, ddsdesc, drawhpflag, drawmanaflag, drawsbarflag, drawbtnflag);

	lock_buf(0);
	scrollrt_draw_cursor_back_buffer();
	unlock_buf(0);

	drawhpflag = FALSE;
	drawmanaflag = FALSE;
	drawbtnflag = FALSE;
	drawsbarflag = FALSE;
}

DEVILUTION_END_NAMESPACE
