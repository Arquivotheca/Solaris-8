/*
 * Copyright (c) 1997, by Sun Microsystems, Inc
 * All Rights Reserved.
 */


#ident "@(#)ataprobe.c   1.2   98/02/26 SMI"

#define DEBUG /* */

#include <types.h>
#include <stdio.h>
#include "ataprobe.h"


/*
 * ata_id_data is set in ata_id,
 * Its used later to do the equivalent to a scsi inquiry
 */
unchar ata_id_data[512] = {0};


/*
 * Valid ata base addresses, terminated by 0.
 */
static ushort base_addr[] = {0x1f0, 0x170, 0};

unsigned int debuglevel = 0xffff;


static void
ata_pause()
{
	int	x;

	printf("\r\nPress ENTER to proceed ");
	x = getchar();
	printf("\r\n");
	return;
}



#define DX(label, val)		printf("%s: %x \t", label, val);
#define DS(cnt, label, str)	printf("%s: %.*s ", label, cnt, str);

static void
atapi_id_dump(void)
{
	struct ata_id *P = (struct ata_id *)ata_id_data;

#ifdef DEBUG
	
					/* WORD			*/
					/* OFFSET COMMENT	*/
DS(40, "model[40]",P->ai_model);	/* 27 Model # */
printf("\r\n");
DS(8, "fw[8]",P->ai_fw);		/* 23 Firmware revision */
printf("\r\n");
DS(20, "drvser[20]",P->ai_drvser);	/* 10 Serial number */
printf("\r\n    ");

DX("config",P->ai_config);		/* 0 general configuration bits */
DX("fixcyls",P->ai_fixcyls);		/* 1 # of fixed cylinders */
DX("remcyls",P->ai_remcyls);		/* 2 # of removable cylinders */
DX("heads",P->ai_heads);		/* 3 # of heads */
printf("\r\n    ");

DX("trksiz",P->ai_trksiz);		/* 4 # of unformatted bytes/track */
DX("secsiz",P->ai_secsiz);		/* 5 # of unformatted bytes/sector */
DX("sectors",P->ai_sectors);		/* 6 # of sectors/track */
DX("resv1[0]",P->ai_resv1[0]);		/* 7-9 "Vendor Unique" */
printf("\r\n    ");

DX("resv1[1]",P->ai_resv1[1]);		/* 7-9 "Vendor Unique" */
DX("resv1[2]",P->ai_resv1[2]);		/* 7-9 "Vendor Unique" */
DX("buftype",P->ai_buftype);		/* 20 Buffer type */
DX("bufsz",P->ai_bufsz);		/* 21 Buffer size in 512 byte incr */
printf("\r\n    ");

DX("ecc",P->ai_ecc);			/* 22 # of ecc bytes avail on rd/wr */
DX("mult1",P->ai_mult1);		/* 47 Multiple command flags */
DX("dwcap",P->ai_dwcap);		/* 48 Doubleword capabilities */
DX("capabilties",P->ai_cap);		/* 49 Capabilities */
printf("\r\n    ");

DX("resv2",P->ai_resv2);		/* 50 Reserved */
DX("piomode",P->ai_piomode);		/* 51 PIO timing mode */
DX("dmamode",P->ai_dmamode);		/* 52 DMA timing mode */
DX("validinfo",P->ai_validinfo);	/* 53 bit0: wds 54-58, bit1: 64-70 */
printf("\r\n    ");

DX("curcyls",P->ai_curcyls);		/* 54 # of current cylinders */
DX("curheads",P->ai_curheads);		/* 55 # of current heads */
DX("cursectrk",P->ai_cursectrk);	/* 56 # of current sectors/track */
DX("cursccp[0]",P->ai_cursccp[0]);	/* 57 current sectors capacity */
printf("\r\n    ");

DX("cursccp[1]",P->ai_cursccp[1]);	/* 57 current sectors capacity */
DX("mult2",P->ai_mult2);		/* 59 multiple sectors info */
DX("addrsec[0]",P->ai_addrsec[0]);	/* 60 LBA only: no of addr secs */
DX("addrsec[1]",P->ai_addrsec[1]);	/* 60 LBA only: no of addr secs */
printf("\r\n    ");

DX("sworddma",P->ai_sworddma);		/* 62 single word dma modes */
DX("dworddma",P->ai_dworddma);		/* 63 double word dma modes */
DX("advpiomode",P->ai_advpiomode);	/* 64 advanced PIO modes supported */
DX("minmwdma",P->ai_minmwdma);		/* 65 min multi-word dma cycle info */
printf("\r\n    ");

DX("recmwdma",P->ai_recmwdma);		/* 66 rec multi-word dma cycle info */
DX("minpio",P->ai_minpio);		/* 67 min PIO cycle info */
DX("minpioflow",P->ai_minpioflow);	/* 68 min PIO cycle info w/flow ctl */
DX("resv3[0]",P->ai_resv3[0]);		/* 69,70 reserved */
printf("\r\n    ");

DX("resv3[1]",P->ai_resv3[1]);		/* 69,70 reserved */
DX("resv4[0]",P->ai_resv4[0]);		/* 71-74 reserved */
DX("resv4[1]",P->ai_resv4[1]);		/* 71-74 reserved */
DX("resv4[2]",P->ai_resv4[2]);		/* 71-74 reserved */
printf("\r\n    ");

DX("resv4[3]",P->ai_resv4[3]);		/* 71-74 reserved */
DX("qdepth",P->ai_qdepth);		/* 75 queue depth */
DX("resv5[0]",P->ai_resv5[0]);		/* 76-79 reserved */
DX("resv5[1]",P->ai_resv5[1]);		/* 76-79 reserved */
printf("\r\n    ");

DX("resv5[2]",P->ai_resv5[2]);		/* 76-79 reserved */
DX("resv5[3]",P->ai_resv5[3]);		/* 76-79 reserved */
DX("majorver",P->ai_majorversion);	/* 80 major versions supported */
DX("minorver",P->ai_minorversion);	/* 81 minor version number supported*/
printf("\r\n    ");

DX("cmdset82",P->ai_cmdset82);		/* 82 command set supported */
DX("cmdset83",P->ai_cmdset83);		/* 83 more command sets supported */
DX("cmdset84",P->ai_cmdset84);		/* 84 more command sets supported */
DX("features85",P->ai_features85);	/* 85 enabled features */
printf("\r\n    ");

DX("features86",P->ai_features86);	/* 86 enabled features */
DX("features87",P->ai_features87);	/* 87 enabled features */
DX("ultradma",P->ai_ultradma);		/* 88 Ultra DMA mode */
DX("erasetime",P->ai_erasetime);	/* 89 security erase time */
printf("\r\n    ");

DX("erasetimex",P->ai_erasetimex);	/* 90 enhanced security erase time */
DX("padding1[0]",P->ai_padding1[0]);	/* pad to 125 */
DX("lastlun",P->ai_lastlun);		/* 126 last LUN, as per SFF-8070i */
DX("resv6",P->ai_resv6);		/* 127 reserved */
printf("\r\n    ");

DX("securestat",P->ai_securestatus);	/* 128 security status */
DX("vendor[0]",P->ai_vendor[0]);	/* 129-159 vendor specific */
DX("padding2[0]",P->ai_padding2[0]);	/* pad to 255 */
printf("\r\n");

#endif
	return;
}


