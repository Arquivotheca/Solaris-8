/*
 * Copyright (c) 1998 by Sun Microsystems, Inc
 * All rights reserved.
 */


#ident "@(#)main.c   1.37   98/05/14 SMI"

/*
 * Solaris Primary Boot Subsystem - BIOS Extension Driver
 * =========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name: AT Attachment (ATA) or IDE cdrom driver (ata.c)
 *
 * This file is the initail entry point, and determines which disks (ata.c)
 * or cd-roms (atapi.c) are present.
 * It also defines common routines and data.
 */

/* #define DEBUG /* */

#include <types.h>
#include <bef.h>
#include <common.h>
#include "..\scsi.h"
#include <dev_info.h>
#include <befext.h>
#include <stdio.h>
#include "ata.h"
#include "ataconf.h"

#define	BUFSIZE 2048	/* Largest supported physical block size */

#ifndef NULL
#define	NULL	0
#endif

/* GLOBAL VARIABLES USED BY THE DRIVER FRAMEWORK */

char ident[] = "ata";


/*
 * Per drive soft-state information
 */
#define	MAXDEVS 10
ata_info_t	ata_info[MAXDEVS + 1] = {0};
int	ata_info_last = -1;

/*
 * ata_id_data is set in ata_id,
 * Its used later to do the equivalent to a scsi inquiry
 */
unchar ata_id_data[NBPSCTR] = {0};

/*
 * These definitions provide the stack for resident code.
 * They are used by low.s.  They are placed here so that the
 * stack size can be optimized for this module.
 */
#define	STACKSIZE 1000 /* Resident stack size in words */
ushort stack[STACKSIZE];
ushort stacksize = STACKSIZE;

/*
 * Valid ata base addresses, terminated by 0.
 */
static ushort base_addr[] = {0x1f0, 0x170, 0};
static ushort base_irq[]  = {14, 15};

static	int	PciIde;			/* true if installing PCI-IDE dev */
static	DWORD	PciIdeAddr[AddrTupleSize]; /* PCI address of the PCI-IDE dev */
static	int	PciIdeCtlr;		/* ata controller within PCI-IDE dev */

/*
 * some local data
 */
static	char	 nec_260[]	= "NEC CD-ROM DRIVE";
static	char	 ls120_model[]	= "LS-120 HIMA";

unsigned int debuglevel = DERR | 0xffff;

/*
 * Local function prototypes
 */
static	void	ata_dev(ushort base_port, ushort targid, ushort lun);
static	void	atapi_dev(ushort base_port, ushort targid, ushort lun);
static	void	ata_pciide_dev(ushort base_port, ushort targid, ushort lun);
static	int	check_for_dpt(uint ioaddr1);
static	int	findcmosdrives(void);
static	int	findctl(uint ioaddr1, uint ioaddr2);
static	int	finddev(uint ioaddr1, uint ioaddr2, uint count_disks,
			uint findctl);

static ata_info_t *addr2atap(ushort base_port, ushort targid, ushort lun);
static ata_info_t *infop2atap(DEV_INFO *info);

static	int	ata_strncmp(char *p1, char *p2, int cnt);
static	void	atapi_id_dump(void);
static	void	atapi_id_swab(struct ata_id *aidp);

#pragma comment(compiler)

int
dev_init(DEV_INFO *info)
{

	return (0);
}

/* ARGSUSED */
int
init_dev(ushort base_port, ushort dataseg)
{
}

/*
 * dev_find() -- Returns number of ata or atapi devices for all controllers.
 *
 * This interface is only used on old (pre-2.6) systems. Therefore
 * we ignore ATA disk drives and only setup ATAPI CDROM drives. On a 2.6
 * system, legacyprobe() and installonly() are called instead.
 *
 */
int
dev_find()
{
	int	drives_found = 0;
	int	ctlr;
	int	ioaddr1;
	int	ioaddr2;
	int	oldspl;

	DEB_STR(DENT, "dev_find()\r\n");

	oldspl = splhi();

	for (ctlr = 0; ioaddr1 = base_addr[ctlr]; ctlr++) {

		/*
		 * Check for a conflict with a DPT adapter which can also
		 * occupy these locations.
		 * The DPT identifies itself.
		 */
		if (check_for_dpt(ioaddr1))
			continue;

		ioaddr2 = ioaddr1 + AT_IOADDR2;
		drives_found += finddev(ioaddr1, ioaddr2, FALSE, FALSE);
	}
	DEB_STR(DINIT, "dev_find: drives_found:");
	DEB_HEX(DINIT, drives_found);
	DEB_PAU(DINIT, "\r\n");

	splx(oldspl);
	return (drives_found);
}

/*
 * []----------------------------------------------------------[]
 * | legacyprobe() -- the boot hill project has a new interface |
 * | for drivers. To avoid probe conflicts the driver must check|
 * | with the caller to see if it can used an address before	|
 * | it tries to poke at it. Once it finds an adapter other	|
 * | devices will be prevent from access this devices resources	|
 * |								|
 * | Note. The primary ata controllers 2nd io address (0x3f6)	|
 * | often conflicts with the floppy controller resources	|
 * | To get around this, we attempt to set it but don't	worry	|
 * | if it fails.						|
 * []----------------------------------------------------------[]
 */
