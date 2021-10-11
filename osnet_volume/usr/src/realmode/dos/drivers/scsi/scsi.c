/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 */

#ident "@(#)scsi.c	1.21	99/11/25 SMI"



/*
 * Code to implement the SCSI real mode driver interface in terms
 * of the generic driver interface.
 *
 * This layer implements the driver_init routine and expects the
 * device-dependent code to supply a scsi_driver_init routine
 * (called from this file) and the "stack" and "stack_size"
 * variables expected by the generic layer.
 */



#include "rmscscsi.h"

Static rmsc_scsi_driver_init scsi_init_data;

/* Buffer for handling sector-oriented reads from 2K devices */
Static unchar block_buffer[2048];

#define	UNKNOWN_BSIZE	0



/*
 * Debugging facilities in this file are partly intended for debugging
 * the SCSI code itself and partly for helping to debug hardware-specific
 * realmode code that implements a SCSI driver in terms of this code.
 *
 * Various classes of messages can be turned on by setting flags in
 * scsi_debug_flag.  Flags can be set by changing the definition of
 * SCSI_DEBUG_FLAG below, by setting SCSI_DEBUG_FLAG from the compiler
 * command line, or by writing the scsi_debug_flag using a debugger
 * while running the driver.
 */
#ifdef DEBUG
#pragma message(__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)

/* SCSI-specific flags.  See rmsc.h for common flags */
#define	DBG_SCAN	0x0001	/* enable messages from device scan */
#define	DBG_REG		0x0002	/* enable messages from device registration */
#define	DBG_INTR	0x0004	/* enable messages from device interrupts */
#define	DBG_OP		0x0008	/* enable messages from SCSI operations */

#ifndef SCSI_DEBUG_FLAG
#define	SCSI_DEBUG_FLAG	DBG_ERRS
#endif
int scsi_debug_flag = SCSI_DEBUG_FLAG;
#define	MODULE_DEBUG_FLAG scsi_debug_flag

#endif /* DEBUG */



/* Prototypes of routines called from the generic layer */
Static int scsi_configure(void);
Static void scsi_device_interrupt(int);
Static int scsi_init(rmsc_handle, struct bdev_info *);
Static int scsi_read(rmsc_handle, struct bdev_info *, ulong, ushort,
	char far *, ushort);
Static int scsi_extgetparms(rmsc_handle, struct bdev_info *,
	struct ext_getparm_resbuf far *);


/* Prototypes of routines called inside the SCSI layer */
Static int scsi_block_read(rmsc_handle, struct bdev_info *, ulong, ushort,
	char far *);
Static void scsi_bsize(rmsc_handle, struct bdev_info *);
Static int scsi_channel_setup(ushort, rmsc_handle, int);
Static int scsi_device_search(ushort, rmsc_handle, char *, int);
Static ushort scsi_inquire(rmsc_handle, ushort, ushort,
	struct inquiry_data *);
Static int scsi_lock(rmsc_handle, struct bdev_info *, int);
Static void scsi_missing_assign(char *);
Static int scsi_motor(rmsc_handle, struct bdev_info *, int);
Static unsigned long scsi_ret_ulreadcap(rmsc_handle, struct bdev_info *);
Static int scsi_readcap(rmsc_handle, struct bdev_info *,
	struct readcap_data far *);
Static int scsi_register(rmsc_handle, ushort, char *, ushort, ushort,
	struct inquiry_data *);



/* Prototypes for interrupt stub routines */
extern void scsi_irq0(void), scsi_irq1(void), scsi_irq2(void);
extern void scsi_irq3(void), scsi_irq4(void), scsi_irq5(void);
extern void scsi_irq6(void), scsi_irq7(void), scsi_irq8(void);
extern void scsi_irq9(void), scsi_irqa(void), scsi_irqb(void);
extern void scsi_irqc(void), scsi_irqd(void), scsi_irqe(void);
extern void scsi_irqf(void);



/*
 * Initialize the SCSI layer.  Depends on successful initialization
 * of the hardware-specific code.
 */
