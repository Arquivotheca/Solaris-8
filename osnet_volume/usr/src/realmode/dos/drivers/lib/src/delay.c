/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */
 
#ident "@(#)delay.c	1.2	97/02/10 SMI"
 
/*
 *  Real mode time delay routines:
 */

void
drv_usecwait(unsigned long usecs)
{
	_asm {
		/*
		 *  Issue BIOS call to wait for specified number of microseconds
		 */

		mov   ax, 8600h
		mov   cx, word ptr [usecs+2]
		mov   dx, word ptr [usecs]
		int   15h
	}
}

void
drv_msecwait(unsigned long msecs)
{
	unsigned long usecs = msecs * 1000;

	drv_usecwait(usecs);
}
