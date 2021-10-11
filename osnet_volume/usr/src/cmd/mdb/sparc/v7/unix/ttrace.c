/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ttrace.c	1.2	99/11/19 SMI"

#include <mdb/mdb_modapi.h>
#include <sys/cpuvar.h>
#include <sys/traptrace.h>

/*
 * These structures aren't defined anywhere, but they match the implementation
 * in traptrace.h and locore.s.  Note that we use the padding word of the
 * trap_trace_ctl to stash our current trap trace record in the "ttrace" walk.
 */
typedef struct trap_trace_rec {
	uint32_t tt_tbr;
	uint32_t tt_psr;
	uint32_t tt_pc;
	uint32_t tt_sp;
	uint32_t tt_g7;
	uint32_t tt_tr;
	uint32_t tt_f1;
	uint32_t tt_f2;
} trap_trace_rec_t;

typedef struct trap_trace_ctl {
	uintptr_t ttc_next;
	uintptr_t ttc_first;
	uintptr_t ttc_limit;
	uintptr_t ttc_current;
} trap_trace_ctl_t;

int
ttrace_walk_init(mdb_walk_state_t *wsp)
{
	GElf_Sym sym;
	trap_trace_ctl_t *ttc;
	uintptr_t addr;

	if (wsp->walk_addr >= NCPU) {
		mdb_warn("cpu_id %d out of range\n", wsp->walk_addr);
		return (WALK_ERR);
	}

	if (mdb_lookup_by_name("trap_trace_ctl", &sym) == -1) {
		mdb_warn("couldn't find 'trap_trace_ctl'\n");
		return (WALK_ERR);
	}

	addr = (uintptr_t)sym.st_value +
	    wsp->walk_addr * sizeof (trap_trace_ctl_t);

	ttc = mdb_alloc(sizeof (trap_trace_ctl_t), UM_SLEEP);

	if (mdb_vread(ttc, sizeof (trap_trace_ctl_t), addr) == -1) {
		mdb_warn("couldn't read trap_trace_ctl at %p", addr);
		mdb_free(ttc, sizeof (trap_trace_ctl_t));
		return (WALK_ERR);
	}

	ttc->ttc_current = ttc->ttc_next - sizeof (trap_trace_rec_t);

	wsp->walk_data = ttc;

	return (WALK_NEXT);
}

int
ttrace_walk_step(mdb_walk_state_t *wsp)
{
	trap_trace_ctl_t *ttc = wsp->walk_data;
	trap_trace_rec_t rec;
	int rval;

	if (ttc->ttc_current < ttc->ttc_first)
		ttc->ttc_current = ttc->ttc_limit - sizeof (trap_trace_rec_t);

	if (mdb_vread(&rec, sizeof (rec), ttc->ttc_current) == -1) {
		mdb_warn("couldn't read trace record at %p", ttc->ttc_current);
		return (WALK_ERR);
	}

	rval = wsp->walk_callback(ttc->ttc_current, &rec, wsp->walk_cbdata);

	if (ttc->ttc_current == ttc->ttc_next)
		return (WALK_DONE);

	ttc->ttc_current -= sizeof (trap_trace_rec_t);

	return (rval);
}

void
ttrace_walk_fini(mdb_walk_state_t *wsp)
{
	mdb_free(wsp->walk_data, sizeof (trap_trace_ctl_t));
}

static const char NOT[] = "reserved";	/* common reserved string */
static const char BAD[] = "unused";	/* common unused string */

#define	BAD4	BAD, BAD, BAD, BAD

