/*
 * Copyright 1990 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL M.I.T.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, MIT X Consortium
 */

#pragma ident	"@(#)readfont.c	1.11	98/04/14 SMI"

#include	<rpc/types.h>
#include	<sys/kmem.h>
#include	<sys/cred.h>
#include	<sys/font.h>
#include	<sys/sunddi.h>
#include	<sys/kobj.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/errno.h>
#include <sys/modctl.h>

extern struct fontlist fonts[];
bitmap_data_t *read_fontfile(char *);

/*
 * Module linkage information for the kernel.
 */
static struct modlmisc modlmisc = {
	&mod_miscops, "readfont",
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlmisc, NULL
};

int
_init(void)
{
	struct fontlist *fl;
	int e;

	for (fl = fonts; fl->name; fl++) {
		fl->fontload = read_fontfile;
	}
	if ((e = mod_install(&modlinkage)) != 0) {
		for (fl = fonts; fl->name; fl++) {
			fl->fontload = NULL;
		}
	}
	return (e);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

int
_fini(void)
{
	int e;
	struct fontlist *fl;

	if ((e = mod_remove(&modlinkage)) != 0)
		return (e);

	for (fl = fonts; fl->name; fl++) {
		fl->fontload = NULL;
	}

	return (e);
}

extern int on_fault(label_t *);
extern void no_fault(void);
static void freeup_mem(void);

#define	MSBFirst	0
#define	LSBFirst	1
#define	NO_SUCH_CHAR	-1
#define	AllocError	80
#define	StillWorking	81
#define	FontNameAlias	82
#define	BadFontName	83
#define	Suspended	84
#define	Successful	85
#define	BadFontPath	86
#define	BadCharRange	87
#define	BadFontFormat	88
#define	FPEResetFailed	89
#define	LoadAll		0x1

typedef int		Bool;

#define	B32
#define	B16
typedef long		INT32;
typedef short		INT16;
#if __STDC__ || defined(sgi) || defined(AIXV3)
typedef signed char	INT8;
#else
typedef char		INT8;
#endif

typedef unsigned long	CARD32;
typedef unsigned short	CARD16;
typedef unsigned char	CARD8;
typedef unsigned long	BITS32;
typedef unsigned short	BITS16;
typedef unsigned char	BYTE;
typedef unsigned char	BOOL;

void FontDefaultFormat(int *, int *, int *, int *);
int RepadBitmap(char *, char *, unsigned, unsigned, int, int);

/*
 * from Xproto.h
 */

typedef struct {
	INT16 leftSideBearing B16,
		rightSideBearing B16,
		characterWidth B16,
		ascent B16,
		descent B16;
	CARD16 attributes B16;
} xCharInfo;

/* end Xproto.h */

/* from fonts/include/FSproto.h */

/* temp decls */
#define	Mask		CARD32
#define	Font		CARD32
#define	AccContext	CARD32

typedef struct {
	CARD8	low,
		high;
} fsChar2b;

typedef struct {
	fsChar2b	min_char,
		max_char;
} fsRange;

typedef struct	{
	CARD32	position B32;
	CARD32	length B32;
} fsOffset;

typedef struct {
	INT16	left B16,
		right B16;
	INT16	width B16;
	INT16	ascent B16,
	descent	B16;
	CARD16	attributes B16;
} fsCharInfo;
/* end fonts/include/FSproto.h */


unsigned long metrics_ptr;
unsigned long bitmap_ptr;
unsigned long ink_metrics_ptr;
unsigned long encoding_ptr;
void *props_ptr;
void *isStringProp_ptr;
void *bitmapFont_ptr;
int metrics_size;
int bitmap_size;
int ink_metrics_size;
int encoding_size;
int props_size;
int isStringProp_size;
int bitmapFont_size;

#define	xalloc(n)	kmem_zalloc((int)n, KM_SLEEP)
#define	xfree(p, n)	if (p) kmem_free((unsigned char *)p, n)

#define	assert(x)

/* font format macros */
#define	BitmapFormatByteOrderMask	(1L << 0)
#define	BitmapFormatBitOrderMask	(1L << 1)
#define	BitmapFormatImageRectMask	(3L << 2)
#define	BitmapFormatScanlinePadMask	(3L << 8)
#define	BitmapFormatScanlineUnitMask	(3L << 12)

#define	BitmapFormatByteOrderLSB	(0)
#define	BitmapFormatByteOrderMSB	(1L << 0)
#define	BitmapFormatBitOrderLSB		(0)
#define	BitmapFormatBitOrderMSB		(1L << 1)

#define	BitmapFormatImageRectMin	(0L << 2)
#define	BitmapFormatImageRectMaxWidth	(1L << 2)
#define	BitmapFormatImageRectMax	(2L << 2)

#define	BitmapFormatScanlinePad8	(0L << 8)
#define	BitmapFormatScanlinePad16	(1L << 8)
#define	BitmapFormatScanlinePad32	(2L << 8)
#define	BitmapFormatScanlinePad64	(3L << 8)

#define	BitmapFormatScanlineUnit8	(0L << 12)
#define	BitmapFormatScanlineUnit16	(1L << 12)
#define	BitmapFormatScanlineUnit32	(2L << 12)
#define	BitmapFormatScanlineUnit64	(3L << 12)

#define	BitmapFormatMaskByte		(1L << 0)
#define	BitmapFormatMaskBit		(1L << 1)
#define	BitmapFormatMaskImageRectangle	(1L << 2)
#define	BitmapFormatMaskScanLinePad	(1L << 3)
#define	BitmapFormatMaskScanLineUnit	(1L << 4)

typedef unsigned long	fsBitmapFormat;
typedef unsigned long	fsBitmapFormatMask;

/*
 * this type is for people who want to talk about the font encoding
 */

typedef enum {
	Linear8Bit, TwoD8Bit, Linear16Bit, TwoD16Bit
} FontEncoding;

typedef struct _CharInfo {
	xCharInfo metrics;	/* info preformatted for Queries */
	char	*bits;		/* pointer to glyph image */
} CharInfoRec;

typedef struct _CharInfo *CharInfoPtr;

typedef struct _FontProp {
	CARD32	name;		/* offset of string */
	INT32	value;		/* number or offset of string */
	int	indirect;	/* value is a string offset */
} FontPropRec;

typedef struct _FontProp *FontPropPtr;

typedef struct _FontInfo {
	unsigned short firstCol;
	unsigned short lastCol;
	unsigned short firstRow;
	unsigned short lastRow;
	unsigned short defaultCh;
	unsigned int noOverlap:1;
	unsigned int terminalFont:1;
	unsigned int constantMetrics:1;
	unsigned int constantWidth:1;
	unsigned int inkInside:1;
	unsigned int inkMetrics:1;
	unsigned int allExist:1;
	unsigned int drawDirection:2;
	unsigned int cachable:1;
	unsigned int anamorphic:1;
	short	maxOverlap;
	short	pad;
	xCharInfo	maxbounds;
	xCharInfo	minbounds;
	xCharInfo	ink_maxbounds;
	xCharInfo	ink_minbounds;
	short	fontAscent;
	short	fontDescent;
	int	 nprops;
	FontPropPtr props;
	char	*isStringProp;
} FontInfoRec;

typedef struct _FontInfo *FontInfoPtr;

/* External view of font paths */
typedef struct _FontPathElement {
	int	 name_length;
	char	*name;
	int	 type;
	int	 refcount;
	unsigned char *private;
} FontPathElementRec;

typedef struct _FontPathElement *FontPathElementPtr;

/*
 *	(*get_glyphs)(font, count, chars, encoding, count, glyphs);
 *	(*get_metrics)(font, count, chars, encoding, count, glyphs);
 *	(*get_extents)(client, font, format, flags, ranges,
 */

typedef struct _Font *FontPtr;

typedef struct _Font {
	int	 refcnt;
	FontInfoRec info;
	char	bit;
	char	byte;
	char	glyph;
	char	scan;
	fsBitmapFormat format;
	int	 (*get_glyphs)();
	int	 (*get_metrics)();
#ifdef	__STDC__
	int	 (*get_bitmaps)(
		unsigned char *client,
		FontPtr		pfont,
		fsBitmapFormat	format,
		Mask		flags,
		unsigned long	num_ranges,
		fsRange		*range,
		int		*size,
		unsigned long	*num_glyphs,
		fsOffset	**offsets,
		int		*offsets_size,
		unsigned char	**data,
		int		*freeData);
#else
	int	 (*get_bitmaps)();
#endif
	int	 (*get_extents)();
	void	(*unload_font)();
	FontPathElementPtr fpe;
	unsigned char *svrPrivate;
	unsigned char *fontPrivate;
	unsigned char *fpePrivate;
	int		maxPrivate;
	unsigned char **devPrivates;
} FontRec;

extern int	bitmapReadFont(), bitmapReadFontInfo();
extern int	bitmapGetGlyphs(), bitmapGetMetrics();
extern int	bitmapGetBitmaps(), bitmapGetExtents();
extern void	bitmapUnloadFont();

#define	Atom CARD32
#define	GLYPHPADOPTIONS 4	/* 1, 2, 4, or 8 */

typedef struct _BitmapExtra {
	Atom	*glyphNames;
	int	*sWidths;
	CARD32	bitmapsSizes[GLYPHPADOPTIONS];
	FontInfoRec info;
}BitmapExtraRec, *BitmapExtraPtr;

typedef struct _BitmapFont {
	unsigned	version_num;
	int	 num_chars;
	int	 num_tables;
	CharInfoPtr metrics;	/* font metrics, including glyph pointers */
	xCharInfo	*ink_metrics;	/* ink metrics */
	unsigned char *bitmaps;	/* base of bitmaps, useful only to free */
	CharInfoPtr *encoding;	/* array of char info pointers */
	CharInfoPtr pDefault;	/* default character */
	BitmapExtraPtr bitmapExtra;	/* stuff not used by X server */
}BitmapFontRec, *BitmapFontPtr;

static int fontpos = 0;

static struct _buf *fontfd;

FontFileRead(buf, n)
char	*buf;
int	n;
{
	int	bytesread;

	bytesread = kobj_read_file(fontfd, buf, n, fontpos);
	fontpos += bytesread;
	return (bytesread);
}

unsigned char
FontFileGetc()
{
	char	ch;
	int	bytesread;

	bytesread = kobj_read_file(fontfd, &ch, 1, fontpos);
	fontpos += bytesread;
	return (ch);
}

void
FontFileSkip(offset)
{
	fontpos += offset;
}


void
FontFileSeek(offset)
{
	fontpos = offset;
}


typedef struct _PCFTable {
	CARD32	type;
	CARD32	format;
	CARD32	size;
	CARD32	offset;
} PCFTableRec, *PCFTablePtr;

#define	PCF_FILE_VERSION	(('p'<<24)|('c'<<16)|('f'<<8)|1)
#define	PCF_FORMAT_MASK		0xffffff00

#define	PCF_DEFAULT_FORMAT	0x00000000
#define	PCF_INKBOUNDS		0x00000200
#define	PCF_ACCEL_W_INKBOUNDS	0x00000100
#define	PCF_COMPRESSED_METRICS	0x00000100

#define	PCF_FORMAT_MATCH(a, b) (((a)&PCF_FORMAT_MASK) == ((b)&PCF_FORMAT_MASK))

#define	PCF_GLYPH_PAD_MASK	(3<<0)
#define	PCF_BYTE_MASK		(1<<2)
#define	PCF_BIT_MASK		(1<<3)
#define	PCF_SCAN_UNIT_MASK	(3<<4)

#define	PCF_BYTE_ORDER(f)	(((f) & PCF_BYTE_MASK)?MSBFirst:LSBFirst)
#define	PCF_BIT_ORDER(f)	(((f) & PCF_BIT_MASK)?MSBFirst:LSBFirst)
#define	PCF_GLYPH_PAD_INDEX(f)	((f) & PCF_GLYPH_PAD_MASK)
#define	PCF_GLYPH_PAD(f)	(1<<PCF_GLYPH_PAD_INDEX(f))
#define	PCF_SCAN_UNIT_INDEX(f)	(((f) & PCF_SCAN_UNIT_MASK) >> 4)
#define	PCF_SCAN_UNIT(f)	(1<<PCF_SCAN_UNIT_INDEX(f))
#define	PCF_FORMAT_BITS(f)	((f) & (PCF_GLYPH_PAD_MASK|PCF_BYTE_MASK| \
				    PCF_BIT_MASK|PCF_SCAN_UNIT_MASK))

