/*
 * Copyright (c) 1995 Sun Microsystems, Inc.  All rights reserved.
 */

#ident	"@(#)mdexec.c	1.27	95/08/23 SMI\n"

/*
 * SOLARIS MDB EXECUTIVE:
 *
 * PURPOSE: allows the selection of alternate bootable devices.
 *          First implementation of SunSoft MDB (Multiple Device Boot) Spec.
 *
 *  SYNOPSIS:
 *      loaded from the boot diskette.  The boot executive is
 *            assumed to exist in consecutive sectors, starting from
 *            the first available cluster in the DOS filesystem.
 *            It is also the first filename in the root directory.
 *      begins execution at 5000:0000h
 *      verify boot record signature bytes
 *
 *      load & execute the SOLARIS ufsboot
 *
 *      error handler - can either reboot, or invoke INT 18h.
 *
 *  interface with master boot: BootDev in DL
 *
 *===========================================================================
 * MDB Boot Executive: first file in root directory
 *
 */

#pragma comment (compiler)
#pragma comment (user, "mdexec.c	1.27	95/08/23")

#if defined(DEBUG) || defined(DEBUG_CONF) || defined(DEBUG_MULTI) || \
	defined(DEBUG_UFSBOOT)
    #pragma message (__FILE__ ": << WARNING! DEBUG MODE >>")
    #pragma comment (user, __FILE__ ": DEBUG ON " __TIMESTAMP__)

#define PAK	printf("Press any key to continue: "), \
		read_key(), \
		putchar('\r'), \
		putchar('\n')
#define	PERCENT_C(c)	(long)(unsigned char)c	/* C lib oddity */
#endif

#ifdef FARDATA
#define _FAR_ _far
#else
#define _FAR_
#endif

#include <sys\types.h>
#include <ctype.h>
#include <bioserv.h>          /* BIOS interface support routines */
#include <bootdefs.h>         /* primary boot environment values */
#include <dev_info.h>         /* MDB device information structure */
#include "..\extbios\scsi\scsi.h"

#define UFSFILE	"UFSBOOT    "
#define	DEVFILE	"BOOTDEV CNF"
#define	SETFILE	"DISKETTE"

/*
 * Disable use of boot device configuration file pending ARC approval.
 */
#define	DISABLE_DEVFILE

/*
 * Define some of the conditions we use for validating boot device number
 */

#define BIOSDRIVE(num) \
	((num) >= DEFAULT_BOOT_DEVNUM && (num) <= DEFAULT_BOOT_DEVNUM+3)

/* must have lastdev in scope to use this or LEGAL_BOOTDEV */
#define BEFDRIVE(num) \
	((num) >= FIRST_BOOT_DEVNUM && (num) <= lastdev)

/* must have lastdev and numata in scope */
#define LEGAL_BOOTDEV(num) \
	((BIOSDRIVE(num) && ((1 << (num & 0x7F)) & numata)) || BEFDRIVE(num))


/*
 * Prototypes for routines in dosfs.c.
 */
extern long close(long);
extern long filesize(long);
extern long open(char *);
extern int readfile(long, char _far *);

/*
 * The same source file is used to build the boot executive program for
 * all floppy media sizes.
 */

char _far *MDBmast_boot = 0;       /* bogus dcl to placate MSC 6.0 */
/* extern void (_far *MDBmast_boot)();   /* MDB master boot record */

extern void displ_err();
extern get_drive_geometry();
extern int get_rdir(int, DOSdirent *);

short build_menu(short lastdev);
char *build_outstr(short);
void clear_line(short);
void clear_status(short, short);
unchar determineDevType(DEV_INFO _far *);
char *get_dev_type(DEV_INFO *);
DEV_INFO _far *get_id(short);
char *get_str(char);
char _far *loadFile(char *, char _far *);
short loadNrun(char *, char _far *);
void lowcase(char *);
void master_boot(void);
short menu_header(short);
char _far *keyVal(char _far *, char **, char **);
void process_dev(char _far *);
void prt_time(u_short);
void selectDefaultDevice();


extern short BootDev;
extern short MDXdebug;


/*
 * Definitions and variables used in boot device selection.
 */
#define	TYPE_NONE	0
#define	TYPE_CDROM	1
#define	TYPE_DISK	2
#define	TYPE_NET	3
unchar defaultDevType = TYPE_NONE;
unchar defaultDevCode = 0;
short defaultDev = DEFAULT_BOOT_DEVNUM;

short view_parts = 0;
short curpage = 0, currow = 0, curcol = 0;
static char devbuf[20];
static long int10_save = 0;
static short numata = 0;

