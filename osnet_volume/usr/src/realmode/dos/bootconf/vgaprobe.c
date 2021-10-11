/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 *
 *  vga_probe.c - vga device/resource probing
 *
 */

#ident	"@(#)vgaprobe.c	1.31	99/01/28 SMI"

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <names.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include "types.h"

#include "adv.h"
#include "bios.h"
#include "menu.h"
#include "boot.h"
#include "boards.h"
#include "debug.h"
#include "devdb.h"
#include "dir.h"
#include "enum.h"
#include "err.h"
#include "escd.h"
#include "gettext.h"
#include "menu.h"
#include "probe.h"
#include "prop.h"
#include "resmgmt.h"
#include "setjmp.h"
#include "tree.h"
#include "tty.h"
#include "vgaprobe.h"
#include "pci.h"


#define	OPT_SIG		0xaa55		/* Identifies an option BIOS */
#define	VIDEO_BUF	0xa0000		/* Address of video frame buffer */
#define	VIDBUFLEN	0x20000		/* Length of video frame buffer	*/
#define	VIDEO_BIOS	0xc0000L	/* Address of video bios (normally) */
#define	ATI_VENDOR_ID	0x1002		/* ATI's Vendor Id */

#define	VGABIOS_ALT_FUNC_SELECT		0x12
#define	VGABIOS_RW_DISPLAY_CODE		0x1a
#define	VGABIOS_GET_VGA_STATE		0x1b
#define	XGABIOS_DISPLAY_MODE_INFO	0x1f
#define	VESABIOS_SUPP_FUNC		0x4f
#define	VESABIOS_DISPLAY_ID_EXT		0x15

static char SubVgaName[] = "SUN0005";
static char XgaName[] = "SUN0006";
static char VgaName[] = "ISY0010";
static char P9000Name[] = "SUN0010";
static char P9100Name[] = "SUN0020";
static char VGA8514Name[] = "PNP0902";

#pragma pack(1)
/*
 * Structure returned by VESA BIOS SVGA get SVGA info call
 */
struct VESAtbl {
	char	sig[4];
	short	version;
	char far *OEMptr;
	unsigned long capabilities;
	unsigned short far *vidmodelist;
	short memblks;
/* VBE 2.0 area */
	short softvers;
	char far *vendor;
	char far *product;
	char far *rev;
	char reserved[222];
	char OEMdata[256];
} VESAtbl;

/*
 * structure returned by VGA BIOS get video system state call
 */
struct VGAinfo {
	char far *funcstatetbl;
	unsigned char vidmode;
	short ncol;
	short bytesperpg;
	short pageoff;
	short cursorpos[8];
	short cursorshape;
	unsigned char activepg;
	short modereg;
	short colorpalettereg;
	unsigned char nrows;
	short scanlinesperchar;
	unsigned char displaycode;
	unsigned char altdisplaycode;
	unsigned short ncolors;
	unsigned char npages;
	unsigned char nscanlines;
	unsigned char fontpage0;
	unsigned char fontpage1;
	unsigned char statebits;
	char unused[3];
	unsigned char vidmemsize;
	unsigned char savestate;
	char unused2[13];
	unsigned char fill[512];
} VGAinfo;

/*
 * Structure returned by monitors that support VESA DDC
 */
struct EDIFinfo {
	unsigned char hdr[8];
	unsigned short mfname; /* EISA style compressed id */
	unsigned short pid;
	unsigned long serialno;
	char mfweek;
	unsigned char mfyear;
	unsigned char edidver;
	unsigned char edidrev;
	unsigned char vidindef;
	unsigned char maxHimagesz; /* cm */
	unsigned char maxVimagesz; /* cm */
	unsigned char displayxferchar;
	unsigned char DPMSfeat;
	unsigned char RGlowbits;
	unsigned char BWlowbits;
	unsigned char redx;
	unsigned char redy;
	unsigned char greenx;
	unsigned char greeny;
	unsigned char bluex;
	unsigned char bluey;
	unsigned char whitex;
	unsigned char whitey;
	unsigned char esttimings1;
	unsigned char esttimings2;
	unsigned char rsvdtimings;
	short stdtimingid1;
	short stdtimingid2;
	short stdtimingid3;
	short stdtimingid4;
	short stdtimingid5;
	short stdtimingid6;
	short stdtimingid7;
	short stdtimingid8;
	unsigned char dettimingdesc1[18];
	unsigned char dettimingdesc2[18];
	unsigned char dettimingdesc3[18];
	unsigned char dettimingdesc4[18];
	unsigned char extflg;
	unsigned char chksum;
} EDIFinfo;

