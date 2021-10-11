/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)bootblk.c	1.12	95/09/13 SMI\n"

/*
 * 3rd-party boot BOOT BLOCK:
 *
 * PURPOSE: to load the SECONDARY BOOT from the Solaris MDB diskette.
 *
 * Part of SOLARIS Multi Device Boot Executive.
 * This piece will have to be able to "grok" a MS-DOS filesystem.
 * It needs to take a designated file name, locate the file within the
 *    MS-DOS filesystem, translate this filesystem location(s) into absolute
 *    disk sectors, then use ROM BIOS INT 13 calls to load the file.
 * After this, we jump to newly loaded piece of code and proceed to
 *    execute the SECONDARY BOOT.
 *
 */

#ifdef DEBUG
#pragma message(__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)
#define PAUSE
#endif

#pragma comment(compiler)
#pragma comment(user, "bootblk.c	95/09/13	1.12")

#ifdef FARDATA
#define _FAR_ _far
#else
#define _FAR_
#endif

#include <sys\types.h>
#include <sys\param.h>
#include <sys\fdisk.h>
#include <sys\vnode.h>
#include <sys\vtoc.h>
#include <sys\dkio.h>
#include <sys\dklabel.h>
#include <bioserv.h>	/* BIOS interface support routines */
#include <bootdefs.h>	/* primary boot environment values */
#include <dev_info.h>	/* MDB extended device information */
#include <bootp2s.h>	/* primary/secondary boot interface */

extern void displ_err();
extern DEV_INFO _far *get_id(short);
extern int reboot(struct partentry *, struct pri_to_secboot *);

char _far *mov_ptr(char _far *, long);
struct partentry *read_fdisk(void);
void read_vtoc(void);
void init_p2s_parms(struct pri_to_secboot *);
void get_dev_info(struct pri_to_secboot *);
void get_ctlr(char _far *, char *);
void register_prep();


typedef union {
	struct {
		u_short offp;
		u_short segp;
	} s;
	char _far *p;
	u_long l;
} seg_ptr;

extern MDXdebug;		/* global debug output switch */
extern short BootDev;
extern short fBootDev;

short cyl, nCyl, secPerCyl;
u_char secPerTrk, sector, head, trkPerCyl;
long relsect;

struct pri_to_secboot *realp;

seg_ptr	secboot;      /* hope this fools MSC! */
/* char _far *secboot;      / * hope this fools MSC! */

struct pri_to_secboot p2s;

struct dk_label boot_vtoc;
struct mboot boot_sector;

char GeomErr[] = "Cannot read device geometry.";
char LoadErr[] = "Cannot load SOLARIS secondary boot.";
char InfoErr[] = "Cannot read extended device data.";
char FdskErr[] = "Cannot read fdisk table.";
char OpenErr[] = "Cannot find /ufsboot.";
char VtocErr[] = "Cannot read SOLARIS disk label.";

char *ctlr_str[] = {
	"AHA154X", 
	"AHA174X",
	"DPT",
	""
	""
};

char *ctlr_id[]  = {
	"aha\0\0\0\0",
	"eha\0\0\0\0",
	"dpt\0\0\0\0",
	"ata\0\0\0\0"
};

short GeomErrSiz = sizeof(GeomErr);
short LoadErrSiz = sizeof(LoadErr);
short InfoErrSiz = sizeof(InfoErr);
short FdskErrSiz = sizeof(FdskErr);
short OpenErrSiz = sizeof(OpenErr);
short VtocErrSiz = sizeof(VtocErr);