char LoadErr1[] = "Cannot load MBR from specified boot device.";
short LoadErr1Siz = sizeof(LoadErr1);
char BadRecErr[] = "Invalid MD master boot record.";
short BadRecErrSiz = sizeof(BadRecErr);
char DrvErr[] = "Error reading driver file.";
short DrvErrSiz = sizeof(DrvErr);
char BefErr[] = "Illegal BEF driver format.";
short BefErrSiz = sizeof(BefErr);
char IDErr[] = "Extended device call failed.";
short IDErrSiz = sizeof(IDErr);
/*char NoMDBErr[] = "No MDB devices detected; performing standard \"Drive C:\" boot.";*/
char NoMDBErr[] = "No bootable devices found; please check SCSI bus termination and device ID#'s.";
short NoMDBErrSiz = sizeof(NoMDBErr);
char FlopErr[] = "Cannot access diskette.";
short FlopErrSiz = sizeof(FlopErr);
char Banner[] = "Solaris for x86                                             Multiple Device Boot\n";
short BannerSiz = sizeof(Banner);
char Hdr1[] = "Solaris/x86 Multiple Device Boot Menu\n";
char Hdr2[] = "Code  Device Vendor   Model/Desc       Rev\n";
char Hdr3[] = "===============================================\n";
char DevPrmpt[] = "Enter the boot device code: ";
char outstr[MENU_WIDTH] = { 0 };
char blnkL[] = "                                                                   ";
char DevcodeErr[] = "Invalid MDB device code entered; please re-enter code.";
char LoadMDB[] = "Loading master boot record from specified device ...\r\n";
char LoadDEF[] = "Loading master boot record from BIOS drive ...\r\n";
char workmsg[] = " Working ...";

/*
 *  OK, I've just been loaded by mdboot.  Now what?
 */

mdb_exec()
{
	DOSdirent dirent;
	DOSdirent *pdir;
	int filenum;
	int diskNum = 1;
	int menuDisk = 0;
	int setNum;
	int setSeen;
	short rc;
	short lastdev = 0;
	short ufs_exists = 0;
	u_short cfbyte;
	char filename[12];
	char _far *loadspc;

#ifdef DEBUG
	MDXdebug = 1;
#endif

	curpage = ask_page();
	putstr("\r\nLaunching MDB device probes ...\r\n\n");
	Banner[80] = (u_char)255;


	/*
	 * Start out by using the UFSBOOT buffer area for general buffer.
	 */
	loadspc = (char _far *)DEST_ADDR;

	pdir = &dirent;

	/*
	 * Pre-scan the root directory of the boot diskette, looking for
	 * a DISKETTE.nnn file.  This file is optional for the boot disk
	 * and required otherwise.
	 */
new_disk:
	if (diskNum > 1) {
		printf(
"Insert diskette %d in drive A and type any key to continue: ",
			diskNum);
		read_key();
		printf("\n");
	}

	if (dosfs_init() < 0) {
		_asm {
		mov	di, offset FlopErr	  ;"Cannot access floppy."
		mov	si, FlopErrSiz
		call	displ_err
		jmp	$
		}
	}

	setSeen = 0;
	for (filenum = 0; (filenum = get_rdir(filenum, &dirent)) > 0; ) {
		if (!strncmp(pdir->fname, SETFILE, sizeof(pdir->fname))) {
			setNum = atoi(pdir->ext);
			setSeen++;
#ifdef DEBUG_MULTI
			printf("located %s.%03d on diskette %d\n",
				SETFILE, setNum, diskNum);
#endif
			bcopy(&pdir->fname[0], filename,
				sizeof(pdir->fname) + sizeof(pdir->ext));
			filename[11] = '\0';
		}
	}

	if (diskNum == 1) {
		if (setSeen == 0) {
			/* Boot disk with no DISKETTE.nnn is single disk */
			menuDisk = 1;
		}
		else if (setSeen > 1 || setNum != 1) {
			/*
			 * Boot disk with more than one DISKETTE.nnn or
			 * with nnn not equal to 1 is improperly formed.
			 * Assume it has been corrupted.
			 */
			printf("MDB boot diskette (disk 1) is corrupted.\n");
			for (;;)
				continue;
		}
	}
	else if (setSeen != 1 || setNum != diskNum) {
		goto new_disk;
	}
	else
		printf("\n");

	if (setSeen) {
		/* Check the contents of the DISKETTE.nnn file */
		menuDisk = process_set(filename, loadspc);
	}

	/* scan the root directory of boot diskette, looking for *.BEF files */
	for (filenum = 0; (filenum = get_rdir(filenum, &dirent)) > 0; ) {

		if (!strncmp(pdir->ext, "BEF", sizeof(pdir->ext))) {
			/* for each *.BEF file on this diskette, do the following: */
			bcopy(&pdir->fname[0], filename,
			    sizeof(pdir->fname) + sizeof(pdir->ext));
			filename[11] = '\0';
#ifdef DEBUG
			if (MDXdebug)
			printf("checking driver file: %s [%d]\n",
			    filename, pdir->size);
#endif
			rc = loadNrun(filename, loadspc);

			lastdev = (rc > 0) ? rc : lastdev;
#ifdef DEBUG
			if (MDXdebug)
			printf("returned driver code: 0x%x\n", lastdev);
#endif
			continue;
		}
		if (!strncmp(pdir->fname, UFSFILE,
		    sizeof(pdir->fname) + sizeof(pdir->ext))) {
#ifdef DEBUG
			if (MDXdebug)
			printf("located %s on diskette %d\n", UFSFILE, diskNum);
#endif
			ufs_exists++;
			if (ufs_exists > 1) {
				printf(
"Ignoring extra copy of %s on diskette %d.\n", UFSFILE, diskNum);
				continue;
			}
			loadspc = loadFile(UFSFILE, loadspc);
			continue;
		}
#ifndef	DISABLE_DEVFILE
		if (strncmp(pdir->fname, DEVFILE,
		    sizeof (pdir->fname) + sizeof (pdir->ext)) == 0) {
#ifdef DEBUG
			printf("located %s on diskette\n", DEVFILE);
#endif
			/*
			 * Parse the file now in case we switch diskettes.
			 */
			process_dev(loadspc);
			continue;
		}
#endif
	}

	if (!menuDisk) {
		diskNum++;
		goto new_disk;
	}

	pause_ms(1500);

#ifdef DEBUG
	if (MDXdebug)
	printf("highest MDB device code that was detected: 0x%x\n", lastdev);
	if (MDXdebug & 0x8000) {
		printf("Press any key to continue ...");
		wait_key();
	}
	putstr("\r\n");
#endif
	numata = ata_find();

#ifdef DEBUG
	if (MDXdebug && defaultDevCode != 0 || defaultDevType != TYPE_NONE)
		PAK;
#endif

	/*
	 * Determine the default boot device based on the configuration
	 * file and the devices found.  The menu will still be presented
	 * to allow user override.
	 */
	selectDefaultDevice(lastdev);

#ifdef DEBUG_CONF
	printf("Default boot device is 0x%x\n", defaultDev);
	PAK;
#endif

retry:
	if (lastdev >= FIRST_BOOT_DEVNUM || numata > 0)
		/*
		 * Found at least one bootable device.
		 *
		 * generate bootable device menu
		 */
		build_menu(lastdev);

	else {
		_asm {
		mov	di, offset NoMDBErr
		mov	si, NoMDBErrSiz
		call	displ_err
		jmp	$
		}
	}

	/*
	 * display menu, begin timeout period; wait for user input.
	 * if timeout period expires, boot from default Drive C:
	 */
	BootDev = process_menu(lastdev, defaultDev);
	if (BIOSDRIVE(BootDev)) {
		/* reset disk subsystem to allow BIOS to boot the drive */
		_asm {
		mov	ax, 00h
		int	13h
		}
	}

	_asm {
	mov	dx, BootDev
	call	get_drive_geometry	; let the drive tell us its parameters!
	}

	if (ufs_exists && !BIOSDRIVE(BootDev)) {
#ifdef DEBUG
		if (MDXdebug)
		printf("Taking 3rd-party boot path\n");
#endif
		boot_blk();
	} else {
		clr_screen_attr(7);
		prtstr_attr(Banner, sizeof(Banner), curpage, 0, 0, 7);
		if (BIOSDRIVE(BootDev))
			prtstr_attr(LoadDEF, sizeof(LoadDEF), curpage, 2, 0, 7);
		else
			prtstr_attr(LoadMDB, sizeof(LoadMDB), curpage, 2, 0, 7);
		/*
		 * for the specified device, call INT 13h to retrieve the first
		 * physical sector from the bootable device.
		 */
		master_boot();
		/* lastmod(); */

		/* not reached */
	}
	pause_ms(500);
	goto retry;
}

