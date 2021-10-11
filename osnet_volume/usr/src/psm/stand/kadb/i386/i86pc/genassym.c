/*
 * Copyright (c) 1994, 1997 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)genassym.c	1.5	97/11/24 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/reg.h>
#include <sys/debugreg.h>
#include <sys/segment.h>
#include <sys/cpuvar.h>
#include <sys/xc_levels.h>
#include <sys/dditypes.h>
#include <sys/uadmin.h>
#include <stdio.h>

main()
{
	register    struct  cpu *cp = (struct cpu *) 0;

	printf ("#define\tSS\t%d\n", SS);		/* 18 */
	printf ("#define\tUESP\t%d\n", UESP);		/* 17 */
	printf ("#define\tEFL\t%d\n", EFL);		/* 16 */
	printf ("#define\tCS\t%d\n", CS);		/* 15 */
	printf ("#define\tEIP\t%d\n", EIP);		/* 14 */
	printf ("#define\tERR\t%d\n", ERR);		/* 13 */
	printf ("#define\tTRAPNO\t%d\n", TRAPNO);	/* 12 */
	printf ("#define\tEAX\t%d\n", EAX);		/* 11 */
	printf ("#define\tECX\t%d\n", ECX);		/* 10 */
	printf ("#define\tEDX\t%d\n", EDX);		/*  9 */
	printf ("#define\tEBX\t%d\n", EBX);		/*  8 */
	printf ("#define\tESP\t%d\n", ESP);		/*  7 */
	printf ("#define\tEBP\t%d\n", EBP);		/*  6 */
	printf ("#define\tESI\t%d\n", ESI);		/*  5 */
	printf ("#define\tEDI\t%d\n", EDI);		/*  4 */
	printf ("#define\tDS\t%d\n", DS);		/*  3 */
	printf ("#define\tES\t%d\n", ES);		/*  2 */
	printf ("#define\tFS\t%d\n", FS);		/*  1 */
	printf ("#define\tGS\t%d\n", GS);		/*  0 */

	printf ("#define\tKCSSEL\t0x%x\n", KCSSEL);	/* 0x158: kernel %cs */
	printf ("#define\tKDSSEL\t0x%x\n", KDSSEL);	/* 0x160: kernel %ds */
	printf ("#define\tKFSSEL\t0x%x\n", KFSSEL);	/* 0x1a8: kernel %fs */
	printf ("#define\tKGSSEL\t0x%x\n", KGSSEL);	/* 0x1b0: kernel %gs */

	printf ("#define\tCPU_ID\t0x%x\n", (int) &(cp->cpu_id)); /* 0x0: cpu */

	printf ("#define\tNCPU\t%d\n", NCPU);		/* 8: number of cpus */
	printf ("#define\tX_CALL_HIPRI\t%d\n", X_CALL_HIPRI);	/* 2 */
	printf ("#define\tA_SHUTDOWN\t%d\n", A_SHUTDOWN);	/* 2 */
	printf ("#define\tAD_BOOT\t%d\n", AD_BOOT);		/* 1 */

	return (0);
}
