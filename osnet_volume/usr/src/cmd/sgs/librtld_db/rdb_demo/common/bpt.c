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
#pragma ident	"@(#)bpt.c	1.7	98/03/18 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/procfs.h>
#include <sys/auxv.h>
#include <libelf.h>
#include <sys/param.h>
#include <stdarg.h>

#include "rdb.h"


static const char * fault_strings[] = {
	"<null string>",
	"illegal instruction",
	"privileged instruction",
	"breakpoint instruction",
	"trace trap (single-step)",
	"Memory access (e.g., alignment)",
	"Memory bounds (invalid address)",
	"Integer overflow",
	"Integer zero divide"
	"Floating-point exception",
	"Irrecoverable stack faul",
	"Recoverable page fault (no associated sig)"
};

#define	MAXFAULT	FLTPAGE

retc_t
set_breakpoint(struct ps_prochandle * ph, ulong_t addr, unsigned flags)
{
	bptlist_t *	new;
	bptlist_t *	cur;
	bptlist_t *	prev;

	for (cur = ph->pp_breakpoints, prev = 0;
	    (cur && (cur->bl_addr < addr));
	    prev = cur, cur = cur->bl_next)
		;
	if (cur && (cur->bl_addr == addr)) {
		/*
		 * already have break point set here.
		 */
		cur->bl_flags |= flags;
		return (RET_OK);
	}

	new = malloc(sizeof (bptlist_t));
	new->bl_addr = addr;
	new->bl_flags = flags;
	if (prev == 0) {
		/*
		 * insert at head
		 */
		new->bl_next = ph->pp_breakpoints;
		ph->pp_breakpoints = new;
		return (RET_OK);
	}

	prev->bl_next = new;
	new->bl_next = cur;
	return (RET_OK);
}

bptlist_t *
find_bp(struct ps_prochandle * ph, ulong_t addr)
{
	bptlist_t *	cur;

	for (cur = ph->pp_breakpoints;
	    (cur && (cur->bl_addr != addr));
	    cur = cur->bl_next)
		;

	if ((cur == 0) || (cur->bl_addr != addr))
		return ((bptlist_t *)-1);
	return (cur);
}


retc_t
delete_bp(struct ps_prochandle * ph, ulong_t addr)
{
	bptlist_t *	cur;
	bptlist_t *	prev;

	for (cur = ph->pp_breakpoints, prev = 0;
	    (cur && (cur->bl_addr < addr));
	    prev = cur, cur = cur->bl_next)
		;
	if ((cur == 0) || (cur->bl_addr != addr))
		return (RET_FAILED);

	if (prev == 0)
		ph->pp_breakpoints = cur->bl_next;
	else
		prev->bl_next = cur->bl_next;

	free(cur);
	return (RET_OK);
}


void
list_breakpoints(struct ps_prochandle * ph)
{
	bptlist_t *	cur;

	if (ph->pp_breakpoints == 0) {
		printf("no active breakpoints.\n");
		return;
	}

	printf("active breakpoints:\n");
	for (cur = ph->pp_breakpoints; cur; cur = cur->bl_next) {
		printf("\t0x%08lx:0x%04x - %s\n", cur->bl_addr,
			cur->bl_flags,
			print_address_ps(ph, cur->bl_addr,
			FLG_PAP_SONAME));
	}
}


static void
set_breaks(struct ps_prochandle * ph)
{
	bptlist_t *	cur;
	bptinstr_t	bpt_instr = BPINSTR;

	for (cur = ph->pp_breakpoints; cur; cur = cur->bl_next) {
		bptinstr_t	old_inst;
		old_inst = 0;

		if (ps_pread(ph, cur->bl_addr, (char *)&old_inst,
		    sizeof (bptinstr_t)) != PS_OK)
			perr("sb: error setting breakpoint\n");

		cur->bl_instr = old_inst;

		if (ps_pwrite(ph, cur->bl_addr, (char *)&bpt_instr,
		    sizeof (bptinstr_t)) != PS_OK)
			perr("sb1: error setting breakpoint\n");
	}

}


static void
clear_breaks(struct ps_prochandle * ph)
{
	bptlist_t *	cur;

	/*
	 * Restore all the original instructions
	 */
	for (cur = ph->pp_breakpoints; cur; cur = cur->bl_next)
		if (ps_pwrite(ph, cur->bl_addr, (char *)&(cur->bl_instr),
		    sizeof (bptinstr_t)) != PS_OK)
			perr("cb: error clearing breakpoint");
}

retc_t
delete_all_breakpoints(struct ps_prochandle * ph)
{
	bptlist_t *	cur;
	bptlist_t *	prev;


	if (ph->pp_breakpoints == 0)
		return (RET_OK);

	for (prev = 0, cur = ph->pp_breakpoints;
	    cur; prev = cur, cur = cur->bl_next)
		if (prev)
			free(prev);
	if (prev)
		free(prev);

	ph->pp_breakpoints = 0;
	return (RET_OK);
}

