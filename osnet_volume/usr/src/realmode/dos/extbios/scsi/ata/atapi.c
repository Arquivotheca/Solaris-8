/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident "@(#)atapi.c   1.18   98/02/26 SMI\n"

/*
 * Provides atapi support (currently cd-rom drive)
 */

/* #define	DEBUG /* */

#include <types.h>
#include <bef.h>
#include <common.h>
#include "..\scsi.h"
#include "ata.h"
#include "atapi.h"

/*
 * Local function prototypes
 */
static	int	atapi_do_cmd(ata_info_t *atap, ushort ioaddr1, ccb_t *ccbp);
static	void	atapi_fatal_error(ata_info_t *atap, ushort ioaddr1,
				  ushort ioaddr2);
static	int	atapi_fsm(ata_info_t *atap, ushort ioaddr1, ushort ioaddr2,
			  unchar func, ccb_t *ccbp, unchar status);
static	void	atapi_start_cmd(ata_info_t *atap, ushort ioaddr1,
				ushort ioaddr2, ccb_t *ccbp);
static	void	atapi_pio_data_in(ata_info_t *atap, ushort ioaddr1,
				  ushort ioaddr2, ccb_t *ccbp);
static	void	atapi_pio_data_out(ata_info_t *atap, ushort ioaddr1,
				   ushort ioaddr2, ccb_t *ccbp);
static	void	atapi_send_cdb(ata_info_t *atap, ushort ioaddr1,
			       ushort ioaddr2, ccb_t *ccbp);
static	void	atapi_set_fallback_mode(ata_info_t *atap, int enable);
static	void	atapi_status(ata_info_t *atap, ushort ioaddr1, ushort ioaddr2,
			     ccb_t *ccbp, unchar status);


/*
 * local data, with initializers so that when compiled for 2.5.1 
 * these structures end up in the data segment of the driver (which
 * is copied to high memory by scsi.c) rather than the bss segment
 * of which only only part is relocated to high memory.
 */
ccb_t	ccb = {0};
unchar sense_data[18] = {0}; /* even no of bytes */

struct mbuf {
	unchar	mode_header[8];
	unchar	mode_page;
	unchar	mode_len;
	unchar	mode_resv[2];
	unchar	mode_cap0;
	unchar	mode_cap1;
	unchar	mode_cap2;
	unchar	mode_cap3;
	unchar	mode_max_speed_msb;
	unchar	mode_max_speed_lsb;
	unchar	mode_volume_msb;
	unchar	mode_volume_lsb;
	unchar	mode_bufsize_msb;
	unchar	mode_bufsize_lsb;
	unchar	mode_current_speed_msb;
	unchar	mode_current_speed_lsb;
	unchar	mode_adpcm;
	unchar	mode_pad;
} mbuf = {1};

#define	ATAPI_FSM_START		0
#define	ATAPI_FSM_INTERRUPT	1
#define	ATAPI_FSM_TIMEOUT	2

/* ARGSUSED */
int
atapi_read(	ata_info_t *atap,
		ulong	  start_block,
		ushort	  count,
		ushort	  bsize,
		unchar far *bufaddr )
{
	ushort	totbytes;
	int	i;
	int	rc;

	DEB_STR(DENT, "atapi_read()\r\n");

	/* Quick and dirty hack to allow long result of multiply */
	for (totbytes = 0, i = 0; i < count; i++) {
		totbytes += bsize;
	}

	if (totbytes > ATAPI_MAX_XFER) {
		DEB_STR(DERR, "atapi_read(): count too high 0x");
		DEB_HEX(DERR, totbytes);
		DEB_PAU(DERR, "\r\n");
		return (ATA_FATAL_ERROR);
	}

	bzero((unchar *)&ccb, sizeof (ccb));
	ccb.ccb_drive = atap->ata_targid;
	ccb.ccb_lun = atap->ata_lun;
	ccb.ccb_data_length = totbytes;
	ccb.ccb_data_addr = bufaddr;
	ccb.ccb_scsi_cmd[0] = SX_READ;
	ccb.ccb_scsi_cmd[2] = (start_block >> 24) & 0xff;
	ccb.ccb_scsi_cmd[3] = (start_block >> 16) & 0xff;
	ccb.ccb_scsi_cmd[4] = (start_block >> 8) & 0xff;
	ccb.ccb_scsi_cmd[5] = start_block & 0xff;
	ccb.ccb_scsi_cmd[7] = count >> 8;
	ccb.ccb_scsi_cmd[8] = count & 0xFF;

	DEB_STR(DIO, "atapi_read(): totbytes ");
	DEB_HEX(DIO, (ushort) totbytes);
	DEB_STR(DIO, " start_block ");
	DEB_HEX(DIO, (short) start_block);
	DEB_PAU(DIO, "\r\n");

	rc = atapi_do_cmd(atap, atap->ata_base_port, &ccb);
	if (rc != ATA_IO_OKAY) {
		atapi_cmd_error(atap, atap->ata_base_port);
	}
	return (rc);
}