/*
 * Structure returned by VESA getmodeinfo call
 */
struct ModeInfo {
	unsigned short modeattrs;
	unsigned char winaattrs;
	unsigned char winbattrs;
	unsigned short wingranularity;
	unsigned short winsize;
	unsigned short winaseg;
	unsigned short winbseg;
	unsigned char far *winfuncptr;
	unsigned short bytesperscanline;
	unsigned short xresolution;
	unsigned short yresolution;
	unsigned char xcharsize;
	unsigned char ycharsize;
	unsigned char numofplanes;
	unsigned char bitsperpixel;
	unsigned char numofbanks;
	unsigned char memorymodel;
	unsigned char banksize;
	unsigned char numimagepgs;
	unsigned char reserved;
	unsigned char redmasksize;
	unsigned char redfieldpos;
	unsigned char greenmasksize;
	unsigned char greenfieldpos;
	unsigned char bluemasksize;
	unsigned char bluefieldpos;
	unsigned char rsvdmasksize;
	unsigned char rsvdfieldpos;
	unsigned char directcolormodeinfo;
	unsigned char far *physbaseptr;
	unsigned long offscreenmemoffset;
	unsigned short offscreenmemsize;
	unsigned char fill[206];
} ModeInfo;
#pragma pack()

struct Mode {
	unsigned long modenum;	/* Mode number */
	unsigned long x_res;	/* X resolution */
	unsigned long y_res; 	/* Y resolution */
	unsigned long ncolors;	/* number of colors */
} modes[32];

Board *find_video_board();
Board *create_vga_node(unsigned char far *biosp);
int do_ati_probe(Board *bp, unsigned char far *bios_adr, char *buf);
int do_genoa_probe(Board *bp, unsigned char far *bios_adr, char *buf);
int get_mode_info(unsigned short mode, struct Mode *mp);
void get_vga_properties(Board *bp, unsigned char far *biosp);

/*
 * This function searches the boards list for a device having the
 * video buffer resource.
 */
Board *
find_video_board()
{
	/*
	 * A video board should have at least 64k of memory at 0xb0000
	 */

	return (Query_Mem(0xb0000, 0x10000));
}


unsigned char far *
find_vga_bios()
{
	unsigned char far *cfp;
	unsigned long bios_adr;
	long bios_len;
	u_long incr;

	/*
	 * We search for the Video BIOS starting at 0xc0000, and assume
	 * it is the first bios found that contains the string "vga".
	 * XXX - Note: this is possibly wrong, but There is no sure
	 * way to determine if a bios is a VGA video bios or some other
	 * BIOS.  If we can't find the vga string then we just assume
	 * any bios at 0xc0000 is the vga bios.
	 */
	for (bios_adr = BIOS_MIN; bios_adr < 0xff800000; bios_adr += incr) {
		cfp = (unsigned char far *)bios_adr;
		if (*(short far *)cfp == OPT_SIG) {
			register u_char far *vp, *ep;

			bios_len = (unsigned long)cfp[2] * 512L;
			ep = cfp + bios_len - 3;
			for (vp = cfp; vp < ep; vp++)
				if (*vp == 'v' || *vp == 'V')
					if (_strnicmp((char *)vp,
					    "vga", 3) == 0)
						goto found;
			incr = bios_len << 12; /* shift to far pointer format */
		} else {
			incr = BIOS_ALIGN;
		}
	}
	/*
	 * If we couldn't find a vga signature, fall back and assume bios is
	 * at 0xc0000
	 */
	if (bios_adr >= 0xff800000)
		cfp = (unsigned char far *)0xc0000000;
found:
	return (cfp);
}

/*
 * Search for the given string in the given block of memory.
 * Return true if found, else false.
 */
int
findstr(char far *cp, char far *sp, int len)
{
	char far *vp;
	int slen;

	slen = strlen(sp);
	for (vp = cp; vp < cp + len; vp++)
		if (*vp == *sp)
			if (strncmp(vp, sp, slen) == 0)
				return (1);
	return (0);
}