void
legacyprobe()
{
	ushort	base;
	ushort	ioaddr1;
	ushort	ioaddr2;
	DWORD	val[3];
	DWORD	len;
	int	oldspl;

	oldspl = splhi();

	for (base = 0; ioaddr1 = base_addr[base]; base++) {
		ioaddr2 = ioaddr1 + AT_IOADDR2;
		/*
		 * Check that all the rsources are available
		 */
		/* ---- set the first address for this device ---- */
		if (node_op(NODE_START) != NODE_OK) {
			splx(oldspl);
			return;
		}
		/*
		 * Note the port 0x3f7, or 0x377
		 * is no longer shared with the floppy controller
		 */
		val[0] = ioaddr1;
		val[1] = 8;
		val[2] = 0;	/* ---- no flags ---- */
		len = 3;
		if (set_res("port", val, &len, 0) != RES_OK) {
			node_op(NODE_FREE);
			continue;
		}
		val[0] = ioaddr2;
		val[1] = 1;
		val[2] = 0;	/* ---- no flags ---- */
		len = 3;
		if (set_res("port", val, &len, 0) != RES_OK) {
			node_op(NODE_FREE);
			continue;
		}
		/*
		 * Resources are available. Now test to see if the
		 * controller exists
		 */
		if (findctl(ioaddr1, ioaddr2)) {
			val[0] = base_irq[base];
			val[1] = 0;
			len = 2;
			if (set_res("irq", val, &len, 0) != RES_OK) {
				node_op(NODE_FREE);
				continue;
			}
			node_op(NODE_DONE);
		} else {
			node_op(NODE_FREE);
		}
	}

	splx(oldspl);
	return;
}

/*
 * []----------------------------------------------------------[]
 * | installonly() is part of the new bef interface for the	|
 * | boot hill project. this routine will only touch i/o address|
 * | that the parent provides via the get_res() call. everything|
 * | else is off limits to avoid probe conflicts.		|
 * []----------------------------------------------------------[]
 */
int
installonly()
{
	DWORD	port[6 * PortTupleSize];
	DWORD	bus[NameTupleSize];
	DWORD	len;
	int	Rtn = BEF_FAIL;
	uint	ioaddr1;
	uint	ioaddr2;
	int	oldspl;

	oldspl = splhi();

	DPRINTF(DINIT, ("ata installonly "));

	do {
		if (node_op(NODE_START) == NODE_FAIL) {
			DPRINTF(DINIT, (" ata installonly exit\n"));
			splx(oldspl);
			return (Rtn);
		}

		/*
		 * Second DWORD of name resource contains the bus
		 * for this board.  Fetch it and check it.
		 */
		len = NameTupleSize;
		if (get_res("name", bus, &len) != RES_OK) {
			DPRINTF(DINIT, (" name fail \n"));
			node_op(NODE_FREE);
			return (BEF_FAIL);
		}

		len = sizeof port / sizeof (DWORD);
		for (ioaddr1 = 0; ioaddr1 < len; ioaddr1++)
			port[ioaddr1] = 0;
		len = sizeof port / PortTupleSize;
		if (get_res("port", port, &len) != RES_OK) {
			DPRINTF(DINIT, (" port fail \n"));
			node_op(NODE_FREE);
			continue;
		}
		DPRINTF(DINIT, (" port len=%d [0]=0x%x", len, port[0]));

		switch (bus[1]) {
		case RES_BUS_ISA:
			DPRINTF(DINIT, (" ISA "));
			PciIde = FALSE;
			ioaddr1 = (uint) port[0 * PortTupleSize];
			ioaddr2 = (uint) port[1 * PortTupleSize];
			/* see if find some drives, if so return success. */
			if (finddev(ioaddr1, ioaddr2, TRUE, FALSE))
				Rtn = BEF_OK;
			break;

		case RES_BUS_PCI:
			DPRINTF(DINIT, (" PCI "));
			/*
			 * set flag that enables PCI-IDE stuff before
			 * calling scsi_dev()
			 */
			PciIde = TRUE;

			/* save the device's PCI bus address */
			len = AddrTupleSize;
			if (get_res("addr", PciIdeAddr, &len) != RES_OK)
				continue;
			
			DPRINTF(DINIT, (" len=%d ", len));
			/* primary controller */
			PciIdeCtlr = 0;
			ioaddr1 = (uint) port[0 * PortTupleSize];
			ioaddr2 = (uint) port[1 * PortTupleSize];
			/* see if find some drives, if so return success. */
			if (finddev(ioaddr1, ioaddr2, TRUE, FALSE))
				Rtn = BEF_OK;

			/* secondary controller */
			PciIdeCtlr = 1;
			ioaddr1 = (uint) port[2 * PortTupleSize];
			ioaddr2 = (uint) port[3 * PortTupleSize];
			/* see if find some drives, if so return success. */
			if (finddev(ioaddr1, ioaddr2, TRUE, FALSE))
				Rtn = BEF_OK;
			break;
		default:
			DPRINTF(DINIT, (" default "));
		}

		DPRINTF(DINIT, (" NODE_DONE\n"));
		node_op(NODE_DONE);
	} while (1);
}

