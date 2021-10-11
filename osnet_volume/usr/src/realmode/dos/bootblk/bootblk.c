/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)bootblk.c	1.21	99/03/18 SMI\n"

/*
 * BOOT BLK:
 *
 * PURPOSE: to load the SECONDARY BOOT from the boot medium and
 * execute it.  Boot medium can be a UFS or DOS filesystem.
 *
 * loaded by:
 *	SOLARIS PARTITION PRIMARY BOOT (Solaris hard drive partition)
 *	MDBOOT (Solaris boot partition of boot diskette)
 *	El Torito BIOS (direct CDROM boot)
 *
 * This program is a unified version of the previously independent
 * programs BOOTBLK (for UFS filesystems) and STRAP.COM (for DOS
 * filesystems).  The purpose of the unification is to reduce the
 * need for parallel modifications to two programs that do essentially
 * the same job.  Because the two programs have different external
 * interfaces we still build two different programs using conditional
 * compilation and different sets of object files.
 *
 * The initial goal of unification was to preserve all essential
 * functionality of the two versions while combining the basic
 * algorithm.  Further unification might be possible if it can be
 * determined that some of the differences are unnecessary.
 *
 * Suggested enhancement:
 *
 *	Bootblk presently does not attempt to distinguish between
 *	being booted from the active partition of a hard drive and
 *	being booted via an agent similar to reboot() in some other
 *	partition which is marked active.  Bootblk simply reads the
 *	fdisk table from disk.
 *
 *	We would like bootblk to detect that a selection has already
 *	been made and just boot without presenting its own menu.
 *
 *	One way to detect that case would be to use the pointer
 *	passed by standard master bootstraps, but Solaris mboot
 *	does not follow the convention and Solaris pboot does not
 *	use it.  [Solaris pboot rereads the fdisk table and selects
 *	the active partition which is also needs to change.  The
 *	simplest change to pboot would be to change it to search for
 *	a Solaris partition rather than the active one.]
 *
 *	Another way would be to examine the type of the active
 *	partition and determine whether it is reasonable for a
 *	Solaris partition.  Whenever the system ID is not a
 *	recognized Solaris ID, the reboot() function will be
 *	suppressed.  Note that this change would have the side
 *	effect of disabling the reboot functionality if the Solaris
 *	system ID gets mangled - probably not a serious worry.
 */

#include <sys\types.h>
#include <sys\param.h>
#include <sys\fdisk.h>
#include <sys\vnode.h>
#include <stand\sys\saio.h>   /* standalone I/O structure */
#include <bioserv.h>          /* BIOS interface support routines */
#include <bootdefs.h>         /* primary boot environment values */
#include <dev_info.h>         /* MDB extended device information */
#include "bootblk.h"

// BootDbg gets set early in callboot.s by copying from an
// easily patched location.
int BootDbg;               /* global debug output switch */

#ifdef DEBUG
    #pragma message ( __FILE__ ": << WARNING! DEBUG MODE >>" )
    #pragma comment ( user, __FILE__ ": DEBUG ON " __TIMESTAMP__ )
#endif

#pragma comment ( compiler )
#pragma comment ( user, "bootblk.c	1.21	99/03/18" )

#ifndef STACK_SIZE
#define	STACK_SIZE	4096
#endif

int stack_size = STACK_SIZE;
char prog_stack[STACK_SIZE];
struct mboot bootrec;
char far *load_addr = (char far *)DEST_ADDR;
struct pri_to_secboot *realp;

void announce(char *);
void fatal_err();
char far *mov_ptr(char far *, unsigned long);
void spinner(void);

extern void reboot();

int reboot_timeout = REBOOT_TIMEOUT;
short tmpAX = 0, tmpDX = 0;

#ifdef STRAP

long Debug;
char *TitleStr = "\\Sc\\Sp0.0Solaris Boot\\Sp0.69Version 2.0";

#define	NAMESIZ		8
#define MENU_START	8

char *bootfile = "boot.bin";
char *alt_bootfile;
#else
char *bootfile = "/boot/solaris/boot.bin";
char *alt_bootfile = "/platform/i86pc/boot/solaris/boot.bin";
#endif

