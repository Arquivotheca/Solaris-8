/*
 * Copyright (c) 1988,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CG8VAR_H
#define	_SYS_CG8VAR_H

#pragma ident	"@(#)cg8var.h	1.6	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/pixrect.h>	/* Definition for struct rect */
#include <sys/memfb.h>		/* Needed by sbusdev/cg8reg.h. */
#include <sys/cms.h>

/* FBIOSATTR device specific array indices, copied from cg4var.h */
#define	FB_ATTR_CG8_SETOWNER_CMD	0	/* 1 indicates PID is valid */
#define	FB_ATTR_CG8_SETOWNER_PID	1	/* new owner of device */

#define	CG8_NFBS	8

#define	CG8_PRIMARY		0x01	/* Mark the PRIMARY Pixrect */
#define	CG8_OVERLAY_CMAP	0x02	/* Overlay CMAP to be changed */
#define	CG8_24BIT_CMAP		0x04	/* 24 Bit CMAP to be changed */
#define	CG8_KERNEL_UPDATE	0x08	/* kernel vs. user ioctl */
					/* 0x10 & 0x20 are dbl buf in cg9 */
#define	CG8_SLEEPING		0x40	/* Denote if wake_up is necessary */
#define	CG8_COLOR_OVERLAY	0x80	/* view overlay & enable as 2 bits */
#define	CG8_UPDATE_PENDING	0x100
#define	CG8_PIP_PRESENT		0x200	/* PIP is present. */
#define	CG8_STOP_PIP		0x400	/* Stop PIP when acc. this plane grp */
#define	CG8_EIGHT_BIT_PRESENT	0x800	/* There is an 8-bit frame buffer. */

#ifndef _KERNEL

Pixrect *cg8_make();
int cg8_destroy();
Pixrect *cg8_region();
int cg8_getcolormap();
int cg8_getattributes();
int cg8_vector();
int cg8_get();
int cg8_put();
#ifdef	__STDC__
static int cg8_rop(Pixrect *dpr, int dx, int dy, int w, int h, int op,
	Pixrect *spr, int sx, int sy);
#else	/* __STDC__ */
int cg8_rop();
#endif	/* __STDC__ */

#endif	/* _KERNEL */

#ifndef pipio_DEFINED

#define	pipio_DEFINED

/*
 * IOCTL definitions for the SBus True Card PIP
 */

/* First unused ioctl number is 43. */

/*
 * EEPROM Write Byte operation control information:
 */
typedef struct {
	int	address;	/* Address in EEPROM to write to. */
	int	value;		/* Value to write (in low-order 8 bits.) */
} EEPROM_Write_Byte;

/*
 * Frame buffer and memory mapping information ioctl:
 */
typedef struct Fb_Description {	/* Frame buffer description: */
	short	group;		/* ... Sun "plane group" of frame buffer. */
	short	width;		/* ... width of frame buffer. */
	short	height;		/* ... height of frame buffer. */
	short	depth;		/* ... depth of frame buffer. */
	uint_t	linebytes;	/* ... # of bytes per scan line for fb. */
	uint_t	mmap_size;	/* ... size of mapping for frame buffer. */
	uint_t	mmap_offset;	/* ... offset for memory map of fb. */
} Fb_Description;

#define	PIP_NFBS 10 /* # of frame buffer descriptions in Pipio_Fb_Info. */
#define	FB_NPGS  12 /* # of plane groups possible. */

typedef struct Pipio_Fb_Info {		/* Frame buffer info record: */
	int	frame_buffer_count;	/* ... # of fbs supported. */
	uint_t	total_mmap_size;	/* ... memory map size of all fbs */
	Fb_Description fb_descriptions[PIP_NFBS];
					/* ... individual fb descriptions */
} Pipio_Fb_Info;

/*
 * Frame buffer emulation ioctl:
 */
typedef struct Pipio_Emulation {	/* Emulation control layout: */
	uchar_t	plane_groups[FB_NPGS];	/* ... plane groups to enable. */
	ushort_t timing;		/* ... timing/size regimen. */
} Pipio_Emulation;
#define	NATIVE_TIMING	0	/* Provide standard (default) timing. */
#define	NTSC_TIMING	1	/* Provide NTSC timing. */
#define	PAL_TIMING	2	/* Provide PAL timing. */

/*
 * I/O controls used by Sunview Pixrect library routines.
 */
#define	XIOC	('X'<<8)
#define	PIPIO_G_FB_INFO		(XIOC|1) /* Get info about fbs. */
#define	PIPIO_G_EMULATION_MODE	(XIOC|3) /* Return current emulation mode. */
#define	PIPIO_S_EMULATION_MODE	(XIOC|4) /* Set the device being emulated. */
#define	PIPIO_G_PIP_ON_OFF	(XIOC|5) /* Get the value of the pip on bit. */
#define	PIPIO_S_PIP_ON_OFF	(XIOC|7) /* Set or clear pip on bit. */
#define	PIPIO_G_PIP_ON_OFF_RESUME (XIOC|9)
				/* Resume (pop) pip operations, return */
				/* new status. */
#define	PIPIO_G_PIP_ON_OFF_SUSPEND (XIOC|10)
				/* Get pip status, & suspend pip ops. */
#define	PIPIO_G_CURSOR_COLOR_FREEZE (XIOC|40)
				/* Get setting of cursor color frozen switch. */
#define	PIPIO_S_CURSOR_COLOR_FREEZE (XIOC|41)
				/* Set cursor color frozen switch. */
#define	PIPIO_S_MAP_SLOT	(XIOC|42) /* Map SBus slot at offset 0x900000 */
#define	PIPIO_G_TEST		(XIOC|43) /* For testing purposes. */
#define	PIPIO_S_TEST		(XIOC|44) /* For testing purposes. */

#endif	/* pipio_DEFINED */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CG8VAR_H */
