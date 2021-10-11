/*
 *  Copyright (c) 1996 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    This file contains a probe-only pseudo-bef that tries to determine
 *    if there's a game port (joystick) in the system.
 */

#ident "@(#)joyst.c   1.4   96/07/26 SMI"

#include <befext.h>

#define	JOY_PORTS_START 0x200
#define	JOY_PORTS_LEN 8

long ports[3] = {JOY_PORTS_START, JOY_PORTS_LEN, 0};
long portcnt = PortTupleSize;

int
dispatch(int func)
{
	int node_ok = 0;

	if (func != BEF_LEGACYPROBE) {
		return (BEF_BADFUNC);	/* Not an operation we support! */
	}

	if (node_op(NODE_START) == NODE_OK) {
		/*
		 *  We've created a node, now check to see if the I/O
		 *  ports reserved for the joy stick are available.
		 */
		if (set_res("port", ports, &portcnt, 0) == RES_OK) {
			/*
			 * A simple read from port 0x201 returns 0xff for
			 * no game port, and usually 0xf0 for a game port,
			 * assuming nobody is holding down any buttons.
			 * Some joysticks seem to return 0xf3 - so
			 * we additionally check for those.
			 */
			if ((inp(JOY_PORTS_START + 1) == 0xf0) ||
			    (inp(JOY_PORTS_START + 1) == 0xf3) ||
			    (inp(JOY_PORTS_START + 1) == 0xfc)) {
				node_ok = 1;
			}
		}
		node_op(node_ok ? NODE_DONE : NODE_FREE);
	}
	return (BEF_OK);
}
