/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)unix.c	1.1	99/08/11 SMI"

#include <mdb/mdb_modapi.h>
#include <sys/cpuvar.h>
#include <sys/traptrace.h>
#include <sys/avintr.h>
#include <sys/systm.h>
#include <sys/trap.h>

#define	TT_HDLR_WIDTH	17

int
ttrace_walk_init(mdb_walk_state_t *wsp)
{
	trap_trace_ctl_t *ttcp;
	size_t ttc_size = sizeof (trap_trace_ctl_t) * NCPU;
	int i;

	ttcp = mdb_zalloc(ttc_size, UM_SLEEP);

	if (wsp->walk_addr != NULL) {
		mdb_warn("ttrace only supports global walks\n");
		return (WALK_ERR);
	}

	if (mdb_readsym(ttcp, ttc_size, "trap_trace_ctl") == -1) {
		mdb_warn("symbol 'trap_trace_ctl' not found; "
		    "non-TRAPTRACE kernel?\n");
		mdb_free(ttcp, ttc_size);
		return (WALK_ERR);
	}

	/*
	 * We'll poach the ttc_current pointer (which isn't used for
	 * anything) to store a pointer to our current TRAPTRACE record.
	 * This allows us to only keep the array of trap_trace_ctl structures
	 * as our walker state (ttc_current may be the only kernel data
	 * structure member added exclusively to make writing the mdb walker
	 * a little easier).
	 */
	for (i = 0; i < NCPU; i++) {
		trap_trace_ctl_t *ttc = &ttcp[i];

		if (ttc->ttc_first == NULL)
			continue;

		/*
		 * Assign ttc_current to be the last completed record.
		 * Note that the error checking (i.e. in the ttc_next ==
		 * ttc_first case) is performed in the step function.
		 */
		ttc->ttc_current = ttc->ttc_next - sizeof (trap_trace_rec_t);
	}

	wsp->walk_data = ttcp;
	return (WALK_NEXT);
}

int
ttrace_walk_step(mdb_walk_state_t *wsp)
{
	trap_trace_ctl_t *ttcp = wsp->walk_data, *ttc, *latest_ttc;
	trap_trace_rec_t rec;
	int rval, i, recsize = sizeof (trap_trace_rec_t);
	hrtime_t latest = 0;

	/*
	 * Loop through the CPUs, looking for the latest trap trace record
	 * (we want to walk through the trap trace records in reverse
	 * chronological order).
	 */
	for (i = 0; i < NCPU; i++) {
		ttc = &ttcp[i];

		if (ttc->ttc_current == NULL)
			continue;

		if (ttc->ttc_current < ttc->ttc_first)
			ttc->ttc_current = ttc->ttc_limit - recsize;

		if (mdb_vread(&rec, sizeof (rec), ttc->ttc_current) == -1) {
			mdb_warn("couldn't read rec at %p", ttc->ttc_current);
			return (WALK_ERR);
		}

		if (rec.ttr_stamp > latest) {
			latest = rec.ttr_stamp;
			latest_ttc = ttc;
		}
	}

	if (latest == 0)
		return (WALK_DONE);

	ttc = latest_ttc;

	if (mdb_vread(&rec, sizeof (rec), ttc->ttc_current) == -1) {
		mdb_warn("couldn't read rec at %p", ttc->ttc_current);
		return (WALK_ERR);
	}

	rval = wsp->walk_callback(ttc->ttc_current, &rec, wsp->walk_cbdata);

	if (ttc->ttc_current == ttc->ttc_next)
		ttc->ttc_current = NULL;
	else
		ttc->ttc_current -= sizeof (trap_trace_rec_t);

	return (rval);
}

void
ttrace_walk_fini(mdb_walk_state_t *wsp)
{
	mdb_free(wsp->walk_data, sizeof (trap_trace_ctl_t) * NCPU);
}

static int
ttrace_syscall(trap_trace_rec_t *rec)
{
	GElf_Sym sym;
	int sysnum = rec->ttr_sysnum;
	uintptr_t addr;
	struct sysent sys;

	mdb_printf("%s%-*x", sysnum < 0x10 ? " " : "",
	    sysnum < 0x10 ? 2 : 3, sysnum);

	if (rec->ttr_sysnum > NSYSCALL) {
		mdb_printf("%-*s", TT_HDLR_WIDTH, "(???)");
		return (0);
	}

	if (mdb_lookup_by_name("sysent", &sym) == -1) {
		mdb_warn("\ncouldn't find 'sysent'");
		return (-1);
	}

	addr = (uintptr_t)sym.st_value + sysnum * sizeof (struct sysent);

	if (addr >= (uintptr_t)sym.st_value + sym.st_size) {
		mdb_warn("\nsysnum %d out-of-range\n", sysnum);
		return (-1);
	}

	if (mdb_vread(&sys, sizeof (sys), addr) == -1) {
		mdb_warn("\nfailed to read sysent at %p", addr);
		return (-1);
	}

	mdb_printf("%-*a", TT_HDLR_WIDTH, sys.sy_callc);

	return (0);
}