/*
 * Special routine to recognize vlb p9000/p9100 boards and
 * change the name of the generic VGA board to a p9000/p9100
 * Also will identify and assign a free memory range to the card.
 */
Board *
p9x00_check(Board *bp, unsigned char far *bios_adr)
{
	Board *nbp;
	unsigned long memaddr;

	if (Pci) /* no vlb/PCI mixed systems supported */
		return (bp);
	/*
	 * check for p9000/p9100
	 */
	if (!findstr((char far *)bios_adr, "VIPER", 512))
		return (bp); /* not a p9000/p9100 */
	if (findstr((char far *)bios_adr, "P9100", 512))
		bp->devid = CompressName(P9100Name);
	else
		bp->devid = CompressName(P9000Name);
	/*
	 * The card must be at one of 0x20000000, 0x80000000, or
	 * 0xA0000000, with a length of 0x400000.  See if one of
	 * these ranges is free starting at the highest addr.
	 */
	memaddr = 0;
	if (!Query_Mem(0xa0000000, 0x400000))
		memaddr = 0xa0000000;
	else if (!Query_Mem(0x80000000, 0x400000))
		memaddr = 0x80000000;
	else if (!Query_Mem(0x20000000, 0x400000))
		memaddr = 0x20000000;
	if (memaddr == 0)
		return (bp); /* can't get required memory range */
	nbp = AddResource_devdb(bp, RESF_Mem, memaddr, 0x400000);
	return (nbp);
}

/*
 * See if we have a 8514/A in the system by probing the ERR_TERM register.
 * This should be fairly safe since we are running last and will only probe
 * if no other board has claimed the resource.
 */
int
probe8514(Board *bp)
{
	unsigned short oldreg;

	/*
	 * See if both the probe address and the standard (unaliased)
	 * port addresses are available.
	 */
	if (Query_Port(0x92e8, 2))
		return (0);
	if (Query_Port(0x2e8, 8))
		return (0);
	/*
	 * Write a pattern to the ERR_TERM register on the 8514
	 * and see if it reads back as written.
	 */
	oldreg = _inpw(0x92e8);
	_outpw(0x92e8, 0xa5a5);
	if (_inpw(0x92e8) != 0xa5a5) {
		_outpw(0x92e8, oldreg);
		return (0);
	}
	/*
	 * Now try the inverse to be sure
	 */
	_outpw(0x92e8, 0x5a5a);
	if (_inpw(0x92e8) != 0x5a5a) {
		_outpw(0x92e8, oldreg);
		return (0);
	}
	/*
	 * We seem to have an 8514/A in the system, change the name to
	 * reflect this
	 */
	bp->devid = CompressName(VGA8514Name);
	return (1);
}


/*
 * This function creates a sub-vga video node, a pointer to the created node
 * is returned.
 */
void
create_subvga_node(unsigned char far *biosp)
{
	Board *bp;
	unsigned long bios_adr;
	long bios_len = 0;

	bp = new_board();
	/*
	 * Mark this as a scanned (probed) for node.
	 */
	bp->slotflags |= RES_SLOT_PROBE;
	bp->bustype = RES_BUS_ISA;
	bp->devid = CompressName(SubVgaName);

	bp = AddResource_devdb(bp, RESF_Port, 0x3b0, 12);
	bp = AddResource_devdb(bp, RESF_Port, 0x3c0, 32);
	/*
	 * Reserve memory space for the video buffer and BIOS
	 */
	if (*(short far *)biosp == OPT_SIG && (unsigned long)biosp < BIOS_MAX)
		bios_len = (unsigned long)biosp[2] * 512L;
	bios_adr = MK_PHYS(biosp);
	bp = AddResource_devdb(bp, RESF_Mem, 0xb0000, 0x10000);
	if (bios_len) {
		bp = AddResource_devdb(bp, RESF_Mem, bios_adr, bios_len);
	}
	add_board(bp); /* Tell rest of system about device */
	(void) find_video_board();
}

/*
 * check if IIT XGA resides at given register base
 */
