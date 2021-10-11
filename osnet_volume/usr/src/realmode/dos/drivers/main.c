/*
 *  Copyright (c) 1999 by Sun Microsystems, Inc.
 *  All rights reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)main.c	1.26	99/11/25 SMI"
 */

/*
 * Code to implement the generic driver interface.
 *
 * This layer implements the basic framework for a realmode driver
 * such that the external details of the realmode driver definition
 * are already handled and driver developers can write to a simpler
 * internal interface.
 *
 * If you are writing a realmode driver for a SCSI or network adapter,
 * you should use the SCSI or network interface rather than this
 * generic interface.
 *
 * If you are writing a driver for the first of a new class of devices,
 * you should consider creating a new common layer for that class of
 * devices, analagous to the SCSI and network layers.
 *
 * If you are writing a driver for any other kind of device, or a
 * "half-BEF" for a non-bootable device, you should use this generic
 * interface.
 *
 * For more details see the "Realmode Drivers" white paper.
 */



#include <rmsc.h>



/* Structure to be filled in by driver_init */
Static rmsc_driver_init driver_init_data;



/* Structure and list head for list of registered devices */
struct device_info {
	struct device_info *next;
	ushort device_code;
	rmsc_handle handle;
	struct bdev_info info;
};
Static struct device_info *device_list_head;
Static ushort myfirstdev, mylastdev;
Static ushort checkpointdev;



/*
 * Variables used for implementing calloc().
 *
 * The boot subsystem allocates sufficient memory to hold the driver up
 * to _end.  The driver framework attempts to extend the allocated block
 * so that the extra space is available for allocation within the driver
 * via calloc.  Any unused space is freed before returning.
 */
extern struct bef_interface far *befvec;
extern int _end[];
Static ushort next_alloc = (unsigned)_end;
Static ushort end_alloc = (unsigned)_end;



/*
 * Variable used for keeping track of error that affect checkpointing.
 */
Static ushort calloc_error;
Static ushort bootcode_error;



/*
 * Save location for the original INT 13 vector for passing on
 * service calls if the request is not for a device handled by
 * this driver.
 */
long oldvect;



/*
 * Definitions for the range of boot device codes available for
 * realmode drivers.
 */
#define	FIRST_BOOT_DEVNUM 0x10   /* MDB uses 10-7Fh to dynamically */
#define	LAST_BOOT_DEVNUM  0x7F   /* assign boot device codes */



/* Prototypes of local routines */
Static int do_driver_init(void);
Static void first_free_dev(void);
Static void install_driver(void);
Static void missing_assign(char *);
Static int null_read(rmsc_handle, struct bdev_info *, long, ushort, char far *);


/* Prototype of non-local routine not defined in rmsc.h */
extern int bef_ident(ushort, char far **, char far **);



/*
 * The following debug definitions work in conjunction with the
 * debug definitions in rmsc.h.
 *
 * Debugging facilities in this file are partly intended for debugging
 * the generic code itself and partly for helping to debug code
 * that implements a driver or driver class layer in terms of this code.
 *
 * Various classes of messages can be turned on by setting flags in
 * main_debug_flag.  Flags can be set by changing the definition of
 * MAIN_DEBUG_FLAG below, by setting MAIN_DEBUG_FLAG from the compiler
 * command line, or by writing the main_debug_flag using a debugger
 * while running the driver.
 */
#ifdef DEBUG

#pragma	message(__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment(user, __FILE__ ": DEBUG ON " __TIMESTAMP__)

#define	DBG_DISPATCH	0x0001	/* enable messages from dispatch() */

#ifndef MAIN_DEBUG_FLAG
#define	MAIN_DEBUG_FLAG	DBG_ERRS
#endif

int main_debug_flag = MAIN_DEBUG_FLAG;
#define	MODULE_DEBUG_FLAG main_debug_flag

#endif /* DEBUG */


