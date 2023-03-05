/*
	MYOSGLUE.c

	Copyright (C) 2012 Paul C. Pratt

	You can redistribute this file and/or modify it under the terms
	of version 2 of the GNU General Public License as published by
	the Free Software Foundation.  You should have received a copy
	of the license along with this file; see the file COPYING.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	license for more details.
*/

/*
	MY Operating System GLUE. (for SDL Library)

	All operating system dependent code for the
	SDL Library should go here.
*/

#include "CNFGRAPI.h"
#include "SYSDEPNS.h"
#include "ENDIANAC.h"

#include "MYOSGLUE.h"

#include "STRCONST.h"

/* SDL2 window */
SDL_Window *window;

/* SDL2 renderer */
SDL_Renderer *renderer;

/* SDL2 texture */
SDL_Texture *texture;

/* SDL2 blitting rects for hw scaling 
 * ratio correction using SDL_RenderCopy() */
SDL_Rect src_rect;
SDL_Rect dst_rect;

/* SDL2 physical screen dimensions */
int displayWidth;
int displayHeight;

/* --- some simple utilities --- */

GLOBALPROC MyMoveBytes(anyp srcPtr, anyp destPtr, si5b byteCount)
{
	(void) memcpy((char *)destPtr, (char *)srcPtr, byteCount);
}

/* --- control mode and internationalization --- */

#define NeedCell2PlainAsciiMap 1

#include "INTLCHAR.h"

/* --- sending debugging info to file --- */

#if dbglog_HAVE

#define dbglog_ToStdErr 0

#if ! dbglog_ToStdErr
LOCALVAR FILE *dbglog_File = NULL;
#endif

LOCALFUNC blnr dbglog_open0(void)
{
#if dbglog_ToStdErr
	return trueblnr;
#else
	dbglog_File = fopen("dbglog.txt", "w");
	return (NULL != dbglog_File);
#endif
}

LOCALPROC dbglog_write0(char *s, uimr L)
{
#if dbglog_ToStdErr
	(void) fwrite(s, 1, L, stderr);
#else
	if (dbglog_File != NULL) {
		(void) fwrite(s, 1, L, dbglog_File);
	}
#endif
}

LOCALPROC dbglog_close0(void)
{
#if ! dbglog_ToStdErr
	if (dbglog_File != NULL) {
		fclose(dbglog_File);
		dbglog_File = NULL;
	}
#endif
}

#endif

/* --- information about the environment --- */

#define WantColorTransValid 0

#include "COMOSGLU.h"

#include "CONTROLM.h"

/* --- parameter buffers --- */

#if IncludePbufs
LOCALVAR void *PbufDat[NumPbufs];
#endif

#if IncludePbufs
LOCALFUNC tMacErr PbufNewFromPtr(void *p, ui5b count, tPbuf *r)
{
	tPbuf i;
	tMacErr err;

	if (! FirstFreePbuf(&i)) {
		free(p);
		err = mnvm_miscErr;
	} else {
		*r = i;
		PbufDat[i] = p;
		PbufNewNotify(i, count);
		err = mnvm_noErr;
	}

	return err;
}
#endif

#if IncludePbufs
GLOBALFUNC tMacErr PbufNew(ui5b count, tPbuf *r)
{
	tMacErr err = mnvm_miscErr;

	void *p = calloc(1, count);
	if (NULL != p) {
		err = PbufNewFromPtr(p, count, r);
	}

	return err;
}
#endif

#if IncludePbufs
GLOBALPROC PbufDispose(tPbuf i)
{
	free(PbufDat[i]);
	PbufDisposeNotify(i);
}
#endif

#if IncludePbufs
LOCALPROC UnInitPbufs(void)
{
	tPbuf i;

	for (i = 0; i < NumPbufs; ++i) {
		if (PbufIsAllocated(i)) {
			PbufDispose(i);
		}
	}
}
#endif

#if IncludePbufs
GLOBALPROC PbufTransfer(ui3p Buffer,
	tPbuf i, ui5r offset, ui5r count, blnr IsWrite)
{
	void *p = ((ui3p)PbufDat[i]) + offset;
	if (IsWrite) {
		(void) memcpy(p, Buffer, count);
	} else {
		(void) memcpy(Buffer, p, count);
	}
}
#endif

/* --- text translation --- */

LOCALPROC NativeStrFromCStr(char *r, char *s)
{
	ui3b ps[ClStrMaxLength];
	int i;
	int L;

	ClStrFromSubstCStr(&L, ps, s);

	for (i = 0; i < L; ++i) {
		r[i] = Cell2PlainAsciiMap[ps[i]];
	}

	r[L] = 0;
}

/* --- drives --- */

#define NotAfileRef NULL

LOCALVAR FILE *Drives[NumDrives]; /* open disk image files */

LOCALPROC InitDrives(void)
{
	/*
		This isn't really needed, Drives[i] and DriveNames[i]
		need not have valid values when not vSonyIsInserted[i].
	*/
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		Drives[i] = NotAfileRef;
	}
}

GLOBALFUNC tMacErr vSonyTransfer(blnr IsWrite, ui3p Buffer,
	tDrive Drive_No, ui5r Sony_Start, ui5r Sony_Count,
	ui5r *Sony_ActCount)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	ui5r NewSony_Count = 0;

	if (0 == fseek(refnum, Sony_Start, SEEK_SET)) {
		if (IsWrite) {
			NewSony_Count = fwrite(Buffer, 1, Sony_Count, refnum);
		} else {
			NewSony_Count = fread(Buffer, 1, Sony_Count, refnum);
		}

		if (NewSony_Count == Sony_Count) {
			err = mnvm_noErr;
		}
	}

	if (nullpr != Sony_ActCount) {
		*Sony_ActCount = NewSony_Count;
	}

	return err; /*& figure out what really to return &*/
}

GLOBALFUNC tMacErr vSonyGetSize(tDrive Drive_No, ui5r *Sony_Count)
{
	tMacErr err = mnvm_miscErr;
	FILE *refnum = Drives[Drive_No];
	long v;

	if (0 == fseek(refnum, 0, SEEK_END)) {
		v = ftell(refnum);
		if (v >= 0) {
			*Sony_Count = v;
			err = mnvm_noErr;
		}
	}

	return err; /*& figure out what really to return &*/
}

LOCALFUNC tMacErr vSonyEject0(tDrive Drive_No, blnr deleteit)
{
	FILE *refnum = Drives[Drive_No];

	DiskEjectedNotify(Drive_No);

	fclose(refnum);
	Drives[Drive_No] = NotAfileRef; /* not really needed */

	return mnvm_noErr;
}

GLOBALFUNC tMacErr vSonyEject(tDrive Drive_No)
{
	return vSonyEject0(Drive_No, falseblnr);
}

LOCALPROC UnInitDrives(void)
{
	tDrive i;

	for (i = 0; i < NumDrives; ++i) {
		if (vSonyIsInserted(i)) {
			(void) vSonyEject(i);
		}
	}
}

LOCALFUNC blnr Sony_Insert0(FILE *refnum, blnr locked,
	char *drivepath)
{
	tDrive Drive_No;
	blnr IsOk = falseblnr;

	if (! FirstFreeDisk(&Drive_No)) {
		MacMsg(kStrTooManyImagesTitle, kStrTooManyImagesMessage,
			falseblnr);
	} else {
		/* printf("Sony_Insert0 %d\n", (int)Drive_No); */

		{
			Drives[Drive_No] = refnum;
			DiskInsertNotify(Drive_No, locked);

			IsOk = trueblnr;
		}
	}

	if (! IsOk) {
		fclose(refnum);
	}

	return IsOk;
}

