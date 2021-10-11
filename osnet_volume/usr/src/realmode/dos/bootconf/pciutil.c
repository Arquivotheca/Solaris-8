/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * pciutil.c -- pci utility routines for reading configuration space
 */

#ident	"<@(#)pciutil.c	1.10	97/03/21 SMI>"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "types.h"

#include "debug.h"
#include "devdb.h"
#include "err.h"
#include "pci.h"
#include "pciutil.h"
#include "tty.h"

#include <dos.h>

u_char
pci_getb(u_char bus, u_char devfunc, u_int offset)
{
	union _REGS inregs, outregs;

	inregs.h.ah = PCI_FUNCTION_ID;
	inregs.h.al = READ_CONFIG_BYTE;
	inregs.h.bh = bus;
	inregs.h.bl = devfunc;
	inregs.x.di = offset;

	_int86(0x1a, &inregs, &outregs);

	if (outregs.h.ah == PCI_SUCCESS) {
		return (outregs.h.cl);
	}
	return (0xff);
}

u_int
pci_getw(u_char bus, u_char devfunc, u_int offset)
{
	union _REGS inregs, outregs;

	inregs.h.ah = PCI_FUNCTION_ID;
	inregs.h.al = READ_CONFIG_WORD;
	inregs.h.bh = bus;
	inregs.h.bl = devfunc;
	inregs.x.di = offset;

	_int86(0x1a, &inregs, &outregs);

	if (outregs.h.ah == PCI_SUCCESS) {
		return (outregs.x.cx);
	}
	return (0xffff);
}

u_long
pci_getl(u_char bus, u_char devfunc, u_int off)
{
	/*
	 * Unfortunately, _asm can't handle 386 code
	 * so we have to drop into assembler to read the ecx register
	 */
	u_long val;

	if (pci_getl2(bus, devfunc, off, (u_long *) &val) == PCI_SUCCESS) {
		return (val);
	}
	return (0xffffffff);
}

void
pci_putl(u_char bus, u_char devfunc, u_int off, u_long val)
{
	/*
	 * Unfortunately, _asm can't handle 386 code
	 * so we have to drop into assembler to write the ecx register
	 */
	if (pci_putl2(bus, devfunc, off, val) == PCI_SUCCESS) {
		return;
	}
	fatal("ERROR: bad pci long write to conf space\n");
}

int
pci_present(u_char *mechanism, u_char *nbus, u_short *vers)
{
	return (pci_present2(mechanism, nbus, vers));
}

#ifdef	__lint
int
pci_getl2(u_char bus, u_char devfunc, u_int off, u_long far *val)
{
	*val = (u_long)(bus + devfunc + off);
	return (off);
}

int
pci_putl2(u_char bus, u_char devfunc, u_int off, u_long val)
{
	int foo;

	foo = (int)(bus + devfunc) + off, (int)val;
	return (foo);
}

int
pci_present2(u_char far *mechanism, u_char far *nbus, u_short *vers)
{
	*mechanism = *nbus;
	return ((int)*vers);
}
#endif
