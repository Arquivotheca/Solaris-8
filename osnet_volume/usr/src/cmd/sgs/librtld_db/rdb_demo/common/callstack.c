
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)callstack.c	1.4	98/03/24 SMI"


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/procfs.h>

#include "rdb.h"

#if defined(sparc) || defined(__sparcv9)
#define	FRAME_PTR_INDEX R_FP
#endif

#if defined(i386)
#define	FRAME_PTR_INDEX R_FP
#endif

#ifndef	STACK_BIAS
#define	STACK_BIAS	0
#endif


static void
get_frame(struct ps_prochandle * ph, psaddr_t fp, struct frame * frm)
{
#if	defined(_LP64)
	/*
	 * Use special structures to read a 32-bit process
	 * from a 64-bit process.
	 */
	if (ph->pp_dmodel == PR_MODEL_ILP32) {
		struct frame32 frm32;

		if (ps_pread(ph, (psaddr_t)fp, (char *)&frm32,
		    sizeof (struct frame32)) != PS_OK) {
			printf("stack trace: bad frame pointer: 0x%lx\n",
				fp);
			return;
		}

		frm->fr_savpc = (long)frm32.fr_savpc;
		frm->fr_savfp = (struct frame *)frm32.fr_savfp;
		return;
	}
#endif	/* defined(_LP64) */

	if (ps_pread(ph, (psaddr_t)fp + STACK_BIAS, (char *)frm,
	    sizeof (struct frame)) != PS_OK)
		printf("stack trace: bad frame pointer: 0x%lx\n", fp);
}


/*
 * Relatively architecture neutral routine to display
 * the callstack.
 */
void
CallStack(struct ps_prochandle * ph)
{
	prstatus_t	prstatus;
	greg_t		fp;
	struct frame	frm;
	char *		symstr;

	if (ioctl(ph->pp_fd, PIOCSTATUS, &prstatus) == -1)
		perr("cs: PIOCSTATUS");

	symstr = print_address_ps(ph, (ulong_t)prstatus.pr_reg[R_PC],
		FLG_PAP_SONAME);
	printf(" 0x%08lx:%-17s\n", prstatus.pr_reg[R_PC],
		symstr);

	fp = prstatus.pr_reg[FRAME_PTR_INDEX];

	while (fp) {
		get_frame(ph, (psaddr_t)fp, &frm);
		if (frm.fr_savpc) {
			symstr = print_address_ps(ph, (ulong_t)frm.fr_savpc,
				FLG_PAP_SONAME);
			printf(" 0x%08lx:%-17s\n", frm.fr_savpc,
				symstr);
		}
		fp = (greg_t)frm.fr_savfp;
	}
}