int
is_xga_base_valid(int index)
{
	unsigned short port = 0x2100 + (index << 4);
	unsigned char old, new;
	int ireg, dreg;

	if (Query_Port(port, 16))
		return (0); /* i/o ports are not available */
	ireg = port + 0x0a;
	dreg = port + 0x0b;
	_outp(ireg, 0x6e); /* point at AGX mode register 4 */
	old = _inp(dreg);
	_outp(dreg, 0xaa);
	new = _inp(dreg);
	_outp(dreg, old);
	return (new == 0xaa);
}

/*
 * This function creates a isa XGA node, a pointer to the created node
 * is returned.
 */
Board *
create_xga_node(unsigned char far *biosp)
{
	Board *bp;
	unsigned long bios_adr;
	long bios_len = 0;
	int i;
	unsigned short base;

	bp = new_board();
	/*
	 * Mark this as a scanned (probed) for node.
	 */
	bp->slotflags |= RES_SLOT_PROBE;
	bp->bustype = RES_BUS_ISA;
	bp->devid = CompressName(XgaName);

	/*
	 * Find XGA register base
	 */
	base = 0x2100;
	for (i = 0; i < 8; i++)
		if (is_xga_base_valid(i)) {
			base += i << 4;
			bp = AddResource_devdb(bp, RESF_Port, base, 16);
			break;
		}
	/*
	 * Reserve memory space for the video buffer and BIOS
	 */
	if (*(short far *)biosp == OPT_SIG && (unsigned long)biosp < BIOS_MAX)
		bios_len = (unsigned long)biosp[2] * 512L;
	bios_adr = MK_PHYS(biosp);
	bp = AddResource_devdb(bp, RESF_Mem, VIDEO_BUF, VIDBUFLEN);
	if (bios_len) {
		bp = AddResource_devdb(bp, RESF_Mem, bios_adr, bios_len);
	}
	add_board(bp); /* Tell rest of system about device */
	return (find_video_board());
}

/*
 * This function creates a isa vga node, a pointer to the created node
 * is returned.
 */
Board *
create_vga_node(unsigned char far *biosp)
{
	Board *bp;
	unsigned long bios_adr;
	long bios_len = 0;

	bp = new_board();
	/*
	 * Mark this as a scanned (probed) for node.
	 */
	bp->slotflags |= RES_SLOT_PROBE;
	bp->bustype = RES_BUS_ISA;
	bp->devid = CompressName(VgaName);

	bp = AddResource_devdb(bp, RESF_Port, 0x3b0, 12);
	bp = AddResource_devdb(bp, RESF_Port, 0x3c0, 32);
	/*
	 * Check for 8514/A in the machine.  If found, add the registers
	 * used by the 8514/A (Note that they alias to lots more regs).
	 */
	if (probe8514(bp)) {
		bp = AddResource_devdb(bp, RESF_Port, 0x2e8, 8);
	}
	/*
	 * Reserve memory space for the video buffer and BIOS
	 */
	if (*(short far *)biosp == OPT_SIG && (unsigned long)biosp < BIOS_MAX)
		bios_len = (unsigned long)biosp[2] * 512L;
	bios_adr = MK_PHYS(biosp);
	bp = AddResource_devdb(bp, RESF_Mem, VIDEO_BUF, VIDBUFLEN);
	if (bios_len) {
		bp = AddResource_devdb(bp, RESF_Mem, bios_adr, bios_len);
	}
	/*
	 * Special check for vlb P9000/p9100
	 */
	bp = p9x00_check(bp, biosp);
	add_board(bp); /* Tell rest of system about device */
	return (find_video_board());
}

int
do_ati_probe(Board *bp, unsigned char far *bios_adr, char *buf)
{
	unsigned char far *cp;
	unsigned short extadr;
	unsigned char status;
	unsigned long memsize;

	/*
	 * Look for ATI BIOS signature
	 */
	cp = bios_adr + 0x31; /* ATI SVGA signature loc */
	if (memcmp(cp, "761295520", 9))
		return (0); /* not an ATI */
	cp = bios_adr + 0x40; /* ATI SVGA type loc */
	memsize = 0;
	if (*cp == '3' && *(cp+1) == '1') {
		switch (*(cp+3)) {
		case '1':
		case '2':
		case '3':
		case '4':
		case '5': /* Wonder & Wonder+ */
			extadr = *(unsigned short *)(cp+0x10);
			_outp(extadr, 0xbb);
			status = _inp(extadr+1);
			memsize = (unsigned long)256 * 1024;
			if (status & 0x20)
				memsize = memsize << 1;
			break;
		case 'a': /* Mach-32 */
			break;
		default:
			break;
		};
	}
	(void) sprintf(buf, "\"ATI SVGA %c%c%02x%c\"", *cp, *(cp+1), *(cp+2),
		*(cp+3));
	(void) SetDevProp_devdb(&bp->prop, "vesa-oem-string", buf,
		strlen(buf)+1, 0);
	(void) SetDevProp_devdb(&bp->prop, "video-memory-size", &memsize,
		sizeof (long), PROP_BIN);
	return (1);
}