/* ARGSUSED */
int
atapi_sense(ata_info_t *atap, ushort base_port, ushort targid, ushort lun)
{
	int i;


	DEB_STR(DENT, "atapi_sense()\r\n");

	bzero((unchar *)&ccb, sizeof (ccb));
	ccb.ccb_drive = targid;
	ccb.ccb_lun = lun;
	ccb.ccb_data_length = sizeof sense_data;
	ccb.ccb_data_addr = (unchar far *) sense_data;
	ccb.ccb_scsi_cmd[0] = SC_RSENSE;
	ccb.ccb_scsi_cmd[4] = sizeof sense_data;

	for (i = 0; i < ATA_SENSE_RETRIES; i++) {
		if (atapi_do_cmd(atap, base_port, &ccb) != ATA_IO_OKAY) {
			continue;
		}

		/*
		 * Sense Key == 2, ASC == 4, ASCQ == 1, means the
		 * drive is in the process of becoming ready. Give
		 * it a second to finish and then retry the request.
		 */
		if ((sense_data[2] & 0x0f) == 2
		&&  sense_data[12] == 4 &&  sense_data[13] == 1) {
			DPRINTF(DINIT, ("atapi_sense: wait for drive\r\n"));
			milliseconds(1000);
			continue;
		}

		/* good sense */
		DEB_STR(DINIT, "dev_sense success: 0x");
		DEB_HEX(DINIT, sense_data[2] & 0x0f);
		DEB_STR(DINIT, "\r\n");
		{
		    unchar *cp = sense_data;
		    ulong i;

		    DEB_STR(DINIT, "sense data < ");
		    for (i = 0; i < (sizeof (sense_data)); i++) {
			    DPRINTF(DINIT, ("%.2x ",*cp));
			    cp++;
		    }
		    DEB_STR(DINIT, ">\r\n");
		}

		DEB_PAU(DINIT, "atapi_sense: ");
		return (sense_data[2] & 0x0f);
	}

	return (ATAPI_REQSENSE_FAILED); /* bad sense */
}

/*
 * atapi_inquire() -- return 0 for ok else failed
 */
/* ARGSUSED */
int
atapi_inquire(ata_info_t *atap, ushort base_port, ushort targid, ushort lun)
{
	extern struct inquiry_data inqd;
	int	rc;

	DEB_STR(DENT, "atapi_inquire()\r\n");

	DEB_STR(DINIT, "dev_inquire() base ");
	DEB_HEX(DINIT, base_port);
	DEB_STR(DINIT, " drive ");
	DEB_HEX(DINIT, targid);
	DEB_STR(DINIT, "\r\n");

	bzero((unchar *)&ccb, sizeof (ccb));
	ccb.ccb_drive = targid;
	ccb.ccb_lun = lun;
	ccb.ccb_data_length = sizeof (inqd);
	ccb.ccb_data_addr = (unchar far *) &inqd;
	ccb.ccb_scsi_cmd[0] = SC_INQUIRY;
	ccb.ccb_scsi_cmd[4] = sizeof (inqd);

	if ((rc = atapi_do_cmd(atap, base_port, &ccb)) != ATA_IO_OKAY) {
		return (rc);
	}
	if (inqd.inqd_pdt == INQD_PDT_NOLUN) {
		/* not sure this is possible for atapi */
		DEB_STR(DIO, "dev_inquire() INQD_PDT_NOLUN\r\n");
		rc = ATA_FATAL_ERROR;
	}
#ifdef DEBUG
	{
	    unchar *cp = (unchar *) &inqd;
	    ulong i;

	    DEB_STR(DIO, "inq data is:- ");
	    for (i = 0; i < (sizeof (inqd)); i++) {
		    DEB_HEX(DIO, *cp);
		    cp++;
	    }
	    DEB_STR(DIO, "\r\n");
	}
#endif

	return (rc); /* success */
}

/*
 * atapi_readcap -- entry point to read capacity
 */
int
atapi_readcap(ata_info_t *atap)
{
	extern struct readcap_data readcap_data;
	int	rc;

	DEB_STR(DENT, "atapi_readcap(): base 0x");
	DEB_HEX(DENT, atap->ata_base_port);
	DEB_STR(DENT, " drive ");
	DEB_HEX(DENT, atap->ata_targid);
	DEB_STR(DENT, "\r\n");

	bzero((unchar *)&ccb, sizeof (ccb));
	bzero((unchar *)&readcap_data, sizeof (readcap_data));
	ccb.ccb_drive = atap->ata_targid;
	ccb.ccb_lun = atap->ata_lun;
	ccb.ccb_data_addr = (unchar far *) &readcap_data;
	ccb.ccb_data_length = sizeof (readcap_data);
	ccb.ccb_scsi_cmd[0] = SX_READCAP;

	rc = atapi_do_cmd(atap, atap->ata_base_port, &ccb);
	if (rc != ATA_IO_OKAY) {
		return (rc); /* error */
	}

	DEB_STR(DINIT, "dev_readcap() success\r\n");
	DEB_STR(DINIT, "READCAP: # of blocks ");
	DEB_HEX(DINIT, readcap_data.rdcd_lba[0]);
	DEB_HEX(DINIT, readcap_data.rdcd_lba[1]);
	DEB_HEX(DINIT, readcap_data.rdcd_lba[2]);
	DEB_HEX(DINIT, readcap_data.rdcd_lba[3]);
	DEB_STR(DINIT, " block length ");
	DEB_HEX(DINIT, readcap_data.rdcd_bl[0]);
	DEB_HEX(DINIT, readcap_data.rdcd_bl[1]);

	/* accept 512 or 2k bytes */
	if ((readcap_data.rdcd_bl[2] != 0x8 && readcap_data.rdcd_bl[2] != 0x2)
	||  readcap_data.rdcd_bl[3] != 0) {
		DEB_PAU(DENT, "Block size adjusted\r\n");
		readcap_data.rdcd_bl[2] = 0x8;
		readcap_data.rdcd_bl[3] = 0x0;
	}

	DEB_HEX(DINIT, readcap_data.rdcd_bl[2]);
	DEB_HEX(DINIT, readcap_data.rdcd_bl[3]);
	DEB_STR(DINIT, " drive ");
	DEB_HEX(DINIT, ccb.ccb_drive);
	DEB_STR(DINIT, "\r\n");

	return (rc);
}