LOCALFUNC blnr Sony_Insert1(char *drivepath, blnr silentfail)
{
	blnr locked = falseblnr;
	/* printf("Sony_Insert1 %s\n", drivepath); */
	FILE *refnum = fopen(drivepath, "rb+");
	if (NULL == refnum) {
		locked = trueblnr;
		refnum = fopen(drivepath, "rb");
	}
	if (NULL == refnum) {
		if (! silentfail) {
			MacMsg(kStrOpenFailTitle, kStrOpenFailMessage, falseblnr);
		}
	} else {
		return Sony_Insert0(refnum, locked, drivepath);
	}
	return falseblnr;
}

LOCALFUNC blnr Sony_Insert2(char *s)
{
	return Sony_Insert1(s, trueblnr);
}

LOCALFUNC blnr LoadInitialImages(void)
{
	if (! AnyDiskInserted()) {
		int n = NumDrives > 9 ? 9 : NumDrives;
		int i;
		char s[] = "disk?.dsk";

		for (i = 1; i <= n; ++i) {
			s[4] = '0' + i;
			if (! Sony_Insert2(s)) {
				/* stop on first error (including file not found) */
				return trueblnr;
			}
		}
	}

	return trueblnr;
}

/* --- ROM --- */

LOCALVAR char *rom_path = NULL;

LOCALFUNC tMacErr LoadMacRomFrom(char *path)
{
	tMacErr err;
	FILE *ROM_File;
	int File_Size;

	ROM_File = fopen(path, "rb");
	if (NULL == ROM_File) {
		err = mnvm_fnfErr;
	} else {
		File_Size = fread(ROM, 1, kROM_Size, ROM_File);
		if (File_Size != kROM_Size) {
			if (feof(ROM_File)) {
				err = mnvm_eofErr;
			} else {
				err = mnvm_miscErr;
			}
		} else {
			err = mnvm_noErr;
		}
		fclose(ROM_File);
	}

	return err;
}

LOCALFUNC blnr LoadMacRom(void)
{
	tMacErr err;

	if ((NULL == rom_path)
		|| (mnvm_fnfErr == (err = LoadMacRomFrom(rom_path))))
	if (mnvm_fnfErr == (err = LoadMacRomFrom(RomFileName)))
	{
	}

	if (mnvm_noErr != err) {
		if (mnvm_fnfErr == err) {
			MacMsg(kStrNoROMTitle, kStrNoROMMessage, trueblnr);
		} else if (mnvm_eofErr == err) {
			MacMsg(kStrShortROMTitle, kStrShortROMMessage,
				trueblnr);
		} else {
			MacMsg(kStrNoReadROMTitle, kStrNoReadROMMessage,
				trueblnr);
		}
		SpeedStopped = trueblnr;
	}

	return trueblnr; /* keep launching Mini vMac, regardless */
}

/* --- video out --- */

#if VarFullScreen
LOCALVAR blnr UseFullScreen = (WantInitFullScreen != 0);
#endif

#if EnableMagnify
LOCALVAR blnr UseMagnify = (WantInitMagnify != 0);
#endif

LOCALVAR blnr gBackgroundFlag = falseblnr;
LOCALVAR blnr gTrueBackgroundFlag = falseblnr;
LOCALVAR blnr CurSpeedStopped = trueblnr;

#if EnableMagnify
#define MaxScale MyWindowScale
#else
#define MaxScale 1
#endif


LOCALVAR SDL_Surface *my_surface = nullpr;

LOCALVAR ui3p ScalingBuff = nullpr;

LOCALVAR ui3p CLUT_final;

#define CLUT_finalsz (256 * 8 * 4 * MaxScale)
	/*
		256 possible values of one byte
		8 pixels per byte maximum (when black and white)
		4 bytes per destination pixel maximum
			multiplied by MyWindowScale if EnableMagnify
	*/

#define ScrnMapr_DoMap UpdateBWDepth3Copy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 3
#define ScrnMapr_Map CLUT_final

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateBWDepth4Copy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 4
#define ScrnMapr_Map CLUT_final

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateBWDepth5Copy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 5
#define ScrnMapr_Map CLUT_final

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateBWDepth3ScaledCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 3
#define ScrnMapr_Map CLUT_final
#define ScrnMapr_Scale MyWindowScale

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateBWDepth4ScaledCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 4
#define ScrnMapr_Map CLUT_final
#define ScrnMapr_Scale MyWindowScale

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateBWDepth5ScaledCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth 0
#define ScrnMapr_DstDepth 5
#define ScrnMapr_Map CLUT_final
#define ScrnMapr_Scale MyWindowScale

#include "SCRNMAPR.h"


#if (0 != vMacScreenDepth) && (vMacScreenDepth < 4)

#define ScrnMapr_DoMap UpdateColorDepth3Copy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth vMacScreenDepth
#define ScrnMapr_DstDepth 3
#define ScrnMapr_Map CLUT_final

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateColorDepth4Copy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth vMacScreenDepth
#define ScrnMapr_DstDepth 4
#define ScrnMapr_Map CLUT_final

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateColorDepth5Copy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth vMacScreenDepth
#define ScrnMapr_DstDepth 5
#define ScrnMapr_Map CLUT_final

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateColorDepth3ScaledCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth vMacScreenDepth
#define ScrnMapr_DstDepth 3
#define ScrnMapr_Map CLUT_final
#define ScrnMapr_Scale MyWindowScale

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateColorDepth4ScaledCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth vMacScreenDepth
#define ScrnMapr_DstDepth 4
#define ScrnMapr_Map CLUT_final
#define ScrnMapr_Scale MyWindowScale

#include "SCRNMAPR.h"

#define ScrnMapr_DoMap UpdateColorDepth5ScaledCopy
#define ScrnMapr_Src GetCurDrawBuff()
#define ScrnMapr_Dst ScalingBuff
#define ScrnMapr_SrcDepth vMacScreenDepth
#define ScrnMapr_DstDepth 5
#define ScrnMapr_Map CLUT_final
#define ScrnMapr_Scale MyWindowScale

#include "SCRNMAPR.h"

#endif