static const char *const ttdescr[] = {
	"reset",			/* 00	reset */
	"inst-xcp",			/* 01	instruction access exception */
	"ill-inst",			/* 02	illegal instruction */
	"priv-inst",			/* 03	privileged instruction */
	"fp-disabled",			/* 04	floating point disabled */
	"overflow",			/* 05	register window overflow */
	"underflow",			/* 06	register window underflow */
	"alignment",			/* 07	alignment */
	"fp-xcp",			/* 08	floating point exception */
	"data-xcp",			/* 09	data access exception */
	"tag-overflw",			/* 0A	tag overflow */
	BAD, BAD, BAD4,			/* 0B - 10 unused */
	"level-1",			/* 11	interrupt level 1 */
	"level-2",			/* 12	interrupt level 2 */
	"level-3",			/* 13	interrupt level 3 */
	"level-4",			/* 14	interrupt level 4 */
	"level-5",			/* 15	interrupt level 5 */
	"level-6",			/* 16	interrupt level 6 */
	"level-7",			/* 17	interrupt level 7 */
	"level-8",			/* 18	interrupt level 8 */
	"level-9",			/* 19	interrupt level 9 */
	"level-10",			/* 1A	interrupt level 10 */
	"level-11",			/* 1B	interrupt level 11 */
	"level-12",			/* 1C	interrupt level 12 */
	"level-13",			/* 1D	interrupt level 13 */
	"level-14",			/* 1E	interrupt level 14 */
	"level-15",			/* 1F	interrupt level 15 */
	NOT,				/* 20	reserved */
	"inst-err",			/* 21	instruction access error */
	NOT, NOT,			/* 22 - 23 reserved */
	"cp-disabled",			/* 24	coprocessor disabled */
	"unimp-flush",			/* 25	unimplemented flush */
	BAD, BAD,			/* 26 - 27 unused */
	"cp-xcp",			/* 28	coprocessor exception */
	"data-err",			/* 29	data access error */
	"div-zero",			/* 2A	division by zero */
	"data-st-xcp",			/* 2B	data store exception */
	"data-mmu",			/* 2C	data access MMU miss */
	BAD4, BAD4, BAD4, BAD, BAD,	/* 2D - 3A unused */
	BAD,				/* 3B	unused */
	"inst-mmu",			/* 3C	instruction access MMU miss */
	BAD, BAD,			/* 3D - 3E unused */
	BAD4, BAD4, BAD4, BAD4,		/* 3F - 4F unused */
	BAD4, BAD4, BAD4, BAD4,		/* 50 - 5F unused */
	BAD4, BAD4, BAD4, BAD4,		/* 60 - 6F unused */
	BAD4, BAD4, BAD4, BAD4,		/* 70 - 7F unused */
	"syscall-4x",			/* 80	old system call */
	"usr-brkpt",			/* 81	user breakpoint */
	"usr-div-zero",			/* 82	user divide by zero */
	"flush-wins",			/* 83	flush windows */
	"clean-wins",			/* 84	clean windows */
	"range-chk",			/* 85	range check */
	"fix-align",			/* 86	do unaligned references */
	BAD,				/* 87	unused */
	"syscall",			/* 88	system call */
	"set-t0-addr",			/* 89	set trap0 address */
	BAD, BAD, BAD4,			/* 8A - 8F unused */
	BAD4, BAD4, BAD4, BAD4,		/* 90 - 9F unused */
	"get-cc",			/* A0	get condition codes */
	"set-cc",			/* A1	set condition codes */
	"get-psr",			/* A2	get psr */
	"set-psr",			/* A3	set psr (some fields) */
	"getts",			/* A4	get timestamp */
	"gethrvtime",			/* A5	get lwp virtual time */
	BAD,				/* A6	unused */
	"gethrtime",			/* A7	get hrestime */
	BAD4, BAD4,			/* A8 - AF unused */
	"trace-0",			/* B0	trace, no data */
	"trace-1",			/* B1	trace, 1 data word */
	"trace-2",			/* B2	trace, 2 data words */
	"trace-3",			/* B3	trace, 3 data words */
	"trace-4",			/* B4	trace, 4 data words */
	"trace-5",			/* B5	trace, 5 data words */
	BAD,				/* B6	trace, unused */
	"trace-wr-buf",			/* B7	trace, atomic buf write */
	BAD4, BAD4,			/* B8 - BF unused */
	BAD4, BAD4, BAD4, BAD4,		/* C0 - CF unused */
	BAD4, BAD4, BAD4, BAD4,		/* D0 - DF unused */
	BAD4, BAD4, BAD4, BAD4,		/* E0 - EF unused */
	BAD4, BAD4, BAD4, BAD4		/* F0 - FF unused */
};
static const size_t ttndescr = sizeof (ttdescr) / sizeof (ttdescr[0]);

