/*
 *  Copyright (c) 1997, by Sun Microsystems, Inc.
 *  All rights reserved.
 */

#ident "@(#)aha1540.c	1.18	99/07/20 SMI"


/*
 * Solaris Primary Boot Subsystem - Realmode Driver
 * ===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *   Device name: Adaptec 154x ISA SCSI HBA (aha1540.c)
 *
 */

/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	This driver is intended to be both a working realmode driver and
 *	a sample SCSI realmode driver for use as a guide to writing other
 *	SCSI realmode drivers.  Where comments in this file are intended
 *	for guidance in writing other realmode drivers they start and end
 *	with SAMPLE. SunSoft personnel should keep this in mind when updating
 *	the file for either purpose.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */

/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	The hardware-specific code for a SCSI real mode driver must define
 *	a stack (by supplying variables "stack" and "stack_size") and an
 *	initialization routine (scsi_driver_init).  All other communication
 *	is based on data filled in by scsi_driver_init.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */

#include <rmscscsi.h>
#include "aha1540.h"



/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	For most real mode drivers 1000 words of stack is plenty.  This
 *	definition can be changed locally or overridden from the makefile
 *	for drivers that require more.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
#ifndef STACKSIZE
#define	STACKSIZE 1000
#endif

ushort stack[STACKSIZE];
ushort stack_size = sizeof (stack);

/*
 * The following debug definitions allow various classes of
 * messages to be turned on.  Flags in aha_debug_flag can be set
 * by editing the definition for aha_debug_flag or AHA_DEBUG_FLAG
 * below or by defining AHA_DEBUG_FLAG on the compiler command line or
 * by writing aha_debug_flag from a debugger.
 */
#ifdef DEBUG
#pragma message (__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment (user, __FILE__ ": DEBUG ON " __TIMESTAMP__)

#define	Dstatus(f, x) \
	Dprintf(f, ("AHA: status 0x%x, istatus 0x%x\n", \
		inb((x)+AHASTAT), inb((x)+AHAINTFLGS)))

/* AHA-specific flags.  See rmsc.h for common flags */
#define	DBG_VERRS	0x0001	/* enable verbose error messages */

#ifndef AHA_DEBUG_FLAG
#define	AHA_DEBUG_FLAG	DBG_ERRS
#endif

Static int aha_debug_flag = AHA_DEBUG_FLAG;
#define	MODULE_DEBUG_FLAG aha_debug_flag

#else

#define	Dstatus(f, x)

#endif

#pragma comment (compiler)
#pragma comment (user, "aha1540.c	1.15	96/04/24")



/*
 *	Local variables used by the hardware-specific code.
 */
Static struct aha_ccb aha_ccb;
Static struct {
	struct mbox_entry mbo, mbi;
} mbx;

/*
 * The boards support several different ISA base I/O port addresses but
 * versions of Solaris earlier than 2.6 required the address to be 0x330 to
 * help prevent probe conflicts.  For compatability this restriction has
 * not been removed in Solaris 2.6 even though the device configuration
 * mechanism protects against probe conflicts.
 */
unsigned short aha_bases[] = {
#ifdef ALLOW_ALL_POSSIBLE_BASE_PORTS
	0x330, 0x130, 0x134, 0x230, 0x234, 0x334
#else
	0x330, 0x230
#endif
};



/*
 *	Local routines made visible to the SCSI layer by scsi_driver_init.
 */
Static int aha_configure(ushort, rmsc_handle *, char **, int *);
Static int aha_initialize(ulong);
Static int aha_legacy_probe(void);
Static int aha_scsi_op(rmsc_handle, struct scsi_op *);



/*
 *	Local routines called only from other local routines.
 */
Static void aha_addr3(unchar *, void far *);
Static int aha_board_init(ushort);
Static int aha_common_probe(ushort);
Static int aha_docmd(ushort, unchar, unchar far *, unchar far *);
Static int aha_init_cmd(ushort, AHA_CCB far *);
Static int aha_mbox_setup(ushort, void far *);
Static int aha_mbox_unlock(ushort);
Static int aha_wait(ushort, ushort, ushort, ushort);



/*
 *	Structure and flags for building device command table.
 */
struct  aha_cmd {
	unchar  ac_flags;	/* flag bits (below) */
	unchar  ac_args;	/* # of argument bytes */
	unchar  ac_vals;	/* number of value bytes */
};

/* ac_flags bits: */
#define	ACF_WAITIDLE    0x01    /* Wait for STAT_IDLE before issuing */
#define	ACF_WAITCPLTE   0x02    /* Wait for INT_HACC before returning */
#define	ACF_INVDCMD	0x04    /* INVALID COMMAND CODE */



/*
 *	Device command table.
 */
