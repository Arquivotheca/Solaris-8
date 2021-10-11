/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)ata.c	1.20	99/03/27 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/cmosram.h>
#include <sys/bootinfo.h>
#include <sys/bootlink.h>
#include <sys/promif.h>
#include <sys/salib.h>
#include <sys/ihandle.h>

extern struct int_pb    ic;
extern struct bootenv *btep;
extern struct bootops *bop;
extern void wait100ms(void);
extern int bsetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern int doint(void);
/*
 *	Local Function Prototypes
 */
static	void	check_ata();
static int ata_id(ushort ioaddr, ushort drive);
static int ata_atapi(ushort ioaddr, ushort drive);
#ifdef	notdef
static int ata_wait(ushort port, ushort mask, ushort onbits, ushort offbits,
    ulong interval);
#endif	/* notdef */
static void getbiosinfo(ushort ioaddr, ushort drive);
static void chk_drv(ushort ioaddr);
static test_hdc(ushort reg, ushort pat, ushort msk);

/*
 * The following structure and defines were lifted from the
 * kernel header ata.h. Cannot include ata.h here as it pulls in
 * to much kernel stuff and causes lint problems.
 */

struct atarpbuf {
/*  					WORD				*/
/* 					OFFSET COMMENT			*/
	ushort  atarp_config;	  /*   0  general configuration bits 	*/
	ushort  atarp_fixcyls;	  /*   1  # of fixed cylinders		*/
	ushort  atarp_remcyls;	  /*   2  # of removable cylinders	*/
	ushort  atarp_heads;	  /*   3  # of heads			*/
	ushort  atarp_trksiz;	  /*   4  # of unformatted bytes/track 	*/
	ushort  atarp_secsiz;	  /*   5  # of unformatted bytes/sector	*/
	ushort  atarp_sectors;    /*   6  # of sectors/track		*/
	ushort  atarp_resv1[3];   /*   7  "Vendor Unique"		*/
	char	atarp_drvser[20]; /*  10  Serial number			*/
	ushort	atarp_buftype;	  /*  20  Buffer type			*/
	ushort	atarp_bufsz;	  /*  21  Buffer size in 512 byte incr  */
	ushort	atarp_ecc;	  /*  22  # of ecc bytes avail on rd/wr */
	char	atarp_fw[8];	  /*  23  Firmware revision		*/
	char	atarp_model[40];  /*  27  Model #			*/
	ushort	atarp_mult1;	  /*  47  Multiple command flags	*/
	ushort	atarp_dwcap;	  /*  48  Doubleword capabilities	*/
	ushort	atarp_cap;	  /*  49  Capabilities			*/
	ushort	atarp_resv2;	  /*  50  Reserved			*/
	ushort	atarp_piomode;	  /*  51  PIO timing mode		*/
	ushort	atarp_dmamode;	  /*  52  DMA timing mode		*/
	ushort	atarp_validinfo;  /*  53  bit0: wds 54-58, bit1: 64-70	*/
	ushort	atarp_curcyls;	  /*  54  # of current cylinders	*/
	ushort	atarp_curheads;	  /*  55  # of current heads		*/
	ushort	atarp_cursectrk;  /*  56  # of current sectors/track	*/
	ushort	atarp_cursccp[2]; /*  57  current sectors capacity	*/
	ushort	atarp_mult2;	  /*  59  multiple sectors info		*/
	ushort	atarp_addrsec[2]; /*  60  LBA only: no of addr secs	*/
	ushort	atarp_sworddma;	  /*  62  single word dma modes		*/
	ushort	atarp_dworddma;	  /*  63  double word dma modes		*/
	ushort	atarp_advpiomode; /*  64  advanced PIO modes supported	*/
	ushort	atarp_minmwdma;   /*  65  min multi-word dma cycle info	*/
	ushort	atarp_recmwdma;   /*  66  rec multi-word dma cycle info	*/
	ushort	atarp_minpio;	  /*  67  min PIO cycle info		*/
	ushort	atarp_minpioflow; /*  68  min PIO cycle info w/flow ctl */
};

