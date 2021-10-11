
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
#pragma ident	"@(#)regs.c	1.2	96/09/10 SMI"


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>

#include "rdb.h"


static void
disp_reg_line(struct ps_prochandle * ph, prstatus_t * prst,
	char * r1, int ind1, char * r2, int ind2)
{
	char 		str1[MAXPATHLEN];
	char 		str2[MAXPATHLEN];

	strcpy(str1, print_address_ps(ph, prst->pr_reg[ind1],
		FLG_PAP_NOHEXNAME));

	strcpy(str2, print_address_ps(ph, prst->pr_reg[ind2],
		FLG_PAP_NOHEXNAME));

	printf("%8s: 0x%08x %-16s %8s: 0x%08x %-16s\n",
		r1, prst->pr_reg[ind1], str1,
		r2, prst->pr_reg[ind2], str2);
}


void
display_local_regs(struct ps_prochandle * ph, prstatus_t * prst)
{
	prstatus_t	prstatus;

	if (prst == 0) {
		if (ioctl(ph->pp_fd, PIOCSTATUS, &prstatus) == -1) {
			perror("dar: PIOCSTATUS");
			return;
		}
		prst = &prstatus;
	}
	printf("locals:\n");
	disp_reg_line(ph, prst, "l0", R_L0, "l4", R_L4);
	disp_reg_line(ph, prst, "l1", R_L1, "l5", R_L5);
	disp_reg_line(ph, prst, "l2", R_L2, "l6", R_L6);
	disp_reg_line(ph, prst, "l3", R_L3, "l7", R_L7);
}

void
display_out_regs(struct ps_prochandle * ph, prstatus_t * prst)
{
	prstatus_t	prstatus;

	if (prst == 0) {
		if (ioctl(ph->pp_fd, PIOCSTATUS, &prstatus) == -1) {
			perror("dar: PIOCSTATUS");
			return;
		}
		prst = &prstatus;
	}
	printf("outs:\n");
	disp_reg_line(ph, prst, "o0", R_O0, "o4", R_O4);
	disp_reg_line(ph, prst, "o1", R_O1, "o5", R_O5);
	disp_reg_line(ph, prst, "o2", R_O2, "o6(sp)", R_O6);
	disp_reg_line(ph, prst, "o3", R_O3, "o7", R_O7);
}

void
display_special_regs(struct ps_prochandle * ph, prstatus_t * prst)
{
	prstatus_t	prstatus;

	if (prst == 0) {
		if (ioctl(ph->pp_fd, PIOCSTATUS, &prstatus) == -1) {
			perror("dar: PIOCSTATUS");
			return;
		}
		prst = &prstatus;
	}
	printf("specials:\n");
	disp_reg_line(ph, prst, "psr", R_PSR, "pc", R_PC);
	disp_reg_line(ph, prst, "npc", R_nPC, "Y", R_Y);
	disp_reg_line(ph, prst, "wim", R_WIM, "TBR", R_TBR);
}

void
display_global_regs(struct ps_prochandle * ph, prstatus_t * prst)
{
	prstatus_t	prstatus;

	if (prst == 0) {
		if (ioctl(ph->pp_fd, PIOCSTATUS, &prstatus) == -1) {
			perror("dar: PIOCSTATUS");
			return;
		}
		prst = &prstatus;
	}
	printf("globals:\n");
	disp_reg_line(ph, prst, "g0", R_G0, "g4", R_G4);
	disp_reg_line(ph, prst, "g1", R_G1, "g5", R_G5);
	disp_reg_line(ph, prst, "g2", R_G2, "g6", R_G6);
	disp_reg_line(ph, prst, "g3", R_G3, "g7", R_G7);
}

void
display_in_regs(struct ps_prochandle * ph, prstatus_t * prst)
{
	prstatus_t	prstatus;

	if (prst == 0) {
		if (ioctl(ph->pp_fd, PIOCSTATUS, &prstatus) == -1) {
			perror("dar: PIOCSTATUS");
			return;
		}
		prst = &prstatus;
	}
	printf("ins:\n");
	disp_reg_line(ph, prst, "i0", R_I0, "i4", R_I4);
	disp_reg_line(ph, prst, "i1", R_I1, "i5", R_I5);
	disp_reg_line(ph, prst, "i2", R_I2, "i6(fp)", R_I6);
	disp_reg_line(ph, prst, "i3", R_I3, "i7", R_I7);

}


retc_t
display_all_regs(struct ps_prochandle * ph)
{
	prstatus_t	prstatus;

	if (ioctl(ph->pp_fd, PIOCSTATUS, &prstatus) == -1) {
		perror("dar: PIOCSTATUS");
		return (RET_FAILED);
	}
	display_global_regs(ph, &prstatus);
	display_in_regs(ph, &prstatus);
	display_local_regs(ph, &prstatus);
	display_out_regs(ph, &prstatus);
	display_special_regs(ph, &prstatus);

	return (RET_OK);
}