Static struct aha_cmd aha_cmds[] = {
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 0, 0}, /* 00 - CMD_NOP */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 4, 0}, /* 01 - CMD_MBXINIT */
	{ACF_WAITIDLE, 0, 0},	/* 02 - CMD_DOSCSI */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 10, 1}, /* 03 - CMD_ATBIOS */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 0, 4}, /* 04 - CMD_ADINQ */
	{ACF_WAITCPLTE, 1, 0},	/* 05 - CMD_MBOE_CTL */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 4, 0}, /* 06 - CMD_SELTO_CTL */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 1, 0}, /* 07 - CMD_BONTIME */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 1, 0}, /* 08 - CMD_BOFFTIME */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 1, 0}, /* 09 - CMD_XFERSPEED */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 0, 8}, /* 0a - CMD_INSTDEV */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 0, 3}, /* 0b - CMD_CONFIG */
	{ACF_INVDCMD, 0, 0},	/* 0c -- INVALID */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 1, 17}, /* 0d - CMD_RETSETUP */
	{ACF_INVDCMD, 0, 0},	/* 0e -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 0f -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 10 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 11 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 12 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 13 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 14 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 15 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 16 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 17 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 18 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 19 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 1a -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 1b -- INVALID */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 3, 0}, /* 1c - CMD_WTFIFO */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 3, 0}, /* 1d - CMD_RDFIFO */
	{ACF_INVDCMD, 0, 0},	/* 1e -- INVALID */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 1, 1}, /* 1f - CMD_ECHO */
	{ACF_INVDCMD, 0, 0},	/* 20 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 21 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 22 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 23 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 24 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 25 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 26 -- INVALID */
	{ACF_INVDCMD, 0, 0},	/* 27 -- INVALID */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 0, 2}, /* 28 - CMD_GET_EXTBIOS */
	{(ACF_WAITIDLE | ACF_WAITCPLTE), 2, 0}, /* 29 - CMD_UNLOCK_MBOX */
};



/*
 *	Driver initialization entry point.
 *
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	The primary purpose of this routine is to provide information to
 *	the SCSI framework layer.  Other driver initialization is permitted
 *	here provided that it does not involve accessing the device.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
int
scsi_driver_init(rmsc_scsi_driver_init *p)
{
	Dprintf(DBG_INIT, ("Entering scsi_driver_init\n"));

	p->driver_name = "AHA154X";
	p->legacy_probe = aha_legacy_probe;
	p->configure = aha_configure;
	p->initialize = aha_initialize;
	p->scsi_op = aha_scsi_op;

	Dprintf(DBG_INIT, ("Leaving scsi_driver_init\n"));

	return (BEF_OK);
}



/*
 *	Hardware-specific legacy probe routine.
 *
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	To avoid probe conflicts the driver must reserve resources
 *	before using them during the probe. Once it finds an adapter
 *	other devices will be prevented from accessing the resources
 *	reserved for this device.  If it does not find an adapter after
 *	reserving resources for the probe it must release the resources.
 *	Calling node_op(NODE_FREE) releases all resources reserved for
 *	the current node.  Individual resources can be released by
 *	calling rel_res.  If a device is detected, but one or more of
 *	its resources are not available, call node_op(NODE_INCOMPLETE)
 *	to report the conflict.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
Static int
aha_legacy_probe(void)
{
	ushort index, i, base_port;
	DWORD val[MaxTupleSize], len;
	unchar data[10], boardsig;

	Dprintf(DBG_PROBE, ("Entering aha_legacy_probe\n"));

	/* Try each of the supported base I/O addresses */
	for (index = 0; index < sizeof (aha_bases) / sizeof (aha_bases[0]);
						index++) {
		/*
		 * Notify framework that we're going to need space to store
		 * some information.
		 */
		if (node_op(NODE_START) != NODE_OK) {
			Dprintf(DBG_PROBE | DBG_ERRS,
				("node_op(NODE_START) failed\n"));
			break;
		}

		base_port = aha_bases[index];
		Dprintf(DBG_PROBE, ("Probing I/O address %x\n", base_port));

		/*
		 * Try to reserve the base port addresses.  Skip this
		 * address if not available.
		 */
		val[0] = base_port; val[1] = 4; val[2] = 0; len = 3;
		if (set_res("port", val, &len, 0) != RES_OK) {
			node_op(NODE_FREE);
			Dprintf(DBG_PROBE,
				("Failed to reserve \"port\" %x\n",
				base_port));
			continue;
		}
		if (aha_common_probe(base_port) == 0) {
			node_op(NODE_FREE);
			Dprintf(DBG_PROBE,
				("Device is not present\n"));
			continue;
		}

		/* Read device configuration data */
		if (aha_docmd(base_port, CMD_ADINQ, 0, data)) {
			node_op(NODE_FREE);
			Dprintf(DBG_PROBE | DBG_ERRS,
				("aha_legacy_probe: CMD_ADINQ failed\n"));
			continue;
		}
		boardsig = data[0];
		if (aha_docmd(base_port, CMD_CONFIG, 0, data)) {
			node_op(NODE_FREE);
			Dprintf(DBG_PROBE | DBG_ERRS,
				("aha_legacy_probe: CMD_CONFIG failed\n"));
			continue;
		}

		/* Reserve the DMA channel if one is configured */
		if ((boardsig != 'B') && (data[0] & CFG_DMA_MASK)) {
			for (i = 0; (i < 8) && (data[0] != 1); i++)
				data[0] >>= 1;
			val[0] = i; val[1] = 0; len = 2;
			if (set_res("dma", val, &len, 0) != RES_OK) {
				node_op(NODE_INCOMPLETE);
				Dprintf(DBG_PROBE,
					("Failed to reserve \"dma\" %d\n",
					i));
				continue;
			}
		}

		/* Reserve the IRQ if one is configured */
		if ((boardsig != 'B') && (data[1] & CFG_INT_MASK)) {
			for (i = 0; (i < 8) && (data[1] != 1); i++)
				data[1] >>= 1;
			val[0] = i + 9; val[1] = 0; len = 2;
			if (set_res("irq", val, &len, 0) != RES_OK) {
				node_op(NODE_INCOMPLETE);
				Dprintf(DBG_PROBE,
					("Failed to reserve \"irq\" %d\n",
					i));
				continue;
			}
		}

		/* Everything worked.  Confirm that the node is complete */
		node_op(NODE_DONE);
		Dprintf(DBG_PROBE, ("Node complete for adapter at %x\n",
			base_port));
	}

	Dprintf(DBG_PROBE, ("Leaving aha_legacy_probe\n"));
}