/*
 * port offsets from base address ioaddr1
 */
#define	AT_DATA		0x00	/* data register 			*/
#define	AT_ERROR	0x01	/* error register (read)		*/
#define	AT_FEATURE	0x01	/* features (write)			*/
#define	AT_COUNT	0x02    /* sector count 			*/
#define	AT_SECT		0x03	/* sector number 			*/
#define	AT_LCYL		0x04	/* cylinder low byte 			*/
#define	AT_HCYL		0x05	/* cylinder high byte 			*/
#define	AT_DRVHD	0x06    /* drive/head register 			*/
#define	AT_STATUS	0x07	/* status/command register 		*/
#define	AT_CMD		0x07	/* status/command register 		*/

/*
 * port offsets from base address ioaddr2
 */
#define	AT_ALTSTATUS	0x06	/* alternate status (read)		*/
#define	AT_DEVCTL	0x06	/* device control (write)		*/
#define	AT_DRVADDR	0x07 	/* drive address (read)			*/

/*	Device control register						*/
#define	AT_NIEN    	0x02    /* disable interrupts 			*/
#define	AT_SRST		0x04	/* controller reset			*/

/*
 * Status bits from AT_STATUS register
 */
#define	ATS_BSY		0x80    /* controller busy 			*/
#define	ATS_DRDY	0x40    /* drive ready 				*/
#define	ATS_DWF		0x20    /* write fault 				*/
#define	ATS_DSC    	0x10    /* seek operation complete 		*/
#define	ATS_DRQ		0x08	/* data request 			*/
#define	ATS_CORR	0x04    /* ECC correction applied 		*/
#define	ATS_IDX		0x02    /* disk revolution index 		*/
#define	ATS_ERR		0x01    /* error flag 				*/
/*
 * Drive selectors for AT_DRVHD register
 */
#define	ATDH_LBA	0x40	/* addressing in LBA mode not chs 	*/
#define	ATDH_DRIVE0	0xa0    /* or into AT_DRVHD to select drive 0 	*/
#define	ATDH_DRIVE1	0xb0    /* or into AT_DRVHD to select drive 1 	*/
/*
 * ATA commands.
 */
#define	ATC_READPARMS	0xec    /* Read Parameters command 		*/

/*
**  Port definitions:
*/

#define	hi_byte(reg)	((reg) >> 8)
#define	lo_byte(reg)	((reg) & 0xff)

void
check_hdbios(void)
{
#ifdef	BOOT_DEBUG
	btep->db_flag |= BOOTTALK;
#endif
	check_ata();
#ifdef	BOOT_DEBUG
	(void) goany();
#endif
}

/*
 *  Check if hard disk controller is present and
 *  probably register compatible with the standard
 *  AT controller.  Abort if not.
 */
static
test_hdc(ushort reg, ushort pat, ushort msk)
{
	outb(reg, pat);
	return ((inb(reg)&msk) != (pat&msk) ? -1 : 0);
}

/*
 *  Check if controller, drive present.
 */
static void
chk_drv(ushort ioaddr)
{

	ushort	drive;
#define	MASK	(ATS_BSY|ATS_DRDY|ATS_DWF|ATS_DSC)
#define	EXP	(ATS_DRDY|ATS_DSC)

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("checking ioaddr: 0x%x\n", ioaddr);
#endif

	if (test_hdc(ioaddr+AT_LCYL, 0x55, 0xff) ||
			test_hdc(ioaddr+AT_LCYL, 0xaa, 0xff) ||
			test_hdc(ioaddr+AT_HCYL, 0x55, 0x01) ||
			test_hdc(ioaddr+AT_HCYL, 0xaa, 0x01)) {
#ifdef BOOT_DEBUG
		if (btep->db_flag & BOOTTALK)
			printf("hd controller not present: 0x%x.\n", ioaddr);
#endif
		return;
	}

	for (drive = 0; drive < 2; drive++) {
#ifdef BOOT_DEBUG
		if (btep->db_flag & BOOTTALK)
			printf("**** checking: 0x%x %d ", ioaddr, drive);
#endif
		if (ata_atapi(ioaddr, drive) == 0) {
#ifdef BOOT_DEBUG
			if (btep->db_flag & BOOTTALK)
				printf("atapi device found\n");
#endif
			continue;
		}
		if (ata_id(ioaddr, drive) == 0) {
#ifdef BOOT_DEBUG
			if (btep->db_flag & BOOTTALK)
				printf("ata device found\n");
#endif
			getbiosinfo(ioaddr, drive);
			continue;
		}
#ifdef BOOT_DEBUG
		if (btep->db_flag & BOOTTALK)
			printf("device not found\n");
#endif
	}
}

