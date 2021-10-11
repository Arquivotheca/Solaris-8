/*
 * Copyright (c) 1997, by Sun Microsystems, Inc
 * All Rights Reserved.
 */

#ident "@(#)ata.c   1.15   99/03/29 SMI"

/*
 * This file provides ata disk support only.
 * Any atapi (cd-rom) support is handled in atapi.c
 */

/* #define	DEBUG /* */

#include <types.h>
#include <bef.h>
#include <common.h>
#include "..\scsi.h"
#include "ata.h"

/*
 * local data
 */


/*
 * ata_pio_data_in()
 *
 *	Issue a command and then read a single 512 byte response
 *
 */

int
ata_pio_data_in(	uint	ioaddr1,
			uint	ioaddr2,
			unchar	cmd,
			unchar far *bufaddr,
			ushort	sec_count,
			int	long_wait,
			int	do_ata )
{
	unchar	status;

	DPRINTF(DENT, ("ata_pio_data_in()\r\n"));

        /*
         * Always make certain interrupts are enabled, tri-stating the
         * interrupt line generates all sorts of glitches and seems
         * to totally confuse some NEC drives.
         */
        outb(ioaddr2 + AT_DEVCTL, AT_DC3);

	outb(ioaddr1 + AT_CMD, cmd);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(ioaddr2);

	do {
		/*
		 * wait for not busy
		 */
		ata_poll3(ioaddr2, 0, ATS_BSY, ATS_ERR, ATS_BSY,
				   0x7f, 0, long_wait ? 7000 : 4000);

		/*
		 * read the status byte and clear the interrupt
		 */
		status = inb(ioaddr1 + AT_STATUS);

		if (status == 0xff || status == 0x7f) {
			/* invalid status */
			return (ATA_FATAL_ERROR);
		}

		if (status & ATS_BSY) {
			return (ATA_FATAL_ERROR);
		}

		/*
		 * wait for DRQ (on some older drives DRQ might not be
		 * asserted at the same time BUSY is de-asserted)
		 */

		if (!(status & ATS_DRQ)) {
			/* but don't wait if the command aborted */
			if (status & (ATS_ERR | ATS_DF)) {
				return (ATA_CHECK_CONDITION);
			}
			if (!ata_poll1(ioaddr2, ATS_DRQ, ATS_BSY, 1000)) {
				return (ATA_FATAL_ERROR);
			}
		}


		/* transfer the data */
		readphys(bufaddr, ioaddr1 + AT_DATA, NBPSCTR >> 1);
		bufaddr += NBPSCTR;

		/* wait for the busy bit to settle */
		ATA_DELAY_400NSEC(ioaddr2);

		/*
		 * bailout if error detected
		 */
		if (status & (ATS_DF | ATS_ERR)) {
			break;
		}

	} while (--sec_count > 0);

	/*
	 * wait for the drive to recognize I've read all the data.  some
	 * drives have been observed to take as much as 3msec to finish
	 * sending the data; allow 5 msec just in case.
	 *
	 * Note: some non-compliant ATAPI drives (e.g., NEC Multispin 6V,
	 * CDR-1350A) don't assert DRDY. If we've made it this far we can
	 * safely ignore the DRDY bit since the ATAPI Packet command
	 * doesn't require it to ever be asserted.
	 *
	 */
	ata_poll1(ioaddr2, (unchar)(do_ata ? ATS_DRDY : 0),
			   ATS_DRQ | ATS_BSY, 5);

	/*
	 * Check to see if the command aborted. This happens
	 * when a ata_id command is issued to a atapi drive
	 * or when a atapi_id command is issued to a ata drive.
	 */
	if (status & (ATS_DF | ATS_ERR)) {
		return (ATA_CHECK_CONDITION);
	}

	return (ATA_IO_OKAY);
}