void
boot_blk(unchar dev)
{
	long fsz;
	char *bf;
	char far *dest;
	char far *pntr;
	long fd;

#ifdef DEBUG
	// Use absence of NUM LOCK to trigger certain debugging behaviors.
	// Also allow patched in DBG_NONUMLOCK to trigger same behavior.
	if (numLockClear() || DBG_NONUMLOCK) {
		int c;

		BootDbg |= DBG_NONUMLOCK;

		// Allow using a non-boot device.
		c = pause(DBG_FDBOOTBIN | DBG_NONUMLOCK, "\n\nPress A to "
			"force floppy or C for hard drive.\nAny other key "
			"to leave unchanged ... ");
		switch (c) {
		case 'A': case 'a':
			dev = 0;
			break;
		case 'C': case 'c':
			dev = 0x80;
			break;
		}
	}
#endif

	/* Check device characteristics and initialize I/O */
	chk_boot_device(dev);

	/*
	 * Read fdisk table unless booting from floppy.
	 * Initialize filesystem access code.
	 */
	if (boot_dev.dev_type != DT_FLOPPY) {
		if (read_sectors(&boot_dev, 0, 1, (char far *)&bootrec) == 0 ||
				bootrec.signature != MBB_MAGIC) {
			c_fatal_err("Cannot read fdisk table.");
		}
		fs_init((struct partentry *)bootrec.parts);
	} else {
		fs_init(0);
	}

	/*
	 * There are significant differences between bootblk and strap.com
	 * that are handled by this ifdef.  It would be possible to hide this
	 * ifdef within a subroutine call, but I chose to leave it like this
	 * to make the differences clear.  AFB 1/7/1999.
	 *
	 * strap.com supports localization by reading an optional locale
	 * file and changing to a location-specific subdirectory.  In order
	 * to support this capability the DOS FS code handles cd while the
	 * UFS FS code does not.  At this time it is not worth changing the
	 * UFS code to match.
	 */
#ifdef STRAP
	fs_cd("\\Solaris");

	/* If there is a locale file read and process it */
	if ((fd = fs_open("locale")) != -1) {
		char langdir[NAMESIZ + 1];
		short rc;
		int i;

		rc = (short)fs_read(fd, langdir, NAMESIZ);
		if (rc > 0) {
			for (i = 0; i < rc; i++) {
				if (langdir[i] == '\r' || langdir[i] == '\n')
					break;
			}
			langdir[i] = 0;

			if (fs_cd(langdir))
				c_fatal_err("Cannot find locale directory.");
		}
	}

	/* If there is a strap.rc file read and process it */
	if ((fd = fs_open("strap.rc")) != -1) {
		char *bp;
		long len;

		/*
		 * The memory allocated here must not be freed
		 * because the parse results point into it.
		 */
		len = fs_seek(fd, 0, 2);
		fs_seek(fd, 0, 0);
		bp = (char *)malloc_util(len);
		if (fs_read(fd, bp, len) != len)
			c_fatal_err("Error reading strap.rc.");
		parserc_strap(bp, (short)len);
	}
#endif

	/*
	 * Allow user to reboot from different OS partition if appropriate.
	 * Follows strap.rc parsing because strap.rc contents can include
	 * reboot-related options.
	 */
	if (boot_dev.dev_type != DT_FLOPPY) {
		Dpause(DBG_FLOW, 0);	/* Allow reading DEBUG messages */
		reboot(bootrec.parts, dev);
	}

	if ((fd = fs_open(bf = bootfile)) == -1 && (alt_bootfile == 0 ||
			(fd = fs_open(bf = alt_bootfile)) == -1)) {
		c_fatal_err("Cannot find boot.bin.");
	}

	announce(bf);

	fsz = fs_seek(fd, 0, 2);
	fs_seek(fd, 0, 0);
	for (dest = load_addr; fsz > 0; fsz -= BLOCK_SIZE,
			dest = mov_ptr(dest, BLOCK_SIZE)) {
		long rc;

		rc = fs_read(fd, dest, BLOCK_SIZE);

		Dprintf(DBG_READ, ("return code from bootstrap read: %ld\n", rc));

		spinner();
	}
	putstr("\r\n\n");

	read_boot_bin_from_floppy();

	Dprintf(DBG_FLOW, ("register initialized; invoking bootstrap\n"));
	Dpause(DBG_FLOW, 0);

	/*
	 * Theoreticlly we could always pass a null pointer to boot.bin.
	 * But in practice, when booting from a hard drive, boot.bin
	 * indirectly uses the presence or absence of a non-null realp
	 * to distinguish between booting from a PCFS boot partition (null)
	 * or a UFS Solaris boot partition (non-null).
	 */
	pntr = (char far *)0L;
#ifndef STRAP
	if (boot_dev.dev_type == DT_HARD) {
		pntr = (char far *)realp;
	}
#endif

	/*
	 * Clean up the registers (especially the upper bits)
	 * to make comparisons easier in boot.bin.
	 */
	register_prep();

	/* Load up the registers used for argument passing */
	_asm {
		mov	dl, dev
		les	di, pntr
	}

	/* Jump to the bootstrap just loaded */
	((void (far *)())load_addr)();
}