#define	PCF_SIZE_TO_INDEX(s)	((s) == 4 ? 2 : (s) == 2 ? 1 : 0)
#define	PCF_INDEX_TO_SIZE(b)	(1<<b)

#define	PCF_FORMAT(bit, byte, glyph, scan) (\
	(PCF_SIZE_TO_INDEX(scan) << 4) | \
	(((bit) == MSBFirst ? 1 : 0) << 3) | \
	(((byte) == MSBFirst ? 1 : 0) << 2) | \
	(PCF_SIZE_TO_INDEX(glyph) << 0))

#define	PCF_PROPERTIES			(1<<0)
#define	PCF_ACCELERATORS		(1<<1)
#define	PCF_METRICS			(1<<2)
#define	PCF_BITMAPS			(1<<3)
#define	PCF_INK_METRICS			(1<<4)
#define	PCF_BDF_ENCODINGS		(1<<5)
#define	PCF_SWIDTHS			(1<<6)
#define	PCF_GLYPH_NAMES			(1<<7)
#define	PCF_BDF_ACCELERATORS		(1<<8)

void TwoByteSwap(unsigned char *, int);
void FourByteSwap(unsigned char *, int);
void BitOrderInvert(unsigned char *, int);

/* from fonts/lib/font/fontfile/defaults.c */

#ifndef DEFAULT_BIT_ORDER
#ifdef BITMAP_BIT_ORDER
#define	DEFAULT_BIT_ORDER BITMAP_BIT_ORDER
#else
#define	DEFAULT_BIT_ORDER MSBFirst
#endif
#endif

#ifndef DEFAULT_BYTE_ORDER
#ifdef IMAGE_BYTE_ORDER
#define	DEFAULT_BYTE_ORDER IMAGE_BYTE_ORDER
#else
#define	DEFAULT_BYTE_ORDER MSBFirst
#endif
#endif

#ifndef DEFAULT_GLYPH_PAD
#ifdef GLYPHPADBYTES
#define	DEFAULT_GLYPH_PAD GLYPHPADBYTES
#else
#define	DEFAULT_GLYPH_PAD 4
#endif
#endif

#ifndef DEFAULT_SCAN_UNIT
#define	DEFAULT_SCAN_UNIT 1
#endif


/* from fonts/lib/font/bitmap/fsfuncs.c */

#define	GLWIDTHBYTESPADDED(bits, nbytes) \
	((nbytes) == 1 ? (((bits)+7)>>3)	/* pad to 1 byte */ \
	:(nbytes) == 2 ? ((((bits)+15)>>3)&~1)	/* pad to 2 bytes */ \
	:(nbytes) == 4 ? ((((bits)+31)>>3)&~3)	/* pad to 4 bytes */ \
	:(nbytes) == 8 ? ((((bits)+63)>>3)&~7)	/* pad to 8 bytes */ \
	: 0)

#define	GLYPH_SIZE(ch, nbytes)		\
	GLWIDTHBYTESPADDED((ch)->metrics.rightSideBearing - \
	(ch)->metrics.leftSideBearing, (nbytes))


#define	n2dChars(pfi)	(((pfi)->lastRow - (pfi)->firstRow + 1) * \
			    ((pfi)->lastCol - (pfi)->firstCol + 1))

static CharInfoRec junkDefault;

/*
 * Macros for computing different bounding boxes for fonts; from
 * the font protocol
 */