int
atapi_id( ushort ioaddr1, ushort ioaddr2, int long_wait )
{
	extern unchar ata_id_data[NBPSCTR];
	struct ata_id *aidp = (struct ata_id *)ata_id_data;

	int	rc;

	DEB_STR(DENT, "atapi_id()\r\n");

	bzero((unchar *)aidp, sizeof (struct ata_id));
	rc = ata_pio_data_in(ioaddr1, ioaddr2, ATC_PI_ID_DEV,
			     (unchar far *)aidp, 1, long_wait, FALSE);
	
	if (rc != ATA_IO_OKAY)
		return (FALSE);
	
	if ((aidp->ai_config & ATAC_ATAPI_TYPE_MASK) != ATAC_ATAPI_TYPE)
		return (FALSE);

	return (TRUE);
}


/*
 * dev_motor -- start/stop motor
 */
int
atapi_motor(ata_info_t *atap, int on)
{
	int	rc;

	DEB_STR(DENT, "atapi_motor()\r\n");

	bzero((unchar *)&ccb, sizeof (ccb));
	ccb.ccb_drive = atap->ata_targid;
	ccb.ccb_lun = atap->ata_lun;
	ccb.ccb_data_length = 0;
	ccb.ccb_scsi_cmd[0] = SC_STRT_STOP;
	ccb.ccb_scsi_cmd[4] = on; /* start or stop motor flag */

	rc = atapi_do_cmd(atap, atap->ata_base_port, &ccb);
	if (rc != ATA_IO_OKAY)  {
		milliseconds(ATA_START_MOTOR_WAIT); /* if error wait */
	}
	return (rc);
}

/*
 * dev_lock -- lock / unlock door
 */
int
atapi_lock(ata_info_t *atap, int lock)
{

	DEB_STR(DENT, "atapi_lock()\r\n");

	bzero((unchar *)&ccb, sizeof (ccb));
	ccb.ccb_drive = atap->ata_targid;
	ccb.ccb_lun = atap->ata_lun;
	ccb.ccb_data_length = 0;
	ccb.ccb_scsi_cmd[0] = SC_REMOV;
	ccb.ccb_scsi_cmd[4] = lock; /* lock or unlock flag */

	return (atapi_do_cmd(atap, atap->ata_base_port, &ccb));
}

/*
 * Check for 1.2 spec atapi device (eg cdrom)
 */
/* ARGSUSED */
int
atapi_check_for_atapi_12( ushort ioaddr1, ushort ioaddr2 )
{
	unchar	hcyl;
	unchar	lcyl;
	int	rc = FALSE;

	DEB_STR((DENT | DINIT), "atapi_check_for_atapi_12()");

	hcyl = inb(ioaddr1 + AT_HCYL);
	lcyl = inb(ioaddr1 + AT_LCYL);
	DEB_STR(DINIT, " hcyl: 0x");
	DEB_HEX(DINIT, hcyl);
	DEB_STR(DINIT, ", lcyl 0x");
	DEB_HEX(DINIT, lcyl);
	DEB_STR(DINIT, ", feat 0x");
	DEB_HEX(DINIT, inb(ioaddr1 + AT_FEATURE));
	DEB_STR(DINIT, ", count 0x");
	DEB_HEX(DINIT, inb(ioaddr1 + AT_COUNT));
	DEB_STR(DINIT, ", sec 0x");
	DEB_HEX(DINIT, inb(ioaddr1 + AT_SECT));
	DEB_STR(DINIT, ", drvhd 0x");
	DEB_HEX(DINIT, inb(ioaddr1 + AT_DRVHD));
	DEB_STR((DENT | DINIT), "\r\n");
	ata_pause();

	if ((hcyl == ATAPI_SIG_HI) && (lcyl == ATAPI_SIG_LO))
		rc = TRUE;


	/*
	 * The following is a little bit of bullet proofing. 
	 *
	 * When some drives are configured on a master-only bus they
	 * "shadow" their registers for the not-present slave drive.
	 * This is bogus and if you're not careful it may cause a
	 * master-only drive to be mistakenly recognized as both
	 * master and slave. By clearing the signature registers here
	 * I can make certain that when finddev() switches from
	 * the master to slave drive that I'll read back non-signature
	 * values regardless of whether the master-only drive does
	 * the "shadow" register trick. This prevents a bogus
	 * IDENTIFY PACKET DEVICE command from being issued which
	 * a really bogus master-only drive will return "shadow"
	 * data for.
	 */

	outb(ioaddr1 + AT_HCYL, 0);
	outb(ioaddr1 + AT_LCYL, 0);

	return (rc);
}