/*
 * If the user chose a specific device code, that takes precedence.
 * Otherwise look for a device of the chosen type.
 */
void
selectDefaultDevice(short lastdev)
{
	ushort i;
	DEV_INFO _far *dip;

	if (LEGAL_BOOTDEV(defaultDevCode)) {
		defaultDev = defaultDevCode;
		return;
	}
	if (defaultDevType == TYPE_NONE)
		return;

	/*
	 * Devices indicated by numata are all disks and are considered
	 * to precede extended BIOS codes for purpose of selecting first disk.
	 * Non-disk devices are handled by ata.bef and have extended BIOS codes.
	 */
	if (defaultDevType == TYPE_DISK) {
		for (i = 0; i < 4; i++) {
			if (numata & (1 << i)) {
				defaultDev = 0x80 + i;
				return;
			}
		}
	}

	/* Extended BIOS codes can represent any valid device type */
	for (i = FIRST_BOOT_DEVNUM; i <= lastdev; i++) {
		if ((dip = get_id(i)) == 0)
			continue;
		if (determineDevType(dip) == defaultDevType) {
			defaultDev = i;
			return;
		}
	}
			
}

unchar
determineDevType(DEV_INFO _far *dip)
{
	if (dip->dev_type == MDB_NET_CARD)
		return (TYPE_NET);

	if (dip->dev_type == MDB_SCSI_HBA) {
		switch(dip->MDBdev.scsi.pdt & INQD_PDT_DMASK) {
		case INQD_PDT_DA:
			return (TYPE_DISK);
		case INQD_PDT_ROM:
			return (TYPE_CDROM);
		}
	}
	return (TYPE_NONE);
}

/*
 * Collect any user-supplied default boot device information.
 * Do not attempt to valid the results because on a multi-diskette
 * set the default info might precede BEFs it selects.
 */
