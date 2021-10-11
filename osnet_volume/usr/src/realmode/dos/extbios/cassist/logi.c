/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Realmode pseudo-driver for Logitech bus mice:
 *	Probe for Logitech bus mice.
 */

#ident "<@(#)logi.c	1.3	96/08/02 SMI>"
#include <befext.h>
#include <string.h>
#include <biosmap.h>
#include <stdio.h>


/*
 * Definitions to aid Logitech bus mouse probing:
 */
#define	LBM_PORT	0x23c	/* Base port for Logitech bus mouse */
#define	LBM_DATA	0x00	/* Port offset for data register */
#define	LBM_SIGP	0x01	/* Port offset for signature */
#define	LBM_CTRL	0x02	/* Port offset for adaptor control */
#define	LBM_INIT	0x03	/* Port offset for initialization */
#define	LBM_INI		0x91	/* Initialization command */
#define	LBM_SIG		0xa5	/* Signature value */

#define	LBM_INTR_DISABLE	0x10	/* disable interrupts cntrl bit */

/*
 * Probe for a Logitech bus mouse, if found return non-zero,
 * else, return zero.  Also reserves the irq for the mouse if
 * the probe was successful.
 */
static int
probe_ltm()
{
	int j;
	int irq = 0;
	unsigned char map = 0;
	unsigned char w, v;
	long val[2], len;
	long nbuttons;
	int plen;
	char far *fptr;

	outp(LBM_PORT+LBM_INIT, LBM_INI);
	/*
	 * Write a signature byte to the signature register
	 * then read it back, if we got it back we think the
	 * register is there. (Try all bits in the register)
	 */
	outp(LBM_PORT+LBM_SIGP, LBM_SIG);
	if ((inp(LBM_PORT+LBM_SIGP) != LBM_SIG))
		return (0);
	outp(LBM_PORT+LBM_SIGP, ~LBM_SIG);
	if (inp(LBM_PORT+LBM_SIGP) != (~LBM_SIG & 0xff))
		return (0);
	/*
	 * The signatures we wrote came back unaltered.
	 * Chances are reasonably good that this is, indeed,
	 * a Logitech mouse (we'll know for sure after probing
	 * for the IRQ).
	 */
	outp(LBM_PORT+LBM_CTRL, 0); /* enable interrupts */
	v = inp(LBM_PORT+LBM_CTRL);

	/*
	 * Look for a bit that is changing in the interrupt status reg.
	 * This will be the interrupt line used by the mouse.  The
	 * mouse interrupts the cpu every (unknown) milliseconds with
	 * a new position report.
	 * XXX - If machines get fast enough, the loop to 10000 below
	 * may not be long enough to always see a transition.
	 */
	for (j = 0; j < 10000; j++) {
		w = inp(LBM_PORT+LBM_CTRL);
		map |= (v ^ w);
		v = w;
	}

	/*
	 * The preceeding loop should terminate with only
	 * one bit would be set in the "map".
	 * The IRQ values associate backward with the
	 * bit set in the map:
	 *
	 *       Bit 0  ->  IRQ 5
	 *       Bit 1  ->  IRQ 4
	 *       Bit 2  ->  IRQ 3
	 *       Bit 3  ->  IRQ 2
	 */
	switch (map) {
	case 0x1:
		irq = 5;
		break;
	case 0x2:
		irq = 4;
		break;
	case 0x4:
		irq = 3;
		break;
	case 0x8:
		irq = 2;
		break;
	default:
		/*
		 * hmm, couldn't see a proper bit toggling
		 * fail the probe for the mouse
		 */
		return (0);
	}
	outp(LBM_PORT+LBM_CTRL, LBM_INTR_DISABLE);
	val[0] = irq;
	val[1] = 0;
	len = IrqTupleSize;
	if (set_res("irq", val, &len, RES_SET) != RES_OK)
		return (0);
	/*
	 * Assume a 3 button mouse
	 */
	nbuttons = 3;
	plen = 4;
	fptr = (char far *)&nbuttons;
	set_prop("mouse-nbuttons", &fptr, &plen, PROP_BIN);
	return (1); /* We found a Logitech bus mouse */
}

/*
 * Driver Function Dispatcher:
 *
 * This is the realmode driver equivalent of a "main" routine.  It
 * processes one of the four possible driver functions - the one that
 * does device probing.  The "install" functions are not supported.
 */
int
dispatch(int func)
{
	unsigned short port;
	long ports[3], portcnt;

	if (func != BEF_LEGACYPROBE)
		return (BEF_BADFUNC);	/* Not an operation we support! */
	/*
	 * Try A Logitech bus mouse at 0x23C.
	 */
	if (node_op(NODE_START) != NODE_OK)
		return (BEF_OK); /* Couldn't create a node */
	ports[0] = LBM_PORT;
	ports[1] = 4;
	ports[2] = 0;
	portcnt = PortTupleSize;
	/*
	 * Assume no mouse here if we can't reserve the ports
	 */
	if (set_res("port", ports, &portcnt, 0) == RES_OK && probe_ltm())
		node_op(NODE_DONE);
	else
		node_op(NODE_FREE);
	return (BEF_OK);
}
