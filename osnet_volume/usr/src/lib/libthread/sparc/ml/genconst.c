/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)genconst.c	1.5	99/09/02 SMI"

/*
 * genconst generates datamodel-independent constants.  Such constants
 * are enum values and constants defined via preprocessor macros.
 * Under no circumstances should this program generate structure size
 * or structure member offset information, those belong in offsets.in.
 */

#ifndef	_GENASSYM
#define	_GENASSYM
#endif

#include "libthread.h"
#include <signal.h>
#include <sys/psw.h>

/*
 * Proactively discourage anyone from referring to structures or
 * member offsets in this program.
 */
#define	struct	struct...
#define	OFFSET	OFFSET...

int
main(int argc, char *argv[])
{
	/* cancellation support begin */
	printf("#define\tTC_PENDING\t%d\n", TC_PENDING);
	printf("#define\tTC_DISABLE\t%d\n", TC_DISABLE);
	printf("#define\tTC_ENABLE\t%d\n", TC_ENABLE);
	printf("#define\tTC_ASYNCHRONOUS\t%d\n", TC_ASYNCHRONOUS);
	printf("#define\tTC_DEFERRED\t%d\n", TC_DEFERRED);
	printf("#define\tPTHREAD_CANCELED\t%d\n", PTHREAD_CANCELED);
	/* cancellation support end */

	printf("#define\tTS_ZOMB\t0x%x\n", TS_ZOMB);
	printf("#define\tT_IDLETHREAD\t0x%x\n", T_IDLETHREAD);
	printf("#define\tTS_ONPROC\t0x%x\n", TS_ONPROC);
	printf("#define\tSIG_SETMASK\t0x%x\n", SIG_SETMASK);
	printf("#define\tPSR_EF\t0x%x\n", PSR_EF);
	printf("#define\tPAGESIZE\t0x%lx\n", PAGESIZE);

#ifdef TRACE_INTERNAL
	printf("#define\tTR_T_SWTCH\t0x%x\n", TR_T_SWTCH);
#endif
	printf("#define\tLOCK_MASK\t0xff000000\n");
	printf("#define\tWAITER_MASK\t0x00000001\n");
	exit(0);
}

int
bit(long mask)
{
	int i;

	for (i = 0; i < sizeof (mask) * NBBY; i++) {
		if (mask & 1)
			return (i);
		mask >>= 1;
	}

	exit(1);
}