/*
 * All driver initialization calls are directed here by the
 * assembler-language wrapper.  Service calls go to main_service.
 */
int
dispatch(int func)
{
	int ret = BEF_FAIL;	/* Prepare for the worst */
	int sub_ret;
	extern int (*putvec)(int);

	Dprintf(DBG_DISPATCH, ("Entering main module dispatch routine\n"));
	if (do_driver_init() == BEF_OK)
	    switch (func) {
		case BEF_LEGACYPROBE:
			Dcall(DBG_DISPATCH, "legacy_probe\n");
			if (driver_init_data.legacy_probe)
				(*driver_init_data.legacy_probe)();
			Dback(DBG_DISPATCH, "legacy_probe\n");
			ret = BEF_OK;
			break;

		case BEF_INSTALLONLY:
			Dcall(DBG_DISPATCH, "configure");
			first_free_dev();
			if (driver_init_data.configure)
				sub_ret = (*driver_init_data.configure)();
			Dreturn(DBG_DISPATCH, "configure", sub_ret);
			if (sub_ret == BEF_OK) {
				Dcall(DBG_DISPATCH, "install_driver");
				install_driver();
				Dback(DBG_DISPATCH, "install_driver");
				ret = mylastdev;
			}
			break;
	    }

	Dprintf(DBG_DISPATCH,
		("Returning 0x%x from main module dispatch routine\n", ret));

	/*
	 * Prevent putc from going through the callback vector
	 * because the caller's presence can no longer be guaranteed.
	 */
	putvec = 0;
	return (ret);
}



/*
 * Call on the hardware-specific or device class layer code to
 * initialize the driver.
 *
 * To save space in the final driver, most of the tests on the
 * values assigned in the initialization structure are done
 * only under DEBUG.
 */
Static int
do_driver_init(void)
{
	static driver_init_flag = 0;
	int ret = BEF_OK;
	ushort new_size;
	ushort good_size;
	ushort bad_size;

	Dprintf(DBG_INIT, ("Entering do_driver_init\n"));

	/* Prevent multiple initializations */
	if (driver_init_flag != 0) {
		Dprintf(DBG_ERRS | DBG_INIT,
			("do_driver_init: already done\n"));
		return (BEF_OK);
	}

	/*
	 * Older versions of bootconf do not support memory allocation.
	 * Newer ones will have allocated just enough memory for the static
	 * portion of the driver (including BSS) to fit.  Try to adjust
	 * the block to allow for a full 64K of data so that there
	 * is memory available for allocation using calloc.
	 */
	if (befvec->version >= BEF_IF_VERS_MEM_SIZE) {
		new_size = get_data_selector() + 0xFFF - befvec->mem_base;

		Dprintf(DBG_INIT,
			("do_driver_init: base %x, ds %x, "
			"size %x, desired size %x\n",
			befvec->mem_base, get_data_selector(),
			befvec->mem_size, new_size));

		if (new_size <= befvec->mem_size) {
			/*
			 * We already have at least as much space as we need.
			 * This does not normally happen but is possible
			 * with debug drivers.  Give back any extra memory.
			 */
			(void) mem_op(new_size);
		} else {
			/*
			 * We want more space than we have.  Try to get what
			 * we want, but settle for what we can get.
			 *
			 * mem_op() doesn't tell us how much we could get
			 * when it fails.  So binary search for the best we
			 * can do.
			 */
			if (mem_op(new_size) != BEF_OK) {
				good_size = befvec->mem_size;
				bad_size = new_size;
				while (bad_size > good_size + 1) {
					new_size = good_size +
						(bad_size - good_size) / 2;
					if (mem_op(new_size) == BEF_OK) {
						good_size = new_size;
					} else {
						bad_size = new_size;
						new_size = good_size;
					}
				}
			}
		}

		/* Determine the calloc() limit implied by the new size */
		end_alloc = (befvec->mem_base + new_size -
			get_data_selector()) << 4;

		Dprintf(DBG_INIT,
			("do_driver_init: original size %x, new size %x\n",
			befvec->mem_size, new_size));
	}

	memset(&driver_init_data, 0, sizeof (driver_init_data));
#ifdef DEBUG
	driver_init_data.read = null_read;
#endif /* DEBUG */

	Dcall(DBG_INIT, "driver_init");
	if (driver_init(&driver_init_data) != BEF_OK) {
		Dfail(DBG_INIT | DBG_ERRS, "driver_init");
		return (BEF_FAIL);
	}
	Dsucceed(DBG_INIT, "driver_init");

#ifdef DEBUG
	/* Report missing required items now rather than when used */
	if (driver_init_data.driver_name == 0) {
		missing_assign("driver_name");
		ret = BEF_FAIL;
	}
	if (driver_init_data.legacy_probe == 0 &&
			driver_init_data.configure == 0) {
		/*
		 * A regular driver must have a configure() routine and
		 * a half-BEF must have a legacy_probe.  Report both
		 * omissions because we do not know what was intended.
		 */
		missing_assign("legacy_probe");
		missing_assign("configure");
		ret = BEF_FAIL;
	}
	if (driver_init_data.configure != 0 && driver_init_data.init == 0) {
		/* A regular driver must have an init routine */
		missing_assign("init");
		ret = BEF_FAIL;
	}
#endif /* DEBUG */

	driver_init_flag = 1;
	Dprintf(DBG_INIT, ("Returning %s from do_driver_init\n",
		DBG_RET_STR(ret)));
	return (ret);
}