LOCALPROC HaveChangedScreenBuff(ui4r top, ui4r left,
	ui4r bottom, ui4r right)
{
	int i;
	int j;
	ui3b *p;
	Uint32 pixel;
#if (0 != vMacScreenDepth) && (vMacScreenDepth < 4)
	Uint32 CLUT_pixel[CLUT_size];
#endif
	Uint32 BWLUT_pixel[2];
	ui5r top2 = top;
	ui5r left2 = left;
	ui5r bottom2 = bottom;
	ui5r right2 = right;

#if EnableMagnify
	if (UseMagnify) {
		top2 *= MyWindowScale;
		left2 *= MyWindowScale;
		bottom2 *= MyWindowScale;
		right2 *= MyWindowScale;
	}
#endif

	if (SDL_MUSTLOCK(my_surface)) {
		if (SDL_LockSurface(my_surface) < 0) {
			return;
		}
	}

	{

	int bpp = my_surface->format->BytesPerPixel;
	ui5r ExpectedPitch = vMacScreenWidth * bpp;

#if EnableMagnify
	if (UseMagnify) {
		ExpectedPitch *= MyWindowScale;
	}
#endif

#if 0 != vMacScreenDepth
	if (UseColorMode) {
#if vMacScreenDepth < 4
		for (i = 0; i < CLUT_size; ++i) {
			CLUT_pixel[i] = SDL_MapRGB(my_surface->format,
				CLUT_reds[i] >> 8,
				CLUT_greens[i] >> 8,
				CLUT_blues[i] >> 8);
		}
#endif
	} else
#endif
	{
		BWLUT_pixel[1] = SDL_MapRGB(my_surface->format, 0, 0, 0); /* black */
		BWLUT_pixel[0] = SDL_MapRGB(my_surface->format, 255, 255, 255); /* white */
	}

	if ((0 == ((bpp - 1) & bpp)) /* a power of 2 */
		&& (my_surface->pitch == ExpectedPitch)
#if (vMacScreenDepth > 3)
		&& ! UseColorMode
#endif
		)
	{
		int k;
		Uint32 v;
#if EnableMagnify
		int a;
#endif
		int PixPerByte =
#if (0 != vMacScreenDepth) && (vMacScreenDepth < 4)
			UseColorMode ? (1 << (3 - vMacScreenDepth)) :
#endif
			8;
		Uint8 *p4 = (Uint8 *)CLUT_final;

		for (i = 0; i < 256; ++i) {
			for (k = PixPerByte; --k >= 0; ) {

#if (0 != vMacScreenDepth) && (vMacScreenDepth < 4)
				if (UseColorMode) {
					v = CLUT_pixel[
#if 3 == vMacScreenDepth
						i
#else
						(i >> (k << vMacScreenDepth))
							& (CLUT_size - 1)
#endif
						];
				} else
#endif
				{
					v = BWLUT_pixel[(i >> k) & 1];
				}

#if EnableMagnify
				for (a = UseMagnify ? MyWindowScale : 1; --a >= 0; )
#endif
				{
					switch (bpp) {
						case 1: /* Assuming 8-bpp */
							*p4++ = v;
							break;
						case 2: /* Probably 15-bpp or 16-bpp */
							*(Uint16 *)p4 = v;
							p4 += 2;
							break;
						case 4: /* Probably 32-bpp */
							*(Uint32 *)p4 = v;
							p4 += 4;
							break;
					}
				}
			}
		}

		ScalingBuff = (ui3p)my_surface->pixels;

#if (0 != vMacScreenDepth) && (vMacScreenDepth < 4)
		if (UseColorMode) {
#if EnableMagnify
			if (UseMagnify) {
				switch (bpp) {
					case 1:
						UpdateColorDepth3ScaledCopy(top, left, bottom, right);
						break;
					case 2:
						UpdateColorDepth4ScaledCopy(top, left, bottom, right);
						break;
					case 4:
						UpdateColorDepth5ScaledCopy(top, left, bottom, right);
						break;
				}
			} else
#endif
			{
				switch (bpp) {
					case 1:
						UpdateColorDepth3Copy(top, left, bottom, right);
						break;
					case 2:
						UpdateColorDepth4Copy(top, left, bottom, right);
						break;
					case 4:
						UpdateColorDepth5Copy(top, left, bottom, right);
						break;
				}
			}
		} else
#endif
		{
#if EnableMagnify
			if (UseMagnify) {
				switch (bpp) {
					case 1:
						UpdateBWDepth3ScaledCopy(top, left, bottom, right);
						break;
					case 2:
						UpdateBWDepth4ScaledCopy(top, left, bottom, right);
						break;
					case 4:
						UpdateBWDepth5ScaledCopy(top, left, bottom, right);
						break;
				}
			} else
#endif
			{
				switch (bpp) {
					case 1:
						UpdateBWDepth3Copy(top, left, bottom, right);
						break;
					case 2:
						UpdateBWDepth4Copy(top, left, bottom, right);
						break;
					case 4:
						UpdateBWDepth5Copy(top, left, bottom, right);
						break;
				}
			}
		}

	} else {
		ui3b *the_data = (ui3b *)GetCurDrawBuff();

		/* adapted from putpixel in SDL documentation */

		for (i = top2; i < bottom2; ++i) {
			for (j = left2; j < right2; ++j) {
				int i0 = i;
				int j0 = j;
				Uint8 *bufp = (Uint8 *)my_surface->pixels
					+ i * my_surface->pitch + j * bpp;

#if EnableMagnify
				if (UseMagnify) {
					i0 /= MyWindowScale;
					j0 /= MyWindowScale;
				}
#endif

#if 0 != vMacScreenDepth
				if (UseColorMode) {
#if vMacScreenDepth < 4
					p = the_data + ((i0 * vMacScreenWidth + j0) >> (3 - vMacScreenDepth));
					{
						ui3r k = (*p >> (((~ j0) & ((1 << (3 - vMacScreenDepth)) - 1))
							<< vMacScreenDepth)) & (CLUT_size - 1);
						pixel = CLUT_pixel[k];
					}
#elif 4 == vMacScreenDepth
					p = the_data + ((i0 * vMacScreenWidth + j0) << 1);
					{
						ui4r t0 = do_get_mem_word(p);
						pixel = SDL_MapRGB(my_surface->format,
							((t0 & 0x7C00) >> 7) | ((t0 & 0x7000) >> 12),
							((t0 & 0x03E0) >> 2) | ((t0 & 0x0380) >> 7),
							((t0 & 0x001F) << 3) | ((t0 & 0x001C) >> 2));
					}
#elif 5 == vMacScreenDepth
					p = the_data + ((i0 * vMacScreenWidth + j0) << 2);
					pixel = SDL_MapRGB(my_surface->format,
						p[1],
						p[2],
						p[3]);
#endif
				} else
#endif
				{
					p = the_data + ((i0 * vMacScreenWidth + j0) / 8);
					pixel = BWLUT_pixel[(*p >> ((~ j0) & 0x7)) & 1];
				}

				switch (bpp) {
					case 1: /* Assuming 8-bpp */
						*bufp = pixel;
						break;
					case 2: /* Probably 15-bpp or 16-bpp */
						*(Uint16 *)bufp = pixel;
						break;
					case 3:
						/* Slow 24-bpp mode, usually not used */
						if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
							bufp[0] = (pixel >> 16) & 0xff;
							bufp[1] = (pixel >> 8) & 0xff;
							bufp[2] = pixel & 0xff;
						} else {
							bufp[0] = pixel & 0xff;
							bufp[1] = (pixel >> 8) & 0xff;
							bufp[2] = (pixel >> 16) & 0xff;
						}
						break;
					case 4: /* Probably 32-bpp */
						*(Uint32 *)bufp = pixel;
						break;
				}
			}
		}
	}

	}

	if (SDL_MUSTLOCK(my_surface)) {
		SDL_UnlockSurface(my_surface);
	}

	// SDL2 screen update block
	SDL_UpdateTexture(texture, NULL, my_surface->pixels, vMacScreenWidth * sizeof (Uint32));
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, &src_rect, &dst_rect); //&src_rect, &dst_rect
	SDL_RenderPresent(renderer);
	//
}

LOCALPROC MyDrawChangesAndClear(void)
{
	if (ScreenChangedBottom > ScreenChangedTop) {
		HaveChangedScreenBuff(ScreenChangedTop, ScreenChangedLeft,
			ScreenChangedBottom, ScreenChangedRight);
		ScreenClearChanges();
	}
}

/* --- mouse --- */

/* cursor hiding */

LOCALVAR blnr HaveCursorHidden = falseblnr;
LOCALVAR blnr WantCursorHidden = falseblnr;

LOCALPROC ForceShowCursor(void)
{
	if (HaveCursorHidden) {
		HaveCursorHidden = falseblnr;
		(void) SDL_ShowCursor(SDL_ENABLE);
	}
}

/* cursor moving */

LOCALFUNC blnr MyMoveMouse(si4b h, si4b v)
{
#if EnableMagnify
	if (UseMagnify) {
		h *= MyWindowScale;
		v *= MyWindowScale;
	}
#endif

	SDL_WarpMouseInWindow(window, h, v);

	return trueblnr;
}

/* cursor state */

