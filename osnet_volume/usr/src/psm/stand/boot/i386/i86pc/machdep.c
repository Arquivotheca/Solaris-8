/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)machdep.c	1.4	99/10/29 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/salib.h>
#include <sys/dosemul.h>

extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern struct bootops	*bop;

void
setup_aux(void)
{
	extern char *mmulist;
	static char mmubuf[3 * OBP_MAXDRVNAME];
	int plen;

	if (((plen = bgetproplen(bop, "mmu-modlist", 0)) > 0) &&
	    (plen < (3 * OBP_MAXDRVNAME)))
		(void) bgetprop(bop, "mmu-modlist", mmubuf, 0, 0);
	else
		(void) strcpy(mmubuf, "mmu32"); /* default to mmu32 */
	mmulist = mmubuf;
}

void
reset(void)
{
	/* Disable BIOS memory check */
	pokes((unsigned short *)0x472, 0x1234);

	/* Tell the keyboard controller to reset the processor */
	outb(0x64, 0xfe);
	for (;;) {
		/*
		 * Loop forever.
		 *
		 * Pretty soon the keyboard controller will yank the
		 * processor reset line.
		 */
	}
	/* NOTREACHED */
}
