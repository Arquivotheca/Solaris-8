/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)reboot.c	1.22	99/03/18 SMI\n"


/*
 * REBOOT.C:
 *
 * routine to allow boot-time selection of alternate boot partition.
 * This routine may be compiled as a standalone utility, or as a
 * submodule of the primary boot subsystem.
 *
 * if more than one bootable partition is detected in the fdisk table,
 * a menu of bootable partitions is displayed.  If the user selects a
 * valid partition during the timeout period, we will reboot
 * from the selected partition.  If the user presses <ENTER>, or the
 * timeout period expires, we resume booting from the active partition.
 *
 */

#include <sys/types.h>
#include <sys/fdisk.h>
#include <ctype.h>
#include <bioserv.h>          /* BIOS interface support routines */
#include <bootdefs.h>         /* primary boot environment values */
#include "bootblk.h"

char far *select_priboot;

#ifdef DEBUG
    #pragma message ( __FILE__ ": << WARNING! DEBUG MODE >>" )
    #pragma comment ( user, __FILE__ ": DEBUG ON " __TIMESTAMP__ )
#endif

#pragma comment ( compiler )
#pragma comment ( user, "reboot.c	1.22	99/03/18" )


struct partentry *process_menu(short, short *, struct partentry *);
void clear_line();
void clear_status();
void prt_time();
void reset_active_part(short, struct partentry *);
char *get_str(char);
int validate_range(char *, short nboot, struct partentry *);

extern BootDbg;                 /* global debug output switch */
extern int reboot_timeout;	/* default menu timeout */

short curpage, active_part, errflg = 0;
char mstr[FD_NUMPART][LINELEN];
char blanks[] = { "                                                                             " };
char Banner[] = "SunOS - Intel Platform Edition                   Primary Boot Subsystem, vsn 2.0\n";
char Prmpt[]   = "Please select the partition you wish to boot: ";
char PartErr[] = "Invalid selection - please enter valid partition number.";
char ActvErr[] = "The partition you selected is already active.";
char BootPart[] = "Booting the selected fdisk partition.....\n";
char BootDEF[] = "Booting the default active partition.....\n";
char devbuf[20];

/* Table of FDISK system IDs and menu labels */
static struct { unsigned char id; char label[11]; } OS_table[] =
{
	{	DOSOS12,	"DOS12     "	},
	{	DOSOS16,	"DOS16     "	},
	{	DOSHUGE,	"BIGDOS    "	},
	{	DOSDATA,	"EXT_DOS   "	},
	{	EXTDOS,		"EXT_DOS   "	},
	{	PCIXOS,		"PCIX      "	},
	{	UNIXOS,		"UNIX      "	},
	{	SUNIXOS,	"SOLARIS   "	},
	{	DIAGPART,	"DIAGNOSTIC"	},
	{	IFS,		"IFS: NTFS "	},
	{	AIXBOOT,	"AIX Boot  "	},
	{	AIXDATA,	"AIX Data  "	},
	{	OS2BOOT,	"OS/2 Boot "	},
	{	WINDOWS,	"WINDOWS   "	},
	{	EXT_WIN,	"EXT_WIN   "	},
	{	FAT95,		"WIN FAT16 "	},
	{	EXTLBA,		"EXTLBA    "	},
	{	LINUX,		"LINUX     "	},
	{	CPM,		"CP/M      "	},
	{	NOVELL3,	"Novell 3.x"	},
	{	QNX4,		"QNX 4.x   "	},
	{	QNX42,		"QNX 4.x 2 "	},
	{	QNX43,		"QNX 4.x 3 "	},
	{	LINUXNAT,	"LINUX Nat "	},
	{	NTFSVOL1,	"NTFS Vol1 "	},
	{	NTFSVOL2,	"NTFS Vol2 "	},
	{	BSD,		"BSD System"	},
	{	NEXTSTEP,	"NeXTSTEP  "	},
	{	BSDIFS,		"BSDI FS   "	},
	{	BSDISWAP,	"BSDI SWAP "	},
	{	X86BOOT,	"X86 BOOT  "	},
	{	0,		"<unused>  "	}
};

void
reboot(struct partentry *pte, unsigned char BootDev)
{
	register short nboot;
	register daddr_t relsect;

	struct partentry *old_pte;
	short partnum = -1;

	/*
	 * get the windex!
	 *
	 */
	(long)select_priboot = MBOOT_ADDR;
	clr_screen_attr(MENU_COLOR);
	set_cursor((curpage = ask_page()), 0, 0);
	putstr(Banner);

	old_pte = pte;
	if (count_parts(pte) == 1)
		return;

	/*
	 * step 2: generate a menu that consists of a one-line entry for
	 *         each bootable partition within the fdisk table.
	 */
	nboot = build_menu(pte);

	/*
	 * step 3: display menu, wait for user input
	 */
	if ((pte = process_menu(nboot, &partnum, pte)) == 0) {
		clr_screen_attr(MENU_COLOR);
		prtstr_attr(Banner, sizeof (Banner), curpage, 0, 0,
			MENU_COLOR);
		prtstr_attr(BootDEF, sizeof (BootDEF), curpage, 2, 0,
			MENU_COLOR);
		return;
	}

	/*
	 * step 4: reboot from selected partition.
	 */
	reset_active_part(partnum - 1, old_pte);
#ifdef DEBUG
	if (BootDbg) {
		printf("\nSelected partition: %d\trelsect: %ld\n",
			partnum, pte->relsect);
#ifdef PAUSE
		putstr("Press any key to continue.....");
		wait_key();
#endif
		putstr("\r\n");
	}
#endif

	/* Load boot device and execute bootstrap */
	_asm {
		mov	dl, BootDev
		call	DWORD PTR select_priboot
	}
}