static int
atapi_do_cmd( ata_info_t *atap, ushort ioaddr1, ccb_t *ccbp )
{
	ushort	ioaddr2 = ioaddr1 + AT_IOADDR2;
	ushort	watchdog;
	unchar	status;

	DEB_STR(DENT, "atapi_do_cmd()\r\n");

	/*
	 * Send the PACKET command to the device
	 */
	atapi_fsm(atap, ioaddr1, ioaddr2, ATAPI_FSM_START, ccbp, 0);

	watchdog = 0;
	for (;;)  {
		if (watchdog++ == 10000) {
			DPRINTF(DERR, ("atapi_do_cmd: watchdog expired: "));
			atapi_fsm(atap, ioaddr1, ioaddr2, ATAPI_FSM_TIMEOUT,
				  ccbp, 0);
			break;
		}

		if (atap->ata_bogus_bsy) {
			/*
			 * Workaround for old drives that don't
			 * manage the busy bit and command block register
			 * reads correctly.
			 */
			ata_poll1(ioaddr2, ATS_BSY, 0,
				  ((atap->ata_drq_delay + 999) / 1000));
		}

		/* Wait for the next state */
		if (!ata_poll1(ioaddr2, 0, ATS_BSY, 30000)) {
			DPRINTF(DERR, ("atapi_do_cmd: stuck busy: "));
			atapi_fsm(atap, ioaddr1, ioaddr2, ATAPI_FSM_TIMEOUT,
				  ccbp, 0);
			break;
		}

		/* wait for the DRQ bit to settle */
		ATA_DELAY_400NSEC(ioaddr2);

		/*
		 * This clears the hardware interrupt signal. some
		 * non-compliant drives get confused if the status is read
		 * more than once per state change.
		 */
		status = inb(ioaddr1 + AT_STATUS);

		/*
		 * some non-compliant (i.e., NEC) drives don't
		 * set ATS_BSY within 400 nsec. and/or don't keep
		 * it asserted until they're actually non-busy.
		 * There's a small window between calling ata_poll1()
		 * and the inb() above where the drive might "bounce"
		 * the ATS_BSY bit.
		 */
		if (status & ATS_BSY)
			continue;

		if (atapi_fsm(atap, ioaddr1, ioaddr2, ATAPI_FSM_INTERRUPT,
			      ccbp, status)) {
			break;
		}
	}

	return (ccbp->ccb_rc);
}


static void
atapi_start_cmd(	ata_info_t	*atap,
			ushort		 ioaddr1,
			ushort		 ioaddr2,
			ccb_t		*ccbp)
{
	ushort xfer_cnt = ccbp->ccb_data_length;

	DEB_STR(DENT, "atapi_start_cmd()\r\n");
	ccbp->ccb_xfer_resid = xfer_cnt;
	ccbp->ccb_xferp = ccbp->ccb_data_addr;

	/*
	 * Select drive
	 */
	if (ccbp->ccb_drive == 0) {
		outb(ioaddr1 + AT_DRVHD, ATDH_DRIVE0);
	} else {
		outb(ioaddr1 + AT_DRVHD, ATDH_DRIVE1);
	}
	ATA_DELAY_400NSEC(ioaddr2);

	/*
	 * Wait for all clear.
	 */
	if (!ata_poll1(ioaddr2, 0, (ATS_DRQ | ATS_BSY), 2000)) {
		DEB_STR(DERR, "err0: not ready for task file setup: ");
		DEB_HEX(DERR, inb(ioaddr2));
		DEB_PAU(DERR, "\r\n");
	}
	/*
	 * Set up task file
	 *	- turn off interrupts
	 *	- clear dma feature bit
	 * 	- set max size of data expected
	 *	- output command
	 */
	outb(ioaddr2 + AT_DEVCTL, AT_DC3); /* turn interrupts on */
	outb(ioaddr1 + AT_FEATURE, 0); /* clear dma feature bit */
	outb(ioaddr1  + AT_COUNT, 0);	/* clear any garbage */
	outb(ioaddr1  + AT_SECT, 0);	/* clear any garbage */
	outb(ioaddr1 + AT_HCYL, (unchar) (xfer_cnt >> 8));
	outb(ioaddr1 + AT_LCYL, (unchar) (xfer_cnt & 0xff));
	outb(ioaddr1 + AT_CMD, ATC_PI_PKT);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(ioaddr2);

	/*
	 * variable delay for DRQ 
	 */
#if __not_needed_here_done_in_the_main_fsm_loop
	microseconds(atap->ata_drq_delay);
#endif

	return;
}


static void
atapi_send_cdb(	ata_info_t	*atap,
		ushort		 ioaddr1,
		ushort		 ioaddr2,
		ccb_t		*ccbp )
{

	DEB_STR(DIO, "atapi_send_cdb:\r\n");
	/*
	 * Send the 12 or 16 byte CDB to the device
	 */

	repoutsw(ioaddr1 + AT_DATA, (ushort *) ccbp->ccb_scsi_cmd,
		 (atap->ata_cdb16 ? (16 >> 1) : (12 >> 1)));

	if (atap->ata_single_sector && !atap->ata_bogus_bsy) {
		/*
		 * Drives that can only handle a single sector
		 * at a time also seem to take more than 400 nsec
		 * for the busy bit to assert after sending the
		 * CDB (e.g. the ancient Mitsumi FX001DE sometimes
		 * takes 1.2 msecs to set the busy bit). Allow 
		 * 2 msec just to be safe.
		 */
		ata_poll1(ioaddr2, ATS_BSY, 0, 2000);
	} else {

		/* wait for the busy bit to assert */
		ATA_DELAY_400NSEC(ioaddr2);
	}

	return;
}