/*
 *	Hardware-specific configuration routine.
 *
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	This routine should determine whether the device described by the
 *	active node is installed and supply a device handle, device name and
 *	interrupt request line number if it is installed.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
Static int
aha_configure(ushort channel, rmsc_handle *handle, char **device_name, int *irq)
{
	DWORD val[MaxTupleSize], len;
	ushort base_port, i;
	unchar data[10], boardsig;

	Dprintf(DBG_CONFIG, ("Entering aha_configure\n"));

	/* This driver supports only single-channel boards */
	if (channel != 0) {
		Dprintf(DBG_CONFIG,
			("aha_configure: non-zero channel number\n"));
		return (BEF_FAIL);
	}

	/* Determine the base I/O address */
	len = 3;
	if (get_res("port", val, &len) != RES_OK) {
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("aha_configure: get_res failed for \"port\"\n"));
		return (BEF_FAIL);
	}
	base_port = (ushort)val[0];
	Dprintf(DBG_CONFIG, ("aha_configure: port 0x%x\n", base_port));

	/* Determine the bus type */
	len = 2;
	if (get_res("name", val, &len) != RES_OK || len < 2) {
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("aha_configure: get_res failed for \"name\"\n"));
		return (BEF_FAIL);
	}

	/*
	 * If the device is an AHA-1640, determine the configured interrupt
	 * and mask it off at the interrupt controller.  The alternative would
	 * be to disable interrupts in the MCA configuration registers but
	 * that is more intrusive.
	 */
	if (val[1] == RES_BUS_MCA && val[0] == AHA1640_SIGNATURE) {
		len = 2;
		if (get_res("irq", val, &len) != RES_OK || len < 2) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("aha_configure: get_res failed "
				"for \"irq\"\n"));
			return (BEF_FAIL);
		}
		irq_mask((ushort)val[0], 1);
	}

	/*
	 * aha_common_probe is intended to be non-intrusive to avoid affecting
	 * other devices.  But it can also fail to find a device that needs
	 * to be reset because someone else clobbered it.  Since we have
	 * been told the device is present, it is safe to do a reset before
	 * doing a confirming probe. So we call aha_board_init first.
	 */
	aha_board_init(base_port);

	/*
	 * Use 0 for the hba_id. It's only used for displaying board info.
	 */
	if (aha_common_probe(base_port)) {

		/* Read configuration data from the device */
		if (aha_docmd(base_port, CMD_ADINQ, 0, data)) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("aha_configure: CMD_ADINQ failed\n"));
			return (BEF_FAIL);
		}
		boardsig = data[0];
		if (aha_docmd(base_port, CMD_CONFIG, 0, data)) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("aha_configure: CMD_CONFIG failed\n"));
			return (BEF_FAIL);
		}

		/*
		 * If the device uses DMA, make sure the channel
		 * was reserved.
		 */
		if ((boardsig != 'B') && (data[0] & CFG_DMA_MASK)) {
			for (i = 0; (i < 8) && (data[0] != 1); i++)
				data[0] >>= 1;
			len = 2;
			if (get_res("dma", val, &len) != RES_OK) {
				Dprintf(DBG_CONFIG | DBG_ERRS,
					("aha_configure: no DMA\n"));
				return (BEF_FAIL);
			}
			if (val[0] != i) {
				Dprintf(DBG_CONFIG | DBG_ERRS,
					("aha_configure: wrong DMA\n"));
				return (BEF_FAIL);
			}
		}

		/*
		 * If the device uses an IRQ, make sure the channel
		 * was reserved.  This driver doesn't use it but the
		 * Solaris driver does.
		 */
		if ((boardsig != 'B') && (data[1] & CFG_INT_MASK)) {
			for (i = 0; (i < 8) && (data[1] != 1); i++)
				data[1] >>= 1;
			len = 2;
			if (get_res("irq", val, &len) != RES_OK) {
				Dprintf(DBG_CONFIG | DBG_ERRS,
					("aha_configure: no IRQ\n"));
				return (BEF_FAIL);
			}
			if (val[0] != i + 9) {
				Dprintf(DBG_CONFIG | DBG_ERRS,
					("aha_configure: wrong IRQ\n"));
				return (BEF_FAIL);
			}
		}

		/* Use the base I/O address as the device handle */
		*handle = (ulong)base_port;
		*device_name = "AHA154X";
		*irq = RMSC_NO_INTERRUPTS;

		Dprintf(DBG_CONFIG, ("Leaving aha_configure\n"));

		return (BEF_OK);
	}

	Dprintf(DBG_CONFIG | DBG_ERRS, ("aha_configure probe failed\n"));

	return (BEF_FAIL);
}



/*
 *	Hardware-specific device initialization routine.
 *	Called by SCSI layer before any calls to aha_scsi_op.
 */
