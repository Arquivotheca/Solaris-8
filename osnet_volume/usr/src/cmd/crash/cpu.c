/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpu.c	1.10	98/03/30 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <sys/cpuvar.h>
#include "crash.h"

static void prcpu(void *);

void
getcpu()
{
	intptr_t addr;
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind]) {
		do {
			if ((addr = strcon(args[optind++], 'h')) == -1)
				continue;
			prcpu((void *)addr);
		} while (args[optind]);
	} else longjmp(syn, 0);
}

void
prcpu(void *addr)
{
	struct cpu cpub;

	readbuf(addr, 0, 0, &cpub, sizeof (cpub), "cpu entry");
	fprintf(fp, "CPU:\n");
	fprintf(fp, "\tid %-8d\tflags %-8x\tthread %-8p\tidle_thread %-8p\n",
		cpub.cpu_id,
		cpub.cpu_flags,
		cpub.cpu_thread,
		cpub.cpu_idle_thread);
	fprintf(fp, "\tlwp %-8p\tfpu %-8p\n",
		cpub.cpu_lwp,
		cpub.cpu_fpowner);
	fprintf(fp,
		"\trunrun %-8d\tkprunrun %-8d\tdispthread %-8p\ton_intr %-8d\n",
		cpub.cpu_runrun,
		cpub.cpu_kprunrun,
		cpub.cpu_dispthread,
		cpub.cpu_on_intr);
	fprintf(fp, "\tintr_stack %-8p\tintr_thread %-8p\tintr_actv %-8u\n",
		cpub.cpu_intr_stack,
		cpub.cpu_intr_thread,
		cpub.cpu_intr_actv);
	fprintf(fp, "\tbase_spl %8d\n",
		cpub.cpu_base_spl);
}