boot_blk()
{
	register long fsz;	/* make these extern's for debugging purposes */
	register char *dest;
	union key_t key;
	union fdrc_t devrc;
	long fd;

	key.k.scancode = key.k.keycode = '\0';

	realp = &p2s;
	memset((char *)&p2s, 0, sizeof(p2s));

	/*
	 * display harddisk fdisk table, allow the user to select boot partition.
	 */
	if (!reboot(read_fdisk(), realp))
		return 0;

	relsect = realp->bootfrom.ufs.Sol_start + 1;	/* compute vtoc address */
	read_vtoc();		   /* vtoc values are relative, */
				   /* must be adjusted to abs */
	init_p2s_parms(realp);

	get_dev_info(realp);

#ifdef DEBUG
	if (MDXdebug) {
	printf("dev_info dump for device 0x%x\n", BootDev);
	printf("  size of dev_info structure: %d\n", sizeof(DEV_INFO));
	printf("  base_port: 0x%x\n", realp->F8.base_port);
	printf("  bsize/irq_level: %d\n", realp->F8.MDBdev.scsi.bsize);
	printf("  targ-lun/mem_base: 0x%x\n", realp->F8.MDBdev.net.mem_base);
	printf("  pdt-dtq/mem_size: 0x%x\n", realp->F8.MDBdev.net.mem_size);
	printf("  index: %d\n", realp->F8.MDBdev.net.index);
	printf("  dev_type: 0x%x\n", realp->F8.dev_type);
	printf("  bios_dev: 0x%x\n", realp->F8.bios_dev);
	printf("  hba_id: %s\n", realp->F8.hba_id);
	set_cursor(ask_page(), 23, 0);

	printf("\nboot interface info initialized\n");
	}
#endif

	/*
	 * find the secondary boot in the DOS diskette filesystem.
	 */
	if ((fd = open("UFSBOOT    ")) < 0) {
#ifdef DEBUG
		printf("open \"ufsboot\" file failed\n");
#endif
		_asm {
		mov	di, offset OpenErr	  ;"Cannot find /ufsboot."
		mov	si, OpenErrSiz
		call	displ_err
		jmp	$
		}
	}

	/*
	 * read the secondary-boot program from diskette.
	 */
	secboot.l = DEST_ADDR;
	if (readfile(fd, secboot.p)) {
#ifdef DEBUG
		printf("reading \"ufsboot\" file failed\n");
#endif
		_asm {
		mov	di, offset LoadErr	  ; "Cannot load /ufsboot."
		mov	si, LoadErrSiz
		call	displ_err
		jmp	$
		}
	}

#ifdef DEBUG
	if (MDXdebug) {
	printf("register initialized; invoking /ufsboot at %04x:%04x\n",
	    secboot.s.segp, secboot.s.offp);
	if (MDXdebug & 0x8000) {
		putstr("Press any key to continue ...");
		wait_key();
	}
	putstr("\r\n");
	}
#endif

	putstr ( "\r\n\n" );

	register_prep();

/*   testpt("g");*/

	_asm {
	mov	dx, ds
	mov	es, dx
	mov	di, word ptr realp     ;primary/secondary interface structure
	mov	dl, byte ptr BootDev
	mov	dh, byte ptr fBootDev
	}

	((void (_far *)())secboot.l)(); 
	/* lastmod(); */
}


char _far *	    /* returns adjusted segment:offset pointer */
mov_ptr(char _far * p, long len)
{
	seg_ptr old, new;

	old.p = p;
	new.l = 0L;
	new.s.offp = old.s.offp;   /* cannot combine - cwd sign extends! */
	new.l += len;

	if (new.s.segp)		/* crossing a 64K boundary */
		new.s.segp <<= 12;

	new.s.segp += old.s.segp;

	return new.p;
}


/*
 * We need to provide information about our hardware environment to the
 * secondary boot.  (Used for system autoconfiguration, and construction
 * of the devinfo tree.)
 *
 * There are several available sources of information.  One is the vtoc,
 * and the INT 13h, function 08h call.  Another source for extended boot
 * devices, is the new INT 13h, function F8 call.  This call is supported
 * for devices other than 80h and 81h.
 *
 */
void
read_vtoc(void)
{
	_asm {
	;read_vtoc PROC NEAR */
	;relsect contains relative sector number marking start of bootblk
	;calculate cylinder# (even more fornow - use cx:bx)

	;compute vtoc location, put sector number into dx:ax
	pusha

	;load disk sector address of vtoc(disk label)
	mov	ax, WORD PTR relsect
	mov	dx, WORD PTR relsect + 2

	;calculate cylinder#
	div	secPerCyl
	mov	cyl, ax

	;calculate head#
	mov	ax, dx	  ;dx contains remainder from last division
	div	secPerTrk
	mov	head, al

	;calculate sector#
	inc	ah	      ;convert logical -> physical sector #
	mov	sector, ah      ;ah contains remainder from last division

	;reconstruct BIOS geometry
	mov	cx, cyl	 ;ah contains sector, cl contains cylinder
	cmp	cx, 0FFh	;top two bits go into cylinder value
	jbe	do_read

	;recalc_cyl:
	mov	bh, ch
	shr	bx, 2	   ;0's shifted in from left
	and	bx, 0C0h

	;merge top two bits into cylinder word
	or	bl, ah
	mov	word ptr sector, bx

do_read:
	}		/* end of _asm section */

	if (read_disk(BootDev, cyl, head, sector, 1,
	    (char _far *) &boot_vtoc) != 0) {
		_asm {			; printf("Cannot read SOLARIS disk label.");
		mov	di, offset VtocErr
		mov	si, VtocErrSiz
		call	displ_err
		jmp	$
		}
	}
}