int
do_genoa_probe(Board *bp, unsigned char far *bios_adr, char *buf)
{
	unsigned char far *cp;
	unsigned char sigaddr;

	/*
	 * Look for Genoa BIOS signature
	 */
	cp = bios_adr + 0x37; /* Genoa signature pointer */
	sigaddr = *cp;
	cp = cp + sigaddr;
	if (*cp != 0x77 || *(cp + 2) != 0x99 || *(cp + 3) != 0x66)
		return (0); /* not a Genoa */
	(void) sprintf(buf, "\"Genoa SVGA %x%02x%x%x\"", *cp, *(cp+1), *(cp+2),
		*(cp+3));
	(void) SetDevProp_devdb(&bp->prop, "vesa-oem-string", buf,
		strlen(buf)+1, 0);
	return (1);
}

int
get_mode_info(unsigned short mode, struct Mode *mp)
{
	union _REGS inregs, outregs;
	struct _SREGS segregs;
	char far *fp;

	_segread(&segregs);
	fp = (char far *)&ModeInfo;
	inregs.h.ah = VESABIOS_SUPP_FUNC;
	inregs.h.al = 0x01; /* Get Mode Info */
	inregs.x.cx = mode;
	inregs.x.di = _FP_OFF(fp);
	segregs.es = _FP_SEG(fp);
	_int86x(0x10, &inregs, &outregs, &segregs);
	if (outregs.x.ax != VESABIOS_SUPP_FUNC)
		return (0); /* get mode call failed */
	if ((ModeInfo.modeattrs & 0x01) == 0)
		return (0); /* mode not supported */
	if ((ModeInfo.modeattrs & 0x10) == 0)
		return (0); /* not a graphics mode */
	mp->modenum = (unsigned long)mode;
	mp->x_res = ModeInfo.xresolution;
	mp->y_res = ModeInfo.yresolution;
	mp->ncolors = 1L << ModeInfo.bitsperpixel;
	return (1);
}

