/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)rd_mach.c	1.8	98/08/28 SMI"

#include	<proc_service.h>
#include	<link.h>
#include	<rtld_db.h>
#include	"_rtld_db.h"
#include	"msg.h"


/*
 * A un-initialized PLT look like so:
 *
 * .PLT
 *	sethi	(.-.PLT0), %g1
 *	ba,a	.PLT0
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
	unsigned long	instr[4];
	rd_err_e	rerr;

	RDAGLOCK(rap);
	if (ps_pread(rap->rd_psp, pc, (char *)instr,
	    sizeof (unsigned long) * 4) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_2), EC_ADDR(pc)));
		RDAGUNLOCK(rap);
		return (RD_ERR);
	}

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

	RDAGUNLOCK(rap);
	return (RD_OK);
}
