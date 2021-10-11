/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rd_mach.c	1.6	99/02/17 SMI"

#include	<proc_service.h>
#include	<link.h>
#include	<rtld_db.h>
#include	"_rtld_db.h"
#include	"msg.h"


/*
 * A un-initialized SPARCV9 PLT look like so:
 *
 * .PLT
 *	sethi	(. - .PLT0), %g1
 *	ba,a	%xcc, .PLT1
 *	nop
 *	nop
 *	nop
 *	nop
 *	nop
 *	nop
 *
 * To test to see if this is an uninitialized PLT we check
 * the second instruction and confirm that it's a branch.
 */
/* ARGSUSED 2 */
rd_err_e
rd_plt_resolution(rd_agent_t * rap, psaddr_t pc, lwpid_t lwpid,
	psaddr_t pltbase, rd_plt_info_t * rpi)
{
	instr_t		instr[8];
	rd_err_e	rerr;

	RDAGLOCK(rap);
	if (ps_pread(rap->rd_psp, pc, (char *)instr,
	    sizeof (instr_t) * 8) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_2), EC_ADDR(pc)));
		RDAGUNLOCK(rap);
		return (RD_ERR);

	}

	if (rap->rd_dmodel == PR_MODEL_LP64) {
		if ((instr[1] & (~(S_MASK(19)))) == M_BA_A_XCC) {
			if ((rerr = rd_binder_exit_addr(rap,
			    &(rpi->pi_target))) != RD_OK) {
				RDAGUNLOCK(rap);
				return (rerr);
			}
			rpi->pi_skip_method = RD_RESOLVE_TARGET_STEP;
			rpi->pi_nstep = 1;
		} else if ((instr[6] & (~(S_MASK(13)))) == M_JMPL_G5G0) {
			rpi->pi_skip_method = RD_RESOLVE_STEP;
			rpi->pi_nstep = 8;
			rpi->pi_target = 0;
		} else if (instr[3] == M_JMPL) {
			rpi->pi_skip_method = RD_RESOLVE_STEP;
			rpi->pi_nstep = 5;
			rpi->pi_target = 0;
		} else if ((instr[2] & (~(S_MASK(13)))) == M_XNOR_G5G1) {
			rpi->pi_skip_method = RD_RESOLVE_STEP;
			rpi->pi_nstep = 6;
			rpi->pi_target = 0;
		} else
			rpi->pi_skip_method = RD_RESOLVE_NONE;
	} else /* (rap->rd_dmodel == PR_MODEL_ILP32) */ {
		if ((instr[1] & (~(S_MASK(22)))) == M_BA_A) {
			if ((rerr = rd_binder_exit_addr(rap,
			    &(rpi->pi_target))) != RD_OK) {
				RDAGUNLOCK(rap);
				return (rerr);
			}
			rpi->pi_skip_method = RD_RESOLVE_TARGET_STEP;
			rpi->pi_nstep = 1;
		} else if ((instr[2] & (~(S_MASK(13)))) == M_JMPL) {
			rpi->pi_skip_method = RD_RESOLVE_STEP;
			rpi->pi_nstep = 4;
			rpi->pi_target = 0;
		} else
			rpi->pi_skip_method = RD_RESOLVE_NONE;
	}

	RDAGUNLOCK(rap);
	return (RD_OK);
}