LOCALPROC MousePositionNotify(int NewMousePosh, int NewMousePosv)
{
	blnr ShouldHaveCursorHidden = trueblnr;

#if EnableMagnify
	if (UseMagnify) {
		NewMousePosh /= MyWindowScale;
		NewMousePosv /= MyWindowScale;
	}
#endif

#if EnableMouseMotion && MayFullScreen
	if (HaveMouseMotion) {
		MyMousePositionSetDelta(NewMousePosh - SavedMouseH,
			NewMousePosv - SavedMouseV);
		SavedMouseH = NewMousePosh;
		SavedMouseV = NewMousePosv;
	} else
#endif
	{
		if (NewMousePosh < 0) {
			NewMousePosh = 0;
			ShouldHaveCursorHidden = falseblnr;
		} else if (NewMousePosh >= vMacScreenWidth) {
			NewMousePosh = vMacScreenWidth - 1;
			ShouldHaveCursorHidden = falseblnr;
		}
		if (NewMousePosv < 0) {
			NewMousePosv = 0;
			ShouldHaveCursorHidden = falseblnr;
		} else if (NewMousePosv >= vMacScreenHeight) {
			NewMousePosv = vMacScreenHeight - 1;
			ShouldHaveCursorHidden = falseblnr;
		}

#if VarFullScreen
		if (UseFullScreen)
#endif
#if MayFullScreen
		{
			ShouldHaveCursorHidden = trueblnr;
		}
#endif

		/* if (ShouldHaveCursorHidden || CurMouseButton) */
		/*
			for a game like arkanoid, would like mouse to still
			move even when outside window in one direction
		*/
		MyMousePositionSet(NewMousePosh, NewMousePosv);
	}

	WantCursorHidden = ShouldHaveCursorHidden;
}

LOCALPROC MousePositionNotifyRelative(int deltah, int deltav)
{
	blnr ShouldHaveCursorHidden = trueblnr;

#if EnableMagnify
	if (UseMagnify) {
		deltah /= MyWindowScale;
		deltav /= MyWindowScale;
	}
#endif
	MyMousePositionSetDelta(deltah,
		deltav);

	WantCursorHidden = ShouldHaveCursorHidden;
}

LOCALPROC CheckMouseState(void)
{
	/*
		this doesn't work as desired, doesn't get mouse movements
		when outside of our window.
	*/
	int x;
	int y;

	(void) SDL_GetMouseState(&x, &y);
	MousePositionNotify(x, y);
}

/* --- keyboard input --- */

LOCALFUNC int SDLKey2MacKeyCode(SDL_Keycode i)
{
	int v = -1;

	switch (i) {
		case SDLK_BACKSPACE: v = MKC_BackSpace; break;
		case SDLK_TAB: v = MKC_Tab; break;
		case SDLK_CLEAR: v = MKC_Clear; break;
		case SDLK_RETURN: v = MKC_Return; break;
		case SDLK_PAUSE: v = MKC_Pause; break;
		case SDLK_ESCAPE: v = MKC_Escape; break;
		case SDLK_SPACE: v = MKC_Space; break;
		case SDLK_EXCLAIM: /* ? */ break;
		case SDLK_QUOTEDBL: /* ? */ break;
		case SDLK_HASH: /* ? */ break;
		case SDLK_DOLLAR: /* ? */ break;
		case SDLK_AMPERSAND: /* ? */ break;
		case SDLK_QUOTE: v = MKC_SingleQuote; break;
		case SDLK_LEFTPAREN: /* ? */ break;
		case SDLK_RIGHTPAREN: /* ? */ break;
		case SDLK_ASTERISK: /* ? */ break;
		case SDLK_PLUS: /* ? */ break;
		case SDLK_COMMA: v = MKC_Comma; break;
		case SDLK_MINUS: v = MKC_Minus; break;
		case SDLK_PERIOD: v = MKC_Period; break;
		case SDLK_SLASH: v = MKC_Slash; break;
		case SDLK_0: v = MKC_0; break;
		case SDLK_1: v = MKC_1; break;
		case SDLK_2: v = MKC_2; break;
		case SDLK_3: v = MKC_3; break;
		case SDLK_4: v = MKC_4; break;
		case SDLK_5: v = MKC_5; break;
		case SDLK_6: v = MKC_6; break;
		case SDLK_7: v = MKC_7; break;
		case SDLK_8: v = MKC_8; break;
		case SDLK_9: v = MKC_9; break;
		case SDLK_COLON: /* ? */ break;
		case SDLK_SEMICOLON: v = MKC_SemiColon; break;
		case SDLK_LESS: /* ? */ break;
		case SDLK_EQUALS: v = MKC_Equal; break;
		case SDLK_GREATER: /* ? */ break;
		case SDLK_QUESTION: /* ? */ break;
		case SDLK_AT: /* ? */ break;

		case SDLK_LEFTBRACKET: v = MKC_LeftBracket; break;
		case SDLK_BACKSLASH: v = MKC_BackSlash; break;
		case SDLK_RIGHTBRACKET: v = MKC_RightBracket; break;
		case SDLK_CARET: /* ? */ break;
		case SDLK_UNDERSCORE: /* ? */ break;
		case SDLK_BACKQUOTE: v = MKC_Grave; break;

		case SDLK_a: v = MKC_A; break;
		case SDLK_b: v = MKC_B; break;
		case SDLK_c: v = MKC_C; break;
		case SDLK_d: v = MKC_D; break;
		case SDLK_e: v = MKC_E; break;
		case SDLK_f: v = MKC_F; break;
		case SDLK_g: v = MKC_G; break;
		case SDLK_h: v = MKC_H; break;
		case SDLK_i: v = MKC_I; break;
		case SDLK_j: v = MKC_J; break;
		case SDLK_k: v = MKC_K; break;
		case SDLK_l: v = MKC_L; break;
		case SDLK_m: v = MKC_M; break;
		case SDLK_n: v = MKC_N; break;
		case SDLK_o: v = MKC_O; break;
		case SDLK_p: v = MKC_P; break;
		case SDLK_q: v = MKC_Q; break;
		case SDLK_r: v = MKC_R; break;
		case SDLK_s: v = MKC_S; break;
		case SDLK_t: v = MKC_T; break;
		case SDLK_u: v = MKC_U; break;
		case SDLK_v: v = MKC_V; break;
		case SDLK_w: v = MKC_W; break;
		case SDLK_x: v = MKC_X; break;
		case SDLK_y: v = MKC_Y; break;
		case SDLK_z: v = MKC_Z; break;

		case SDLK_KP_0: v = MKC_KP0; break;
		case SDLK_KP_1: v = MKC_KP1; break;
		case SDLK_KP_2: v = MKC_KP2; break;
		case SDLK_KP_3: v = MKC_KP3; break;
		case SDLK_KP_4: v = MKC_KP4; break;
		case SDLK_KP_5: v = MKC_KP5; break;
		case SDLK_KP_6: v = MKC_KP6; break;
		case SDLK_KP_7: v = MKC_KP7; break;
		case SDLK_KP_8: v = MKC_KP8; break;
		case SDLK_KP_9: v = MKC_KP9; break;

		case SDLK_KP_PERIOD: v = MKC_Decimal; break;
		case SDLK_KP_DIVIDE: v = MKC_KPDevide; break;
		case SDLK_KP_MULTIPLY: v = MKC_KPMultiply; break;
		case SDLK_KP_MINUS: v = MKC_KPSubtract; break;
		case SDLK_KP_PLUS: v = MKC_KPAdd; break;
		case SDLK_KP_ENTER: v = MKC_Enter; break;
		case SDLK_KP_EQUALS: v = MKC_KPEqual; break;

		case SDLK_UP: v = MKC_Up; break;
		case SDLK_DOWN: v = MKC_Down; break;
		case SDLK_RIGHT: v = MKC_Right; break;
		case SDLK_LEFT: v = MKC_Left; break;
		case SDLK_INSERT: v = MKC_Help; break;
		case SDLK_HOME: v = MKC_Home; break;
		case SDLK_END: v = MKC_End; break;
		case SDLK_PAGEUP: v = MKC_PageUp; break;
		case SDLK_PAGEDOWN: v = MKC_PageDown; break;

		case SDLK_F1: v = MKC_F1; break;
		case SDLK_F2: v = MKC_F2; break;
		case SDLK_F3: v = MKC_F3; break;
		case SDLK_F4: v = MKC_F4; break;
		case SDLK_F5: v = MKC_F5; break;
		case SDLK_F6: v = MKC_F6; break;
		case SDLK_F7: v = MKC_F7; break;
		case SDLK_F8: v = MKC_F8; break;
		case SDLK_F9: v = MKC_F9; break;
		case SDLK_F10: v = MKC_F10; break;
		case SDLK_F11: v = MKC_F11; break;
		case SDLK_F12: v = MKC_F11; break;

		case SDLK_F13: /* ? */ break;
		case SDLK_F14: /* ? */ break;
		case SDLK_F15: /* ? */ break;

		case SDLK_NUMLOCKCLEAR: v = MKC_ForwardDel; break;
		case SDLK_CAPSLOCK: v = MKC_CapsLock; break;
		case SDLK_SCROLLLOCK: v = MKC_ScrollLock; break;
		case SDLK_RSHIFT: v = MKC_Shift; break;
		case SDLK_LSHIFT: v = MKC_Shift; break;
		case SDLK_RCTRL: v = MKC_Control; break;
		case SDLK_LCTRL: v = MKC_Control; break;
		case SDLK_RALT: v = MKC_Option; break;
		case SDLK_LALT: v = MKC_Option; break;
		case SDLK_RGUI: v = MKC_Command; break;
		case SDLK_LGUI: v = MKC_Command; break;
		//case SDLK_LSUPER: v = MKC_Option; break;
		//case SDLK_RSUPER: v = MKC_Option; break;

		case SDLK_MODE: /* ? */ break;
		//case SDLK_COMPOSE: /* ? */ break;

		case SDLK_HELP: v = MKC_Help; break;
		case SDLK_PRINTSCREEN: v = MKC_Print; break;

		case SDLK_SYSREQ: /* ? */ break;
		//case SDLK_BREAK: /* ? */ break;
		case SDLK_MENU: /* ? */ break;
		case SDLK_POWER: /* ? */ break;
		//case SDLK_EURO: /* ? */ break;
		case SDLK_UNDO: /* ? */ break;

		default:
			break;
	}

	return v;
}