int
driver_init(struct driver_init *p)
{
	int ret = BEF_OK;

	/* Set up init structure before calling hardware-specific init */
	memset(&scsi_init_data, 0, sizeof (scsi_init_data));
	scsi_init_data.driver_name = "NAME NOT SUPPLIED";

	Dcall(DBG_INIT, "scsi_driver_init");
	if (scsi_driver_init(&scsi_init_data) != BEF_OK) {
		Dfail(DBG_INIT | DBG_ERRS, "scsi_driver_init");
		return (BEF_FAIL);
	}
	Dsucceed(DBG_INIT, "scsi_driver_init");

	/*
	 * Assign driver_init struct members defined in rmsc.h.
	 */
	p->driver_name = scsi_init_data.driver_name;
	p->legacy_probe = scsi_init_data.legacy_probe;
	p->configure = scsi_configure;
	p->init = scsi_init;
	p->read = scsi_read;
	p->extgetparms = scsi_extgetparms;

#ifdef DEBUG
	/*
	 * Check for required items in scsi_init_data.
	 * These tests are included only under DEBUG to save
	 * space in a non-DEBUG driver.
	 */
	if (scsi_init_data.driver_name == 0) {
		scsi_missing_assign("driver_name");
		ret = BEF_FAIL;
	}
	if (scsi_init_data.configure == 0) {
		scsi_missing_assign("configure");
		ret = BEF_FAIL;
	}
	if (scsi_init_data.initialize == 0) {
		scsi_missing_assign("initialize");
		ret = BEF_FAIL;
	}
	if (scsi_init_data.scsi_op == 0) {
		scsi_missing_assign("scsi_op");
		ret = BEF_FAIL;
	}
#endif /* DEBUG */

	return (ret);
}



/*
 * Configure the devices supported by this driver by using node_op
 * to scan the list of device nodes.  For each node found, call the
 * hardware-specific configuration routine to configure each channel
 * of the corresponding SCSI adapter.  Then search for attached SCSI
 * devices.
 */
Static int
scsi_configure(void)
{
	int ret = BEF_FAIL;	/* Haven't configured any devices yet */
	rmsc_handle handle;
	struct bdev_info info;
	char *device_name;
	ushort channel;
	int irq;

	while (node_op(NODE_START) == NODE_OK) {
		for (channel = 0; channel != 0xFFFF; channel++) {
			/*
			 * Default the device name to the driver name.
			 * The device name is not supposed to be optional;
			 * we just do this as a cheap way to provide error
			 * handling for the device-dependent code.
			 */
			device_name = scsi_init_data.driver_name;
			Dcall(DBG_CONFIG, "configure");
			if ((*scsi_init_data.configure)
					(channel, &handle, &device_name,
					&irq) != BEF_OK) {
				Dfail(channel == 1 ? DBG_CONFIG :
					(DBG_CONFIG | DBG_ERRS),
					"configure");
				break;
			}
			Dsucceed(DBG_CONFIG, "configure");

			if (scsi_channel_setup(channel, handle, irq) != BEF_OK)
				continue;

			if (scsi_device_search(channel, handle, device_name,
					irq) == BEF_OK)
				ret = BEF_OK;
		}

		/*
		 * The check point mechanism allows us to report failure
		 * for the node even if we already registered some devices
		 * for it.
		 */
		switch (rmsc_checkpoint(0)) {
		case RMSC_CHKP_GOOD:
			node_op(NODE_DONE);
			continue;
		case RMSC_CHKP_STOP:
			node_op(NODE_DONE);
			break;
		default:
		case RMSC_CHKP_BAD:
			node_op(NODE_FREE);
			break;
		}
		break;
	}

	return (ret);
}



/*
 * Prepare one channel of a SCSI adapter for operation.
 */
Static int
scsi_channel_setup(ushort channel, rmsc_handle handle, int irq)
{
	DWORD res_space[IrqTupleSize];
	DWORD len;
	int node_irq;
	static void (*(irq_handlers[]))(void) = {
		scsi_irq0, scsi_irq1, scsi_irq2, scsi_irq3,
		scsi_irq4, scsi_irq5, scsi_irq6, scsi_irq7,
		scsi_irq8, scsi_irq9, scsi_irqa, scsi_irqb,
		scsi_irqc, scsi_irqd, scsi_irqe, scsi_irqf
	};
	static ushort irq_vectors[] = {
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
		0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77
	};

	/* Extract irq number (if any) from hardware node */
	len = IrqTupleSize;
	if (get_res("irq", res_space, &len) != BEF_OK || len < 1) {
		node_irq = RMSC_NO_INTERRUPTS;
	} else {
		node_irq = res_space[0];
	}

	/* Prevent device interrupts from happening */
	if (node_irq != RMSC_NO_INTERRUPTS)
		irq_mask(node_irq, 1);

	/*
	 * If the driver requested an interrupt channel, make sure it
	 * matches the DevConf one.
	 */
	if (irq != RMSC_NO_INTERRUPTS) {
		/* Validate IRQ number for array lookup */
		if (irq < 0 || irq > 15) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
			    ("configure routine returned bad IRQ 0x%x\n"));
			return (BEF_FAIL);
		}
		if (node_irq == RMSC_NO_INTERRUPTS) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("get_res no irq, config %x\n", irq));
			return (BEF_FAIL);
		}
		if (node_irq != irq) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("get_res reported irq %x, config %x\n",
				node_irq, irq));
			return (BEF_FAIL);
		}

		/*
		 * Set up the vector for the requested interrupt to
		 * point to the matching SCSI interrupt stub.  The
		 * stubs cause the irq number to be passed as argument
		 * to scsi_device_interrupt.
		 */
		save_ds();	/* Necessary setup irq stubs */
		set_vector(irq_vectors[irq],
			MK_FP(get_code_selector(),
			(ushort)(irq_handlers[irq])));
	}

	Dcall(DBG_CONFIG, "initialize");
	if ((*scsi_init_data.initialize)(handle) != BEF_OK) {
		irq_mask(irq, 1);	/* Disable interrupts */
		Dfail(DBG_CONFIG | DBG_ERRS, "initialize");
		return (BEF_FAIL);
	}
	Dsucceed(DBG_CONFIG, "initialize");

	/* Allow device interrupts */
	if (irq != RMSC_NO_INTERRUPTS)
		irq_mask(irq, 0);

	return (BEF_OK);
}