/*
 * ata_poll1()
 *
 * Wait for a register of a controller to achieve a specific state.
 * Arguments are status port address, two sub-masks, and a time interval.
 *
 * Returns:
 *
 *	TRUE	iff all the bits in the first sub-mask are ON and
 *		all the bits in the second sub-mask are OFF.
 *
 * 	FALSE	if the specified time interval elapses.
 *
 * The interval passed is in 1 msec units.
 */
int
ata_poll1(	ushort	port,
		unchar	onbits1,
		unchar	offbits1,
		long	interval)
{
	unchar val;

	do {
		val = inb(port);
		if ((val & onbits1) == onbits1
		&&  (val & offbits1) == 0) {
			return (TRUE);
		}
		milliseconds(1);
	} while (--interval > 0);

	return (FALSE);
}



int
ata_poll3(	ushort	port,
		unchar	onbits1,
		unchar	offbits1,
		unchar	failure_onbits2,
		unchar	failure_offbits2,
		unchar	failure_onbits3,
		unchar	failure_offbits3,
		long	interval)
{
	unchar val;

	do {
		val = inb(port);
		if ((val & onbits1) == onbits1
		&&  (val & offbits1) == 0) {
			return (TRUE);
		}

		if ((val & failure_onbits2) == failure_onbits2
		&&  (val & failure_offbits2) == 0) {
			return (FALSE);
		}

		if ((val & failure_onbits3) == failure_onbits3
		&&  (val & failure_offbits3) == 0) {
			return (FALSE);
		}
		milliseconds(1);
	} while (--interval > 0);

	return (FALSE);
}