/*       
 * This routine fills in the "pri_to_secboot" structure, that defines the
 * interface between the primary and secondary boot phases.
 *
 * The information contained in this structure is retrieved from several
 * sources:
 *     device geometry comes from the INT 13h, function 08h call;
 *     partition information comes from the fdisk table;
 *     slice (filesystem) parameters are taken from the vtoc;
 *     extended device information is being returned by our INT 13h,
 *     function F8 handler.
 */
void
init_p2s_parms(register struct pri_to_secboot *realp)
{
	register short i, j;

	realp->magic = 0x0abe;

	/* information gleaned from get_device_geometry call */
	realp->bootfrom.ufs.boot_dev = BootDev;
	realp->bootfrom.ufs.secPerTrk = secPerTrk;
	realp->bootfrom.ufs.trkPerCyl = trkPerCyl;
	realp->bootfrom.ufs.ncyls = nCyl;
#ifdef DEBUG
	if (MDXdebug) {
	printf("number of cylinders: %ld\n", nCyl);
	printf("secPerTrk: %d\n", secPerTrk);
	}
#endif

	/*
	 * information excavated from the vtoc
	 * this is a hack requested by Billt.  The vtoc sometimes has incorrect
	 * information in this field, and Sherman doesn't want to change the
	 * driver.  So hardwire it here, because getting a bogus
	 * sector size really screws things up.
	 */
	/* realp->bootfrom.ufs.bytPerSec = boot_vtoc.dkl_vtoc.v_sectorsz; */
	realp->bootfrom.ufs.bytPerSec = 512;

	for (i = 0, j = boot_vtoc.dkl_vtoc.v_nparts; i < j; i++) {
		switch(boot_vtoc.dkl_vtoc.v_part[i].p_tag) {

		/* NOTE: vtoc sector addresses are always relative to the
		 * start of the partition.  Therefore, we must add "relsect"
		 * to each starting block number to derive the absolute
		 * sector number in each case.
		 */
		case V_BOOT:
			realp->bootfrom.ufs.bslice_start = boot_vtoc.dkl_vtoc.v_part[i].p_start
			    + realp->bootfrom.ufs.Sol_start;
			realp->bootfrom.ufs.bslice_size = boot_vtoc.dkl_vtoc.v_part[i].p_size;
#ifdef DEBUG
			if (MDXdebug) {
			printf("vtoc boot slice addr: 0x%lx", boot_vtoc.dkl_vtoc.v_part[i].p_start);
			printf("  starting sector: 0x%lx\n", realp->bootfrom.ufs.bslice_start);
			printf("  partition size: 0x%lx\n", realp->bootfrom.ufs.bslice_size);
			if (MDXdebug & 0x8000) {
				putstr("Press any key to continue ...");
				wait_key();
			}
			putstr("\r\n");
			}
#endif
			break;

		case V_CACHE:
		case V_ROOT:
			realp->bootfrom.ufs.root_start = boot_vtoc.dkl_vtoc.v_part[i].p_start
			    + realp->bootfrom.ufs.Sol_start;
			realp->bootfrom.ufs.root_size = boot_vtoc.dkl_vtoc.v_part[i].p_size;
			realp->bootfrom.ufs.root_slice = i; /* slice number from vtoc */
#ifdef DEBUG
			if (MDXdebug) {
			printf("vtoc root slice addr: 0x%lx", boot_vtoc.dkl_vtoc.v_part[i].p_start);
			printf("  starting sector: 0x%lx\n", realp->bootfrom.ufs.root_start);
			printf("  partition size: 0x%lx", realp->bootfrom.ufs.root_size);
			printf("  partition number: 0x%lx\n", realp->bootfrom.ufs.root_slice);
			if (MDXdebug & 0x8000) {
				putstr("Press any key to continue ...");
				wait_key();
			}
			putstr("\r\n");
			}
#endif
			break;

		case V_ALTSCTR:
			realp->bootfrom.ufs.alts_start = boot_vtoc.dkl_vtoc.v_part[i].p_start
			    + realp->bootfrom.ufs.Sol_start;
			realp->bootfrom.ufs.alts_size = boot_vtoc.dkl_vtoc.v_part[i].p_size;
			realp->bootfrom.ufs.alts_slice = i; /* slice number from vtoc */
#ifdef DEBUG
			if (MDXdebug) {
			printf("vtoc alts slice addr: 0x%lx", boot_vtoc.dkl_vtoc.v_part[i].p_start);
			printf("  starting sector: 0x%lx\n", realp->bootfrom.ufs.alts_start);
			printf("  partition size: 0x%lx", realp->bootfrom.ufs.alts_size);
			printf("  partition number: 0x%lx\n", realp->bootfrom.ufs.alts_slice);
			if (MDXdebug & 0x8000) {
				putstr("Press any key to continue ...");
				wait_key();
			}
			putstr("\r\n");
			}
#endif
			break;

default:
			break;
		}
	}
}