Static int
aha_initialize(rmsc_handle handle)
{
	ushort base_port = (ushort)handle;

	Dprintf(DBG_CONFIG, ("Entering aha_initialize\n"));

	if (aha_board_init(base_port) != BEF_OK) {
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("Failed to initialize host adapter\n"));
		return (BEF_FAIL);
	}

	Dprintf(DBG_CONFIG, ("Leaving aha_initialize\n"));

	return (BEF_OK);
}



/*
 *	Hardware-specific SCSI operation routine.
 */
Static int
aha_scsi_op(rmsc_handle handle, struct scsi_op *op)
{
	AHA_CCB *ccbp = &aha_ccb;
	union targ_field tf;
	ulong phys_buf = FP_TO_LINEAR(op->buffer);
	short base_port = (ushort)handle;
	int i;
	int cmd_status;

	Dprintf(DBG_GEN, ("Entering aha_scsi_op\n"));

	/*
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 *	==============================================================
	 *	If RMSC_SFL_BUS_RESET flag is set, just reset the SCSI bus.
	 *	This test must go first because nothing else in the structure
	 *	is meaningful if this bit is set.
	 *	==============================================================
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 */
	if (op->request_flags & RMSC_SFL_BUS_RESET) {
		outb(base_port + AHACTL, CTL_SCRST);
		drv_msecwait(1000);
		op->result_flags |= RMSC_SOF_COMPLETED;

		Dprintf(DBG_GEN, ("aha_scsi_op: completed bus reset\n"));

		return (BEF_OK);
	}

	/*
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 *	==============================================================
	 *	Next test must be for target number out of range or the device
	 *	search will not terminate properly.
	 *	==============================================================
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 */
	if (op->target >= 8) {
		op->result_flags = RMSC_SOF_BAD_TARGET;

		Dprintf(DBG_GEN, ("aha_scsi_op: target out of range\n"));

		return (BEF_OK);
	}

	/*
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 *	==============================================================
	 *	Next test must be for LUN out of range or the device search
	 *	will not terminate properly.
	 *	==============================================================
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 */
	if (op->lun >= 8) {
		op->result_flags = RMSC_SOF_BAD_LUN;

		Dprintf(DBG_GEN, ("aha_scsi_op: LUN out of range\n"));

		return (BEF_OK);
	}

	/* Fill in the AHA_CCB using the scsi_op data */
	memset((char *)ccbp, 0, sizeof (AHA_CCB));
	ccbp->ccb_op = COP_COMMAND;
	tf.tff.tf_lun = op->lun;
	tf.tff.tf_tid = op->target;

	/*
	 * The 1740 does not emulate the 1542 correctly, because the 1542
	 * does not behave as advertised.  This causes problems when 1742's
	 * run in emulation mode, because it detects and returns error
	 * conditions, in cases where the 1542 doesn't even check.
	 * The direction flags should both be turned off in all cases
	 * because we are running in initiator mode.  This will prevent
	 * the dev_sense command from failing in the case where we have
	 * data underruns on the 512-byte device during ask_blocksize().
	 *
	 */
	tf.tff.tf_in = 0;
	tf.tff.tf_out = 0;
	ccbp->ccb_reqsense = 0;
	ccbp->ccb_targ = tf.tfc;
	ccbp->ccb_cdblen = op->cmd_len;
	memcpy(ccbp->ccb_cdb, op->scsi_cmd, op->cmd_len);
	ccbp->ccb_dlen[0] = (op->transfer_len >> 16);
	ccbp->ccb_dlen[1] = (op->transfer_len >> 8);
	ccbp->ccb_dlen[2] = op->transfer_len;
	ccbp->ccb_dptr[0] = (phys_buf >> 16);
	ccbp->ccb_dptr[1] = (phys_buf >> 8);
	ccbp->ccb_dptr[2] = phys_buf;

	/* Attempt the command and decipher the result */
	if ((cmd_status = aha_init_cmd(base_port, ccbp)) == 0) {
		/* Everything worked and target status was 0 */
		op->result_flags = (RMSC_SOF_COMPLETED | RMSC_SOF_GOOD);
		op->scsi_status = 0;

		Dprintf(DBG_GEN,
			("aha_scsi_op: command completed with no errors\n"));

		return (BEF_OK);
	}

	/*
	 * A selection timeout gives MBX_STAT_ERROR, so we need to check
	 * for it separately in order to supply the required result flag.
	 */
	if (ccbp->ccb_hastat == HS_SELTO) {
		/* Selection timeout */
		op->result_flags = RMSC_SOF_NO_TARGET;

		Dprintf(DBG_GEN, ("aha_scsi_op: target selection timeout\n"));

		return (BEF_OK);
	}

	if (cmd_status == MBX_STAT_ERROR) {
		/* Command completed but there was an error */
		op->result_flags = (RMSC_SOF_COMPLETED | RMSC_SOF_ERROR);
		op->scsi_status = ccbp->ccb_tarstat;

		if (ccbp->ccb_tarstat == S_CK_COND) {
			/* Return the request sense data */
			i = (RMSC_MAX_ARS < 14 ? RMSC_MAX_ARS : 14);
			memcpy(op->ars_data,
				ccbp->ccb_cdb + ccbp->ccb_cdblen, i);
			op->ars_len = i;
			op->result_flags |= RMSC_SOF_ARS_DONE;
		}

		Dprintf(DBG_GEN,
			("aha_scsi_op: command gave CHECK CONDITION\n"));

		return (BEF_OK);
	}

	Dprintf(DBG_GEN | DBG_ERRS, (" host stat 0x%x, targ stat 0x%x\n",
			ccbp->ccb_hastat, ccbp->ccb_tarstat));
#ifdef DEBUG
	if (aha_debug_flag & DBG_VERRS) {
		printf("Cmd data  : ");
		for (i = 0; i < MAX_CDB_LEN; i++)
			printf("%02x ", ccbp->ccb_cdb[i]);
		printf("\nSense data: ");
		for (i = 0; i < 14; i++)
			printf ("%02x ", ccbp->ccb_cdb[ccbp->ccb_cdblen + i]);
		printf("\n");
	}
#endif /* DEBUG */

	/* Some other error occurred.  We are not required to be specific */
	op->result_flags = RMSC_SOF_ERROR;

	Dprintf(DBG_GEN,
			("aha_scsi_op: command completed with an error\n"));
	return (BEF_OK);
}



