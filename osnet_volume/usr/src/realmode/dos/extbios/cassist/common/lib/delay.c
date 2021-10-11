/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Realmode "sleep" routine:
 */

#ident "<@(#)delay.c	1.4	96/05/06	SMI>"

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
delay(unsigned msecs)
{
	unsigned long usecs = ((long)msecs * 1000L);

	drv_usecwait(usecs);
}