/*
 * Search for SCSI devices attached to one SCSI bus of a SCSI adapter
 * and register them as bootable devices.
 */
Static int
scsi_device_search(ushort channel, rmsc_handle handle, char *device_name,
		int irq)
{
	ushort target;
	ushort lun;
	ushort ret;
	struct inquiry_data inqd;
	int answer = BEF_FAIL;

	/* Allow the driver to do its own device search */
	if (scsi_init_data.find_devices) {
		Dcall(DBG_SCAN, "find_devices");
		answer = (*scsi_init_data.find_devices)(handle);
		Dreturn(DBG_SCAN, "find_devices", answer);
		return (answer);
	}

	/*
	 * Loop should be terminated by being told that a target
	 * number is invalid.  Make sure we cannot loop indefinitely.
	 * Could still take a very long time ...
	 */
	for (target = 0; target != 0xFFFF; target++) {
		for (lun = 0; lun != 0xFFFF; lun++) {
			Dprintf(DBG_SCAN, ("checking target %d, LUN %d\n",
				target, lun));
			ret = scsi_inquire(handle, target, lun, &inqd);

			if (ret & RMSC_SOF_BAD_TARGET) {
				/*
				 * Target number is out of range for device.
				 * Device search is finished.
				 */
				Dprintf(DBG_SCAN,
					("bad target: search completed\n"));
				goto no_more_targets;
			}
			if (ret & RMSC_SOF_NO_TARGET) {
				/*
				 * Target is not present or not valid.  Go
				 * to next target.
				 */
				Dprintf(DBG_SCAN, ("target not present\n"));
				break;
			}
			if (ret & RMSC_SOF_BAD_LUN) {
				/*
				 * Lun is out of range.  Go to next
				 * target.
				 */
				Dprintf(DBG_SCAN,
					("bad LUN: finished with target\n"));
				break;
			}
			if ((ret & RMSC_SOF_COMPLETED) == 0 ||
					(ret & RMSC_SOF_GOOD) == 0) {
				/*
				 * Command failed on this lun.  Proceed
				 * with next lun.
				 */
				Dprintf(DBG_SCAN, ("LUN not present\n"));
				continue;
			}
			if (inqd.inqd_pdt == INQD_PDT_NOLUN) {
				/*
				 * Inquiry data gave "no such lun".
				 * Proceed with next lun.
				 */
				Dprintf(DBG_SCAN, ("LUN not present\n"));
				continue;
			}
			Dprintf(DBG_SCAN, ("valid device found\n"));
			if (scsi_register(handle, channel, device_name, target,
					lun, &inqd) == BEF_OK)
				answer = BEF_OK;
		}
	}
no_more_targets:
	return (answer);
}



/*
 * Build a bdev_info structure for a SCSI device and register the
 * device as a bootable device.
 */