static	ata_hdbioscount;

static void
getbiosinfo(ushort ioaddr, ushort drive)
{
	int	carryflag, cmosd;
	short	*chs;
	char	configname[35];


	/* first look to see if Int 13/Fn 4x (LBA) access is allowed */

	ic.intval = DEVT_DSK;
	ic.ax = INT13_CHKEXT << 8;
	ic.bx = 0x55AA;
	ic.cx = 0;
	ic.dx = 0x80 + drive;

	carryflag = doint();

	if (!carryflag && ic.bx == 0xAA55 && (ic.cx & 1))  {
		/* support LBA in BIOS; don't create prop, just return */
#ifdef BOOT_DEBUG
		printf("ufsboot: LBA support for ATA drive %d\n", drive);
#endif
		return;
	}

	/* create property to hold BIOS geometry */
	(void) sprintf(configname, "SUNW-ata-%x-d%d-chs", ioaddr, drive+1);

	ic.intval = DEVT_DSK;
	ic.ax = INT13_PARMS << 8;
	ic.dx = 0x80 + drive;

	carryflag = doint();

	if (carryflag || hi_byte(ic.ax))
		return;

	chs = (short *)bkmem_alloc(3 * sizeof (*chs));
	chs[0] = ((lo_byte(ic.cx) & ~0x3f) << 2) | hi_byte(ic.cx);
	chs[1] = hi_byte(ic.dx);
	chs[2] = lo_byte(ic.cx) & 0x3f;

	/*
	 * If the bios setting is due to a scsi controller, we would see
	 * a drive 0 type in cmos, which means we should not bother.
	 */
	outb(CMOS_ADDR, FDTB);
	cmosd = inb(CMOS_DATA);
	if (drive == 0 && !(cmosd & 0xf0) || drive == 1 && !(cmosd & 0xf))
		return;

	ata_hdbioscount++;
#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("ufsboot: setting drive %d  c: %d  h: %d  s: %d\n",
			drive, chs[0], chs[1], chs[2]);
#endif
	/*
	 * Caveat if we get here, there is a chance that the drive data is
	 * put there by the SCSI controller, this would be true for
	 * the 3rd and 4th drive only.
	 */
	(void) bsetprop(bop, configname, (caddr_t)chs, 3 * sizeof (*chs), 0);
}

#ifndef	ATA_BASE0
#define	ATA_BASE0	0x1f0
#define	ATA_BASE1	0x170
#endif

static void
check_ata()
{
	chk_drv(ATA_BASE0);
	chk_drv(ATA_BASE1);
}

/*
 * Similar to ata_wait but the timeout is varaible.
 */
static int
ata_wait1(register ushort port, ushort mask, ushort onbits, ushort offbits,
    int interval)
{
	register int i;
	register ushort maskval;

	for (i = interval; i; i--) {
		maskval = inb(port) & mask;
		if (((maskval & onbits) == onbits) &&
		    ((maskval & offbits) == 0))
			return (0);
		wait100ms();
	}
	return (-1);
}