#define	FONT_MAX_ASCENT(pi)	((pi)->fontAscent > (pi)->ink_maxbounds.ascent \
				? (pi)->fontAscent : (pi)->ink_maxbounds.ascent)
#define	FONT_MAX_DESCENT(pi)	((pi)->fontDescent > \
				    (pi)->ink_maxbounds.descent ? \
				(pi)->fontDescent : (pi)->ink_maxbounds.descent)
#define	FONT_MAX_HEIGHT(pi)	(FONT_MAX_ASCENT(pi) + FONT_MAX_DESCENT(pi))
#define	FONT_MIN_LEFT(pi)	((pi)->ink_minbounds.leftSideBearing < 0 ? \
				(pi)->ink_minbounds.leftSideBearing : 0)
#define	FONT_MAX_RIGHT(pi)	((pi)->ink_maxbounds.rightSideBearing > \
				(pi)->ink_maxbounds.characterWidth ? \
				(pi)->ink_maxbounds.rightSideBearing : \
				(pi)->ink_maxbounds.characterWidth)
#define	FONT_MAX_WIDTH(pi)	(FONT_MAX_RIGHT(pi) - FONT_MIN_LEFT(pi))

/* from fonts/lib/font/util/bitmaputil.c */

static unsigned char _reverse_byte[0x100] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

/* Read PCF font files */

static int
pcfGetLSB32()
{
	int	 c;

	c = FontFileGetc();
	c |= FontFileGetc() << 8;
	c |= FontFileGetc() << 16;
	c |= FontFileGetc() << 24;
	return (c);
}

static int
pcfGetINT32(format)
	CARD32	format;
{
	int	 c;

	if (PCF_BYTE_ORDER(format) == MSBFirst) {
		c = FontFileGetc() << 24;
		c |= FontFileGetc() << 16;
		c |= FontFileGetc() << 8;
		c |= FontFileGetc();
	} else {
		c = FontFileGetc();
		c |= FontFileGetc() << 8;
		c |= FontFileGetc() << 16;
		c |= FontFileGetc() << 24;
	}
	return (c);
}

static int
pcfGetINT16(format)
	CARD32	format;
{
	int	 c;

	if (PCF_BYTE_ORDER(format) == MSBFirst) {
		c = FontFileGetc() << 8;
		c |= FontFileGetc();
	} else {
		c = FontFileGetc();
		c |= FontFileGetc() << 8;
	}
	return (c);
}

#define	pcfGetINT8(format) (FontFileGetc())

static	PCFTablePtr
pcfReadTOC(countp)
	int	*countp;
{
	CARD32	version;
	PCFTablePtr tables;
	int	 count;
	int	 i;

	fontpos = 0;
	version = pcfGetLSB32();
	if (version != PCF_FILE_VERSION)
		return ((PCFTablePtr)NULL);
	count = pcfGetLSB32();
	tables = (PCFTablePtr) xalloc(count * sizeof (PCFTableRec));
	if (!tables)
		return ((PCFTablePtr)NULL);
	for (i = 0; i < count; i++) {
		tables[i].type = pcfGetLSB32();
		tables[i].format = pcfGetLSB32();
		tables[i].size = pcfGetLSB32();
		tables[i].offset = pcfGetLSB32();
	}
	*countp = count;
	return (tables);
}

/*
 * PCF supports two formats for metrics, both the regular
 * jumbo size, and 'lite' metrics, which are useful
 * for most fonts which have even vaguely reasonable
 * metrics
 */

static void
pcfGetMetric(format, metric)
	CARD32	format;
	xCharInfo	*metric;
{
	metric->leftSideBearing = pcfGetINT16(format);
	metric->rightSideBearing = pcfGetINT16(format);
	metric->characterWidth = pcfGetINT16(format);
	metric->ascent = pcfGetINT16(format);
	metric->descent = pcfGetINT16(format);
	metric->attributes = pcfGetINT16(format);
}

/*ARGSUSED*/
static void
pcfGetCompressedMetric(format, metric)
	CARD32	format;
	xCharInfo	*metric;
{
	metric->leftSideBearing = pcfGetINT8(format) - 0x80;
	metric->rightSideBearing = pcfGetINT8(format) - 0x80;
	metric->characterWidth = pcfGetINT8(format) - 0x80;
	metric->ascent = pcfGetINT8(format) - 0x80;
	metric->descent = pcfGetINT8(format) - 0x80;
	metric->attributes = 0;
}

/*
 * Position the file to the begining of the specified table
 * in the font file
 */
static Bool
pcfSeekToType(tables, ntables, type, formatp, sizep)
	PCFTablePtr tables;
	int	 ntables;
	CARD32	type;
	CARD32	*formatp;
	CARD32	*sizep;
{
	int	 i;

	for (i = 0; i < ntables; i++)
	if (tables[i].type == type) {
		if (fontpos > (int)tables[i].offset) {
			return (FALSE);
		}

		FontFileSeek((int)tables[i].offset);
		*sizep = tables[i].size;
		*formatp = tables[i].format;
		return (TRUE);
	}
	return (FALSE);
}

static Bool
pcfHasType(tables, ntables, type)
	PCFTablePtr tables;
	int	 ntables;
	CARD32	type;
{
	int	 i;

	for (i = 0; i < ntables; i++)
		if (tables[i].type == type)
			return (TRUE);
	return (FALSE);
}

/*
 * pcfGetProperties
 *
 * Reads the font properties from the font file, filling in the FontInfo rec
 * supplied.  Used by by both ReadFont and ReadFontInfo routines.
 */

static Bool
pcfGetProperties(pFontInfo, tables, ntables)
	FontInfoPtr pFontInfo;
	PCFTablePtr tables;
	int	 ntables;
{
	FontPropPtr props = 0;
	int	 nprops;
	char	*isStringProp = 0;
	CARD32	format;
	int	 i;
	int	 size;
	int	 string_size;
	char	*strings = 0;

	/* font properties */

	if (!pcfSeekToType(tables, ntables, (CARD32)PCF_PROPERTIES, &format,
		    (CARD32 *)&size))
		goto Bail;
	format = pcfGetLSB32();
	if (!PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT))
		goto Bail;
	nprops = pcfGetINT32(format);
	props_size = nprops * sizeof (FontPropRec);
	props = (FontPropPtr) xalloc(props_size);
	if (!props)
		goto Bail;
	props_ptr = props;
	isStringProp_size = nprops * sizeof (char);
	isStringProp = (char *)xalloc(isStringProp_size);
	if (!isStringProp)
		goto Bail;
	isStringProp_ptr = isStringProp;
	for (i = 0; i < nprops; i++) {
		props[i].name = pcfGetINT32(format);
		isStringProp[i] = pcfGetINT8(format);
		props[i].value = pcfGetINT32(format);
	}
	/* pad the property array */
	/*
	 * clever here - nprops is the same as the number of odd-units read, as
	 * only isStringProp are odd length
	 */
	if (nprops & 3) {
		i = 4 - (nprops & 3);
		FontFileSkip(i);
	}
	string_size = pcfGetINT32(format);
	strings = (char *)xalloc(string_size);
	if (!strings) {
		goto Bail;
	}
	FontFileRead(strings, string_size);
	xfree(strings, string_size);
	pFontInfo->isStringProp = isStringProp;
	pFontInfo->props = props;
	pFontInfo->nprops = nprops;
	return (TRUE);
Bail:
	xfree(isStringProp, nprops * sizeof (char));
	xfree(props, nprops * sizeof (FontPropRec));
	return (FALSE);
}


/*
 * pcfReadAccel
 *
 * Fill in the accelerator information from the font file; used
 * to read both BDF_ACCELERATORS and old style ACCELERATORS
 */