void
process_dev(char _far *load_adr)
{
	long fd;
	long size;
	char *keyword;
	char *value;

	/* Mark the buffer before reading */
	load_adr[0] = ' ';

	if ((fd = open(DEVFILE)) < 0L) {
#ifdef DEBUG_CONF
		printf("process_dev: error opening %s\n", DEVFILE);
#endif
		return;
	}
	if ((size = filesize(fd)) == 0) {
#ifdef DEBUG_CONF
		printf("process_dev: %s is empty\n", DEVFILE);
#endif
		close(fd);
		return;
	}
	if (size > 64000L) {
#ifdef DEBUG_CONF
		printf("process_dev: %s is too big\n", DEVFILE);
#endif
		close(fd);
		return;
	}
	if (readfile(fd, load_adr) != 0) {
		close(fd);
#ifdef DEBUG_CONF
		printf("process_dev: error reading %s\n", DEVFILE);
#endif
		return;
	}
	close(fd);
	load_adr[size] = 0;	/* Null-terminate file contents */

#ifdef DEBUG_CONF
	printf("process_dev: parsing %s\n", DEVFILE);
#endif

	while ((load_adr = keyVal(load_adr, &keyword, &value)) != 0) {
#ifdef DEBUG_CONF
		printf("process_dev: keyVal keyword \"%s\", value \"%s\"\n",
			keyword, value ? value : "(null)");
#endif
		lowcase(keyword);
		if (strcmp(keyword, "code") == 0) {
			if (value == 0 || *value == 0)
				continue;
			lowcase(value);
			for (defaultDevCode = 0; *value; value++) {
				if (*value >= '0' && *value <= '9')
					defaultDevCode = (defaultDevCode << 4) +
					(*value - '0');
				else if (*value >= 'a' && *value <= 'f')
					defaultDevCode = (defaultDevCode << 4) +
						(*value - 'a' + 0xA);
				else {
					defaultDevCode = 0;
					break;
				}
			}
#ifdef DEBUG_CONF
			printf("process_dev: default device code=%x\n",
				defaultDevCode);
#endif
		}
		else if (strcmp(keyword, "type") == 0) {
			if (value == 0 || *value == 0) {
				defaultDevType = TYPE_NONE;
				continue;
			}
			lowcase(value);
			if (strcmp(value, "cdrom") == 0) {
				defaultDevType = TYPE_CDROM;
			}
			else if (strcmp(value, "disk") == 0) {
				defaultDevType = TYPE_DISK;
			}
			else if (strcmp(value, "net") == 0) {
				defaultDevType = TYPE_NET;
			}
			else
				defaultDevType = TYPE_NONE;
#ifdef DEBUG_CONF
			printf("process_dev: default device type=%x\n",
				defaultDevType);
#endif
		}
#ifdef DEBUG_CONF
		else
			printf("process_dev: unknown keyword %s\n", keyword);
#endif
	}

#ifdef DEBUG_CONF
	PAK;
#endif
}

int
process_set(char *filename, char _far *load_adr)
{
	long fd;
	long size;
	int answer = 0;
	char *keyword;
	char *value;

	/* Mark the buffer before reading */
	load_adr[0] = ' ';

	if ((fd = open(filename)) < 0L) {
#ifdef DEBUG_MULTI
		printf("process_set: error opening %s\n", filename);
#endif
		return (0);
	}
	if ((size = filesize(fd)) == 0) {
#ifdef DEBUG_MULTI
		printf("process_set: %s is empty\n", filename);
#endif
		close(fd);
		return (0);
	}
	if (size > 64000L) {
#ifdef DEBUG_MULTI
		printf("process_set: %s is too big\n", filename);
#endif
		close(fd);
		return (0);
	}
	if (readfile(fd, load_adr) != 0) {
		close(fd);
#ifdef DEBUG_MULTI
		printf("process_set: error reading %s\n", filename);
#endif
		return (0);
	}
	close(fd);
	load_adr[size] = 0;	/* Null-terminate file contents */

#ifdef DEBUG_MULTI
	printf("process_set: parsing %s\n", filename);
#endif

	while ((load_adr = keyVal(load_adr, &keyword, &value)) != 0) {
#ifdef DEBUG_MULTI
		printf("process_set: keyVal keyword \"%s\", value \"%s\"\n",
			keyword, value ? value : "(null)");
#endif
		lowcase(keyword);
		if (strcmp(keyword, "menu") == 0) {
			answer = 1;
#ifdef DEBUG_MULTI
			printf("process_set: found MENU keyword.\n");
#endif
		}
#ifdef DEBUG_MULTI
		else
			printf("process_set: unknown keyword %s\n", keyword);
#endif
	}

#ifdef DEBUG_MULTI
	PAK;
#endif
	return (answer);
}


/*
 * Cannot put these items inside keyVal.  static is apparently defined
 * away in some header file.
 */
#define	KEYWORDLEN	80
#define	VALUELEN	160
static char keywordBuf[KEYWORDLEN + 1];
static char valueBuf[VALUELEN + 1];

/*
 * keyVal expects a far pointer into a buffer containing the image of a
 * file being parsed.  Parsing is line-oriented and lines are expected to
 * be empty, or contain comments, or contain entries of the form "keyword"
 * or "keyword = value".  keyVal returns null if no more entries are found,
 * otherwise it returns the starting point for the next line.  If keyVal
 * returns a non-null pointer, it also returns pointers to the keyword and
 * value at kp and pv.  kp is a pointer to the keyword and is always non-null
 * but can address a null keyword (if a line is of the form "= value").  vp
 * is a pointer to the value and can be null (for "keyword") or address a
 * null value (for "keyword =").
 *
 * Note that buffer is a far pointer and kp and vp are near pointers.
 */