LOCALPROC DoKeyCode(SDL_Keycode r, blnr down)
{
	int v = SDLKey2MacKeyCode(r);
	if (v >= 0) {
		Keyboard_UpdateKeyMap2(v, down);
	}
}

LOCALPROC DisableKeyRepeat(void)
{
}

LOCALPROC RestoreKeyRepeat(void)
{
}

LOCALPROC ReconnectKeyCodes3(void)
{
}

LOCALPROC DisconnectKeyCodes3(void)
{
	DisconnectKeyCodes2();
	MyMouseButtonSet(falseblnr);
}

/* --- time, date, location --- */

LOCALVAR ui5b TrueEmulatedTime = 0;
LOCALVAR ui5b CurEmulatedTime = 0;

#define MyInvTimeDivPow 16
#define MyInvTimeDiv (1 << MyInvTimeDivPow)
#define MyInvTimeDivMask (MyInvTimeDiv - 1)
#define MyInvTimeStep 1089590 /* 1000 / 60.14742 * MyInvTimeDiv */

LOCALVAR Uint32 LastTime;

LOCALVAR Uint32 NextIntTime;
LOCALVAR ui5b NextFracTime;

LOCALPROC IncrNextTime(void)
{
	NextFracTime += MyInvTimeStep;
	NextIntTime += (NextFracTime >> MyInvTimeDivPow);
	NextFracTime &= MyInvTimeDivMask;
}

LOCALPROC InitNextTime(void)
{
	NextIntTime = LastTime;
	NextFracTime = 0;
	IncrNextTime();
}

LOCALVAR ui5b NewMacDateInSeconds;

LOCALFUNC blnr UpdateTrueEmulatedTime(void)
{
	Uint32 LatestTime;
	si5b TimeDiff;

	LatestTime = SDL_GetTicks();
	if (LatestTime != LastTime) {

		NewMacDateInSeconds = LatestTime / 1000;
			/* no date and time api in SDL */

		LastTime = LatestTime;
		TimeDiff = (LatestTime - NextIntTime);
			/* this should work even when time wraps */
		if (TimeDiff >= 0) {
			if (TimeDiff > 64) {
				/* emulation interrupted, forget it */
				++TrueEmulatedTime;
				InitNextTime();
			} else {
				do {
					++TrueEmulatedTime;
					IncrNextTime();
					TimeDiff = (LatestTime - NextIntTime);
				} while (TimeDiff >= 0);
			}
			return trueblnr;
		} else {
			if (TimeDiff < -20) {
				/* clock goofed if ever get here, reset */
				InitNextTime();
			}
		}
	}
	return falseblnr;
}


LOCALFUNC blnr CheckDateTime(void)
{
	if (CurMacDateInSeconds != NewMacDateInSeconds) {
		CurMacDateInSeconds = NewMacDateInSeconds;
		return trueblnr;
	} else {
		return falseblnr;
	}
}

LOCALPROC StartUpTimeAdjust(void)
{
	LastTime = SDL_GetTicks();
	InitNextTime();
}

LOCALFUNC blnr InitLocationDat(void)
{
	LastTime = SDL_GetTicks();
	InitNextTime();
	NewMacDateInSeconds = LastTime / 1000;
	CurMacDateInSeconds = NewMacDateInSeconds;

	return trueblnr;
}

/* --- sound --- */

#if MySoundEnabled

#define kLn2SoundBuffers 4 /* kSoundBuffers must be a power of two */
#define kSoundBuffers (1 << kLn2SoundBuffers)
#define kSoundBuffMask (kSoundBuffers - 1)

#define DesiredMinFilledSoundBuffs 3
	/*
		if too big then sound lags behind emulation.
		if too small then sound will have pauses.
	*/

#define kLnOneBuffLen 9
#define kLnAllBuffLen (kLn2SoundBuffers + kLnOneBuffLen)
#define kOneBuffLen (1UL << kLnOneBuffLen)
#define kAllBuffLen (1UL << kLnAllBuffLen)
#define kLnOneBuffSz (kLnOneBuffLen + kLn2SoundSampSz - 3)
#define kLnAllBuffSz (kLnAllBuffLen + kLn2SoundSampSz - 3)
#define kOneBuffSz (1UL << kLnOneBuffSz)
#define kAllBuffSz (1UL << kLnAllBuffSz)
#define kOneBuffMask (kOneBuffLen - 1)
#define kAllBuffMask (kAllBuffLen - 1)
#define dbhBufferSize (kAllBuffSz + kOneBuffSz)

LOCALVAR tpSoundSamp TheSoundBuffer = nullpr;
LOCALVAR ui4b ThePlayOffset;
LOCALVAR ui4b TheFillOffset;
LOCALVAR ui4b TheWriteOffset;
LOCALVAR ui4b MinFilledSoundBuffs;

LOCALPROC MySound_Start0(void)
{
	/* Reset variables */
	ThePlayOffset = 0;
	TheFillOffset = 0;
	TheWriteOffset = 0;
	MinFilledSoundBuffs = kSoundBuffers + 1;
}

GLOBALFUNC tpSoundSamp MySound_BeginWrite(ui4r n, ui4r *actL)
{
	ui4b ToFillLen = kAllBuffLen - (TheWriteOffset - ThePlayOffset);
	ui4b WriteBuffContig =
		kOneBuffLen - (TheWriteOffset & kOneBuffMask);

	if (WriteBuffContig < n) {
		n = WriteBuffContig;
	}
	if (ToFillLen < n) {
		/* overwrite previous buffer */
		TheWriteOffset -= kOneBuffLen;
	}

	*actL = n;
	return TheSoundBuffer + (TheWriteOffset & kAllBuffMask);
}

LOCALFUNC blnr MySound_EndWrite0(ui4r actL)
{
	blnr v;

	TheWriteOffset += actL;

	if (0 != (TheWriteOffset & kOneBuffMask)) {
		v = falseblnr;
	} else {
		/* just finished a block */

		TheFillOffset = TheWriteOffset;

		v = trueblnr;
	}

	return v;
}