static int
finddev(uint ioaddr1, uint ioaddr2, uint count_disks, uint findctl)
{
	int	drives_found = 0;
	uint	drive;

	/*
	 * ignore PCI-IDE devs that don't have a valid BAR
	 */
	if (ioaddr1 == 0)
		return (0);

	for (drive = 0; drive < 2; drive++) {
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

		DPRINTF(DINIT, ("finddev: ioaddr1: 0x%x drive: 0x%x ",
				ioaddr1, drive));

	step1:
		if (ata_id(ioaddr1, ioaddr2)) {
			DEB_STR(DINIT, " found ata disk\r\n");
			if (findctl)
				return (1);
			if (count_disks) {
				ata_dev(ioaddr1, drive, 0);
				drives_found++;
			}
			continue;
		}

	step2:
		if (atapi_check_for_atapi_12(ioaddr1, ioaddr2)) {
			DEB_STR(DINIT, " found 1.2 atapi sig (B) ");
			if (atapi_id(ioaddr1, ioaddr2, TRUE)) {
				/*
				 * Add device to list.
				 */
				DEB_STR(DINIT, " and 1.2 unit\r\n");
				if (findctl)
					return (1);
				atapi_dev(ioaddr1, drive, 0);
				drives_found++;
			}
			continue;
		}

	step3:
		if (atapi_id(ioaddr1, ioaddr2, FALSE)) {
			/*
			 * Add device to list.
			 */
			DEB_STR(DINIT, " found 1.7B atapi unit\r\n");
			if (findctl)
				return (1);
			atapi_dev(ioaddr1, drive, 0);
			drives_found++;
			continue;
		}
		DEB_STR(DINIT, " nothing found (B)\r\n");
	}
	return (drives_found);
}

/*
 * Find any ata (ide) controllers with devices attached
 */
static int
findctl(uint ioaddr1, uint ioaddr2)
{
	/*
	 * Just a reset to the controller can disturb the bios 0x80 driver
	 * so if the controller is the primary (0x1f0) and the cmos is enabled
	 * then return controller found.
	 */
	if ((ioaddr1 == 0x1f0) && (findcmosdrives() & 1)) {
		return (1);
	}

	/*
	 * Check for a conflict with a DPT adapter which can also
	 * occupy these locations.
	 * The DPT identifies itself.
	 */
	if (check_for_dpt(ioaddr1))
		return (0);

	return (finddev(ioaddr1, ioaddr2, TRUE, TRUE));
}

#define	HA_AUX_STATUS	0x08

/*
 * Check for a DPT adapter at our address.  if there is such a beast,
 * return 1.  If not, return 0.
 */
static int
check_for_dpt(uint ioaddr1)
{
	unchar	v;

	v = inb(ioaddr1 + HA_AUX_STATUS);
	if (v == 0xfc || v == 0xfd || v == 0xfe) {

		DEB_STR(DINIT, "ata: dpt device found at baseaddr 0x");
		DEB_HEX(DINIT, ioaddr1);
		DEB_STR(DINIT, "\r\n");

		return (1);
	}
	return (0);
}

/*
 * ata_poll()
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


void
repoutsw(ushort port, ushort *buffer, ulong count)
{
	while (count-- > 0) {
		outw(port, *buffer++);
	}
}

void
bzero(unchar *p, ulong n)
{
	for (; n > 0; n--) {
		*p++ = 0;
	}
}

#define	CMOS_ADDR 0x70
#define	CMOS_DATA 0x71
#define	CMOS_ADDR_HD_INFO 0x12

static int
findcmosdrives(void)
{
	unchar	hd_data;
	int	numcmos = 0;

	outb(CMOS_ADDR, CMOS_ADDR_HD_INFO);
	hd_data = inb(CMOS_DATA);
	if (hd_data & 0xF0)
		numcmos |= 1;
	if (hd_data & 0x0F)
		numcmos |= 2;
	return (numcmos);
}


static ata_info_t *
init_ata_info(ushort base_port, ushort targid, ushort lun, unchar isatapi)
{
	extern	ushort	 devs;
	ata_info_t	*atap;

	ata_info_last = devs;
	atap = ata_info + devs;
	atap->ata_base_port = base_port;
	atap->ata_targid = targid;
	atap->ata_lun = lun;
	atap->ata_atapi = isatapi;

	return (atap);
}

/*
 * map a drive address to its soft-state struct
 */

static ata_info_t *
addr2atap(ushort base_port, ushort targid, ushort lun)
{
	ata_info_t	*atap;
	ushort		 idx;

	for (idx = 0, atap = ata_info; idx <= ata_info_last; atap++, idx++) {
		if (atap->ata_base_port != base_port)
			continue;
		if (atap->ata_targid != targid)
			continue;
		if (atap->ata_lun != lun)
			continue;
		return (atap);
	}
	return (NULL);
}


/*
 * map a DEV_INFO ptr to the drive's soft-state struct
 */