char _far *
keyVal(char _far *s, char **kp, char **vp)
{
#define	CONTROL_Z	26
#define	WHITE_SPACE(s)	((s) == ' ' || (s) == '\t')
#define	LINE_END(s)	((s) == '\r' || (s) == '\n' || (s) == CONTROL_Z)
#define	COMMENT(s)	((s) == '#')

	int index;

  keyValRestart:

	/* Skip leading white space and empty lines */
	while (WHITE_SPACE(*s) || LINE_END(*s))
		s++;

	/* Stop if reached end of file */
	if (*s == 0)
		return (0);

	/* Skip remainder of line if comment */
	if (COMMENT(*s)) {
		do {
			s++;
		} while (*s != 0 && !LINE_END(*s));
		goto keyValRestart;
	}

	for (index = 0; index < KEYWORDLEN; index++) {
		if (*s == 0 || *s == '=' || WHITE_SPACE(*s) || LINE_END(*s))
			break;
		keywordBuf[index] = *s++;
#ifdef DEBUG
		if (MDXdebug)
		printf("keyVal: keyword char '%c' 0x%x\n",
			PERCENT_C(keywordBuf[index]), keywordBuf[index]);
#endif
	}
	keywordBuf[index] = 0;
	*kp = keywordBuf;

	while (WHITE_SPACE(*s))
		s++;

	if (*s == '=') {
		s++;
		while (WHITE_SPACE(*s))
			s++;
		for (index = 0; index < VALUELEN; index++) {
			if (*s == 0 || *s == '=' || WHITE_SPACE(*s) ||
					LINE_END(*s))
				break;
			valueBuf[index] = *s++;
#ifdef DEBUG
			if (MDXdebug)
			printf("keyVal: value char '%c' 0x%x\n",
				PERCENT_C(valueBuf[index]), valueBuf[index]);
#endif
		}
		valueBuf[index] = 0;
		*vp = valueBuf;
	}
	else
		*vp = 0;

	/* Ignore any trailing characters on line */
	while (*s && !LINE_END(*s))
		s++;

	return (s);
}


void
lowcase(char *s)
{
	for (; *s; s++)
		if (isupper(*s))
			*s += 'a' - 'A';
}


void
int10_restore()
{
	/*
	 * Restore the BIOS INT0x10 vector saved earlier.
	 */
	long _far *int10_vect = (long _far *)0x40;

	*int10_vect = int10_save;
}


int
int10_rtn()
{
	/*
	 * This routine serves two purposes.  The inner portion (from
	 * int10_entry to int10_done is the interrupt routine where we
	 * intercept BIOS INT 0x10.  When we catch an INT 0x10 we undo
	 * the catching, clear the status line and jump to the original
	 * INT 0x10 handler.  The outer portion can be called to
	 * return the offset for the interrupt routine.  It is a little
	 * strange but should be compiler independent and does not
	 * require a separate assembler code file.
	 */
	static short savpos;	/* "static" keeps it off the stack */

	_asm {
	mov	ax, offset int10_entry
	jmp	int10_done
int10_entry:
	pusha
	push	ds
	push	es
	push	cs
	pop	ds
	}

	int10_restore();

	savpos = read_cursor(curpage);
	clear_line(STATUS_ROW);
	set_cursor(curpage, (short)(savpos >> 8), (short)(savpos & 0x0F));

	_asm {
	pop	es
	pop	ds
	popa
	jmp	cs:far ptr int10_save
int10_done:
	}
}


void
int10_catch()
{
	/*
	 * Routine to set up to catch BIOS INT 0x10 calls.
	 * See int10_rtn above for details of how it works.
	 */
	union {
		long l;
		short s[2];
	} u;
	short entry_cs;
	long _far *int10_vect = (long _far *)0x40;

	_asm {
	mov	ax, cs
	mov	entry_cs, ax
	}
	u.s[0] = int10_rtn();
	u.s[1] = entry_cs;
	int10_save = *int10_vect;
	*int10_vect = u.l;
}