Static int
scsi_register(rmsc_handle handle, ushort channel, char *device_name,
		ushort target, ushort lun, struct inquiry_data *inqd)
{
	struct bdev_info info;
	DWORD res_space[MaxTupleSize];
	DWORD len;
	int i;

	memset((char *)&info, 0, sizeof (info));
	len = PortTupleSize;
	if (get_res("port", res_space, &len) != BEF_OK || len < 1) {
		info.base_port = 0;
	} else {
		info.base_port = (ushort)res_space[0];
	}

	info.MDBdev.scsi.targ = target;
	info.MDBdev.scsi.lun = lun;
	memcpy(info.vid, inqd->inqd_vid, 8);
	info.blank1 = ' ';
	memcpy(info.pid, inqd->inqd_pid, 16);
	info.blank2 = ' ';
	memcpy(info.prl, inqd->inqd_prl, 4);
	info.term = 0;
	info.MDBdev.scsi.pdt = inqd->inqd_pdt;
	info.MDBdev.scsi.dtq = inqd->inqd_dtq;
	info.version = MDB_VERS_CURRENT;

	/*
	 * Set the block size to "unknown".  The first read will
	 * determine the blocksize.
	 */
	info.MDBdev.scsi.bsize = UNKNOWN_BSIZE;

	/* mark this MDB device as a SCSI HBA */
	info.dev_type = MDB_SCSI_HBA;

	memset(info.hba_id, ' ', 8);
	i = strlen(device_name);
	if (i > 8)
		i = 8;
	memcpy(info.hba_id, device_name, i);

	len = NameTupleSize;
	if (get_res("name", res_space, &len) != BEF_OK || len < 2) {
		Dprintf(DBG_REG | DBG_ERRS,
			("get_res failed for \"name\"\n"));
		return (BEF_FAIL);
	}
	if (res_space[1] == RES_BUS_PCI) {
		info.pci_valid = 1;
		info.pci_ven_id = RES_PCI_NAME_TO_VENDOR(res_space[0]);
		info.pci_vdev_id = RES_PCI_NAME_TO_DEVICE(res_space[0]);

		len = AddrTupleSize;
		if (get_res("addr", res_space, &len) != BEF_OK || len < 1) {
			Dprintf(DBG_REG | DBG_ERRS,
				("get_res failed for \"addr\"\n"));
			return (BEF_FAIL);
		}
		info.pci_bus = PCI_COOKIE_TO_BUS((ushort)res_space[0]);
		info.pci_dev = PCI_COOKIE_TO_DEV((ushort)res_space[0]);
		info.pci_func = PCI_COOKIE_TO_FUNC((ushort)res_space[0]);
	}

	/*
	 * mscsi devices need a bootpath segment of "mscsi@CHANNEL_NUMBER,0".
	 * For pci devices the mscsi segment must be preceded by
	 * "pciVENDOR,DEVICE@SLOT/".
	 */
	if (scsi_init_data.flags & RMSC_SCSI_MSCSI) {
		if (info.pci_valid) {
			sprintf(info.user_bootpath, "pci%x,%x@%x",
				info.pci_ven_id, info.pci_vdev_id,
				(ushort)res_space[0] >> 3);
			if (info.pci_func) {
				sprintf(info.user_bootpath +
					strlen(info.user_bootpath),
					",%x", info.pci_func);
			}
			strcpy(info.user_bootpath +
				strlen(info.user_bootpath), "/");
		}
		sprintf(info.user_bootpath + strlen(info.user_bootpath),
			"mscsi@%x,0", channel);
	}

	if (scsi_init_data.modify_dev_info) {
		Dcall(DBG_REG, "modify_dev_info");
		if ((*scsi_init_data.modify_dev_info)(handle, &info) !=
				BEF_OK) {
			Dreturn(DBG_REG | DBG_ERRS,
				"modify_dev_info", BEF_FAIL);
			return (BEF_FAIL);
		}
		Dreturn(DBG_REG, "modify_dev_info", BEF_OK);
	}

	return (rmsc_register(handle, &info));
}



/*
 * Handle a SCSI adapter device interrupt.  The interrupt vector
 * points to an assembler language stub that establishes normal
 * driver context before calling this routine.  This routine
 * in turn calls the hardware-specific device interrupt routine,
 * if any.
 */
Static void
scsi_device_interrupt(int irq)
{
	ushort old_mask;

	/* Mask off device interrupts, allow other interrupts, call handler */
	old_mask = irq_mask(irq, 1);
	splx(RMSC_INTR_ENABLE);

	/*
	 * Pass on the interrupt if there is a handler, otherwise
	 * try to leave future interrupts masked off.
	 */
	if (scsi_init_data.device_interrupt) {
		/* These messages are brief to minimize timing effect */
		Dprintf(DBG_INTR | DBG_CALL, ("I"));
		(*scsi_init_data.device_interrupt)(irq);
		Dprintf(DBG_INTR | DBG_CALL, ("i"));
	} else
		old_mask = 1;

	/* Prevent all interrupts, send the EOI(s), restore device mask */
	splhi();
	if (irq > 7)
		outb(SPIC_CMD, PIC_EOI);
	outb(MPIC_CMD, PIC_EOI);
	irq_mask(irq, old_mask);
}



