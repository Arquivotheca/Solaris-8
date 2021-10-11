/*
 * Copyright (c) 1992-1996, Sun Microsystems, Inc.  All Rights Reserved.
 *
 *	i86pc routines to identify various supported bus types.
 *      It is modeled on the real-mode routines of the same name.
 */

#pragma ident	"@(#)bus_probe.c	1.4	96/03/23 SMI"

#include <sys/types.h>
#include <sys/bootlink.h>
#include <sys/bootinfo.h>

#define	EISA_IDLOC ((long *)0xFFFD9)
#define	EISA_IDSTR (*(long *)"EISA")
#define	SYS_CONF   ((paddr_t)0xFE6F5)

#define	FEATURE_OFFSET	5
#define	MC_BUSFLG	0x02

extern int doint(void);
extern struct int_pb ic;
extern struct bootenv *btep;


/*
 * returns physical address of the PC's system configuration table.
 */
paddr_t
get_sysconf()
{
	ic.intval = 0x15;
	ic.ax = 0xC000;
	if (doint()) {	/* if nonzero, int call failed... */
#ifdef BOOT_DEBUG
		if (btep->db_flag & BOOTTALK)
			printf("int failed, using addr 0xFE6F5.\n");
#endif
		return (SYS_CONF);
	}

#ifdef BOOT_DEBUG
	if (btep->db_flag & BOOTTALK)
		printf("system configuration table located at: 0x%lx\n",
		    (ic.es << 4) | ic.bx);
#endif
	return ((paddr_t)((paddr_t)ic.es << 4) + (ushort)ic.bx);
}

/*
 * returns 1 if EISA bus detected, 0 otherwise.
 */
is_EISA()
{
	if (*EISA_IDLOC == EISA_IDSTR)
		return (1);
	else
		return (0);
}


/*
 * returns 1 if MC bus detected, 0 otherwise.
 */
is_MC()
{
	/*
	 * check the value of the bus-type flag within the
	 * Feature Information Byte.  This flag is always on for
	 * Micro Channel machines, off for ISA/EISA machines.
	 * A more reliable indicator than tha Model byte that various
	 * unnamed OEM's have perverted!
	 */
	return ((*((char *)get_sysconf() + FEATURE_OFFSET)) & MC_BUSFLG);

}
