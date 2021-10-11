/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Realmode pseudo-driver for Microsoft bus mice:
 *	Probe for Microsoft and compatible bus mice.
 */

#ident "<@(#)msm.c	1.3	96/08/02 SMI>"
#include <befext.h>
#include <string.h>
#include <biosmap.h>
#include <stdio.h>


/*
 * Definitions to aid Microsoft bus mouse probing:
 */
#define	MAX_MSPORT	0x23c	/* Max port for Microsoft bus mouse */
#define	MIN_MSPORT	0x230	/* Min port for Microsoft bus mouse */
#define	MSM_SIG		0xde	/* Microsoft bus mouse signature */
#define	MSM_RESET	0x80	/* Mouse reset command */
#define	MSM_MDREG	0x07	/* Mode register def */
#define	MSM_DATA	0x01	/* Port offset for data regster */
#define	MSM_ID		0x02	/* Port offset for id regster */
#define	MSM_MODE0	0x00	/* Mode 0: 0 HZ - INTR = 0 */
#define	MSM_MODE1	0x06	/* Mode 1: 0 HZ - INTR = 1 */
#define	MSM_TIMER	0x10	/* Timer interrupt enable */

/*
 * Interrupt controller ports (for IRQ probing):
 */
#define	PIC1_PORT	0x20	/* Master interrupt controller port */
#define	PIC2_PORT	0xa0	/* Slave interrupt controller port */
#define	IRQ_MASTER	0xb8	/* Mask for probing master IRQ list */
#define	IRQ_SLAVE	0x02	/* Mask for probing slave IRQ list */
#define	READ_IRR	0x0a	/* Read interrupt request reg */
#define	READ_ISR	0x0b	/* Read interrupt status reg */

void
set_pic_imrs(unsigned short mask)
{
	/*
	 * Do master pic
	 */
	outp(PIC1_PORT+1, (mask & 0xff) | inp(PIC1_PORT+1));
	/*
	 * Do slave pic
	 */
	outp(PIC2_PORT+1, (mask >> 8) | inp(PIC2_PORT+1));
}

unsigned short
read_pic_irrs()
{
	unsigned short irrs;

	/*
	 * Do master pic
	 */
	outp(PIC1_PORT, READ_IRR);
	irrs = inp(PIC1_PORT);
	outp(PIC1_PORT, READ_ISR);
	/*
	 * Do slave pic
	 */
	outp(PIC2_PORT, READ_IRR);
	irrs |= inp(PIC2_PORT) << 8;
	outp(PIC2_PORT, READ_ISR);
	return (irrs);
}

/*
 * set the proper bit in the mask word for the given irq
 */
#define	imask(irq)	(1 << ((irq) == 2 ? 9 : (irq)))

/*
 * Probe for IRQ usage:
 *
 * For Microsoft bus mice, we force the mouse to generate an interrupt
 * and watch which interrupt line goes hi.
 *
 * Routine returns the configured IRQ number, or 0 if it can't figure
 * it out (0 is always invalid as it's reserved for NMIs).
 */
static int
FindIrq(int port)
{
	int j, irq = 0;
	unsigned short irrs, mask, map, nmap;
	unsigned char SaveMaster, SaveSlave;

	SaveMaster = inp(PIC1_PORT+1);
	SaveSlave = inp(PIC2_PORT+1);
	/*
	 * Make mask for possible mouse irq's
	 */
	mask = imask(5)|imask(4)|imask(3)|imask(2);
	set_pic_imrs(mask);
	outp(port, MSM_RESET);
	delay(3);
	/*
	 * point at the mouse mode register
	 */
	outp(port, MSM_MDREG);
	delay(1);
	/*
	 * Loop 10 times forcing two timer interrupts from
	 * the mouse on each iteration of the loop.  For the
	 * first interrupt, we use MSM_MODE0 to clear the
	 * corresponding interrupt bit in the pic irr.  For
	 * the second interrupt, we use MSM_MODE1 to set it!
	 */
	map = mask;
	for (j = 0; j < 10; j++) {
		outp(port+MSM_DATA, MSM_TIMER|MSM_MODE0);
		delay(1);
		irrs = read_pic_irrs() & mask;
		outp(port+MSM_DATA, MSM_TIMER|MSM_MODE1);
		delay(1);
		/*
		 * map of bits that toggled
		 */
		nmap = (read_pic_irrs() & mask) ^ irrs;
		/*
		 * now keep only bits that toggled before
		 */
		map &= nmap;
	}
	/*
	 * clear the interrupt
	 */
	outp(port+MSM_DATA, MSM_RESET|MSM_MODE0);

	/*
	 * Now look for bits set in the interrupt map.  There
	 * should be only one, it corresponds to the IRQ
	 * assigned to the mouse.
	 */
	for (j = 0; j < 16; (j++, map >>= 1)) {
		if (map & 1) {
			irq = j;
			break;
		}
	}
	outp(PIC1_PORT+1, SaveMaster);
	outp(PIC2_PORT+1, SaveSlave);
	return (irq);
}

/*
 * Probe for a Microsoft bus mouse, if found return non-zero,
 * else, return zero.  Also reserves the irq for the mouse if
 * the probe was successful.
 */
static int
probe_msm(unsigned short port)
{
	long val[2], len;
	unsigned char rev;
	long nbuttons;
	int plen;
	char far *fptr;

	/*
	 * On Microsoft InPort mice, alternate reads to
	 * base port + 2 should return a
	 * signature byte (MSM_SIG) followed by the revison
	 * number.  Try 5 times to make sure we've got a
	 * proper rodent.
	 */
	if ((inp(port+MSM_ID) == MSM_SIG) || (inp(port+MSM_ID) == MSM_SIG)) {
		int k = 5;

		rev = inp(port+MSM_ID);
		while (k-- && inp(port+MSM_ID) == MSM_SIG &&
			inp(port+MSM_ID) == rev)
			;
		if (k >= 0)
			return (0);
	} else
		return (0);
	/*
	 * Device appears to be a Microsoft mouse.
	 * Figure out its IRQ setting and see if the
	 * interrupt line is available.
	 */
	if (!(val[0] = FindIrq(port)))
		return (0); /* Fail, if we can't figure out the IRQ */
	val[1] = 0;
	len = IrqTupleSize;
	/*
	 * If the jumpered irq is not available, return failure
	 */
	if (set_res("irq", val, &len, 0) != RES_OK)
		return (0);
	/*
	 * Assume a 2 button mouse, should be right most of the time
	 */
	nbuttons = 2;
	plen = 4;
	fptr = (char far *)&nbuttons;
	set_prop("mouse-nbuttons", &fptr, &plen, PROP_BIN);
	return (1); /* We found a Microsoft bus mouse */
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
	 * Microsoft bus mice may be attached at a number of places
	 * (MIN_MSPORT thru MAX_MSPORT), so we'll have to try them all.
	 */
	for (port = MAX_MSPORT; port >= MIN_MSPORT; port -= 4) {
		if (node_op(NODE_START) != NODE_OK)
			continue; /* Couldn't create a node */
		/*
		 * Assume no mouse here if we can't reserve the ports
		 */
		ports[0] = port;
		ports[1] = 4;
		ports[2] = 0;
		portcnt = PortTupleSize;
		if (set_res("port", ports, &portcnt, 0) != RES_OK) {
			node_op(NODE_FREE);
			continue;
		}
		node_op(probe_msm(port) ? NODE_DONE : NODE_FREE);
	}
	return (BEF_OK);
}