/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	All the routines after this point in the file are private routines
 *	called from the routines declared above.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */



/*
 * aha_docmd -- get the AHA-154X to do something.
 *		Args are a base address, a command op code,
 *		an output data buffer pointer (if required), and an input
 *		data buffer pointer (if required).  Value is 0 if command
 *		completes normally, 1 if STAT_INVDCMD is set during execution,
 *		or if op code is invalid (NOTE: RANGE IS NOT CHECKED!).
 */
Static int
aha_docmd(ushort base, unchar opcode, unchar far *sbp, unchar far *rbp)
{
	struct aha_cmd *cp = &aha_cmds[opcode];
	register int i;
	ushort oldspl;

	if (cp->ac_flags & ACF_INVDCMD) {
		Dprintf(DBG_GEN | DBG_ERRS, ("aha_docmd: invalid command\n"));
		return (1);	/* invalid command */
	}

	/* ---- wait for the adapter to go idle ---- */
	if (cp->ac_flags & ACF_WAITIDLE)	{
		if (aha_wait (base+AHASTAT, STAT_IDLE, STAT_IDLE, 0)) {
			Dprintf(DBG_GEN | DBG_ERRS,
				("aha_docmd: not idle\n"));
			return (1);
		}
	}

	/* wait for data buffer to be clear */
	if (aha_wait(base+AHASTAT, STAT_CDF, 0, STAT_CDF)) {
		Dprintf(DBG_GEN | DBG_ERRS, ("aha_docmd: cmd reg full\n"));
		Dprintf(DBG_GEN | DBG_ERRS,
			("           cmd 0x%x, base 0x%x\n",
			opcode, base));
		Dstatus(DBG_GEN | DBG_ERRS, base);
		return (1);
	}

	Dprintf(DBG_GEN, ("aha_docmd: before sending cmd "));
	Dstatus(DBG_GEN, base);

	/*
	 * Disable interrupts before issuing the command because we
	 * do not want an interrupt handler to process HACC.
	 */
	oldspl = splhi();
	outb (base + AHADATACMD, opcode); /* issue the command */

#ifdef DEBUG
	if (aha_wait(base+AHASTAT, STAT_CDF, 0, STAT_CDF)) {
		splx(oldspl);
		Dprintf(DBG_GEN | DBG_ERRS,
			("aha_docmd: cmd reg stayed full\n"));
		Dprintf(DBG_GEN | DBG_ERRS,
			("           cmd %x, base %x, stat %x, istat %x\n",
			opcode, base, inb(base + AHASTAT),
			inb(base + AHAINTFLGS)));
		return (1);
	}
#endif

	/* If any output data, write it out */
	if ((i = cp->ac_args) != 0) { /* send it byte at a time */
		while ((i--) > 0) {
			/* wait for STAT_CDF to turn off */
			if (aha_wait(base+AHASTAT, STAT_CDF, 0, STAT_CDF)) {
				splx(oldspl);
				Dprintf(DBG_GEN | DBG_ERRS,
					("aha_docmd: data reg full\n"));
				return (1);
			}
			outb(base+AHADATACMD, *(sbp++));
#ifdef DEBUG
			if (aha_wait(base+AHASTAT, STAT_CDF, 0, STAT_CDF)) {
				splx(oldspl);
				Dprintf(DBG_GEN | DBG_ERRS,
				    ("aha_docmd: data reg stayed full\n"));
				return (1);
			}
#endif
			if (inb (base+AHASTAT) & STAT_INVDCMD) {
				splx(oldspl);
				Dprintf(DBG_GEN | DBG_ERRS,
					("aha_docmd: command rejected\n"));
				return (1);
			}
		}
	}

	/* if any return data, get it */
	if ((i = cp->ac_vals) != 0) { /* receive it byte at a time */
		while ((i--) > 0) {
			/* wait for STAT_DF to turn on */
			if (aha_wait(base+AHASTAT, STAT_DF, STAT_DF, 0)) {
				splx(oldspl);
				Dprintf(DBG_GEN | DBG_ERRS,
					("aha_docmd: no incoming data\n"));
				return (1);
			}
			*(rbp++) = inb(base+AHADATACMD);
		}
	}

	/* Wait for completion if necessary */
	if ((cp->ac_flags & ACF_WAITCPLTE) == 0) {
		splx(oldspl);
		Dprintf(DBG_GEN,
			("aha_docmd: (no cmd completion) status at exit: "));
		Dstatus(DBG_GEN, base);
		return (0);
	}
	if (aha_wait(base+AHAINTFLGS, INT_HACC, INT_HACC, 0)) {
		splx(oldspl);
		Dprintf(DBG_GEN | DBG_ERRS,
			("aha_docmd: no command completion\n"));
		Dstatus(DBG_GEN | DBG_ERRS, base);
		return (1);
	}

	/* Reset the interrupts or we get out of sync with the board */
	outb(base+AHACTL, CTL_IRST);
	splx(oldspl);

	/* Check for error */
	if (inb(base+AHASTAT) & STAT_INVDCMD) {
		Dprintf(DBG_GEN | DBG_ERRS,
			("aha_docmd: command caused error\n"));
		return (1);
	}

	return (0);
}



