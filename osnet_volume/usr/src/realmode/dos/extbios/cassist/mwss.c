/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Realmode Windows Sound System / Compatibles detection driver.
 *
 * Could some day be extended to cope with additional AD184x devices
 */

#ident "@(#)mwss.c   1.2   97/05/12 SMI"

#include <befext.h>
#include <biosmap.h>
#include <stdio.h>
#include <string.h>

#include "mwss.h"

/* #define DEBUG 1 */

typedef unsigned short u_short;

static int probe_mwss(u_short ioaddr);

extern void drv_usecwait(unsigned long usecs);

static int irqs[] = {9, 10, 11, 7}; /* ordered by preference */
static int irqs_val[] = {0x10, 0x18, 0x20, 0x8};
static int dmas[] = {1, 3, 0}; /* ordered by preference */
static int dmas_val[] = {0x2, 0x3, 0x1};

/*
 * Probe for an MWSS/compatible at the specified port. If sucessful return 1
 * else, return zero.  Also reserves the irq and dma(s) for the board if
 * the probe was successful.
 */
static int
probe_mwss(u_short ioaddr)
{
	int		i, delay, reg_val;
	unsigned long	resource[2], len;

	/* If INIT is set, wait for it to clear. */
	delay =  600;		/* 6 milliseconds max */
	while ((inp(ioaddr+AD184x_INDEX) & 0x80) && delay--)
		drv_usecwait(10);
	if (delay == 0)
		return (NODE_FREE);

	i = inp(ioaddr+MWSS_IRQSTAT);
	if ((i & 0x3f) != 4) {
#ifdef DEBUG
		printf("mwss: probe_mwss_ad184x @ %x: MWSS ID 0x%x != 4\n",
		    ioaddr, i);
#endif
		return (NODE_FREE);
	}

	/*
	 * Acquire resources for the board: first the irq
	 */
	for (i = 0; i < sizeof (irqs) / sizeof (irqs[0]); i++) {
		resource[0] = irqs[i];
		resource[1] = 0;
		len = IrqTupleSize;
		if (set_res("irq", resource, &len, RES_SILENT) == RES_OK) {
			/* Convert to  bit mask for the board */
			reg_val = irqs_val[i];

			/* Test IRQ */
			outp(ioaddr+MWSS_IRQSTAT, reg_val | 0x40);
			if (!(inp(ioaddr+MWSS_IRQSTAT) & 0x40)) {
#ifdef DEBUG
				printf("mwss: MWSS_AD184x IRQ %d is 'in use.'",
				    irqs[i]);
#endif
				resource[0] = irqs[i];
				resource[1] = 0;
				len = IrqTupleSize;
				rel_res("irq", resource, &len);

				outp(ioaddr+MWSS_IRQSTAT, 0);
			} else
				break;
		}
	}
	if (i == sizeof (irqs) / sizeof (irqs[0]))
		return (NODE_INCOMPLETE);

	/*
	 * Now the dma channel
	 */
	for (i = 0; i < sizeof (dmas) / sizeof (dmas[0]); i++) {
		resource[0] = dmas[i];
		resource[1] = 0;
		len = DmaTupleSize;
		if (set_res("dma", resource, &len, RES_SILENT) == RES_OK) {
			/* Convert to  bit mask for the board */
			reg_val |= dmas_val[i];
			break;
		}
	}
	if (i == sizeof (dmas) / sizeof (dmas[0]))
		return (NODE_INCOMPLETE);

	outp(ioaddr+MWSS_IRQSTAT, reg_val);	/* Set IRQ, DMA */

	return (NODE_DONE);
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
	u_short port;
	long ports[3], portcnt = PortTupleSize;
	static	int iobase[] = { 0x530, 0x604, 0xe80, 0xf40 };
	int idx;

	if (func != BEF_LEGACYPROBE)
		return (BEF_BADFUNC);	/* Not an operation we support! */

	for (idx = 0; idx < sizeof (iobase) / sizeof (iobase[0]); idx++) {
		if (node_op(NODE_START) != NODE_OK) {
			return (0);
		}

		port = iobase[idx];

		/*
		 * Assume no AD184x here if we can't reserve the ports
		 */
		ports[0] = port;
		ports[1] = MWSS_IO_LEN;
		ports[2] = 0;
		if (set_res("port", ports, &portcnt, 0) != RES_OK) {
			node_op(NODE_FREE);
			continue;
		}

		node_op(probe_mwss(port));
#ifdef DEBUG
		delay(8000);
#endif
	}
#ifdef DEBUG
	delay(8000);
#endif
	return (0);
}