retc_t
delete_breakpoint(struct ps_prochandle * ph, ulong_t addr, unsigned flags)
{
	bptlist_t *	bpt;

	if (((bpt = find_bp(ph, addr)) == (bptlist_t *)-1) ||
	    ((bpt->bl_flags & flags) == 0))
		return (RET_FAILED);

	bpt->bl_flags &= ~flags;
	if (bpt->bl_flags)
		return (RET_OK);

	return (delete_bp(ph, addr));
}


static void
handle_sp_break(struct ps_prochandle * ph)
{
	rd_event_msg_t	emt;

	if (rd_event_getmsg(ph->pp_rap, &emt) != RD_OK) {
		fprintf(stderr, "hsb: failed rd_event_getmsg()\n");
		return;
	}


	if (emt.type == RD_DLACTIVITY) {
		if (emt.u.state == RD_CONSISTENT)
			ph->pp_flags |= FLG_PP_LMAPS;
		else
			ph->pp_flags &= ~FLG_PP_LMAPS;
		if ((rdb_flags & RDB_FL_EVENTS) == 0)
			return;

		printf("dlactivity: state changed to: ");
		switch (emt.u.state) {
		case RD_CONSISTENT:
			printf("RD_CONSISTENT\n");
			break;
		case RD_ADD:
			printf("RD_ADD\n");
			break;
		case RD_DELETE:
			printf("RD_DELETE\n");
			break;
		default:
			printf("unknown: 0x%x\n", emt.u.state);
		}
		return;
	}

	if ((rdb_flags & RDB_FL_EVENTS) == 0)
		return;

	if (emt.type == RD_PREINIT) {
		printf("preinit reached\n");
		return;
	}

	if (emt.type == RD_POSTINIT)
		printf("postinit reached\n");
}

unsigned
continue_to_break(struct ps_prochandle * ph)
{
	prrun_t		prrun;
	bptlist_t *	bpt;
	prstatus_t	prstatus;
	int		pfd = ph->pp_fd;

	/*
	 * We step by the first instruction encase their was
	 * a break-point there.
	 */
	step_n(ph, 1, FLG_SN_NONE);
	prrun.pr_flags = PRSFAULT;
	premptyset(&prrun.pr_fault);
	praddset(&prrun.pr_fault, FLTBPT);
	praddset(&prrun.pr_fault, FLTILL);
	praddset(&prrun.pr_fault, FLTPRIV);
	praddset(&prrun.pr_fault, FLTACCESS);
	praddset(&prrun.pr_fault, FLTBOUNDS);
	praddset(&prrun.pr_fault, FLTIZDIV);
	praddset(&prrun.pr_fault, FLTSTACK);

	/* LINTED CONSTANT */
	while (1) {
		set_breaks(ph);
		if ((ioctl(pfd, PIOCRUN, &prrun)) != 0)
			perr("ctb: PIOCRUN");

		if ((ioctl(pfd, PIOCWSTOP, &prstatus)) != 0) {
			if (errno == ENOENT) {
				ph->pp_flags &= ~FLG_PP_PACT;
				ps_close(ph);
				printf("process terminated.\n");
				return (0);
			}
			perr("ctb: PIOCWSTOP");
		}

		if ((prstatus.pr_why != PR_FAULTED) ||
		    (prstatus.pr_what != FLTBPT)) {
			const char * fltmsg;
			if ((prstatus.pr_what <= MAXFAULT) &&
			    (prstatus.pr_why == PR_FAULTED))
				fltmsg = fault_strings[prstatus.pr_what];
			else
				fltmsg = "<unknown error>";

			fprintf(stderr, "ctb: bad stop - stoped on why: 0x%x "
				"what: %s(0x%x)\n",
				prstatus.pr_why, fltmsg, prstatus.pr_what);
			return (0);
		}
		if (ioctl(pfd, PIOCCFAULT, 0) != 0) {
			perr("PIOCCFAULT");
		}
		if ((bpt = find_bp(ph, prstatus.pr_reg[R_PC])) ==
		    (bptlist_t *)-1) {
			fprintf(stderr,
			    "stoped at unregistered breakpoint! "
			    "addr: 0x%lx\n",
			    prstatus.pr_reg[R_PC]);
			break;
		}
		clear_breaks(ph);

		/*
		 * If this was a BP at which we should stop
		 */
		if (bpt->bl_flags & MASK_BP_STOP)
			break;

		step_n(ph, 1, FLG_SN_NONE);
	}

	if (bpt->bl_flags & FLG_BP_USERDEF)
		printf("break point reached at addr: 0x%lx\n",
			prstatus.pr_reg[R_PC]);

	if (bpt->bl_flags & MASK_BP_SPECIAL)
		handle_sp_break(ph);

	if (ph->pp_flags & FLG_PP_LMAPS) {
		if (get_linkmaps(ph) != PS_OK)
			fprintf(stderr, "problem loading linkmaps\n");
	}

	return (bpt->bl_flags);
}


