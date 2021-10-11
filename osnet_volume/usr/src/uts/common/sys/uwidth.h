/*
 * Copyright (c) 1996, 1998, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Came from en_US.UTF-8 locale's width definition at 6/24/1999.
 *
 * Epoch:	Based on Unicode 2.0 / ISO/IEC 10646-1:1993 plus
 *		AM2 (UTF-8) and DAM5 (Hangul) as of 6/1996.
 *
 * 2/28/1998:	Added missed Tibetan block (U+0F00 ~ U+0FBF),
 *		Added OBJECT REPLACEMENT CHARACTERS (U+FFFC) and
 *		EURO SIGN (U+20AC) for Unicode 2.1.
 * 8/3/1999:	Added Unicode 3.0 Beta characters.
 */

#ifndef	_SYS_UWIDTH_H
#define	_SYS_UWIDTH_H

#pragma ident	"@(#)uwidth.h	1.4	99/08/09 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Private use area characters' width. We set it to two since PU will be
 * used mostly by Asian locales.
 */
#ifndef	PU
#define	PU			2
#endif	/* PU */

/* Not-yet-assigned/some control/invalid characters will have width of 1. */
#ifndef	IL
#define	IL			1
#endif	/* IL */

/*
 * Following table contains width information for Unicode.
 *
 * There are only three different kind of width: zero, one, or two.
 * The fourth possible value was -1 but changed to 1; the value means not yet
 * assigned, some control, or, invalid Unicode character, i.e., U+FFFE and
 * U+FFFF.
 */