short
CallExec(char _far *load_addr)
{
	/*
	 * CallExec:  exec's a loaded driver file.
	 * Assumes "foo.BEF" has already been loaded.
	 * This routine bypasses the header, and exec's the binary image
	 * directly as a subroutine call.
	 *
	 * Return value is 0 if driver was not installed, otherwise number
	 * represents the highest device code that was assigned.
	 */
	extern short tempSP, tempSS;
	short savpos;
	short retcode;

#ifdef DEBUG
	if (MDXdebug)
	printf("CallExec(): About to execute loaded BEF driver\n");
#endif

	/*
	 * Next 3 lines put out a message on the bottom line of the
	 * screen to reassure the user if the driver takes a while.
	 */
	savpos = read_cursor(curpage);
	prtstr_attr(workmsg, sizeof(workmsg), curpage, STATUS_ROW, 0, WORK_ATTR);
	set_cursor(curpage, (short)(savpos >> 8), (short)(savpos & 0x0F));

	/*
	 * Set up to catch first BIOS INT 0x10 call.  The first call
	 * will erase the message written to the status line before
	 * proceeding with the INT 0x10.  Otherwise the reassuring
	 * message scrolls up the screen and is much less reassuring.
	 */
	int10_catch();

	_asm {
;       Execute from end of header looking like a long call
	pusha
	push	ds
	push	es
	mov	cs:tempSS, ss
	mov	cs:tempSP, sp
	push	cs
	lea	ax, comeback
	push	ax                      ;create our exit frame

	les	bx, DWORD PTR load_addr
	mov	ax, es
	add	ax, es:[bx+8]           ;convert address to equivalent
	shr	bx, 4                   ;that uses offset 0; i.e., put
	add	ax, bx                  ;everything into the segment
	xor	bx, bx
	push	ax
	push	bx                      ;create our entry point
	retf                          ;geronimo!
comeback:
	cli
	mov   ss, cs:tempSS            ;in case probe leaves a mess....
	mov   sp, cs:tempSP
	pop   es
	pop   ds
	sti
	mov   retcode, ax            ;preserve return code
	popa
	}

	/*
	 * The purpose of the following 2 lines is to generate an INT 0x10.
	 * This INT 0x10 guarantees that the status line message will be
	 * erased even if the driver did not do an INT 0x10.
	 */
	savpos = read_cursor(curpage);
	set_cursor(curpage, (short)(savpos >> 8), (short)(savpos & 0x0F));

#ifdef DEBUG
	if (MDXdebug)
	printf("CallExec: driver probe returned 0x%x\n", retcode);
	if (MDXdebug & 0x8000) {
		printf("Press any key to continue ...");
		wait_key();
	}
	putstr("\r\n");
#endif
	_asm  mov   ax, retcode
}

/*
 * Read a file from the diskette into memory and return the address of
 * the next free paragraph beyond it.
 */
char _far *
loadFile(char *filename, char _far *load_adr)
{
	long fd;
	long size;
	ushort paras;
	union {
		char _far *cfp;
		ushort us[2];
	} u;

	if ((fd = open(filename)) < 0L) {
#ifdef DEBUG_UFSBOOT
		printf("loadFile: error opening %s\n", filename);
#endif
		return (load_adr);
	}
	if ((size = filesize(fd)) == 0) {
#ifdef DEBUG_UFSBOOT
		printf("loadFile: %s is empty\n", filename);
#endif
		close(fd);
		return (load_adr);
	}
	if (readfile(fd, load_adr) != 0) {
#ifdef DEBUG_UFSBOOT
		printf("loadFile: error reading %s\n", filename);
#endif
		close(fd);
		return (load_adr);
	}
	close(fd);

	/* Adjust load address to minimize offset portion */
	u.cfp = load_adr;
	paras = u.us[0] >> 4;
	u.us[1] += paras;
	u.us[0] &= 0xF;

#ifdef DEBUG_UFSBOOT
	printf("loadFile: read %ld bytes at %x:%x from %s\n",
		size, u.us[1], u.us[0], filename);
#endif

	/* Round up file size to paragraphs */
	size += 0xF;
	size &= ~0xF;

	/* Calculate next free paragraph */
	paras = size >> 4;
	u.us[1] += paras;

#ifdef DEBUG_UFSBOOT
	printf("loadFile: updated buffer address is %x:%x\n", u.us[1], u.us[0]);
#endif

	return (u.cfp);
}

short
loadNrun(char *bef_file, char _far *load_adr)
{
	u_long nread;
	long fd;
	short savpos;

	/* load binary instructions. */
	/* strip header. */
	/* call the routine */

#ifdef DEBUG
	if (MDXdebug)
	printf("load and run: %s\n", bef_file);
#endif

	if ((fd = open(bef_file)) < 0L || readfile(fd, load_adr) != 0) {
		if (fd >= 0)
			close(fd);
		savpos = read_cursor(curpage);
		_asm {
		mov	di, offset DrvErr
		mov	si, DrvErrSiz
		call	displ_err
		}
		set_cursor(curpage, (short)(savpos >> 8),
		    (short)(savpos & 0x0F));
		return -1;
	}
	close(fd);

	if (*(u_short _far *)load_adr != BEF_SIG) {    /* verify EXE/BEF header */
		savpos = read_cursor(curpage);

#ifdef DEBUG
		if (MDXdebug)
		printf("BEF Signature check failed: found 0x%x\n",
		    *(u_short _far *)load_adr);
#endif
		_asm {
		mov	di, offset BefErr
		mov	si, BefErrSiz
		call	displ_err
		}
		set_cursor(curpage, (short)(savpos >> 8),
		    (short)(savpos & 0x0F));
		return -2;
	}
	return CallExec(load_adr);
}


short
build_menu(short lastdev)
{
	short i, j;
	char *pstr;

	menu_header(curpage);

	for (i = FIRST_BOOT_DEVNUM, j = MENU_STARTROW+3; i <= lastdev; i++, j++) {
		/* this simple loop currently only handles one of the MDB ranges, 10-7Fh.
		 * This is overkill for now; but will have to be enhanced if we encounter
		  * machines with zillions of MDB's!
		 */
		pstr = build_outstr(i);
		prtstr_attr(pstr, (short) strlen(pstr), curpage, j,
		    MENU_STARTCOL, MENU_COLOR);
	}
	for (i = 0; i < 4; i++) {
		char *driveStr = " ";
		extern char *ata_getname(int drive);

		if ((numata & (1 << i)) == 0)
			continue;
		memset((char *) &outstr[0], ' ', sizeof(outstr)); 
		*driveStr = 'C' + i;
		sprintf(outstr, "8%d   DISK   %s: %s", i, 
				driveStr, ata_getname(i));
		prtstr_attr(outstr, (short)strlen(outstr), curpage, j,
		    MENU_STARTCOL, MENU_COLOR);
		j++;
	}

	currow = j + 2;
	prtstr_attr(DevPrmpt, sizeof(DevPrmpt), curpage, currow, MENU_STARTCOL,
	    MENU_COLOR);
}

