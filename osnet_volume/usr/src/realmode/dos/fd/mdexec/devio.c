/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)devio.c	1.9	94/05/23 SMI\n"

/*
 * Device interface code for standalone I/O system.
 * Floppy diskette version.
 *
 * */

#ifdef FARDATA
#define _FAR_ _far
#else
#define _FAR_
#endif


extern MDXdebug;		/* global debug output switch */

#ifdef DEBUG
#pragma message ( __FILE__ ": << WARNING! DEBUG MODE >>" )
#pragma comment ( user, __FILE__ ": DEBUG ON " __TIMESTAMP__ )
#endif

#pragma comment ( compiler )
#pragma comment ( user, "devio.c	1.9	94/05/23" )

#include <sys\types.h>
#include <sys\param.h>
#include <bioserv.h>	  /* BIOS interface support routines */
#include <dev_info.h>	 /* MDB extended device information */
#include <bootp2s.h>	  /* primary/secondary boot interface */
#include <bootdefs.h>	 /* primary boot environment values */
#include <bpb.h>		/* BIOS parameter block */

extern short fBootDev;
extern struct bios_param_blk bpb;

typedef union {
	struct {
		u_short offp;
		u_short segp;
	} s;
	char _far *p;
	u_long l;
} seg_ptr;

extern char _far *mov_ptr(char _far *, long);


static u_short
floppy_read(u_short drive, u_short cyl, u_short head, u_short sector,
    u_short nsectors, char _far *buffer)
{
	_asm {
	mov	al, byte ptr nsectors
	mov	ah, 02h
	mov	cl, byte ptr sector
	mov	ch, byte ptr cyl
	mov	dl, byte ptr drive
	mov	dh, byte ptr head
	mov	bx, word ptr buffer
	mov	es, word ptr buffer + 2

	int	13h
	}
}


long
devread(daddr_t adjusted_blk, char _far *Buffer, long numsect)
{
	seg_ptr longadr;
	union fdrc_t devrc;
	long nsect;
	long xfrd_sectors = 0;
	int retry = 4;
	u_short BIOSCylinder, BIOSSector, BIOSHead;
	u_short sectsize = bpb.VBytesPerSector;
	u_short track;
	char _far *addr;
	u_char rc;

#ifdef DEBUG
	if (MDXdebug) {
	printf("devread: adjusted_blk %ld\n", (long)adjusted_blk);
#if 0
	printf("SecPerTrk: %ld\n", (long)SecPerTrk);
	printf("SecPerCyl: %ld\n", (long)SecPerCyl);
	printf("Head: %ld\n", (u_short)Head(adjusted_blk));
	printf("Track: %ld\n", (u_short)Track(adjusted_blk));
	printf("Sector: %ld\n", (u_short)Sector(adjusted_blk));
#endif
	}
#endif
	for (addr = (char _far *) Buffer; numsect > 0;
	    adjusted_blk += devrc.f.nsectors,
	    addr = mov_ptr(addr, devrc.f.nsectors * sectsize),
	    numsect -= devrc.f.nsectors) {

		track = (u_short) adjusted_blk / bpb.VSectorsPerTrack;
		BIOSCylinder = track / bpb.VNumberOfHeads;
		BIOSHead =  track - (BIOSCylinder * bpb.VNumberOfHeads);
		BIOSSector = (u_short) adjusted_blk -
		    (track * bpb.VSectorsPerTrack) + 1;

		/*
		 * check for DMA restrictions
		 */
		nsect = numsect;
		longadr.p = addr;
		longadr.l = ((u_long)longadr.s.segp << 4) + longadr.s.offp;
		if ((long)~longadr.s.offp < nsect * sectsize - 1L) {
			/* don't cross 64K address boundary */
			nsect = ((long)~longadr.s.offp + 1L) / sectsize;
			if (!nsect) {
				longadr.l = (u_long) addr;
#ifdef DEBUG
				printf("devread: addr %4x:%4x is not aligned\n",
				    longadr.s.segp, longadr.s.offp);
#endif
				return 0;
			}
		}
		/*
		 * check for floppy track boundary
		 */
		if (((adjusted_blk % bpb.VSectorsPerTrack) + nsect) >
		    bpb.VSectorsPerTrack) {
			/* keep request to single track */
			nsect = bpb.VSectorsPerTrack -
			     (adjusted_blk % bpb.VSectorsPerTrack);
		}
#ifdef DEBUG
		if (MDXdebug) {
			longadr.p = addr;
			printf("devread: c=%d h=%d s=%d n=%ld addr=%04x:%04x\n",
			    BIOSCylinder, BIOSHead, BIOSSector,
			    nsect, longadr.s.segp, longadr.s.offp);
		}
#endif
		do {
			devrc.retcode = floppy_read(fBootDev,
			    BIOSCylinder, BIOSHead, BIOSSector,
			    (u_short) nsect, addr);

			if (rc = devrc.f.fd_rc) {
#ifdef DEBUG
				if (MDXdebug)
				printf("devread: diskette error %x\n", rc);
#endif
				if (rc & 0xC0) {
					/* on seek error or timeout */
					(void) reset_disk(0);
				}
			}
		} while (rc && retry--);

		if (rc)
			return xfrd_sectors * sectsize;
		else {
#ifdef DEBUG
			if (MDXdebug)
			printf("devread: %d sectors.\n", devrc.f.nsectors);
#endif
			devrc.f.nsectors = nsect;
			xfrd_sectors += devrc.f.nsectors;
		}
	}
	nsect = xfrd_sectors * sectsize;
#ifdef DEBUG
	if (MDXdebug)
	printf("devread: %ld bytes read.\n", nsect);
#endif
	return nsect;
}