static void
atapi_pio_data_out(	ata_info_t	*atap,
			ushort		 ioaddr1,
			ushort		 ioaddr2,
			ccb_t		*ccbp )
{
	/* pio data out, not supported */
	DEB_STR(DERR, "atapi_pio_data_out: data out: ");
	atapi_fatal_error(atap, ioaddr1, ioaddr2);
#ifdef DEBUG
	ata_pause();
#endif
	return;

}
static void
atapi_pio_data_in(	ata_info_t	*atap,
			ushort		 ioaddr1,
			ushort		 ioaddr2,
			ccb_t		*ccbp )
{
	ushort	drive_bytes;
	int	xfer_bytes;
	int	xfer_words;

	DEB_STR(DIO, "atapi_pio_data_in: data in");

	/*
	 * Get the device's byte count for this transfer
	 */
	drive_bytes = (ushort)inb(ioaddr1 + AT_HCYL) << 8
			    | inb(ioaddr1 + AT_LCYL);

	/*
	 * Determine actual number I'm going to transfer. My
	 * buffer might have fewer bytes than what the device
	 * expects or handles on each interrupt.
	 *
	 * xfer_bytes = min(ccbp->ccb_xfer_resid, drive_bytes);
	 *
	 */
	if (ccbp->ccb_xfer_resid < drive_bytes)
		xfer_bytes = ccbp->ccb_xfer_resid;
	else
		xfer_bytes = drive_bytes;

	DEB_STR(DIO, "atapi_pio_data_in: resid: 0x");
	DEB_HEX(DIO, ccbp->ccb_xfer_resid);
	DEB_STR(DIO, " drive_bytes: 0x");
	DEB_HEX(DIO, drive_bytes);
	DEB_STR(DIO, "\r\n");
	/*
	 * Round down my transfer count to whole words so that
	 * if the transfer count is odd it's still handled correctly.
	 */
	xfer_words = xfer_bytes / 2;

	if (xfer_words) {
		int	byte_count = xfer_words * 2;


		readphys(ccbp->ccb_xferp, ioaddr1 + AT_DATA, xfer_words);

		ccbp->ccb_xferp += byte_count;
		drive_bytes -= byte_count;
	}

	/*
	 * Handle possible odd byte at end. Read a 16-bit
	 * word but discard the high-order byte.
	 */
	if (xfer_bytes & 1) {
		ushort	tmp_word;

		tmp_word = inw(ioaddr1 + AT_DATA);
		*ccbp->ccb_xferp++ = tmp_word & 0xff;
		drive_bytes -= 2;
	}

	ccbp->ccb_xfer_resid -= xfer_bytes;

	/*
	 * Discard any unwanted data.
	 */
	while (drive_bytes >= 2) {
		inw(ioaddr1 + AT_DATA);
		drive_bytes -= 2;
	}

	/*
	 * discard the last byte if the count was odd
	 */
	if (drive_bytes)
		inw(ioaddr1 + AT_DATA);

	/* wait for the busy bit to settle */
	ATA_DELAY_400NSEC(ioaddr2);;

	if (ccbp->ccb_xfer_resid == 0) {
		/* wait for DRQ to clear */
		DEB_STR(DIO, "atapi_pio_data_in: DRQ wait\r\n");
		ata_poll1(ioaddr2, 0, ATS_DRQ | ATS_BSY, 5000);
	}

	return;
}

static void
atapi_status(	ata_info_t	*atap,
		ushort		 ioaddr1,
		ushort		 ioaddr2,
		ccb_t		*ccbp,
		unchar		 status )
{

	/* final status */
	DEB_STR(DIO, "atapi_status: status ");
	if ((status & (ATS_DF | ATS_ERR)) == 0) {
		ccbp->ccb_rc = ATA_IO_OKAY;
		ccbp->ccb_skey = 0;
		return;
	}
	ccbp->ccb_rc = ATA_CHECK_CONDITION;
	ccbp->ccb_skey = inb(ioaddr1 + AT_ERROR);

	return;
}

void
atapi_cmd_error(	ata_info_t	*atap,
			ushort		 ioaddr1 )
{
	ushort	ioaddr2 = ioaddr1 + AT_IOADDR2;

	DEB_STR(DERR, "failed cmd: ");
	DEB_HEX(DERR, ccb.ccb_scsi_cmd[0]);
	DEB_STR(DERR, " data_length ");
	DEB_HEX(DERR, ccb.ccb_data_length);
	DEB_STR(DERR, " data_left ");
	DEB_HEX(DERR, ccb.ccb_xfer_resid);
	DEB_STR(DERR, " status ");
	DEB_HEX(DERR, inb(ioaddr2));
	DEB_STR(DERR, " intr ");
	DEB_HEX(DERR, inb(ioaddr1 + AT_COUNT));
	DEB_STR(DERR, " error ");
	DEB_HEX(DERR, ccb.ccb_skey);
	DEB_STR(DERR, "\r\n");

	return;
}