short
menu_header(register short curpage)
{
	clr_screen_attr(MENU_COLOR);
	prtstr_attr(Banner, sizeof(Banner), curpage, 0, 0, MENU_COLOR);
	prtstr_attr(Hdr1, sizeof(Hdr1), curpage, MENU_STARTROW-1,
	    MENU_STARTCOL + 2, MENU_COLOR);
	prtstr_attr(Hdr2, sizeof(Hdr2), curpage, MENU_STARTROW+1,
	    MENU_STARTCOL - 1, MENU_COLOR);
	prtstr_attr(Hdr3, sizeof(Hdr3), curpage, MENU_STARTROW+2,
	    MENU_STARTCOL - 1, MENU_COLOR);
}


char *
build_outstr(short devcode)
{
	DEV_INFO devinfo;
	DEV_INFO _far *dip;
	char *codestr;

	dip = get_id(devcode);
	if (dip == (DEV_INFO _far *) 0) {
		_asm {
		mov	di, offset IDErr
		mov	si, IDErrSiz
		call	displ_err
		jmp	$
		}
	}
	farbcopy((char _far *) dip, (char _far *) &devinfo, sizeof(DEV_INFO));

#ifdef DEBUG
	if (MDXdebug) {
	printf("\n\ndev_info dump for device 0x%x  ", devcode);
	printf("  size of dev_info structure: %d\n", sizeof(DEV_INFO));
	printf("  base_port: 0x%x  ", devinfo.base_port);
	printf("  bsize/irq_level: %d\n", devinfo.MDBdev.scsi.bsize);
	printf("  targ-lun/mem_base: 0x%x  ", devinfo.MDBdev.net.mem_base);
	printf("  pdt-dtq/mem_size: 0x%x\n", devinfo.MDBdev.net.mem_size);
	printf("  index: %d  ", devinfo.MDBdev.net.index);
	printf("  dev_type: 0x%x  ", devinfo.dev_type);
	printf("  bios_dev: 0x%x  ", devinfo.bios_dev);
	printf("  hba_id: %s\n", devinfo.hba_id);
	if (MDXdebug & 0x8000) {
		printf("Press any key to continue ...");
		wait_key();
	}
	printf("\r\n");
	}
#endif

	memset((char *) &outstr[0], ' ', sizeof(outstr)); /* (re-)initialize outstr */

	/*
	 *    Solaris/x86 Multiple Device Boot Menu
	 *  Code Device Vendor   Model/Desc       Rev
	 * ============================================
	 *  10   DISK   MICROPLS xxxxxxxxxxxxxxxx xxxx
	 */

	return sprintf(outstr, "%x%s%s", devcode, get_dev_type(&devinfo),
	    (char *) &devinfo);
}


char *
get_dev_type(DEV_INFO *pdev)
{
	if (pdev->dev_type == MDB_NET_CARD) return "   NET    ";

	switch(pdev->MDBdev.scsi.pdt) {
	case INQD_PDT_DA:      return "   DISK   ";

	case INQD_PDT_SEQ:     return "   TAPE   ";

	case INQD_PDT_ROM:     return "   CD     ";

	default:               return "   ???    ";
	}
}


DEV_INFO _far *
get_id(short devcode)
{
	DEV_INFO _far *dip;

	/* for each installed bootable devices, call INT13h, function F8h */
	_asm {
	push	es
	mov	dx, devcode
	mov	ah, 0F8h
	int	13h

	cmp	dx, 0BEF1h	;check for the magic cookie!
	jne	gi_fail		;can't use CF - BIOS usage is inconsistent

	mov	dx, es		;function F8h returns pointer to data structure
	mov	ax, bx		;in ES:BX.
	jmp	gi_exit
gi_fail:
	xor	ax, ax		;return null pointer if error occurs
	mov	dx, ax
gi_exit:
	pop	es
	mov	WORD PTR dip, ax
	mov	WORD PTR dip+2, dx
	}
	return dip;
}

/*
 * process_menu - retrieves user input for boot device selection; returns
 *                the device code that corresponds to the selected device.
 *                Menu selection allowed during timeout period; if period
 *                expires, we boot from defaultdev.
 */