/*
 * this compare routine squeezes out extra blanks and
 * returns TRUE if p1 matches the leftmost substring of s2
 */

static int
ata_strncmp(char *p1, char *p2, int cnt)
{

	while (1) {
		/*
		 * skip over any extra blanks in both strings
		 */
		while (*p1 != '\0' && *p1 == ' ')
			p1++;

		while (cnt != 0 && *p2 == ' ') {
			p2++;
			cnt--;
		}

		/*
		 * compare the two strings
		 */

		if (cnt == 0 || *p1 != *p2)
			break;

		while (cnt > 0 && *p1 == *p2) {
			p1++;
			p2++;
			cnt--;
		}

	}

	/* return TRUE if both strings ended at same point */
	return ((*p1 == '\0') ? TRUE : FALSE);
}


/*
 * Swap bytes in 16-bit [half-]words
 */
static void
ata_swab(unchar *pf, unchar *pt, int nbytes)
{
	register unchar tmp;
	register int nshorts;

	nshorts = nbytes >> 1;

	while (--nshorts >= 0) {
		tmp = *pf++;
		*pt++ = *pf++;
		*pt++ = tmp;
	}
}


/*
 * ata_id_swab()
 *
 *	swap bytes of text fields in the ata/atapi ID response
 *
 */

static void
atapi_id_swab( struct ata_id *aidp )
{

	ata_swab(aidp->ai_drvser, aidp->ai_drvser, sizeof (aidp->ai_drvser));
	ata_swab(aidp->ai_fw, aidp->ai_fw, sizeof (aidp->ai_fw));
	ata_swab(aidp->ai_model, aidp->ai_model, sizeof (aidp->ai_model));

}


/*
 * atapi_dev() - setup an ATAPI CDROM drive for use by the booting subsystem
 *
 * Note: LS-120 floptical drives are ATAPI devices but can only be used
 * as floppy type devices via the BIOS, not as removable hard drives.
 * Therefore, they're ignored here.
 *
 */

static void
atapi_dev(ushort base_port, ushort targid, ushort lun)
{
	struct ata_id	*aidp = (struct ata_id *)ata_id_data;
	static	char	 nec_260[]	= "NEC CD-ROM DRIVE";
	static	char	 ls120_model[]	= "LS-120 HIMA";


	/*
	 * swap bytes of all text fields
	 */
	if (!ata_strncmp(nec_260, aidp->ai_model, sizeof aidp->ai_model)) {
		atapi_id_swab(aidp);
	}

	atapi_id_dump();


	return;
}

/*
 * ata_dev() - setup an ATA disk drive for use by boot subsystem
 *
 * Note: this function isn't called on a pre-2.6 system.
 */

static void
ata_dev(ushort base_port, ushort targid, ushort lun)
{
	struct ata_id *aidp = (struct ata_id *)ata_id_data;

	/*
	 * swap bytes of all text fields
	 */
	atapi_id_swab(aidp);

	atapi_id_dump();

	return;
}

/*
 * ata_pio_data_in()
 *
 *	Issue a command and then read a single 512 byte response
 *
 */