/*
 * main_service handles all INT 13 requests intercepted by the driver.
 * That includes requests that must be passed on to drivers that were
 * installed before this one.
 */
int
main_service(ushort ax, ushort bx, ushort cx, ushort dx, ushort si,
		ushort di, ushort es, ushort ds)
{
	ushort req_dev = (dx & 0xFF);
	ushort cylinder;
	unchar head, sector;
	ulong ul;
	struct device_info *device;
	DEV_INFO *info;

	ushort numblocks;
	ushort ofs;
	ushort seg;
	struct ext_getparm_resbuf far *resbuffer;
	struct ext_dev_addr_pkt far *readpkt;

	Dprintf(DBG_SERVICE,
		("Entering main_service in %s: request 0x%x, device 0x%x\n",
		driver_init_data.driver_name, ax >> 8, req_dev));

	/* Find the device structure for req_dev */
	for (device = device_list_head; device; device = device->next) {
		info = &device->info;
		if (device->device_code == req_dev)
			break;
		if (info->bdev >= 0x80 && req_dev == info->bdev) {
			Dprintf(DBG_SERVICE, ("Handling BIOS device\n"));
			break;
		}
	}
	if (device == 0) {	/* Didn't find the BIOS code */
		Dprintf(DBG_SERVICE,
			("Leaving main_service: not my device\n"));
		return (0);
	}

	Dprintf(DBG_SERVICE,
		("In main_service: handle 0x%lx, device 0x%x, port 0x%x\n",
		device->handle, device->device_code, info->base_port));

	switch (ax >> 8) {
	case BEF_IDENT:
		/*
		 * Return the bdev_info structure pointer in es:bs and
		 * the driver identification string in es:cx.
		 * DX returns a magic word because we seem not to be able
		 * to rely on all BIOSes to set the carry flag for calls
		 * to unsupported devices.
		 */
		bx = (unsigned short)info;
		cx = (unsigned short)driver_init_data.driver_name;
		dx = BEF_MAGIC;
		es = get_data_selector();

		Dprintf(DBG_SERVICE,
			("BEF_IDENT: bx %02x, cx %02x, dx %02x es %02x\n",
			bx, cx, dx, es));

		return (1);

	case BEF_READ:
		Dprintf(DBG_SERVICE, ("BEF_READ\n"));

		/*
		 * Isolate BIOS-style cyl/head/sector, convert sector
		 * to be 0-based, check head and sector for limits and
		 * calculate SCSI 512-byte sector offset.
		 */
		cylinder = (((cx & 0xC0) << 2) | ((cx & 0xFF00) >> 8));
		head = (unchar)((dx & 0xFF00) >> 8);
		sector = (unchar)((cx & 0x3F) - 1);

		/*
		 * Calculate the logical sector address using either the
		 * BIOS device parameters from the bdev_info structure or
		 * the standard MDB layout.
		 */
		if (req_dev == info->bdev) {
			ul = ((ulong) cylinder * (info->heads + 1) +
			    (ulong) head) * info->secs +
			    (ulong) sector; /* 1 based */
		} else {
			ul = ((ulong) cylinder * 255 +
			    (ulong) head) * 63 +
			    (ulong) sector; /* 1 based */
		}

		/* Hand off the read to the device-specific portion */
		Dcall(DBG_SERVICE, "read");
		if (driver_init_data.read == 0 ||
				(*driver_init_data.read)(device->handle, info,
				ul, ax & 0xFF, MK_FP(es, bx), 1) != BEF_OK) {
			Dfail(DBG_SERVICE | DBG_ERRS, "read");
			ax = 0xBB00;    /* "Undefined error" */
			return (-1);	/* Failure */
		}
		Dsucceed(DBG_SERVICE, "read");
		ax = 0;		/* "Success" */
		return (1);

	case BEF_GETPARMS:
		Dprintf(DBG_SERVICE, ("BEF_GETPARMS\n"));
		if (req_dev == info->bdev) {
			dx = ((info->heads & 0xff) << 8) | 1; /* one drive */
			cx = ((info->cyls & 0xff) << 8) |
			    ((info->cyls & 0x300) >> 2) |
			    (info->secs & 0x3f);
		} else {
			/*
			 * We cannot support head number 255 because some
			 * BIOSes increment the head number in the byte
			 * register to calculate the divisor/multiplier.
			 * For 255 that gives a result of 0.
			 */
			dx = 0xFE01;   /* Allow head 0-254, 1 drive */
			cx = 0xFFFF;   /* Allow cyl 0-1023, sector 1-63 */
		}
		ax = 0;	  /* good return */
		return (1);	  /* have handled int, don't chain */

	case BEF_RESET:
		Dprintf(DBG_SERVICE, ("INIT\n"));
		Dcall(DBG_SERVICE, "init");
		if ((*driver_init_data.init)(device->handle, info) != BEF_OK) {
			Dfail(DBG_SERVICE | DBG_ERRS, "init");
			ax = 0xBB00;    /* "Undefined error" */
			return (-1);	/* Failure */
		}
		Dsucceed(DBG_SERVICE, "init");
		ax = 0;
		return (1);

	case BEF_CHKEXT:

		if (info->dev_type != MDB_SCSI_HBA)
			goto error;

		/* BEGIN CSTYLED */
		/*
		 * Input:
		 *   AH = 41H
		 *   BX = 55AAH
		 *   DL = Drive number
		 *
		 * Output:
		 *
		 *  (Carry Clear)
		 *   AH = Extensions version
		 *     10h = 1.x
		 *     20h = 2.0/EDD-1.0
		 *     21h = 2.1/EDD-1.1
		 *     30h = EDD-3.0
		 *   AL = Internal Use only  (??)
		 *   BX = AA55H
		 *   CX = Interface support bit map
		 *     Bit	Description
		 *     -----	------------
		 *     0	1 - Fixed disk access subset
		 *     1	1 - Drive locking and ejecting subset
		 *     2	1 - Enhanced disk drive support subset
		 *     3-15	Reserved, must be 0
		 *
		 *   (Carry Set)
		 *    AH = Error code (01h = Invalid command)
		 */
		/* END CSTYLED */

		Dprintf(DBG_SERVICE, ("CHKEXT\n"));
		if (bx != 0x55AA) {
			ax = 0x0100 | (ax & 0xFF);
			return (-1);
		}

		/* EDD-1.1, "Fixed disk access", signature */
		ax = 0x2100 | (ax & 0x00FF);
		cx = 0x0001;
		bx = 0xAA55;

		return (1);

	case BEF_EXTREAD:

		if (info->dev_type != MDB_SCSI_HBA)
			goto error;

		Dprintf(DBG_SERVICE, ("EXTREAD\n"));
		/* Get the packet address */
		readpkt = (struct ext_dev_addr_pkt far *) MK_FP(ds, si);

		ofs = FP_OFF(readpkt->bufaddr);
		seg = FP_SEG(readpkt->bufaddr);

		if (readpkt->bufaddr == 0xFFFFFFFF ||
		    readpkt->lba_addr_hi != 0 ||
		    readpkt->numblocks > 0x7F) {
			/* We only handle 2^32, 4GB addrs, and 127 blocks */
			ax = 0x0100;
			return (-1);
		}

		/*
		 * Address is already in LBA, so no conversion needed like
		 * for BEF_READ.
		 */

		ul = (ulong) readpkt->lba_addr_lo;
		numblocks = (ushort) readpkt->numblocks;

		/* Hand off the read to the device-specific portion */
		Dcall(DBG_SERVICE, "read");
		if (driver_init_data.read == 0 ||
				(*driver_init_data.read)(device->handle, info,
				ul, numblocks, MK_FP(seg, ofs), 0) != BEF_OK) {
			Dfail(DBG_SERVICE | DBG_ERRS, "read");
			ax = 0xBB00;    /* "Undefined error" */
			readpkt->numblocks = 0;
			return (-1);	/* Failure */
		}
		Dsucceed(DBG_SERVICE, "read");
		ax = 0;
		return (1);

	case BEF_EXTGETPARMS:

		Dprintf(DBG_SERVICE, ("EXTGETPARMS\n"));
		resbuffer = (struct ext_getparm_resbuf far *)MK_FP(ds, si);

		Dcall(DBG_SERVICE, "extgetparms\n");
		if (driver_init_data.extgetparms == 0 ||
		    (*driver_init_data.extgetparms)(device->handle, info,
		    resbuffer) != BEF_OK) {
			Dfail(DBG_SERVICE | DBG_ERRS, "extgetparms");
			ax = 0x0100;
			return (-1);
		}
		Dsucceed(DBG_SERVICE, "extgetparms\n");
		ax = 0;
		return (1);

	case BEF_EXTWRITE:
	case BEF_EXTVERIFY:
	case BEF_EXTSEEK:

		/*
		 * These three need to be supported in order to claim
		 * support for some of the other functions.  As a safety
		 * precaution, we put these here to return failure.
		 *
		 * An alternative approach would be to not include these
		 * and let the default case handle this error.
		 */

		/*FALLTHROUGH*/

	error:
	default:
		Dprintf(DBG_SERVICE | DBG_ERRS,
			("Leaving main_service: bad request 0x%x\n", ax >> 8));
		ax = 0x100;	/* Return "bad command" for status */
		return (-1);
	}
}



