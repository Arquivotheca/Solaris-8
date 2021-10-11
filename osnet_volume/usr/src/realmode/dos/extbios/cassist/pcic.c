/*
 *  Copyright (c) 1996 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Realmode pseudo-driver for PCIC controllers:
 *
 *    This file contains code that "probe"s for standard ISA PCIC controllers
 *    and reserves appropriate bus resources for all PCICs it finds.
 *
 */

#ident "@(#)pcic.c   1.1   96/11/21 SMI"

#include <befext.h>
#include <string.h>
#include <stdio.h>
#include <biosmap.h>
#include "pcic.h"

static const struct { int Port; int Base; } pcics[] = {
	{ 0x3e0, 0x00 },
	{ 0x3e0, 0x80 },
	{ 0x3e2, 0x00 },
	{ 0x3e2, 0x80 }
};

#define	MAX_PCICS (sizeof(pcics)/sizeof(pcics[0]))

void
writePCIC(int port, int reg, unsigned char val)
{
	outp(port+PCIC_INDEX, reg);
	outp(port+PCIC_DATA, val);
}

unsigned char
readPCIC(int port, int reg)
{
	outp(port+PCIC_INDEX, reg);
	return (inp(port+PCIC_DATA));
}

int
pcicProbeSocket(int port, int base)
{
	unsigned char idbyte;
	unsigned short temp;

	idbyte = readPCIC(port, base + PCIC_IDREV);
	/* Does the following test need to be better? */
	if ((idbyte == 0x83) || (idbyte == 0x84) || (idbyte == 0x82)) {
		/* disable all windows */
		writePCIC(port, base + PCIC_ADDRWINENABLE, 0);
		/* disable interrupts and assert socket reset */
		/* also resets I/O IRQ routing. */
		writePCIC(port, base + PCIC_INTRGENCON, 0);
		/* disable all statchange interrupts and reset routing */
		writePCIC(port, base + PCIC_CARDSTATCONF, 0);
		/* reset the global control register */
		writePCIC(port, base + PCIC_GLOBALCON, 0);

		writePCIC(port, base + PCIC_IO_START0L, 0x55);
		writePCIC(port, base + PCIC_IO_START0H, 0xAA);
		/*
		 * If we read 0xAA55, we're probably not seeing a
		 * a bus holding pattern.
		 */
		temp = readPCIC(port, base + PCIC_IO_START0L) |
			(readPCIC(port, base + PCIC_IO_START0H) << 8);
		writePCIC(port, base + PCIC_IO_START0L, 0);
		writePCIC(port, base + PCIC_IO_START0H, 0);
		if (temp == 0xAA55)
			return(1);	/* found a real socket */
	}

	return(0);
}


void
probePCIC(int x)
{
	static struct {
		unsigned long addr;
		unsigned long size;
		unsigned long flags;
	} portval;
	unsigned long portlen;
	int	port, base, rv;
	unsigned char pval;

	if (x >= MAX_PCICS)
		return;

	port = pcics[x].Port;
	base = pcics[x].Base;

	portval.addr = port;
	portval.size = 2;
	portval.flags = 0;
	portlen = PortTupleSize;

	node_op(NODE_START);

	rv = set_res("port", (DWORD far *) &portval, (DWORD far *) &portlen,
		0);
	if (rv != RES_OK) {
		node_op(NODE_FREE);
		return;
	}

	/* probe socket 0 */
	pval = pcicProbeSocket(port, base);

	/* probe socket 1 */
	pval |= pcicProbeSocket(port, base + 0x40);

	if (!pval)
		node_op(NODE_FREE);
	else
		node_op(NODE_DONE);
	return;
}

int
dispatch(int func)
{
	int	index;

	/*
	 *  Driver Function Dispatcher:
	 *
	 *  This is the realmode driver equivalent of a "main" routine.  Use
	 *  the "func"tion code to determine the nature of the processing we're
	 *  being asked to perform.
	 */

	if (func != BEF_LEGACYPROBE)
		return (BEF_BADFUNC);

	for (index = 0; index < MAX_PCICS; index++) {
		probePCIC(index);
	}

	return (BEF_OK);
}
