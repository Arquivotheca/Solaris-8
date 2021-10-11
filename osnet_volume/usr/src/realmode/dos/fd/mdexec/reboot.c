/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)reboot.c	1.7	94/05/23 SMI\n"

/*
 * REBOOT.C:
 *
 * routine to allow boot-time selection of alternate boot partition.
 *
 * if more than one bootable partition is detected in the fdisk table,
 * a menu of bootable partitions is displayed.  If the user selects a
 * valid partition during the 30-second timeout period, we will reboot
 * from the selected partition.  If the user presses <ENTER>, or the 
 * timeout period expires, we resume booting from the active partition.
 *
 */

#include <sys/types.h>      
#include <sys/fdisk.h>
#include <sys/vtoc.h>
#include <ctype.h>
#include <bioserv.h>          /* BIOS interface support routines */
#include <bootdefs.h>         /* primary boot environment values */
#include <dev_info.h>	/* MDB extended device information */
#include <bootp2s.h>

char _far *select_priboot;      /* hope this fools MSC! */

#ifdef DEBUG
#pragma message (__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment (user, __FILE__ ": DEBUG ON " __TIMESTAMP__)
#define PAUSE
#endif

#pragma comment (compiler)
#pragma comment (user, "reboot.c	1.7	94/05/23")

extern short BootDev;
extern short MDXdebug;				/* global debug output switch */
extern int view_parts;
extern char Banner[];
extern short BannerSiz;

short curpage, errflg = 0;
char mstr[FD_NUMPART][LINELEN];
char blanks[] = {"                                                                             " };
char ReadErr[] = "Cannot load selected partition boot record.";
char SigErr[]  = "Invalid partition boot record.";
short ReadErrSiz = sizeof(ReadErr);
short SigErrSiz  = sizeof(SigErr);
char Prmpt[] = "Please select the partition you wish to boot: ";
char PartErr[] = "Invalid selection - please enter valid partition number.";
char NoActErr[] = "No partition is marked active for default boot.";
char BootPart[] = "Booting the selected fdisk partition ...\r\n";
char BootDEF[] = "Booting the active partition ...\r\n";
static char devbuf[20] = {0};


/*
 * count_parts - pre-screening step; count the number of actual partitions
 *               used within the fdisk table. 
 *               Returns number of bootable partitions.
 */
count_parts(struct partentry *pte, short *sol_part)
{
	register short idx;
	short nboot;

	*sol_part = nboot = 0;
	for (idx = 1; idx <= FD_NUMPART; idx++, pte++) {
		if (pte->systid != 0) {
			/* count number of bootable partitions */
			++nboot;
			if (pte->systid == SUNIXOS && !*sol_part)
				/* indicate existence of Solaris partition */
				*sol_part = idx;
		}
	}
#ifdef DEBUG
	if (MDXdebug) {
	printf("\nNumber of bootable partitions = %d\n", nboot);
	}
#endif
	return nboot;
}


/*
 * build_rb_menu - generates menu strings based on information encoded in
 *              fdisk table.  Returns number of bootable partitions.
 *
 */