process_menu(register short lastdev, short defaultdev)
{
	u_long cur_time, end_time, next_sec;
	short bootdev = defaultdev;
	short tctr, rc;
	short entrycol, erflg = 0;
	char *pdev;

	clear_line(currow);
	prtstr_attr(DevPrmpt, sizeof(DevPrmpt), curpage, currow, MENU_STARTCOL,
	    MENU_COLOR);
	entrycol = MENU_STARTCOL + sizeof(DevPrmpt);
	while (1) {                 /* boot menu processor */

		prtstr_attr(blnkL, sizeof(devbuf), curpage, currow, entrycol,
		    MENU_COLOR);
		set_cursor(curpage, currow, entrycol);
		flush_kb();

		cur_time = ask_time();
		end_time = cur_time + (u_long)(TicksPerSec * MDB_TIMEOUT);
		next_sec = cur_time + (u_long)TicksPerSec;
		cur_time = ask_time();

		/* user is expected to specify the bootable device number */
		for (tctr = MDB_TIMEOUT;
		    !(rc = nowait_read_key()) && tctr >= 0; ) {
			if ((cur_time = ask_time()) > next_sec) { /* update screen */
				prt_time(tctr--);
				next_sec = cur_time + (u_long)TicksPerSec;
				set_cursor(curpage, currow, entrycol);
			}
		}

		if (rc && rc != '\r') {
			/* user responded */
			if (erflg) {
				/* error message is still displayed */
				clear_status(8, MENU_COLOR);
				set_cursor(curpage, currow, entrycol);
				erflg = 0;
			}
			pdev = get_str((char) rc);
			if (strcmp(pdev, "viewparts") == 0 ) {
				view_parts++;
				continue;
			}
			bootdev = atox(pdev);

			/* allow extended devices' numbers *or* 0x80 or 0x81 */
			if (LEGAL_BOOTDEV(bootdev))
				break;

			/* other (non-numeric) responses result in error */
			erflg = 1;
			prtstr_attr(DevcodeErr, sizeof(DevcodeErr), curpage,
			    STATUS_ROW, 8, STATUS_ATTR);
			/* reset to default on error */
			bootdev = defaultdev;
		} else
			/*
			 * timeout or user hits Return
			 * boot from default Drive c:
			 */
			break;
	}
	clr_screen_attr(MENU_COLOR);
	prtstr_attr(Banner, sizeof(Banner), curpage, 0, 0, MENU_COLOR);
	return bootdev;
}


void
clear_line(register short row)
{
	prtstr_attr(blnkL, sizeof(blnkL), curpage, row, 0, MENU_COLOR);
}


void
clear_status(register short col, register short attr)
{
	prtstr_attr(blnkL, sizeof(blnkL), curpage, STATUS_ROW, col, attr);
}


void
prt_time(register u_short timeleft)
{
	char timestr[8];

	memset((char *) timestr, 0, sizeof(timestr));
	prtstr_attr(itoa(timeleft, timestr), 3, curpage, STATUS_ROW, 0,
	    MENU_COLOR);
}


char *
get_str(char first)
{
	char devbuf[20];
	char *pstr;
	register short newc;
	short bkspflg, firstcol;

	memset((char *) &devbuf[0], 0, sizeof(devbuf));
	pstr = devbuf;
	bkspflg = 0;

	read_cursor(curpage);

	_asm {
	mov	dl, ah                  ;initialize current row#
	xor	dh, dh
	mov	currow, dx
	xor	ah, ah                  ;initialize current column#
	mov	curcol, ax
	mov	firstcol, ax
	}

	for (newc = (short) first; newc != '\r'; newc = read_key()) {
		if (newc == '\b' && curcol > firstcol) {
			newc = ' ';
			set_cursor(curpage, currow, --curcol);
			*pstr-- = '\0';
			bkspflg = 1;
		} else if ((!isdigit(newc) && !isalpha(newc)) ||
		    (curcol - firstcol) >= sizeof(devbuf)) {
			/* exclude invalid characters */
			/* beep(); */
			continue;
		}
		_asm {                        ;echo character to screen.
		pusha
		mov	ax, newc
		mov	ah, 0ah
		mov	bx, curpage
		mov	bl, 0
		mov	cx, 1

		int	10h
		popa
		}

		if (!bkspflg) {
			*pstr++ = (char) newc;  /* append character to output string */
			set_cursor(curpage, currow, ++curcol);
		} else
			bkspflg = 0;
	}
	return devbuf;
}

/*
 * Load the sector at the specified address, (0000:7c00h) then continue
 * the boot process as if that sector contained the original primary
 * boot record, and had been invoked directly by INT 19h
 */
void
master_boot(void)
{
	char buffer[512];

#ifdef DEBUG
	if (MDXdebug)
	printf("loading boot record from device %x\n", BootDev);
#endif
	(u_long) MDBmast_boot = MBOOT_ADDR;

	if (read_disk(BootDev, 0, 0, 1, 1, (char _far *)buffer) != 0) {
		_asm {
		mov	di, offset LoadErr1
		mov	si, LoadErr1Siz
		call	displ_err
		}
		return;
	}

#ifdef DEBUG
	if (MDXdebug)
	printf("checking boot record signature\n");
#endif

	if (*(u_short *)(buffer + BOOT_SIGNATURE_START) != BOOT_SIG) {
		_asm {
		mov	di, offset BadRecErr
		mov	si, BadRecErrSiz
		call	displ_err
		}
		return;
	}
	farbcopy((char _far *) buffer, MDBmast_boot, SECTOR_SIZE);

	putstr("\r\n\n");
	pause_ms(1000);

	_asm mov dx, BootDev    ;required by PC boot convention

	_asm mov bx, 0xFACE 	;add signature for mboot to recognize
	_asm mov cx, 0xCAFE	; so it knows it can rely on BootDev above

	((void (_far *)())MDBmast_boot)();
}