static void
atapi_fatal_error(	ata_info_t	*atap,
			ushort		 ioaddr1,
			ushort		 ioaddr2 )
{
	DEB_STR(DERR, "cmd: ");
	DEB_HEX(DERR, ccb.ccb_scsi_cmd[0]);
	DEB_STR(DERR, " data_length ");
	DEB_HEX(DERR, ccb.ccb_data_length);
	DEB_STR(DERR, " data_left ");
	DEB_HEX(DERR, ccb.ccb_xfer_resid);
	DEB_STR(DERR, " ioaddr1 ");
	DEB_HEX(DERR, ioaddr1);
	DEB_STR(DERR, " status ");
	DEB_HEX(DERR, inb(ioaddr2));
	DEB_STR(DERR, " intr ");
	DEB_HEX(DERR, inb(ioaddr1 + AT_COUNT));
	DEB_STR(DERR, " error ");
	DEB_HEX(DERR, inb(ioaddr1 + AT_ERROR));
	DEB_STR(DERR, " drvhd ");
	DEB_HEX(DERR, inb(ioaddr1 + AT_DRVHD));
	DEB_STR(DERR, " cylhi ");
	DEB_HEX(DERR, inb(ioaddr1 + AT_HCYL));
	DEB_STR(DERR, " cyllow ");
	DEB_HEX(DERR, inb(ioaddr1 + AT_LCYL));
	DEB_STR(DERR, "\r\n");

	ata_pause();

	return;
}

/*
 * Finite State Machine for ATAPI PIO interrupt handler
 *
 *  IO  CoD  DRQ
 *  --  ---  ---
 *   0    0    0  == 0 invalid
 *   0    0    1  == 1 Data to device
 *   0    1    0  == 2 Idle
 *   0    1    1  == 3 Send ATAPI CDB to device
 *   1    0    0  == 4 invalid
 *   1    0    1  == 5 Data from device
 *   1    1    0  == 6 Status ready
 *   1    1    1  == 7 Future use
 *
 */

/*
 * Given the current state and the current event this
 * table determines what action to take. Note, in the actual
 * table I've left room for the invalid event codes: 0, 2, and 7.
 *
 *		+-----------------------------------------------------
 *		|		Current Event 
 *		|			
 *	State	|	dataout	idle	cdb	datain	status	
 *		|	1	2	3	5	6
 *		|-----------------------------------------------------
 *	idle	|	sendcmd	sendcmd	sendcmd	sendcmd	sendcmd
 *	cmd	|	*	 *	sendcdb	*	read-err-code
 *	cdb	|	xfer-out nada	nada	xfer-in read-err-code	
 *	datain	|	*	 *	*	xfer-in	read-err-code
 *	dataout	|	xfer-out *	*	*	read-err-code
 *
 */

unchar	atapi_PioAction[ATAPI_NSTATES][ATAPI_NEVENTS] = {
/*		invalid	dataout	idle	cdb	invalid	datain	status	future*/
/* idle */	{A_NADA,A_NADA,	A_NADA,	A_NADA,	A_NADA,	A_NADA,	A_NADA,	A_NADA},
/* cmd */	{0,	0,	0,	A_CDB,	0,	0,	A_RE,	0 },
/* cdb */	{A_REX,	A_OUT,	A_IDLE,	A_NADA,	A_IDLE,	A_IN,	A_RE,	0 },
/* data-in */	{A_REX,	0,	A_IDLE,	0,	A_NADA,	A_IN,	A_RE,	0 },
/* the data-out state isn't used in the realmode driver: */
/* data-out */	{A_REX,	A_OUT,	A_IDLE,	0,	A_NADA,	0,	A_RE,	0 }
};

/*
 *
 * Give the current state and the current event this table
 * determines the new state of the device.
 *
 *		+----------------------------------------------
 *		|		Current Event 
 *		|			
 *	State	|	dataout	idle	cdb	datain	status
 *		|----------------------------------------------
 *	idle	|	cmd	cmd	cmd	cmd	cmd
 *	cmd	|	*	*	cdb	*	*
 *	cdb	|	dataout	cdb	cdb	datain	(idle)
 *	datain	|	*	*	*	datain	(idle)
 *	dataout	|	dataout	*	*	*	(idle)
 *
 *
 * Note: the states enclosed in parens "(state)", are the accept states
 * for this FSM. A separate table is used to encode the done
 * states rather than extra state codes.
 *
 */

unchar	atapi_PioNextState[ATAPI_NSTATES][ATAPI_NEVENTS] = {
/*		invalid	dataout	idle	cdb	invalid	datain	status	future*/
/* idle */	{S_IDLE,S_IDLE,	S_IDLE,	S_IDLE,	S_IDLE,	S_IDLE,	S_IDLE,	S_IDLE},
/* cmd */	{0,	0,	0,	S_CDB,	0,	0,	0,	0 },
/* cdb */	{S_IDLE, 0,	S_CDB,	S_CDB,	S_CDB,	S_IN,	S_IDLE,	0 },
/* data-in */	{S_IDLE, 0,	S_IN,	0,	S_IN,	S_IN,	S_IDLE,	0 },
/* the data-out state isn't used in the realmode driver: */
/* data-out */	{S_IDLE, S_OUT,	S_OUT,	0,	S_OUT,	0,	S_IDLE,	0 }
};