static
build_rb_menu(struct partentry *pte, short *active_part)
{
	register short idx, nboot;
	char numbuf[16];

	set_cursor(curpage, MENU_STARTROW - 1, 24);
	putstr("Current Disk Partition Information");
	set_cursor(curpage, MENU_STARTROW + 1, MENU_STARTCOL - 2);
	putstr("Part#   Status     Type      Start      Length");
	set_cursor(curpage, MENU_STARTROW + 2, MENU_STARTCOL - 3);
	putstr("================================================");

	*active_part = 0;
	for (idx = nboot = 0; idx < FD_NUMPART; idx++, pte++) {

		itoa(idx + 1, mstr[idx]);
		strcat(mstr[idx], "     ");

		if (pte->bootid == ACTIVE) {
			strcat(mstr[idx], "active  ");
			*active_part = idx + 1;
		} else
			strcat(mstr[idx], "        ");

		switch (pte->systid) {
		case DOSOS12:  strcat(mstr[idx], "  DOS-12   "); break;
		case DOSOS16:  strcat(mstr[idx], "  DOS-16   "); break;
		case DOSHUGE:  strcat(mstr[idx], "  BigDOS   "); break;
		case DOSDATA:  
		case EXTDOS:   strcat(mstr[idx], "  Ext_DOS  "); break;            
                      

		case PCIXOS:   strcat(mstr[idx], "  PCIX     "); break;
		case UNIXOS:   strcat(mstr[idx], "  unix     "); break;
		case SUNIXOS:  strcat(mstr[idx], "  SOLARIS  "); break;
		case 0:        strcat(mstr[idx], " <unused>  "); break;
		default:       strcat(mstr[idx], "    ?      "); break;
		}

		if (pte->systid != 0)    /* count number of bootable partitions */
			++nboot;

		if (pte->relsect) {
			sprintf(numbuf, "%7ld", pte->relsect);
			strcat(mstr[idx], numbuf);
			strncat(mstr[idx], blanks,
			    NUMLEN - (short)strlen(numbuf));
			sprintf(numbuf, "%7ld", pte->numsect);
			strcat(mstr[idx], numbuf);
		}

		prtstr_attr(mstr[idx], (short)strlen(mstr[idx]), 0,
		    MENU_STARTROW + idx + 3, MENU_STARTCOL, MENU_COLOR);
	}
	return nboot;
}

static void
clear_rbline(register short linenum)
{
	prtstr_attr(blanks, sizeof(blanks), curpage, linenum, 0, MENU_COLOR);
	set_cursor(curpage, linenum, 0);
	putstr(blanks);
}

static void
clear_rbstatus(register short col, register short attr)
{
	prtstr_attr(blanks, sizeof(blanks), curpage, STATUS_ROW, col, attr);
}

/*
 * validate_range - takes input character string (number of desired boot
 *                  partition); checks that the input partition number
 *                  is within range (0 < x < number of bootable partitions)
 */
validate_range(char *pstr, register short nboot)
{
	short partnum;

	if (errflg) {
		clear_rbline(20);
		errflg = 0;
	}
   
	if ((partnum = atoi(pstr)) < 1 || partnum > nboot)
		return 0;
	else
		return partnum;
}

static void
prt_rbtime(register u_short timeleft)
{
	char timestr[8];

	memset((char *) timestr, 0, sizeof(timestr));
	prtstr_attr(itoa(timeleft, timestr), 3, curpage, 23, 0, MENU_COLOR);
}

/*
 * get_rbstr - primitive keyboard handler - filters invalid characters,
 *           echoes valid characters to screen.
 *           Returns pointer to input character string.
 */
