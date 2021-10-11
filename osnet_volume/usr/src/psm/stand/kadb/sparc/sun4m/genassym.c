/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)genassym.c	1.8	94/10/18 SMI" /* From SunOS 4.1.1 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/pte.h>
#include <sys/privregs.h>
#include <sys/mmu.h>
#include "allregs.h"

main()
{
	register struct allregs *rp = (struct allregs *)0;

	printf("#define\tR_PSR %d\n", &rp->r_psr);
	printf("#define\tR_PC %d\n", &rp->r_pc);
	printf("#define\tR_NPC %d\n", &rp->r_npc);
	printf("#define\tR_TBR %d\n", &rp->r_tbr);
	printf("#define\tR_WIM %d\n", &rp->r_wim);
	printf("#define\tR_Y %d\n", &rp->r_y);
	printf("#define\tR_G1 %d\n", &rp->r_globals[0]);
	printf("#define\tR_G2 %d\n", &rp->r_globals[1]);
	printf("#define\tR_G3 %d\n", &rp->r_globals[2]);
	printf("#define\tR_G4 %d\n", &rp->r_globals[3]);
	printf("#define\tR_G5 %d\n", &rp->r_globals[4]);
	printf("#define\tR_G6 %d\n", &rp->r_globals[5]);
	printf("#define\tR_G7 %d\n", &rp->r_globals[6]);
	printf("#define\tR_WINDOW %d\n", rp->r_window);
	printf("#define\tPSR_PIL_BIT %d\n", bit(PSR_PIL));
	return (0);
}

bit(mask)
	register long mask;
{
	register int i;

	for (i = 0; i < 32; i++) {
		if (mask & 1)
			return (i);
		mask >>= 1;
	}
	/*
	 * Due to kernel function prototypes, exit() is declared
	 * to take two arguments and we get a compiler error if
	 * we don't provide two.  The second argument doesn't hurt.
	 */
	exit (1, 0);
}