int
ata_id(uint ioaddr1, uint ioaddr2)
{
	extern unchar	 ata_id_data[NBPSCTR];
	struct ata_id	*aidp = (struct ata_id *)ata_id_data;
	int		 rc;

	DPRINTF(DENT, (" ata_id() "));

	bzero((unchar *)aidp, sizeof (struct ata_id));

	rc = ata_pio_data_in(ioaddr1, ioaddr2, ATC_READPARMS, 
			    (unchar far *)aidp, 1, FALSE, TRUE);

	if (rc != ATA_IO_OKAY) {
		DPRINTF(DENT, (" ata_id() pio fail "));
		return (FALSE);
	}
	
	if ((aidp->ai_config & ATAC_ATA_TYPE_MASK) != ATAC_ATA_TYPE) {
		DPRINTF(DENT, (" ata_id() ai_config 0x%x", aidp->ai_config));
		return (FALSE);
	}

	/*
	 * Bug fix 4038217 - also check for non zero heads and sectors
	 * just like the 2nd level boot ata.c does
	 */
	if (aidp->ai_heads == 0 || aidp->ai_sectors == 0) {
		DPRINTF(DENT, (" ata_id() h %d s %d ",
			aidp->ai_heads, aidp->ai_sectors));
		return (FALSE);
	}

	return (TRUE);
}

/* ARGSUSED */
int
ata_read(	ata_info_t *atap,
		ulong	    start_block,
		ushort	    sec_count,
		ushort	    bsize,
		unchar far *bufaddr )
{
	uint	ioaddr1 = atap->ata_base_port;
	uint	ioaddr2 = ioaddr1 + AT_IOADDR2;
	ushort	status;
	int	i;
	ushort	cyl;
	unchar	head;
	unchar	sector;

	DEB_STR(DENT, "ata_read()\r\n");

/* ??? my bios spec says the limit is 128 ??? */
	if (sec_count > 255) {
		DEB_PAU(DERR, "Invalid sector count\n");
		return (ATA_FATAL_ERROR);
	}

	if (atap->ata_lba_mode) {
		/*
		 * Most drives now support LBA mode
		 */
		if (start_block > 0x10000000) {
			DEB_PAU(DERR, "Invalid LBA start\n");
			return (ATA_FATAL_ERROR);
		}
		if (start_block + sec_count > 0x10000000) {
			DEB_PAU(DERR, "Invalid LBA end\n");
			return (ATA_FATAL_ERROR);
		}
		sector = start_block & 0xff;
		cyl = (start_block >> 8) & 0xffff;
		head = ((start_block >> 24) & 0xf) | ATDH_LBA;
	} else {
		/*
		 * Convert the LBA to the CHS using the geometry
		 * reported by the disk
		 */
		ulong	sectors_per_cyl = atap->ata_heads * atap->ata_secs;
		ulong	tmp;

		if (start_block > atap->ata_disksize) {
			DEB_PAU(DERR, "Invalid CHS start\n");
			return (ATA_FATAL_ERROR);
		}
		if (start_block + sec_count > atap->ata_disksize) {
			DEB_PAU(DERR, "Invalid CHS end\n");
			return (ATA_FATAL_ERROR);
		}

		/*
		 * Note: long multiples and divides are done
		 * via libarary routines
		 *
		 *
		 * compute:
		 *	cyl = start_block / sectors_per_cyl
		 */
		cyl = ULongDiv(start_block, sectors_per_cyl);

		/*
		 * compute:
		 *	tmp = (start_block % sectors_per_cyl)
		 *	head = tmp / atap->ata_secs;
		 */
		
		tmp = ULongRem(start_block, sectors_per_cyl);
		head = ULongDiv(tmp, (ulong)atap->ata_secs);


		/*
		 * compute:
		 *	sector = (start_block % atap->ata_secs) + 1;
		 */
		sector = ULongRem(start_block, (ulong)atap->ata_secs) + 1;

		DEB_STR(DIO, "ata_read() ioaddr1:");
		DEB_HEX(DIO, ioaddr1);
		DEB_STR(DIO, " cyl:");
		DEB_HEX(DIO, cyl);
		DEB_STR(DIO, " head:");
		DEB_HEX(DIO, head);
		DEB_STR(DIO, " sector:");
		DEB_HEX(DIO, sector);
		DEB_STR(DIO, " sec_count:");
		DEB_HEX(DIO, (sec_count & 0xff));
		DEB_STR(DIO, "\r\n");
	}

	/*
	 * Select drive and head
	 */
	if (atap->ata_targid == 0) {
		outb(ioaddr1 + AT_DRVHD, (unchar)ATDH_DRIVE0);
	} else {
		outb(ioaddr1 + AT_DRVHD, (unchar)ATDH_DRIVE1);
	}

	/* wait for the drive to settle */
	ATA_DELAY_400NSEC(ioaddr2);

	/*
	 * Set the head
	 */
	if (atap->ata_targid == 0) {
		outb(ioaddr1 + AT_DRVHD, (unchar)(ATDH_DRIVE0 | head));
	} else {
		outb(ioaddr1 + AT_DRVHD, (unchar)(ATDH_DRIVE1 | head));
	}

	/*
	 * Set up task file
	 *	- turn off interrupts
	 *	- clear dma feature bit
	 *	- output read sector(s) command
	 */
	outb(ioaddr1 + AT_SECT, sector);
	outb(ioaddr1 + AT_COUNT, (unchar) (sec_count & 0xff));
	outb(ioaddr1 + AT_LCYL, (unchar) (cyl & 0xff));
	outb(ioaddr1 + AT_HCYL, (unchar) (cyl >> 8));
	outb(ioaddr1 + AT_FEATURE, 0); /* clear dma feature bit */

	/* read the sector */
	if (ata_pio_data_in(ioaddr1, ioaddr2, ATC_RDSEC, (unchar far *)bufaddr,
			    sec_count, FALSE, TRUE) == ATA_IO_OKAY) {
		DEB_STR(DIO, "ata_read: cmd succeeded\r\n");
		return (ATA_IO_OKAY);
	}

	DPRINTF(DERR, ("ata_read: failed:"));
	DPRINTF(DERR, (" ioaddr1 0x%x hcyl 0x%x lcyl 0x%x",
			 ioaddr1, inb(ioaddr1 +AT_LCYL), inb(ioaddr1+AT_HCYL)));
	DPRINTF(DERR, (" sect 0x%x count 0x%x status 0x%x altstatus 0x%x",
			 inb(ioaddr1 + AT_SECT), inb(ioaddr1 + AT_COUNT),
			 inb(ioaddr1 + AT_STATUS), inb(ioaddr2)));
	DPRINTF(DERR, (" error 0x%x drvhd 0x%xr\n",
			 inb(ioaddr1 + AT_ERROR), inb(ioaddr1 + AT_DRVHD)));
	DEB_PAU(DERR, "ata_read");

	return (ATA_FATAL_ERROR);
}

