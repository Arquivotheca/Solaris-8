/*
 * Copyright (c) 1993, 1999 Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident  "@(#)bootdefs.h 1.12     99/05/26 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		bootdefs.h
 *
 *   Description:	some useful values used throughout the real mode primary
 *			boot environment; put them here in one central location.
 *
 */

extern short BootDev;	/* boot device; passed in DL between boot stages */
/*
 * used by mastboot.c
 */
#define	MBOOT_ADDR 0x00007C00
#define	PBOOT_ADDR 0x00007E00

#define	FDISK_START 0x1BE /* relative offset of fdisk table within bootrec */
#define	FDISK_CYL_MASK 0xC0

/*
 * used by partboot.c
 */
#define	SM_SIZE 512
#define	VTOC_OFFSET  3

/*
 * used by cdboot.c
 */
#define	CD_VTOC_OFFSET  3

/*
 * used by blueboot.c
 */
#define	BIG_BLOCK 0x2000
#define	BLOCK_SIZE (long)0x2000
#define	DEST_ADDR 0x00008000  /* load address of secondary boot */

/*
 * used by reboot.c
 */
#define	LINELEN   64
#define	NUMLEN    12
#define	ESCAPE    0x1b
#define	PROMPTROW 16
#define	PROMPTCOL MENU_STARTCOL - 5
#define	FTNOTEROW 16

/*
 * the current list of supported hard disk controllers.
 */
#define	DEFAULT_HBA_TYPE 3	/* this value is used as the default hba */
				/* type, in case no other string matches. */
#define	AHA154X_TYPE	0
#define	AHA174X_TYPE	1
#define	DPT_TYPE	2
#define	IDE_TYPE	3
#define	MAX_HBA_TYPE	4	/* current max number of defined hba types */

/*
 * used by ufs.c
 */
/* hard-wire these for now, until we can get INT 13, function 08h to work. */

#define	static	/* big vla fornow!..... */

#define	TEST_UNIT		  0
#define	TEST_CTLR		  1
/* #define	TEST_DEV		  0x81 */

/*
 * used by devio.c
 */
#define	Nsectors 0x10
#define	SECTOR_SIZE 512

/*
 * used by mdboot.c (Multiple Device Boot Loader)
 */
#define	MDEXEC_ADDR 0x60000000

/*
 * used by mdexec.c (Multiple Device Boot)
 */
#define	N_RETRIES 5
#define	N_DIRBLKS 14
#define	N_DIRENTS 16
#define	BEF_ADDR 0x00008000


/*
 * used by mdexec.c
 * this set of define's, prototypes came from the DOS version of "string.h"
 * The DOS size_t is two bytes, the UNIX size_t is four bytes; so the DOS
 * version is renamed DOS_size_t; all function prototypes that are required
 * by mdexec are defined here, so that we don't need to include "string.h".
 * Also, any int's are resolved here explicitly into short's/long's; each
 * OS uses "int" in a cavalier fashion that has caused many headaches.
 */
#define	BOOT_SIGNATURE_START 0x1FE  /* offset of signature within bootrec */
#define	BOOT_SIG	0xAA55	/* boot record signature bytes */
#define	MDB_SIG   0x5D42444D	/* MDBexec signature bytes "MDBX" */
#define	BEF_SIG   0x5A4D	/* BEF/(exe) header signature bytes */

#ifndef USHORT_DEFINED
typedef unsigned short ushort;
#define	USHORT_DEFINED
#endif

/*
 * We need to explicitly declare any function prototypes from the DOS
 * standard C library, because "int", (used freely everywhere in both
 * DOS and UNIX), differs between these two worlds.
 */
/* File attribute constants (from dos.h) */

#define	_A_NORMAL	0x00	/* Normal file - No read/write restrictions */
#define	_A_RDONLY	0x01	/* Read only file */
#define	_A_HIDDEN	0x02	/* Hidden file */
#define	_A_SYSTEM	0x04	/* System file */
#define	_A_VOLID	0x08	/* Volume ID file */
#define	_A_SUBDIR	0x10	/* Subdirectory */
#define	_A_ARCH 	0x20	/* Archive file */

/* String-related function prototypes (from string.h) */

#if 0
#ifndef DOS_SIZE_T_DEFINED
typedef unsigned short DOS_size_t;
#define	DOS_SIZE_T_DEFINED
#endif

char _far * _far _cdecl _fstrcpy(char _far *, const char _far *);
short _far _cdecl _fstrncmp(const char _far *, const char _far *, DOS_size_t);
void _FAR_ * _FAR_ _cdecl memset(void _FAR_ *, short, DOS_size_t);

/* Type-conversion function prototypes (from stdlib.h) */

short _FAR_ _cdecl atoi(const char _FAR_ *);
char _FAR_ * _FAR_ _cdecl itoa(short, char _FAR_ *, short);
#endif

/* MDB defines */
#define	FIRST_BOOT_DEVNUM 0x10   /* MDB uses 10-7Fh, 90-9Fh to dynamically */
#define	LAST_BOOT_DEVNUM  0x7F   /* assign boot device codes */
#define	DEFAULT_BOOT_DEVNUM 0x80
#define	ERASED_FILE 0xFFE5

/*
 * Screen definitions used by the MDB boot device menu,
 * and the boot-time alternate partition boot menu.
 */
#define	MENU_STARTCOL 19
#define	MENU_STARTROW 6
#define	SCREEN_WIDTH  80
#define	MENU_WIDTH    55
#define	STATUS_ROW    23
#ifdef	MONO
#define	MENU_COLOR	0x70	/* black on white */
#define	ERROR_ATTR	0x0f	/* bright white on black */
#define	STATUS_ATTR	0x07	/* white on black */
#define	WORK_ATTR	0x87	/* blinking white on black */
/* #define	MENU_COLOR	0x07	/+ white on black */
/* #define	ERROR_ATTR	0x0f	/+ bright white on black */
/* #define	STATUS_ATTR	0x70	/+ black on white */
/* #define	WORK ATTR	0xf0	/+ blinking black on white */
#else
#define	MENU_COLOR	0x1b	/* bright cyan on blue */
#define	ERROR_ATTR	0x4f	/* bright white on red */
#define	STATUS_ATTR	0x31	/* blue on cyan */
#define	WORK_ATTR	0xb1	/* blinking blue on cyan */
#endif

#define	MDB_TIMEOUT    30	/* timeout period for bootable device menu */
#define	REBOOT_TIMEOUT 30	/* timeout period for boot partition menu */
#define	TicksPerSec  18		/* clock ticks per sec (rounded) */