/*
 * Register a device for INT 13 service.
 */
int
rmsc_register(rmsc_handle handle, struct bdev_info *info)
{
#ifndef DEVICE_ARRAY
#define	DEVICE_ARRAY 2
#endif
	static struct device_info device_array[DEVICE_ARRAY];
	static slots_used;
	struct device_info *new_device;
	ushort newdev;

	Dprintf(DBG_CONFIG, ("Entering rmsc_register\n"));
	/* Calculate the device number.  Fail if all numbers taken */
	if (mylastdev == 0)
		newdev = myfirstdev;
	else
		newdev = mylastdev + 1;
	if (newdev == LAST_BOOT_DEVNUM + 1) {
		bootcode_error = 1;
		Dprintf(DBG_CONFIG | DBG_ERRS,
			("Leaving rmsc_register: no device code available\n"));
		return (BEF_FAIL);
	}

	/*
	 * Grab a device slot from the table, if any are left,
	 * otherwise try to calloc one.
	 *
	 * Use of the table is to guarantee enough space to allow
	 * a driver to support a few devices even if it cannot
	 * allocate any memory.
	 */
	if (slots_used < DEVICE_ARRAY) {
		new_device = device_array + slots_used;
		slots_used++;
	} else {
		new_device = (struct device_info *)
			calloc(1, sizeof (struct device_info));
		/* Fail if out of space */
		if (new_device == 0) {
			Dprintf(DBG_CONFIG | DBG_ERRS,
				("Leaving rmsc_register: out of memory\n"));
			return (BEF_FAIL);
		}
	}

	mylastdev = newdev;

	/* Save the information */
	new_device->handle = handle;
	new_device->info = *info;
	new_device->device_code = newdev;
	new_device->info.bios_dev = (unchar)newdev;

	/* Add to device list (reverse order!) */
	new_device->next = device_list_head;
	device_list_head = new_device;

	Dprintf(DBG_CONFIG, ("Leaving rmsc_register: device registered\n"));
	return (BEF_OK);
}