char far *            /* returns adjusted segment:offset pointer */
mov_ptr(char far *p, unsigned long len)
{
	seg_ptr u;
	unsigned long linear;

	u.p = p;
	linear = ((unsigned long)u.s.segp << 4) + u.s.offp + len;
	u.s.offp = (linear & 0xF);
	u.s.segp = (linear >> 4);
	return (u.p);
}

/*
 * For backwards compatibility strap.com puts out a title string
 * and "Loading ..." message, bootblk does neither.
 *
 * XXX Should we remove the loading message except for diskette?
 */
void
announce(char *bf)
{
#ifdef STRAP
	int cnt;

	Dpause(DBG_FLOW, 0);
	printf_util(TitleStr);

	cnt = (80 - (strlen_util(bf) + 8)) / 2;
	setprint_util(MENU_START, cnt, "Loading %s", bf);
#endif
}

/*
 * For backwards compatibility spinner is for strap.com only.
 */
void
spinner(void)
{
#ifdef STRAP
#ifdef static
#undef static	/* Somebody #define'd static to null in bootdefs.h! */
#endif
	static char spin_chars[] = "|/-\\";
	static int n;

	/*
	 * Update spinner if I/O request generated any real I/O as
	 * opposed to being fulfilled from the cache.  Gives a more
	 * even spin.
	 *
	 * Original strap.com also did spinner for hard drive (i.e.
	 * boot partition).  Removed for hard drive because I/O
	 * is too fast for spinner to be meaningful.
	 */
	if (boot_dev.dev_type == DT_FLOPPY && io_done())
		setprint_util(MENU_START + 2, 40, "%c",	spin_chars[n++ % 4]);
#endif
}

/*
 * Strings must be null-terminated and count passed to fatal_err
 * includes the null (same as "sizeof" mechanism it replaces).
 */
void
c_fatal_err(char *s)
{
	int n = strlen(s) + 1;

	_asm {
		mov di, s
		mov si, n
		push	es
		pusha
		mov	ax, ds
		mov	es, ax
		mov	ax, 1301h       ;function, cursor mode
		mov	bx, 004fh       ;video page, attribute
		mov	cx, si          ;string length
		mov	dx, 1700h       ;row, column
		mov	bp, di          ;string pointer
		int	10h
		popa
		pop	es

	forever:
		jmp	forever
	}
}

int
pause(int flg, char *s)
{
	unchar key = 0;

	if (flg == DBG_ALWAYS || (BootDbg & flg)) {
		printf(s ? s : "Press any key to continue ... ");
		_asm {
			mov	ah, 0
			int	16h
			mov	key, al
		}
		printf("\n");

#ifdef DEBUG
		/* Stop doing NUM LOCK debugging if NUM LOCK turned on */
		if ((BootDbg & DBG_NONUMLOCK) && !numLockClear()) {
			BootDbg &= ~DBG_NONUMLOCK;
		}
#endif
	}
	return (key);	// Returns the keystroke.
}