static const ldterm_unicode_data_cell_t ucode[1][16384] = {
	{
/*		0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F */
/*		---------------------------------------------- */
/* U+0000 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+000F */
/* U+0010 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+001F */
/* U+0020 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+002F */
/* U+0030 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+003F */
/* U+0040 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+004F */
/* U+0050 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+005F */
/* U+0060 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+006F */
/* U+0070 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+007F */
/* U+0080 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+008F */
/* U+0090 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+009F */
/* U+00A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+00AF */
/* U+00B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+00BF */
/* U+00C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+00CF */
/* U+00D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+00DF */
/* U+00E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+00EF */
/* U+00F0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+00FF */
/* U+0100 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+010F */
/* U+0110 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+011F */
/* U+0120 */	1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+012F */
/* U+0130 */	1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+013F */
/* U+0140 */	2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1,    /* U+014F */
/* U+0150 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+015F */
/* U+0160 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+016F */
/* U+0170 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+017F */
/* U+0180 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+018F */
/* U+0190 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+019F */
/* U+01A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+01AF */
/* U+01B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+01BF */
/* U+01C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1,    /* U+01CF */
/* U+01D0 */	2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 1,    /* U+01DF */
/* U+01E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+01EF */
/* U+01F0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+01FF */
/* U+0200 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+020F */
/* U+0210 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+021F */
/* U+0220 */	IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+022F */
/* U+0230 */	1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+023F */
/* U+0240 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+024F */
/* U+0250 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+025F */
/* U+0260 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+026F */
/* U+0270 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+027F */
/* U+0280 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+028F */
/* U+0290 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+029F */
/* U+02A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,   /* U+02AF */
/* U+02B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+02BF */
/* U+02C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1,    /* U+02CF */
/* U+02D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 2, 1, 1, 1,    /* U+02DF */
/* U+02E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,   /* U+02EF */
/* U+02F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+02FF */
/* U+0300 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* U+030F */
/* U+0310 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* U+031F */
/* U+0320 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* U+032F */
/* U+0330 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* U+033F */
/* U+0340 */	0, 0, 0, 0, 0, 0, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+034F */
/* U+0350 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+035F */
/* U+0360 */	0, 0, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+036F */
/* U+0370 */	IL,IL,IL,IL,1, 1, IL,IL,IL,IL,1, IL,IL,IL,1, IL,   /* U+037F */
/* U+0380 */	IL,IL,IL,IL,1, 1, 1, 1, 1, 1, 1, IL,1, IL,1, 1,    /* U+038F */
/* U+0390 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+039F */
/* U+03A0 */	1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+03AF */
/* U+03B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+03BF */
/* U+03C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+03CF */
/* U+03D0 */	1, 1, 1, 1, 1, 1, 1, IL,IL,IL,1, IL,1, IL,1, IL,   /* U+03DF */
/* U+03E0 */	1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+03EF */
/* U+03F0 */	1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+03FF */
/* U+0400 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+040F */
/* U+0410 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+041F */
/* U+0420 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+042F */
/* U+0430 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+043F */
/* U+0440 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+044F */
/* U+0450 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+045F */
/* U+0460 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+046F */
/* U+0470 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+047F */
/* U+0480 */	1, 1, 1, 0, 0, 0, 0, IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+048F */
/* U+0490 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+049F */
/* U+04A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+04AF */
/* U+04B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+04BF */
/* U+04C0 */	1, 1, 1, 1, 1, IL,IL,1, 1, IL,IL,1, 1, IL,IL,IL,   /* U+04CF */
/* U+04D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+04DF */
/* U+04E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+04EF */
/* U+04F0 */	1, 1, 1, 1, 1, 1, IL,IL,1, 1, IL,IL,IL,IL,IL,IL,   /* U+04FF */
/* U+0500 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+050F */
/* U+0510 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+051F */
/* U+0520 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+052F */
/* U+0530 */	IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+053F */
/* U+0540 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+054F */
/* U+0550 */	1, 1, 1, 1, 1, 1, 1, IL,IL,1, 1, 1, 1, 1, 1, 1,    /* U+055F */
/* U+0560 */	IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+056F */
/* U+0570 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+057F */
/* U+0580 */	1, 1, 1, 1, 1, 1, 1, 1, IL,1, IL,IL,IL,IL,IL,IL,   /* U+058F */
/* U+0590 */	IL,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* U+059F */
/* U+05A0 */	0, 0, IL,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* U+05AF */
/* U+05B0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, IL,0, 0, 0, 1, 0,    /* U+05BF */
/* U+05C0 */	1, 0, 0, 1, 0, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+05CF */
/* U+05D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+05DF */
/* U+05E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,   /* U+05EF */
/* U+05F0 */	1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+05FF */
/* U+0600 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,1, IL,IL,IL,   /* U+060F */
/* U+0610 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,1, IL,IL,IL,1,    /* U+061F */
/* U+0620 */	IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+062F */
/* U+0630 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,   /* U+063F */
/* U+0640 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,    /* U+064F */
/* U+0650 */	0, 0, 0, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+065F */
/* U+0660 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,   /* U+066F */
/* U+0670 */	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+067F */
/* U+0680 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+068F */
/* U+0690 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+069F */
/* U+06A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+06AF */
/* U+06B0 */	1, 1, 1, 1, 1, 1, 1, 1, IL,IL,1, 1, 1, 1, 1, IL,   /* U+06BF */
/* U+06C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+06CF */
/* U+06D0 */	1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* U+06DF */
/* U+06E0 */	0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, IL,IL,   /* U+06EF */
/* U+06F0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,   /* U+06FF */
/* U+0700 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+070F */
/* U+0710 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+071F */
/* U+0720 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+072F */
/* U+0730 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+073F */
/* U+0740 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+074F */
/* U+0750 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+075F */
/* U+0760 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+076F */
/* U+0770 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+077F */
/* U+0780 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+078F */
/* U+0790 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+079F */
/* U+07A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+07AF */
/* U+07B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+07BF */
/* U+07C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+07CF */
/* U+07D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+07DF */
/* U+07E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+07EF */
/* U+07F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+07FF */
/* U+0800 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+080F */
/* U+0810 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+081F */
/* U+0820 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+082F */
/* U+0830 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+083F */
/* U+0840 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+084F */
/* U+0850 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+085F */
/* U+0860 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+086F */
/* U+0870 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+087F */
/* U+0880 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+088F */
/* U+0890 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+089F */
/* U+08A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+08AF */
/* U+08B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+08BF */
/* U+08C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+08CF */
/* U+08D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+08DF */
/* U+08E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+08EF */
/* U+08F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+08FF */
/* U+0900 */	IL,0, 0, 0, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+090F */
/* U+0910 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+091F */
/* U+0920 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+092F */
/* U+0930 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,0, 1, 0, 0,    /* U+093F */
/* U+0940 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, IL,IL,   /* U+094F */
/* U+0950 */	1, 0, 0, 0, 0, IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1,    /* U+095F */
/* U+0960 */	1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+096F */
/* U+0970 */	1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+097F */
/* U+0980 */	IL,0, 0, 0, IL,1, 1, 1, 1, 1, 1, 1, 1, IL,IL,1,    /* U+098F */
/* U+0990 */	1, IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+099F */
/* U+09A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1,    /* U+09AF */
/* U+09B0 */	1, IL,1, IL,IL,IL,1, 1, 1, 1, IL,IL,0, IL,0, 0,    /* U+09BF */
/* U+09C0 */	0, 0, 0, 0, 0, IL,IL,0, 0, IL,IL,0, 0, 0, IL,IL,   /* U+09CF */
/* U+09D0 */	IL,IL,IL,IL,IL,IL,IL,0, IL,IL,IL,IL,1, 1, IL,1,    /* U+09DF */
/* U+09E0 */	1, 1, 0, 0, IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+09EF */
/* U+09F0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,   /* U+09FF */
/* U+0A00 */	IL,IL,0, IL,IL,1, 1, 1, 1, 1, 1, IL,IL,IL,IL,1,    /* U+0A0F */
/* U+0A10 */	1, IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0A1F */
/* U+0A20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1,    /* U+0A2F */
/* U+0A30 */	1, IL,1, 1, IL,1, 1, IL,1, 1, IL,IL,0, IL,0, 0,    /* U+0A3F */
/* U+0A40 */	0, 0, 0, IL,IL,IL,IL,0, 0, IL,IL,0, 0, 0, IL,IL,   /* U+0A4F */
/* U+0A50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,1, 1, 1, 1, IL,1, IL,   /* U+0A5F */
/* U+0A60 */	IL,IL,IL,IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0A6F */
/* U+0A70 */	0, 0, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0A7F */
/* U+0A80 */	IL,0, 0, 0, IL,1, 1, 1, 1, 1, 1, 1, IL,1, IL,1,    /* U+0A8F */
/* U+0A90 */	1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0A9F */
/* U+0AA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1,    /* U+0AAF */
/* U+0AB0 */	1, IL,1, 1, IL,1, 1, 1, 1, 1, IL,IL,0, 1, 0, 0,    /* U+0ABF */
/* U+0AC0 */	0, 0, 0, 0, 0, 0, IL,0, 0, 0, IL,0, 0, 0, IL,IL,   /* U+0ACF */
/* U+0AD0 */	1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0ADF */
/* U+0AE0 */	1, IL,IL,IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0AEF */
/* U+0AF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0AFF */
/* U+0B00 */	IL,0, 0, 0, IL,1, 1, 1, 1, 1, 1, 1, 1, IL,IL,1,    /* U+0B0F */
/* U+0B10 */	1, IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0B1F */
/* U+0B20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1,    /* U+0B2F */
/* U+0B30 */	1, IL,1, 1, IL,IL,1, 1, 1, 1, IL,IL,0, 1, 0, 0,    /* U+0B3F */
/* U+0B40 */	0, 0, 0, 0, IL,IL,IL,0, 0, IL,IL,0, 0, 0, IL,IL,   /* U+0B4F */
/* U+0B50 */	IL,IL,IL,IL,IL,IL,0, 0, IL,IL,IL,IL,1, 1, IL,1,    /* U+0B5F */
/* U+0B60 */	1, 1, IL,IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0B6F */
/* U+0B70 */	1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0B7F */
/* U+0B80 */	IL,IL,0, 0, IL,1, 1, 1, 1, 1, 1, IL,IL,IL,1, 1,    /* U+0B8F */
/* U+0B90 */	1, IL,1, 1, 1, 1, IL,IL,IL,1, 1, IL,1, IL,1, 1,    /* U+0B9F */
/* U+0BA0 */	IL,IL,IL,1, 1, IL,IL,IL,1, 1, 1, IL,IL,IL,1, 1,    /* U+0BAF */
/* U+0BB0 */	1, 1, 1, 1, 1, 1, IL,1, 1, 1, IL,IL,IL,IL,0, 0,    /* U+0BBF */
/* U+0BC0 */	0, 0, 0, IL,IL,IL,0, 0, 0, IL,0, 0, 0, 0, IL,IL,   /* U+0BCF */
/* U+0BD0 */	IL,IL,IL,IL,IL,IL,IL,0, IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0BDF */
/* U+0BE0 */	IL,IL,IL,IL,IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0BEF */
/* U+0BF0 */	1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0BFF */
/* U+0C00 */	IL,0, 0, 0, IL,1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1,    /* U+0C0F */
/* U+0C10 */	1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0C1F */
/* U+0C20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1,    /* U+0C2F */
/* U+0C30 */	1, 1, 1, 1, IL,1, 1, 1, 1, 1, IL,IL,IL,IL,0, 0,    /* U+0C3F */
/* U+0C40 */	0, 0, 0, 0, 0, IL,0, 0, 0, IL,0, 0, 0, 0, IL,IL,   /* U+0C4F */
/* U+0C50 */	IL,IL,IL,IL,IL,0, 0, IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0C5F */
/* U+0C60 */	1, 1, IL,IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0C6F */
/* U+0C70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0C7F */
/* U+0C80 */	IL,IL,0, 0, IL,1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1,    /* U+0C8F */
/* U+0C90 */	1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0C9F */
/* U+0CA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1,    /* U+0CAF */
/* U+0CB0 */	1, 1, 1, 1, IL,1, 1, 1, 1, 1, IL,IL,IL,IL,0, 0,    /* U+0CBF */
/* U+0CC0 */	0, 0, 0, 0, 0, IL,0, 0, 0, IL,0, 0, 0, 0, IL,IL,   /* U+0CCF */
/* U+0CD0 */	IL,IL,IL,IL,IL,0, 0, IL,IL,IL,IL,IL,IL,IL,1, IL,   /* U+0CDF */
/* U+0CE0 */	1, 1, IL,IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0CEF */
/* U+0CF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0CFF */
/* U+0D00 */	IL,IL,0, 0, IL,1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1,    /* U+0D0F */
/* U+0D10 */	1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0D1F */
/* U+0D20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1,    /* U+0D2F */
/* U+0D30 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,0, 0,    /* U+0D3F */
/* U+0D40 */	0, 0, 0, 0, IL,IL,0, 0, 0, IL,0, 0, 0, 0, IL,IL,   /* U+0D4F */
/* U+0D50 */	IL,IL,IL,IL,IL,IL,IL,0, IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0D5F */
/* U+0D60 */	1, 1, IL,IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0D6F */
/* U+0D70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0D7F */
/* U+0D80 */	IL,IL,1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0D8F */
/* U+0D90 */	1, 1, 1, 1, 1, 1, 1, IL,IL,IL,1, 1, 1, 1, 1, 1,    /* U+0D9F */
/* U+0DA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0DAF */
/* U+0DB0 */	1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, IL,IL,   /* U+0DBF */
/* U+0DC0 */	1, 1, 1, 1, 1, 1, 1, IL,IL,IL,1, IL,IL,IL,IL,1,    /* U+0DCF */
/* U+0DD0 */	1, 1, 1, 1, 1, IL,1, IL,1, 1, 1, 1, 1, 1, 1, 1,    /* U+0DDF */
/* U+0DE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0DEF */
/* U+0DF0 */	IL,IL,1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0DFF */
/* U+0E00 */	IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0E0F */
/* U+0E10 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0E1F */
/* U+0E20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0E2F */
/* U+0E30 */	1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, IL,IL,IL,IL,1,    /* U+0E3F */
/* U+0E40 */	1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1,    /* U+0E4F */
/* U+0E50 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,   /* U+0E5F */
/* U+0E60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0E6F */
/* U+0E70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0E7F */
/* U+0E80 */	IL,1, 1, IL,1, IL,IL,1, 1, IL,1, IL,IL,1, IL,IL,   /* U+0E8F */
/* U+0E90 */	IL,IL,IL,IL,1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1, 1,    /* U+0E9F */
/* U+0EA0 */	IL,1, 1, 1, IL,1, IL,1, IL,IL,1, 1, IL,1, 1, 1,    /* U+0EAF */
/* U+0EB0 */	1, 0, 1, 1, 0, 0, 0, 0, 0, 0, IL,0, 0, 1, IL,IL,   /* U+0EBF */
/* U+0EC0 */	1, 1, 1, 1, 1, IL,1, IL,0, 0, 0, 0, 0, 0, IL,IL,   /* U+0ECF */
/* U+0ED0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,1, 1, IL,IL,   /* U+0EDF */
/* U+0EE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0EEF */
/* U+0EF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0EFF */
/* U+0F00 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0F0F */
/* U+0F10 */	1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1,    /* U+0F1F */
/* U+0F20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0F2F */
/* U+0F30 */	1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1,    /* U+0F3F */
/* U+0F40 */	1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1, 1,    /* U+0F4F */
/* U+0F50 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+0F5F */
/* U+0F60 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,   /* U+0F6F */
/* U+0F70 */	IL,0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,    /* U+0F7F */
/* U+0F80 */	0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, IL,IL,IL,IL,   /* U+0F8F */
/* U+0F90 */	0, 0, 0, 0, 0, 0, IL,0, IL,0, 0, 0, 0, 0, 0, 0,    /* U+0F9F */
/* U+0FA0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, IL,IL,   /* U+0FAF */
/* U+0FB0 */	IL,0, 0, 0, 0, 0, 0, 0, IL,0, IL,IL,IL,IL,IL,IL,   /* U+0FBF */
/* U+0FC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0FCF */
/* U+0FD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0FDF */
/* U+0FE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0FEF */
/* U+0FF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+0FFF */
/* U+1000 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+100F */
/* U+1010 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+101F */
/* U+1020 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+102F */
/* U+1030 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+103F */
/* U+1040 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+104F */
/* U+1050 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+105F */
/* U+1060 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+106F */
/* U+1070 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+107F */
/* U+1080 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+108F */
/* U+1090 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+109F */
/* U+10A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+10AF */
/* U+10B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+10BF */
/* U+10C0 */	1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+10CF */
/* U+10D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+10DF */
/* U+10E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+10EF */
/* U+10F0 */	1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,1, IL,IL,IL,IL,   /* U+10FF */
/* U+1100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+110F */
/* U+1110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+111F */
/* U+1120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+112F */
/* U+1130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+113F */
/* U+1140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+114F */
/* U+1150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,IL,IL,2,    /* U+115F */
/* U+1160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+116F */
/* U+1170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+117F */
/* U+1180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+118F */
/* U+1190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+119F */
/* U+11A0 */	2, 2, 2, IL,IL,IL,IL,IL,2, 2, 2, 2, 2, 2, 2, 2,    /* U+11AF */
/* U+11B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+11BF */
/* U+11C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+11CF */
/* U+11D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+11DF */
/* U+11E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+11EF */
/* U+11F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,IL,IL,IL,   /* U+11FF */
/* U+1200 */	1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1,    /* U+120F */
/* U+1210 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+121F */
/* U+1220 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+122F */
/* U+1230 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+123F */
/* U+1240 */	1, 1, 1, 1, 1, 1, 1, IL,1, IL,1, 1, 1, 1, IL,IL,   /* U+124F */
/* U+1250 */	1, 1, 1, 1, 1, 1, 1, IL,1, IL,1, 1, 1, 1, IL,IL,   /* U+125F */
/* U+1260 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+126F */
/* U+1270 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+127F */
/* U+1280 */	1, 1, 1, 1, 1, 1, 1, IL,1, IL,1, 1, 1, 1, IL,IL,   /* U+128F */
/* U+1290 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+129F */
/* U+12A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+12AF */
/* U+12B0 */	1, IL,1, 1, 1, 1, IL,IL,1, 1, 1, 1, 1, 1, 1, IL,   /* U+12BF */
/* U+12C0 */	1, IL,1, 1, 1, 1, IL,IL,1, 1, 1, 1, 1, 1, 1, IL,   /* U+12CF */
/* U+12D0 */	1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1,    /* U+12DF */
/* U+12E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+12EF */
/* U+12F0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+12FF */
/* U+1300 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+130F */
/* U+1310 */	1, IL,1, 1, 1, 1, IL,IL,1, 1, 1, 1, 1, 1, 1, IL,   /* U+131F */
/* U+1320 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+132F */
/* U+1330 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+133F */
/* U+1340 */	1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1,    /* U+134F */
/* U+1350 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,   /* U+135F */
/* U+1360 */	IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+136F */
/* U+1370 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,   /* U+137F */
/* U+1380 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+138F */
/* U+1390 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+139F */
/* U+13A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+13AF */
/* U+13B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+13BF */
/* U+13C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+13CF */
/* U+13D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+13DF */
/* U+13E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+13EF */
/* U+13F0 */	1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+13FF */
/* U+1400 */	IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+140F */
/* U+1410 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+141F */
/* U+1420 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+142F */
/* U+1430 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+143F */
/* U+1440 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+144F */
/* U+1450 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+145F */
/* U+1460 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+146F */
/* U+1470 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+147F */
/* U+1480 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+148F */
/* U+1490 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+149F */
/* U+14A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+14AF */
/* U+14B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+14BF */
/* U+14C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+14CF */
/* U+14D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+14DF */
/* U+14E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+14EF */
/* U+14F0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+14FF */
/* U+1500 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+150F */
/* U+1510 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+151F */
/* U+1520 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+152F */
/* U+1530 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+153F */
/* U+1540 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+154F */
/* U+1550 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+155F */
/* U+1560 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+156F */
/* U+1570 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+157F */
/* U+1580 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+158F */
/* U+1590 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+159F */
/* U+15A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+15AF */
/* U+15B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+15BF */
/* U+15C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+15CF */
/* U+15D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+15DF */
/* U+15E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+15EF */
/* U+15F0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+15FF */
/* U+1600 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+160F */
/* U+1610 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+161F */
/* U+1620 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+162F */
/* U+1630 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+163F */
/* U+1640 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+164F */
/* U+1650 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+165F */
/* U+1660 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+166F */
/* U+1670 */	1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+167F */
/* U+1680 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+168F */
/* U+1690 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,   /* U+169F */
/* U+16A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+16AF */
/* U+16B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+16BF */
/* U+16C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+16CF */
/* U+16D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+16DF */
/* U+16E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+16EF */
/* U+16F0 */	1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+16FF */
/* U+1700 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+170F */
/* U+1710 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+171F */
/* U+1720 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+172F */
/* U+1730 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+173F */
/* U+1740 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+174F */
/* U+1750 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+175F */
/* U+1760 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+176F */
/* U+1770 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+177F */
/* U+1780 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+178F */
/* U+1790 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+179F */
/* U+17A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+17AF */
/* U+17B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+17BF */
/* U+17C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+17CF */
/* U+17D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,   /* U+17DF */
/* U+17E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,   /* U+17EF */
/* U+17F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+17FF */
/* U+1800 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+180F */
/* U+1810 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,   /* U+181F */
/* U+1820 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+182F */
/* U+1830 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+183F */
/* U+1840 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+184F */
/* U+1850 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+185F */
/* U+1860 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+186F */
/* U+1870 */	1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,   /* U+187F */
/* U+1880 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+188F */
/* U+1890 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+189F */
/* U+18A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,   /* U+18AF */
/* U+18B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+18BF */
/* U+18C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+18CF */
/* U+18D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+18DF */
/* U+18E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+18EF */
/* U+18F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+18FF */
/* U+1900 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+190F */
/* U+1910 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+191F */
/* U+1920 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+192F */
/* U+1930 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+193F */
/* U+1940 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+194F */
/* U+1950 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+195F */
/* U+1960 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+196F */
/* U+1970 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+197F */
/* U+1980 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+198F */
/* U+1990 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+199F */
/* U+19A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+19AF */
/* U+19B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+19BF */
/* U+19C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+19CF */
/* U+19D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+19DF */
/* U+19E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+19EF */
/* U+19F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+19FF */
/* U+1A00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A0F */
/* U+1A10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A1F */
/* U+1A20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A2F */
/* U+1A30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A3F */
/* U+1A40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A4F */
/* U+1A50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A5F */
/* U+1A60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A6F */
/* U+1A70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A7F */
/* U+1A80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A8F */
/* U+1A90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1A9F */
/* U+1AA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1AAF */
/* U+1AB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1ABF */
/* U+1AC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1ACF */
/* U+1AD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1ADF */
/* U+1AE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1AEF */
/* U+1AF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1AFF */
/* U+1B00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B0F */
/* U+1B10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B1F */
/* U+1B20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B2F */
/* U+1B30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B3F */
/* U+1B40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B4F */
/* U+1B50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B5F */
/* U+1B60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B6F */
/* U+1B70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B7F */
/* U+1B80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B8F */
/* U+1B90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1B9F */
/* U+1BA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1BAF */
/* U+1BB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1BBF */
/* U+1BC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1BCF */
/* U+1BD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1BDF */
/* U+1BE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1BEF */
/* U+1BF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1BFF */
/* U+1C00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C0F */
/* U+1C10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C1F */
/* U+1C20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C2F */
/* U+1C30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C3F */
/* U+1C40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C4F */
/* U+1C50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C5F */
/* U+1C60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C6F */
/* U+1C70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C7F */
/* U+1C80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C8F */
/* U+1C90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1C9F */
/* U+1CA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1CAF */
/* U+1CB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1CBF */
/* U+1CC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1CCF */
/* U+1CD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1CDF */
/* U+1CE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1CEF */
/* U+1CF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1CFF */
/* U+1D00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D0F */
/* U+1D10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D1F */
/* U+1D20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D2F */
/* U+1D30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D3F */
/* U+1D40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D4F */
/* U+1D50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D5F */
/* U+1D60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D6F */
/* U+1D70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D7F */
/* U+1D80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D8F */
/* U+1D90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1D9F */
/* U+1DA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1DAF */
/* U+1DB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1DBF */
/* U+1DC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1DCF */
/* U+1DD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1DDF */
/* U+1DE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1DEF */
/* U+1DF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+1DFF */
/* U+1E00 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1E0F */
/* U+1E10 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1E1F */
/* U+1E20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1E2F */
/* U+1E30 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1E3F */
/* U+1E40 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1E4F */
/* U+1E50 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1E5F */
/* U+1E60 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1E6F */
/* U+1E70 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1E7F */
/* U+1E80 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1E8F */
/* U+1E90 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,   /* U+1E9F */
/* U+1EA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1EAF */
/* U+1EB0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1EBF */
/* U+1EC0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1ECF */
/* U+1ED0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1EDF */
/* U+1EE0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1EEF */
/* U+1EF0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,   /* U+1EFF */
/* U+1F00 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1F0F */
/* U+1F10 */	1, 1, 1, 1, 1, 1, IL,IL,1, 1, 1, 1, 1, 1, IL,IL,   /* U+1F1F */
/* U+1F20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1F2F */
/* U+1F30 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1F3F */
/* U+1F40 */	1, 1, 1, 1, 1, 1, IL,IL,1, 1, 1, 1, 1, 1, IL,IL,   /* U+1F4F */
/* U+1F50 */	1, 1, 1, 1, 1, 1, 1, 1, IL,1, IL,1, IL,1, IL,1,    /* U+1F5F */
/* U+1F60 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1F6F */
/* U+1F70 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,   /* U+1F7F */
/* U+1F80 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1F8F */
/* U+1F90 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1F9F */
/* U+1FA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1FAF */
/* U+1FB0 */	1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1FBF */
/* U+1FC0 */	1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1FCF */
/* U+1FD0 */	1, 1, 1, 1, IL,IL,1, 1, 1, 1, 1, 1, IL,1, 1, 1,    /* U+1FDF */
/* U+1FE0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+1FEF */
/* U+1FF0 */	IL,IL,1, 1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+1FFF */
/* U+2000 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, IL,IL,IL,IL,   /* U+200F */
/* U+2010 */	2, 1, 1, 2, 2, 1, 2, 1, 2, 2, 1, 1, 2, 2, 1, 1,    /* U+201F */
/* U+2020 */	2, 2, 2, 1, 1, 2, 2, 1, 0, 0, IL,IL,IL,IL,IL,IL,   /* U+202F */
/* U+2030 */	2, 1, 2, 2, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1,    /* U+203F */
/* U+2040 */	1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+204F */
/* U+2050 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+205F */
/* U+2060 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+206F */
/* U+2070 */	1, IL,IL,IL,2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,    /* U+207F */
/* U+2080 */	1, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+208F */
/* U+2090 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+209F */
/* U+20A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,   /* U+20AF */
/* U+20B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+20BF */
/* U+20C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+20CF */
/* U+20D0 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    /* U+20DF */
/* U+20E0 */	0, 0, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+20EF */
/* U+20F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+20FF */
/* U+2100 */	1, 1, 1, 2, 1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1,    /* U+210F */
/* U+2110 */	1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+211F */
/* U+2120 */	1, 2, 2, 1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1,    /* U+212F */
/* U+2130 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,   /* U+213F */
/* U+2140 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+214F */
/* U+2150 */	IL,IL,IL,2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1,    /* U+215F */
/* U+2160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1,    /* U+216F */
/* U+2170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1,    /* U+217F */
/* U+2180 */	1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+218F */
/* U+2190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1,    /* U+219F */
/* U+21A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+21AF */
/* U+21B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+21BF */
/* U+21C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+21CF */
/* U+21D0 */	1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+21DF */
/* U+21E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+21EF */
/* U+21F0 */	1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+21FF */
/* U+2200 */	2, 1, 2, 2, 1, 1, 1, 2, 2, 1, 1, 2, 1, 1, 1, 2,    /* U+220F */
/* U+2210 */	1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 2, 2, 2,    /* U+221F */
/* U+2220 */	2, 1, 1, 2, 1, 2, 1, 2, 2, 2, 2, 2, 2, 1, 2, 1,    /* U+222F */
/* U+2230 */	1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1,    /* U+223F */
/* U+2240 */	1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1,    /* U+224F */
/* U+2250 */	1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+225F */
/* U+2260 */	2, 2, 1, 1, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2,    /* U+226F */
/* U+2270 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+227F */
/* U+2280 */	1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+228F */
/* U+2290 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1,    /* U+229F */
/* U+22A0 */	1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+22AF */
/* U+22B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,    /* U+22BF */
/* U+22C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+22CF */
/* U+22D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+22DF */
/* U+22E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+22EF */
/* U+22F0 */	1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+22FF */
/* U+2300 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+230F */
/* U+2310 */	1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+231F */
/* U+2320 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+232F */
/* U+2330 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+233F */
/* U+2340 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+234F */
/* U+2350 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+235F */
/* U+2360 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+236F */
/* U+2370 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1,    /* U+237F */
/* U+2380 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+238F */
/* U+2390 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,   /* U+239F */
/* U+23A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+23AF */
/* U+23B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+23BF */
/* U+23C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+23CF */
/* U+23D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+23DF */
/* U+23E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+23EF */
/* U+23F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+23FF */
/* U+2400 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+240F */
/* U+2410 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+241F */
/* U+2420 */	1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+242F */
/* U+2430 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+243F */
/* U+2440 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,   /* U+244F */
/* U+2450 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+245F */
/* U+2460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+246F */
/* U+2470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+247F */
/* U+2480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+248F */
/* U+2490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+249F */
/* U+24A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+24AF */
/* U+24B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+24BF */
/* U+24C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+24CF */
/* U+24D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+24DF */
/* U+24E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,IL,IL,   /* U+24EF */
/* U+24F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+24FF */
/* U+2500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+250F */
/* U+2510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+251F */
/* U+2520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+252F */
/* U+2530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+253F */
/* U+2540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+254F */
/* U+2550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+255F */
/* U+2560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+256F */
/* U+2570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+257F */
/* U+2580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+258F */
/* U+2590 */	2, 2, 2, 2, 2, 2, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+259F */
/* U+25A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+25AF */
/* U+25B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+25BF */
/* U+25C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+25CF */
/* U+25D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+25DF */
/* U+25E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+25EF */
/* U+25F0 */	1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,   /* U+25FF */
/* U+2600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+260F */
/* U+2610 */	2, 2, 2, 2, IL,IL,IL,IL,IL,1, 2, 2, 2, 2, 2, 2,    /* U+261F */
/* U+2620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+262F */
/* U+2630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+263F */
/* U+2640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+264F */
/* U+2650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+265F */
/* U+2660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+266F */
/* U+2670 */	1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+267F */
/* U+2680 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+268F */
/* U+2690 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+269F */
/* U+26A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+26AF */
/* U+26B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+26BF */
/* U+26C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+26CF */
/* U+26D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+26DF */
/* U+26E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+26EF */
/* U+26F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+26FF */
/* U+2700 */	IL,1, 1, 1, 1, IL,1, 1, 1, 1, IL,IL,1, 1, 1, 1,    /* U+270F */
/* U+2710 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+271F */
/* U+2720 */	1, 1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, 1, 1,    /* U+272F */
/* U+2730 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+273F */
/* U+2740 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,1, IL,1,    /* U+274F */
/* U+2750 */	1, 1, 1, IL,IL,IL,1, IL,1, 1, 1, 1, 1, 1, 1, IL,   /* U+275F */
/* U+2760 */	IL,1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,   /* U+276F */
/* U+2770 */	IL,IL,IL,IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+277F */
/* U+2780 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+278F */
/* U+2790 */	1, 1, 1, 1, 1, IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1,    /* U+279F */
/* U+27A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+27AF */
/* U+27B0 */	IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+27BF */
/* U+27C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+27CF */
/* U+27D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+27DF */
/* U+27E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+27EF */
/* U+27F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+27FF */
/* U+2800 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+280F */
/* U+2810 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+281F */
/* U+2820 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+282F */
/* U+2830 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+283F */
/* U+2840 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+284F */
/* U+2850 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+285F */
/* U+2860 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+286F */
/* U+2870 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+287F */
/* U+2880 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+288F */
/* U+2890 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+289F */
/* U+28A0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+28AF */
/* U+28B0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+28BF */
/* U+28C0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+28CF */
/* U+28D0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+28DF */
/* U+28E0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+28EF */
/* U+28F0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+28FF */
/* U+2900 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+290F */
/* U+2910 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+291F */
/* U+2920 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+292F */
/* U+2930 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+293F */
/* U+2940 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+294F */
/* U+2950 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+295F */
/* U+2960 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+296F */
/* U+2970 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+297F */
/* U+2980 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+298F */
/* U+2990 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+299F */
/* U+29A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+29AF */
/* U+29B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+29BF */
/* U+29C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+29CF */
/* U+29D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+29DF */
/* U+29E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+29EF */
/* U+29F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+29FF */
/* U+2A00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A0F */
/* U+2A10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A1F */
/* U+2A20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A2F */
/* U+2A30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A3F */
/* U+2A40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A4F */
/* U+2A50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A5F */
/* U+2A60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A6F */
/* U+2A70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A7F */
/* U+2A80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A8F */
/* U+2A90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2A9F */
/* U+2AA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2AAF */
/* U+2AB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2ABF */
/* U+2AC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2ACF */
/* U+2AD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2ADF */
/* U+2AE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2AEF */
/* U+2AF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2AFF */
/* U+2B00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B0F */
/* U+2B10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B1F */
/* U+2B20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B2F */
/* U+2B30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B3F */
/* U+2B40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B4F */
/* U+2B50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B5F */
/* U+2B60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B6F */
/* U+2B70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B7F */
/* U+2B80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B8F */
/* U+2B90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2B9F */
/* U+2BA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2BAF */
/* U+2BB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2BBF */
/* U+2BC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2BCF */
/* U+2BD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2BDF */
/* U+2BE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2BEF */
/* U+2BF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2BFF */
/* U+2C00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C0F */
/* U+2C10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C1F */
/* U+2C20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C2F */
/* U+2C30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C3F */
/* U+2C40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C4F */
/* U+2C50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C5F */
/* U+2C60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C6F */
/* U+2C70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C7F */
/* U+2C80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C8F */
/* U+2C90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2C9F */
/* U+2CA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2CAF */
/* U+2CB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2CBF */
/* U+2CC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2CCF */
/* U+2CD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2CDF */
/* U+2CE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2CEF */
/* U+2CF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2CFF */
/* U+2D00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D0F */
/* U+2D10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D1F */
/* U+2D20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D2F */
/* U+2D30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D3F */
/* U+2D40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D4F */
/* U+2D50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D5F */
/* U+2D60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D6F */
/* U+2D70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D7F */
/* U+2D80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D8F */
/* U+2D90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2D9F */
/* U+2DA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2DAF */
/* U+2DB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2DBF */
/* U+2DC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2DCF */
/* U+2DD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2DDF */
/* U+2DE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2DEF */
/* U+2DF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2DFF */
/* U+2E00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2E0F */
/* U+2E10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2E1F */
/* U+2E20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2E2F */
/* U+2E30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2E3F */
/* U+2E40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2E4F */
/* U+2E50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2E5F */
/* U+2E60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2E6F */
/* U+2E70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2E7F */
/* U+2E80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2E8F */
/* U+2E90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,2, 2, 2, 2, 2,    /* U+2E9F */
/* U+2EA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2EAF */
/* U+2EB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2EBF */
/* U+2EC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2ECF */
/* U+2ED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2EDF */
/* U+2EE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2EEF */
/* U+2EF0 */	2, 2, 2, 2, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2EFF */
/* U+2F00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F0F */
/* U+2F10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F1F */
/* U+2F20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F2F */
/* U+2F30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F3F */
/* U+2F40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F4F */
/* U+2F50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F5F */
/* U+2F60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F6F */
/* U+2F70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F7F */
/* U+2F80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F8F */
/* U+2F90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2F9F */
/* U+2FA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2FAF */
/* U+2FB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2FBF */
/* U+2FC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+2FCF */
/* U+2FD0 */	2, 2, 2, 2, 2, 2, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2FDF */
/* U+2FE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+2FEF */
/* U+2FF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,IL,   /* U+2FFF */
/* U+3000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+300F */
/* U+3010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+301F */
/* U+3020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0,    /* U+302F */
/* U+3030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,2, 1,    /* U+303F */
/* U+3040 */	IL,2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+304F */
/* U+3050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+305F */
/* U+3060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+306F */
/* U+3070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+307F */
/* U+3080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+308F */
/* U+3090 */	2, 2, 2, 2, 2, IL,IL,IL,IL,0, 0, 2, 2, 2, 2, IL,   /* U+309F */
/* U+30A0 */	IL,2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+30AF */
/* U+30B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+30BF */
/* U+30C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+30CF */
/* U+30D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+30DF */
/* U+30E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+30EF */
/* U+30F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,   /* U+30FF */
/* U+3100 */	IL,IL,IL,IL,IL,2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+310F */
/* U+3110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+311F */
/* U+3120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,   /* U+312F */
/* U+3130 */	IL,2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+313F */
/* U+3140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+314F */
/* U+3150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+315F */
/* U+3160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+316F */
/* U+3170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+317F */
/* U+3180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,   /* U+318F */
/* U+3190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+319F */
/* U+31A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+31AF */
/* U+31B0 */	2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,IL,IL,IL,IL,IL,   /* U+31BF */
/* U+31C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+31CF */
/* U+31D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+31DF */
/* U+31E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+31EF */
/* U+31F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+31FF */
/* U+3200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+320F */
/* U+3210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,   /* U+321F */
/* U+3220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+322F */
/* U+3230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+323F */
/* U+3240 */	2, 2, 2, 2, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+324F */
/* U+3250 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+325F */
/* U+3260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+326F */
/* U+3270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,2,    /* U+327F */
/* U+3280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+328F */
/* U+3290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+329F */
/* U+32A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+32AF */
/* U+32B0 */	2, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+32BF */
/* U+32C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,IL,   /* U+32CF */
/* U+32D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+32DF */
/* U+32E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+32EF */
/* U+32F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,   /* U+32FF */
/* U+3300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+330F */
/* U+3310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+331F */
/* U+3320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+332F */
/* U+3330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+333F */
/* U+3340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+334F */
/* U+3350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+335F */
/* U+3360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+336F */
/* U+3370 */	2, 2, 2, 2, 2, 2, 2, IL,IL,IL,IL,2, 2, 2, 2, 2,    /* U+337F */
/* U+3380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+338F */
/* U+3390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+339F */
/* U+33A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+33AF */
/* U+33B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+33BF */
/* U+33C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+33CF */
/* U+33D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,   /* U+33DF */
/* U+33E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+33EF */
/* U+33F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,   /* U+33FF */
/* U+3400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+340F */
/* U+3410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+341F */
/* U+3420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+342F */
/* U+3430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+343F */
/* U+3440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+344F */
/* U+3450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+345F */
/* U+3460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+346F */
/* U+3470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+347F */
/* U+3480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+348F */
/* U+3490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+349F */
/* U+34A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+34AF */
/* U+34B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+34BF */
/* U+34C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+34CF */
/* U+34D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+34DF */
/* U+34E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+34EF */
/* U+34F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+34FF */
/* U+3500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+350F */
/* U+3510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+351F */
/* U+3520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+352F */
/* U+3530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+353F */
/* U+3540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+354F */
/* U+3550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+355F */
/* U+3560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+356F */
/* U+3570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+357F */
/* U+3580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+358F */
/* U+3590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+359F */
/* U+35A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+35AF */
/* U+35B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+35BF */
/* U+35C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+35CF */
/* U+35D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+35DF */
/* U+35E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+35EF */
/* U+35F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+35FF */
/* U+3600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+360F */
/* U+3610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+361F */
/* U+3620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+362F */
/* U+3630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+363F */
/* U+3640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+364F */
/* U+3650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+365F */
/* U+3660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+366F */
/* U+3670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+367F */
/* U+3680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+368F */
/* U+3690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+369F */
/* U+36A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+36AF */
/* U+36B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+36BF */
/* U+36C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+36CF */
/* U+36D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+36DF */
/* U+36E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+36EF */
/* U+36F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+36FF */
/* U+3700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+370F */
/* U+3710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+371F */
/* U+3720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+372F */
/* U+3730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+373F */
/* U+3740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+374F */
/* U+3750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+375F */
/* U+3760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+376F */
/* U+3770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+377F */
/* U+3780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+378F */
/* U+3790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+379F */
/* U+37A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+37AF */
/* U+37B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+37BF */
/* U+37C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+37CF */
/* U+37D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+37DF */
/* U+37E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+37EF */
/* U+37F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+37FF */
/* U+3800 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+380F */
/* U+3810 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+381F */
/* U+3820 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+382F */
/* U+3830 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+383F */
/* U+3840 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+384F */
/* U+3850 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+385F */
/* U+3860 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+386F */
/* U+3870 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+387F */
/* U+3880 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+388F */
/* U+3890 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+389F */
/* U+38A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+38AF */
/* U+38B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+38BF */
/* U+38C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+38CF */
/* U+38D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+38DF */
/* U+38E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+38EF */
/* U+38F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+38FF */
/* U+3900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+390F */
/* U+3910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+391F */
/* U+3920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+392F */
/* U+3930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+393F */
/* U+3940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+394F */
/* U+3950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+395F */
/* U+3960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+396F */
/* U+3970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+397F */
/* U+3980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+398F */
/* U+3990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+399F */
/* U+39A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+39AF */
/* U+39B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+39BF */
/* U+39C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+39CF */
/* U+39D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+39DF */
/* U+39E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+39EF */
/* U+39F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+39FF */
/* U+3A00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A0F */
/* U+3A10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A1F */
/* U+3A20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A2F */
/* U+3A30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A3F */
/* U+3A40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A4F */
/* U+3A50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A5F */
/* U+3A60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A6F */
/* U+3A70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A7F */
/* U+3A80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A8F */
/* U+3A90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3A9F */
/* U+3AA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3AAF */
/* U+3AB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3ABF */
/* U+3AC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3ACF */
/* U+3AD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3ADF */
/* U+3AE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3AEF */
/* U+3AF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3AFF */
/* U+3B00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B0F */
/* U+3B10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B1F */
/* U+3B20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B2F */
/* U+3B30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B3F */
/* U+3B40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B4F */
/* U+3B50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B5F */
/* U+3B60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B6F */
/* U+3B70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B7F */
/* U+3B80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B8F */
/* U+3B90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3B9F */
/* U+3BA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3BAF */
/* U+3BB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3BBF */
/* U+3BC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3BCF */
/* U+3BD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3BDF */
/* U+3BE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3BEF */
/* U+3BF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3BFF */
/* U+3C00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C0F */
/* U+3C10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C1F */
/* U+3C20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C2F */
/* U+3C30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C3F */
/* U+3C40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C4F */
/* U+3C50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C5F */
/* U+3C60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C6F */
/* U+3C70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C7F */
/* U+3C80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C8F */
/* U+3C90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3C9F */
/* U+3CA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3CAF */
/* U+3CB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3CBF */
/* U+3CC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3CCF */
/* U+3CD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3CDF */
/* U+3CE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3CEF */
/* U+3CF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3CFF */
/* U+3D00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D0F */
/* U+3D10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D1F */
/* U+3D20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D2F */
/* U+3D30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D3F */
/* U+3D40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D4F */
/* U+3D50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D5F */
/* U+3D60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D6F */
/* U+3D70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D7F */
/* U+3D80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D8F */
/* U+3D90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3D9F */
/* U+3DA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3DAF */
/* U+3DB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3DBF */
/* U+3DC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3DCF */
/* U+3DD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3DDF */
/* U+3DE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3DEF */
/* U+3DF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3DFF */
/* U+3E00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E0F */
/* U+3E10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E1F */
/* U+3E20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E2F */
/* U+3E30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E3F */
/* U+3E40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E4F */
/* U+3E50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E5F */
/* U+3E60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E6F */
/* U+3E70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E7F */
/* U+3E80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E8F */
/* U+3E90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3E9F */
/* U+3EA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3EAF */
/* U+3EB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3EBF */
/* U+3EC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3ECF */
/* U+3ED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3EDF */
/* U+3EE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3EEF */
/* U+3EF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3EFF */
/* U+3F00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F0F */
/* U+3F10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F1F */
/* U+3F20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F2F */
/* U+3F30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F3F */
/* U+3F40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F4F */
/* U+3F50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F5F */
/* U+3F60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F6F */
/* U+3F70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F7F */
/* U+3F80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F8F */
/* U+3F90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3F9F */
/* U+3FA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3FAF */
/* U+3FB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3FBF */
/* U+3FC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3FCF */
/* U+3FD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3FDF */
/* U+3FE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3FEF */
/* U+3FF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+3FFF */
/* U+4000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+400F */
/* U+4010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+401F */
/* U+4020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+402F */
/* U+4030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+403F */
/* U+4040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+404F */
/* U+4050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+405F */
/* U+4060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+406F */
/* U+4070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+407F */
/* U+4080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+408F */
/* U+4090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+409F */
/* U+40A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+40AF */
/* U+40B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+40BF */
/* U+40C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+40CF */
/* U+40D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+40DF */
/* U+40E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+40EF */
/* U+40F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+40FF */
/* U+4100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+410F */
/* U+4110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+411F */
/* U+4120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+412F */
/* U+4130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+413F */
/* U+4140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+414F */
/* U+4150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+415F */
/* U+4160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+416F */
/* U+4170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+417F */
/* U+4180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+418F */
/* U+4190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+419F */
/* U+41A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+41AF */
/* U+41B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+41BF */
/* U+41C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+41CF */
/* U+41D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+41DF */
/* U+41E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+41EF */
/* U+41F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+41FF */
/* U+4200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+420F */
/* U+4210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+421F */
/* U+4220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+422F */
/* U+4230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+423F */
/* U+4240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+424F */
/* U+4250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+425F */
/* U+4260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+426F */
/* U+4270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+427F */
/* U+4280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+428F */
/* U+4290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+429F */
/* U+42A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+42AF */
/* U+42B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+42BF */
/* U+42C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+42CF */
/* U+42D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+42DF */
/* U+42E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+42EF */
/* U+42F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+42FF */
/* U+4300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+430F */
/* U+4310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+431F */
/* U+4320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+432F */
/* U+4330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+433F */
/* U+4340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+434F */
/* U+4350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+435F */
/* U+4360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+436F */
/* U+4370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+437F */
/* U+4380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+438F */
/* U+4390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+439F */
/* U+43A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+43AF */
/* U+43B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+43BF */
/* U+43C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+43CF */
/* U+43D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+43DF */
/* U+43E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+43EF */
/* U+43F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+43FF */
/* U+4400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+440F */
/* U+4410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+441F */
/* U+4420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+442F */
/* U+4430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+443F */
/* U+4440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+444F */
/* U+4450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+445F */
/* U+4460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+446F */
/* U+4470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+447F */
/* U+4480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+448F */
/* U+4490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+449F */
/* U+44A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+44AF */
/* U+44B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+44BF */
/* U+44C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+44CF */
/* U+44D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+44DF */
/* U+44E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+44EF */
/* U+44F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+44FF */
/* U+4500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+450F */
/* U+4510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+451F */
/* U+4520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+452F */
/* U+4530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+453F */
/* U+4540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+454F */
/* U+4550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+455F */
/* U+4560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+456F */
/* U+4570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+457F */
/* U+4580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+458F */
/* U+4590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+459F */
/* U+45A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+45AF */
/* U+45B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+45BF */
/* U+45C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+45CF */
/* U+45D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+45DF */
/* U+45E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+45EF */
/* U+45F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+45FF */
/* U+4600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+460F */
/* U+4610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+461F */
/* U+4620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+462F */
/* U+4630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+463F */
/* U+4640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+464F */
/* U+4650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+465F */
/* U+4660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+466F */
/* U+4670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+467F */
/* U+4680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+468F */
/* U+4690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+469F */
/* U+46A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+46AF */
/* U+46B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+46BF */
/* U+46C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+46CF */
/* U+46D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+46DF */
/* U+46E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+46EF */
/* U+46F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+46FF */
/* U+4700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+470F */
/* U+4710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+471F */
/* U+4720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+472F */
/* U+4730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+473F */
/* U+4740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+474F */
/* U+4750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+475F */
/* U+4760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+476F */
/* U+4770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+477F */
/* U+4780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+478F */
/* U+4790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+479F */
/* U+47A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+47AF */
/* U+47B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+47BF */
/* U+47C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+47CF */
/* U+47D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+47DF */
/* U+47E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+47EF */
/* U+47F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+47FF */
/* U+4800 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+480F */
/* U+4810 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+481F */
/* U+4820 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+482F */
/* U+4830 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+483F */
/* U+4840 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+484F */
/* U+4850 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+485F */
/* U+4860 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+486F */
/* U+4870 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+487F */
/* U+4880 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+488F */
/* U+4890 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+489F */
/* U+48A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+48AF */
/* U+48B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+48BF */
/* U+48C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+48CF */
/* U+48D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+48DF */
/* U+48E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+48EF */
/* U+48F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+48FF */
/* U+4900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+490F */
/* U+4910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+491F */
/* U+4920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+492F */
/* U+4930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+493F */
/* U+4940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+494F */
/* U+4950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+495F */
/* U+4960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+496F */
/* U+4970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+497F */
/* U+4980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+498F */
/* U+4990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+499F */
/* U+49A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+49AF */
/* U+49B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+49BF */
/* U+49C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+49CF */
/* U+49D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+49DF */
/* U+49E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+49EF */
/* U+49F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+49FF */
/* U+4A00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A0F */
/* U+4A10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A1F */
/* U+4A20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A2F */
/* U+4A30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A3F */
/* U+4A40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A4F */
/* U+4A50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A5F */
/* U+4A60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A6F */
/* U+4A70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A7F */
/* U+4A80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A8F */
/* U+4A90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4A9F */
/* U+4AA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4AAF */
/* U+4AB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4ABF */
/* U+4AC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4ACF */
/* U+4AD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4ADF */
/* U+4AE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4AEF */
/* U+4AF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4AFF */
/* U+4B00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B0F */
/* U+4B10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B1F */
/* U+4B20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B2F */
/* U+4B30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B3F */
/* U+4B40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B4F */
/* U+4B50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B5F */
/* U+4B60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B6F */
/* U+4B70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B7F */
/* U+4B80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B8F */
/* U+4B90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4B9F */
/* U+4BA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4BAF */
/* U+4BB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4BBF */
/* U+4BC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4BCF */
/* U+4BD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4BDF */
/* U+4BE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4BEF */
/* U+4BF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4BFF */
/* U+4C00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C0F */
/* U+4C10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C1F */
/* U+4C20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C2F */
/* U+4C30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C3F */
/* U+4C40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C4F */
/* U+4C50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C5F */
/* U+4C60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C6F */
/* U+4C70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C7F */
/* U+4C80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C8F */
/* U+4C90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4C9F */
/* U+4CA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4CAF */
/* U+4CB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4CBF */
/* U+4CC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4CCF */
/* U+4CD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4CDF */
/* U+4CE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4CEF */
/* U+4CF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4CFF */
/* U+4D00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D0F */
/* U+4D10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D1F */
/* U+4D20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D2F */
/* U+4D30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D3F */
/* U+4D40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D4F */
/* U+4D50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D5F */
/* U+4D60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D6F */
/* U+4D70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D7F */
/* U+4D80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D8F */
/* U+4D90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4D9F */
/* U+4DA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4DAF */
/* U+4DB0 */	2, 2, 2, 2, 2, 2, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+4DBF */
/* U+4DC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+4DCF */
/* U+4DD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+4DDF */
/* U+4DE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+4DEF */
/* U+4DF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+4DFF */
/* U+4E00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E0F */
/* U+4E10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E1F */
/* U+4E20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E2F */
/* U+4E30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E3F */
/* U+4E40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E4F */
/* U+4E50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E5F */
/* U+4E60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E6F */
/* U+4E70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E7F */
/* U+4E80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E8F */
/* U+4E90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4E9F */
/* U+4EA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4EAF */
/* U+4EB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4EBF */
/* U+4EC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4ECF */
/* U+4ED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4EDF */
/* U+4EE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4EEF */
/* U+4EF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4EFF */
/* U+4F00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F0F */
/* U+4F10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F1F */
/* U+4F20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F2F */
/* U+4F30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F3F */
/* U+4F40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F4F */
/* U+4F50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F5F */
/* U+4F60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F6F */
/* U+4F70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F7F */
/* U+4F80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F8F */
/* U+4F90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4F9F */
/* U+4FA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4FAF */
/* U+4FB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4FBF */
/* U+4FC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4FCF */
/* U+4FD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4FDF */
/* U+4FE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4FEF */
/* U+4FF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+4FFF */
/* U+5000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+500F */
/* U+5010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+501F */
/* U+5020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+502F */
/* U+5030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+503F */
/* U+5040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+504F */
/* U+5050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+505F */
/* U+5060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+506F */
/* U+5070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+507F */
/* U+5080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+508F */
/* U+5090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+509F */
/* U+50A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+50AF */
/* U+50B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+50BF */
/* U+50C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+50CF */
/* U+50D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+50DF */
/* U+50E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+50EF */
/* U+50F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+50FF */
/* U+5100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+510F */
/* U+5110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+511F */
/* U+5120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+512F */
/* U+5130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+513F */
/* U+5140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+514F */
/* U+5150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+515F */
/* U+5160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+516F */
/* U+5170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+517F */
/* U+5180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+518F */
/* U+5190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+519F */
/* U+51A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+51AF */
/* U+51B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+51BF */
/* U+51C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+51CF */
/* U+51D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+51DF */
/* U+51E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+51EF */
/* U+51F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+51FF */
/* U+5200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+520F */
/* U+5210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+521F */
/* U+5220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+522F */
/* U+5230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+523F */
/* U+5240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+524F */
/* U+5250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+525F */
/* U+5260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+526F */
/* U+5270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+527F */
/* U+5280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+528F */
/* U+5290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+529F */
/* U+52A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+52AF */
/* U+52B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+52BF */
/* U+52C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+52CF */
/* U+52D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+52DF */
/* U+52E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+52EF */
/* U+52F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+52FF */
/* U+5300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+530F */
/* U+5310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+531F */
/* U+5320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+532F */
/* U+5330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+533F */
/* U+5340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+534F */
/* U+5350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+535F */
/* U+5360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+536F */
/* U+5370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+537F */
/* U+5380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+538F */
/* U+5390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+539F */
/* U+53A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+53AF */
/* U+53B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+53BF */
/* U+53C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+53CF */
/* U+53D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+53DF */
/* U+53E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+53EF */
/* U+53F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+53FF */
/* U+5400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+540F */
/* U+5410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+541F */
/* U+5420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+542F */
/* U+5430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+543F */
/* U+5440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+544F */
/* U+5450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+545F */
/* U+5460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+546F */
/* U+5470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+547F */
/* U+5480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+548F */
/* U+5490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+549F */
/* U+54A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+54AF */
/* U+54B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+54BF */
/* U+54C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+54CF */
/* U+54D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+54DF */
/* U+54E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+54EF */
/* U+54F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+54FF */
/* U+5500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+550F */
/* U+5510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+551F */
/* U+5520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+552F */
/* U+5530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+553F */
/* U+5540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+554F */
/* U+5550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+555F */
/* U+5560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+556F */
/* U+5570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+557F */
/* U+5580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+558F */
/* U+5590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+559F */
/* U+55A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+55AF */
/* U+55B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+55BF */
/* U+55C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+55CF */
/* U+55D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+55DF */
/* U+55E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+55EF */
/* U+55F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+55FF */
/* U+5600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+560F */
/* U+5610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+561F */
/* U+5620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+562F */
/* U+5630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+563F */
/* U+5640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+564F */
/* U+5650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+565F */
/* U+5660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+566F */
/* U+5670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+567F */
/* U+5680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+568F */
/* U+5690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+569F */
/* U+56A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+56AF */
/* U+56B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+56BF */
/* U+56C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+56CF */
/* U+56D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+56DF */
/* U+56E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+56EF */
/* U+56F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+56FF */
/* U+5700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+570F */
/* U+5710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+571F */
/* U+5720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+572F */
/* U+5730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+573F */
/* U+5740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+574F */
/* U+5750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+575F */
/* U+5760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+576F */
/* U+5770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+577F */
/* U+5780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+578F */
/* U+5790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+579F */
/* U+57A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+57AF */
/* U+57B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+57BF */
/* U+57C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+57CF */
/* U+57D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+57DF */
/* U+57E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+57EF */
/* U+57F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+57FF */
/* U+5800 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+580F */
/* U+5810 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+581F */
/* U+5820 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+582F */
/* U+5830 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+583F */
/* U+5840 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+584F */
/* U+5850 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+585F */
/* U+5860 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+586F */
/* U+5870 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+587F */
/* U+5880 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+588F */
/* U+5890 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+589F */
/* U+58A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+58AF */
/* U+58B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+58BF */
/* U+58C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+58CF */
/* U+58D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+58DF */
/* U+58E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+58EF */
/* U+58F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+58FF */
/* U+5900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+590F */
/* U+5910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+591F */
/* U+5920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+592F */
/* U+5930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+593F */
/* U+5940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+594F */
/* U+5950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+595F */
/* U+5960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+596F */
/* U+5970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+597F */
/* U+5980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+598F */
/* U+5990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+599F */
/* U+59A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+59AF */
/* U+59B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+59BF */
/* U+59C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+59CF */
/* U+59D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+59DF */
/* U+59E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+59EF */
/* U+59F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+59FF */
/* U+5A00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A0F */
/* U+5A10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A1F */
/* U+5A20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A2F */
/* U+5A30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A3F */
/* U+5A40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A4F */
/* U+5A50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A5F */
/* U+5A60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A6F */
/* U+5A70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A7F */
/* U+5A80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A8F */
/* U+5A90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5A9F */
/* U+5AA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5AAF */
/* U+5AB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5ABF */
/* U+5AC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5ACF */
/* U+5AD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5ADF */
/* U+5AE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5AEF */
/* U+5AF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5AFF */
/* U+5B00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B0F */
/* U+5B10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B1F */
/* U+5B20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B2F */
/* U+5B30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B3F */
/* U+5B40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B4F */
/* U+5B50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B5F */
/* U+5B60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B6F */
/* U+5B70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B7F */
/* U+5B80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B8F */
/* U+5B90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5B9F */
/* U+5BA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5BAF */
/* U+5BB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5BBF */
/* U+5BC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5BCF */
/* U+5BD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5BDF */
/* U+5BE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5BEF */
/* U+5BF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5BFF */
/* U+5C00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C0F */
/* U+5C10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C1F */
/* U+5C20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C2F */
/* U+5C30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C3F */
/* U+5C40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C4F */
/* U+5C50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C5F */
/* U+5C60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C6F */
/* U+5C70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C7F */
/* U+5C80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C8F */
/* U+5C90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5C9F */
/* U+5CA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5CAF */
/* U+5CB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5CBF */
/* U+5CC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5CCF */
/* U+5CD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5CDF */
/* U+5CE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5CEF */
/* U+5CF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5CFF */
/* U+5D00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D0F */
/* U+5D10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D1F */
/* U+5D20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D2F */
/* U+5D30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D3F */
/* U+5D40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D4F */
/* U+5D50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D5F */
/* U+5D60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D6F */
/* U+5D70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D7F */
/* U+5D80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D8F */
/* U+5D90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5D9F */
/* U+5DA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5DAF */
/* U+5DB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5DBF */
/* U+5DC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5DCF */
/* U+5DD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5DDF */
/* U+5DE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5DEF */
/* U+5DF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5DFF */
/* U+5E00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E0F */
/* U+5E10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E1F */
/* U+5E20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E2F */
/* U+5E30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E3F */
/* U+5E40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E4F */
/* U+5E50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E5F */
/* U+5E60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E6F */
/* U+5E70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E7F */
/* U+5E80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E8F */
/* U+5E90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5E9F */
/* U+5EA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5EAF */
/* U+5EB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5EBF */
/* U+5EC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5ECF */
/* U+5ED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5EDF */
/* U+5EE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5EEF */
/* U+5EF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5EFF */
/* U+5F00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F0F */
/* U+5F10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F1F */
/* U+5F20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F2F */
/* U+5F30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F3F */
/* U+5F40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F4F */
/* U+5F50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F5F */
/* U+5F60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F6F */
/* U+5F70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F7F */
/* U+5F80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F8F */
/* U+5F90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5F9F */
/* U+5FA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5FAF */
/* U+5FB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5FBF */
/* U+5FC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5FCF */
/* U+5FD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5FDF */
/* U+5FE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5FEF */
/* U+5FF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+5FFF */
/* U+6000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+600F */
/* U+6010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+601F */
/* U+6020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+602F */
/* U+6030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+603F */
/* U+6040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+604F */
/* U+6050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+605F */
/* U+6060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+606F */
/* U+6070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+607F */
/* U+6080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+608F */
/* U+6090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+609F */
/* U+60A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+60AF */
/* U+60B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+60BF */
/* U+60C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+60CF */
/* U+60D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+60DF */
/* U+60E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+60EF */
/* U+60F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+60FF */
/* U+6100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+610F */
/* U+6110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+611F */
/* U+6120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+612F */
/* U+6130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+613F */
/* U+6140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+614F */
/* U+6150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+615F */
/* U+6160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+616F */
/* U+6170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+617F */
/* U+6180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+618F */
/* U+6190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+619F */
/* U+61A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+61AF */
/* U+61B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+61BF */
/* U+61C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+61CF */
/* U+61D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+61DF */
/* U+61E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+61EF */
/* U+61F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+61FF */
/* U+6200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+620F */
/* U+6210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+621F */
/* U+6220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+622F */
/* U+6230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+623F */
/* U+6240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+624F */
/* U+6250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+625F */
/* U+6260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+626F */
/* U+6270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+627F */
/* U+6280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+628F */
/* U+6290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+629F */
/* U+62A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+62AF */
/* U+62B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+62BF */
/* U+62C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+62CF */
/* U+62D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+62DF */
/* U+62E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+62EF */
/* U+62F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+62FF */
/* U+6300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+630F */
/* U+6310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+631F */
/* U+6320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+632F */
/* U+6330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+633F */
/* U+6340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+634F */
/* U+6350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+635F */
/* U+6360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+636F */
/* U+6370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+637F */
/* U+6380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+638F */
/* U+6390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+639F */
/* U+63A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+63AF */
/* U+63B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+63BF */
/* U+63C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+63CF */
/* U+63D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+63DF */
/* U+63E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+63EF */
/* U+63F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+63FF */
/* U+6400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+640F */
/* U+6410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+641F */
/* U+6420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+642F */
/* U+6430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+643F */
/* U+6440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+644F */
/* U+6450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+645F */
/* U+6460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+646F */
/* U+6470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+647F */
/* U+6480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+648F */
/* U+6490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+649F */
/* U+64A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+64AF */
/* U+64B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+64BF */
/* U+64C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+64CF */
/* U+64D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+64DF */
/* U+64E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+64EF */
/* U+64F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+64FF */
/* U+6500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+650F */
/* U+6510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+651F */
/* U+6520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+652F */
/* U+6530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+653F */
/* U+6540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+654F */
/* U+6550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+655F */
/* U+6560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+656F */
/* U+6570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+657F */
/* U+6580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+658F */
/* U+6590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+659F */
/* U+65A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+65AF */
/* U+65B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+65BF */
/* U+65C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+65CF */
/* U+65D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+65DF */
/* U+65E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+65EF */
/* U+65F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+65FF */
/* U+6600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+660F */
/* U+6610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+661F */
/* U+6620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+662F */
/* U+6630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+663F */
/* U+6640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+664F */
/* U+6650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+665F */
/* U+6660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+666F */
/* U+6670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+667F */
/* U+6680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+668F */
/* U+6690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+669F */
/* U+66A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+66AF */
/* U+66B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+66BF */
/* U+66C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+66CF */
/* U+66D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+66DF */
/* U+66E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+66EF */
/* U+66F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+66FF */
/* U+6700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+670F */
/* U+6710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+671F */
/* U+6720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+672F */
/* U+6730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+673F */
/* U+6740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+674F */
/* U+6750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+675F */
/* U+6760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+676F */
/* U+6770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+677F */
/* U+6780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+678F */
/* U+6790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+679F */
/* U+67A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+67AF */
/* U+67B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+67BF */
/* U+67C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+67CF */
/* U+67D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+67DF */
/* U+67E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+67EF */
/* U+67F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+67FF */
/* U+6800 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+680F */
/* U+6810 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+681F */
/* U+6820 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+682F */
/* U+6830 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+683F */
/* U+6840 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+684F */
/* U+6850 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+685F */
/* U+6860 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+686F */
/* U+6870 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+687F */
/* U+6880 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+688F */
/* U+6890 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+689F */
/* U+68A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+68AF */
/* U+68B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+68BF */
/* U+68C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+68CF */
/* U+68D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+68DF */
/* U+68E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+68EF */
/* U+68F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+68FF */
/* U+6900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+690F */
/* U+6910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+691F */
/* U+6920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+692F */
/* U+6930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+693F */
/* U+6940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+694F */
/* U+6950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+695F */
/* U+6960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+696F */
/* U+6970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+697F */
/* U+6980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+698F */
/* U+6990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+699F */
/* U+69A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+69AF */
/* U+69B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+69BF */
/* U+69C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+69CF */
/* U+69D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+69DF */
/* U+69E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+69EF */
/* U+69F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+69FF */
/* U+6A00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A0F */
/* U+6A10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A1F */
/* U+6A20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A2F */
/* U+6A30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A3F */
/* U+6A40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A4F */
/* U+6A50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A5F */
/* U+6A60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A6F */
/* U+6A70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A7F */
/* U+6A80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A8F */
/* U+6A90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6A9F */
/* U+6AA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6AAF */
/* U+6AB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6ABF */
/* U+6AC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6ACF */
/* U+6AD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6ADF */
/* U+6AE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6AEF */
/* U+6AF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6AFF */
/* U+6B00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B0F */
/* U+6B10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B1F */
/* U+6B20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B2F */
/* U+6B30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B3F */
/* U+6B40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B4F */
/* U+6B50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B5F */
/* U+6B60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B6F */
/* U+6B70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B7F */
/* U+6B80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B8F */
/* U+6B90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6B9F */
/* U+6BA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6BAF */
/* U+6BB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6BBF */
/* U+6BC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6BCF */
/* U+6BD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6BDF */
/* U+6BE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6BEF */
/* U+6BF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6BFF */
/* U+6C00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C0F */
/* U+6C10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C1F */
/* U+6C20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C2F */
/* U+6C30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C3F */
/* U+6C40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C4F */
/* U+6C50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C5F */
/* U+6C60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C6F */
/* U+6C70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C7F */
/* U+6C80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C8F */
/* U+6C90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6C9F */
/* U+6CA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6CAF */
/* U+6CB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6CBF */
/* U+6CC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6CCF */
/* U+6CD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6CDF */
/* U+6CE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6CEF */
/* U+6CF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6CFF */
/* U+6D00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D0F */
/* U+6D10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D1F */
/* U+6D20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D2F */
/* U+6D30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D3F */
/* U+6D40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D4F */
/* U+6D50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D5F */
/* U+6D60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D6F */
/* U+6D70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D7F */
/* U+6D80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D8F */
/* U+6D90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6D9F */
/* U+6DA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6DAF */
/* U+6DB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6DBF */
/* U+6DC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6DCF */
/* U+6DD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6DDF */
/* U+6DE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6DEF */
/* U+6DF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6DFF */
/* U+6E00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E0F */
/* U+6E10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E1F */
/* U+6E20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E2F */
/* U+6E30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E3F */
/* U+6E40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E4F */
/* U+6E50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E5F */
/* U+6E60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E6F */
/* U+6E70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E7F */
/* U+6E80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E8F */
/* U+6E90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6E9F */
/* U+6EA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6EAF */
/* U+6EB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6EBF */
/* U+6EC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6ECF */
/* U+6ED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6EDF */
/* U+6EE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6EEF */
/* U+6EF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6EFF */
/* U+6F00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F0F */
/* U+6F10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F1F */
/* U+6F20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F2F */
/* U+6F30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F3F */
/* U+6F40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F4F */
/* U+6F50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F5F */
/* U+6F60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F6F */
/* U+6F70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F7F */
/* U+6F80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F8F */
/* U+6F90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6F9F */
/* U+6FA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6FAF */
/* U+6FB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6FBF */
/* U+6FC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6FCF */
/* U+6FD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6FDF */
/* U+6FE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6FEF */
/* U+6FF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+6FFF */
/* U+7000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+700F */
/* U+7010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+701F */
/* U+7020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+702F */
/* U+7030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+703F */
/* U+7040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+704F */
/* U+7050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+705F */
/* U+7060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+706F */
/* U+7070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+707F */
/* U+7080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+708F */
/* U+7090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+709F */
/* U+70A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+70AF */
/* U+70B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+70BF */
/* U+70C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+70CF */
/* U+70D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+70DF */
/* U+70E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+70EF */
/* U+70F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+70FF */
/* U+7100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+710F */
/* U+7110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+711F */
/* U+7120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+712F */
/* U+7130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+713F */
/* U+7140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+714F */
/* U+7150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+715F */
/* U+7160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+716F */
/* U+7170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+717F */
/* U+7180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+718F */
/* U+7190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+719F */
/* U+71A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+71AF */
/* U+71B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+71BF */
/* U+71C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+71CF */
/* U+71D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+71DF */
/* U+71E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+71EF */
/* U+71F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+71FF */
/* U+7200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+720F */
/* U+7210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+721F */
/* U+7220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+722F */
/* U+7230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+723F */
/* U+7240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+724F */
/* U+7250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+725F */
/* U+7260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+726F */
/* U+7270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+727F */
/* U+7280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+728F */
/* U+7290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+729F */
/* U+72A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+72AF */
/* U+72B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+72BF */
/* U+72C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+72CF */
/* U+72D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+72DF */
/* U+72E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+72EF */
/* U+72F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+72FF */
/* U+7300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+730F */
/* U+7310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+731F */
/* U+7320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+732F */
/* U+7330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+733F */
/* U+7340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+734F */
/* U+7350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+735F */
/* U+7360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+736F */
/* U+7370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+737F */
/* U+7380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+738F */
/* U+7390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+739F */
/* U+73A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+73AF */
/* U+73B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+73BF */
/* U+73C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+73CF */
/* U+73D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+73DF */
/* U+73E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+73EF */
/* U+73F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+73FF */
/* U+7400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+740F */
/* U+7410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+741F */
/* U+7420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+742F */
/* U+7430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+743F */
/* U+7440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+744F */
/* U+7450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+745F */
/* U+7460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+746F */
/* U+7470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+747F */
/* U+7480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+748F */
/* U+7490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+749F */
/* U+74A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+74AF */
/* U+74B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+74BF */
/* U+74C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+74CF */
/* U+74D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+74DF */
/* U+74E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+74EF */
/* U+74F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+74FF */
/* U+7500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+750F */
/* U+7510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+751F */
/* U+7520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+752F */
/* U+7530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+753F */
/* U+7540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+754F */
/* U+7550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+755F */
/* U+7560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+756F */
/* U+7570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+757F */
/* U+7580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+758F */
/* U+7590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+759F */
/* U+75A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+75AF */
/* U+75B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+75BF */
/* U+75C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+75CF */
/* U+75D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+75DF */
/* U+75E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+75EF */
/* U+75F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+75FF */
/* U+7600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+760F */
/* U+7610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+761F */
/* U+7620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+762F */
/* U+7630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+763F */
/* U+7640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+764F */
/* U+7650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+765F */
/* U+7660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+766F */
/* U+7670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+767F */
/* U+7680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+768F */
/* U+7690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+769F */
/* U+76A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+76AF */
/* U+76B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+76BF */
/* U+76C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+76CF */
/* U+76D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+76DF */
/* U+76E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+76EF */
/* U+76F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+76FF */
/* U+7700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+770F */
/* U+7710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+771F */
/* U+7720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+772F */
/* U+7730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+773F */
/* U+7740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+774F */
/* U+7750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+775F */
/* U+7760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+776F */
/* U+7770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+777F */
/* U+7780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+778F */
/* U+7790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+779F */
/* U+77A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+77AF */
/* U+77B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+77BF */
/* U+77C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+77CF */
/* U+77D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+77DF */
/* U+77E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+77EF */
/* U+77F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+77FF */
/* U+7800 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+780F */
/* U+7810 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+781F */
/* U+7820 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+782F */
/* U+7830 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+783F */
/* U+7840 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+784F */
/* U+7850 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+785F */
/* U+7860 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+786F */
/* U+7870 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+787F */
/* U+7880 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+788F */
/* U+7890 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+789F */
/* U+78A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+78AF */
/* U+78B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+78BF */
/* U+78C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+78CF */
/* U+78D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+78DF */
/* U+78E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+78EF */
/* U+78F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+78FF */
/* U+7900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+790F */
/* U+7910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+791F */
/* U+7920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+792F */
/* U+7930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+793F */
/* U+7940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+794F */
/* U+7950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+795F */
/* U+7960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+796F */
/* U+7970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+797F */
/* U+7980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+798F */
/* U+7990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+799F */
/* U+79A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+79AF */
/* U+79B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+79BF */
/* U+79C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+79CF */
/* U+79D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+79DF */
/* U+79E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+79EF */
/* U+79F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+79FF */
/* U+7A00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A0F */
/* U+7A10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A1F */
/* U+7A20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A2F */
/* U+7A30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A3F */
/* U+7A40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A4F */
/* U+7A50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A5F */
/* U+7A60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A6F */
/* U+7A70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A7F */
/* U+7A80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A8F */
/* U+7A90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7A9F */
/* U+7AA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7AAF */
/* U+7AB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7ABF */
/* U+7AC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7ACF */
/* U+7AD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7ADF */
/* U+7AE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7AEF */
/* U+7AF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7AFF */
/* U+7B00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B0F */
/* U+7B10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B1F */
/* U+7B20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B2F */
/* U+7B30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B3F */
/* U+7B40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B4F */
/* U+7B50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B5F */
/* U+7B60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B6F */
/* U+7B70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B7F */
/* U+7B80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B8F */
/* U+7B90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7B9F */
/* U+7BA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7BAF */
/* U+7BB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7BBF */
/* U+7BC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7BCF */
/* U+7BD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7BDF */
/* U+7BE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7BEF */
/* U+7BF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7BFF */
/* U+7C00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C0F */
/* U+7C10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C1F */
/* U+7C20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C2F */
/* U+7C30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C3F */
/* U+7C40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C4F */
/* U+7C50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C5F */
/* U+7C60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C6F */
/* U+7C70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C7F */
/* U+7C80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C8F */
/* U+7C90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7C9F */
/* U+7CA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7CAF */
/* U+7CB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7CBF */
/* U+7CC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7CCF */
/* U+7CD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7CDF */
/* U+7CE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7CEF */
/* U+7CF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7CFF */
/* U+7D00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D0F */
/* U+7D10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D1F */
/* U+7D20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D2F */
/* U+7D30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D3F */
/* U+7D40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D4F */
/* U+7D50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D5F */
/* U+7D60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D6F */
/* U+7D70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D7F */
/* U+7D80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D8F */
/* U+7D90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7D9F */
/* U+7DA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7DAF */
/* U+7DB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7DBF */
/* U+7DC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7DCF */
/* U+7DD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7DDF */
/* U+7DE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7DEF */
/* U+7DF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7DFF */
/* U+7E00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E0F */
/* U+7E10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E1F */
/* U+7E20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E2F */
/* U+7E30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E3F */
/* U+7E40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E4F */
/* U+7E50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E5F */
/* U+7E60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E6F */
/* U+7E70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E7F */
/* U+7E80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E8F */
/* U+7E90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7E9F */
/* U+7EA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7EAF */
/* U+7EB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7EBF */
/* U+7EC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7ECF */
/* U+7ED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7EDF */
/* U+7EE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7EEF */
/* U+7EF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7EFF */
/* U+7F00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F0F */
/* U+7F10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F1F */
/* U+7F20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F2F */
/* U+7F30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F3F */
/* U+7F40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F4F */
/* U+7F50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F5F */
/* U+7F60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F6F */
/* U+7F70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F7F */
/* U+7F80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F8F */
/* U+7F90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7F9F */
/* U+7FA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7FAF */
/* U+7FB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7FBF */
/* U+7FC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7FCF */
/* U+7FD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7FDF */
/* U+7FE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7FEF */
/* U+7FF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+7FFF */
/* U+8000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+800F */
/* U+8010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+801F */
/* U+8020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+802F */
/* U+8030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+803F */
/* U+8040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+804F */
/* U+8050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+805F */
/* U+8060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+806F */
/* U+8070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+807F */
/* U+8080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+808F */
/* U+8090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+809F */
/* U+80A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+80AF */
/* U+80B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+80BF */
/* U+80C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+80CF */
/* U+80D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+80DF */
/* U+80E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+80EF */
/* U+80F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+80FF */
/* U+8100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+810F */
/* U+8110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+811F */
/* U+8120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+812F */
/* U+8130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+813F */
/* U+8140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+814F */
/* U+8150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+815F */
/* U+8160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+816F */
/* U+8170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+817F */
/* U+8180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+818F */
/* U+8190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+819F */
/* U+81A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+81AF */
/* U+81B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+81BF */
/* U+81C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+81CF */
/* U+81D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+81DF */
/* U+81E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+81EF */
/* U+81F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+81FF */
/* U+8200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+820F */
/* U+8210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+821F */
/* U+8220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+822F */
/* U+8230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+823F */
/* U+8240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+824F */
/* U+8250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+825F */
/* U+8260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+826F */
/* U+8270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+827F */
/* U+8280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+828F */
/* U+8290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+829F */
/* U+82A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+82AF */
/* U+82B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+82BF */
/* U+82C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+82CF */
/* U+82D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+82DF */
/* U+82E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+82EF */
/* U+82F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+82FF */
/* U+8300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+830F */
/* U+8310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+831F */
/* U+8320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+832F */
/* U+8330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+833F */
/* U+8340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+834F */
/* U+8350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+835F */
/* U+8360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+836F */
/* U+8370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+837F */
/* U+8380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+838F */
/* U+8390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+839F */
/* U+83A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+83AF */
/* U+83B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+83BF */
/* U+83C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+83CF */
/* U+83D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+83DF */
/* U+83E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+83EF */
/* U+83F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+83FF */
/* U+8400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+840F */
/* U+8410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+841F */
/* U+8420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+842F */
/* U+8430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+843F */
/* U+8440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+844F */
/* U+8450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+845F */
/* U+8460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+846F */
/* U+8470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+847F */
/* U+8480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+848F */
/* U+8490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+849F */
/* U+84A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+84AF */
/* U+84B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+84BF */
/* U+84C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+84CF */
/* U+84D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+84DF */
/* U+84E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+84EF */
/* U+84F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+84FF */
/* U+8500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+850F */
/* U+8510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+851F */
/* U+8520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+852F */
/* U+8530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+853F */
/* U+8540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+854F */
/* U+8550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+855F */
/* U+8560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+856F */
/* U+8570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+857F */
/* U+8580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+858F */
/* U+8590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+859F */
/* U+85A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+85AF */
/* U+85B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+85BF */
/* U+85C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+85CF */
/* U+85D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+85DF */
/* U+85E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+85EF */
/* U+85F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+85FF */
/* U+8600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+860F */
/* U+8610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+861F */
/* U+8620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+862F */
/* U+8630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+863F */
/* U+8640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+864F */
/* U+8650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+865F */
/* U+8660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+866F */
/* U+8670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+867F */
/* U+8680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+868F */
/* U+8690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+869F */
/* U+86A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+86AF */
/* U+86B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+86BF */
/* U+86C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+86CF */
/* U+86D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+86DF */
/* U+86E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+86EF */
/* U+86F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+86FF */
/* U+8700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+870F */
/* U+8710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+871F */
/* U+8720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+872F */
/* U+8730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+873F */
/* U+8740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+874F */
/* U+8750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+875F */
/* U+8760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+876F */
/* U+8770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+877F */
/* U+8780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+878F */
/* U+8790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+879F */
/* U+87A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+87AF */
/* U+87B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+87BF */
/* U+87C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+87CF */
/* U+87D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+87DF */
/* U+87E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+87EF */
/* U+87F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+87FF */
/* U+8800 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+880F */
/* U+8810 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+881F */
/* U+8820 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+882F */
/* U+8830 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+883F */
/* U+8840 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+884F */
/* U+8850 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+885F */
/* U+8860 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+886F */
/* U+8870 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+887F */
/* U+8880 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+888F */
/* U+8890 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+889F */
/* U+88A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+88AF */
/* U+88B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+88BF */
/* U+88C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+88CF */
/* U+88D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+88DF */
/* U+88E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+88EF */
/* U+88F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+88FF */
/* U+8900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+890F */
/* U+8910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+891F */
/* U+8920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+892F */
/* U+8930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+893F */
/* U+8940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+894F */
/* U+8950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+895F */
/* U+8960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+896F */
/* U+8970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+897F */
/* U+8980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+898F */
/* U+8990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+899F */
/* U+89A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+89AF */
/* U+89B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+89BF */
/* U+89C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+89CF */
/* U+89D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+89DF */
/* U+89E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+89EF */
/* U+89F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+89FF */
/* U+8A00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A0F */
/* U+8A10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A1F */
/* U+8A20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A2F */
/* U+8A30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A3F */
/* U+8A40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A4F */
/* U+8A50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A5F */
/* U+8A60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A6F */
/* U+8A70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A7F */
/* U+8A80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A8F */
/* U+8A90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8A9F */
/* U+8AA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8AAF */
/* U+8AB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8ABF */
/* U+8AC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8ACF */
/* U+8AD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8ADF */
/* U+8AE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8AEF */
/* U+8AF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8AFF */
/* U+8B00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B0F */
/* U+8B10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B1F */
/* U+8B20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B2F */
/* U+8B30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B3F */
/* U+8B40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B4F */
/* U+8B50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B5F */
/* U+8B60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B6F */
/* U+8B70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B7F */
/* U+8B80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B8F */
/* U+8B90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8B9F */
/* U+8BA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8BAF */
/* U+8BB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8BBF */
/* U+8BC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8BCF */
/* U+8BD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8BDF */
/* U+8BE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8BEF */
/* U+8BF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8BFF */
/* U+8C00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C0F */
/* U+8C10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C1F */
/* U+8C20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C2F */
/* U+8C30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C3F */
/* U+8C40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C4F */
/* U+8C50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C5F */
/* U+8C60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C6F */
/* U+8C70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C7F */
/* U+8C80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C8F */
/* U+8C90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8C9F */
/* U+8CA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8CAF */
/* U+8CB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8CBF */
/* U+8CC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8CCF */
/* U+8CD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8CDF */
/* U+8CE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8CEF */
/* U+8CF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8CFF */
/* U+8D00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D0F */
/* U+8D10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D1F */
/* U+8D20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D2F */
/* U+8D30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D3F */
/* U+8D40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D4F */
/* U+8D50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D5F */
/* U+8D60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D6F */
/* U+8D70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D7F */
/* U+8D80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D8F */
/* U+8D90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8D9F */
/* U+8DA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8DAF */
/* U+8DB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8DBF */
/* U+8DC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8DCF */
/* U+8DD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8DDF */
/* U+8DE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8DEF */
/* U+8DF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8DFF */
/* U+8E00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E0F */
/* U+8E10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E1F */
/* U+8E20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E2F */
/* U+8E30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E3F */
/* U+8E40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E4F */
/* U+8E50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E5F */
/* U+8E60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E6F */
/* U+8E70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E7F */
/* U+8E80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E8F */
/* U+8E90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8E9F */
/* U+8EA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8EAF */
/* U+8EB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8EBF */
/* U+8EC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8ECF */
/* U+8ED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8EDF */
/* U+8EE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8EEF */
/* U+8EF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8EFF */
/* U+8F00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F0F */
/* U+8F10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F1F */
/* U+8F20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F2F */
/* U+8F30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F3F */
/* U+8F40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F4F */
/* U+8F50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F5F */
/* U+8F60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F6F */
/* U+8F70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F7F */
/* U+8F80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F8F */
/* U+8F90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8F9F */
/* U+8FA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8FAF */
/* U+8FB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8FBF */
/* U+8FC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8FCF */
/* U+8FD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8FDF */
/* U+8FE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8FEF */
/* U+8FF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+8FFF */
/* U+9000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+900F */
/* U+9010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+901F */
/* U+9020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+902F */
/* U+9030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+903F */
/* U+9040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+904F */
/* U+9050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+905F */
/* U+9060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+906F */
/* U+9070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+907F */
/* U+9080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+908F */
/* U+9090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+909F */
/* U+90A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+90AF */
/* U+90B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+90BF */
/* U+90C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+90CF */
/* U+90D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+90DF */
/* U+90E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+90EF */
/* U+90F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+90FF */
/* U+9100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+910F */
/* U+9110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+911F */
/* U+9120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+912F */
/* U+9130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+913F */
/* U+9140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+914F */
/* U+9150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+915F */
/* U+9160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+916F */
/* U+9170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+917F */
/* U+9180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+918F */
/* U+9190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+919F */
/* U+91A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+91AF */
/* U+91B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+91BF */
/* U+91C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+91CF */
/* U+91D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+91DF */
/* U+91E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+91EF */
/* U+91F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+91FF */
/* U+9200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+920F */
/* U+9210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+921F */
/* U+9220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+922F */
/* U+9230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+923F */
/* U+9240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+924F */
/* U+9250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+925F */
/* U+9260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+926F */
/* U+9270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+927F */
/* U+9280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+928F */
/* U+9290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+929F */
/* U+92A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+92AF */
/* U+92B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+92BF */
/* U+92C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+92CF */
/* U+92D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+92DF */
/* U+92E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+92EF */
/* U+92F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+92FF */
/* U+9300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+930F */
/* U+9310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+931F */
/* U+9320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+932F */
/* U+9330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+933F */
/* U+9340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+934F */
/* U+9350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+935F */
/* U+9360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+936F */
/* U+9370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+937F */
/* U+9380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+938F */
/* U+9390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+939F */
/* U+93A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+93AF */
/* U+93B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+93BF */
/* U+93C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+93CF */
/* U+93D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+93DF */
/* U+93E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+93EF */
/* U+93F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+93FF */
/* U+9400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+940F */
/* U+9410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+941F */
/* U+9420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+942F */
/* U+9430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+943F */
/* U+9440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+944F */
/* U+9450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+945F */
/* U+9460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+946F */
/* U+9470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+947F */
/* U+9480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+948F */
/* U+9490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+949F */
/* U+94A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+94AF */
/* U+94B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+94BF */
/* U+94C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+94CF */
/* U+94D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+94DF */
/* U+94E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+94EF */
/* U+94F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+94FF */
/* U+9500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+950F */
/* U+9510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+951F */
/* U+9520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+952F */
/* U+9530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+953F */
/* U+9540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+954F */
/* U+9550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+955F */
/* U+9560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+956F */
/* U+9570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+957F */
/* U+9580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+958F */
/* U+9590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+959F */
/* U+95A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+95AF */
/* U+95B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+95BF */
/* U+95C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+95CF */
/* U+95D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+95DF */
/* U+95E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+95EF */
/* U+95F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+95FF */
/* U+9600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+960F */
/* U+9610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+961F */
/* U+9620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+962F */
/* U+9630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+963F */
/* U+9640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+964F */
/* U+9650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+965F */
/* U+9660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+966F */
/* U+9670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+967F */
/* U+9680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+968F */
/* U+9690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+969F */
/* U+96A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+96AF */
/* U+96B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+96BF */
/* U+96C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+96CF */
/* U+96D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+96DF */
/* U+96E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+96EF */
/* U+96F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+96FF */
/* U+9700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+970F */
/* U+9710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+971F */
/* U+9720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+972F */
/* U+9730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+973F */
/* U+9740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+974F */
/* U+9750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+975F */
/* U+9760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+976F */
/* U+9770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+977F */
/* U+9780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+978F */
/* U+9790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+979F */
/* U+97A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+97AF */
/* U+97B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+97BF */
/* U+97C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+97CF */
/* U+97D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+97DF */
/* U+97E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+97EF */
/* U+97F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+97FF */
/* U+9800 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+980F */
/* U+9810 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+981F */
/* U+9820 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+982F */
/* U+9830 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+983F */
/* U+9840 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+984F */
/* U+9850 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+985F */
/* U+9860 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+986F */
/* U+9870 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+987F */
/* U+9880 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+988F */
/* U+9890 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+989F */
/* U+98A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+98AF */
/* U+98B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+98BF */
/* U+98C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+98CF */
/* U+98D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+98DF */
/* U+98E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+98EF */
/* U+98F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+98FF */
/* U+9900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+990F */
/* U+9910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+991F */
/* U+9920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+992F */
/* U+9930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+993F */
/* U+9940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+994F */
/* U+9950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+995F */
/* U+9960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+996F */
/* U+9970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+997F */
/* U+9980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+998F */
/* U+9990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+999F */
/* U+99A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+99AF */
/* U+99B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+99BF */
/* U+99C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+99CF */
/* U+99D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+99DF */
/* U+99E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+99EF */
/* U+99F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+99FF */
/* U+9A00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A0F */
/* U+9A10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A1F */
/* U+9A20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A2F */
/* U+9A30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A3F */
/* U+9A40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A4F */
/* U+9A50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A5F */
/* U+9A60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A6F */
/* U+9A70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A7F */
/* U+9A80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A8F */
/* U+9A90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9A9F */
/* U+9AA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9AAF */
/* U+9AB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9ABF */
/* U+9AC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9ACF */
/* U+9AD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9ADF */
/* U+9AE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9AEF */
/* U+9AF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9AFF */
/* U+9B00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B0F */
/* U+9B10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B1F */
/* U+9B20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B2F */
/* U+9B30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B3F */
/* U+9B40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B4F */
/* U+9B50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B5F */
/* U+9B60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B6F */
/* U+9B70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B7F */
/* U+9B80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B8F */
/* U+9B90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9B9F */
/* U+9BA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9BAF */
/* U+9BB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9BBF */
/* U+9BC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9BCF */
/* U+9BD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9BDF */
/* U+9BE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9BEF */
/* U+9BF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9BFF */
/* U+9C00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C0F */
/* U+9C10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C1F */
/* U+9C20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C2F */
/* U+9C30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C3F */
/* U+9C40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C4F */
/* U+9C50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C5F */
/* U+9C60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C6F */
/* U+9C70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C7F */
/* U+9C80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C8F */
/* U+9C90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9C9F */
/* U+9CA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9CAF */
/* U+9CB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9CBF */
/* U+9CC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9CCF */
/* U+9CD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9CDF */
/* U+9CE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9CEF */
/* U+9CF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9CFF */
/* U+9D00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D0F */
/* U+9D10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D1F */
/* U+9D20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D2F */
/* U+9D30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D3F */
/* U+9D40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D4F */
/* U+9D50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D5F */
/* U+9D60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D6F */
/* U+9D70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D7F */
/* U+9D80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D8F */
/* U+9D90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9D9F */
/* U+9DA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9DAF */
/* U+9DB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9DBF */
/* U+9DC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9DCF */
/* U+9DD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9DDF */
/* U+9DE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9DEF */
/* U+9DF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9DFF */
/* U+9E00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E0F */
/* U+9E10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E1F */
/* U+9E20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E2F */
/* U+9E30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E3F */
/* U+9E40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E4F */
/* U+9E50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E5F */
/* U+9E60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E6F */
/* U+9E70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E7F */
/* U+9E80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E8F */
/* U+9E90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9E9F */
/* U+9EA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9EAF */
/* U+9EB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9EBF */
/* U+9EC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9ECF */
/* U+9ED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9EDF */
/* U+9EE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9EEF */
/* U+9EF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9EFF */
/* U+9F00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F0F */
/* U+9F10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F1F */
/* U+9F20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F2F */
/* U+9F30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F3F */
/* U+9F40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F4F */
/* U+9F50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F5F */
/* U+9F60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F6F */
/* U+9F70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F7F */
/* U+9F80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F8F */
/* U+9F90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+9F9F */
/* U+9FA0 */	2, 2, 2, 2, 2, 2, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+9FAF */
/* U+9FB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+9FBF */
/* U+9FC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+9FCF */
/* U+9FD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+9FDF */
/* U+9FE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+9FEF */
/* U+9FF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+9FFF */
/* U+A000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A00F */
/* U+A010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A01F */
/* U+A020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A02F */
/* U+A030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A03F */
/* U+A040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A04F */
/* U+A050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A05F */
/* U+A060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A06F */
/* U+A070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A07F */
/* U+A080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A08F */
/* U+A090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A09F */
/* U+A0A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A0AF */
/* U+A0B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A0BF */
/* U+A0C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A0CF */
/* U+A0D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A0DF */
/* U+A0E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A0EF */
/* U+A0F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A0FF */
/* U+A100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A10F */
/* U+A110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A11F */
/* U+A120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A12F */
/* U+A130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A13F */
/* U+A140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A14F */
/* U+A150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A15F */
/* U+A160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A16F */
/* U+A170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A17F */
/* U+A180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A18F */
/* U+A190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A19F */
/* U+A1A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A1AF */
/* U+A1B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A1BF */
/* U+A1C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A1CF */
/* U+A1D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A1DF */
/* U+A1E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A1EF */
/* U+A1F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A1FF */
/* U+A200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A20F */
/* U+A210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A21F */
/* U+A220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A22F */
/* U+A230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A23F */
/* U+A240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A24F */
/* U+A250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A25F */
/* U+A260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A26F */
/* U+A270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A27F */
/* U+A280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A28F */
/* U+A290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A29F */
/* U+A2A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A2AF */
/* U+A2B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A2BF */
/* U+A2C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A2CF */
/* U+A2D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A2DF */
/* U+A2E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A2EF */
/* U+A2F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A2FF */
/* U+A300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A30F */
/* U+A310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A31F */
/* U+A320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A32F */
/* U+A330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A33F */
/* U+A340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A34F */
/* U+A350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A35F */
/* U+A360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A36F */
/* U+A370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A37F */
/* U+A380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A38F */
/* U+A390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A39F */
/* U+A3A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A3AF */
/* U+A3B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A3BF */
/* U+A3C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A3CF */
/* U+A3D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A3DF */
/* U+A3E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A3EF */
/* U+A3F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A3FF */
/* U+A400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A40F */
/* U+A410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A41F */
/* U+A420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A42F */
/* U+A430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A43F */
/* U+A440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A44F */
/* U+A450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A45F */
/* U+A460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A46F */
/* U+A470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A47F */
/* U+A480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,IL,   /* U+A48F */
/* U+A490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A49F */
/* U+A4A0 */	2, 2, IL,IL,2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A4AF */
/* U+A4B0 */	2, 2, 2, 2, IL,2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+A4BF */
/* U+A4C0 */	2, 2, 2, 2, 2, IL,2, IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A4CF */
/* U+A4D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A4DF */
/* U+A4E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A4EF */
/* U+A4F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A4FF */
/* U+A500 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A50F */
/* U+A510 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A51F */
/* U+A520 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A52F */
/* U+A530 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A53F */
/* U+A540 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A54F */
/* U+A550 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A55F */
/* U+A560 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A56F */
/* U+A570 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A57F */
/* U+A580 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A58F */
/* U+A590 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A59F */
/* U+A5A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A5AF */
/* U+A5B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A5BF */
/* U+A5C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A5CF */
/* U+A5D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A5DF */
/* U+A5E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A5EF */
/* U+A5F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A5FF */
/* U+A600 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A60F */
/* U+A610 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A61F */
/* U+A620 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A62F */
/* U+A630 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A63F */
/* U+A640 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A64F */
/* U+A650 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A65F */
/* U+A660 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A66F */
/* U+A670 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A67F */
/* U+A680 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A68F */
/* U+A690 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A69F */
/* U+A6A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A6AF */
/* U+A6B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A6BF */
/* U+A6C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A6CF */
/* U+A6D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A6DF */
/* U+A6E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A6EF */
/* U+A6F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A6FF */
/* U+A700 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A70F */
/* U+A710 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A71F */
/* U+A720 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A72F */
/* U+A730 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A73F */
/* U+A740 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A74F */
/* U+A750 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A75F */
/* U+A760 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A76F */
/* U+A770 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A77F */
/* U+A780 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A78F */
/* U+A790 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A79F */
/* U+A7A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A7AF */
/* U+A7B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A7BF */
/* U+A7C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A7CF */
/* U+A7D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A7DF */
/* U+A7E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A7EF */
/* U+A7F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A7FF */
/* U+A800 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A80F */
/* U+A810 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A81F */
/* U+A820 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A82F */
/* U+A830 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A83F */
/* U+A840 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A84F */
/* U+A850 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A85F */
/* U+A860 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A86F */
/* U+A870 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A87F */
/* U+A880 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A88F */
/* U+A890 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A89F */
/* U+A8A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A8AF */
/* U+A8B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A8BF */
/* U+A8C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A8CF */
/* U+A8D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A8DF */
/* U+A8E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A8EF */
/* U+A8F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A8FF */
/* U+A900 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A90F */
/* U+A910 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A91F */
/* U+A920 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A92F */
/* U+A930 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A93F */
/* U+A940 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A94F */
/* U+A950 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A95F */
/* U+A960 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A96F */
/* U+A970 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A97F */
/* U+A980 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A98F */
/* U+A990 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A99F */
/* U+A9A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A9AF */
/* U+A9B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A9BF */
/* U+A9C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A9CF */
/* U+A9D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A9DF */
/* U+A9E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A9EF */
/* U+A9F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+A9FF */
/* U+AA00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA0F */
/* U+AA10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA1F */
/* U+AA20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA2F */
/* U+AA30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA3F */
/* U+AA40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA4F */
/* U+AA50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA5F */
/* U+AA60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA6F */
/* U+AA70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA7F */
/* U+AA80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA8F */
/* U+AA90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AA9F */
/* U+AAA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AAAF */
/* U+AAB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AABF */
/* U+AAC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AACF */
/* U+AAD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AADF */
/* U+AAE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AAEF */
/* U+AAF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AAFF */
/* U+AB00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB0F */
/* U+AB10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB1F */
/* U+AB20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB2F */
/* U+AB30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB3F */
/* U+AB40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB4F */
/* U+AB50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB5F */
/* U+AB60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB6F */
/* U+AB70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB7F */
/* U+AB80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB8F */
/* U+AB90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+AB9F */
/* U+ABA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+ABAF */
/* U+ABB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+ABBF */
/* U+ABC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+ABCF */
/* U+ABD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+ABDF */
/* U+ABE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+ABEF */
/* U+ABF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+ABFF */
/* U+AC00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC0F */
/* U+AC10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC1F */
/* U+AC20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC2F */
/* U+AC30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC3F */
/* U+AC40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC4F */
/* U+AC50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC5F */
/* U+AC60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC6F */
/* U+AC70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC7F */
/* U+AC80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC8F */
/* U+AC90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AC9F */
/* U+ACA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ACAF */
/* U+ACB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ACBF */
/* U+ACC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ACCF */
/* U+ACD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ACDF */
/* U+ACE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ACEF */
/* U+ACF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ACFF */
/* U+AD00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD0F */
/* U+AD10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD1F */
/* U+AD20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD2F */
/* U+AD30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD3F */
/* U+AD40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD4F */
/* U+AD50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD5F */
/* U+AD60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD6F */
/* U+AD70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD7F */
/* U+AD80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD8F */
/* U+AD90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AD9F */
/* U+ADA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ADAF */
/* U+ADB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ADBF */
/* U+ADC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ADCF */
/* U+ADD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ADDF */
/* U+ADE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ADEF */
/* U+ADF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+ADFF */
/* U+AE00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE0F */
/* U+AE10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE1F */
/* U+AE20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE2F */
/* U+AE30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE3F */
/* U+AE40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE4F */
/* U+AE50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE5F */
/* U+AE60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE6F */
/* U+AE70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE7F */
/* U+AE80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE8F */
/* U+AE90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AE9F */
/* U+AEA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AEAF */
/* U+AEB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AEBF */
/* U+AEC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AECF */
/* U+AED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AEDF */
/* U+AEE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AEEF */
/* U+AEF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AEFF */
/* U+AF00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF0F */
/* U+AF10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF1F */
/* U+AF20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF2F */
/* U+AF30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF3F */
/* U+AF40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF4F */
/* U+AF50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF5F */
/* U+AF60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF6F */
/* U+AF70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF7F */
/* U+AF80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF8F */
/* U+AF90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AF9F */
/* U+AFA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AFAF */
/* U+AFB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AFBF */
/* U+AFC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AFCF */
/* U+AFD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AFDF */
/* U+AFE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AFEF */
/* U+AFF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+AFFF */
/* U+B000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B00F */
/* U+B010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B01F */
/* U+B020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B02F */
/* U+B030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B03F */
/* U+B040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B04F */
/* U+B050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B05F */
/* U+B060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B06F */
/* U+B070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B07F */
/* U+B080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B08F */
/* U+B090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B09F */
/* U+B0A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B0AF */
/* U+B0B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B0BF */
/* U+B0C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B0CF */
/* U+B0D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B0DF */
/* U+B0E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B0EF */
/* U+B0F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B0FF */
/* U+B100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B10F */
/* U+B110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B11F */
/* U+B120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B12F */
/* U+B130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B13F */
/* U+B140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B14F */
/* U+B150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B15F */
/* U+B160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B16F */
/* U+B170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B17F */
/* U+B180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B18F */
/* U+B190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B19F */
/* U+B1A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B1AF */
/* U+B1B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B1BF */
/* U+B1C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B1CF */
/* U+B1D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B1DF */
/* U+B1E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B1EF */
/* U+B1F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B1FF */
/* U+B200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B20F */
/* U+B210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B21F */
/* U+B220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B22F */
/* U+B230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B23F */
/* U+B240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B24F */
/* U+B250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B25F */
/* U+B260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B26F */
/* U+B270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B27F */
/* U+B280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B28F */
/* U+B290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B29F */
/* U+B2A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B2AF */
/* U+B2B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B2BF */
/* U+B2C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B2CF */
/* U+B2D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B2DF */
/* U+B2E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B2EF */
/* U+B2F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B2FF */
/* U+B300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B30F */
/* U+B310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B31F */
/* U+B320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B32F */
/* U+B330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B33F */
/* U+B340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B34F */
/* U+B350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B35F */
/* U+B360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B36F */
/* U+B370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B37F */
/* U+B380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B38F */
/* U+B390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B39F */
/* U+B3A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B3AF */
/* U+B3B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B3BF */
/* U+B3C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B3CF */
/* U+B3D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B3DF */
/* U+B3E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B3EF */
/* U+B3F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B3FF */
/* U+B400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B40F */
/* U+B410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B41F */
/* U+B420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B42F */
/* U+B430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B43F */
/* U+B440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B44F */
/* U+B450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B45F */
/* U+B460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B46F */
/* U+B470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B47F */
/* U+B480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B48F */
/* U+B490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B49F */
/* U+B4A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B4AF */
/* U+B4B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B4BF */
/* U+B4C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B4CF */
/* U+B4D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B4DF */
/* U+B4E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B4EF */
/* U+B4F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B4FF */
/* U+B500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B50F */
/* U+B510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B51F */
/* U+B520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B52F */
/* U+B530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B53F */
/* U+B540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B54F */
/* U+B550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B55F */
/* U+B560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B56F */
/* U+B570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B57F */
/* U+B580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B58F */
/* U+B590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B59F */
/* U+B5A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B5AF */
/* U+B5B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B5BF */
/* U+B5C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B5CF */
/* U+B5D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B5DF */
/* U+B5E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B5EF */
/* U+B5F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B5FF */
/* U+B600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B60F */
/* U+B610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B61F */
/* U+B620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B62F */
/* U+B630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B63F */
/* U+B640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B64F */
/* U+B650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B65F */
/* U+B660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B66F */
/* U+B670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B67F */
/* U+B680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B68F */
/* U+B690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B69F */
/* U+B6A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B6AF */
/* U+B6B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B6BF */
/* U+B6C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B6CF */
/* U+B6D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B6DF */
/* U+B6E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B6EF */
/* U+B6F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B6FF */
/* U+B700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B70F */
/* U+B710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B71F */
/* U+B720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B72F */
/* U+B730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B73F */
/* U+B740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B74F */
/* U+B750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B75F */
/* U+B760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B76F */
/* U+B770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B77F */
/* U+B780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B78F */
/* U+B790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B79F */
/* U+B7A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B7AF */
/* U+B7B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B7BF */
/* U+B7C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B7CF */
/* U+B7D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B7DF */
/* U+B7E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B7EF */
/* U+B7F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B7FF */
/* U+B800 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B80F */
/* U+B810 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B81F */
/* U+B820 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B82F */
/* U+B830 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B83F */
/* U+B840 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B84F */
/* U+B850 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B85F */
/* U+B860 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B86F */
/* U+B870 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B87F */
/* U+B880 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B88F */
/* U+B890 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B89F */
/* U+B8A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B8AF */
/* U+B8B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B8BF */
/* U+B8C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B8CF */
/* U+B8D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B8DF */
/* U+B8E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B8EF */
/* U+B8F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B8FF */
/* U+B900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B90F */
/* U+B910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B91F */
/* U+B920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B92F */
/* U+B930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B93F */
/* U+B940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B94F */
/* U+B950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B95F */
/* U+B960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B96F */
/* U+B970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B97F */
/* U+B980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B98F */
/* U+B990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B99F */
/* U+B9A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B9AF */
/* U+B9B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B9BF */
/* U+B9C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B9CF */
/* U+B9D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B9DF */
/* U+B9E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B9EF */
/* U+B9F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+B9FF */
/* U+BA00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA0F */
/* U+BA10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA1F */
/* U+BA20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA2F */
/* U+BA30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA3F */
/* U+BA40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA4F */
/* U+BA50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA5F */
/* U+BA60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA6F */
/* U+BA70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA7F */
/* U+BA80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA8F */
/* U+BA90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BA9F */
/* U+BAA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BAAF */
/* U+BAB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BABF */
/* U+BAC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BACF */
/* U+BAD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BADF */
/* U+BAE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BAEF */
/* U+BAF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BAFF */
/* U+BB00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB0F */
/* U+BB10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB1F */
/* U+BB20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB2F */
/* U+BB30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB3F */
/* U+BB40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB4F */
/* U+BB50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB5F */
/* U+BB60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB6F */
/* U+BB70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB7F */
/* U+BB80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB8F */
/* U+BB90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BB9F */
/* U+BBA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BBAF */
/* U+BBB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BBBF */
/* U+BBC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BBCF */
/* U+BBD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BBDF */
/* U+BBE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BBEF */
/* U+BBF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BBFF */
/* U+BC00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC0F */
/* U+BC10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC1F */
/* U+BC20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC2F */
/* U+BC30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC3F */
/* U+BC40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC4F */
/* U+BC50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC5F */
/* U+BC60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC6F */
/* U+BC70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC7F */
/* U+BC80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC8F */
/* U+BC90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BC9F */
/* U+BCA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BCAF */
/* U+BCB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BCBF */
/* U+BCC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BCCF */
/* U+BCD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BCDF */
/* U+BCE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BCEF */
/* U+BCF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BCFF */
/* U+BD00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD0F */
/* U+BD10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD1F */
/* U+BD20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD2F */
/* U+BD30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD3F */
/* U+BD40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD4F */
/* U+BD50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD5F */
/* U+BD60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD6F */
/* U+BD70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD7F */
/* U+BD80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD8F */
/* U+BD90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BD9F */
/* U+BDA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BDAF */
/* U+BDB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BDBF */
/* U+BDC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BDCF */
/* U+BDD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BDDF */
/* U+BDE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BDEF */
/* U+BDF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BDFF */
/* U+BE00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE0F */
/* U+BE10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE1F */
/* U+BE20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE2F */
/* U+BE30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE3F */
/* U+BE40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE4F */
/* U+BE50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE5F */
/* U+BE60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE6F */
/* U+BE70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE7F */
/* U+BE80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE8F */
/* U+BE90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BE9F */
/* U+BEA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BEAF */
/* U+BEB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BEBF */
/* U+BEC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BECF */
/* U+BED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BEDF */
/* U+BEE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BEEF */
/* U+BEF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BEFF */
/* U+BF00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF0F */
/* U+BF10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF1F */
/* U+BF20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF2F */
/* U+BF30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF3F */
/* U+BF40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF4F */
/* U+BF50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF5F */
/* U+BF60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF6F */
/* U+BF70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF7F */
/* U+BF80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF8F */
/* U+BF90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BF9F */
/* U+BFA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BFAF */
/* U+BFB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BFBF */
/* U+BFC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BFCF */
/* U+BFD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BFDF */
/* U+BFE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BFEF */
/* U+BFF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+BFFF */
/* U+C000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C00F */
/* U+C010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C01F */
/* U+C020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C02F */
/* U+C030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C03F */
/* U+C040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C04F */
/* U+C050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C05F */
/* U+C060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C06F */
/* U+C070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C07F */
/* U+C080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C08F */
/* U+C090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C09F */
/* U+C0A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C0AF */
/* U+C0B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C0BF */
/* U+C0C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C0CF */
/* U+C0D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C0DF */
/* U+C0E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C0EF */
/* U+C0F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C0FF */
/* U+C100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C10F */
/* U+C110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C11F */
/* U+C120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C12F */
/* U+C130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C13F */
/* U+C140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C14F */
/* U+C150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C15F */
/* U+C160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C16F */
/* U+C170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C17F */
/* U+C180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C18F */
/* U+C190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C19F */
/* U+C1A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C1AF */
/* U+C1B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C1BF */
/* U+C1C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C1CF */
/* U+C1D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C1DF */
/* U+C1E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C1EF */
/* U+C1F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C1FF */
/* U+C200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C20F */
/* U+C210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C21F */
/* U+C220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C22F */
/* U+C230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C23F */
/* U+C240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C24F */
/* U+C250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C25F */
/* U+C260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C26F */
/* U+C270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C27F */
/* U+C280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C28F */
/* U+C290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C29F */
/* U+C2A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C2AF */
/* U+C2B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C2BF */
/* U+C2C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C2CF */
/* U+C2D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C2DF */
/* U+C2E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C2EF */
/* U+C2F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C2FF */
/* U+C300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C30F */
/* U+C310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C31F */
/* U+C320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C32F */
/* U+C330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C33F */
/* U+C340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C34F */
/* U+C350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C35F */
/* U+C360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C36F */
/* U+C370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C37F */
/* U+C380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C38F */
/* U+C390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C39F */
/* U+C3A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C3AF */
/* U+C3B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C3BF */
/* U+C3C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C3CF */
/* U+C3D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C3DF */
/* U+C3E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C3EF */
/* U+C3F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C3FF */
/* U+C400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C40F */
/* U+C410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C41F */
/* U+C420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C42F */
/* U+C430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C43F */
/* U+C440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C44F */
/* U+C450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C45F */
/* U+C460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C46F */
/* U+C470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C47F */
/* U+C480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C48F */
/* U+C490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C49F */
/* U+C4A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C4AF */
/* U+C4B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C4BF */
/* U+C4C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C4CF */
/* U+C4D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C4DF */
/* U+C4E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C4EF */
/* U+C4F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C4FF */
/* U+C500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C50F */
/* U+C510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C51F */
/* U+C520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C52F */
/* U+C530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C53F */
/* U+C540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C54F */
/* U+C550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C55F */
/* U+C560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C56F */
/* U+C570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C57F */
/* U+C580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C58F */
/* U+C590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C59F */
/* U+C5A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C5AF */
/* U+C5B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C5BF */
/* U+C5C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C5CF */
/* U+C5D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C5DF */
/* U+C5E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C5EF */
/* U+C5F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C5FF */
/* U+C600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C60F */
/* U+C610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C61F */
/* U+C620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C62F */
/* U+C630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C63F */
/* U+C640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C64F */
/* U+C650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C65F */
/* U+C660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C66F */
/* U+C670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C67F */
/* U+C680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C68F */
/* U+C690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C69F */
/* U+C6A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C6AF */
/* U+C6B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C6BF */
/* U+C6C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C6CF */
/* U+C6D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C6DF */
/* U+C6E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C6EF */
/* U+C6F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C6FF */
/* U+C700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C70F */
/* U+C710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C71F */
/* U+C720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C72F */
/* U+C730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C73F */
/* U+C740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C74F */
/* U+C750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C75F */
/* U+C760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C76F */
/* U+C770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C77F */
/* U+C780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C78F */
/* U+C790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C79F */
/* U+C7A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C7AF */
/* U+C7B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C7BF */
/* U+C7C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C7CF */
/* U+C7D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C7DF */
/* U+C7E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C7EF */
/* U+C7F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C7FF */
/* U+C800 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C80F */
/* U+C810 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C81F */
/* U+C820 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C82F */
/* U+C830 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C83F */
/* U+C840 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C84F */
/* U+C850 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C85F */
/* U+C860 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C86F */
/* U+C870 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C87F */
/* U+C880 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C88F */
/* U+C890 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C89F */
/* U+C8A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C8AF */
/* U+C8B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C8BF */
/* U+C8C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C8CF */
/* U+C8D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C8DF */
/* U+C8E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C8EF */
/* U+C8F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C8FF */
/* U+C900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C90F */
/* U+C910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C91F */
/* U+C920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C92F */
/* U+C930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C93F */
/* U+C940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C94F */
/* U+C950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C95F */
/* U+C960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C96F */
/* U+C970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C97F */
/* U+C980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C98F */
/* U+C990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C99F */
/* U+C9A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C9AF */
/* U+C9B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C9BF */
/* U+C9C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C9CF */
/* U+C9D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C9DF */
/* U+C9E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C9EF */
/* U+C9F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+C9FF */
/* U+CA00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA0F */
/* U+CA10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA1F */
/* U+CA20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA2F */
/* U+CA30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA3F */
/* U+CA40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA4F */
/* U+CA50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA5F */
/* U+CA60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA6F */
/* U+CA70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA7F */
/* U+CA80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA8F */
/* U+CA90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CA9F */
/* U+CAA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CAAF */
/* U+CAB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CABF */
/* U+CAC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CACF */
/* U+CAD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CADF */
/* U+CAE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CAEF */
/* U+CAF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CAFF */
/* U+CB00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB0F */
/* U+CB10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB1F */
/* U+CB20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB2F */
/* U+CB30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB3F */
/* U+CB40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB4F */
/* U+CB50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB5F */
/* U+CB60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB6F */
/* U+CB70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB7F */
/* U+CB80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB8F */
/* U+CB90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CB9F */
/* U+CBA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CBAF */
/* U+CBB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CBBF */
/* U+CBC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CBCF */
/* U+CBD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CBDF */
/* U+CBE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CBEF */
/* U+CBF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CBFF */
/* U+CC00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC0F */
/* U+CC10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC1F */
/* U+CC20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC2F */
/* U+CC30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC3F */
/* U+CC40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC4F */
/* U+CC50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC5F */
/* U+CC60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC6F */
/* U+CC70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC7F */
/* U+CC80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC8F */
/* U+CC90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CC9F */
/* U+CCA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CCAF */
/* U+CCB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CCBF */
/* U+CCC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CCCF */
/* U+CCD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CCDF */
/* U+CCE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CCEF */
/* U+CCF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CCFF */
/* U+CD00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD0F */
/* U+CD10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD1F */
/* U+CD20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD2F */
/* U+CD30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD3F */
/* U+CD40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD4F */
/* U+CD50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD5F */
/* U+CD60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD6F */
/* U+CD70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD7F */
/* U+CD80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD8F */
/* U+CD90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CD9F */
/* U+CDA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CDAF */
/* U+CDB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CDBF */
/* U+CDC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CDCF */
/* U+CDD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CDDF */
/* U+CDE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CDEF */
/* U+CDF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CDFF */
/* U+CE00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE0F */
/* U+CE10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE1F */
/* U+CE20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE2F */
/* U+CE30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE3F */
/* U+CE40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE4F */
/* U+CE50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE5F */
/* U+CE60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE6F */
/* U+CE70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE7F */
/* U+CE80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE8F */
/* U+CE90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CE9F */
/* U+CEA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CEAF */
/* U+CEB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CEBF */
/* U+CEC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CECF */
/* U+CED0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CEDF */
/* U+CEE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CEEF */
/* U+CEF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CEFF */
/* U+CF00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF0F */
/* U+CF10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF1F */
/* U+CF20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF2F */
/* U+CF30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF3F */
/* U+CF40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF4F */
/* U+CF50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF5F */
/* U+CF60 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF6F */
/* U+CF70 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF7F */
/* U+CF80 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF8F */
/* U+CF90 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CF9F */
/* U+CFA0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CFAF */
/* U+CFB0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CFBF */
/* U+CFC0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CFCF */
/* U+CFD0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CFDF */
/* U+CFE0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CFEF */
/* U+CFF0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+CFFF */
/* U+D000 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D00F */
/* U+D010 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D01F */
/* U+D020 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D02F */
/* U+D030 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D03F */
/* U+D040 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D04F */
/* U+D050 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D05F */
/* U+D060 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D06F */
/* U+D070 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D07F */
/* U+D080 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D08F */
/* U+D090 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D09F */
/* U+D0A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D0AF */
/* U+D0B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D0BF */
/* U+D0C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D0CF */
/* U+D0D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D0DF */
/* U+D0E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D0EF */
/* U+D0F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D0FF */
/* U+D100 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D10F */
/* U+D110 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D11F */
/* U+D120 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D12F */
/* U+D130 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D13F */
/* U+D140 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D14F */
/* U+D150 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D15F */
/* U+D160 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D16F */
/* U+D170 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D17F */
/* U+D180 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D18F */
/* U+D190 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D19F */
/* U+D1A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D1AF */
/* U+D1B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D1BF */
/* U+D1C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D1CF */
/* U+D1D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D1DF */
/* U+D1E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D1EF */
/* U+D1F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D1FF */
/* U+D200 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D20F */
/* U+D210 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D21F */
/* U+D220 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D22F */
/* U+D230 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D23F */
/* U+D240 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D24F */
/* U+D250 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D25F */
/* U+D260 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D26F */
/* U+D270 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D27F */
/* U+D280 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D28F */
/* U+D290 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D29F */
/* U+D2A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D2AF */
/* U+D2B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D2BF */
/* U+D2C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D2CF */
/* U+D2D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D2DF */
/* U+D2E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D2EF */
/* U+D2F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D2FF */
/* U+D300 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D30F */
/* U+D310 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D31F */
/* U+D320 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D32F */
/* U+D330 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D33F */
/* U+D340 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D34F */
/* U+D350 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D35F */
/* U+D360 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D36F */
/* U+D370 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D37F */
/* U+D380 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D38F */
/* U+D390 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D39F */
/* U+D3A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D3AF */
/* U+D3B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D3BF */
/* U+D3C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D3CF */
/* U+D3D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D3DF */
/* U+D3E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D3EF */
/* U+D3F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D3FF */
/* U+D400 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D40F */
/* U+D410 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D41F */
/* U+D420 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D42F */
/* U+D430 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D43F */
/* U+D440 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D44F */
/* U+D450 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D45F */
/* U+D460 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D46F */
/* U+D470 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D47F */
/* U+D480 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D48F */
/* U+D490 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D49F */
/* U+D4A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D4AF */
/* U+D4B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D4BF */
/* U+D4C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D4CF */
/* U+D4D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D4DF */
/* U+D4E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D4EF */
/* U+D4F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D4FF */
/* U+D500 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D50F */
/* U+D510 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D51F */
/* U+D520 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D52F */
/* U+D530 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D53F */
/* U+D540 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D54F */
/* U+D550 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D55F */
/* U+D560 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D56F */
/* U+D570 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D57F */
/* U+D580 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D58F */
/* U+D590 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D59F */
/* U+D5A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D5AF */
/* U+D5B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D5BF */
/* U+D5C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D5CF */
/* U+D5D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D5DF */
/* U+D5E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D5EF */
/* U+D5F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D5FF */
/* U+D600 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D60F */
/* U+D610 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D61F */
/* U+D620 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D62F */
/* U+D630 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D63F */
/* U+D640 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D64F */
/* U+D650 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D65F */
/* U+D660 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D66F */
/* U+D670 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D67F */
/* U+D680 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D68F */
/* U+D690 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D69F */
/* U+D6A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D6AF */
/* U+D6B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D6BF */
/* U+D6C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D6CF */
/* U+D6D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D6DF */
/* U+D6E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D6EF */
/* U+D6F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D6FF */
/* U+D700 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D70F */
/* U+D710 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D71F */
/* U+D720 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D72F */
/* U+D730 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D73F */
/* U+D740 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D74F */
/* U+D750 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D75F */
/* U+D760 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D76F */
/* U+D770 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D77F */
/* U+D780 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D78F */
/* U+D790 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+D79F */
/* U+D7A0 */	2, 2, 2, 2, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D7AF */
/* U+D7B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D7BF */
/* U+D7C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D7CF */
/* U+D7D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D7DF */
/* U+D7E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D7EF */
/* U+D7F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D7FF */
/* U+D800 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D80F */
/* U+D810 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D81F */
/* U+D820 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D82F */
/* U+D830 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D83F */
/* U+D840 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D84F */
/* U+D850 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D85F */
/* U+D860 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D86F */
/* U+D870 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D87F */
/* U+D880 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D88F */
/* U+D890 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D89F */
/* U+D8A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D8AF */
/* U+D8B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D8BF */
/* U+D8C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D8CF */
/* U+D8D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D8DF */
/* U+D8E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D8EF */
/* U+D8F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D8FF */
/* U+D900 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D90F */
/* U+D910 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D91F */
/* U+D920 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D92F */
/* U+D930 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D93F */
/* U+D940 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D94F */
/* U+D950 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D95F */
/* U+D960 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D96F */
/* U+D970 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D97F */
/* U+D980 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D98F */
/* U+D990 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D99F */
/* U+D9A0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D9AF */
/* U+D9B0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D9BF */
/* U+D9C0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D9CF */
/* U+D9D0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D9DF */
/* U+D9E0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D9EF */
/* U+D9F0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+D9FF */
/* U+DA00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA0F */
/* U+DA10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA1F */
/* U+DA20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA2F */
/* U+DA30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA3F */
/* U+DA40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA4F */
/* U+DA50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA5F */
/* U+DA60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA6F */
/* U+DA70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA7F */
/* U+DA80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA8F */
/* U+DA90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DA9F */
/* U+DAA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DAAF */
/* U+DAB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DABF */
/* U+DAC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DACF */
/* U+DAD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DADF */
/* U+DAE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DAEF */
/* U+DAF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DAFF */
/* U+DB00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB0F */
/* U+DB10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB1F */
/* U+DB20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB2F */
/* U+DB30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB3F */
/* U+DB40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB4F */
/* U+DB50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB5F */
/* U+DB60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB6F */
/* U+DB70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB7F */
/* U+DB80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB8F */
/* U+DB90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DB9F */
/* U+DBA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DBAF */
/* U+DBB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DBBF */
/* U+DBC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DBCF */
/* U+DBD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DBDF */
/* U+DBE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DBEF */
/* U+DBF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DBFF */
/* U+DC00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC0F */
/* U+DC10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC1F */
/* U+DC20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC2F */
/* U+DC30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC3F */
/* U+DC40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC4F */
/* U+DC50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC5F */
/* U+DC60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC6F */
/* U+DC70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC7F */
/* U+DC80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC8F */
/* U+DC90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DC9F */
/* U+DCA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DCAF */
/* U+DCB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DCBF */
/* U+DCC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DCCF */
/* U+DCD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DCDF */
/* U+DCE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DCEF */
/* U+DCF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DCFF */
/* U+DD00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD0F */
/* U+DD10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD1F */
/* U+DD20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD2F */
/* U+DD30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD3F */
/* U+DD40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD4F */
/* U+DD50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD5F */
/* U+DD60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD6F */
/* U+DD70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD7F */
/* U+DD80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD8F */
/* U+DD90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DD9F */
/* U+DDA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DDAF */
/* U+DDB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DDBF */
/* U+DDC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DDCF */
/* U+DDD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DDDF */
/* U+DDE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DDEF */
/* U+DDF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DDFF */
/* U+DE00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE0F */
/* U+DE10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE1F */
/* U+DE20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE2F */
/* U+DE30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE3F */
/* U+DE40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE4F */
/* U+DE50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE5F */
/* U+DE60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE6F */
/* U+DE70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE7F */
/* U+DE80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE8F */
/* U+DE90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DE9F */
/* U+DEA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DEAF */
/* U+DEB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DEBF */
/* U+DEC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DECF */
/* U+DED0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DEDF */
/* U+DEE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DEEF */
/* U+DEF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DEFF */
/* U+DF00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF0F */
/* U+DF10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF1F */
/* U+DF20 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF2F */
/* U+DF30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF3F */
/* U+DF40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF4F */
/* U+DF50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF5F */
/* U+DF60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF6F */
/* U+DF70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF7F */
/* U+DF80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF8F */
/* U+DF90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DF9F */
/* U+DFA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DFAF */
/* U+DFB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DFBF */
/* U+DFC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DFCF */
/* U+DFD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DFDF */
/* U+DFE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DFEF */
/* U+DFF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+DFFF */
/* U+E000 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E00F */
/* U+E010 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E01F */
/* U+E020 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E02F */
/* U+E030 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E03F */
/* U+E040 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E04F */
/* U+E050 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E05F */
/* U+E060 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E06F */
/* U+E070 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E07F */
/* U+E080 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E08F */
/* U+E090 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E09F */
/* U+E0A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E0AF */
/* U+E0B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E0BF */
/* U+E0C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E0CF */
/* U+E0D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E0DF */
/* U+E0E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E0EF */
/* U+E0F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E0FF */
/* U+E100 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E10F */
/* U+E110 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E11F */
/* U+E120 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E12F */
/* U+E130 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E13F */
/* U+E140 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E14F */
/* U+E150 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E15F */
/* U+E160 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E16F */
/* U+E170 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E17F */
/* U+E180 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E18F */
/* U+E190 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E19F */
/* U+E1A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E1AF */
/* U+E1B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E1BF */
/* U+E1C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E1CF */
/* U+E1D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E1DF */
/* U+E1E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E1EF */
/* U+E1F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E1FF */
/* U+E200 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E20F */
/* U+E210 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E21F */
/* U+E220 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E22F */
/* U+E230 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E23F */
/* U+E240 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E24F */
/* U+E250 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E25F */
/* U+E260 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E26F */
/* U+E270 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E27F */
/* U+E280 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E28F */
/* U+E290 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E29F */
/* U+E2A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E2AF */
/* U+E2B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E2BF */
/* U+E2C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E2CF */
/* U+E2D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E2DF */
/* U+E2E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E2EF */
/* U+E2F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E2FF */
/* U+E300 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E30F */
/* U+E310 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E31F */
/* U+E320 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E32F */
/* U+E330 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E33F */
/* U+E340 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E34F */
/* U+E350 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E35F */
/* U+E360 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E36F */
/* U+E370 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E37F */
/* U+E380 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E38F */
/* U+E390 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E39F */
/* U+E3A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E3AF */
/* U+E3B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E3BF */
/* U+E3C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E3CF */
/* U+E3D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E3DF */
/* U+E3E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E3EF */
/* U+E3F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E3FF */
/* U+E400 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E40F */
/* U+E410 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E41F */
/* U+E420 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E42F */
/* U+E430 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E43F */
/* U+E440 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E44F */
/* U+E450 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E45F */
/* U+E460 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E46F */
/* U+E470 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E47F */
/* U+E480 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E48F */
/* U+E490 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E49F */
/* U+E4A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E4AF */
/* U+E4B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E4BF */
/* U+E4C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E4CF */
/* U+E4D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E4DF */
/* U+E4E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E4EF */
/* U+E4F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E4FF */
/* U+E500 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E50F */
/* U+E510 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E51F */
/* U+E520 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E52F */
/* U+E530 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E53F */
/* U+E540 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E54F */
/* U+E550 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E55F */
/* U+E560 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E56F */
/* U+E570 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E57F */
/* U+E580 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E58F */
/* U+E590 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E59F */
/* U+E5A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E5AF */
/* U+E5B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E5BF */
/* U+E5C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E5CF */
/* U+E5D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E5DF */
/* U+E5E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E5EF */
/* U+E5F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E5FF */
/* U+E600 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E60F */
/* U+E610 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E61F */
/* U+E620 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E62F */
/* U+E630 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E63F */
/* U+E640 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E64F */
/* U+E650 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E65F */
/* U+E660 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E66F */
/* U+E670 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E67F */
/* U+E680 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E68F */
/* U+E690 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E69F */
/* U+E6A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E6AF */
/* U+E6B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E6BF */
/* U+E6C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E6CF */
/* U+E6D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E6DF */
/* U+E6E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E6EF */
/* U+E6F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E6FF */
/* U+E700 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E70F */
/* U+E710 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E71F */
/* U+E720 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E72F */
/* U+E730 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E73F */
/* U+E740 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E74F */
/* U+E750 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E75F */
/* U+E760 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E76F */
/* U+E770 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E77F */
/* U+E780 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E78F */
/* U+E790 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E79F */
/* U+E7A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E7AF */
/* U+E7B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E7BF */
/* U+E7C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E7CF */
/* U+E7D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E7DF */
/* U+E7E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E7EF */
/* U+E7F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E7FF */
/* U+E800 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E80F */
/* U+E810 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E81F */
/* U+E820 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E82F */
/* U+E830 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E83F */
/* U+E840 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E84F */
/* U+E850 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E85F */
/* U+E860 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E86F */
/* U+E870 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E87F */
/* U+E880 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E88F */
/* U+E890 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E89F */
/* U+E8A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E8AF */
/* U+E8B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E8BF */
/* U+E8C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E8CF */
/* U+E8D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E8DF */
/* U+E8E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E8EF */
/* U+E8F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E8FF */
/* U+E900 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E90F */
/* U+E910 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E91F */
/* U+E920 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E92F */
/* U+E930 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E93F */
/* U+E940 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E94F */
/* U+E950 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E95F */
/* U+E960 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E96F */
/* U+E970 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E97F */
/* U+E980 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E98F */
/* U+E990 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E99F */
/* U+E9A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E9AF */
/* U+E9B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E9BF */
/* U+E9C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E9CF */
/* U+E9D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E9DF */
/* U+E9E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E9EF */
/* U+E9F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+E9FF */
/* U+EA00 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA0F */
/* U+EA10 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA1F */
/* U+EA20 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA2F */
/* U+EA30 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA3F */
/* U+EA40 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA4F */
/* U+EA50 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA5F */
/* U+EA60 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA6F */
/* U+EA70 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA7F */
/* U+EA80 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA8F */
/* U+EA90 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EA9F */
/* U+EAA0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EAAF */
/* U+EAB0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EABF */
/* U+EAC0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EACF */
/* U+EAD0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EADF */
/* U+EAE0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EAEF */
/* U+EAF0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EAFF */
/* U+EB00 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB0F */
/* U+EB10 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB1F */
/* U+EB20 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB2F */
/* U+EB30 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB3F */
/* U+EB40 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB4F */
/* U+EB50 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB5F */
/* U+EB60 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB6F */
/* U+EB70 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB7F */
/* U+EB80 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB8F */
/* U+EB90 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EB9F */
/* U+EBA0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EBAF */
/* U+EBB0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EBBF */
/* U+EBC0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EBCF */
/* U+EBD0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EBDF */
/* U+EBE0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EBEF */
/* U+EBF0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EBFF */
/* U+EC00 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC0F */
/* U+EC10 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC1F */
/* U+EC20 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC2F */
/* U+EC30 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC3F */
/* U+EC40 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC4F */
/* U+EC50 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC5F */
/* U+EC60 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC6F */
/* U+EC70 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC7F */
/* U+EC80 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC8F */
/* U+EC90 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EC9F */
/* U+ECA0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ECAF */
/* U+ECB0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ECBF */
/* U+ECC0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ECCF */
/* U+ECD0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ECDF */
/* U+ECE0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ECEF */
/* U+ECF0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ECFF */
/* U+ED00 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED0F */
/* U+ED10 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED1F */
/* U+ED20 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED2F */
/* U+ED30 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED3F */
/* U+ED40 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED4F */
/* U+ED50 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED5F */
/* U+ED60 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED6F */
/* U+ED70 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED7F */
/* U+ED80 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED8F */
/* U+ED90 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+ED9F */
/* U+EDA0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EDAF */
/* U+EDB0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EDBF */
/* U+EDC0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EDCF */
/* U+EDD0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EDDF */
/* U+EDE0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EDEF */
/* U+EDF0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EDFF */
/* U+EE00 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE0F */
/* U+EE10 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE1F */
/* U+EE20 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE2F */
/* U+EE30 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE3F */
/* U+EE40 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE4F */
/* U+EE50 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE5F */
/* U+EE60 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE6F */
/* U+EE70 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE7F */
/* U+EE80 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE8F */
/* U+EE90 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EE9F */
/* U+EEA0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EEAF */
/* U+EEB0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EEBF */
/* U+EEC0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EECF */
/* U+EED0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EEDF */
/* U+EEE0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EEEF */
/* U+EEF0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EEFF */
/* U+EF00 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF0F */
/* U+EF10 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF1F */
/* U+EF20 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF2F */
/* U+EF30 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF3F */
/* U+EF40 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF4F */
/* U+EF50 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF5F */
/* U+EF60 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF6F */
/* U+EF70 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF7F */
/* U+EF80 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF8F */
/* U+EF90 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EF9F */
/* U+EFA0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EFAF */
/* U+EFB0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EFBF */
/* U+EFC0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EFCF */
/* U+EFD0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EFDF */
/* U+EFE0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EFEF */
/* U+EFF0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+EFFF */
/* U+F000 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F00F */
/* U+F010 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F01F */
/* U+F020 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F02F */
/* U+F030 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F03F */
/* U+F040 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F04F */
/* U+F050 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F05F */
/* U+F060 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F06F */
/* U+F070 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F07F */
/* U+F080 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F08F */
/* U+F090 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F09F */
/* U+F0A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F0AF */
/* U+F0B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F0BF */
/* U+F0C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F0CF */
/* U+F0D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F0DF */
/* U+F0E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F0EF */
/* U+F0F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F0FF */
/* U+F100 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F10F */
/* U+F110 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F11F */
/* U+F120 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F12F */
/* U+F130 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F13F */
/* U+F140 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F14F */
/* U+F150 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F15F */
/* U+F160 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F16F */
/* U+F170 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F17F */
/* U+F180 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F18F */
/* U+F190 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F19F */
/* U+F1A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F1AF */
/* U+F1B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F1BF */
/* U+F1C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F1CF */
/* U+F1D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F1DF */
/* U+F1E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F1EF */
/* U+F1F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F1FF */
/* U+F200 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F20F */
/* U+F210 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F21F */
/* U+F220 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F22F */
/* U+F230 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F23F */
/* U+F240 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F24F */
/* U+F250 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F25F */
/* U+F260 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F26F */
/* U+F270 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F27F */
/* U+F280 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F28F */
/* U+F290 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F29F */
/* U+F2A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F2AF */
/* U+F2B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F2BF */
/* U+F2C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F2CF */
/* U+F2D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F2DF */
/* U+F2E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F2EF */
/* U+F2F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F2FF */
/* U+F300 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F30F */
/* U+F310 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F31F */
/* U+F320 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F32F */
/* U+F330 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F33F */
/* U+F340 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F34F */
/* U+F350 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F35F */
/* U+F360 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F36F */
/* U+F370 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F37F */
/* U+F380 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F38F */
/* U+F390 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F39F */
/* U+F3A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F3AF */
/* U+F3B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F3BF */
/* U+F3C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F3CF */
/* U+F3D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F3DF */
/* U+F3E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F3EF */
/* U+F3F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F3FF */
/* U+F400 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F40F */
/* U+F410 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F41F */
/* U+F420 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F42F */
/* U+F430 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F43F */
/* U+F440 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F44F */
/* U+F450 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F45F */
/* U+F460 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F46F */
/* U+F470 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F47F */
/* U+F480 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F48F */
/* U+F490 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F49F */
/* U+F4A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F4AF */
/* U+F4B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F4BF */
/* U+F4C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F4CF */
/* U+F4D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F4DF */
/* U+F4E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F4EF */
/* U+F4F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F4FF */
/* U+F500 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F50F */
/* U+F510 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F51F */
/* U+F520 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F52F */
/* U+F530 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F53F */
/* U+F540 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F54F */
/* U+F550 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F55F */
/* U+F560 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F56F */
/* U+F570 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F57F */
/* U+F580 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F58F */
/* U+F590 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F59F */
/* U+F5A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F5AF */
/* U+F5B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F5BF */
/* U+F5C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F5CF */
/* U+F5D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F5DF */
/* U+F5E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F5EF */
/* U+F5F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F5FF */
/* U+F600 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F60F */
/* U+F610 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F61F */
/* U+F620 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F62F */
/* U+F630 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F63F */
/* U+F640 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F64F */
/* U+F650 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F65F */
/* U+F660 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F66F */
/* U+F670 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F67F */
/* U+F680 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F68F */
/* U+F690 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F69F */
/* U+F6A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F6AF */
/* U+F6B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F6BF */
/* U+F6C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F6CF */
/* U+F6D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F6DF */
/* U+F6E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F6EF */
/* U+F6F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F6FF */
/* U+F700 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F70F */
/* U+F710 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F71F */
/* U+F720 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F72F */
/* U+F730 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F73F */
/* U+F740 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F74F */
/* U+F750 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F75F */
/* U+F760 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F76F */
/* U+F770 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F77F */
/* U+F780 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F78F */
/* U+F790 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F79F */
/* U+F7A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F7AF */
/* U+F7B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F7BF */
/* U+F7C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F7CF */
/* U+F7D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F7DF */
/* U+F7E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F7EF */
/* U+F7F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F7FF */
/* U+F800 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F80F */
/* U+F810 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F81F */
/* U+F820 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F82F */
/* U+F830 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F83F */
/* U+F840 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F84F */
/* U+F850 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F85F */
/* U+F860 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F86F */
/* U+F870 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F87F */
/* U+F880 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F88F */
/* U+F890 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F89F */
/* U+F8A0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F8AF */
/* U+F8B0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F8BF */
/* U+F8C0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F8CF */
/* U+F8D0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F8DF */
/* U+F8E0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F8EF */
/* U+F8F0 */	PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,PU,   /* U+F8FF */
/* U+F900 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F90F */
/* U+F910 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F91F */
/* U+F920 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F92F */
/* U+F930 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F93F */
/* U+F940 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F94F */
/* U+F950 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F95F */
/* U+F960 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F96F */
/* U+F970 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F97F */
/* U+F980 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F98F */
/* U+F990 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F99F */
/* U+F9A0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F9AF */
/* U+F9B0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F9BF */
/* U+F9C0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F9CF */
/* U+F9D0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F9DF */
/* U+F9E0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F9EF */
/* U+F9F0 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+F9FF */
/* U+FA00 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+FA0F */
/* U+FA10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+FA1F */
/* U+FA20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,IL,   /* U+FA2F */
/* U+FA30 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FA3F */
/* U+FA40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FA4F */
/* U+FA50 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FA5F */
/* U+FA60 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FA6F */
/* U+FA70 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FA7F */
/* U+FA80 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FA8F */
/* U+FA90 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FA9F */
/* U+FAA0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FAAF */
/* U+FAB0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FABF */
/* U+FAC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FACF */
/* U+FAD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FADF */
/* U+FAE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FAEF */
/* U+FAF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FAFF */
/* U+FB00 */	1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FB0F */
/* U+FB10 */	IL,IL,IL,1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,0, 1,    /* U+FB1F */
/* U+FB20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FB2F */
/* U+FB30 */	1, 1, 1, 1, 1, 1, 1, IL,1, 1, 1, 1, 1, IL,1, IL,   /* U+FB3F */
/* U+FB40 */	1, 1, IL,1, 1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FB4F */
/* U+FB50 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FB5F */
/* U+FB60 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FB6F */
/* U+FB70 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FB7F */
/* U+FB80 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FB8F */
/* U+FB90 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FB9F */
/* U+FBA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FBAF */
/* U+FBB0 */	1, 1, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FBBF */
/* U+FBC0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FBCF */
/* U+FBD0 */	IL,IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FBDF */
/* U+FBE0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FBEF */
/* U+FBF0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FBFF */
/* U+FC00 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC0F */
/* U+FC10 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC1F */
/* U+FC20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC2F */
/* U+FC30 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC3F */
/* U+FC40 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC4F */
/* U+FC50 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC5F */
/* U+FC60 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC6F */
/* U+FC70 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC7F */
/* U+FC80 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC8F */
/* U+FC90 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FC9F */
/* U+FCA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FCAF */
/* U+FCB0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FCBF */
/* U+FCC0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FCCF */
/* U+FCD0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FCDF */
/* U+FCE0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FCEF */
/* U+FCF0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FCFF */
/* U+FD00 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FD0F */
/* U+FD10 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FD1F */
/* U+FD20 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FD2F */
/* U+FD30 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FD3F */
/* U+FD40 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FD4F */
/* U+FD50 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FD5F */
/* U+FD60 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FD6F */
/* U+FD70 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FD7F */
/* U+FD80 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FD8F */
/* U+FD90 */	IL,IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FD9F */
/* U+FDA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FDAF */
/* U+FDB0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FDBF */
/* U+FDC0 */	1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FDCF */
/* U+FDD0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FDDF */
/* U+FDE0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FDEF */
/* U+FDF0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,IL,   /* U+FDFF */
/* U+FE00 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FE0F */
/* U+FE10 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FE1F */
/* U+FE20 */	0, 0, 0, 0, IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,IL,   /* U+FE2F */
/* U+FE30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+FE3F */
/* U+FE40 */	2, 2, 2, 2, 2, IL,IL,IL,IL,2, 2, 2, 2, 2, 2, 2,    /* U+FE4F */
/* U+FE50 */	2, 2, 2, IL,2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+FE5F */
/* U+FE60 */	2, 2, 2, 2, 2, 2, 2, IL,2, 2, 2, 2, IL,IL,IL,IL,   /* U+FE6F */
/* U+FE70 */	1, 1, 1, IL,1, IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FE7F */
/* U+FE80 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FE8F */
/* U+FE90 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FE9F */
/* U+FEA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FEAF */
/* U+FEB0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FEBF */
/* U+FEC0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FECF */
/* U+FED0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FEDF */
/* U+FEE0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FEEF */
/* U+FEF0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,IL,IL,   /* U+FEFF */
/* U+FF00 */	IL,2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+FF0F */
/* U+FF10 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+FF1F */
/* U+FF20 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+FF2F */
/* U+FF30 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+FF3F */
/* U+FF40 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,    /* U+FF4F */
/* U+FF50 */	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, IL,   /* U+FF5F */
/* U+FF60 */	IL,1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FF6F */
/* U+FF70 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FF7F */
/* U+FF80 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FF8F */
/* U+FF90 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FF9F */
/* U+FFA0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,    /* U+FFAF */
/* U+FFB0 */	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, IL,   /* U+FFBF */
/* U+FFC0 */	IL,IL,1, 1, 1, 1, 1, 1, IL,IL,1, 1, 1, 1, 1, 1,    /* U+FFCF */
/* U+FFD0 */	IL,IL,1, 1, 1, 1, 1, 1, IL,IL,1, 1, 1, IL,IL,IL,   /* U+FFDF */
/* U+FFE0 */	2, 2, 2, 2, 2, 2, 2, IL,1, 1, 1, 1, 1, 1, 1, IL,   /* U+FFEF */
/* U+FFF0 */	IL,IL,IL,IL,IL,IL,IL,IL,IL,0, 0, 0, 1, 1, IL,IL    /* U+FFFF */
	}
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_UWIDTH_H */