/*
 * rmsc_checkpoint.  Maintain checkpointing to allow backing off from a
 * device node if we run out of space or boot codes or if the caller reports
 * an error.
 *
 * Checkpointing is optional.  If used, this routine should be called
 * during the configuration function after processing each device node.
 * If there has been no error (at present memory allocation failures are
 * the only kind), the checkpoint code records the last device code for
 * the node just processed.  If there has been an error, it restores the
 * last saved value.  Never fail the first node, because otherwise we will
 * never make progress.
 *
 * If this routine returns RMSC_CHKP_GOOD the caller should call preserve
 * the node by calling node_op(NODE_DONE) and continue processing any other
 * nodes.  If it returns RMSC_CHKP_STOP the caller should preserve the node
 * by calling node_op(NODE_DONE) but not process any more nodes.  If it
 * returns RMSC_CHKP_BAD the caller should discard the node by calling
 * node_op(NODE_FREE) and not process any more nodes.
 */
int
rmsc_checkpoint(int caller_error)
{
	static ushort first_node = 1;
	int error = (caller_error | calloc_error | bootcode_error);

	if (first_node) {
		first_node = 0;
		checkpointdev = mylastdev;
		/* Always preserve the first node, but stop on errors */
		return (error ? RMSC_CHKP_STOP : RMSC_CHKP_GOOD);
	}
	if (error) {
		mylastdev = checkpointdev;
		return (RMSC_CHKP_BAD);
		/*
		 * Bootconf has not been enhanced to run multiple instances
		 * of a driver when the driver cannot handle all the devices.
		 * So return RMSC_CHKP_STOP here to preserve as many attached
		 * devices as possible.  When bootconf can run multiple
		 * instances this value should change to RMSC_CHKP_BAD.
		 */
		return (RMSC_CHKP_STOP);
	}
	checkpointdev = mylastdev;
	return (RMSC_CHKP_GOOD);
}