static int
atapi_fsm(	ata_info_t	*atap,
		ushort		 ioaddr1,
		ushort		 ioaddr2,
		unchar		 func,
		ccb_t		*ccbp,
		unchar		 status )
{
	unchar	intr_reason;
	unchar	event;
	unchar	state;
	unchar	action;
	int	rc;

	if (func == ATAPI_FSM_START) {
		/*
		 * output the command byte
		 */
		atap->ata_state = S_CMD;
		atapi_start_cmd(atap, ioaddr1, ioaddr2, ccbp);
		return (FALSE);
	}

	if (func == ATAPI_FSM_TIMEOUT) {
		atap->ata_state = S_IDLE;
		atapi_fatal_error(atap, ioaddr1, ioaddr2);
		ccbp->ccb_rc = ATA_FATAL_ERROR;
		return (FALSE);
	}

	/* else it's an interrupt */

	/*
	 * get the prior state
	 */
	state = atap->ata_state;

	/*
	 * get the interrupt reason code
	 */
	intr_reason = inb(ioaddr1 + AT_COUNT);

	if (atap->ata_bogus_bsy) {
		/*
		 * some old NEC drives seem to randomly busy out the
		 * command block registers at any old time and return the
		 * status register contents (with the busy bit set) rather
		 * than the register I'm attempting to read. 
		 */
		if (intr_reason & ATS_BSY) {
			DPRINTF(DIO, ("atapi_fsm: bogus bsy 0x%x 0x%x\r\n",
					intr_reason, status));
			return (FALSE);
		}
	}

	/*
	 * encode the status and interrupt reason bits
	 * into an event code which is used to index the
	 * FSM tables
	 */
	event = ATAPI_EVENT(status, intr_reason);

	/*
	 * determine the action for this event
	 */
	action = atapi_PioAction[state][event];

	/*
	 * determine the new state
	 */
	atap->ata_state = atapi_PioNextState[state][event];

	switch (action) {
	default:
	case 0:
		DPRINTF(DERR, ("atapi_fsm: protocol error: "));
		DPRINTF(DERR, ("old state %d event %d\r\n", state, event));
		atapi_fatal_error(atap, ioaddr1, ioaddr2);
		milliseconds(1);
		break;

	case A_NADA:
		/*
		 * do nothing
		 */
		milliseconds(1);
		break;

	case A_CDB:
		/*
		 * send out atapi pkt
		 */
		atapi_send_cdb(atap, ioaddr1, ioaddr2, ccbp);
		break;


	case A_IN:
		/*
		 * read in the data
		 */
		atapi_pio_data_in(atap, ioaddr1, ioaddr2, ccbp);
		break;

	case A_OUT:
		/*
		 * send out data
		 */
		atapi_pio_data_out(atap, ioaddr1, ioaddr2, ccbp);
		break;

	case A_IDLE:
		/*
		 * The DRQ bit deasserted before or between the data
		 * transfer phases.
		 */
		if (!atap->ata_bogus_drq) {
			/*
			 * This isn't supposed to happen, report it once
			 * and then turn on the .5 second delay (which
			 * will really slow things down, but that's
			 * what you get for buying a non-compliant
			 * drive).
			 */
			atap->ata_bogus_drq = TRUE;
			DPRINTF(DERR, ("atapi_fsm: protocol error: "));
			DPRINTF(DERR, ("old state %d event %d\r\n",
					state, event));
			atapi_fatal_error(atap, ioaddr1, ioaddr2);
		}
		/*
		 * Give the drive 500 msecs to recover, it's
		 * probably processing a random seek which
		 * takes a looonnng time on some drives (I've
		 * seen some 6X drives which take 350+ msec
		 * to do each random seek so its average
		 * thruput just plain sucks).
		 */
		milliseconds(500);
		break;

	case A_RE:
		/*
		 * If we get here, a command has completed!
		 * 
		 * check status of completed command
		 */
		atapi_status(atap, ioaddr1, ioaddr2, ccbp, status);
		return (TRUE);

	case A_REX:
		/*
		 * some NEC drives don't report the right interrupt
		 * reason code for the status phase
		 */
		if (!atap->ata_nec_bad_status) {
			atap->ata_nec_bad_status = TRUE;
			DPRINTF(DERR, ("atapi_fsm: protocol error: "));
			DPRINTF(DERR, ("old state %d event %d\r\n",
					state, event));
			atapi_fatal_error(atap, ioaddr1, ioaddr2);
		}
		/* treat like a valid status phase */
		atapi_status(atap, ioaddr1, ioaddr2, ccbp, status);
		return (TRUE);

	}
	return (FALSE);
}