static Bool
pcfGetAccel(pFontInfo, tables, ntables, type)
	FontInfoPtr pFontInfo;
	PCFTablePtr	tables;
	int		ntables;
	CARD32	type;
{
	CARD32	format;
	int		size;

	if (!pcfSeekToType(tables, ntables, (CARD32)type, &format,
		    (CARD32 *)&size))
		goto Bail;
	format = pcfGetLSB32();
	if (!PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT) &&
	!PCF_FORMAT_MATCH(format, PCF_ACCEL_W_INKBOUNDS)) {
		goto Bail;
	}
	pFontInfo->noOverlap = pcfGetINT8(format);
	pFontInfo->constantMetrics = pcfGetINT8(format);
	pFontInfo->terminalFont = pcfGetINT8(format);
	pFontInfo->constantWidth = pcfGetINT8(format);
	pFontInfo->inkInside = pcfGetINT8(format);
	pFontInfo->inkMetrics = pcfGetINT8(format);
	pFontInfo->drawDirection = pcfGetINT8(format);
	pFontInfo->anamorphic = FALSE;
	/* natural alignment */ pcfGetINT8(format);
	pFontInfo->fontAscent = pcfGetINT32(format);
	pFontInfo->fontDescent = pcfGetINT32(format);
	pFontInfo->maxOverlap = pcfGetINT32(format);
	pcfGetMetric(format, &pFontInfo->minbounds);
	pcfGetMetric(format, &pFontInfo->maxbounds);
	if (PCF_FORMAT_MATCH(format, PCF_ACCEL_W_INKBOUNDS)) {
		pcfGetMetric(format, &pFontInfo->ink_minbounds);
		pcfGetMetric(format, &pFontInfo->ink_maxbounds);
	} else {
		pFontInfo->ink_minbounds = pFontInfo->minbounds;
		pFontInfo->ink_maxbounds = pFontInfo->maxbounds;
	}
	return (TRUE);
Bail:
	return (FALSE);
}


int
pcfReadFont(pFont, bit, byte, glyph, scan)
	FontPtr	pFont;
	int	 bit,
		byte,
		glyph,
		scan;
{
	CARD32	format;
	CARD32	size;
	BitmapFontPtr	bitmapFont = 0;
	int	 i;
	PCFTablePtr tables = 0;
	int	 ntables;
	int	 nmetrics;
	int	 nbitmaps;
	int	 sizebitmaps;
	int	 nink_metrics;
	CharInfoPtr metrics = 0;
	xCharInfo	*ink_metrics = 0;
	unsigned char	*bitmaps = 0;
	CharInfoPtr *encoding;
	int	 nencoding;
	int	 encodingOffset;
	CARD32	bitmapSizes[GLYPHPADOPTIONS];
	CARD32	*offsets;
	Bool	hasBDFAccelerators;

	tables = 0;
	metrics = 0;
	offsets = 0;
	bitmaps = 0;
	ink_metrics = 0;
	encoding = 0;
	bitmapFont = 0;
	pFont->format = 0;

	if (!(tables = pcfReadTOC(&ntables))) {
		goto Bail;
	}

	/* properties */

	if (!pcfGetProperties(&pFont->info, tables, ntables)) {
		goto Bail;
	}

	/* Use the old accelerators if no BDF accelerators are in the file */

	hasBDFAccelerators = pcfHasType(tables, ntables,
				(CARD32)PCF_BDF_ACCELERATORS);
	if (!hasBDFAccelerators)
		if (!pcfGetAccel(&pFont->info, tables, ntables,
				(CARD32)PCF_ACCELERATORS)) {
			goto Bail;
		}

	/* metrics */

	if (!pcfSeekToType(tables, ntables, (CARD32)PCF_METRICS, &format,
		    &size)) {
		goto Bail;
	}
	format = pcfGetLSB32();
	if (!PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT) &&
			!PCF_FORMAT_MATCH(format, PCF_COMPRESSED_METRICS)) {
		goto Bail;
	}
	if (PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT))
		nmetrics = pcfGetINT32(format);
	else
		nmetrics = pcfGetINT16(format);
	metrics_size = nmetrics * sizeof (CharInfoRec);
	metrics = (CharInfoPtr) xalloc(metrics_size);
	metrics_ptr = (unsigned long)metrics;
	if (!metrics) {
		goto Bail;
	}
	for (i = 0; i < nmetrics; i++) {
		if (PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT))
			pcfGetMetric(format, &(metrics + i)->metrics);
		else
			pcfGetCompressedMetric(format, &(metrics + i)->metrics);
	}

	/* bitmaps */

	if (!pcfSeekToType(tables, ntables, (CARD32)PCF_BITMAPS, &format,
		    &size)) {
		goto Bail;
	}
	format = pcfGetLSB32();
	if (!PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT)) {
		goto Bail;
	}

	nbitmaps = pcfGetINT32(format);
	if (nbitmaps != nmetrics) {
		goto Bail;
	}

	offsets = (CARD32 *) xalloc(nbitmaps * sizeof (CARD32));
	if (!offsets) {
		goto Bail;
	}

	for (i = 0; i < nbitmaps; i++)
		offsets[i] = pcfGetINT32(format);

	for (i = 0; i < GLYPHPADOPTIONS; i++)
		bitmapSizes[i] = pcfGetINT32(format);
	sizebitmaps = bitmapSizes[PCF_GLYPH_PAD_INDEX(format)];
	bitmaps = (unsigned char *) xalloc(sizebitmaps);
	bitmap_ptr = (unsigned long)bitmaps;
	bitmap_size = sizebitmaps;
	if (!bitmaps) {
		goto Bail;
	}
	FontFileRead((char *)bitmaps, sizebitmaps);

	if (PCF_BIT_ORDER(format) != bit)
	BitOrderInvert(bitmaps, sizebitmaps);
	if ((PCF_BYTE_ORDER(format) == PCF_BIT_ORDER(format)) !=
		(bit == byte)) {
		switch (bit == byte ? PCF_SCAN_UNIT(format) : scan) {
		case 1:
			break;
		case 2:
			TwoByteSwap(bitmaps, sizebitmaps);
			break;
		case 4:
			FourByteSwap(bitmaps, sizebitmaps);
			break;
		}
	}
	if (PCF_GLYPH_PAD(format) != glyph) {
		char	*padbitmaps;
		int	 sizepadbitmaps;
		int	 old,
				new;
		xCharInfo	*metric;

		padbitmaps = 0;
		sizepadbitmaps = bitmapSizes[PCF_SIZE_TO_INDEX(glyph)];
		padbitmaps = (char *)xalloc(sizepadbitmaps);
		if (!padbitmaps) {
			goto Bail;
		}
		new = 0;
		for (i = 0; i < nbitmaps; i++) {
			old = offsets[i];
			metric = &metrics[i].metrics;
			offsets[i] = new;
			new += RepadBitmap((char *)bitmaps + old,
					padbitmaps + new,
					PCF_GLYPH_PAD(format), glyph,
					metric->rightSideBearing -
					metric->leftSideBearing,
					metric->ascent + metric->descent);
		}
		xfree(bitmaps, sizebitmaps);
		bitmaps = (unsigned char *)padbitmaps;
		sizebitmaps = sizepadbitmaps;
		bitmap_ptr = (unsigned long)bitmaps;
		bitmap_size = sizebitmaps;
	}
	for (i = 0; i < nbitmaps; i++)
		metrics[i].bits = (char *)(bitmaps + offsets[i]);

	xfree(offsets, nbitmaps * sizeof (CARD32));
	offsets = NULL;

	/* ink metrics ? */

	ink_metrics = NULL;
	if (pcfSeekToType(tables, ntables, (CARD32)PCF_INK_METRICS, &format,
		    &size)) {
		format = pcfGetLSB32();
		if (!PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT) &&
			!PCF_FORMAT_MATCH(format, PCF_COMPRESSED_METRICS)) {
			goto Bail;
		}
		if (PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT))
			nink_metrics = pcfGetINT32(format);
		else
			nink_metrics = pcfGetINT16(format);
		if (nink_metrics != nmetrics) {
			goto Bail;
		}
		ink_metrics_size = nink_metrics * sizeof (xCharInfo);
		ink_metrics = (xCharInfo *) xalloc(ink_metrics_size);
		ink_metrics_ptr = (unsigned long)ink_metrics;
		if (!ink_metrics) {
			goto Bail;
		}
		for (i = 0; i < nink_metrics; i++)
			if (PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT))
				pcfGetMetric(format, ink_metrics + i);
			else
				pcfGetCompressedMetric(format, ink_metrics + i);
	}

	/* encoding */

	if (!pcfSeekToType(tables, ntables, (CARD32)PCF_BDF_ENCODINGS, &format,
		    &size)) {
		goto Bail;
	}
	format = pcfGetLSB32();
	if (!PCF_FORMAT_MATCH(format, PCF_DEFAULT_FORMAT)) {
		goto Bail;
	}

	pFont->info.firstCol = pcfGetINT16(format);
	pFont->info.lastCol = pcfGetINT16(format);
	pFont->info.firstRow = pcfGetINT16(format);
	pFont->info.lastRow = pcfGetINT16(format);
	pFont->info.defaultCh = pcfGetINT16(format);

	nencoding = (pFont->info.lastCol - pFont->info.firstCol + 1) *
	(pFont->info.lastRow - pFont->info.firstRow + 1);

	encoding_size = nencoding * sizeof (CharInfoPtr);
	encoding = (CharInfoPtr *) xalloc(encoding_size);
	encoding_ptr = (unsigned long)encoding;
	if (!encoding) {
		goto Bail;
	}

	pFont->info.allExist = TRUE;
	for (i = 0; i < nencoding; i++) {
		encodingOffset = pcfGetINT16(format);
		if (encodingOffset == 0xFFFF) {
			pFont->info.allExist = FALSE;
			encoding[i] = 0;
		} else {
			encoding[i] = metrics + encodingOffset;
		}
	}

	/* BDF style accelerators (i.e. bounds based on encoded glyphs) */

	if (hasBDFAccelerators)
	if (!pcfGetAccel(&pFont->info, tables, ntables,
		    (CARD32)PCF_BDF_ACCELERATORS)) {
		goto Bail;
	}

	if (!pFont->info.constantWidth) {
		printf("pcfReadFont: non-constant width font, flag %x\n",
			pFont->info.constantWidth);
		goto Bail;
	}

	bitmapFont_size = sizeof (*bitmapFont);
	bitmapFont = (BitmapFontPtr) xalloc(sizeof (*bitmapFont));
	if (!bitmapFont) {
		goto Bail;
	}
	bitmapFont_ptr = bitmapFont;

	bitmapFont->version_num = PCF_FILE_VERSION;
	bitmapFont->num_chars = nmetrics;
	bitmapFont->num_tables = ntables;
	bitmapFont->metrics = metrics;
	bitmapFont->ink_metrics = ink_metrics;
	bitmapFont->bitmaps = bitmaps;
	bitmapFont->encoding = encoding;
	bitmapFont->pDefault = (CharInfoPtr) 0;
	if (pFont->info.defaultCh != (unsigned short) NO_SUCH_CHAR) {
		int	r,
			c,
			cols;

		r = pFont->info.defaultCh >> 8;
		c = pFont->info.defaultCh & 0xFF;
		if ((int)pFont->info.firstRow <= r &&
		    r <= (int)pFont->info.lastRow &&
		    (int)pFont->info.firstCol <= c &&
		    c <= (int)pFont->info.lastCol) {
			cols = pFont->info.lastCol - pFont->info.firstCol + 1;
			r = r - pFont->info.firstRow;
			c = c - pFont->info.firstCol;
			bitmapFont->pDefault = encoding[r * cols + c];
		}
	}
	bitmapFont->bitmapExtra = (BitmapExtraPtr) 0;
	pFont->fontPrivate = (unsigned char *) bitmapFont;
	pFont->get_bitmaps = bitmapGetBitmaps;
	pFont->bit = bit;
	pFont->byte = byte;
	pFont->glyph = glyph;
	pFont->scan = scan;
	xfree(tables, ntables * sizeof (PCFTableRec));
	return (Successful);