LOCALPROC MySound_SecondNotify0(void)
{
	if (MinFilledSoundBuffs <= kSoundBuffers) {
		if (MinFilledSoundBuffs > DesiredMinFilledSoundBuffs) {
			/* fprintf(stderr, "MinFilledSoundBuffs too high\n"); */
			++CurEmulatedTime;
		} else if (MinFilledSoundBuffs < DesiredMinFilledSoundBuffs) {
			/* fprintf(stderr, "MinFilledSoundBuffs too low\n"); */
			--CurEmulatedTime;
		}
		MinFilledSoundBuffs = kSoundBuffers + 1;
	}
}

#define SOUND_SAMPLERATE 22255 /* = round(7833600 * 2 / 704) */

LOCALVAR blnr HaveSoundOut = falseblnr;
LOCALVAR blnr HaveStartedPlaying = falseblnr;

LOCALPROC MySound_Start(void)
{
	if (HaveSoundOut) {
		MySound_Start0();
		SDL_PauseAudio(0);
	}
}

LOCALPROC MySound_Stop(void)
{
	if (HaveSoundOut) {
		SDL_PauseAudio(1);
		HaveStartedPlaying = falseblnr;
	}
}

static void my_audio_callback(void *udata, Uint8 *stream, int len)
{
	int i;

label_retry:
	{
		ui4b ToPlayLen = TheFillOffset - ThePlayOffset;
		ui4b FilledSoundBuffs = ToPlayLen >> kLnOneBuffLen;

		if (! HaveStartedPlaying) {
			if ((ToPlayLen >> kLnOneBuffLen) < 12) {
				ToPlayLen = 0;
			} else {
				HaveStartedPlaying = trueblnr;
			}
		}

		if (0 == len) {
			/* done */

			if (FilledSoundBuffs < MinFilledSoundBuffs) {
				MinFilledSoundBuffs = FilledSoundBuffs;
			}
		} else if (ToPlayLen == 0) {
			/* under run */

			/* fprintf(stderr, "under run\n"); */

			for (i = 0; i < len; ++i) {
				*stream++ = 0x80;
			}
			MinFilledSoundBuffs = 0;
		} else {
			ui4b PlayBuffContig = kAllBuffLen
				- (ThePlayOffset & kAllBuffMask);
			tpSoundSamp p = TheSoundBuffer
				+ (ThePlayOffset & kAllBuffMask);

			if (ToPlayLen > PlayBuffContig) {
				ToPlayLen = PlayBuffContig;
			}
			if (ToPlayLen > len) {
				ToPlayLen = len;
			}

			for (i = 0; i < ToPlayLen; ++i) {
				*stream++ = *p++;
			}

			ThePlayOffset += ToPlayLen;
			len -= ToPlayLen;

			goto label_retry;
		}
	}
}

LOCALFUNC blnr MySound_Init(void)
{
	SDL_AudioSpec desired;

	desired.freq = SOUND_SAMPLERATE;
	desired.format = AUDIO_U8;
	desired.channels = 1;
	desired.samples = 1024;
	desired.callback = my_audio_callback;
	desired.userdata = NULL;

	/* Open the audio device */
	if (SDL_OpenAudio(&desired, NULL) < 0) {
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
	} else {
		HaveSoundOut = trueblnr;
	}

	return trueblnr; /* keep going, even if no sound */
}

LOCALPROC MySound_UnInit(void)
{
	if (HaveSoundOut) {
		SDL_CloseAudio();
	}
}

GLOBALPROC MySound_EndWrite(ui4r actL)
{
	if (MySound_EndWrite0(actL)) {
	}
}

LOCALPROC MySound_SecondNotify(void)
{
	if (HaveSoundOut) {
		MySound_SecondNotify0();
	}
}

#endif

/* --- basic dialogs --- */

LOCALPROC CheckSavedMacMsg(void)
{
	/* called only on quit, if error saved but not yet reported */

	if (nullpr != SavedBriefMsg) {
		char briefMsg0[ClStrMaxLength + 1];
		char longMsg0[ClStrMaxLength + 1];

		NativeStrFromCStr(briefMsg0, SavedBriefMsg);
		NativeStrFromCStr(longMsg0, SavedLongMsg);

		fprintf(stderr, "%s\n", briefMsg0);
		fprintf(stderr, "%s\n", longMsg0);

		SavedBriefMsg = nullpr;
	}
}

/* --- clipboard --- */

#define UseMotionEvents 1

#if UseMotionEvents
LOCALVAR blnr CaughtMouse = falseblnr;
#endif

/* --- event handling for main window --- */

LOCALPROC HandleTheEvent(SDL_Event *event)
{
	switch (event->type) {
		case SDL_QUIT:
			RequestMacOff = trueblnr;
			break;
		case SDL_WINDOWEVENT:
			switch (event->window.event) {
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					// Window window gained keyboard focus
					//gTrueBackgroundFlag = (0 == event->active.gain);
					gTrueBackgroundFlag = 0;
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
					// Window window gained keyboard focus
					//gTrueBackgroundFlag = (0 == event->active.gain);
					gTrueBackgroundFlag = 0;
					break;
				case SDL_WINDOWEVENT_ENTER:
					// Mouse has entered the window Window
					CaughtMouse = 1;
					break;
				case SDL_WINDOWEVENT_LEAVE:
					// Mouse has entered the window Window
					CaughtMouse = 1;
					break;
			}
			break;
		case SDL_MOUSEMOTION:
			MousePositionNotifyRelative(
				event->motion.xrel, event->motion.yrel);
			break;
		case SDL_MOUSEBUTTONDOWN:
			/* any mouse button, we don't care which */
			MyMouseButtonSet(trueblnr);
			break;
		case SDL_MOUSEBUTTONUP:
			MyMouseButtonSet(falseblnr);
			break;
		case SDL_KEYDOWN:
			DoKeyCode(event->key.keysym.sym, trueblnr);
			break;
		case SDL_KEYUP:
			DoKeyCode(event->key.keysym.sym, falseblnr);
			break;
#if 0
		case Expose: /* SDL doesn't have an expose event */
			int x0 = event->expose.x;
			int y0 = event->expose.y;
			int x1 = x0 + event->expose.width;
			int y1 = y0 + event->expose.height;

			if (x0 < 0) {
				x0 = 0;
			}
			if (x1 > vMacScreenWidth) {
				x1 = vMacScreenWidth;
			}
			if (y0 < 0) {
				y0 = 0;
			}
			if (y1 > vMacScreenHeight) {
				y1 = vMacScreenHeight;
			}
			if ((x0 < x1) && (y0 < y1)) {
				HaveChangedScreenBuff(y0, x0, y1, x1);
			}
			break;
#endif
	}
}

/* --- main window creation and disposal --- */

LOCALVAR int my_argc;
LOCALVAR char **my_argv;

LOCALFUNC blnr Screen_Init(void)
{
	blnr v = falseblnr;

	InitKeyCodes();

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
	{
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
	} else {
		//SDL_WM_SetCaption(kStrAppName, NULL);
		v = trueblnr;
	}

	return v;
}

#if MayFullScreen
LOCALVAR blnr GrabMachine = falseblnr;
#endif

#if MayFullScreen
LOCALPROC GrabTheMachine(void)
{
	//(void) SDL_WM_GrabInput(SDL_GRAB_ON);
	SDL_SetWindowGrab(window, SDL_TRUE);


#if EnableMouseMotion
	/*
		if magnification changes, need to reset,
		even if HaveMouseMotion already true
	*/
	if (MyMoveMouse(ViewHStart + (ViewHSize / 2),
		ViewVStart + (ViewVSize / 2)))
	{
		SavedMouseH = ViewHStart + (ViewHSize / 2);
		SavedMouseV = ViewVStart + (ViewVSize / 2);
		HaveMouseMotion = trueblnr;
	}
#endif
}
#endif

