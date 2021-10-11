/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Realmode pseudo-driver for parallel ports:
 *
 *    This file contains code that "probe"s for standard ISA parallel ports
 *    and reserves appropriate bus resources for all parallel ports it finds.
 *
 *    Probing for LPT ports must occur after probes for bootable devices
 *    that may be connected via parallel ports (e.g, parallel port SCSI
 *    adapters) because if such devices are configured, LPT resources should
 *    be assigned to them rather than to non-existant printers.  On the other
 *    hand, LPT probing must occur before probes for internal adapters since
 *    many of these are capable of stealing parallel port IRQs and we don't
 *    want to let that happen if we really do have a printer installed.
 */

#ident "@(#)lpt.c   1.14   96/07/29 SMI"

#include <befext.h>
#include <string.h>
#include <stdio.h>
#include <biosmap.h>

static const struct { int adr, irq; } portmap[] =  {

	/* Default IRQ assignment for standard ISA parallel ports: */
	{0x378, 7 /* LPT1 */ },
	{0x278, 5 /* LPT2 */ },
	{0x3BC, 7 /* LPT3 */ }
};

#define	PMcnt (sizeof (portmap)/sizeof (portmap[0]))
#define	PPlen 4

port2idx(int Port)
{
	int j;

	for (j = 0; j < PMcnt; j++)
	  if (portmap[j].adr == Port)
	    return (j);
	return (0);
}

static int
probe(x)
{
	/*
	 *  Probe for parallel port:
	 *
	 *  We don't actually do any probing, we simply make sure that the
	 *  "x"th port address, and the IRQ associated with it, are available.
	 *  If so, we reserve these resources, set the function ID, and
	 *  return 0.
	 */

	unsigned long val[3];
	static unsigned long len[3] = { 1, 2, 3 };

	val[0] = portmap[x].adr;
	val[1] = PPlen;
	val[2] = 0;

	if (!set_res("port", val, &len[2], RES_WEAK)) {
		/*
		 *  Port is unassigned.  Assume there's a printer on the other
		 *  end and reserve an IRQ for it.
		 */

		val[0] = portmap[x].irq;
		val[1] = 0;

		if (!set_res("irq", &val[0], &len[1], RES_WEAK)) {
			/*
			 * The IRQ was successfully reserved. Return zero
			 * so caller will know to install the device node.
			 */

			return (0);
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
	 *  This is the realmode driver equivalent of a "main" routine.  Use
	 *  the "func"tion code to determine the nature of the processing we're
	 *  being asked to perform.
	 */

	int j;
	unsigned long port;

	if (func == BEF_LEGACYPROBE) {
		/*
		 *  Caller is requesting that we probe for ISA parallel ports.
		 *  We don't actually probe the I/O ports, however.  We just
		 *  read the LPT configuration out of the BIOS.
		 */

		for (j = 0; j < bdap->ParallelPorts; j++) {
			/*
			 *  BIOS data area gives the number of installed
			 *  parallel ports.  Check the I/O address of each
			 *  one to make sure the BIOS isn't lying.
			 */
			if (bdap->LptPort[j] && !node_op(NODE_START)) {
				/*
				 *  There appears to be a parallel port at the
				 *  "j"th port address.  Use the "probe" rou-
				 *  tine to assign resources.
				 */

				node_op(probe(port2idx(bdap->LptPort[j])) ?
					NODE_FREE : NODE_DONE|NODE_UNIQ);
			}
		}

		return (0);
	}

	return (BEF_BADFUNC);	/* Not an operation we support!		    */
}