Bail:
	xfree(offsets, nbitmaps * sizeof (CARD32));
	xfree(ink_metrics, nink_metrics * sizeof (xCharInfo));
	xfree(encoding, nencoding * sizeof (CharInfoPtr));
	xfree(bitmaps, sizebitmaps);
	xfree(metrics, nmetrics * sizeof (CharInfoRec));
	pFont->info.props = 0;
	xfree(bitmapFont, sizeof (*bitmapFont));
	xfree(tables, ntables * sizeof (PCFTableRec));
	return (AllocError);
}

FontPtr
getFontBitmaps()
{
	FontPtr	pFont;
	int	 ret;
	int	 bit,
		byte,
		glyph,
		scan;

	pFont = (FontPtr) xalloc(sizeof (FontRec));
	FontDefaultFormat(&bit, &byte, &glyph, &scan);
	pFont->refcnt = 0;
	pFont->maxPrivate = -1;
	pFont->devPrivates = (unsigned char **) 0;
	ret = pcfReadFont(pFont, bit, byte, glyph, scan);
	if (ret == Successful)
		return (pFont);
	else {
		xfree(pFont, sizeof (FontRec));
		return (0);
	}
}

bitmap_data_t *
returnFontBitmaps(FontPtr pFont)
{
	int	 ret;
	unsigned long num_glyphs;
	int		data_size;
	fsOffset	*offsets;
	unsigned char	*glyph_data;
	int		freedata;
	int		i;
	struct bitmap_data *bmap;
	int		offsets_size;
	int		last;
	unsigned char	*default_glyph;
	fsOffset	*po;

	bmap = (bitmap_data_t *)xalloc(sizeof (struct bitmap_data));
	if (!bmap)
		return (NULL);

	bmap->encoding_size = 256 * sizeof (*bmap->encoding);
	bmap->encoding = xalloc(bmap->encoding_size);
	if (!bmap->encoding) {
		xfree(bmap, sizeof (*bmap));
		return (NULL);
	}

	ret = (pFont->get_bitmaps)
		((unsigned char *) NULL, pFont, pFont->format,
			LoadAll, /* c->flags, */
			0, /* c->nranges, */
			NULL, /* c->range, */
			&data_size, &num_glyphs,
			&offsets, &offsets_size,
			&glyph_data, &freedata);

	if (ret != Successful) {
		xfree(bmap->encoding, bmap->encoding_size);
		xfree(bmap, sizeof (*bmap));
		printf("returnFontBitmaps: ret=%d\n", ret);
		return (0);
	}

	default_glyph = glyph_data +
		offsets[pFont->info.defaultCh - pFont->info.firstCol].position;

	for (i = 0; i < 256; i++)
		bmap->encoding[i] = default_glyph;

	last = pFont->info.lastCol;
	if (last > 255) last = 255;

	for (po = offsets, i = pFont->info.firstCol; i < last; i++, po++) {
		if (po->length)
			bmap->encoding[i] = glyph_data + po->position;
	}

	xfree(offsets, offsets_size);

	bmap->width = (short)pFont->info.maxbounds.characterWidth;
	bmap->height = (short)(pFont->info.maxbounds.ascent +
				pFont->info.maxbounds.descent);
	bmap->image = glyph_data;
	bmap->image_size = data_size;
	return (bmap);
}


bitmap_data_t *
read_fontfile(char *filename)
{
	FontPtr	pFont;
	label_t jumpbuf;
	bitmap_data_t	*retval = NULL;

	if (on_fault(&jumpbuf)) {
		no_fault();
		return ((bitmap_data_t *)0);
	}

	fontfd = kobj_open_path(filename, 1, 0);

	if (fontfd == (struct _buf *)-1) {
		no_fault();
		return ((bitmap_data_t *)NULL);
	}

	pFont = getFontBitmaps();
	if (pFont) {
		retval =  returnFontBitmaps(pFont);
		xfree(pFont, sizeof (FontRec));
	}

	kobj_close_file(fontfd);

	freeup_mem();

	no_fault();
	return (retval);
}