/*
 * Aid for debugging from CDROM.  Allows reading a test BOOT.BIN from
 * a diskette, overriding the one on the CD.  That way it is possible
 * to work on BOOT.BIN without cutting a CD every time it changes.
 * Note that this reads a second copy on top of the one read from the
 * CD, so reading from CD is still exercised.
 */
read_boot_bin_from_floppy(void)
{
#ifdef DEBUG
	if (read_boot_bin()) {
		char far *buf = (char far *)load_addr;
		int i;
		int sect_count;
		ushort track;
		ushort error;
		ushort int13_fd_read_track(unchar, ushort, char far *);
#define	FD_SEC_PER_TRK	18	// Assume we are using 1.44M floppy

		// First read will fail due to diskette change, so try twice.
		for (i = 2; i > 0; i--) {
			if (int13_fd_read_track(0, 0, buf) == 0)
				break;
		}
                if (i == 0) {
                	printf("Diskette read failed on track 0.\n");
                	hang();
                }

		// First sector contains ASCII string sector count.  After
		// decoding it, copy remaining sectors from first track to
		// the proper place.  Then update the pointer and counter.
                for (i = 0, sect_count = 0; buf[i] >= '0' && buf[i] <= '9'; i++) {
                	sect_count *= 10;
                	sect_count += buf[i] - '0';
                }
		printf("Reading %d sectors from diskette to %x:%x.\n",
			sect_count, FP_TO_SEG(buf), FP_TO_OFF(buf));
                memcpy(buf, buf + 0x200, (FD_SEC_PER_TRK - 1) * 0x200);
                sect_count -= FD_SEC_PER_TRK - 1;
                buf = (char far *)((ulong)buf + (FD_SEC_PER_TRK - 1) * 0x200000);

                for (track = 1; sect_count > 0; track++) {
			error = int13_fd_read_track(0, track, buf);
			if (error == 0x109) {
				char far *tmpbuf;

				// If error was a DMA boundary error, pick a
				// safe place for the buffer, read and copy.
				tmpbuf = (char far *)((ulong)buf + FD_SEC_PER_TRK * 0x200000);
				error = int13_fd_read_track(0, track, tmpbuf);
                		if (error == 0)
                			memcpy(buf, tmpbuf, FD_SEC_PER_TRK * 0x200);
                	}
			if (error != 0) {
                		printf("Diskette read failed for track %d to %x:%x, error %x.\n",
                			track, FP_TO_SEG(buf), FP_TO_OFF(buf), error);
                		hang();
                	}
                	sect_count -= FD_SEC_PER_TRK;
                	buf = (char far *)((ulong)buf + FD_SEC_PER_TRK * 0x200000);
                }
		pause(DBG_ALWAYS, "Finished reading BOOT.BIN from "
			"diskette.  Press any key to continue... ");
	}
#endif
}

#ifdef DEBUG
ushort
int13_fd_read_track(unchar dev, ushort track, char far *buf)
{
	ushort error;
	unchar head = track % 2;
	unchar cyl = track / 2;
	unchar carry = 0;

        _asm {
               	push	es
                push	bx
                mov	ah, 2
                mov	al, FD_SEC_PER_TRK
                mov	cl, 1
                mov	ch, cyl
                mov	dl, dev
                mov	dh, head
                les	bx, buf
                int	13h
                pop	bx
                pop	es
                mov	al, ah
                mov	ah, 0
                mov	error, ax
                jnc	good_read
		mov	carry, 1
        good_read:
        }
        if (carry)
        	error |= BIOS_CARRY;
        return (error);
}
#endif

#ifdef DEBUG
int
numLockClear(void)
{
	unchar state;

	_asm {
		mov	ah, 2
		int	16h
		mov	state, al
	}
	return ((state & 0x20) ? 0 : 1);
}
#endif

#ifdef DEBUG
int
read_boot_bin(void)
{
	int c;

	if ((BootDbg & (DBG_FDBOOTBIN | DBG_NONUMLOCK)) == 0)
		return (0);
	c = pause(DBG_FDBOOTBIN | DBG_NONUMLOCK, "To read BOOT.BIN "
		"from diskette, insert the diskette then press "
		"<ENTER>.\nPress any other key to skip ... ");
	return ((c == '\r') ? 1 : 0);
}
#endif