/*
 * Reset the SCSI bus containing the specified device.
 * Called from the generic layer in response to an INT 13
 * device initialization request.
 */
Static int
scsi_init(rmsc_handle handle, struct bdev_info *info)
{
	struct scsi_op op;
	int answer;

	memset(&op, 0, sizeof (op));
	op.request_flags = RMSC_SFL_BUS_RESET;
	Dcall(DBG_OP, "scsi_op");
	answer = (*scsi_init_data.scsi_op)(handle, &op);
	Dreturn(DBG_OP, "scsi_op", answer);
	return (((answer == BEF_OK) &&
		(op.result_flags & RMSC_SOF_COMPLETED)) ? BEF_OK : BEF_FAIL);
}



/*
 * Issue a SCSI inquiry command to the specified device.
 */
Static ushort
scsi_inquire(rmsc_handle handle, ushort target, ushort lun,
		struct inquiry_data *inqd)
{
	struct scsi_op op;

	memset(&op, 0, sizeof (op));
	op.request_flags = RMSC_SFL_DATA_IN;
	op.cmd_len = 6;
	op.scsi_cmd[0] = SC_INQUIRY;
	op.scsi_cmd[1] = lun << 5;
	op.scsi_cmd[4] = sizeof (struct inquiry_data);
	op.transfer_len = sizeof (struct inquiry_data);
	op.target = target;
	op.lun = lun;
	op.buffer = (char far *)inqd;
	if (scsi_op_retry(handle, &op) != BEF_OK)
		return (RMSC_SOF_BAD_TARGET);
	return (op.result_flags);
}



/*
 * Read data from the specified device.  Called from the generic
 * layer in response to an INT 13 READ request.  The 'block' and
 * 'count' arguments refer to 512-byte sectors.
 * The deblock flag is set to TRUE when scsi_read is called from
 * BEF_EXTREAD case in main_service().
 */
Static int
scsi_read(rmsc_handle handle, struct bdev_info *info, ulong block,
		ushort count, char far *buffer, ushort deblock)
{
	ushort chunk_offset;
	ushort chunk_sectors;

	/*
	 * If this is the first read attempt we will not yet
	 * have determined the block size for the device.
	 * Try to do so now.
	 */
	if (info->MDBdev.scsi.bsize == UNKNOWN_BSIZE)
		scsi_bsize(handle, info);

	if (!deblock)
		return (scsi_block_read(handle, info, block, count, buffer));

	switch (info->MDBdev.scsi.bsize) {
	default:
		break;
	case 512:
		return (scsi_block_read(handle, info, block, count, buffer));
	case 2048:
		/*
		 * Caller has requested some number of 512-byte blocks.
		 * Device uses 2K blocks.  Might have to do as many as
		 * 3 reads.  1) read a 2K block into a local buffer and
		 * copy some of it to real buffer.  2) read some number
		 * of 2K blocks directly into real buffer.  3) read a 2K
		 * block into a local buffer and copy some of it to real
		 * buffer.
		 */
		while (count > 0) {
			chunk_offset = (block & 3);
			if (chunk_offset || count < 4) {
				/*
				 * Chunk starts in mid 2K block or doesn't
				 * reach end of it: case 1) or 3) above.
				 */
				if (scsi_block_read(handle, info,
						block >> 2, 1,
						block_buffer) != BEF_OK)
					return (BEF_FAIL);
				if (4 - chunk_offset < count)
					chunk_sectors = 4 - chunk_offset;
				else
					chunk_sectors = count;
				memcpy(buffer,
					block_buffer + chunk_offset * 512,
					chunk_sectors * 512);
			} else {
				/*
				 * Chunk is integral 2K blocks: case 2) above.
				 */
				chunk_sectors = (count & ~3);
				if (scsi_block_read(handle, info, block >> 2,
						count >> 2, buffer) != BEF_OK)
					return (BEF_FAIL);
			}

			block += chunk_sectors;
			count -= chunk_sectors;
			buffer += chunk_sectors * 512;
		}
		return (BEF_OK);
	}
	return (BEF_FAIL);
}


/*
 * Implement "Extended Get Parms" (Int 13 function 48)
 */

/* default geometry (if not overridden by BIOS) for (EXT)GETPARMS */