static void
freeup_mem(void)
{
	xfree(props_ptr, props_size);
	props_ptr = NULL;

	xfree(isStringProp_ptr,  isStringProp_size);
	isStringProp_ptr = NULL;

	xfree(bitmapFont_ptr,  bitmapFont_size);
	bitmapFont_ptr = NULL;

	xfree(metrics_ptr,  metrics_size);
	metrics_ptr = NULL;

	xfree(bitmap_ptr,  bitmap_size);
	bitmap_ptr = NULL;

	xfree(ink_metrics_ptr,  ink_metrics_size);
	ink_metrics_ptr = NULL;

	xfree(encoding_ptr,  encoding_size);
	encoding_ptr = NULL;
}


/* from fonts/lib/font/util/format.c */

int
CheckFSFormat(format, fmask, bit, byte, scan, glyph, image)
	fsBitmapFormat format;
	fsBitmapFormatMask fmask;
	int		*bit,
			*byte,
			*scan,
			*glyph,
			*image;
{
	/* convert format to what the low levels want */

	if (fmask & BitmapFormatMaskBit) {
		*bit = format & BitmapFormatBitOrderMask;
		*bit = (*bit == BitmapFormatBitOrderMSB) ? MSBFirst : LSBFirst;

	}
	if (fmask & BitmapFormatMaskByte) {
		*byte = format & BitmapFormatByteOrderMask;
		*byte = (*byte == BitmapFormatByteOrderMSB) ?
			MSBFirst : LSBFirst;

	}
	if (fmask & BitmapFormatMaskScanLineUnit) {
		*scan = format & BitmapFormatScanlineUnitMask;
		/* convert byte paddings into byte counts */
		switch (*scan) {
		case BitmapFormatScanlineUnit8:
			*scan = 1;
			break;
		case BitmapFormatScanlineUnit16:
			*scan = 2;
			break;
		case BitmapFormatScanlineUnit32:
			*scan = 4;
			break;
		default:
			return (BadFontFormat);
		}

	}
	if (fmask & BitmapFormatMaskScanLinePad) {
		*glyph = format & BitmapFormatScanlinePadMask;
		/* convert byte paddings into byte counts */
		switch (*glyph) {
		case BitmapFormatScanlinePad8:
			*glyph = 1;
			break;
		case BitmapFormatScanlinePad16:
			*glyph = 2;
			break;
		case BitmapFormatScanlinePad32:
			*glyph = 4;
			break;
		default:
			return (BadFontFormat);
		}

	}
	if (fmask & BitmapFormatMaskImageRectangle) {
		*image = format & BitmapFormatImageRectMask;

		if (*image != BitmapFormatImageRectMin &&
		    *image != BitmapFormatImageRectMaxWidth &&
		    *image != BitmapFormatImageRectMax) {
			return (BadFontFormat);
		}
	}

	return (Successful);
}

/* from fonts/lib/font/bitmap/fsfuncs.c */

static
numCharsInRange(startRow, endRow, startCol, endCol, firstCol, lastCol)
{
	int	nchars;

	if (startRow != endRow) {
		/* first page */
		nchars = lastCol - startCol + 1;
		/* middle pages */
		nchars += (endRow - startRow - 1) * (lastCol - firstCol + 1);
		/* last page */
		nchars += endCol - firstCol + 1;
	} else {
		nchars = endCol - startCol + 1;
	}
	return (nchars);
}

/* ARGSUSED */
static	int
font_encoding(flags, pfont)
	Mask	flags;
	FontPtr	pfont;
{
	return ((pfont->info.lastRow == 0) ? Linear16Bit : TwoD16Bit);
}

static void
CopyCharInfo(ci, dst)
	CharInfoPtr ci;
	fsCharInfo *dst;
{
	xCharInfo	*src = &ci->metrics;

	dst->ascent = src->ascent;
	dst->descent = src->descent;
	dst->left = src->leftSideBearing;
	dst->right = src->rightSideBearing;
	dst->width = src->characterWidth;
	dst->attributes = 0;
}

/*
 * packs up the glyphs as requested by the format
 */