#ifdef	notdef
/*
 * Wait for a register of a controller to achieve a
 * specific state.  Arguments are a mask of bits we care about,
 * and two sub-masks.  To return normally, all the bits in the
 * first sub-mask must be ON, all the bits in the second sub-
 * mask must be OFF.  If 5 seconds pass without the controller
 * achieving the desired bit configuration, we return 1, else 0.
 */
static int
ata_wait(register ushort port, ushort mask, ushort onbits, ushort offbits,
    ulong interval)
{
	register ushort maskval;

	while (interval-- > 0) {
		maskval = inb(port) & mask;
		if (((maskval & onbits) == onbits) &&
		    ((maskval & offbits) == 0))
			return (0);
		wait100ms();
	}
	return (-1);
}
#endif	/* notdef */

/*
 * This routine restores "ioaddr + AT_DRVHD" after use because
 * changing it confuses some El Torito BIOSes (e.g. AST Bravo
 * MS 6333 with BIOS rev 4.06.14C).
 */
static int
ata_id(ushort ioaddr, ushort drive)
{
	ushort	space[NBPSCTR];
	struct atarpbuf	*rpbp = (struct atarpbuf *)space;
	int	i;
	int result;
	char iosave = inb(ioaddr + AT_DRVHD);

	i = ata_wait1(ioaddr + AT_STATUS, ATS_DRQ | ATS_BSY,
	    ATS_DRQ, ATS_BSY, 5);
#ifdef BOOT_DEBUG
	if (i) {
		if (btep->db_flag & BOOTTALK)
			printf("drive did not settle\n");
	}
#endif
	outb(ioaddr + AT_DRVHD, drive == 0 ? ATDH_DRIVE0 : ATDH_DRIVE1);
	outb(ioaddr + AT_CMD, ATC_READPARMS);
	if (ata_wait1(ioaddr + AT_STATUS, ATS_DRQ | ATS_BSY,
					ATS_DRQ, ATS_BSY, 5)) {
#ifdef BOOT_DEBUG
		if (btep->db_flag & BOOTTALK)
			printf("read params failed\n");
#endif
		outb(ioaddr + AT_DRVHD, iosave);
		return (-1);
	}
	for (i = 0; i < NBPSCTR >> 1; i++)
		space[i] = inb(ioaddr + AT_DATA);

	if (rpbp->atarp_heads && rpbp->atarp_sectors) {
		result = (inb(ioaddr + AT_STATUS) & ATS_ERR ? -1 : 0);
		outb(ioaddr + AT_DRVHD, iosave);
		return (result);
	}

	outb(ioaddr + AT_DRVHD, iosave);
	return (-1);
}

#ifndef ATC_PI_ID_DEV
#define	ATC_PI_ID_DEV	0xa1
#endif

/*
 * This routine restores "ioaddr + AT_DRVHD" after use because
 * changing it confuses some El Torito BIOSes (e.g. AST Bravo
 * MS 6333 with BIOS rev 4.06.14C).
 */
static int
ata_atapi(ushort ioaddr, ushort drive)
{
	int	i;
	int result;
	char iosave = inb(ioaddr + AT_DRVHD);

	if (drive == 0)
		outb(ioaddr + AT_DRVHD, ATDH_DRIVE0);
	else
		outb(ioaddr + AT_DRVHD, ATDH_DRIVE1);

	outb(ioaddr + AT_CMD, ATC_PI_ID_DEV);
	if (ata_wait1(ioaddr + AT_STATUS, ATS_DRQ | ATS_BSY,
	    ATS_DRQ, ATS_BSY, 2)) {
		outb(ioaddr + AT_DRVHD, iosave);
		return (-1);
	}
	for (i = 0; i < NBPSCTR >> 1; i++)
		(void) inb(ioaddr + AT_DATA);

	(void) ata_wait1(ioaddr + AT_STATUS, ATS_BSY | ATS_DRDY,
		ATS_DRDY, ATS_BSY, 2);
	result = (inb(ioaddr + AT_STATUS) & ATS_ERR ? -1 : 0);
	outb(ioaddr + AT_DRVHD, iosave);
	return (result);
}
