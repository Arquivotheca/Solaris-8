/*
 * Copyright (c) 1992-1994,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_enter.c	1.10	99/05/04 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootsvcs.h>

/*
 * The Intel cpu does not have an underlying monitor.
 * So, we emulate the best we can.....
 */

void
prom_enter_mon(void)
{
#ifdef KADB
	printf("Reset the system to reboot.\n");
	while (1)
		prom_getchar();
#endif

#if !defined(KADB) && !defined(I386BOOT)
	while (goany())
		int20();
#endif
}