/*
 * aha_wait --  wait for a register of a controller to achieve a
 *		specific state.  Arguments are a mask of bits we care about,
 *		and two sub-masks.  To return normally, all the bits in the
 *		first sub-mask must be ON, all the bits in the second sub-
 *		mask must be OFF.  If 15 seconds pass without the controller
 *		achieving the desired bit configuration, we return 1, else
 *		0.
 */
Static int
aha_wait(register ushort port, ushort mask, ushort onbits, ushort offbits)
{
	register unsigned long i;

	for (i = 0; i < WAIT_RETRIES; i++) {
		register unsigned short maskval = inb(port) & mask;

		if (((maskval & onbits) == onbits) &&
					((maskval & offbits) == 0))
			return (0);
		drv_msecwait(1);
	}
	return (1);
}



/*
 * aha_init_cmd -- Execute a SCSI command during init time (no interrupts).
 *		   We use only the first MBO entry and return 0 if there
 *		   were no errors.  Otherwise, we return the mbx_cmdstat
 *		   byte.
 */

Static int
aha_init_cmd(register ushort port, register AHA_CCB far *ccbp)
{
	unchar errcod = 0;

	/*
	 * We are not interrupt-driven so, if there is no
	 * available mailbox now, waiting will not help.
	 */
	if (mbx.mbo.mbx_cmdstat != 0) {
		Dprintf(DBG_ERRS, ("aha_init_cmd: no available mailbox\n"));
		return (-1);
	}
	aha_addr3((unchar *)mbx.mbo.mbx_ccb_addr,
			MK_FP(get_data_selector(), ccbp));
	mbx.mbi.mbx_cmdstat = MBX_FREE; /* give mailbox back */
	mbx.mbo.mbx_cmdstat = MBX_CMD_START;

	/* If aha_docmd succeeds, interrupts are disabled on return */
	if (aha_docmd(port, CMD_DOSCSI, 0, 0)) {
		Dprintf(DBG_ERRS, ("aha_init_cmd: aha_docmd failed\n"));
		return (-1);
	}

	/* wait for INT_MBIF */
	if (aha_wait(port+AHAINTFLGS, INT_MBIF, INT_MBIF, 0)) {
		splx(RMSC_INTR_ENABLE);

		Dprintf(DBG_ERRS, ("aha_init_cmd: never got INT_MBIF\n"));
		Dstatus(DBG_ERRS, port);

		return (-1);
	}

	/* Reset interrupts so we don't get out of sync. */
	outb(port+AHACTL, CTL_IRST);
	splx(RMSC_INTR_ENABLE);

	if (mbx.mbi.mbx_cmdstat != MBX_STAT_DONE) {
		/* Save error code for caller */
		errcod = mbx.mbi.mbx_cmdstat;

		/* This case isn't always an error, so use DBG_VERRS */
		Dprintf(DBG_VERRS, ("aha_init_cmd rtn=0x%x\n", errcod));
		Dprintf(DBG_VERRS, ("\
op.%02d, targ.%02d, len.%02d, reqsense.%02d, dlen.%02x.%02x.%02x, \
dptr.%02x.%02x.%02x, link.%x.%x.%x, id.x%02x, hastat.x%02x, tarstat.x%02x\n",
		    ccbp->ccb_op, ccbp->ccb_targ, ccbp->ccb_cdblen,
		    ccbp->ccb_reqsense, ccbp->ccb_dlen[0], ccbp->ccb_dlen[1],
		    ccbp->ccb_dlen[2], ccbp->ccb_dptr[0], ccbp->ccb_dptr[1],
		    ccbp->ccb_dptr[2], ccbp->ccb_link[0], ccbp->ccb_link[1],
		    ccbp->ccb_link[2], ccbp->ccb_linkid, ccbp->ccb_hastat,
		    ccbp->ccb_tarstat));
	}
	mbx.mbi.mbx_cmdstat = MBX_FREE; /* give mailbox back */

	/* ---- returns result of command; 0 if successful ---- */
	return (errcod);
}



Static int
aha_board_init(ushort base_port)
{
	/*
	 * Reset the controller and set up mailboxes.
	 *
	 * returns:
	 *		0  success
	 *		1  command timed out, or unable to set up mailbox.
	 */
	register short i;

	/*
	 * Reset the adapter
	 *
	 */
	outb(base_port + AHACTL, CTL_HRST);
	for (i = 0; /* no test here */; i++) {
		if ((inb(base_port + AHASTAT) & (STAT_STST | STAT_DIAGF)) ==
					0) {
			break;
		}
		if (i >= INIT_RETRIES) {
			Dprintf(DBG_ERRS, ("aha_board_init: status %x\n",
					inb(base_port + AHASTAT)));
			return (BEF_FAIL);
		}
		drv_msecwait(1);
	}

	drv_msecwait(RESET_SETTLE_TIME);

	/* Initialize the mailboxes */
	mbx.mbo.mbx_cmdstat = 0;
	mbx.mbi.mbx_cmdstat = 0;
	if (aha_mbox_setup(base_port, MK_FP(get_data_selector(), &mbx.mbo))) {
		Dprintf(DBG_ERRS,
			("aha_board_init: failed to set up mailboxes\n"));
		return (BEF_FAIL);
	}

	return (BEF_OK);
}



