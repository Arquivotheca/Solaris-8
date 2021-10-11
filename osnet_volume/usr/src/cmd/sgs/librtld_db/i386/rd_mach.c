/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rd_mach.c	1.7	99/11/08 SMI"


#include	<libelf.h>
#include	<sys/reg.h>
#include	<rtld_db.h>
#include	"_rtld_db.h"
#include	"msg.h"


/*
 * On x86, basically, a PLT entry looks like this:
 *	8048738:  ff 25 c8 45 05 08   jmp    *0x80545c8	 < OFFSET_INTO_GOT>
 *	804873e:  68 20 00 00 00      pushl  $0x20
 *	8048743:  e9 70 ff ff ff      jmp    0xffffff70 <80486b8> < &.plt >
 *
 *  The first time around OFFSET_INTO_GOT contains address of pushl; this forces
 *	first time resolution to go thru PLT's first entry (which is a call)
 *  The nth time around, the OFFSET_INTO_GOT actually contains the resolved
 *	address of the symbol(name), so the jmp is direct  [VT]
 *  The only complication is when going from a .so to an a.out or to another
 *	.so, GOT's address is in %ebx
 */
/* ARGSUSED 3 */
rd_err_e
rd_plt_resolution(rd_agent_t * rap, psaddr_t pc, lwpid_t lwpid,
	psaddr_t pltbase, rd_plt_info_t * rpi)
{
	unsigned	addr;
	unsigned	ebx;
	prgregset_t	gr;

	RDAGLOCK(rap);
	/*
	 * This is the target of the jmp instruction
	 */
	if (ps_pread(rap->rd_psp, pc + 2, (char *)&addr,
	    sizeof (unsigned)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_2), EC_ADDR(pc + 2)));
		RDAGUNLOCK(rap);
		return (RD_ERR);
	}
	/*
	 * Is this branch %ebx relative
	 */
	if (ps_pread(rap->rd_psp, pc + 1, (char *)&ebx,
	    sizeof (unsigned)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_2), EC_ADDR(pc + 1)));
		RDAGUNLOCK(rap);
		return (RD_ERR);
	}

	if ((ebx & 0xff) == 0xa3) {
		/*
		 * GOT's base addr is in %ebx
		 */
		if (ps_lgetregs(rap->rd_psp, lwpid, gr) != PS_OK) {
			LOG(ps_plog(MSG_ORIG(MSG_DB_PSGETREGS)));
			RDAGUNLOCK(rap);
			return (RD_ERR);
		}
		addr = addr + gr[EBX];
	}

	/*
	 * Find out what's pointed to by @OFFSET_INTO_GOT
	 */
	if (ps_pread(rap->rd_psp, addr, (char *)&addr,
	    sizeof (unsigned)) != PS_OK) {
		LOG(ps_plog(MSG_ORIG(MSG_DB_READFAIL_2), EC_ADDR(addr)));
		RDAGUNLOCK(rap);
		return (RD_ERR);
	}
	if (addr == (pc + 6)) {
		rd_err_e	rerr;
		/*
		 * If GOT[ind] points to PLT+6 then this is the first
		 * time through this PLT.
		 */
		if ((rerr = rd_binder_exit_addr(rap,
		    &(rpi->pi_target))) != RD_OK) {
			RDAGUNLOCK(rap);
			return (rerr);
		}
		rpi->pi_skip_method = RD_RESOLVE_TARGET_STEP;
		rpi->pi_nstep = 1;
	} else {
		/*
		 * This is the n'th time through and GOT[ind] points
		 * to the final destination.
		 */
		rpi->pi_skip_method = RD_RESOLVE_STEP;
		rpi->pi_nstep = 1;
		rpi->pi_target = 0;
	}

	RDAGUNLOCK(rap);
	return (RD_OK);
}
