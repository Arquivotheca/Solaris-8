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
#pragma ident	"@(#)regs.c	1.3	96/09/10 SMI"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <sys/reg.h>

#include "rdb.h"

static void
disp_reg_line(struct ps_prochandle * ph, prstatus_t * prst,
	char * r1, int ind1, char * r2, int ind2)
{
	char	str1[MAXPATHLEN];
	char	str2[MAXPATHLEN];
	strcpy(str1, print_address_ps(ph, prst->pr_reg[ind1],
		FLG_PAP_NOHEXNAME));
	strcpy(str2, print_address_ps(ph, prst->pr_reg[ind2],
		FLG_PAP_NOHEXNAME));

	printf("%8s: 0x%08x %-16s %8s: 0x%08x %-16s\n",
		r1, prst->pr_reg[ind1], str1,
		r2, prst->pr_reg[ind2], str2);
}


retc_t
display_all_regs(struct ps_prochandle *ph)
{
	prstatus_t	prstatus;
	if (ioctl(ph->pp_fd, PIOCSTATUS, &prstatus) == -1) {
		perror("dar: PIOCSTATUS");
		return (RET_FAILED);
	}
	printf("registers:\n");
	disp_reg_line(ph, &prstatus, "gs", GS, "fs", FS);
	disp_reg_line(ph, &prstatus, "es", ES, "ds", DS);
	disp_reg_line(ph, &prstatus, "edi", EDI, "esi", ESI);
	disp_reg_line(ph, &prstatus, "ebp", EBP, "esp", ESP);
	disp_reg_line(ph, &prstatus, "ebx", EBX, "edx", EDX);
	disp_reg_line(ph, &prstatus, "ecx", ECX, "eax", EAX);
	disp_reg_line(ph, &prstatus, "trapno", TRAPNO, "err", ERR);
	disp_reg_line(ph, &prstatus, "eip", EIP, "cs", CS);
	disp_reg_line(ph, &prstatus, "efl", EFL, "uesp", UESP);
	return (RET_OK);
}