#if MayFullScreen
LOCALPROC UngrabMachine(void)
{
#if EnableMouseMotion
	if (HaveMouseMotion) {
		(void) MyMoveMouse(CurMouseH, CurMouseV);
		HaveMouseMotion = falseblnr;
	}
#endif

	//(void) SDL_WM_GrabInput(SDL_GRAB_OFF);
	SDL_SetWindowGrab(window, SDL_FALSE);
}
#endif

#if EnableMouseMotion && MayFullScreen
LOCALPROC MyMouseConstrain(void)
{
	si4b shiftdh;
	si4b shiftdv;

	if (SavedMouseH < ViewHStart + (ViewHSize / 4)) {
		shiftdh = ViewHSize / 2;
	} else if (SavedMouseH > ViewHStart + ViewHSize - (ViewHSize / 4)) {
		shiftdh = - ViewHSize / 2;
	} else {
		shiftdh = 0;
	}
	if (SavedMouseV < ViewVStart + (ViewVSize / 4)) {
		shiftdv = ViewVSize / 2;
	} else if (SavedMouseV > ViewVStart + ViewVSize - (ViewVSize / 4)) {
		shiftdv = - ViewVSize / 2;
	} else {
		shiftdv = 0;
	}
	if ((shiftdh != 0) || (shiftdv != 0)) {
		SavedMouseH += shiftdh;
		SavedMouseV += shiftdv;
		if (! MyMoveMouse(SavedMouseH, SavedMouseV)) {
			HaveMouseMotion = falseblnr;
		}
	}
}
#endif

LOCALFUNC blnr CreateMainWindow(void)
{
	int NewWindowHeight = vMacScreenHeight;
	int NewWindowWidth = vMacScreenWidth;
	Uint32 flags = SDL_SWSURFACE;
	blnr v = falseblnr;

#if EnableMagnify && 1
	if (UseMagnify) {
		NewWindowHeight *= MyWindowScale;
		NewWindowWidth *= MyWindowScale;
	}
#endif

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		/* We don't want physical screen mode to be changed in modern displays,
		 * so we pass this _DESKTOP flag. */
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
#endif

	ViewHStart = 0;
	ViewVStart = 0;
	ViewHSize = vMacScreenWidth;
	ViewVSize = vMacScreenHeight;

	/* We support 8bpp too: on SDL1.x, we would create a 8bpp surface here using SetVideoMode(),
	 * but now we create a 32bpp surface that WILL be used to render 8bpp graphics using this trick.
	 * Reminder: 8bpp is active when vMacScreenDepth is 0, defined in CNFGGLOB.h.
	 * So setting a different mask we can do 32bpp or 8bpp using a 32bpp surface, and there will be
	 * no conversion before uploading texture because we are already using a 32bpp texture.*/
	my_surface= SDL_CreateRGBSurface(0, vMacScreenWidth, vMacScreenHeight, 32,
#if 0 != vMacScreenDepth
		0,0,0,0
#else
		0x00FF0000,
                0x0000FF00,
                0x000000FF,
                0xFF000000
#endif
		);
	
	if (NULL == my_surface) {
		fprintf(stderr, "SDL_CreateRGBSurface fails: %s\n",
			SDL_GetError());
	} else {
#if 0 != vMacScreenDepth
		ColorModeWorks = trueblnr;
#endif
		v = trueblnr;
	}

#if VarFullScreen
	if (UseFullScreen)
#endif
#if MayFullScreen
	{
		SDL_DisplayMode info;
		SDL_GetCurrentDisplayMode(0, &info);
		
		displayWidth  = info.w; 
		displayHeight = info.h;

		dst_rect.w = displayHeight * ((float)vMacScreenWidth / (float)vMacScreenHeight);
		dst_rect.h = displayHeight;
		dst_rect.x = (displayWidth - dst_rect.w) / 2;
		dst_rect.y = 0;

		/* We don't want physical screen mode to be changed in modern displays,
		 * so we pass this _DESKTOP flag. */
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	else {
		dst_rect.w = vMacScreenWidth;
		dst_rect.h = vMacScreenHeight;
		dst_rect.x = 0;
		dst_rect.y = 0;
	}
#else
	dst_rect.w = vMacScreenWidth;
	dst_rect.h = vMacScreenHeight;
	dst_rect.x = 0;
	dst_rect.y = 0;
#endif

	src_rect.w = vMacScreenWidth;
	src_rect.h = vMacScreenHeight;	
	src_rect.x = 0;
	src_rect.y = 0;

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear"); 
	SDL_ShowCursor(SDL_DISABLE);
	SDL_SetRelativeMouseMode(SDL_ENABLE);

	/* Remember: width and height values will be ignored when we use fullscreen. */
	window = SDL_CreateWindow(
        	"Mini vMac", 0, 0, NewWindowWidth, NewWindowHeight, 
        	flags);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	
	texture = SDL_CreateTexture(renderer,
                               SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               vMacScreenWidth, vMacScreenHeight);

	return v;
}

LOCALFUNC blnr ReCreateMainWindow(void)
{
	ForceShowCursor(); /* hide/show cursor api is per window */

#if MayFullScreen
	if (GrabMachine) {
		GrabMachine = falseblnr;
		UngrabMachine();
	}
#endif

#if EnableMagnify
	UseMagnify = WantMagnify;
#endif
#if VarFullScreen
	UseFullScreen = WantFullScreen;
#endif

	/* First things first, we destroy the window before creating a new one. 
	   We're not in SetVideoMode() land anymore. */
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	(void) CreateMainWindow();

	if (HaveCursorHidden) {
		(void) MyMoveMouse(CurMouseH, CurMouseV);
	}

	return trueblnr;
}

LOCALPROC ZapWinStateVars(void)
{
}

#if VarFullScreen
LOCALPROC ToggleWantFullScreen(void)
{
	WantFullScreen = ! WantFullScreen;
}
#endif

/* --- SavedTasks --- */

LOCALPROC LeaveBackground(void)
{
	ReconnectKeyCodes3();
	DisableKeyRepeat();
}

LOCALPROC EnterBackground(void)
{
	RestoreKeyRepeat();
	DisconnectKeyCodes3();

	ForceShowCursor();
}

LOCALPROC LeaveSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Start();
#endif

	StartUpTimeAdjust();
}

LOCALPROC EnterSpeedStopped(void)
{
#if MySoundEnabled
	MySound_Stop();
#endif
}