Static int
aha_mbox_setup(ushort base_port, void far *addr)
{
	static unchar mailbox_databuf[22];

	mailbox_databuf[0] = 1;
	aha_addr3(&mailbox_databuf[1], addr);

	if (aha_docmd(base_port, CMD_MBXINIT, mailbox_databuf, 0)) {
		printf("AHA adapter mailbox init command failed.\n");
		return (1);
	}
#ifdef DEBUG
	Dprintf(DBG_INIT, ("Sending CMD_RETSETUP\n"));
	mailbox_databuf[4] = 17;
	if (aha_docmd(base_port, CMD_RETSETUP, mailbox_databuf + 4,
			mailbox_databuf + 5)) {
		Dprintf(DBG_ERRS,
			("AHA adapter return setup data command failed\n"));
	} else {
		register int i;
		for (i = 0; i < 4; i++) {
			if (mailbox_databuf[i] != mailbox_databuf[i + 9]) {
				Dprintf(DBG_ERRS,
					("Mailbox setup byte %x wrong.\n",
					i));
				Dprintf(DBG_ERRS, ("Request: %x actual %x\n",
					mailbox_databuf[i],
					mailbox_databuf[i + 9]));
			}
		}
		Dprintf(DBG_INIT, ("Finished checking return setup command\n"));
	}
#endif
	return (0);
}



/*
 *	aha_common_probe()
 *
 *	This routine is called by aha_legacy_probe() and aha_configure()
 *	check for an adapter at the specified address.
 *
 *	Returns 1 is an adapter was found, 0 if not.
 */