/* ARGSUSED */
int
ata_inquire(ata_info_t *atap, ushort base_port, ushort targid, ushort lun)
{
	extern struct inquiry_data	inqd;
	extern unchar	 ata_id_data[NBPSCTR];
	struct ata_id	*aidp = (struct ata_id *)ata_id_data;
	int		 i;
	int		 j;

	/*
         * copy the model (already been swapped by ata_dev())
         */
	for (i = 0, j = 0; i < 8; i++, j++) {
		inqd.inqd_vid[i] = aidp->ai_model[j];
	}
        for (i = 0; i < 16; i++, j++) {
		inqd.inqd_pid[i] = aidp->ai_model[j];
        }
	for (i = 0; i < 4; i++, j++) {
		inqd.inqd_prl[i] = aidp->ai_model[j];
	}
	inqd.inqd_pdt = INQD_PDT_DA;
	inqd.inqd_dtq = 0;
	return (ATA_IO_OKAY);
}

/* ARGSUSED */
int
ata_readcap(ata_info_t *atap)
{
	extern	struct readcap_data readcap_data;
	ulong	disksize_tmp;

	/* disk drives are always 512 bytes per sector */
	readcap_data.rdcd_bl[0] = 0;
	readcap_data.rdcd_bl[1] = 0;
	readcap_data.rdcd_bl[2] = 2;
	readcap_data.rdcd_bl[3] = 0;

	/* readcap_data.rdcd_lba is address of last block (nblks - 1) */
	disksize_tmp = atap->ata_disksize - 1;

	readcap_data.rdcd_lba[0] = (unchar) ((disksize_tmp >> 24) & 0xFF);
	readcap_data.rdcd_lba[1] = (unchar) ((disksize_tmp >> 16) & 0xFF);
	readcap_data.rdcd_lba[2] = (unchar) ((disksize_tmp >> 8)  & 0xFF);
	readcap_data.rdcd_lba[3] = (unchar) (disksize_tmp  & 0xFF);

	return (ATA_IO_OKAY);
}

/* ARGSUSED */
int
ata_sense(ata_info_t *atap, ushort base_port, ushort targid, ushort lun)
{
	return (ATA_IO_OKAY);
}

/* ARGSUSED */
int
ata_lock(ata_info_t *atap, int lock)
{
	return (ATA_IO_OKAY);
}

/* ARGSUSED */
int
ata_motor(ata_info_t *atap, int on)
{
	return (ATA_IO_OKAY);
}