static struct {
	int tbr_code;
	char *tbr_what;
} tbr[] = {
	{ TT_OV_USR,	"overflow-usr" },
	{ TT_OV_SYS,	"overflow-sys" },
	{ TT_OV_SHR,	"overflow-shr" },
	{ TT_OV_SHRK,	"overflow-shrk" },
	{ TT_OV_BUF,	"overflow-buf" },
	{ TT_OV_BUFK,	"overflow-bufk" },
	{ TT_UF_USR,	"underflow-usr" },
	{ TT_UF_SYS,	"underflow-sys" },
	{ TT_UF_FAULT,	"underflow-flt" },
	{ TT_SC_RET,	"syscall-ret" },
	{ TT_SC_POST,	"syscall-post" },
	{ TT_SC_TRAP,	"syscall-trap" },
	{ TT_SYS_RTT,	"sys-rtt" },
	{ TT_SYS_RTTU,	"sys-rtt-usr" },
	{ TT_INTR_ENT,	"intr-entry" },
	{ TT_INTR_RET,	"intr-ret" },
	{ TT_INTR_RET2,	"intr-ret2" },
	{ TT_INTR_EXIT,	"intr-exit" },
	{ 0, NULL }
};

#define	TBR2TT(tbr) ((((uintptr_t)(tbr)) & 0x00000ff0) >> 4)
#define	DUMP(reg) #reg, rec->tt_##reg
#define	FOURREGS  "              %3s: %08x %3s: %08x %3s: %08x %3s: %08x\n"

int
ttrace_walk(uintptr_t addr, trap_trace_rec_t *rec, uint_t *extended)
{
	int tt, i;
	const char *what = NULL;

	for (i = 0; tbr[i].tbr_what != NULL; i++) {
		if (rec->tt_tbr == tbr[i].tbr_code) {
			what = tbr[i].tbr_what;
			/*
			 * The longest code is 16 bits long; we whack off
			 * the upper bits to be able to print the code
			 * out in four characters below.
			 */
			tt = rec->tt_tbr & 0xffff;
			break;
		}
	}

	/*
	 * If the %tbr didn't match any of the special codes, then we lookup
	 * its tt.
	 */
	if (what == NULL) {
		tt = TBR2TT(rec->tt_tbr);
		what = tt < ttndescr ? ttdescr[tt] : "?";
	}

	mdb_printf("%08x %04x %-12s %08x %08x %a\n", addr, tt,
	    what, rec->tt_psr, rec->tt_g7, rec->tt_pc);

	if (*extended == FALSE)
		return (DCMD_OK);

	mdb_printf(FOURREGS, DUMP(tbr), DUMP(psr), DUMP(pc), DUMP(sp));
	mdb_printf(FOURREGS, DUMP(g7), DUMP(tr), DUMP(f1), DUMP(f2));
	mdb_printf("\n");

	return (DCMD_OK);
}

int
ttrace(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	trap_trace_rec_t rec;
	uint_t ex = FALSE;

	if (!(flags & DCMD_ADDRSPEC)) {
		mdb_warn("expected explicit CPU id or buffer address\n");
		return (DCMD_USAGE);
	}

	if (mdb_getopts(argc, argv,
	    'x', MDB_OPT_SETBITS, TRUE, &ex, NULL) != argc)
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%-8s %-4s %-12s %8s %-8s %s\n", "ADDR",
		    "TT", "TYPE", "PSR", "THREAD", "PC");
	}

	/*
	 * This is a little weak.  If one provides an addr greater than NCPU,
	 * we'll attempt to read the addr as a trap trace buffer.  Otherwise,
	 * we'll dump all of the records for the specified cpu.
	 */
	if (addr >= NCPU) {
		if (mdb_vread(&rec, sizeof (rec), addr) == -1) {
			mdb_warn("couldn't read trap trace record at %p", addr);
			return (DCMD_ERR);
		}

		if (ttrace_walk(addr, &rec, &ex) == -1)
			return (DCMD_ERR);

		return (DCMD_OK);
	}

	if (mdb_pwalk("ttrace", (mdb_walk_cb_t)ttrace_walk, &ex, addr) == -1)
		mdb_warn("couldn't walk 'ttrace'");

	return (DCMD_OK);
}