int
bitmap_pack_glyphs(pfont, format, flags, num_ranges, range, tsize, num_glyphs,
			offsets, offsets_size, data, freeData)
	FontPtr	pfont;
	fsBitmapFormat	format;
	Mask	flags;
	unsigned long num_ranges;
	fsRange	*range;
	int	*tsize;
	unsigned long	*num_glyphs;
	fsOffset	**offsets;
	int	*offsets_size;
	unsigned char	**data;
	int		*freeData;
{
	int		i;
	fsOffset	*lengths, *l;
	unsigned long	lsize = 0;
	unsigned long	size = 0;
	unsigned char	*gdata, *gd;
	long	ch;
	int	bitorder,
		byteorder,
		scanlinepad,
		scanlineunit,
		mappad;
	int	height, bpr,
		charsize;
	Bool	contiguous, reformat;
	fsRange	*rp = range;
	FontInfoPtr pinfo = &pfont->info;
	BitmapFontPtr bitmapfont = (BitmapFontPtr) pfont->fontPrivate;
	int	row, col;
	int	nchars;
	int	startCol, endCol;
	int	startRow, endRow;
	int	firstCol = pfont->info.firstCol;
	int	firstRow = pfont->info.firstRow;
	int	lastRow = pfont->info.lastRow;
	int	lastCol = pfont->info.lastCol;
	int	src_glyph_pad = pfont->glyph;
	int	src_bit_order = pfont->bit;
	int	src_byte_order = pfont->byte;
	int	ret;
	int	max_ascent, max_descent;
	int	min_left, max_right;
	fsRange	allRange;

	ret = CheckFSFormat(format, (fsBitmapFormatMask) ~ 0,
		&bitorder, &byteorder, &scanlineunit, &scanlinepad, &mappad);

	if (ret != Successful)
		return (ret);

	/* special case for all glyphs first */
	if (flags & LoadAll) {
		if (firstRow == lastRow) {
			allRange.min_char.low = (u_char)(firstCol & 0xff);
			allRange.min_char.high = (u_char)(firstCol >> 8);
			allRange.max_char.low = (u_char)(lastCol & 0xff);
			allRange.max_char.high = (u_char)(lastCol >> 8);
		} else {
			allRange.min_char.low = (u_char)firstCol;
			allRange.min_char.high = (u_char)firstRow;
			allRange.max_char.low = (u_char)lastCol;
			allRange.max_char.high = (u_char)lastRow;
		}
		range = &allRange;
		num_ranges = 1;
	}
	nchars = 0;
	for (i = 0, rp = range; i < num_ranges; i++, rp++) {
	    if (firstRow == lastRow) {
		endRow = startRow = firstRow;
		startCol = (rp->min_char.high << 8) | rp->min_char.low;
		endCol = (rp->max_char.high << 8) | rp->max_char.low;
	    } else {
		startRow = rp->min_char.high;
		startCol = rp->min_char.low;
		endRow = rp->max_char.high;
		endCol = rp->max_char.low;
	    }
	    /* range check */
	    if (startRow > endRow || startCol > endCol || startRow < firstRow ||
		startCol < firstCol || lastRow < endRow || lastCol < endCol) {
		    return (BadCharRange);
	    }
	    nchars += numCharsInRange(startRow, endRow, startCol, endCol,
				firstCol, lastCol);
	}

	/* get space for glyph offsets */
	lsize = sizeof (fsOffset) * nchars;
	*offsets_size = lsize;
	lengths = (fsOffset *) xalloc(lsize);
	if (!lengths)
		return (AllocError);

	bitorder = (bitorder == BitmapFormatBitOrderLSB) ?
	LSBFirst : MSBFirst;
	byteorder = (byteorder == BitmapFormatByteOrderLSB) ?
	LSBFirst : MSBFirst;

	/* compute bpr for padded out fonts */
	reformat = bitorder != src_bit_order || byteorder != src_byte_order;
	switch (mappad) {
	case BitmapFormatImageRectMax:
		max_ascent = FONT_MAX_ASCENT(pinfo);
		max_descent = FONT_MAX_DESCENT(pinfo);
		height = max_ascent + max_descent;
		if (height != pinfo->minbounds.ascent +
		    pinfo->minbounds.descent)
			reformat = TRUE;
		/* FALLTHROUGH */
	case BitmapFormatImageRectMaxWidth:
		min_left = FONT_MIN_LEFT(pinfo);
		max_right = FONT_MAX_RIGHT(pinfo);
		if (min_left != pinfo->maxbounds.leftSideBearing)
			reformat = TRUE;
		bpr = GLWIDTHBYTESPADDED(max_right - min_left, scanlinepad);
		break;
	case BitmapFormatImageRectMin:
		break;
	}
	charsize = bpr * height;
	size = 0;
	gdata = 0;
	contiguous = TRUE;
	l = lengths;
	for (i = 0, rp = range; i < num_ranges; i++, rp++) {
		CharInfoPtr ci;

		if (firstRow == lastRow) {
			endRow = startRow = firstRow;
			startCol = (rp->min_char.high << 8) | rp->min_char.low;
			endCol = (rp->max_char.high << 8) | rp->max_char.low;
		} else {
			startRow = rp->min_char.high;
			startCol = rp->min_char.low;
			endRow = rp->max_char.high;
			endCol = rp->max_char.low;
		}
		ch = (startRow - firstRow) * (lastCol - firstCol + 1) +
			(startCol - firstCol);
		for (row = startRow; row <= endRow; row++) {
			for (col = (row == startRow ? startCol : firstCol);
			    col <= (row == endRow ? endCol : lastCol);
			    col++) {
			l->position = size;
			/* LINTED [assignment operator "=" found where "==" */
			if ((ci = bitmapfont->encoding[ch]) && ci->bits) {
				if (!gdata)
				gdata = (unsigned char *) ci->bits;
				if ((char *)gdata + size != ci->bits)
				contiguous = FALSE;
				if (mappad == BitmapFormatImageRectMin)
				bpr = GLYPH_SIZE(ci, scanlinepad);
				if (mappad != BitmapFormatImageRectMax)
				{
				height = ci->metrics.ascent +
					    ci->metrics.descent;
				charsize = height * bpr;
				}
				l->length = charsize;
				size += charsize;
			} else {
				l->length = 0;
			}
			l++;
			ch++;
			}
		}
	}
	if (contiguous && !reformat) {
		*num_glyphs = nchars;
		*freeData = FALSE;
		*data = gdata;
		*tsize = size;
		*offsets = lengths;
		return (Successful);
	}
	if (size) {
		gdata = (unsigned char *) xalloc(size);
		if (!gdata) {
			xfree(lengths, lsize);
			return (AllocError);
		}
	}

	if (mappad == BitmapFormatImageRectMax)
		bzero((char *)gdata, size);

	*freeData = TRUE;
	l = lengths;
	gd = (unsigned char *)gdata;

	/* finally do the work */
	for (i = 0, rp = range; i < num_ranges; i++, rp++) {
	if (firstRow == lastRow) {
		endRow = startRow = firstRow;
		startCol = (rp->min_char.high << 8) | rp->min_char.low;
		endCol = (rp->max_char.high << 8) | rp->max_char.low;
	} else {
		startRow = rp->min_char.high;
		startCol = rp->min_char.low;
		endRow = rp->max_char.high;
		endCol = rp->max_char.low;
	}
	ch = (startRow - firstRow) * (lastCol - firstCol + 1) +
		(startCol - firstCol);
	for (row = startRow; row <= endRow; row++)
	{
		for (col = (row == startRow ? startCol : firstCol);
		    col <= (row == endRow ? endCol : lastCol);
		    col++) {
			CharInfoPtr ci;
			xCharInfo	*cim;
			int	 srcbpr;
			unsigned char	*src, *dst;
			unsigned int	bits1, bits2;
			int	 r,
				lshift = 0,
				rshift = 0,
				width,
				w,
				src_extra,
				dst_extra;

			ci = bitmapfont->encoding[ch];
			ch++;

			/* ignore missing chars */
			if (!ci) {
				l++;
				continue;
			}
			cim = &ci->metrics;

			srcbpr = GLWIDTHBYTESPADDED(cim->rightSideBearing -
						cim->leftSideBearing,
						src_glyph_pad);
			/*
			 * caculate bytes-per-row for PadNone (others done in
			 * allocation phase), what (if anything) to ignore or
			 * add as padding
			 */
			switch (mappad) {
			case BitmapFormatImageRectMin:
				bpr = GLYPH_SIZE(ci, scanlinepad);
				break;
			case BitmapFormatImageRectMax:
				/* leave the first padded rows blank */
				gd += bpr * (max_ascent - cim->ascent);
			/* FALLTHROUGH */
			case BitmapFormatImageRectMaxWidth:
				rshift = cim->leftSideBearing - min_left;
				lshift = 8 - lshift;
				break;
			}
		src = (unsigned char *) ci->bits;
		dst = (unsigned char *)gd;
		width = srcbpr;
		if (srcbpr > bpr)
			width = bpr;
		src_extra = srcbpr - width;
		dst_extra = bpr - width;

#if (DEFAULTBITORDER == MSBFirst)
#define	BitLeft(b, c)	((b) << (c))
#define	BitRight(b, c)	((b) >> (c))
#else
#define	BitLeft(b, c)	((b) >> (c))
#define	BitRight(b, c)	((b) << (c))
#endif
		if (!rshift) {
			if (srcbpr == bpr) {
				r = (cim->ascent + cim->descent) * width;
				bcopy((caddr_t)src, (caddr_t)dst, r);
				dst += r;
			} else {
				for (r = cim->ascent + cim->descent; r; r--) {
					for (w = width; w; w--)
						*dst++ = *src++;
					dst += dst_extra;
					src += src_extra;
				}
			}
		} else {
			for (r = cim->ascent + cim->descent; r; r--) {
				bits2 = 0;
				for (w = width; w; w--) {
					bits1 = *src++;
					*dst++ = BitRight(bits1, rshift) |
						BitLeft(bits2, lshift);
					bits2 = bits1;
				}
				dst += dst_extra;
				src += src_extra;
			}
		}
		/* skip the amount we just filled in */
		gd += l->length;
		l++;
		}
	}
	}

	/* now do the bit, byte, word swapping */
	if (bitorder != src_bit_order)
		BitOrderInvert(gdata, size);
	if (byteorder != src_byte_order) {
		if (scanlineunit == 2)
			TwoByteSwap(gdata, size);
		else if (scanlineunit == 4)
			FourByteSwap(gdata, size);
	}
	*num_glyphs = nchars;
	*data = gdata;
	*tsize = size;
	*offsets = lengths;

	return (Successful);
}


/* ARGSUSED */
int
bitmapGetBitmaps(client, pfont, format, flags, num_ranges, range,
		    size, num_glyphs, offsets, offsets_size, data, freeData)
	unsigned char *client;
	FontPtr		pfont;
	fsBitmapFormat	format;
	Mask		flags;
	unsigned long	num_ranges;
	fsRange		*range;
	int		*size;
	unsigned long	*num_glyphs;
	fsOffset	**offsets;
	int		*offsets_size;
	unsigned char	**data;
	int		*freeData;
{
	assert(pfont);

	*size = 0;
	*data = (unsigned char *) 0;
	return (bitmap_pack_glyphs(pfont, format, flags,
				num_ranges, range, size, num_glyphs,
				offsets, offsets_size, data, freeData));
}