/*
 * Simple version of calloc.  Allocates memory by appending
 * to the data segment.  Fails if request would reach 64K.
 * Does everything on 4-byte boundaries.  Allocated memory
 * cannot be freed.
 *
 * Allocation is not permitted after driver installation.
 */
char *
calloc(ushort nelem, ushort elsize)
{
#define	ROUND_UP_TO_EVEN(x)	(((x) + 3) & ~3)
	char *new_chunk = (char *)ROUND_UP_TO_EVEN(next_alloc);
	unsigned size = ROUND_UP_TO_EVEN(nelem * elsize);
	unsigned chunk_end = next_alloc + size;

	/*
	 * Fail if the request is for 0 (or overflows to 0 when rounded up),
	 * or there is not enough room.  Allocation is permitted only when
	 * there is space available at the end of the data segment.  Unused
	 * space is discarded when the driver is installed, so allocation
	 * after installation always fails.
	 */
	if (size == 0 || chunk_end > end_alloc || chunk_end < next_alloc) {
		calloc_error = 1;
		return (0);
	}

	next_alloc = chunk_end;

	/* Clear the memory to be returned */
	memset(new_chunk, 0, size);

	return (new_chunk);
}



/*
 * Perform the framework housekeeping to install the driver
 * and intercept INT 13 calls.
 */