int
atapi_mode_sense(	ata_info_t	*atap,
			unchar		 page_code,
			unchar far	*mbuf,
			ushort		 msize )
{
	ushort	base_port = atap->ata_base_port;
	ushort	targid = atap->ata_targid;
	ushort	lun = atap->ata_lun;
	ushort	temp;
	int	rc;
	int	retries;
	int	skey;

	DEB_STR(DENT, "atapi_mode_sense()\r\n");

	/*
	 * and set extra long delay just for this one mode sense command
	 */
	temp = atap->ata_drq_delay;
	atap->ata_drq_delay = 60000;

	for (retries = 4; retries != 0; retries--) {
		bzero((unchar *)&ccb, sizeof (ccb));
		ccb.ccb_drive = targid;
		ccb.ccb_lun = lun;
		ccb.ccb_data_length = msize;
		ccb.ccb_data_addr =  mbuf;
		ccb.ccb_scsi_cmd[0] = 0x5a;
		ccb.ccb_scsi_cmd[2] = page_code;
		ccb.ccb_scsi_cmd[7] = (unchar)(msize >> 8);
		ccb.ccb_scsi_cmd[8] = (unchar)msize;

		if ((rc = atapi_do_cmd(atap, base_port, &ccb)) == ATA_IO_OKAY)
			break;

		/*
		 * Bail out if okay status or fatal error.
		 */
		if (rc != ATA_CHECK_CONDITION)
			break;
		
		/*
		 * Grab the pending Request Sense status and then retry
		 * the Mode Sense cdb.
		 */
		skey = atapi_sense(atap, base_port, targid, lun);
		if (skey == ATAPI_REQSENSE_FAILED)
			break;

		/*
		 * Bail out if drive doesn't support Mode Sense cdb.
		 */
		if (skey == 5)
			break;
	}

#ifdef DEBUG
	if (rc == ATA_IO_OKAY) {
		unchar far *cp = mbuf;
		ulong i;

		DEB_STR(DINIT, "\r\natapi_mode_sense data: < ");
		for (i = 0; i < msize; i++) {
			DPRINTF(DINIT, ("%.2x ",*cp));
			cp++;
		}
		DEB_STR(DINIT, ">\r\n");
	} else {
		DPRINTF(DINIT, ("atapi_mode_sense: ",
				 (atapi_cmd_error(atap, base_port), 0)));
	}
#endif

	/*
	 * restore the original CMD DRQ delay value
	 */
	atap->ata_drq_delay = temp;
	return (rc); 
}



static void
atapi_set_fallback_mode( ata_info_t *atap, int enable )
{
	if (enable) {
		atap->ata_single_sector = TRUE;
		atap->ata_bogus_bsy = TRUE;
		atap->ata_nec_bad_status = TRUE;
		atap->ata_bogus_drq = TRUE;
		atap->ata_fellback = TRUE;
	} else {
		atap->ata_single_sector = FALSE;
		atap->ata_bogus_bsy = FALSE;
		atap->ata_nec_bad_status = FALSE;
		atap->ata_bogus_drq = FALSE;
		atap->ata_fellback = FALSE;
	}
	return;
}

void
atapi_chk_for_lame_drive( ata_info_t *atap, struct ata_id *aidp)
{
	ushort	temp;
	char	*rate;

#define	MODE_SPEED_OFFSET ((uint)(&((struct mbuf *)0)->mode_max_speed_msb))
	
	DPRINTF(DENT, ("ata_mode_sense "));

	/*
	 * Enable fallback mode. All the known ATAPI non-compliance
	 * workarounds are enabled until we're certain the drive
	 * behaves reasonably.
	 */
	atapi_set_fallback_mode(atap, TRUE);

	/*
	 * retrieve the mode sense CDROM Capabilities, current page
	 */
	if (atapi_mode_sense(atap, 0x2a, (unchar far *)&mbuf, sizeof mbuf)
				!= 0){
		/* MODE SENSE failed, leave fallback mode enabled */
		DPRINTF(DINIT, ("ata_mode_sense failed "));
		return;
	}

	/*
	 * make certain the drive returned enough bytes
	 */
	temp = ((mbuf.mode_header[0] << 8) | mbuf.mode_header[1]) + 2;
	if (temp < MODE_SPEED_OFFSET + 2) {
		/* MODE SENSE data invalid, leave fallback mode enabled */
		DPRINTF(DINIT, ("ata_mode_sense short response (A) "));
		return;
	}
	if (mbuf.mode_len + 1 < MODE_SPEED_OFFSET - sizeof mbuf.mode_header) {
		DPRINTF(DINIT, ("ata_mode_sense short response (B) "));
		return;
	}


	temp = mbuf.mode_max_speed_msb << 8 | mbuf.mode_max_speed_lsb;

	/*
	 * Lets assume for now that all drives faster than 4X are
	 * ATAPI compliant. This may be over-ridden later by model
	 * specific entries in the blacklist.
	 */

	DPRINTF(DINIT, ("atapi max speed %d\r\n", temp));
	if (temp == 0 || temp > 706) {
		DPRINTF(DINIT, ("atapi 4X or faster drive\r\n"));
		atapi_set_fallback_mode(atap, FALSE);
	}
	return;

	if ((temp == 706) && (aidp->ai_cap & ATAC_DMA_SUPPORT)) {
		DPRINTF(DINIT, ("atapi 4X with DMA\r\n"));
		atapi_set_fallback_mode(atap, FALSE);
	}
	return;

	/*
	 * Because of the numerous bogus older drives, I'm going to
	 * flatly assume that all slow drives are brain-dead and leave
	 * fallback mode enabled. This means all the bug workarounds
	 * are enabled. If some slow drive is specifically know not
	 * to be broken or be only partially broken then stick a
	 * model specific entry in the blacklist.
	 */

	/*
	 * map the KB rate into its 1X, 2X, 2.2X, or 3X rate
	 */
	DPRINTF(DINIT, ("ata fallback mode for %s drive\r\n", 
			(temp < 353) ? "1X" :
			((temp < 387) ? "2X" :
			((temp < 528) ? "2.2X" :
			((temp < 706) ? "3X" : "4X"))) ));
	return;
}