#define	UNKNOWN_BSIZE		0
#define	DEFAULT_NUMHEADS	256
#define	DEFAULT_NUMDRIVES	1
#define	DEFAULT_NUMCYLS		1024
#define	DEFAULT_NUMSECS		63

Static int
scsi_extgetparms(rmsc_handle handle, struct bdev_info *info,
	struct ext_getparm_resbuf far *resbuf)
{
	if (resbuf->bufsize < 26) {
		return (BEF_FAIL);
	} else if (resbuf->bufsize < 30) {
		resbuf->bufsize = 26;
	} else {
		resbuf->bufsize = 30;
	}

	if (info->MDBdev.scsi.bsize == UNKNOWN_BSIZE)
		scsi_bsize(handle, info);

	/* Note: can't handle more than 2^32-1 blocks */
	resbuf->num_secs_lo = scsi_ret_ulreadcap(handle, info);
	resbuf->num_secs_hi = 0;

	if (resbuf->num_secs_lo <= 0)
		return (BEF_FAIL);

	/* Fake physical geometry by using logical hds and secs */
	if (info->heads == 0 || info->secs == 0) {
		resbuf->heads = (ulong)DEFAULT_NUMHEADS;
		resbuf->secs = (ulong)DEFAULT_NUMSECS;
	} else {
		resbuf->heads = info->heads;
		resbuf->secs = info->secs;
	}

	resbuf->cyls = resbuf->num_secs_lo /
		(resbuf->heads * resbuf->secs);

	resbuf->bytes_per_sec = info->MDBdev.scsi.bsize;

	if (resbuf->bufsize == 30) {
		resbuf->dpte = MK_FP(0xFFFF, 0xFFFF);
	}

	resbuf->info_flags = 0x0000;

	/* If the drive is large enough, heads/secs/cyls not valid */

	if (resbuf->num_secs_lo <= MAXBLOCKS_FOR_GEOM) {
		resbuf->info_flags |= INFO_PHYSGEOM_VALID;
	}

	if ((info->MDBdev.scsi.pdt & INQD_PDT_DMASK) == INQD_PDT_ROM) {
		resbuf->info_flags |= INFO_REMOVABLE;
	}

	return (BEF_OK);
}

/*
 * Attempt to determine the SCSI blocksize for the specified device.
 */
Static void
scsi_bsize(rmsc_handle handle, struct bdev_info *info)
{
	struct readcap_data readcap_data;
	ushort i;

	info->MDBdev.scsi.bsize = UNKNOWN_BSIZE;

	/* First just try a straight SI_READCAP command */
	if (scsi_readcap(handle, info, &readcap_data) == BEF_OK) {
		i = (readcap_data.rdcd_bl[3] | (readcap_data.rdcd_bl[2] << 8));
		if (readcap_data.rdcd_bl[0] == 0 &&
				readcap_data.rdcd_bl[1] == 0)
			info->MDBdev.scsi.bsize = i;
		return;
	}

	/* Give up if not a removable device */
	if ((info->MDBdev.scsi.dtq & INQD_DTQ_RMB) == 0) {
		return;
	}

	/*
	 * Simple SI_READCAP failed.  Try motor start, lock, readcap, stop,
	 * unlock sequence.  Ignore failure to start motor.
	 */
	for (i = 0; i < 10; i++) {
		if (scsi_motor(handle, info, 1) == BEF_OK) {
			break;
		}
	}

	scsi_lock(handle, info, 1);
	if (scsi_readcap(handle, info, &readcap_data) == 0) {
		i = (readcap_data.rdcd_bl[3] |
			(readcap_data.rdcd_bl[2] << 8));
		if (readcap_data.rdcd_bl[0] == 0 &&
				readcap_data.rdcd_bl[1] == 0)
			info->MDBdev.scsi.bsize = i;
	}

	/*
	 * If we still do not know the blocksize it is probably because:
	 *		a) there is no medium in the drive or
	 *		b) the drive does not support the SX_READCAP command
	 *
	 * If the device is a CDROM drive, attempt a read using a blocksize
	 * of 2048 (the normal blocksize for such devices).  If the read
	 * appears to succeed, and changes the last word of the buffer,
	 * assume case b) and a blocksize of 2048.  Otherwise assume case a).
	 */
	if (info->MDBdev.scsi.bsize == UNKNOWN_BSIZE &&
			(info->MDBdev.scsi.pdt & INQD_PDT_DMASK) ==
			INQD_PDT_ROM) {
		info->MDBdev.scsi.bsize = 2048;
		*(int *)&block_buffer[2046] = 0xAFB0;
		if (scsi_block_read(handle, info, 0L, 1,
				block_buffer) != BEF_OK ||
				*(int *)&block_buffer[2046] == 0xAFB0) {
			info->MDBdev.scsi.bsize = UNKNOWN_BSIZE;
		}
	}

	scsi_lock(handle, info, 0);
	scsi_motor(handle, info, 0);
}