/*
 * count_parts - pre-screening step; count the number of actual partitions
 *               used within the fdisk table.
 *               Returns number of bootable partitions.
 */
int
count_parts(struct partentry *pte)
{
	register short idx, nboot;

	for (idx = nboot = 0; idx < FD_NUMPART; idx++, pte++) {

	if (pte->systid != 0)    /* count number of bootable partitions */
		++nboot;
	}
	Dprintf(DBG_FDISK, ("Number of bootable partitions: %d\n", nboot));
	Dpause(DBG_FDISK, 0);
	return ( nboot );
}


/*
 * build_menu - generates menu strings based on information encoded in
 *              fdisk table.  Returns number of bootable partitions.
 *
 */
int
build_menu(struct partentry *pte)
{
	register short idx, nboot;
	int i;
	char *p;
	char numbuf[16];

	set_cursor ( curpage, MENU_STARTROW - 1, 24 );
	putstr ( "Current Disk Partition Information" );
	set_cursor ( curpage, MENU_STARTROW + 1, MENU_STARTCOL - 2 );
	putstr ( "Part#   Status    Type      Start       Length" );
	set_cursor ( curpage, MENU_STARTROW + 2, MENU_STARTCOL - 3 );
	putstr ( "================================================" );

	for ( idx = nboot = 0; idx < FD_NUMPART; idx++, pte++ ) {

		itoa(idx + 1, mstr[idx] );
		strcat(mstr[idx], "     ");

		if ( pte->bootid == ACTIVE ) {
			strcat(mstr[idx], "Active   ");
			active_part = idx + 1;
		} else
			strcat(mstr[idx], "         ");

		/* Look up the OS id to get a descriptive string */
		p = "?         ";
		for (i = sizeof (OS_table) / sizeof (OS_table[0]) - 1;
				i >= 0; i--) {
			if (OS_table[i].id == pte->systid) {
				p = OS_table[i].label;
				break;
			}
		}
		strcat(mstr[idx], p);

		if (pte->systid != 0)    /* count bootable partitions */
			++nboot;

		if (pte->relsect) {
			sprintf(numbuf, "%7ld", pte->relsect);
			strcat(mstr[idx], numbuf);
			strncat(mstr[idx], blanks,
				NUMLEN - (short)strlen(numbuf));
			sprintf(numbuf, "%7ld", pte->numsect);
			strcat(mstr[idx], numbuf);
		}

		prtstr_attr(mstr[idx], (short)strlen(mstr[idx] ), 0,
			MENU_STARTROW + idx + 3, MENU_STARTCOL, MENU_COLOR);
	}
	Dprintf(DBG_FDISK, ("\nNumber of bootable partitions: %d\n", nboot));
	return (nboot);
}


/*
 * process_menu - retrieves user input for partition selection; returns
 *                the starting sector number of the selected partition.
 *                Menu selection allowed during timeout period; if period
 *                expires, we boot from the default active partition.
 */
