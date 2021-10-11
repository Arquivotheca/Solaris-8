/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

#ident "@(#)proto.c	1.2	97/04/04 SMI"



/*
 * Prototype code to implement a real mode driver in terms
 * of the generic driver interface.
 *
 * This layer implements the driver_init routine and the "stack"
 * and "stack_size" variables expected by the generic layer.
 */
/*
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 *	=====================================================================
 *	This driver is a non-working driver to be used as a framework
 *	when writing drivers based on the generic realmode driver interface.
 *	Comments in this file that are intended for guidance in writing
 *	realmode drivers start and end with SAMPLE.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */



#include "rmsc.h"
#include "proto.h"


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
 * Various classes of messages can be turned on by setting flags in
 * proto_debug_flag.  Flags can be set by changing the definition of
 * PROTO_DEBUG_FLAG below, by setting PROTO_DEBUG_FLAG from the compiler
 * command line, or by writing the proto_debug_flag using a debugger
 * while running the driver.
 */
#ifdef DEBUG
#pragma message (__FILE__ ": << WARNING! DEBUG MODE >>")
#pragma comment (user, __FILE__ ": DEBUG ON " __TIMESTAMP__)

/* PROTO-specific flags.  See rmsc.h for common flags */
#define	DBG_PROTO1	0x0001	/* enable messages of first type */
#define	DBG_PROTO2	0x0002	/* enable messages of second type */

#ifndef PROTO_DEBUG_FLAG
#define	PROTO_DEBUG_FLAG	DBG_ALL
#endif
int proto_debug_flag = PROTO_DEBUG_FLAG;
#define	MODULE_DEBUG_FLAG proto_debug_flag

#endif /* DEBUG */



/* Prototypes of routines called from the generic layer */
Static int proto_configure(void);
Static void proto_device_interrupt(int);
Static int proto_init(rmsc_handle, struct bdev_info *);
Static void proto_legacy_probe(void);
Static int proto_read(rmsc_handle, struct bdev_info *, ulong, ushort,
	char far *);



/*
 * Initialize the driver.
 */
int
driver_init(struct driver_init *p)
{
	int ret = BEF_OK;

	/*
	 * Assign driver_init struct members defined in rmsc.h.
	 */
	p->driver_name = "proto";
	p->legacy_probe = proto_legacy_probe;
	p->configure = proto_configure;
	p->init = proto_init;
	p->read = proto_read;

	return (ret);
}



/*
 *	Legacy probe routine.
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
 *	calling rel_res.
 *	=====================================================================
 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
 */
Static void
proto_legacy_probe(void)
{
	/*
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 *	==============================================================
	 *	Implement this routine if the driver supports any legacy
	 *	devices.  If the driver does not support legacy devices,
	 *	remove this stub and the corresponding assignment in
	 *	driver_init.
	 *	==============================================================
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 */
}



/*
 * Configure the devices supported by this driver by using node_op
 * to scan the list of device nodes.  For each node found, register
 * any associated bootable devices.
 */
Static int
proto_configure(void)
{
	int ret = BEF_FAIL;	/* Haven't configured any devices yet */

	while (node_op(NODE_START) == NODE_OK) {

		/*
		 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
		 *	================================================
		 *	For each device node, read the contents using get_res
		 *	and determine whether the device is installed and is
		 *	configured as described.  If it is, and the device is
		 *	a network adapter fill in a bdev_info structure for
		 *	the device and call rmsc_register to register it for
		 *	booting.  If the device is a disk controller, search
		 *	for attached devices, fill in a bdev_info structure
		 *	for each bootable device and call rmsc_register.
		 *	Return the device code for the last device registered,
		 *	or BEF_FAIL if no devices were registered.
		 *	================================================
		 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
		 */

		node_op(NODE_DONE);
	}

	return (ret);
}



/*
 * Handle an adapter device interrupt.  The interrupt vector
 * points to an assembler language stub that establishes normal
 * driver context before calling this routine.
 */
Static void
proto_device_interrupt(int irq)
{
	ushort old_mask;

	/* Mask off device interrupts, allow other interrupts, call handler */
	old_mask = irq_mask(irq, 1);
	splx(RMSC_INTR_ENABLE);

	/*
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 *	==============================================================
	 *	Handle the device interrupt.  For a disk controller, this
	 *	typically means setting a flag to indicate that an operation
	 *	has completed.  For a network controller the interrupt
	 *	handler needs to clean up after packet transmission and handle
	 *	packet reception and other events.
	 *	==============================================================
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 */

	/* Prevent all interrupts, send the EOI(s), restore device mask */
	splhi();
	if (irq > 7)
		outb(SPIC_CMD, PIC_EOI);
	outb(MPIC_CMD, PIC_EOI);
	irq_mask(irq, old_mask);
}



/*
 * Reset the disk subsystem if the driver is for disk
 * controllers.  Select the device for booting if the
 * driver is for network adapters.
 *
 * Called from the generic layer in response to an INT 13
 * device initialization request.
 */
Static int
proto_init(rmsc_handle handle, struct bdev_info *info)
{
	/*
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 *	==============================================================
	 *	Implement this routine for all drivers.
	 *	==============================================================
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 */
	return (BEF_FAIL);
}



/*
 * Read data from the specified device.
 */
Static int
proto_read(rmsc_handle handle, struct bdev_info *info, ulong block,
		ushort count, char far *buffer)
{
	/*
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 *	==============================================================
	 *	Implement this routine if the driver is for a disk device.  If
	 *	the driver is for a network adapter, omit this routine and the
	 *	line that assigns it in driver_init.
	 *	==============================================================
	 *	SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE SAMPLE
	 */
	return (BEF_FAIL);
}