static fsCharInfo *
do_extent_copy(pfont, flags, start, end, dst)
	FontPtr	pfont;
	Mask	flags;
	int	 start,
		end;
	fsCharInfo *dst;
{
	fsCharInfo *pci;
	int	 i,
		r,
		c;
	int	 encoding;
	BitmapFontPtr bitmapfont;
	CharInfoPtr src;
	int	 firstCol = pfont->info.firstCol;
	int	 firstRow = pfont->info.firstRow;
	int	 lastRow = pfont->info.lastRow;
	int	 lastCol = pfont->info.lastCol;
	int	 num_cols = lastCol - firstCol + 1;
	int	 num_rows = lastRow - firstRow + 1;

	encoding = font_encoding(flags, pfont);
	bitmapfont = (BitmapFontPtr) pfont->fontPrivate;
	pci = dst;

	if (flags & LoadAll) {
		start = (firstRow << 8) + firstCol;
		end = (lastRow << 8) + lastCol;
	}
	switch (encoding) {
	case Linear8Bit:
	case TwoD8Bit:
	case Linear16Bit:
		start -= firstCol;
		end -= firstCol;
		for (i = start; i <= end; i++) {
			src = bitmapfont->encoding[i];
			if (!src)
				src = bitmapfont->pDefault;
			CopyCharInfo(src, pci);
			pci++;
		}
		break;
	case TwoD16Bit:
		for (i = start; i <= end; i++) {
			r = (i >> 8) - firstRow;
			c = (i & 0xff) - firstCol;
			if (r < num_rows && c >= 0 && c < num_cols) {
			src = bitmapfont->encoding[r * num_cols + c];
			if (!src)
				src = bitmapfont->pDefault;
			CopyCharInfo(src, pci);
			pci++;
			}
		}
		break;
	}
	return (pci);
}


/* ARGSUSED */
int
bitmapGetExtents(client, pfont, flags, num_ranges, range, num_extents, data)
	unsigned char *client;
	FontPtr	pfont;
	Mask	flags;
	unsigned long num_ranges;
	fsRange	*range;
	unsigned long *num_extents;
	fsCharInfo **data;

{
	int		start,
			end,
			i;
	unsigned	long size;
	fsCharInfo	*ci,
			*pci;
	fsRange		*rp;
	FontInfoPtr	pinfo;
	BitmapFontPtr	bitmapfont;
	int		firstCol = pfont->info.firstCol;
	int		firstRow = pfont->info.firstRow;
	int		lastRow = pfont->info.lastRow;
	int		lastCol = pfont->info.lastCol;
	int		num_cols = lastCol - firstCol + 1;

	assert(pfont);
	pinfo = &pfont->info;
	bitmapfont = (BitmapFontPtr) pfont->fontPrivate;

	if (!bitmapfont->pDefault)
		bitmapfont->pDefault = &junkDefault;
	if (flags & LoadAll) {
		start = (firstRow << 8) + firstCol;
		end = (lastRow << 8) + lastCol;

		if (lastRow) {
			*num_extents = n2dChars(pinfo);
		} else {
			*num_extents = end - start + 1;
		}
		size = sizeof (fsCharInfo) * (*num_extents);
		pci = ci = (fsCharInfo *) xalloc(size);
		if (!ci)
			return (AllocError);

		pci = do_extent_copy(pfont, flags, 0, 0, ci);

		/* make sure it didn't go off the end */
		assert(pci == (fsCharInfo *)((char *)ci + size));
		assert(pci == (ci + (*num_extents)));

		if (bitmapfont->pDefault == &junkDefault)
			bitmapfont->pDefault = 0;

		*data = ci;
		return (Successful);
	}
	/* normal case */
	/* figure out how big everything has to be */
	*num_extents = 0;
	for (i = 0, rp = range; i < num_ranges; i++, rp++) {
		start = (rp->min_char.high << 8) + rp->min_char.low;
		end = (rp->max_char.high << 8) + rp->max_char.low;

		/* range check */
		if (end < start ||
			(end > (int)((pinfo->lastRow << 8) + pinfo->lastCol)) ||
			(end < (int)((pinfo->firstRow << 8) +
			pinfo->firstCol)) ||
			(start > (int)((pinfo->lastRow << 8) +
				pinfo->lastCol)) ||
			(start < (int)((pinfo->firstRow << 8) +
				pinfo->firstCol)))
			return (BadCharRange);

		/* adjust for SNF layout */
		start -= (firstRow << 8) + firstCol;
		end -= (firstRow << 8) + firstCol;
		if (lastRow) {
			*num_extents = ((end >> 8) - (start >> 8) + 1) *
					    num_cols;
		} else {
			*num_extents += end - start + 1;
		}
	}

	size = sizeof (fsCharInfo) * (*num_extents);
	pci = ci = (fsCharInfo *) xalloc(size);
	if (!ci)
		return (AllocError);

	/* copy all the extents */
	for (i = 0, rp = range; i < num_ranges; i++, rp++) {
		start = (rp->min_char.high << 8) + rp->min_char.low;
		end = (rp->max_char.high << 8) + rp->max_char.low;

		pci = do_extent_copy(pfont, flags, start, end, pci);

		/* make sure it didn't go off the end */
		assert(pci == (fsCharInfo *)((char *)ci + size));
	}

	*data = ci;
	if (bitmapfont->pDefault == &junkDefault)
		bitmapfont->pDefault = 0;

	return (Successful);
}

/* from fonts/lib/font/util/bitmaputil.c */

/*
 *	Invert bit order within each BYTE of an array.
 */
void
BitOrderInvert(buf, nbytes)
	register unsigned char *buf;
	register int nbytes;
{
	register unsigned char *rev = _reverse_byte;

	for (; --nbytes >= 0; buf++)
		*buf = rev[*buf];
}

/*
 *	Invert byte order within each 16-bits of an array.
 */
void
TwoByteSwap(buf, nbytes)
	register unsigned char *buf;
	register int nbytes;
{
	register unsigned char c;

	for (; nbytes > 0; nbytes -= 2, buf += 2) {
		c = buf[0];
		buf[0] = buf[1];
		buf[1] = c;
	}
}


/*
 *	Invert byte order within each 32-bits of an array.
 */
void
FourByteSwap(buf, nbytes)
	register unsigned char *buf;
	register int nbytes;
{
	register unsigned char c;

	for (; nbytes > 0; nbytes -= 4, buf += 4) {
		c = buf[0];
		buf[0] = buf[3];
		buf[3] = c;
		c = buf[1];
		buf[1] = buf[2];
		buf[2] = c;
	}
}


/*
 *	Repad a bitmap
 */

int
RepadBitmap(pSrc, pDst, srcPad, dstPad, width, height)
	char		*pSrc, *pDst;
	unsigned	srcPad, dstPad;
	int		width, height;
{
	int		srcWidthBytes, dstWidthBytes;
	int		row, col;
	char		*pTmpSrc, *pTmpDst;

	switch (srcPad) {
		case 1:
			srcWidthBytes = (width+7)>>3;
			break;
		case 2:
			srcWidthBytes = ((width+15)>>4)<<1;
			break;
		case 4:
			srcWidthBytes = ((width+31)>>5)<<2;
			break;
		case 8:
			srcWidthBytes = ((width+63)>>6)<<3;
			break;
		default:
			return (0);
	}
	switch (dstPad) {
		case 1:
			dstWidthBytes = (width+7)>>3;
			break;
		case 2:
			dstWidthBytes = ((width+15)>>4)<<1;
			break;
		case 4:
			dstWidthBytes = ((width+31)>>5)<<2;
			break;
		case 8:
			dstWidthBytes = ((width+63)>>6)<<3;
			break;
		default:
			return (0);
	}

	width = srcWidthBytes;
	if (width > dstWidthBytes)
	width = dstWidthBytes;
	pTmpSrc = pSrc;
	pTmpDst = pDst;
	for (row = 0; row < height; row++) {
		for (col = 0; col < width; col++)
			*pTmpDst++ = *pTmpSrc++;
		while (col < dstWidthBytes) {
			*pTmpDst++ = '\0';
			col++;
		}
		pTmpSrc += srcWidthBytes - width;
	}
	return (dstWidthBytes * height);
}

/* from fonts/lib/font/fontfile/defaults.c */

void
FontDefaultFormat(bit, byte, glyph, scan)
	int	*bit, *byte, *glyph, *scan;
{
	*bit = DEFAULT_BIT_ORDER;
	*byte = DEFAULT_BYTE_ORDER;
	*glyph = DEFAULT_GLYPH_PAD;
	*scan = DEFAULT_SCAN_UNIT;
}