Static void
install_driver()
{
	ushort new_size;
	extern newvect();

	/*
	 * Try to allocate one extra device structure and a little extra
	 * spare space before releasing unused calloc space.  If it becomes
	 * necessary to rerun the driver later because the boot device was
	 * powered off, this increases the chance of the driver being able to
	 * run successfully again in the same block of memory.  Use two calls
	 * to increase the likelihood of success of at least one.
	 */
	(void) calloc(1, sizeof (struct device_info));
	(void) calloc(1, 50);

	/*
	 * Attempt to adjust memory allocated to the driver to match what
	 * is actually used.  Don't bother to check the return value.
	 * There isn't much we can do if it fails.
	 *
	 * Note that if install_driver is not called, all memory is freed
	 * upon return from the driver.  That is why this code is here
	 * rather than in dispatch.
	 */
	if (end_alloc > next_alloc) {
		new_size = ((next_alloc + 15) >> 4) + get_data_selector() -
			befvec->mem_base;

		Dprintf(DBG_INIT,
			("install_driver: end_alloc %x, next_alloc %x, "
			"reducing size to %x\n",
			end_alloc, next_alloc, new_size));

		(void) mem_op(new_size);

		/* Prevent further allocation */
		end_alloc = next_alloc;
	}

	/* Save the old INT 13 vector for passing on calls to other drivers */
	get_vector(0x13, &oldvect);

	/* Intercept int13 calls */
	set_vector(0x13, (char far *)newvect);
}



/*
 * first_free_dev -- Interrogate all installed BIOS extensions to determine
 * the first available device code.
 */
Static void
first_free_dev(void)
{
	ushort	dev;
	char	far *contstr,
		far *devstr,
		far *s1,
		*s2;

	for (dev = FIRST_BOOT_DEVNUM; dev != LAST_BOOT_DEVNUM + 1; dev++) {
		if (bef_ident(dev, &contstr, &devstr))
			break;
	}
	myfirstdev = dev;
}



/* Routine for reporting absence of member assignments */
#ifdef DEBUG
Static void
missing_assign(char *which)
{
	printf("Driver %s did not assign \"%s\" member.\n",
		driver_init_data.driver_name ?
			driver_init_data.driver_name : "UNNAMED", which);
}
#endif /* DEBUG */



/* Default "read" routine to report error if called */
#ifdef DEBUG
Static int
null_read(rmsc_handle h, struct bdev_info *i, long l, ushort u, char far *p)
{
	missing_assign("read");
	return (BEF_FAIL);
}
#endif /* DEBUG */



/*
 * Read the interrupt vector numbered 'vector' and store its contents
 * at the specified address.  The address should really be a near pointer
 * to a far pointer to a routine.  Calling it a pointer to a long is
 * much clearer and less error prone.
 */
void
get_vector(unsigned short vector, long *address)
{
	long _far *vec;

	vec = (long _far *)(vector << 2);
	*address = *vec;
}



/*
 * Set the interrupt vector numbered 'vector' to the given value.
 * 'newval' should really be a far pointer to a routine.  But we
 * call it a char far pointer for simplicity.
 */
void
set_vector(unsigned short vector, char far *newval)
{
	long _far *vec;
	union {
		char far *cf;
		long l;
	} u;

	vec = (long _far *)(vector << 2);
	u.cf = newval;
	*vec = u.l;
}