static int
ttrace_interrupt(trap_trace_rec_t *rec)
{
	GElf_Sym sym;
	uintptr_t addr;
	struct av_head hd;
	struct autovec av;

	if (rec->ttr_regs.r_trapno == T_SOFTINT) {
		mdb_printf("%2s %-*s", "-", TT_HDLR_WIDTH, "(fakesoftint)");
		return (0);
	}

	mdb_printf("%2x ", rec->ttr_vector);

	if (mdb_lookup_by_name("autovect", &sym) == -1) {
		mdb_warn("\ncouldn't find 'autovect'");
		return (-1);
	}

	addr = (uintptr_t)sym.st_value +
	    rec->ttr_vector * sizeof (struct av_head);

	if (addr >= (uintptr_t)sym.st_value + sym.st_size) {
		mdb_warn("\nav_head for vec %x is corrupt\n", rec->ttr_vector);
		return (-1);
	}

	if (mdb_vread(&hd, sizeof (hd), addr) == -1) {
		mdb_warn("\ncouldn't read av_head for vec %x", rec->ttr_vector);
		return (-1);
	}

	if (hd.avh_link == NULL) {
		mdb_printf("%-*s", TT_HDLR_WIDTH, "(spurious)");
	} else {
		if (mdb_vread(&av, sizeof (av), (uintptr_t)hd.avh_link) == -1) {
			mdb_warn("couldn't read autovec at %p",
			    (uintptr_t)hd.avh_link);
		}

		mdb_printf("%-*a", TT_HDLR_WIDTH, av.av_vector);
	}

	return (0);
}

static struct {
	int tt_trapno;
	char *tt_name;
} ttrace_traps[] = {
	{ T_ZERODIV,	"divide-error" },
	{ T_SGLSTP,	"debug-exception" },
	{ T_NMIFLT,	"nmi-interrupt" },
	{ T_BPTFLT,	"breakpoint" },
	{ T_OVFLW,	"into-overflow" },
	{ T_BOUNDFLT,	"bound-exceeded" },
	{ T_ILLINST,	"invalid-opcode" },
	{ T_NOEXTFLT,	"device-not-avail" },
	{ T_DBLFLT,	"double-fault" },
	{ T_EXTOVRFLT,	"segment-overrun" },
	{ T_TSSFLT,	"invalid-tss" },
	{ T_SEGFLT,	"segment-not-pres" },
	{ T_STKFLT,	"stack-fault" },
	{ T_GPFLT,	"general-protectn" },
	{ T_PGFLT,	"page-fault" },
	{ T_EXTERRFLT,	"error-fault" },
	{ T_ALIGNMENT,	"alignment-check" },
	{ T_MCE,	"machine-check" },
	{ 0,		NULL }
};

static int
ttrace_trap(trap_trace_rec_t *rec)
{
	int i;

	mdb_printf("%2x ", rec->ttr_regs.r_trapno);

	for (i = 0; ttrace_traps[i].tt_name != NULL; i++) {
		if (rec->ttr_regs.r_trapno == ttrace_traps[i].tt_trapno)
			break;
	}

	if (ttrace_traps[i].tt_name == NULL)
		mdb_printf("%-*s", TT_HDLR_WIDTH, "(unknown)");
	else
		mdb_printf("%-*s", TT_HDLR_WIDTH, ttrace_traps[i].tt_name);

	return (0);
}

static struct {
	uchar_t t_marker;
	char *t_name;
	int (*t_hdlr)(trap_trace_rec_t *);
} ttrace_hdlr[] = {
	{ TT_SYSCALL, "sysc", ttrace_syscall },
	{ TT_INTERRUPT, "intr", ttrace_interrupt },
	{ TT_TRAP, "trap", ttrace_trap },
	{ 0, NULL, NULL }
};

typedef struct ttrace_dcmd {
	processorid_t ttd_cpu;
	uint_t ttd_extended;
	trap_trace_ctl_t ttd_ttc[NCPU];
} ttrace_dcmd_t;

