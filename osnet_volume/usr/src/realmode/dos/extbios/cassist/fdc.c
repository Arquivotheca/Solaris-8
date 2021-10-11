/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 *
 *  Realmode pseudo-driver for floppy disks:
 *
 *    This file contains code that "probe"s for an integrated floppy disk
 *    controller.  Probing for the integrated floppy controllers must occur
 *    after we've probed for all expansion devices that might provide floppy
 *    support at the standard port addresses (e.g, SCSI adapaters with built-
 *    in floppy support).
 */

#ident "<@(#)fdc.c	1.20    97/07/25 SMI>"
#include <befext.h>

/* Standard floppy controller ports:					    */
static unsigned long ports[] =
	{ 0x3F0, 6, 0, 0x3f7, 1, 0, 0x370, 6, 0, 0x377, 1, 0 };
static unsigned long irqs[]  = { 6, 0 };
static unsigned long dmas[]  = { 2, 0 };

#define	FCR_MSR	0x04	   /* Port offset for probing.			    */
#define	MS_RQM	0x80	   /* Main status register bits ..		    */
#define	MS_DIO	0x40
#define	MS_CB	0x10


static int
probe(int x)
{
	/*
	 *  Probe for floppy controller:
	 *
	 *  The caller provides an index ("x" argument) into the resource
	 *  lists above.  We check to see if the "x"th I/O port is available
	 *  and, if so, probe it for the presence of a floppy disk controller.
	 *  If a controller appears to be preset at that port, we then try
	 *  to assign resources (from the "x"th entries  in the "irqs" and
	 *  "dma"s lists).
	 *
	 *  Returns 0 if a controller is detected at the given port and all
	 *  resources are successfully assigned.
	 */

	unsigned long len = 3;

	if ((!set_res("port", &ports[x*6], &len, 0))  &&
	    (!set_res("port", &ports[(x*6)+3], &len, RES_SHARE))) {
		/*
		 *  The I/O ports are available, now check to see if there's
		 *  a floppy controller there.  We have to actually probe
		 *  the port because the BIOS can only tells us that a floppy
		 *  controller exists, not where to find it!
		 */

		unsigned char msr = inp((int)ports[(x*6)] + FCR_MSR);

		if (((msr & (MS_RQM+MS_DIO+MS_CB)) == MS_RQM) ||
					    ((msr & ~MS_DIO) == MS_CB)) {
			/*
			 *  Found a floppy controller; try to reserve the
			 *  corresponding IRQ and DMA channel.
			 */

			len = 2;

			if (!set_res("irq", irqs, &len, 0) &&
			    !set_res("dma", dmas, &len, 0)) {
				/*
				 *  It worked!  Return zero so caller will
				 *  know to install the device node.
				 */

				return (0);
			}
		}
	}

	return (-1);
}

int
dispatch(int func)
{
	/*
	 *  Driver Function Dispatcher:
	 *
	 *  This is the realmode driver equivalent of a "main" routine.
	 *  Normally, we'd call one of a number of support routines based
	 *  on the requested driver "func"tion.  Since we only have so little
	 *  to do, however, we do everything right here.
	 */

	if (func == BEF_LEGACYPROBE) {
		/*
		 *  Caller is requesting that we probe for floppy disks
		 *  supported thru the BIOS.
		 */

		int j = 0;

		while (j++ < (sizeof (ports)/(6 * sizeof (unsigned long)))) {
			/*
			 *  Check each of the various ports where a floppy
			 *  disk controller may reside (3F0 & 370).
			 */

			if (node_op(NODE_START) == 0) {
				/*
				 *  We've got a device node started, go probe
				 *  for the controller and dispose of the node
				 *  based on what we find.
				 */

				node_op(probe(j-1) ? NODE_FREE : NODE_DONE);
			}
		}
	}

	return (0);
}