static ata_info_t *
infop2atap(DEV_INFO *info)
{
	return (addr2atap(info->base_port, info->MDBdev.scsi.targ,
			  info->MDBdev.scsi.lun));
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
 * scan the blacklist for a matching model name if found set
 * the indicated workaround bits in the ata_info_t instance for
 * this dev.
 */
static void
ata_chk_blacklist( ata_info_t *atap, struct ata_id *aidp )
{
	bl_t	*blp;

	/*
	 * check if the drive is in my non-complaint blacklist
	 */

	for (blp = ata_blacklist; blp->b_model[0] != '\0'; blp++) {
		if (!ata_strncmp(blp->b_model, aidp->ai_model,
					       sizeof aidp->ai_model))
			continue;

		/* got a match */
		DPRINTF(DINIT, ("ata config: <%s> <", blp->b_model));

		/*
		 * force dev_read() to do all reads a single sector at
		 * a time. This seems to fix certain non-compliant drives
		 * that breakup the requests into multiple data transfers
		 * but don't set the status register correctly inbetween
		 * each partial transfer.
		 */
		if (blp->b_single_sector) {
			DPRINTF(DINIT, ("A"));
			atap->ata_single_sector = TRUE;
		}

		/*
		 * filter the unstable busy bit
		 */
		if (blp->b_bogus_bsy) {
			DPRINTF(DINIT, ("B"));
			atap->ata_bogus_bsy = TRUE;
		}

		/*
		 * accept (ATI_IO | ATI_COD) == 0 as a valid status
		 * phase for non-compliant NEC atapi drives.
		 */
		if (blp->b_nec_bad_status) {
			DPRINTF(DINIT, ("C"));
			atap->ata_nec_bad_status = TRUE;
		}

		/*
		 * the DRQ bit deasserts instead of the BUSY bit
		 * asserting 
		 */
		if (blp->b_bogus_drq) {
			DPRINTF(DINIT, ("D"));
			atap->ata_bogus_drq = TRUE;
		}

		/*
		 * if (blp->b_xxx)
		 *	atap->ata_xxx = TRUE;
		 */
		DPRINTF(DINIT, (">\r\n"));
		return;
	}
	DPRINTF(DINIT, ("no match\r\n"));

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
	ata_info_t	*atap;
	extern	ushort	 devs;


	/*
	 * swap bytes of all text fields
	 */
	if (!ata_strncmp(nec_260, aidp->ai_model, sizeof aidp->ai_model)) {
		atapi_id_swab(aidp);
	}

	atapi_id_dump();


	if (devs >= MAXDEVS) 
		return;

	DPRINTF(DINIT, (" atapi_dev "));

	/*
	 * Ignore any devices with model names starting with "LS-120 HIMA".
	 * Such devices must be accessed via the BIOS floppy
	 * drive functions, not the ata driver.
	 */
	if (ata_strncmp(ls120_model, aidp->ai_model, sizeof ls120_model - 1)) {
		DEB_STR(DINIT, "atapi_dev: found LS120 at baseaddr 0x");
		DEB_HEX(DINIT, base_port);
		DEB_STR(DINIT, "\r\n");
		return;
	}


	/*
	 * init my private soft state info structure for this device
	 */
	atap = init_ata_info(base_port, targid, lun, TRUE);

        /*
	 * Determine ATAPI CDB size
	 */
        if ((aidp->ai_config & ATAPI_ID_CFG_PKT_SZ) == ATAPI_ID_CFG_PKT_16B)
                atap->ata_cdb16 = TRUE;
	else
                atap->ata_cdb16 = FALSE;

	/*
	 * Determine command DRQ type used by the device
	 */
        switch (aidp->ai_config & ATAPI_ID_CFG_DRQ_TYPE) {
	case ATAPI_ID_CFG_DRQ_MICRO:
		/* 3 millisecs */
		atap->ata_drq_delay = 3000;
		break;
	case ATAPI_ID_CFG_DRQ_INTR:
	case ATAPI_ID_CFG_DRQ_RESV:
		/* 10 millisecs */
		atap->ata_drq_delay = 10000;
		break;
	case ATAPI_ID_CFG_DRQ_FAST:
		/* 50 microsecs */
		atap->ata_drq_delay = 50;
		break;
	}

	/*
	 * turn on fallback mode for any drive slower than 4X
	 */
	atapi_chk_for_lame_drive(atap, aidp);

	/*
	 * look for model specific blacklist entries
	 */
	ata_chk_blacklist(atap, aidp);

	/*
	 * add this drive to the boot menu
	 */
	if (PciIde)
		ata_pciide_dev(base_port, targid, lun);
	else
		scsi_dev(base_port, targid, lun);
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
	extern	ushort devs;
	ata_info_t *atap;
	struct ata_id *aidp = (struct ata_id *)ata_id_data;

	if (devs >= MAXDEVS) 
		return;

	DPRINTF(DINIT, (" ata_dev "));
	/*
	 * swap bytes of all text fields
	 */
	atapi_id_swab(aidp);

	atapi_id_dump();

	/*
	 * init my private soft state info structure for this device
	 */
	atap = init_ata_info(base_port, targid, lun, FALSE);

	/*
	 * Check if LBA mode is supported. Requires ATA-3 or newer,
	 * or ATA-2 LBA config bit set.
	 */
	if ((aidp->ai_majorversion & 0x8000) == 0
	&&   aidp->ai_majorversion >= (1 << 3)) {
		/* ATA-3 or better */
		atap->ata_lba_mode = TRUE;
	} else if (aidp->ai_cap & ATAC_LBA_SUPPORT) {
		/* ATA-2 LBA capability bit set */
		atap->ata_lba_mode = TRUE;
	} else {
		/* must use CHS mode */
		atap->ata_lba_mode = FALSE;
	}

	/*
	 * Determine the CHS geometry and/or the disk size
	 */
	if (atap->ata_lba_mode) {
		/* in LBA mode the cyls, head, secs don't matter */
		atap->ata_disksize = aidp->ai_addrsec[0];
		atap->ata_disksize |= ((ulong)aidp->ai_addrsec[1]) << 16;

	} else if (aidp->ai_validinfo & 1) {
		/* the drive has a valid CHS translation mode set */
		atap->ata_cyls = aidp->ai_curcyls;
		atap->ata_heads = aidp->ai_curheads;
		atap->ata_secs = aidp->ai_cursectrk;
		atap->ata_disksize = aidp->ai_cursccp[0];
		atap->ata_disksize |= ((ulong)aidp->ai_cursccp[1]) << 16;
	} else {
		/* try the default CHS geometry */
		atap->ata_cyls = aidp->ai_fixcyls;
		atap->ata_heads = aidp->ai_heads;
		atap->ata_secs = aidp->ai_sectors;

		/*
		 * Note: long multiples and divides are done
		 * via libarary routines
		 *
		 * compute:
		 *	atap->ata_disksize = atap->ata_cyls * atap->ata_heads
		 *				* atap->ata_secs;
		 */
		atap->ata_disksize = ULongMult((ulong)atap->ata_cyls,
				     ULongMult((ulong)atap->ata_heads,
					       (ulong)atap->ata_secs));
	}

	ata_chk_blacklist(atap, aidp);

	if (PciIde)
		ata_pciide_dev(base_port, targid, lun);
	else
		scsi_dev(base_port, targid, lun);
	scsi_dev_info()->misc_flags |= MDB_MFL_DIRECT;
	return;
}

/*
 *
 * If this is a PCI-IDE device, then the bootpath has to be set
 * up like one of the following:
 *
 *		.../pci-ide@<dev>,<func>/ata@<ctrl>/cmdk@<targ>,<lun>
 *	
 * or,
 *
 *		.../pci-ide@<dev>/ata@<ctrl>/cmdk@<targ>,<lun>
 *
 * where <ctlr> is either 0 or 1.
 *
 * If the user_bootpath option wasn't used the bootpath would just
 * be something like this:
 *
 *		.../pciVVVV,DDDD@<dev>,<func>/cmdk@<targ>,<lun>
 *
 * Such a bootpath wouldn't work for dual ata controller PCI-IDE devices.
 *
 * This routine supplies the portion before "/cmdk..." which designates
 * which PCI-IDE device and which ata controller within that device
 * is the controller for the boot disk. The "/cmdk..." portion is
 * filed in by the framework.
 *
 */

static void
ata_pciide_dev(ushort base_port, ushort targid, ushort lun)
{
	extern	ushort devs;
	extern	DEV_INFO dev_info[];
	ushort	pciID = PciIdeAddr[0];
	unchar	bus = (pciID >> 8) & 0xff;
	unchar	device = (pciID >> 3) & 0x1f;
	unchar	func = pciID & 0x3;
	char	*dstp;

	DPRINTF(DINIT, (" ata_pciide_dev "));

	/*
	 * note: no leading "/", if a leading slash is used it becomes
	 * an abosolute path rather than a relative path.
	 *
	 */

	dstp = dev_info[devs].user_bootpath;

#ifdef NO_SPRINTF_AVAILABLE
	{
		char	*srcp;
		static char *hexmap = "0123456789abcdef";

		srcp = "pci-ide@";
		while (*srcp)
			*dstp++ = *srcp++;

		if (device >= 0x10)
			*dstp++ = hexmap[device >> 4];
		*dstp++ = hexmap[device & 0xf];

		if (func) {
			*dstp++ = ',';
			*dstp++ = hexmap[func & 0xf];
		}

		srcp= "/ide@";
		while (*srcp)
			*dstp++ = *srcp++;
		
		*dstp++ = hexmap[PciIdeCtlr];
		*dstp++ = '\0';
	}
#else
	if (func)
		sprintf(dstp, "pci-ide@%x,%x/ide@%d", device, func, PciIdeCtlr);
	else
		sprintf(dstp, "pci-ide@%x/ide@%d", device, PciIdeCtlr);
#endif

	DPRINTF(DINIT, (" %s\n", dev_info[devs].user_bootpath));

	/*
	 * there needs to be a variant of scsi_dev_pci() that doesn't
	 * require the vendorid and deviceid
	 */
	scsi_dev_pci(base_port, targid, lun, bus, 0xdead, 0xbeaf, device, func);
	return;
}


int
dev_read(DEV_INFO *info, ulong start_block, ushort count,
	ushort bufoff, ushort bufseg)
{
	ata_info_t *atap = infop2atap(info);
	unchar far *bufaddr = MK_FP(bufseg, bufoff);
	int	oldspl;
	int	rc;
	ushort	cnt;
	ushort	cnt_incr;
	ulong	byte_incr;


	if (atap == NULL)
		return (-1);

	oldspl = splhi();

	if (atap->ata_single_sector) {
		cnt_incr = 1;
		byte_incr = info->MDBdev.scsi.bsize;
	} else {
		cnt_incr = count;
		byte_incr = 0;
	}

	for (cnt = 0; cnt < count; cnt += cnt_incr) {
		if (atap->ata_atapi) {
			rc = atapi_read(atap, start_block, cnt_incr,
					info->MDBdev.scsi.bsize, bufaddr);
			/*
			 * If Check Condition, clear the pending
			 * request sense data. The framework should
			 * retry the read.
			 */
			if (rc == ATA_CHECK_CONDITION) {
				atapi_sense(atap, atap->ata_base_port,
					    atap->ata_targid, atap->ata_lun);
			}
		} else {
			rc = ata_read(atap, start_block, cnt_incr,
				      info->MDBdev.scsi.bsize, bufaddr);
		}
		if (rc != ATA_IO_OKAY)
			break;
		bufaddr += byte_incr;
		start_block += cnt_incr;
	}

	splx(oldspl);
	return (rc);

}

int
dev_sense(ushort base_port, ushort targid, ushort lun)
{
	ata_info_t *atap = addr2atap(base_port, targid, lun);
	int	oldspl;
	int	rc;

	oldspl = splhi();

	if (atap == NULL) {
		rc = -1;
	} else if (atap->ata_atapi) {
		rc = atapi_sense(atap, base_port, targid, lun);
	} else {
		rc = ata_sense(atap, base_port, targid, lun);
	}

	splx(oldspl);
	return (rc);
}

int
dev_lock(register DEV_INFO *info, int lock)
{
	ata_info_t *atap = infop2atap(info);
	int	retries;
	int	skey;
	int	oldspl;
	int	rc;

	oldspl = splhi();

	if (atap == NULL) {
		rc = -1;
		goto done;
	}

	for (retries = 4; retries != 0; retries--) {
		if (atap->ata_atapi) {
			rc = atapi_lock(atap, lock);
		} else {
			rc = ata_lock(atap, lock);
		}

		/* bail out if it's not Check Condition */
		if (rc != ATA_CHECK_CONDITION)
			break;

		/* get the sense data */
		skey = dev_sense(atap->ata_base_port, atap->ata_targid,
				 atap->ata_lun);

		/*
		 * Retry after Check Condition if no sense data or
		 * if it was just a Unit Attention.
		 */
		if (skey != 0 && skey != 6) {
			break;
		}
	}

done:
	splx(oldspl);
	return (rc);
}

int
dev_motor(register DEV_INFO *info, int on)
{
	ata_info_t *atap = infop2atap(info);
	int	retries;
	int	skey;
	int	oldspl;
	int	rc;

	oldspl = splhi();

	if (atap == NULL) {
		rc = -1;
		goto done;
	}

	for (retries = 4; retries != 0; retries--) {
		if (atap->ata_atapi) {
			rc = atapi_motor(atap, on);
		} else {
			rc = ata_motor(atap, on);
		}

		/* bail out if it's not Check Condition */
		if (rc != ATA_CHECK_CONDITION)
			break;

		/* get the sense data */
		skey = dev_sense(atap->ata_base_port, atap->ata_targid,
				 atap->ata_lun);

		/*
		 * Retry after Check Condition if no sense data or
		 * if it was just a Unit Attention.
		 */
		if (skey != 0 && skey != 6) {
			break;
		}
	}

done:
	splx(oldspl);
	return (rc);
}



int
dev_inquire(ushort base_port, ushort targid, ushort lun)
{
	ata_info_t *atap = addr2atap(base_port, targid, lun);
	int	retries;
	int	skey;
	int	oldspl;
	int	rc;

	oldspl = splhi();

	if (atap == NULL) {
		rc = -1;
		goto done;
	}

	for (retries = 4; retries != 0; retries--) {
		if (atap->ata_atapi) {
			rc = atapi_inquire(atap, base_port, targid, lun);
		} else {
			rc = ata_inquire(atap, base_port, targid, lun);
		}

		/* bail out if it's not Check Condition */
		if (rc != ATA_CHECK_CONDITION)
			break;

		/* get the sense data */
		skey = dev_sense(atap->ata_base_port, atap->ata_targid,
				 atap->ata_lun);

		/*
		 * This should never happen for a INQUIRE command
		 * but do something reasonable if it does.
		 *
		 * Retry after Check Condition if no sense data or
		 * if it was just a Unit Attention.
		 */
		if (skey != 0 && skey != 6) {
			break;
		}
	}

done:
	splx(oldspl);
	return (rc);
}

int
dev_readcap(register DEV_INFO *info)
{
	ata_info_t *atap = infop2atap(info);
	int	retries;
	int	skey;
	int	oldspl;
	int	rc;

	oldspl = splhi();

	if (atap == NULL) {
		rc = -1;
		goto done;
	}

	for (retries = 4; retries != 0; retries--) {
		if (atap->ata_atapi) {
			rc = atapi_readcap(atap);
		} else {
			rc = ata_readcap(atap);
		}

		/* bail out if it's not Check Condition */
		if (rc != ATA_CHECK_CONDITION)
			break;

		DPRINTF(DINIT, ("dev_readcap: check condition "));

		/* get the sense data */
		skey = dev_sense(atap->ata_base_port, atap->ata_targid,
				 atap->ata_lun);

		/*
		 * Retry after Check Condition if no sense data or
		 * if it was just a Unit Attention.
		 */
		if (skey != 0 && skey != 6) {
			break;
		}
	}

	if (rc != ATA_IO_OKAY) {
		/*
		 * print a useful error message
		 */
		if (debuglevel & DINIT) {
			DPRINTF(DINIT, ("dev_readcap: "));
			if (atap->ata_atapi) {
				atapi_cmd_error(atap, atap->ata_base_port);
			} else {
				/* this shouldn't ever happen */
				DPRINTF(DINIT, (" failed: 0x%x\r\n",
						atap->ata_base_port));
			}
		}
	}

done:
	splx(oldspl);
	return (rc);
}


#ifdef __not_yet__
exec_diag(uint ioaddr1, uint ioaddr2)
{
	int	time_limit = 0;
	unchar	drv0;
	unchar	drv1;
	unchar	diag_code;

	outb(ioaddr1 + AT_CMD, 0x90);
	ATA_DELAY_400NSEC(ioaddr2);

	while (1) {
		if (!(inb(ioaddr1 + AT_STATUS) & ATS_BSY))
			break;

		if (time_limit++ == 5000) {
			drv0 = FALSE;
			goto chk_drive1;
		}
		milliseconds(1);
	}

	diag_code = inb(ioaddr1 + AT_ERROR);
	if (diag_code == 0x01 || diag_code == 0x81)
		drv0 = TRUE;
	else
		drv0 = FALSE;

chk_drive1:

	while (1) {
		outb(ioaddr1 + AT_DRVHD, ATDH_DRIVE1);
		ATA_DELAY_400NSEC(ioaddr2);

		if (!(inb(ioaddr1 + AT_STATUS) & ATS_BSY))
			break;

		if (time_limit++ == 6000) {
			drv1 = FALSE;
			goto done;
		}
		milliseconds(1);
	}

	diag_code = inb(ioaddr1 + AT_ERROR);
	if (diag_code == 0x01 || diag_code == 0x81)
		drv1 = TRUE;
	else
		drv1 = FALSE;


done:
	return (drv0 + drv1);
}

/*
 * wait for DRDY 
 */
wait_for_drdy(uint ioaddr1, uint ioaddr2, int drive, int *timep)
{
	unchar	status;


	if (drive == 0)
		outb(ioaddr1 + AT_DRVHD, ATDH_DRIVE0);
	else
		outb(ioaddr1 + AT_DRVHD, ATDH_DRIVE1);
	ATA_DELAY_400NSEC(ioaddr2);

	while (1) {
		status = inb(ioaddr1 + AT_STATUS);
		if (status & ATS_DRDY)
			return (TRUE);

		if (*timep == 0) {
			return (FALSE);
		}
		milliseconds(1);
		*timep--;
	}

	return (TRUE);
}

#endif /* __not_yet__ */


#define DX(label, val)		printf("%s: %x ", label, val);
#define DS(cnt, label, str)	printf("%s: %.*s ", label, cnt, str);

static void
atapi_id_dump(void)
{
	struct ata_id *P = (struct ata_id *)ata_id_data;

#ifdef DEBUG_ID
	
					/* WORD			*/
					/* OFFSET COMMENT	*/
DX("config",P->ai_config);		/* 0 general configuration bits */
DX("fixcyls",P->ai_fixcyls);		/* 1 # of fixed cylinders */
DX("remcyls",P->ai_remcyls);		/* 2 # of removable cylinders */
DX("heads",P->ai_heads);		/* 3 # of heads */
printf("\r\n");

DX("trksiz",P->ai_trksiz);		/* 4 # of unformatted bytes/track */
DX("secsiz",P->ai_secsiz);		/* 5 # of unformatted bytes/sector */
DX("sectors",P->ai_sectors);		/* 6 # of sectors/track */
DX("resv1[0]",P->ai_resv1[0]);	/* 7-9 "Vendor Unique" */
printf("\r\n");

DX("resv1[1]",P->ai_resv1[1]);	/* 7-9 "Vendor Unique" */
DX("resv1[2]",P->ai_resv1[2]);	/* 7-9 "Vendor Unique" */
DS(20, "drvser[20]",P->ai_drvser);	/* 10 Serial number */
printf("\r\n");

DX("buftype",P->ai_buftype);		/* 20 Buffer type */
DX("bufsz",P->ai_bufsz);		/* 21 Buffer size in 512 byte incr */
DX("ecc",P->ai_ecc);			/* 22 # of ecc bytes avail on rd/wr */
printf("\r\n");

DS(8, "fw[8]",P->ai_fw);		/* 23 Firmware revision */
DS(40, "model[40]",P->ai_model);	/* 27 Model # */
printf("\r\n");

DX("mult1",P->ai_mult1);		/* 47 Multiple command flags */
DX("dwcap",P->ai_dwcap);		/* 48 Doubleword capabilities */
DX("cap",P->ai_cap);			/* 49 Capabilities */
DX("resv2",P->ai_resv2);		/* 50 Reserved */
DX("piomode",P->ai_piomode);		/* 51 PIO timing mode */
printf("\r\n");

DX("dmamode",P->ai_dmamode);		/* 52 DMA timing mode */
DX("validinfo",P->ai_validinfo);	/* 53 bit0: wds 54-58, bit1: 64-70 */
DX("curcyls",P->ai_curcyls);		/* 54 # of current cylinders */
DX("curheads",P->ai_curheads);	/* 55 # of current heads */
DX("cursectrk",P->ai_cursectrk);	/* 56 # of current sectors/track */
printf("\r\n");

DX("cursccp[0]",P->ai_cursccp[0]);	/* 57 current sectors capacity */
DX("cursccp[1]",P->ai_cursccp[1]);	/* 57 current sectors capacity */
DX("mult2",P->ai_mult2);		/* 59 multiple sectors info */
DX("addrsec[0]",P->ai_addrsec[0]);	/* 60 LBA only: no of addr secs */
printf("\r\n");

DX("addrsec[1]",P->ai_addrsec[1]);	/* 60 LBA only: no of addr secs */
DX("sworddma",P->ai_sworddma);	/* 62 single word dma modes */
DX("dworddma",P->ai_dworddma);	/* 63 double word dma modes */
DX("advpiomode",P->ai_advpiomode);	/* 64 advanced PIO modes supported */
printf("\r\n");

DX("minmwdma",P->ai_minmwdma);	/* 65 min multi-word dma cycle info */
DX("recmwdma",P->ai_recmwdma);	/* 66 rec multi-word dma cycle info */
DX("minpio",P->ai_minpio);		/* 67 min PIO cycle info */
DX("minpioflow",P->ai_minpioflow);	/* 68 min PIO cycle info w/flow ctl */
printf("\r\n");

DX("resv3[0]",P->ai_resv3[0]);	/* 69,70 reserved */
DX("resv3[1]",P->ai_resv3[1]);	/* 69,70 reserved */
DX("resv4[0]",P->ai_resv4[0]);	/* 71-74 reserved */
DX("resv4[1]",P->ai_resv4[1]);	/* 71-74 reserved */
printf("\r\n");

DX("resv4[2]",P->ai_resv4[2]);	/* 71-74 reserved */
DX("resv4[3]",P->ai_resv4[3]);	/* 71-74 reserved */
DX("qdepth",P->ai_qdepth);		/* 75 queue depth */
DX("resv5[0]",P->ai_resv5[0]);	/* 76-79 reserved */
DX("resv5[1]",P->ai_resv5[1]);	/* 76-79 reserved */
printf("\r\n");

DX("resv5[2]",P->ai_resv5[2]);	/* 76-79 reserved */
DX("resv5[3]",P->ai_resv5[3]);	/* 76-79 reserved */
DX("majorversion",P->ai_majorversion); /* 80 major versions supported */
DX("minorversion",P->ai_minorversion); /* 81 minor version number supported*/
printf("\r\n");

DX("cmdset82",P->ai_cmdset82);	/* 82 command set supported */
DX("cmdset83",P->ai_cmdset83);	/* 83 more command sets supported */
DX("cmdset84",P->ai_cmdset84);	/* 84 more command sets supported */
DX("features85",P->ai_features85);	/* 85 enabled features */
printf("\r\n");

DX("features86",P->ai_features86);	/* 86 enabled features */
DX("features87",P->ai_features87);	/* 87 enabled features */
DX("ultradma",P->ai_ultradma);	/* 88 Ultra DMA mode */
DX("erasetime",P->ai_erasetime);	/* 89 security erase time */
printf("\r\n");

DX("erasetimex",P->ai_erasetimex);	/* 90 enhanced security erase time */
DX("padding1[0]",P->ai_padding1[0]);	/* pad to 125 */
DX("lastlun",P->ai_lastlun);		/* 126 last LUN, as per SFF-8070i */
DX("resv6",P->ai_resv6);		/* 127 reserved */
printf("\r\n");

DX("securestatus",P->ai_securestatus); /* 128 security status */
DX("vendor[0]",P->ai_vendor[0]);	/* 129-159 vendor specific */
DX("padding2[0]",P->ai_padding2[0]);	/* pad to 255 */
printf("\r\n");

	ata_pause();
#endif
	return;
}

void
ata_pause()
{
#ifdef DEBUG
	printf("\r\nPress ENTER to proceed ");
	kbchar();
	printf("\r\n");
#endif
	return;
}


typedef	unsigned short ushort;
typedef	unsigned long ulong;

typedef	union	{
	ulong	l;
	struct	{
		ushort	lo;
		ushort	hi;
	} w;
} uul;

/*
 * Computes: (op1 * op2)
 */
ulong
ULongMult( ulong op1, ulong op2 )
{

	uul	tmp1;
	uul	tmp2;
	tmp1.l = op1;
	tmp2.l = op2;

	_asm {
		push	tmp1.w.hi
		push	tmp1.w.lo
		_emit	0x66
		pop	ax

		push	tmp2.w.hi
		push	tmp2.w.lo
		_emit	0x66
		pop	cx

		_emit	0x66
		mul	cx
		_emit	0x66
		push	ax
		pop	ax
		pop	dx
	}
}


/*
 * Computes: (op1 / op2)
 */
ulong
ULongDiv( ulong op1, ulong op2 )
{
	uul	tmp1;
	uul	tmp2;
	tmp1.l = op1;
	tmp2.l = op2;

	_asm {
		push	tmp1.w.hi
		push	tmp1.w.lo
		_emit	0x66
		pop	ax

		push	tmp2.w.hi
		push	tmp2.w.lo
		_emit	0x66
		pop	cx

		_emit	0x66
		div	cx
		_emit	0x66
		push	ax
		pop	ax
		pop	dx
	}
}



/*
 * Computes: (op1 % op2)
 */
ulong
ULongRem( ulong op1, ulong op2 )
{
	uul	tmp1;
	uul	tmp2;
	tmp1.l = op1;
	tmp2.l = op2;

	_asm {
		push	tmp1.w.hi
		push	tmp1.w.lo
		_emit	0x66
		pop	ax

		push	tmp2.w.hi
		push	tmp2.w.lo
		_emit	0x66
		pop	cx
		
		_emit	0x66
		div	cx
		_emit	0x66
		push	dx
		pop	ax
		pop	dx
	}
}