static int
ata_pio_data_in(	uint	ioaddr1,
			uint	ioaddr2,
			unchar	cmd,
			unchar far *bufaddr,
			int	long_wait,
			int	do_ata )
{
	unchar	status;
	int	cnt;

	/* bzero the buffer */
	for (cnt = 0; cnt < NBPSCTR; cnt++)
		bufaddr[cnt] = 0;

	outb(ioaddr1 + AT_CMD, cmd);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(ioaddr2);

	/* wait for the data */
	(void)ata_poll3(ioaddr2, 0, ATS_BSY, ATS_ERR, ATS_BSY,
				0x7f, 0, long_wait ? 7000 : 1000);

	/*
	 * read status byte and clear the interrupt
	 */
	status = inb(ioaddr1 + AT_STATUS);

	if (status == 0xff || status == 0x7f)
		return (1);

	if (status & ATS_BSY)
		return (2);

	if (!(status & ATS_DRQ)) {
		/*
		 * Command aborted. This happens when a ata_id command
		 * is issued to a atapi drive or when a atapi_id command
		 * is issued to an ata drive.
		 */
		if (status & (ATS_ERR | ATS_DF))
			return (3);
		if (!ata_poll1(ioaddr2, ATS_DRQ, ATS_BSY, 100)) {
			return (4);
		}
	}

	readphys(bufaddr, ioaddr1 + AT_DATA, NBPSCTR >> 1);

	/* wait for the drive to settle */
	ATA_DELAY_400NSEC(ioaddr2);

	/*
	 * wait for the drive to recognize I've read all the data.  some
	 * drives have been observed to take as much as 3msec to finish
	 * sending the data; allow 5 msec just in case.
	 *
	 * Note: some non-compliant ATAPI drives don't assert DRDY. If
	 * we've made it this far we can safely ignore the DRDY bit since
	 * the ATAPI Packet command doesn't require it to ever be asserted.
	 */
	if (!ata_poll1(ioaddr2, (unchar)(do_ata ? ATS_DRDY : 0),
				(ATS_BSY | ATS_DRQ), 5)) {
		return (5);
	}

	if (status & (ATS_DF | ATS_ERR))
		return (6);
	return (0);
}


static int
ata_id(uint ioaddr1, uint ioaddr2)
{
	extern unchar ata_id_data[NBPSCTR];
	struct ata_id *aidp = (struct ata_id *)ata_id_data;
	int	rc;

	rc = ata_pio_data_in(ioaddr1, ioaddr2, ATC_READPARMS, 
			    (unchar far *)aidp, FALSE, TRUE);
	if (rc != 0) {
		DPRINTF(DINIT, (" ata_id rc=%d status=0x%x\n",
				rc, inb(ioaddr1 + AT_STATUS)));
		return (FALSE);
	}
	
	if ((aidp->ai_config & ATAC_ATA_TYPE_MASK) != ATAC_ATA_TYPE) {
		DPRINTF(DINIT, (" ata_id ai_config=0x%x\n",
				aidp->ai_config));
		return (FALSE);
	}

	/*
	 * Bug fix 4038217 - also check for non zero heads and sectors
	 * just like the 2nd level boot ata.c does
	 */
	if (aidp->ai_heads == 0 || aidp->ai_sectors == 0) {
		DPRINTF(DINIT, (" ata_id hds=0x%x sects=0x%x\n",
				aidp->ai_heads, aidp->ai_sectors));
		return (FALSE);
	}

	return (TRUE);
}


static int
atapi_id( ushort ioaddr1, ushort ioaddr2, int long_wait )
{
	extern unchar ata_id_data[NBPSCTR];
	struct ata_id *aidp = (struct ata_id *)ata_id_data;
	int	rc;

	rc = ata_pio_data_in(ioaddr1, ioaddr2, ATC_PI_ID_DEV,
			     (unchar far *)aidp, long_wait, FALSE);

	if (rc != 0) {
		DPRINTF(DINIT, (" atapi_id rc=%d status=0x%x\n", 
				rc, inb(ioaddr1 + AT_STATUS)));
		return (FALSE);
	}
	
	if ((aidp->ai_config & ATAC_ATAPI_TYPE_MASK) != ATAC_ATAPI_TYPE) {
		DPRINTF(DINIT, (" atapi_id ai_config=0x%x\n",
				aidp->ai_config));
atapi_id_dump();
		return (FALSE);
	}

	return (TRUE);
}