Static int
aha_common_probe(ushort base_port)
{
	unsigned char status;
	unchar data[10];
	unchar boardsig;
	ushort hba_id = 0;
	int i;

	/* ---- do interrupt-dependent timing set-up ---- */
	drv_msecwait(1);

	Dprintf(DBG_PROBE, ("aha_common_probe: checking base_port 0x%x\n",
			base_port));
	status = inb(base_port + AHASTAT);

	/*
	 * Assume device is not AHA if status bits are all off, or all on.
	 * That could theoretically happen but in practice it
	 * does not.  We have seen cases where other devices
	 * present 0 so it is better to avoid the long timeout
	 * period.
	 */
	if (status == 0 || status == 0xff) {
		Dprintf(DBG_PROBE,
			("aha_common_probe: %s status bits are set\n",
			status == 0 ? "no" : "all"));
		return (0);
	}

	/*
	 * The aha Selftest takes between 15-20ms, If the ST in progress
	 * bit is set wait 400ms for it to clear, if it does not clear, then
	 * fail the probe, if it does, restart ST verify the bit goes from,
	 * 0 (ST finished) to 1 (ST in progress) to 0. Also check ST passed,
	 * If it fails fail the probe.
	 */

	if (status & STAT_STST){
		drv_msecwait(400);
		status = inb(base_port + AHASTAT);
		if (status & STAT_STST){
			Dprintf(DBG_PROBE, ("aha_common_probe: "
				"Fail - STST bit set\n"));
			return (0);
		}
	}

	/*
	 * Possible aha card and ST finished, verify that bits 5 (on if
	 * ST passed) and bit 6 (on if ST failed) are not both set.
	 */
	
	if ((status & STAT_DIAGF) & (status & STAT_INIT)){
		Dprintf(DBG_PROBE, ("aha_common_probe: Fail - DIAGF and "
			"INIT both set, status = 0x%x\n", status));
		return (0);
	}

	/* Assume device is not AHA if reserved bit is set */
	if (status & STAT_RSRVD) {
		Dprintf(DBG_PROBE, ("aha_common_probe: Fail  - RSRVD set\n"));
		return (0);
	}

	/*
	 * Restart ST verify the bit goes from,
	 * 0 (ST finished) to 1 (ST in progress) to 0. Also check ST passed,
	 * If it fails fail the probe.
	 */

	outb(base_port + AHACTL, CTL_HRST);
	for (i = 0; i < 20; i++){
		status = inb(base_port + AHASTAT);
		if (status & STAT_STST)
			break;
		drv_msecwait(1);
	}

	if (i >= 20){
		Dprintf(DBG_PROBE, ("common_probe: Fail - Could not "
			"start Diag.  Status = 0x%x.\n", status));
		return (0);
	}

	drv_msecwait(400);
	status = inb(base_port + AHASTAT);
	if (status & STAT_STST){
		Dprintf(DBG_PROBE, ("common_probe: Fail - Diag did not "
			"complete.\n"));
		return (0);
	}

	/* Assume AHA is missing or unuseable if DIAGF bit is set */
	if (status & STAT_DIAGF) {
		Dprintf(DBG_PROBE, ("aha_common_probe: Fail - Diag failed.  "
			"Status = 0x%x.\n", status));
		return (0);
	}
	if (!(status & STAT_INIT)) {
		Dprintf(DBG_PROBE, ("common_probe: Failed to Pass Init "
			"Test.  Status = 0x%x.\n", status));
		return (0);
	}

	if ((status & STAT_PROBE_ONBITS) != STAT_PROBE_ONBITS ||
			(status & STAT_PROBE_OFFBITS) != 0) {
		Dprintf(DBG_PROBE, ("common_probe: Fail - failed to reset.  "
			"Status = 0x%x.\n", status));
		return (0);
	}

	drv_msecwait(RESET_SETTLE_TIME);

	/*
	 * Assume AHA is missing or unuseable if it does not respond
	 * properly to adapter inquiry command.
	 */
	if (aha_docmd(base_port, CMD_ADINQ, 0, data)) {
		Dprintf(DBG_PROBE, ("aha_common_probe: ADINQ failed\n"));
		return (0);
	}

	/*
	 * Make sure board is a 154X, not a 1740 which has its
	 * own module.
	 */
	switch (data[0]) {
	    case 0:		/* 1540 (ISA) with 16-head BIOS */
	    case '0':		/* 1540 (ISA) with 64-head BIOS */
	    case 'A':		/* 1540B/1542B (ISA) with 64-head BIOS */
	    case 'B':		/* 1640 (MCA) with 64-head BIOS */
	    case 'C':		/* 1740 (EISA) in compatibility mode */
	    case 'D':		/* 1540C/1542C (ISA) with 64-head BIOS */
	    case 'E':		/* 1540CF/1542CF (ISA)		*/
	    case 'F':		/* 1540CP/1542CP, plug and play version */
		break;
	    default:
		if (data[0] < 'Z')
			break;
		Dprintf(DBG_PROBE, ("aha_common_probe: not a 1540\n"));
		return (0);
	}

	Dprintf(DBG_PROBE, ("Found Adaptec "));
	switch (data[0]) {
	    case 0:		/* 1540 with 16-head BIOS */
	    case '0':		/* 1540 with 64-head BIOS */
		Dprintf(DBG_PROBE, ("1540 SCSI host adapter"));
		break;
	    case 'A':		/* 1540B/1542B with 64-head BIOS */
		Dprintf(DBG_PROBE, ("1540B SCSI host adapter"));
		break;
	    case 'B':		/* 1640 with 64-head BIOS */
		Dprintf(DBG_PROBE, ("1640 SCSI host adapter"));
		if (hba_id == BT646_SIGNATURE) {
			Dprintf(DBG_PROBE, (" (Buslogic BT646)"));
		}
		break;
	    case 'C':		/* 1740 in compatibility mode */
		Dprintf(DBG_PROBE, ("1740 SCSI host adapter in 1540 mode"));
		break;
	    case 'D':		/* 1540C/1542C with 64-head BIOS */
		Dprintf(DBG_PROBE, ("1540C SCSI host adapter"));
		break;
	    case 'E':			/* 1540CF/1542CF 	*/
		Dprintf(DBG_PROBE, ("1540CF SCSI host adapter"));
		break;
	    case 'F':			/* 1540CP/1542CP 	*/
		Dprintf(DBG_PROBE, ("1540CP SCSI host adapter"));
		break;

	    default:		/* any other 1540 clone */
		Dprintf(DBG_PROBE, ("1540-compatible SCSI host adapter"));
		break;
	}
	Dprintf(DBG_PROBE, (" with base addr 0x%x\n", base_port));

	boardsig = data[0];
	data[0] = 0;
	data[2] = 0xFF;
	if (aha_docmd(base_port, CMD_ECHO, data, data + 1) ||
	    aha_docmd(base_port, CMD_ECHO, data + 2, data + 3)) {
		Dprintf(DBG_PROBE | DBG_ERRS,
			("Host adapter ECHO command failed\n"));
		return (0);
	}

	if (data[1] != 0 || data[3] != 0xFF) {
		printf("Host adapter ECHO command gave incorrect result.\n");
		Dprintf(DBG_PROBE | DBG_ERRS, ("Data: %02x %02x %02x %02x\n",
			data[0], data[1], data[2], data[3]));
	}

	if (boardsig == 'D' || boardsig == 'E' || boardsig == 'F') {
		if (aha_mbox_unlock(base_port)) {
			printf("Failed to unlock mailbox interface.\n");
			return (0);
		}
	}
	return (1);
}




Static int
aha_mbox_unlock(ushort base_port)
{
	unchar data[2];

	if (aha_docmd(base_port, CMD_GET_EXTBIOS, 0, data)) {
		printf("Unable to retrieve extended BIOS information.\n");
		return (1);
	}

	Dprintf(DBG_GEN, ("Extended BIOS information: 0x%02x 0x%02x\n",
			data[0], data[1]));

	if (data[1]) {	/* retrieve lock code */
		data[0] = 0;
		if (aha_docmd(base_port, CMD_UNLOCK_MBOX, data, 0)) {
			Dprintf(DBG_GEN | DBG_ERRS,
				("Unable to unlock mailbox interface.\n"));
			return (1);
		}
	}

	Dprintf(DBG_GEN, ("Mailbox interface unlocked.\n"));
	return (0);
}



Static void
aha_addr3(unchar *home, void far *addr)
{
	ulong phys_addr = FP_TO_LINEAR(addr);

	home[0] = (phys_addr >> 16);
	home[1] = (phys_addr >> 8);
	home[2] = phys_addr;
	Dprintf(DBG_GEN, ("aha_addr3: addr 0x%lx == %02x%02x%02x\n",
			addr, home[0], home[1], home[2]));
}