void
get_vga_properties(Board *bp, unsigned char far *biosp)
{
	union _REGS inregs, outregs;
	struct _SREGS segregs;
	char far *fp;
	char *adtype = (char *)0;
	char *dpytype = (char *)0;
	long memsize = 0;
	char buf[512];

	_segread(&segregs);
	adtype = "vga";
	/*
	 * See if we are VESA SVGA
	 */
	inregs.h.ah = VESABIOS_SUPP_FUNC;
	inregs.h.al = 0x00; /* Return Supplemental Specification Info */
	/*
	 * Try for Version 2 info if it is available
	 */
	VESAtbl.sig[0] = 'V';
	VESAtbl.sig[1] = 'B';
	VESAtbl.sig[2] = 'E';
	VESAtbl.sig[3] = '2';
	fp = (char far *)&VESAtbl;
	segregs.es = _FP_SEG(fp);
	inregs.x.di = _FP_OFF(fp);
	_int86x(0x10, &inregs, &outregs, &segregs);
	if (!outregs.x.cflag && outregs.h.al == VESABIOS_SUPP_FUNC) {
		unsigned short *mp;
		struct Mode *modep;

		adtype = "svga";
		/*
		 * We have a SVGA that knows VESA Bios extensions
		 * Make properties of OEM vendor/model/id strings
		 */
		(void) sprintf(buf, "\"%s\"", VESAtbl.OEMptr);
		(void) SetDevProp_devdb(&bp->prop, "vesa-oem-string", buf,
			strlen(buf)+1, 0);
		memsize = (long)VESAtbl.memblks * 0x10000;
		(void) SetDevProp_devdb(&bp->prop, "vesa-capabilities",
			&VESAtbl.capabilities, sizeof (long), PROP_BIN);
		/*
		 * Make additional props if we are VBE 2.0 or higher
		 */
		if ((VESAtbl.version >> 8) >= 0x02) {
			(void) sprintf(buf, "\"%s\"", VESAtbl.vendor);
			(void) SetDevProp_devdb(&bp->prop, "vesa-oem-vendor",
				buf, strlen(buf)+1, 0);
			(void) sprintf(buf, "\"%s\"", VESAtbl.product);
			(void) SetDevProp_devdb(&bp->prop, "vesa-oem-product",
				buf, strlen(buf)+1, 0);
			(void) sprintf(buf, "\"%s\"", VESAtbl.rev);
			(void) SetDevProp_devdb(&bp->prop, "vesa-oem-revision",
				buf, strlen(buf)+1, 0);
		}
		/*
		 * Get supported VESA video modes
		 */
		modep = modes;
		for (mp = VESAtbl.vidmodelist; *mp != 0xffff; mp++)
			if (get_mode_info(*mp, modep))
				modep++;
		if (modep != modes)
			(void) SetDevProp_devdb(&bp->prop,
				"svga-modes-supported",
				modes, (modep - modes) * sizeof (struct Mode),
				PROP_BIN);
	} else { /* check for known older SVGA adapters */
		if (do_ati_probe(bp, biosp, buf) ||
		    do_genoa_probe(bp, biosp, buf))
			adtype = "svga";
	}
	(void) SetDevProp_devdb(&bp->prop, "video-adapter-type", adtype,
		strlen(adtype)+1, 0);
	/*
	 * Now, let's get any monitor information available
	 */
	inregs.h.ah = VGABIOS_GET_VGA_STATE;
	inregs.h.al = 0x00;
	fp = (char far *)&VGAinfo;
	segregs.es = _FP_SEG(fp);
	inregs.x.di = _FP_OFF(fp);
	_int86x(0x10, &inregs, &outregs, &segregs);
	if (!outregs.x.cflag && outregs.h.al == VGABIOS_GET_VGA_STATE) {
		if (VGAinfo.statebits & 0x04) {
			dpytype = "monochrome";
		} else {
			dpytype = "color";
		}
	}
	if (dpytype == (char *)0 || memsize == 0) {
		inregs.h.ah = VGABIOS_ALT_FUNC_SELECT;
		inregs.h.bl = 0x10; /* Get EGA Info */
		_int86x(0x10, &inregs, &outregs, &segregs);
		if (!outregs.x.cflag && dpytype == (char *)0) {
			if (outregs.h.bh == 1) {
				dpytype = "monochrome";
			} else {
				dpytype = "color";
			}
		}
		if (!outregs.x.cflag && memsize == 0) {
			if (outregs.h.bh == 3) {
				memsize = 256 * 1024;
			}
		}
	}
	if (dpytype == (char *)0)
		dpytype = "unknown";
	(void) SetDevProp_devdb(&bp->prop, "video-memory-size", &memsize,
		sizeof (long), PROP_BIN);
	(void) SetDevProp_devdb(&bp->prop, "display-type", dpytype,
		strlen(dpytype)+1, 0);
	/*
	 * See what level of VESA DDC is supported (if any)
	 */
	inregs.h.ah = VESABIOS_SUPP_FUNC;
	inregs.h.al = VESABIOS_DISPLAY_ID_EXT;
	inregs.h.bl = 0x00; /* Report DDC Capcbilities */
	inregs.x.cx = 0;
	inregs.x.dx = 0;
	segregs.es = 0;
	inregs.x.di = 0;
	_int86x(0x10, &inregs, &outregs, &segregs);
	if (!outregs.x.cflag && outregs.h.al == VESABIOS_SUPP_FUNC) {
		/*
		 * BIOS supports VBE/DDC extension
		 */
		if (outregs.h.bl & 0x03) { /* DDC1 or DDC2 supported */
			unsigned long mfn, compid;

			/*
			 * Get VESA DDC EDIF info
			 */
			EDIFinfo.edidver = 0;
			inregs.h.ah = VESABIOS_SUPP_FUNC;
			inregs.h.al = VESABIOS_DISPLAY_ID_EXT;
			inregs.h.bl = 0x01; /* Read EDID */
			inregs.x.cx = 0;
			inregs.x.dx = 0;
			fp = (char far *)&EDIFinfo;
			segregs.es = _FP_SEG(fp);
			inregs.x.di = _FP_OFF(fp);
			_int86x(0x10, &inregs, &outregs, &segregs);
			if (!outregs.x.cflag && EDIFinfo.edidver != 0) {
				(void) SetDevProp_devdb(&bp->prop,
					"display-edif-block",
					&EDIFinfo, sizeof (struct EDIFinfo),
					PROP_BIN);
				/*
				 * Set edif id as a property
				 */
				mfn = (long)EDIFinfo.mfname;
				compid = (long)EDIFinfo.pid << 24 | mfn |
					((long)(EDIFinfo.pid & 0xff00) << 8);
				DecompressName(compid, buf);
				(void) SetDevProp_devdb(&bp->prop,
					"display-edif-id",
					buf, strlen(buf) + 1, PROP_CHAR);
			}
		}
	}
	/*
	 * Set first 512 bytes of video BIOS as a property
	 */
	if (bp->pci_venid == ATI_VENDOR_ID) {
		/*
		 * Some ATI cards return different values for bios bytes 4-7
		 * on each boot, and this causes kdmconfig to think that the
		 * video device has changed when it really hasn't.  To avoid
		 * this problem, we zero these bytes here.  See bug 4184768.
		 */
		memcpy(buf, biosp, 512);
		buf[4] = 0; buf[5] = 0; buf[6] = 0; buf[7] = 0;
		(void) SetDevProp_devdb(&bp->prop, "video-bios-bytes", (void *)buf,
		    512, PROP_BIN);
	} else {
		(void) SetDevProp_devdb(&bp->prop, "video-bios-bytes", (void *)biosp,
		    512, PROP_BIN);
	}
}