LOCALPROC CheckForSavedTasks(void)
{
	if (MyEvtQNeedRecover) {
		MyEvtQNeedRecover = falseblnr;

		/* attempt cleanup, MyEvtQNeedRecover may get set again */
		MyEvtQTryRecoverFromFull();
	}

#if EnableMouseMotion && MayFullScreen
	if (HaveMouseMotion) {
		MyMouseConstrain();
	}
#endif

	if (RequestMacOff) {
		RequestMacOff = falseblnr;
		if (AnyDiskInserted()) {
			MacMsgOverride(kStrQuitWarningTitle,
				kStrQuitWarningMessage);
		} else {
			ForceMacOff = trueblnr;
		}
	}

	if (ForceMacOff) {
		return;
	}

	if (gTrueBackgroundFlag != gBackgroundFlag) {
		gBackgroundFlag = gTrueBackgroundFlag;
		if (gTrueBackgroundFlag) {
			EnterBackground();
		} else {
			LeaveBackground();
		}
	}

	if (CurSpeedStopped != (SpeedStopped ||
		(gBackgroundFlag && ! RunInBackground
#if EnableAutoSlow && 0
			&& (QuietSubTicks >= 4092)
#endif
		)))
	{
		CurSpeedStopped = ! CurSpeedStopped;
		if (CurSpeedStopped) {
			EnterSpeedStopped();
		} else {
			LeaveSpeedStopped();
		}
	}

	if ((nullpr != SavedBriefMsg) & ! MacMsgDisplayed) {
		MacMsgDisplayOn();
	}

#if EnableMagnify || VarFullScreen
	if (0
#if EnableMagnify
		|| (UseMagnify != WantMagnify)
#endif
#if VarFullScreen
		|| (UseFullScreen != WantFullScreen)
#endif
		)
	{
		(void) ReCreateMainWindow();
	}
#endif

#if MayFullScreen
	if (GrabMachine != (
#if VarFullScreen
		UseFullScreen &&
#endif
		! (gTrueBackgroundFlag || CurSpeedStopped)))
	{
		GrabMachine = ! GrabMachine;
		if (GrabMachine) {
			GrabTheMachine();
		} else {
			UngrabMachine();
		}
	}
#endif

	if (NeedWholeScreenDraw) {
		NeedWholeScreenDraw = falseblnr;
		ScreenChangedAll();
	}

	MyDrawChangesAndClear();

	if (HaveCursorHidden != (WantCursorHidden
		&& ! (gTrueBackgroundFlag || CurSpeedStopped)))
	{
		HaveCursorHidden = ! HaveCursorHidden;
		(void) SDL_ShowCursor(
			HaveCursorHidden ? SDL_DISABLE : SDL_ENABLE);
	}
}

/* --- command line parsing --- */

LOCALFUNC blnr ScanCommandLine(void)
{
	char *pa;
	int i = 1;

label_retry:
	if (i < my_argc) {
		pa = my_argv[i++];
		if ('-' == pa[0]) {
			if ((0 == strcmp(pa, "--rom"))
				|| (0 == strcmp(pa, "-r")))
			{
				if (i < my_argc) {
					rom_path = my_argv[i++];
					goto label_retry;
				}
			} else
			{
				MacMsg(kStrBadArgTitle, kStrBadArgMessage, falseblnr);
			}
		} else {
			(void) Sony_Insert1(pa, falseblnr);
			goto label_retry;
		}
	}

	return trueblnr;
}

/* --- main program flow --- */

LOCALVAR ui5b OnTrueTime = 0;

GLOBALFUNC blnr ExtraTimeNotOver(void)
{
	UpdateTrueEmulatedTime();
	return TrueEmulatedTime == OnTrueTime;
}

/* --- platform independent code can be thought of as going here --- */

#include "PROGMAIN.h"

LOCALPROC RunEmulatedTicksToTrueTime(void)
{
	si3b n = OnTrueTime - CurEmulatedTime;

	if (n > 0) {
		if (CheckDateTime()) {
#if MySoundEnabled
			MySound_SecondNotify();
#endif
		}

		if ((! gBackgroundFlag)
#if UseMotionEvents
			&& (! CaughtMouse)
#endif
			)
		{
			CheckMouseState();
		}

#if EnableMouseMotion && MayFullScreen
		if (HaveMouseMotion) {
			AutoScrollScreen();
		}
#endif

		DoEmulateOneTick();
		++CurEmulatedTime;

		MyDrawChangesAndClear();

		if (n > 8) {
			/* emulation not fast enough */
			n = 8;
			CurEmulatedTime = OnTrueTime - n;
		}

		if (ExtraTimeNotOver() && (--n > 0)) {
			/* lagging, catch up */

			EmVideoDisable = trueblnr;

			do {
				DoEmulateOneTick();
				++CurEmulatedTime;
			} while (ExtraTimeNotOver()
				&& (--n > 0));

			EmVideoDisable = falseblnr;
		}

		EmLagTime = n;
	}
}

LOCALPROC RunOnEndOfSixtieth(void)
{
	while (ExtraTimeNotOver()) {
		(void) SDL_Delay(NextIntTime - LastTime);
	}

	OnTrueTime = TrueEmulatedTime;
	RunEmulatedTicksToTrueTime();
}

LOCALPROC WaitForTheNextEvent(void)
{
	SDL_Event event;

	if (SDL_WaitEvent(&event)) {
		HandleTheEvent(&event);
	}
}

LOCALPROC CheckForSystemEvents(void)
{
	SDL_Event event;
	int i = 10;

	while ((--i >= 0) && SDL_PollEvent(&event)) {
		HandleTheEvent(&event);
	}
}

LOCALPROC MainEventLoop(void)
{
	for (; ; ) {
		CheckForSystemEvents();
		CheckForSavedTasks();
		if (ForceMacOff) {
			return;
		}

		if (CurSpeedStopped) {
			WaitForTheNextEvent();
		} else {
			DoEmulateExtraTime();
			RunOnEndOfSixtieth();
		}
	}
}

LOCALPROC ZapOSGLUVars(void)
{
	InitDrives();
	ZapWinStateVars();
}

LOCALPROC ReserveAllocAll(void)
{
#if dbglog_HAVE
	dbglog_ReserveAlloc();
#endif
	ReserveAllocOneBlock(&ROM, kROM_Size, 5, falseblnr);

	ReserveAllocOneBlock(&screencomparebuff,
		vMacScreenNumBytes, 5, trueblnr);
#if UseControlKeys
	ReserveAllocOneBlock(&CntrlDisplayBuff,
		vMacScreenNumBytes, 5, falseblnr);
#endif

	ReserveAllocOneBlock(&CLUT_final, CLUT_finalsz, 5, falseblnr);
#if MySoundEnabled
	ReserveAllocOneBlock((ui3p *)&TheSoundBuffer,
		dbhBufferSize, 5, falseblnr);
#endif

	EmulationReserveAlloc();
}

LOCALFUNC blnr AllocMyMemory(void)
{
	uimr n;
	blnr IsOk = falseblnr;

	ReserveAllocOffset = 0;
	ReserveAllocBigBlock = nullpr;
	ReserveAllocAll();
	n = ReserveAllocOffset;
	ReserveAllocBigBlock = (ui3p)calloc(1, n);
	if (NULL == ReserveAllocBigBlock) {
		MacMsg(kStrOutOfMemTitle, kStrOutOfMemMessage, trueblnr);
	} else {
		ReserveAllocOffset = 0;
		ReserveAllocAll();
		if (n != ReserveAllocOffset) {
			/* oops, program error */
		} else {
			IsOk = trueblnr;
		}
	}

	return IsOk;
}

LOCALPROC UnallocMyMemory(void)
{
	if (nullpr != ReserveAllocBigBlock) {
		free((char *)ReserveAllocBigBlock);
	}
}

LOCALFUNC blnr InitOSGLU(void)
{
	if (AllocMyMemory())
#if dbglog_HAVE
	if (dbglog_open())
#endif
	if (ScanCommandLine())
	if (LoadInitialImages())
	if (LoadMacRom())
	if (InitLocationDat())
#if MySoundEnabled
	if (MySound_Init())
#endif
	if (Screen_Init())
	if (CreateMainWindow())
	if (InitEmulation())
	{
		return trueblnr;
	}
	return falseblnr;
}

LOCALPROC UnInitOSGLU(void)
{
	if (MacMsgDisplayed) {
		MacMsgDisplayOff();
	}

	RestoreKeyRepeat();
#if MayFullScreen
	UngrabMachine();
#endif
#if MySoundEnabled
	MySound_Stop();
#endif
#if MySoundEnabled
	MySound_UnInit();
#endif
#if IncludePbufs
	UnInitPbufs();
#endif
	UnInitDrives();

	ForceShowCursor();

#if dbglog_HAVE
	dbglog_close();
#endif

	UnallocMyMemory();

	CheckSavedMacMsg();

	SDL_Quit();
}

int main(int argc, char **argv)
{
	my_argc = argc;
	my_argv = argv;

	ZapOSGLUVars();
	if (InitOSGLU()) {
		MainEventLoop();
	}
	UnInitOSGLU();

	return 0;
}