struct partentry *
process_menu(short nboot, short *partnum, struct partentry *pte)
{
	short entrycol, tctr, rc;
	unsigned long cur_time, next_sec;
	char *ppart;

	clear_line(PROMPTROW);
	prtstr_attr(Prmpt, sizeof (Prmpt), curpage, PROMPTROW, PROMPTCOL,
		MENU_COLOR);
	entrycol = PROMPTCOL + sizeof (Prmpt);

	while (1) {	/* let user select a boot partition */

		prtstr_attr(blanks, 5, curpage, PROMPTROW, entrycol,
			MENU_COLOR);
		set_cursor(curpage, PROMPTROW, entrycol);
		flush_kb();

		cur_time = ask_time();
		next_sec = cur_time + TicksPerSec;
		cur_time = ask_time();

		/* user is expected to specify the partition # (1-nboot) */
		for (tctr = reboot_timeout;
				!(rc = nowait_read_key()) && tctr >= 0; ) {
			if ((cur_time = ask_time()) > next_sec) {
				prt_time(tctr--);
				next_sec = cur_time + TicksPerSec;
				set_cursor(curpage, PROMPTROW, entrycol);
			}
		}

		if ( rc ) {	/* user responded */
			if ( errflg ) {    /* error message is still up */
				clear_status(10, MENU_COLOR);
				set_cursor(curpage, PROMPTROW, entrycol);
				errflg = 0;
			}
			if (rc == '\r') {
				/* user hits return, boot default active part */
				return (0);
			}

			ppart = get_str((char)rc);
			if ((*partnum = validate_range(ppart,
					nboot, pte)) == -1) {
				errflg = 1;
				prtstr_attr(PartErr, sizeof(PartErr), curpage,
					STATUS_ROW, 10, STATUS_ATTR);
				continue;
			}
			if (*partnum == ESCAPE)
				return (0);
			/*
			 * Note that typing the number for the active
			 * partition is treated the same as timing out
			 * or hitting <ENTER>.  The intent is clearly
			 * to avoid a loop.  But note that the behavior
			 * is incorrect if we were booted by a similar
			 * agent from another partition.  We need to
			 * make sure we detect that case before we
			 * get here.
			 */
			if (*partnum == active_part) {
				prtstr_attr(ActvErr, sizeof (ActvErr),
					curpage, STATUS_ROW, 15, STATUS_ATTR);
				return (0);
			}
			clr_screen_attr(MENU_COLOR);
			prtstr_attr(Banner, sizeof (Banner), curpage, 0, 0,
				MENU_COLOR);

			prtstr_attr(BootPart, sizeof (BootPart),
				curpage, 2, 0, MENU_COLOR );
			pte += (*partnum - 1);
			return (pte);
		}
		/* timeout period expired */
		return (0);
	} /* end of while loop */
}


void
prt_time(unsigned short timeleft)
{
	char timestr[8];

	memset(timestr, 0, sizeof (timestr));
	prtstr_attr(itoa(timeleft, timestr), 3, curpage,
		23, 0, MENU_COLOR );
}


/*
 * get_str - primitive keyboard handler - filters invalid characters,
 *           echoes valid characters to screen.
 *           Returns pointer to input character string.
 */
char *
get_str(char first)
{
	char *pstr;
	register short newc, currow;
	short curcol, bkflg;

	memset(devbuf, 0, sizeof (devbuf));
	pstr = devbuf;
	bkflg = 0;

	read_cursor(curpage);
	_asm {
		mov	dl, ah;		initialize current row#
		xor	dh, dh
		mov	currow, dx
		xor	ah, ah;		initialize current column#
		mov	curcol, ax
	}

	for ( newc = (short)first; ; bkflg = 0, newc = read_key()) {

		if (newc == '\r')	/* check for end of string */
			break;
		if ( newc == ESCAPE ) {
			devbuf[0] = ESCAPE;	/* toss old input */
			break;
		}
		if ( newc == '\b' ) {
			newc = ' ';
			set_cursor(curpage, currow, --curcol);
			*pstr-- = 0;
			bkflg = 1;
		}
		else if (!isdigit(newc)) { /* exclude invalid characters */
			continue;
		}
		/* echo character to screen */
		_asm {
			pusha
			mov	ax, newc
			mov	ah, 0ah
			mov	bh, byte ptr curpage
			mov	cx, 1

			int 10h
			popa
		}

		if (!bkflg) {
			*pstr++ = (char) newc;  /* append to output string */
			set_cursor(curpage, currow, ++curcol);
		}
	}
	return (devbuf);
}


/*
 * validate_range - takes input character string (number of desired boot
 *                  partition); checks that the input partition number
 *                  is within range ( 0 < x < number of bootable partitions )
 */
int
validate_range(char *pstr, register short nboot,
	struct partentry *pte)
{
	short partnum;

	if (errflg) {
		clear_line(20);
		errflg = 0;
	}

	if (*pstr == ESCAPE)
		return (ESCAPE);
	partnum = atoi(pstr);
	if (partnum < 1 || partnum > FD_NUMPART)
		return (-1);
	if (pte[partnum-1].systid == 0)
		return (-1);
	return partnum;
}


/*
 * reset_active_part - based on user's input, reset selected partition
 *                     to be "Active" in in-core master boot record image.
 *
 */
void
reset_active_part(short boot_part, struct partentry *pte)
{
	short i;
	struct partentry *old_pte;

	for (i = 0, old_pte = pte; i < FD_NUMPART; pte++, i++) {
		if (i == boot_part) {
			pte->bootid = ACTIVE;
			Dprintf(DBG_FDISK, ("Partition %d will be booted "
				"at next reboot.\n", i + 1));
		} else {
			pte->bootid = NOTACTIVE;
			Dprintf(DBG_FDISK, ("Partition %d was deactivated.\n",
				i + 1));
		}
	}
	farbcopy((char _FAR_ *)old_pte - BOOTSZ, select_priboot, SECTOR_SIZE);
}


void
clear_line ( register short linenum )
{
   prtstr_attr (blanks, sizeof(blanks), curpage, linenum, 0,
		 MENU_COLOR );
   set_cursor ( curpage, linenum, 0 );
   putstr ( blanks );
}


void
clear_status ( register short col, register short attr )
{
   prtstr_attr (blanks, sizeof(blanks), curpage, STATUS_ROW,
		  col, attr );
}