/*
 * Read data from the specified device.  The 'block' and 'count'
 * arguments refer to the natural block size of the device.
 */
Static int
scsi_block_read(rmsc_handle handle, struct bdev_info *info, ulong block,
		ushort count, char far *buffer)
{
	struct scsi_op op;

	memset(&op, 0, sizeof (op));
	op.request_flags = RMSC_SFL_DATA_IN;
	op.cmd_len = 10;
	op.scsi_cmd[0] = SX_READ;
	op.scsi_cmd[1] = info->MDBdev.scsi.lun << 5;
	op.scsi_cmd[2] = (unchar)(block >> 24);
	op.scsi_cmd[3] = (unchar)(block >> 16);
	op.scsi_cmd[4] = (unchar)(block >> 8);
	op.scsi_cmd[5] = (unchar)block;
	op.scsi_cmd[7] = (unchar)(count >> 8);
	op.scsi_cmd[8] = (unchar)count;
	op.transfer_len = count * info->MDBdev.scsi.bsize;
	op.target = info->MDBdev.scsi.targ;
	op.lun = info->MDBdev.scsi.lun;
	op.buffer = buffer;
	if (scsi_op_retry(handle, &op) == BEF_OK &&
			(op.result_flags & RMSC_SOF_COMPLETED) &&
			op.scsi_status == S_GOOD) {
		return (BEF_OK);
	}
	return (BEF_FAIL);
}




/*
 * Return the unsigned long value of the readcap for use with BEF_EXTREAD.
 */
Static unsigned long scsi_ret_ulreadcap(rmsc_handle handle,
		struct bdev_info *info)
{
	struct readcap_data readcap_data;

	if (scsi_readcap(handle, info, &readcap_data) != BEF_OK) {
		return (0L);
	}

	return ((unsigned long)readcap_data.rdcd_lba[0] << 24 |
		    (unsigned long)readcap_data.rdcd_lba[1] << 16 |
		    (unsigned long)readcap_data.rdcd_lba[2] << 8 |
		    (unsigned long)readcap_data.rdcd_lba[3]) + 1;
}

/*
 * Issue a SCSI read capacity command to the specified device.
 */
Static int
scsi_readcap(rmsc_handle handle, struct bdev_info *info,
		struct readcap_data far *buffer)
{
	struct scsi_op op;

	memset(&op, 0, sizeof (op));
	op.request_flags = RMSC_SFL_DATA_IN;
	op.cmd_len = 10;
	op.scsi_cmd[0] = SX_READCAP;
	op.scsi_cmd[1] = info->MDBdev.scsi.lun << 5;
	op.transfer_len = sizeof (struct readcap_data);
	op.target = info->MDBdev.scsi.targ;
	op.lun = info->MDBdev.scsi.lun;
	op.buffer = (char far *)buffer;
	if (scsi_op_retry(handle, &op) == BEF_OK &&
			(op.result_flags & RMSC_SOF_COMPLETED) &&
			op.scsi_status == S_GOOD)
		return (BEF_OK);
	return (BEF_FAIL);
}



/*
 * Issue a SCSI lock/unlock command to the specified device.
 */
Static int
scsi_lock(rmsc_handle handle, struct bdev_info *info, int lock)
{
	struct scsi_op op;

	memset(&op, 0, sizeof (op));
	op.cmd_len = 6;
	op.scsi_cmd[0] = SC_REMOV;
	op.scsi_cmd[1] = info->MDBdev.scsi.lun << 5;
	op.scsi_cmd[4] = lock;
	op.transfer_len = sizeof (struct readcap_data);
	op.target = info->MDBdev.scsi.targ;
	op.lun = info->MDBdev.scsi.lun;
	if (scsi_op_retry(handle, &op) == BEF_OK &&
			(op.result_flags & RMSC_SOF_COMPLETED) &&
			op.scsi_status == S_GOOD)
		return (BEF_OK);
	return (BEF_FAIL);
}



/*
 * Issue a SCSI motor start/stop command to the specified device.
 */
