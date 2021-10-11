/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

#ident "@(#)joyst.c	1.1	97/03/13 SMI"

/*
 * Code to implement a probe-only realmode driver for a joystick (game
 * port) device in terms of the generic driver interface.
 *
 * In addition to being the source for the real driver for the joystick
 * device, this code is also intended as a sample probe-only driver.
 *
 * This driver must implement a stack and a driver_init routine to
 * satisfy the external references in the generic driver interface.
 * It must supply a legacy probe routine for probing for devices.
 *
 * The system can contain only one joystick, at a fixed address.  Attempt
 * to reserve the address.  If successful, check for the device.  If the
 * device is present, complete the device node, otherwise free it.
 *
 * For examples of more interesting legacy probe routines, look in
 * DRIVERS/NETWORK/PCN/PCN.C or DRIVERS/SCSI/AHA1540/AHA1540.C.
 */

#include <rmsc.h>

/* Allow stack size override from the makefile */
#ifndef STACKSIZE
#define	STACKSIZE 1000
#endif

ushort stack[STACKSIZE];
ushort stack_size = sizeof (stack);


/* Joystick always uses 8 bytes of address space at 0x200 */
#define	JOY_PORTS_START 0x200
#define	JOY_PORTS_LEN 8

Static long ports[PortTupleSize] = {JOY_PORTS_START, JOY_PORTS_LEN, 0};
Static long portcnt = PortTupleSize;

Static void joyst_legacy_probe(void);

int
driver_init(struct driver_init *p)
{
	/*
	 * Assign driver_init struct members defined in rmsc.h.
	 *
	 * For a half-BEF only the name and legacy probe routine
	 * are required.
	 */
	p->driver_name = "joyst";
	p->legacy_probe = joyst_legacy_probe;

	return (BEF_OK);
}


Static void
joyst_legacy_probe(void)
{
	int node_ok = 0;
	unsigned char probe_data;

	if (node_op(NODE_START) == NODE_OK) {
		/*
		 *  We've created a node, now check to see if the I/O
		 *  ports reserved for the joy stick are available.
		 */
		if (set_res("port", ports, &portcnt, 0) == RES_OK) {
			/*
			 * A simple read from base port + 1 returns 0xff
			 * for no game port, and 0xf0 or 0xf3 for a game
			 * port, assuming nobody is holding down any buttons.
			 */
			probe_data = inb(JOY_PORTS_START + 1);
			if (probe_data == 0xf0 || probe_data == 0xf3 ||
					probe_data == 0xfc)
				node_ok = 1;
		}
		node_op(node_ok ? NODE_DONE : NODE_FREE);
	}
}