/*
 * This function determines if there is a vga compatible adapter in the system.
 * It then searches the device list to see if the enumerators
 * have found any vga boards.  If none are found it creates a vga board
 * It then gathers a bunch of info about the display adapter and possibly
 * the monitor via VESA bios calls and attaches the info as properties to
 * the vga node.
 */
void
enumerator_vgaprobe()
{
	union _REGS inregs, outregs;
	Board *bp;
	unsigned char far *biosp;

	biosp = find_vga_bios();
	/*
	 * See if we have a  Video adapter of some kind
	 */
	inregs.h.ah = 0xf; /* Read current video state */
	inregs.h.al = 0; /* Read display code */
	(void) _int86(0x10, &inregs, &outregs);
	if (outregs.x.cflag) {
		/*
		 * int 10h func fh not supported, no video present.
		 */
		return;
	}
	/*
	 * see if we have a  VGA
	 */
	inregs.h.ah = VGABIOS_RW_DISPLAY_CODE;
	inregs.h.al = 0; /* Read display code */
	(void) _int86(0x10, &inregs, &outregs);
	if (outregs.x.cflag || outregs.h.al != VGABIOS_RW_DISPLAY_CODE) {
		/*
		 * int 10h func 1ah not supported, not a VGA or XGA.
		 */
		create_subvga_node(biosp);
		return;
	}
	inregs.h.ah = XGABIOS_DISPLAY_MODE_INFO;
	inregs.h.al = 0; /* Ask for required buffer length */
	(void) _int86(0x10, &inregs, &outregs);
	if (outregs.x.cflag) {
		/*
		 * error - int 10h func 1fh failed.
		 */
		create_subvga_node(biosp);
		return;
	}
	if (outregs.h.al == XGABIOS_DISPLAY_MODE_INFO) {
		/*
		 * int 10h func 1fh supported, adapter is XGA
		 */
		if ((bp = find_video_board()) == (Board *)0)
			bp = create_xga_node(biosp);
		return;
	}
	/*
	 * We have a vga in the system -
	 * See if any vga boards are on the device list.
	 * If there is not one, create it.
	 */
	if ((bp = find_video_board()) == (Board *)0)
		bp = create_vga_node(biosp);
	/*
	 * Now, gather the vga properties to pass up to the os
	 */
	if (bp != (Board *)0)
		get_vga_properties(bp, biosp);
}