Static int
scsi_motor(rmsc_handle handle, struct bdev_info *info, int start)
{
	struct scsi_op op;

	memset(&op, 0, sizeof (op));
	op.cmd_len = 6;
	op.scsi_cmd[0] = SC_STRT_STOP;
	op.scsi_cmd[1] = info->MDBdev.scsi.lun << 5;
	op.scsi_cmd[4] = start;
	op.transfer_len = sizeof (struct readcap_data);
	op.target = info->MDBdev.scsi.targ;
	op.lun = info->MDBdev.scsi.lun;
	if (scsi_op_retry(handle, &op) == BEF_OK &&
			(op.result_flags & RMSC_SOF_COMPLETED) &&
			op.scsi_status == S_GOOD)
		return (BEF_OK);
	return (BEF_FAIL);
}



/*
 *	Centralized routine for requesting device-dependent code to
 *	do SCSI operations.  This routine handles generic retries.
 *	All calls to device-dependent scsi_op routines should go
 *	through here unless they are for dummy operations such as
 *	bus reset or if there is some reason to see the raw result
 *	without any retries.
 *
 *	We will probably need to add other tests to this routine as
 *	we encounter SCSI targets with different idiosyncracies.
 */
Static int
scsi_op_retry(rmsc_handle handle, struct scsi_op *op)
{
	struct scsi_op rs_op;

retry:
	op->scsi_status = 0;
	op->result_flags = 0;
	op->ars_len = 0;
	Dcall(DBG_OP, "scsi_op");
	if ((*scsi_init_data.scsi_op)(handle, op) != BEF_OK) {
		Dfail(DBG_OP | DBG_ERRS, "scsi_op");
		return (BEF_FAIL);
	}
	Dsucceed(DBG_OP, "scsi_op");
	if ((op->result_flags & RMSC_SOF_COMPLETED) == 0)
		return (BEF_OK);

	switch (op->scsi_status) {
	case S_GOOD:
		/* Command completed normally */
		return (BEF_OK);
	case S_BUSY:
		/* Device busy.  Wait a bit then try again */
		drv_msecwait(1);
		goto retry;
	case S_CK_COND:
		break;
	default:
		/* Everything else we just give up */
		return (BEF_OK);
	}

	/*
	 * The original op gave CHECK CONDITION.  Issue a
	 * REQUEST SENSE unless it was done automatically.
	 */
	if ((op->result_flags & RMSC_SOF_ARS_DONE) == 0) {
		memset(&rs_op, 0, sizeof (rs_op));
		rs_op.request_flags = RMSC_SFL_DATA_IN;
		rs_op.cmd_len = 6;
		rs_op.scsi_cmd[0] = SC_RSENSE;
		rs_op.scsi_cmd[1] = (op->scsi_cmd[1] & 0xE0);
		rs_op.scsi_cmd[4] = 14;
		rs_op.transfer_len = 14;
		rs_op.target = op->target;
		rs_op.lun = op->lun;
		rs_op.buffer = op->ars_data;
		Dcall(DBG_OP, "scsi_op");
		if ((*scsi_init_data.scsi_op)(handle, &rs_op) != BEF_OK) {
			Dfail(DBG_OP | DBG_ERRS, "scsi_op");
			return (BEF_OK);
		}
		Dsucceed(DBG_OP, "scsi_op");
		if ((rs_op.result_flags & RMSC_SOF_COMPLETED) == 0)
			return (BEF_OK);
		if ((rs_op.result_flags & RMSC_SOF_GOOD) == 0)
			return (BEF_OK);
		op->ars_len = 14;
		op->result_flags |= RMSC_SOF_ARS_DONE;
	}

	/* Give up if not enough sense data to examine sense key */
	if (op->ars_len < 3)
		return (BEF_OK);

	switch (op->ars_data[2] & 0xF) {
#define	ASC_INDEX 12
#define	ASQ_INDEX 13
	case KEY_NOT_READY:
		/* NOT READY with ASC 4 and ASQ 1 means "becoming ready" */
		if (op->ars_len >= 14 && op->ars_data[ASC_INDEX] == 4 &&
				op->ars_data[ASQ_INDEX] == 1) {
			drv_msecwait(1);
			goto retry;
		}
		break;
	case KEY_UNIT_ATTENTION:
		goto retry;
	}

	/*
	 * Did not know how to handle this sense key.
	 * Return the original result, possibly embellished
	 * by a request sense.
	 */
	return (BEF_OK);
}



#ifdef DEBUG
/* Routine for reporting absence of member assignments */
Static void
scsi_missing_assign(char *which)
{
	printf("SCSI driver %s did not assign \"%s\" member.\n",
		scsi_init_data.driver_name ?
			scsi_init_data.driver_name : "UNNAMED", which);
}
#endif