struct partentry *
read_fdisk(void)
{
	register short i;

	/*
	 * Search the fdisk table sequentially to find a physical partition
	 * that satisfies two criteria:
	 *    marked as "active" (bootable) partition,
	 *    id type is "SUNIXOS".
	 */

	if (read_disk(BootDev, 0, 0, 1, 1, (char _far *)&boot_sector) != 0) {
		_asm {	       
		mov	di, offset FdskErr	;"Cannot read fdisk table."
		mov	si, FdskErrSiz
		call	displ_err
		jmp	$
		}
	}
	farbcopy((char _far *) &boot_sector, (char _far *) MBOOT_ADDR,
	    SECTOR_SIZE);

	return (struct partentry *) &boot_sector.parts[0];
}


void
get_dev_info(struct pri_to_secboot *realp)
{
	DEV_INFO _far *dip;

	if (BootDev == 0x80 || BootDev == 0x81) {
		strcat(realp->F8.hba_id, ctlr_id[IDE_TYPE]);
		return;
	}

	/* extended devices only */
	if (!(dip = get_id(BootDev))) {
		_asm {
		mov	di, offset InfoErr    ;"Cannot read extended device data."
		mov	si, InfoErrSiz
		call	displ_err
		jmp	$
		}
	}
	farbcopy((char _far *) dip, (char _far *) &realp->F8,
	    sizeof(DEV_INFO));

	get_ctlr(dip->hba_id, realp->F8.hba_id);
}

void
get_ctlr(char _far *id, char *hba_str)
{
	register short i;
	char _far *idp;
	char *csp;

	for (i = 0, csp = *ctlr_str; *csp; csp = ctlr_str[++i]) {
		for (idp = id; *csp && *csp == *idp; idp++, csp++)
		    ;
		if (*csp == 0) {
#ifdef DEBUG
			if (MDXdebug) {
			printf("replacing \"%s\" with hba_id string \"%s\"\n",
			    hba_str, ctlr_id[i]);
			if (MDXdebug & 0x8000) {
				putstr("Press any key to continue ...");
				wait_key();
			}
			putstr("\r\n");
			}
#endif
			bcopy(ctlr_id[i], hba_str, sizeof(realp->F8.hba_id));
			return;
		}
	}

	for (i = 0; hba_str[i] && i < sizeof(realp->F8.hba_id); i++) {
		/*
		 * make sure that string is lowercase
		 */
		if (hba_str[i] >= 'A' &&
		    hba_str[i] <= 'Z')
			hba_str[i] += ('a' - 'A');
	}
	if (!i)
		bcopy(ctlr_id[DEFAULT_HBA_TYPE], hba_str,
		    sizeof(realp->F8.hba_id));

#ifdef DEBUG
	if (MDXdebug) {
	printf("converted hba_id string to \"%s\"\n", hba_str);
	if (MDXdebug & 0x8000) {
		putstr("Press any key to continue ...");
		wait_key();
	}
	putstr("\r\n");
	}
#endif
	return;
}
