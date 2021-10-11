/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)genassym.c	1.2	95/06/14 SMI"

#ifndef	_GENASSYM
#define	_GENASSYM
#endif

#define	SIZES	1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/privregs.h>
#include "allregs.h"

main()
{
	register struct v9_fpu *fp = (struct v9_fpu *)0;
	register struct allregs_v9 *rp = (struct allregs_v9 *)0;

	printf("#define\tR_TSTATE %d\n", &rp->r_tstate);
	printf("#define\tR_PC %d\n", &rp->r_pc);
	printf("#define\tR_NPC %d\n", &rp->r_npc);
	printf("#define\tR_TBA %d\n", &rp->r_tba);
	printf("#define\tR_Y %d\n", &rp->r_y);
	printf("#define\tR_TT %d\n", &rp->r_tt);
	printf("#define\tR_PIL %d\n", &rp->r_pil);
	printf("#define\tR_WSTATE %d\n", &rp->r_wstate);
	printf("#define\tR_CWP %d\n", &rp->r_cwp);
	printf("#define\tR_OTHERWIN %d\n", &rp->r_otherwin);
	printf("#define\tR_CLEANWIN %d\n", &rp->r_cleanwin);
	printf("#define\tR_CANSAVE %d\n", &rp->r_cansave);
	printf("#define\tR_CANRESTORE %d\n", &rp->r_canrestore);
	printf("#define\tR_G1 %d\n", &rp->r_globals[0]);
	printf("#define\tR_G2 %d\n", &rp->r_globals[1]);
	printf("#define\tR_G3 %d\n", &rp->r_globals[2]);
	printf("#define\tR_G4 %d\n", &rp->r_globals[3]);
	printf("#define\tR_G5 %d\n", &rp->r_globals[4]);
	printf("#define\tR_G6 %d\n", &rp->r_globals[5]);
	printf("#define\tR_G7 %d\n", &rp->r_globals[6]);
	printf("#define\tR_WINDOW %d\n", &rp->r_window[0]);

	printf("#define\tV9FPUSIZE 0x%x\n", sizeof (struct v9_fpu));
	printf("#define\tFPU_REGS 0x%x\n", &fp->fpu_fr.fpu_regs[0]);
	printf("#define\tFPU_FSR 0x%x\n", &fp->fpu_fsr);
	printf("#define\tFPU_FPRS 0x%x\n", &fp->fpu_fprs);

	printf("#define\tR_OUTS %d\n", &rp->r_outs[0]);

	/*
	 * Gross hack... Although genassym is a user program and hence
	 * exit has one parameter, it is compiled with the kernel headers
	 * and the _KERNEL define so ANSI-C thinks it should have two!
	 */
	exit(0, 0);
}