#define	DUMP(reg) #reg, regs->r_##reg
#define	FOURREGS  "         %3s: %08x %3s: %08x %3s: %08x %3s: %08x\n"
#define	THREEREGS "         %3s: %08x %3s: %08x %3s: %08x\n"

int
ttrace_walk(uintptr_t addr, trap_trace_rec_t *rec, ttrace_dcmd_t *dcmd)
{
	struct regs *regs = &rec->ttr_regs;
	processorid_t cpu = -1, i;

	for (i = 0; i < NCPU; i++) {
		if (addr >= dcmd->ttd_ttc[i].ttc_first &&
		    addr < dcmd->ttd_ttc[i].ttc_limit) {
			cpu = i;
			break;
		}
	}

	if (cpu == -1) {
		mdb_warn("couldn't find %p in any trap trace ctl\n", addr);
		return (WALK_ERR);
	}

	if (dcmd->ttd_cpu != -1 && cpu != dcmd->ttd_cpu)
		return (WALK_NEXT);

	mdb_printf("%3d %08x %15llx ", cpu, addr, rec->ttr_stamp);

	for (i = 0; ttrace_hdlr[i].t_hdlr != NULL; i++) {
		if (rec->ttr_marker != ttrace_hdlr[i].t_marker)
			continue;
		mdb_printf("%4s ", ttrace_hdlr[i].t_name);
		if (ttrace_hdlr[i].t_hdlr(rec) == -1)
			return (WALK_ERR);
	}

	mdb_printf("%a\n", regs->r_eip);

	if (dcmd->ttd_extended == FALSE)
		return (WALK_NEXT);

	mdb_printf(FOURREGS, DUMP(gs), DUMP(fs), DUMP(es), DUMP(ds));
	mdb_printf(FOURREGS, DUMP(edi), DUMP(esi), DUMP(ebp), DUMP(esp));
	mdb_printf(FOURREGS, DUMP(ebx), DUMP(edx), DUMP(ecx), DUMP(eax));
	mdb_printf(FOURREGS, "trp", regs->r_trapno, DUMP(err),
	    DUMP(eip), DUMP(cs));
	mdb_printf(THREEREGS, DUMP(efl), "usp", regs->r_uesp, DUMP(ss));
	mdb_printf("\n");

	return (WALK_NEXT);
}

int
ttrace(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	ttrace_dcmd_t dcmd;
	trap_trace_ctl_t *ttc = dcmd.ttd_ttc;
	trap_trace_rec_t rec;
	size_t ttc_size = sizeof (trap_trace_ctl_t) * NCPU;

	bzero(&dcmd, sizeof (dcmd));
	dcmd.ttd_cpu = -1;
	dcmd.ttd_extended = FALSE;

	if (mdb_readsym(ttc, ttc_size, "trap_trace_ctl") == -1) {
		mdb_warn("symbol 'trap_trace_ctl' not found; "
		    "non-TRAPTRACE kernel?\n");
		return (DCMD_ERR);
	}

	if (mdb_getopts(argc, argv,
	    'x', MDB_OPT_SETBITS, TRUE, &dcmd.ttd_extended, NULL) != argc)
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%3s %8s %15s %4s %2s %-*s%s\n", "CPU", "ADDR",
		    "TIMESTAMP", "TYPE", "VC", TT_HDLR_WIDTH, "HANDLER",
		    "EIP");
	}

	if (flags & DCMD_ADDRSPEC) {
		if (addr >= NCPU) {
			if (mdb_vread(&rec, sizeof (rec), addr) == -1) {
				mdb_warn("couldn't read trap trace record "
				    "at %p", addr);
				return (DCMD_ERR);
			}

			if (ttrace_walk(addr, &rec, &dcmd) == WALK_ERR)
				return (DCMD_ERR);

			return (DCMD_OK);
		}
		dcmd.ttd_cpu = addr;
	}

	if (mdb_walk("ttrace", (mdb_walk_cb_t)ttrace_walk, &dcmd) == -1) {
		mdb_warn("couldn't walk 'ttrace'");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

static const mdb_dcmd_t dcmds[] = {
	{ "ttrace", "[-x]", "dump trap trace buffers", ttrace },
	{ NULL }
};

static const mdb_walker_t walkers[] = {
	{ "ttrace", "walks trap trace buffers in reverse chronological order",
		ttrace_walk_init, ttrace_walk_step, ttrace_walk_fini },
	{ NULL }
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds, walkers };

const mdb_modinfo_t *
_mdb_init(void)
{
	return (&modinfo);
}