static ulong_t
is_plt(struct ps_prochandle * ph, ulong_t pc)
{
	map_info_t *	mip;
	ulong_t	pltbase;

	if ((mip = addr_to_map(ph, pc)) == (map_info_t *)0)
		return ((ulong_t)0);

	pltbase = mip->mi_pltbase;
	if ((mip->mi_flags & FLG_MI_EXEC) == 0)
		pltbase += mip->mi_addr;

	if ((pc >= pltbase) && (pc <= (pltbase + mip->mi_pltsize)))
		return (pltbase);

	return ((ulong_t)0);
}


retc_t
step_n(struct ps_prochandle * ph, size_t count, sn_flags_e flgs)
{
	prrun_t		prrun;
	prstatus_t	prstatus;
	int		pfd = ph->pp_fd;
	int		i;

	if (ioctl(pfd, PIOCSTATUS, &prstatus) == -1)
		perr("sn: PIOCSTATUS");

	memset(&prrun, 0, sizeof (prrun));
	prrun.pr_flags = PRSTEP |  PRCFAULT | PRSFAULT;
	praddset(&prrun.pr_fault, FLTTRACE);
	prrun.pr_vaddr = 0;

	for (i = 0; i < count; i++) {
		bptlist_t *	bpt;
		ulong_t		pc;
		ulong_t		pltbase;

		pc = prstatus.pr_reg[R_PC];

		if ((bpt = find_bp(ph, pc)) != (bptlist_t *)-1) {
			if (bpt->bl_flags & MASK_BP_SPECIAL)
				handle_sp_break(ph);
		}

		if (flgs & FLG_SN_VERBOSE)
			disasm(ph, 1);

		if ((ioctl(pfd, PIOCRUN, &prrun)) != 0)
			perr("PIOCRUN steping");

		if ((ioctl(pfd, PIOCWSTOP, &prstatus)) != 0)
			perr("PIOCWSTOP stepping");

		pc = prstatus.pr_reg[R_PC];

		if ((prstatus.pr_why != PR_FAULTED) ||
		    (prstatus.pr_what != FLTTRACE)) {
			fprintf(stderr, "bad stop - stoped on why: 0x%x "
				"what: 0x%x\n",
				prstatus.pr_why, prstatus.pr_what);
			return (RET_FAILED);
		}

		if ((flgs & FLG_SN_PLTSKIP) &&
		    ((pltbase = is_plt(ph, pc)) != (ulong_t)0)) {
			rd_plt_info_t	rp;
			if (rd_plt_resolution(ph->pp_rap, pc,
			    prstatus.pr_who, pltbase, &rp) != RD_OK) {
				fprintf(stderr,
				    "sn: rd_plt_resolution failed\n");
				return (RET_FAILED);
			}
			if (rp.pi_skip_method == RD_RESOLVE_TARGET_STEP) {
				unsigned	bpflags;

				set_breakpoint(ph, rp.pi_target,
					FLG_BP_PLTRES);
				bpflags = continue_to_break(ph);
				delete_breakpoint(ph, rp.pi_target,
					FLG_BP_PLTRES);
				if (bpflags & FLG_BP_PLTRES)
					step_n(ph, rp.pi_nstep, FLG_SN_NONE);
			} else if (rp.pi_skip_method == RD_RESOLVE_STEP)
				step_n(ph, rp.pi_nstep, FLG_SN_NONE);
		}

	}
	if (ioctl(pfd, PIOCCFAULT, 0) != 0)
		perr("PIOCCFAULT");

	if ((flgs & FLG_SN_VERBOSE) && (ph->pp_flags & FLG_PP_LMAPS)) {
		if (get_linkmaps(ph) != PS_OK)
			fprintf(stderr, "problem loading linkmaps\n");
	}

	return (RET_OK);
}


void
step_to_addr(struct ps_prochandle * ph, ulong_t addr)
{
	prstatus_t	pstat;
	int		count = 0;
	ulong_t		caddr;
	int		pfd = ph->pp_fd;

	if (ioctl(pfd, PIOCSTATUS, &pstat) != 0)
		perr("step_to_addr: PIOCSTATUS");

	caddr = pstat.pr_reg[R_PC];

	while ((caddr > addr) || ((caddr + 0xff) < addr)) {
		step_n(ph, 1, FLG_SN_NONE);
		if (ioctl(pfd, PIOCSTATUS, &pstat) != 0)
			perr("step_to_addr: PIOCSTATUS2");
		caddr = pstat.pr_reg[R_PC];
		if ((count % 10000) == 0) {
			printf("%d: ", count);
			disasm(ph, 1);
		}

		count++;
	}

	printf("address found %d instructions in: pc: 0x%lx addr: 0x%lx\n",
		count, caddr, addr);
}