/*
 * Check for 1.2 spec atapi device (eg cdrom)
 */
/* ARGSUSED */
static int
atapi_check_for_atapi_12( ushort ioaddr1, ushort ioaddr2 )
{
	unchar hcyl, lcyl;

	hcyl = inb(ioaddr1 + AT_HCYL);
	lcyl = inb(ioaddr1 + AT_LCYL);
	DPRINTF(DINIT, (" got:    hcyl: 0x%x lcyl 0x%x cnt 0x%x s# 0x%x"
						" stat 0x%x err 0x%x\r\n",
			hcyl, lcyl,
			inb(ioaddr1 + AT_COUNT), inb(ioaddr1 + AT_SECT),
			inb(ioaddr1 + AT_STATUS), inb(ioaddr1 + AT_ERROR)));
	DPRINTF(DINIT, (" expect: hcyl 0xeb lcyl 0x14 cnt 0x1 s# 0x1\r\n"));

	return ((hcyl == ATAPI_SIG_HI) && (lcyl == ATAPI_SIG_LO));
}

static void
probe_drive( uint ioaddr1, uint ioaddr2, uint drive )
{
	unchar	status;

	/*
	 * load up with the drive number
	 */
	if (drive == 0) {
		outb(ioaddr1 + AT_DRVHD, ATDH_DRIVE0);
	} else {
		outb(ioaddr1 + AT_DRVHD, ATDH_DRIVE1);
	}

	/* wait for the drives to settle */
	ATA_DELAY_400NSEC(ioaddr2);

	DPRINTF(DINIT, (" 0x%x,%d:", ioaddr1, drive));

	/*
	 * make certain the drive is selected, and wait for not busy
	 */
	ata_poll3(ioaddr2, 0, ATS_BSY, 0x7f, 0, 0x7f, 0, 5000);

	status = inb(ioaddr2);

	if (status & ATS_BSY) {
		DPRINTF(DINIT, (" status: BSY 0x%x\r\n", status));
		return;
	}

	if (ata_id(ioaddr1, ioaddr2)) {
		DPRINTF(DINIT, (" found ata disk\r\n"));
		ata_dev(ioaddr1, drive, 0);
		return;
	}

	if (atapi_check_for_atapi_12(ioaddr1, ioaddr2)) {
		DPRINTF(DINIT, (" found 1.2 atapi sig (B)"));
		if (atapi_id(ioaddr1, ioaddr2, TRUE)) {
			/*
			 * Add device to list.
			 */
			DPRINTF(DINIT, (" and 1.2 unit\r\n"));
			atapi_dev(ioaddr1, drive, 0);
		} else {
			DPRINTF(DINIT, (" but no atapi_id data\r\n"));
		}
		outb(ioaddr1 + AT_FEATURE, 0);
		outb(ioaddr1 + AT_COUNT, 0);
		outb(ioaddr1 + AT_SECT, 0);
		outb(ioaddr1 + AT_HCYL, 0);
		outb(ioaddr1 + AT_LCYL, 0);
		return;
	}
	if (atapi_id(ioaddr1, ioaddr2, FALSE)) {
		/*
		 * Add device to list.
		 */
		DPRINTF(DINIT, (" found 1.7B atapi unit\r\n"));
		atapi_dev(ioaddr1, drive, 0);
		return;
	}
	DPRINTF(DINIT, (" (C)\r\n"));
	return;
}


static void
finddev(uint ioaddr1, uint ioaddr2 )
{
	uint	drive;

	for (drive = 0; drive < 2; drive++) {
		probe_drive(ioaddr1, ioaddr2, drive);
		DPRINTF(DINIT, ("\r\n"));
	}
	return;
}


main( int argc, char **argv)
{
	int	ctlr;
	int	ioaddr1;
	int	ioaddr2;
	int	oldspl;

	oldspl = splhi();

	for (ctlr = 0; ioaddr1 = base_addr[ctlr]; ctlr++) {
		ioaddr2 = ioaddr1 + AT_IOADDR2;
		finddev(ioaddr1, ioaddr2);
	}
	splx(oldspl);
	return (0);
}