static char *
get_rbstr(char first)
{
	char *pstr;
	register short newc;
	short bkspflg;
	short currow, curcol, firstcol;

	bkspflg = 0;
	memset((char *) devbuf, 0, sizeof(devbuf));
	pstr = devbuf;

	read_cursor(curpage);
	_asm {
        mov	dl, ah                  ;initialize current row#
        xor	dh, dh
        mov	currow, dx
        xor	ah, ah                  ;initialize current column#
        mov	curcol, ax
        mov	firstcol, ax
 
;        mov	ax, first
;        mov	ah, 0ah
;        mov	bh, byte ptr curpage
;        mov	cx, 1

;        int	10h
	}
/*   set_cursor(curpage, currow, ++curcol); */
	for (newc = (short)first; newc != '\r'; newc = read_key()) {

		if (newc == ESCAPE) { 
			devbuf[0] = ESCAPE;        /* invalidate any existing input */
			break;
		} else if (newc == '\b' && curcol > firstcol) {
			newc = ' ';
			set_cursor(curpage, currow, --curcol);
			*pstr-- = '\0';
			bkspflg = 1;
		} else if (!isdigit(newc) ||
		    (curcol - firstcol) >= sizeof(devbuf)) {
			/* exclude invalid characters */
			/* beep(); */
			continue;
		}
		_asm {                        ;echo character to screen.
		pusha
		mov	ax, newc
		mov	ah, 0ah
		mov	bh, byte ptr curpage
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
	return  devbuf;
}
/*
 * process_rb_menu - retrieves user input for partition selection; returns
 *                the starting sector number of the selected partition.
 *                Menu selection allowed during timeout period; if period
 *                expires, we boot from the default active partition.
 */
static struct partentry *
process_rb_menu(struct partentry *fdisk_tbl, short *active_part)
{
	u_long cur_time, end_time, next_sec;
	register short entrycol, tctr, rc;
	short partnum = *active_part;
	char *ppart;

	clear_rbline(PROMPTROW);
	prtstr_attr(Prmpt, sizeof(Prmpt), curpage, PROMPTROW, PROMPTCOL,
	    MENU_COLOR);
	entrycol = PROMPTCOL + sizeof(Prmpt);

	while (1) {                 /* let user select a boot partition */

		prtstr_attr(blanks, sizeof(devbuf), curpage, PROMPTROW,
		    entrycol, MENU_COLOR);
		set_cursor(curpage, PROMPTROW, entrycol);
		flush_kb();

		cur_time = ask_time();
		end_time = cur_time + (u_long)(TicksPerSec * REBOOT_TIMEOUT);
		next_sec = cur_time + (u_long)TicksPerSec;
		cur_time = ask_time();

		/* user is expected to specify the partition number (1:4) */
		for (tctr = REBOOT_TIMEOUT;
		    !(rc = nowait_read_key()) && tctr > 0; ) {
			if ((cur_time = ask_time()) > next_sec) { /* update screen */
				prt_rbtime(tctr--);
				next_sec = cur_time + (u_long)TicksPerSec;
				set_cursor(curpage, PROMPTROW, entrycol);
			}
		}
		if (errflg) {
			/* error message is still displayed */
			clear_rbstatus(10, MENU_COLOR);
			set_cursor(curpage, PROMPTROW, entrycol);
			errflg = 0;
		}
		if (rc && rc != '\r') {
			/* user responded */
			ppart = get_rbstr((char) rc);

			if (*ppart == ESCAPE) {
				*active_part = partnum = 0;
				break;
			}
			if ((partnum = validate_range(ppart, FD_NUMPART)) &&
			    (fdisk_tbl + (partnum - 1))->systid != 0 &&
    			    (fdisk_tbl + (partnum - 1))->systid != EXTDOS)
				break;

			errflg = 1;
			prtstr_attr(PartErr, sizeof(PartErr), curpage,
			    STATUS_ROW, 10, STATUS_ATTR);
			partnum = *active_part;
		} else {
			if (*active_part == 0) {
				errflg = 1;
				prtstr_attr(NoActErr, sizeof(NoActErr), 
				    curpage, STATUS_ROW, 10, STATUS_ATTR);
			} else
				/*
				 * timeout period expired or user hits Return,
				 * so boot the active partition
				 */
				break;
		}
	}
	if (*active_part || partnum) {
		clr_screen_attr(7);  /* prepare screen for next phase */
		prtstr_attr(Banner, BannerSiz, curpage, 0, 0, 7);
		if (partnum == *active_part)
			prtstr_attr(BootDEF, sizeof(BootDEF), curpage, 2, 0, 7);
		else {
			prtstr_attr(BootPart, sizeof(BootPart), curpage, 2, 0, 7);
			*active_part = partnum;
		}
		/* return pointer to selected fdisk partition */
		return fdisk_tbl + (*active_part - 1);
	} else
		return 0;
}

/*
 * reset_active_part - based on user's input, reset selected partition
 *                     to be "Active" in in-core master boot record image.
 *
 */
void
reset_active_part(struct partentry *fdisk_tbl, register short boot_part)
{
	register short i;
	struct partentry *fdiskp = fdisk_tbl;

	for (i = 1; i <= FD_NUMPART; fdiskp++, i++) {
		if (i == boot_part) {
			fdiskp->bootid = ACTIVE;
#ifdef DEBUG
			if (MDXdebug)
			printf("Partition %d will be booted at next reboot.\n",
			    i);
#endif
		} else {
			fdiskp->bootid = NOTACTIVE;
#ifdef DEBUG
			if (MDXdebug)
			printf("Partition %d was deactivated.\n", i);
#endif
		}
	}
	farbcopy((char _far *) fdisk_tbl,
	    (char _far *) select_priboot + FDISK_START,
	    SECTOR_SIZE - FDISK_START);
}


int
reboot(struct partentry *fdisk_tbl, struct pri_to_secboot *realp)
{
	struct partentry *pte = fdisk_tbl;
	register short nboot;
	register daddr_t relsect;
	short partnum;
	short solaris_part;

	/*
	* get the windex!
	*
	*/
	(long) select_priboot = MBOOT_ADDR;

	if ((partnum = count_parts(fdisk_tbl, &solaris_part)) <= 1 &&
	    !view_parts) {
		/*
		 * bypass the whole menu thing: there is only one choice
		 */
		clr_screen_attr(7);  /* prepare screen for next phase */
		prtstr_attr(Banner, BannerSiz, curpage, 0, 0, 7);
		prtstr_attr(BootDEF, sizeof(BootDEF), curpage, 2, 0, 7);

		pte =  fdisk_tbl + (solaris_part - 1);
	} else {
		if (BootDev == 0x80 || BootDev == 0x81 || BootDev == -1)
			clr_screen_attr(MENU_COLOR);
		set_cursor((curpage = ask_page()), 0, 0);
		putstr(Banner);

		/*
		 * step 1: generate a menu that consists of a one-line entry for
		 *         each bootable partition within the fdisk table. 
		 */
		build_rb_menu(fdisk_tbl, &partnum);

		/*
		 * step 2: display menu, wait for user input
		 */
		pte = process_rb_menu(fdisk_tbl, &partnum);
		solaris_part = partnum;

		if (partnum) {
			/*
			 * step 3: fixup fdisk partition table
			 */
			reset_active_part(fdisk_tbl, partnum);
#ifdef DEBUG
			if (MDXdebug) {
				printf("Selected partition: %d   relsect: %ld\n",
				    partnum, pte->relsect);
				if (MDXdebug & 0x8000) {
					putstr("Press any key to continue ...");
					wait_key();
				}
				putstr("\r\n");
			}
#endif
		}
	}

	putstr("\r\n");
	if (partnum && pte->systid != SUNIXOS) {
		/*
		 * step 4: reboot from selected partition.
		 */
		_asm mov	dx, word ptr BootDev
		_asm call	DWORD PTR select_priboot

		/* not reached */
	}

	if (solaris_part) {
		/*
		 * instead of loading the partition boot from the Boot device
		 * just gather info on the Solaris partition and
		 * prepare to read the copy of ufsboot off the floppy
		 */
		realp->bootfrom.ufs.Sol_start = pte->relsect;
		realp->bootfrom.ufs.Sol_size = pte->numsect;
		realp->bootfrom.ufs.boot_part = solaris_part;
#ifdef DEBUG
		if (MDXdebug) {
		printf("SOLARIS partition start: 0x%lx", pte->relsect);
		printf("  length: 0x%lx\n", pte->numsect);
		printf("  partition booted: %d\n", realp->bootfrom.ufs.boot_part);
		if (MDXdebug & 0x8000) {
			putstr("Press any key to continue ...");
			wait_key();
		}
		putstr("\r\n");
		}
#endif
	}
	return solaris_part;
}
