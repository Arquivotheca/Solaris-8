/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)unix.c	1.4	99/11/20 SMI"

#include <sys/types.h>

#ifndef DEBUG
#define	DEBUG
#define	_SYS_DEBUG_H
#include <sys/xc_impl.h>
#undef	DEBUG
#else
#define	_SYS_DEBUG_H
#include <sys/xc_impl.h>
#endif

#include <sys/traptrace.h>
#include <sys/machparam.h>
#include <sys/ivintr.h>

#include <mdb/mdb_modapi.h>

#ifndef SYSTRAP_TT
#define	SYSTRAP_TT	0x1300
#endif

typedef struct trap_trace_fullrec {
	struct trap_trace_record ttf_rec;
	int ttf_cpu;
} trap_trace_fullrec_t;

/*
 * These strings and accompanying macros allow our string table to look
 * just like the real table in trap_table.s.
 */

static const char NOT[] = "reserved";	/* common reserved string */
static const char BAD[] = "unused";	/* common unused string */

#define	NOT4	NOT, NOT, NOT, NOT
#define	BAD4	BAD, BAD, BAD, BAD

static const char *const ttdescr[] = {
	NOT,				/* 000	reserved */
	"power-on",			/* 001	power on reset */
	"watchdog",			/* 002	watchdog reset */
	"xir",				/* 003	externally initiated reset */
	"sir",				/* 004	software initiated reset */
	"red",				/* 005	red mode exception */
	NOT, NOT,			/* 006 - 007 reserved */
	"reserved",			/* 007	reserved */
	"immu-xcp",			/* 008	instruction access exception */
	"immu-miss",			/* 009	instruction access MMU miss */
	"immu-err",			/* 00A	instruction access error */
	NOT, NOT4,			/* 00B - 00F reserved */
	"ill-inst",			/* 010	illegal instruction */
	"priv-inst",			/* 011	privileged opcode */
	"unimp-ldd",			/* 012	unimplemented LDD */
	"unimp-std",			/* 013	unimplemented STD */
	NOT4, NOT4, NOT4,		/* 014 - 01F reserved */
	"fp-disable"			/* 020	fp disabled */
	"fp-ieee754",			/* 021	fp exception ieee 754 */
	"fp-xcp-other",			/* 022	fp exception other */
	"tag-oflow",			/* 023	tag overflow */
	"cleanwin",			/* 024	clean window */
	"cleanwin",			/* 025	clean window */
	"cleanwin",			/* 026	clean window */
	"cleanwin",			/* 027	clean window */
	"div-zero",			/* 028	division by zero */
	"internal-err",			/* 029	internal processor error */
	NOT, NOT, NOT4,			/* 02A - 02F reserved */
	"dmmu-xcp",			/* 030	data access exception */
	"dmmu-miss",			/* 031	data access MMU miss */
	"dmmu-err",			/* 032	data access error */
	"dmmu-prot",			/* 033	data access protection */
	"unalign",			/* 034	mem address not aligned */
	"lddf-unalign",			/* 035	LDDF mem address not aligned */
	"stdf-unalign",			/* 036	STDF mem address not aligned */
	"priv-act",			/* 037	privileged action */
	"ldqf-unalign",			/* 038	LDQF mem address not aligned */
	"stqf-unalign",			/* 039	STQF mem address not aligned */
	NOT, NOT, NOT4,			/* 03A - 03F reserved */
	"async-d-err",			/* 040	async data error */
	"level-1",			/* 041	interrupt level 1 */
	"level-2",			/* 042	interrupt level 2 */
	"level-3",			/* 043	interrupt level 3 */
	"level-4",			/* 044	interrupt level 4 */
	"level-5",			/* 045	interrupt level 5 */
	"level-6",			/* 046	interrupt level 6 */
	"level-7",			/* 047	interrupt level 7 */
	"level-8",			/* 048	interrupt level 8 */
	"level-9",			/* 049	interrupt level 9 */
	"level-10",			/* 04A	interrupt level 10 */
	"level-11",			/* 04B	interrupt level 11 */
	"level-12",			/* 04C	interrupt level 12 */
	"level-13",			/* 04D	interrupt level 13 */
	"level-14",			/* 04E	interrupt level 14 */
	"level-15",			/* 04F	interrupt level 15 */
	NOT4, NOT4, NOT4, NOT4,		/* 050 - 05F reserved */
	"int-vec",			/* 060	interrupt vector */
	"pa-watch",			/* 061	PA watchpoint */
	"va-watch",			/* 062	VA watchpoint */
	"ecc-err",			/* 063	corrected ECC error */
	"itlb-miss",			/* 064	instruction access MMU miss */
	"itlb-miss",			/* 065	instruction access MMU miss */
	"itlb-miss",			/* 066	instruction access MMU miss */
	"itlb-miss",			/* 067	instruction access MMU miss */
	"dtlb-miss",			/* 068	data access MMU miss */
	"dtlb-miss",			/* 069	data access MMU miss */
	"dtlb-miss",			/* 06A	data access MMU miss */
	"dtlb-miss",			/* 06B	data access MMU miss */
	"dtlb-prot",			/* 06C	data access protection */
	"dtlb-prot",			/* 06D	data access protection */
	"dtlb-prot",			/* 06E	data access protection */
	"dtlb-prot",			/* 06F	data access protection */
	NOT4, NOT4, NOT4, NOT4,		/* 070 - 07F reserved */
	"spill-0-norm",			/* 080	spill 0 normal */
	"spill-0-norm",			/* 081	spill 0 normal */
	"spill-0-norm",			/* 082	spill 0 normal */
	"spill-0-norm",			/* 083	spill 0 normal */
	"spill-1-norm",			/* 084	spill 1 normal */
	"spill-1-norm",			/* 085	spill 1 normal */
	"spill-1-norm",			/* 086	spill 1 normal */
	"spill-1-norm",			/* 087	spill 1 normal */
	"spill-2-norm",			/* 088	spill 2 normal */
	"spill-2-norm",			/* 089	spill 2 normal */
	"spill-2-norm",			/* 08A	spill 2 normal */
	"spill-2-norm",			/* 08B	spill 2 normal */
	"spill-3-norm",			/* 08C	spill 3 normal */
	"spill-3-norm",			/* 08D	spill 3 normal */
	"spill-3-norm",			/* 08E	spill 3 normal */
	"spill-3-norm",			/* 08F	spill 3 normal */
	"spill-4-norm",			/* 090	spill 4 normal */
	"spill-4-norm",			/* 091	spill 4 normal */
	"spill-4-norm",			/* 092	spill 4 normal */
	"spill-4-norm",			/* 093	spill 4 normal */
	"spill-5-norm",			/* 094	spill 5 normal */
	"spill-5-norm",			/* 095	spill 5 normal */
	"spill-5-norm",			/* 096	spill 5 normal */
	"spill-5-norm",			/* 097	spill 5 normal */
	"spill-6-norm",			/* 098	spill 6 normal */
	"spill-6-norm",			/* 099	spill 6 normal */
	"spill-6-norm",			/* 09A	spill 6 normal */
	"spill-6-norm",			/* 09B	spill 6 normal */
	"spill-7-norm",			/* 09C	spill 7 normal */
	"spill-7-norm",			/* 09D	spill 7 normal */
	"spill-7-norm",			/* 09E	spill 7 normal */
	"spill-7-norm",			/* 09F	spill 7 normal */
	"spill-0-oth",			/* 0A0	spill 0 other */
	"spill-0-oth",			/* 0A1	spill 0 other */
	"spill-0-oth",			/* 0A2	spill 0 other */
	"spill-0-oth",			/* 0A3	spill 0 other */
	"spill-1-oth",			/* 0A4	spill 1 other */
	"spill-1-oth",			/* 0A5	spill 1 other */
	"spill-1-oth",			/* 0A6	spill 1 other */
	"spill-1-oth",			/* 0A7	spill 1 other */
	"spill-2-oth",			/* 0A8	spill 2 other */
	"spill-2-oth",			/* 0A9	spill 2 other */
	"spill-2-oth",			/* 0AA	spill 2 other */
	"spill-2-oth",			/* 0AB	spill 2 other */
	"spill-3-oth",			/* 0AC	spill 3 other */
	"spill-3-oth",			/* 0AD	spill 3 other */
	"spill-3-oth",			/* 0AE	spill 3 other */
	"spill-3-oth",			/* 0AF	spill 3 other */
	"spill-4-oth",			/* 0B0	spill 4 other */
	"spill-4-oth",			/* 0B1	spill 4 other */
	"spill-4-oth",			/* 0B2	spill 4 other */
	"spill-4-oth",			/* 0B3	spill 4 other */
	"spill-5-oth",			/* 0B4	spill 5 other */
	"spill-5-oth",			/* 0B5	spill 5 other */
	"spill-5-oth",			/* 0B6	spill 5 other */
	"spill-5-oth",			/* 0B7	spill 5 other */
	"spill-6-oth",			/* 0B8	spill 6 other */
	"spill-6-oth",			/* 0B9	spill 6 other */
	"spill-6-oth",			/* 0BA	spill 6 other */
	"spill-6-oth",			/* 0BB	spill 6 other */
	"spill-7-oth",			/* 0BC	spill 7 other */
	"spill-7-oth",			/* 0BD	spill 7 other */
	"spill-7-oth",			/* 0BE	spill 7 other */
	"spill-7-oth",			/* 0BF	spill 7 other */
	"fill-0-norm",			/* 0C0	fill 0 normal */
	"fill-0-norm",			/* 0C1	fill 0 normal */
	"fill-0-norm",			/* 0C2	fill 0 normal */
	"fill-0-norm",			/* 0C3	fill 0 normal */
	"fill-1-norm",			/* 0C4	fill 1 normal */
	"fill-1-norm",			/* 0C5	fill 1 normal */
	"fill-1-norm",			/* 0C6	fill 1 normal */
	"fill-1-norm",			/* 0C7	fill 1 normal */
	"fill-2-norm",			/* 0C8	fill 2 normal */
	"fill-2-norm",			/* 0C9	fill 2 normal */
	"fill-2-norm",			/* 0CA	fill 2 normal */
	"fill-2-norm",			/* 0CB	fill 2 normal */
	"fill-3-norm",			/* 0CC	fill 3 normal */
	"fill-3-norm",			/* 0CD	fill 3 normal */
	"fill-3-norm",			/* 0CE	fill 3 normal */
	"fill-3-norm",			/* 0CF	fill 3 normal */
	"fill-4-norm",			/* 0D0	fill 4 normal */
	"fill-4-norm",			/* 0D1	fill 4 normal */
	"fill-4-norm",			/* 0D2	fill 4 normal */
	"fill-4-norm",			/* 0D3	fill 4 normal */
	"fill-5-norm",			/* 0D4	fill 5 normal */
	"fill-5-norm",			/* 0D5	fill 5 normal */
	"fill-5-norm",			/* 0D6	fill 5 normal */
	"fill-5-norm",			/* 0D7	fill 5 normal */
	"fill-6-norm",			/* 0D8	fill 6 normal */
	"fill-6-norm",			/* 0D9	fill 6 normal */
	"fill-6-norm",			/* 0DA	fill 6 normal */
	"fill-6-norm",			/* 0DB	fill 6 normal */
	"fill-7-norm",			/* 0DC	fill 7 normal */
	"fill-7-norm",			/* 0DD	fill 7 normal */
	"fill-7-norm",			/* 0DE	fill 7 normal */
	"fill-7-norm",			/* 0DF	fill 7 normal */
	"fill-0-oth",			/* 0E0	fill 0 other */
	"fill-0-oth",			/* 0E1	fill 0 other */
	"fill-0-oth",			/* 0E2	fill 0 other */
	"fill-0-oth",			/* 0E3	fill 0 other */
	"fill-1-oth",			/* 0E4	fill 1 other */
	"fill-1-oth",			/* 0E5	fill 1 other */
	"fill-1-oth",			/* 0E6	fill 1 other */
	"fill-1-oth",			/* 0E7	fill 1 other */
	"fill-2-oth",			/* 0E8	fill 2 other */
	"fill-2-oth",			/* 0E9	fill 2 other */
	"fill-2-oth",			/* 0EA	fill 2 other */
	"fill-2-oth",			/* 0EB	fill 2 other */
	"fill-3-oth",			/* 0EC	fill 3 other */
	"fill-3-oth",			/* 0ED	fill 3 other */
	"fill-3-oth",			/* 0EE	fill 3 other */
	"fill-3-oth",			/* 0EF	fill 3 other */
	"fill-4-oth",			/* 0F0	fill 4 other */
	"fill-4-oth",			/* 0F1	fill 4 other */
	"fill-4-oth",			/* 0F2	fill 4 other */
	"fill-4-oth",			/* 0F3	fill 4 other */
	"fill-5-oth",			/* 0F4	fill 5 other */
	"fill-5-oth",			/* 0F5	fill 5 other */
	"fill-5-oth",			/* 0F6	fill 5 other */
	"fill-5-oth",			/* 0F7	fill 5 other */
	"fill-6-oth",			/* 0F8	fill 6 other */
	"fill-6-oth",			/* 0F9	fill 6 other */
	"fill-6-oth",			/* 0FA	fill 6 other */
	"fill-6-oth",			/* 0FB	fill 6 other */
	"fill-7-oth",			/* 0FC	fill 7 other */
	"fill-7-oth",			/* 0FD	fill 7 other */
	"fill-7-oth",			/* 0FE	fill 7 other */
	"fill-7-oth",			/* 0FF	fill 7 other */
	"syscall-4x",			/* 100	old system call */
	"usr-brkpt",			/* 101	user breakpoint */
	"usr-div-zero",			/* 102	user divide by zero */
	"flush-wins",			/* 103	flush windows */
	"clean-wins",			/* 104	clean windows */
	"range-chk",			/* 105	range check ?? */
	"fix-align",			/* 106	do unaligned references */
	BAD,				/* 107	unused */
	"syscall-32",			/* 108	ILP32 system call on LP64 */
	"set-t0-addr",			/* 109	set trap0 address */
	BAD, BAD, BAD4,			/* 10A - 10F unused */
	BAD4, BAD4, BAD4, BAD4,		/* 110 - 11F unused (V9 user traps?) */
	"get-cc",			/* 120	get condition codes */
	"set-cc",			/* 121	set condition codes */
	"get-psr",			/* 122	get psr */
	"set-psr",			/* 123	set psr (some fields) */
	"getts",			/* 124	get timestamp */
	"gethrvtime",			/* 125	get lwp virtual time */
	"self-xcall",			/* 126	self xcall */
	"gethrtime",			/* 127	get hrestime */
	BAD4, BAD4,			/* 128 - 12F unused */
	"trace-0",			/* 130	trace, no data */
	"trace-1",			/* 131	trace, 1 data word */
	"trace-2",			/* 132	trace, 2 data words */
	"trace-3",			/* 133	trace, 3 data words */
	"trace-4",			/* 134	trace, 4 data words */
	"trace-5",			/* 135	trace, 5 data words */
	BAD,				/* 136	trace, unused */
	"trace-wr-buf",			/* 137	trace, atomic buf write */
	BAD4, BAD4,			/* 138 - 13F unused */
	"syscall-64",			/* 140  LP64 system call */
	BAD,				/* 141  unused */
	"tt-freeze",			/* 142  freeze traptrace */
	"tt-unfreeze",			/* 143  unfreeze traptrace */
	BAD4, BAD4, BAD4,		/* 144 - 14F unused */
	BAD4, BAD4, BAD4, BAD4,		/* 150 - 15F unused */
	BAD4, BAD4, BAD4, BAD4,		/* 160 - 16F unused */
	BAD4, BAD4, BAD4,		/* 170 - 17B unused */
	"ptl1-panic",			/* 17C	test ptl1_panic */
	"kadb-enter",			/* 17D	kadb enter (L1-A) */
	"kadb-brkpt",			/* 17E	kadb breakpoint */
	"obp-brkpt",			/* 17F	obp breakpoint */
	NOT4, NOT4, NOT4, NOT4,		/* 180 - 18F reserved */
	NOT4, NOT4, NOT4, NOT4,		/* 190 - 19F reserved */
	NOT4, NOT4, NOT4, NOT4,		/* 1A0 - 1AF reserved */
	NOT4, NOT4, NOT4, NOT4,		/* 1B0 - 1BF reserved */
	NOT4, NOT4, NOT4, NOT4,		/* 1C0 - 1CF reserved */
	NOT4, NOT4, NOT4, NOT4,		/* 1D0 - 1DF reserved */
	NOT4, NOT4, NOT4, NOT4,		/* 1E0 - 1EF reserved */
	NOT4, NOT4, NOT4, NOT4		/* 1F0 - 1FF reserved */
};
static const size_t ttndescr = sizeof (ttdescr) / sizeof (ttdescr[0]);

/*ARGSUSED*/
int
ttctl(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	TRAP_TRACE_CTL *ctls, *ctl;
	int i;
	int ncpu;

	if (argc != 0)
		return (DCMD_USAGE);

	if (mdb_readsym(&ncpu, sizeof (ncpu), "_ncpu") == -1) {
		mdb_warn("symbol '_ncpu' not found");
		return (DCMD_ERR);
	}
	ctls = mdb_alloc(sizeof (TRAP_TRACE_CTL) * ncpu, UM_SLEEP | UM_GC);
	if (mdb_readsym(ctls, sizeof (TRAP_TRACE_CTL) * ncpu,
	    "trap_trace_ctl") == -1) {
		mdb_warn("symbol 'trap_trace_ctl' not found");
		return (DCMD_ERR);
	}

	for (ctl = &ctls[0], i = 0; i < ncpu; i++, ctl++) {
		if (ctl->d.vaddr_base == 0)
			continue;

		mdb_printf("trap_trace_ctl[%d] = {\n", i);
		mdb_printf("  vaddr_base = 0x%lx\n", (long)ctl->d.vaddr_base);
		mdb_printf("  last_offset = 0x%x\n", ctl->d.last_offset);
		mdb_printf("  offset = 0x%x\n", ctl->d.offset);
		mdb_printf("  limit = 0x%x\n", ctl->d.limit);
		mdb_printf("  paddr_base = 0x%llx\n", ctl->d.paddr_base);
		mdb_printf("  asi = 0x%02x\n}\n", ctl->d.asi);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
ttprint_short(uintptr_t addr, const trap_trace_fullrec_t *full, int *cpu)
{
	const char *ttstr;
	const struct trap_trace_record *ttp = &full->ttf_rec;

	if (*cpu == -1)
		mdb_printf("%3d ", full->ttf_cpu);
	else
		if (*cpu != full->ttf_cpu)
			return (0);

	/*
	 * Decoding the traptype field is a bit messy.  First we check for
	 * several well-defined 16-bit values defined in <sys/traptrace.h>.
	 */
	switch (ttp->tt_tt) {
		case TT_SC_ENTR:
			ttstr = "sys-enter";
			break;
		case TT_SC_RET:
			ttstr = "sys-exit";
			break;
		case TT_SYS_RTT_PROM:
			ttstr = "prom_rtt";
			break;
		case TT_SYS_RTT_PRIV:
			ttstr = "priv_rtt";
			break;
		case TT_SYS_RTT_USER:
			ttstr = "user_rtt";
			break;
		case TT_INTR_EXIT:
			ttstr = "int-thr-exit";
			break;
		default:
			/*
			 * Next we consider several prefixes (which are
			 * typically OR'd with other information such as the
			 * %pil or %tt value at the time of the trace).
			 */
			switch (ttp->tt_tt & 0xff00) {
				case TT_SERVE_INTR:
					ttstr = "serve-intr";
					break;
				case TT_XCALL:
					ttstr = "xcall";
					break;
				case TT_XCALL_CONT:
					ttstr = "xcall-cont";
					break;
				case SYSTRAP_TT:
					ttstr = "sys_trap";
					break;
				default:
					/*
					 * Otherwise we try to convert the
					 * tt value to a string using our
					 * giant lookup table.
					 */
					ttstr = ttp->tt_tt < ttndescr ?
					    ttdescr[ttp->tt_tt] : "?";
			}
	}

	mdb_printf("%016llx %04hx %-12s %04hx %0?p %A\n", ttp->tt_tick,
	    ttp->tt_tt, ttstr, ttp->tt_tl, ttp->tt_tpc, ttp->tt_tpc);

	return (WALK_NEXT);
}

/*ARGSUSED*/
static int
ttprint_long(uintptr_t addr, const trap_trace_fullrec_t *full, int *cpu)
{
	const struct trap_trace_record *ttp = &full->ttf_rec;

	if (*cpu == -1)
		mdb_printf("%3d ", full->ttf_cpu);
	else if (*cpu != full->ttf_cpu)
		return (WALK_NEXT);

	mdb_printf("%016llx %016llx %04hx %04hx %0?p %0?p %0?p [%p,%p,%p,%p]\n",
	    ttp->tt_tick, ttp->tt_tstate, ttp->tt_tt, ttp->tt_tl,
	    ttp->tt_tpc, ttp->tt_sp, ttp->tt_tr,
	    ttp->tt_f1, ttp->tt_f2, ttp->tt_f3, ttp->tt_f4);

	return (WALK_NEXT);
}

typedef struct ttrace_cpu_data {
	struct trap_trace_record *tc_buf;
	struct trap_trace_record *tc_rec;
	struct trap_trace_record *tc_stop;
	size_t tc_bufsiz;
	uintptr_t tc_base;
} ttrace_cpu_data_t;

typedef struct ttrace_walk_data {
	int tw_ncpu;
	ttrace_cpu_data_t *tw_cpus;
} ttrace_walk_data_t;

int
ttrace_walk_init(mdb_walk_state_t *wsp)
{
	TRAP_TRACE_CTL *ctls, *ctl;
	int i;
	ttrace_walk_data_t *tw;
	ttrace_cpu_data_t *tc;
	struct trap_trace_record *buf;
	int ncpu;

	if (wsp->walk_addr != NULL) {
		mdb_warn("ttrace only supports global walks\n");
		return (WALK_ERR);
	}

	if (mdb_readsym(&ncpu, sizeof (ncpu), "_ncpu") == -1) {
		mdb_warn("symbol '_ncpu' not found");
		return (WALK_ERR);
	}

	ctls = mdb_alloc(sizeof (TRAP_TRACE_CTL) * ncpu, UM_SLEEP);
	if (mdb_readsym(ctls, sizeof (TRAP_TRACE_CTL) * ncpu,
	    "trap_trace_ctl") == -1) {
		mdb_warn("symbol 'trap_trace_ctl' not found");
		mdb_free(ctls, sizeof (TRAP_TRACE_CTL) * ncpu);
		return (WALK_ERR);
	}

	tw = mdb_zalloc(sizeof (ttrace_walk_data_t), UM_SLEEP);
	tw->tw_ncpu = ncpu;
	tw->tw_cpus = mdb_zalloc(sizeof (ttrace_cpu_data_t) * ncpu, UM_SLEEP);

	for (i = 0; i < ncpu; i++) {
		ctl = &ctls[i];

		if (ctl->d.vaddr_base == 0)
			continue;

		tc = &(tw->tw_cpus[i]);
		tc->tc_bufsiz = ctl->d.limit -
		    sizeof (struct trap_trace_record);
		tc->tc_buf = buf = mdb_alloc(tc->tc_bufsiz, UM_SLEEP);
		tc->tc_base = (uintptr_t)ctl->d.vaddr_base;

		if (mdb_vread(buf, tc->tc_bufsiz, tc->tc_base) == -1) {
			mdb_warn("failed to read trap trace buffer at %p",
			    ctl->d.vaddr_base);
			mdb_free(buf, tc->tc_bufsiz);
			tc->tc_buf = NULL;
		} else {
			tc->tc_rec = (struct trap_trace_record *)
			    ((uintptr_t)buf + (uintptr_t)ctl->d.last_offset);
			tc->tc_stop = (struct trap_trace_record *)
			    ((uintptr_t)buf + (uintptr_t)ctl->d.offset);
		}
	}

	mdb_free(ctls, sizeof (TRAP_TRACE_CTL) * ncpu);
	wsp->walk_data = tw;
	return (WALK_NEXT);
}

int
ttrace_walk_step(mdb_walk_state_t *wsp)
{
	ttrace_walk_data_t *tw = wsp->walk_data;
	ttrace_cpu_data_t *tc;
	struct trap_trace_record *rec;
	int oldest, i, status;
	uint64_t oldest_tick = 0;
	int done = 1;
	trap_trace_fullrec_t fullrec;

	for (i = 0; i < tw->tw_ncpu; i++) {
		tc = &(tw->tw_cpus[i]);

		if (tc->tc_rec == NULL)
			continue;
		done = 0;

		if (tc->tc_rec->tt_tick == 0)
			mdb_warn("Warning: tt_tick == 0\n");

		if (tc->tc_rec->tt_tick > oldest_tick) {
			oldest_tick = tc->tc_rec->tt_tick;
			oldest = i;
		}
	}

	if (done)
		return (-1);

	tc = &(tw->tw_cpus[oldest]);
	rec = tc->tc_rec;

	fullrec.ttf_rec = *rec;
	fullrec.ttf_cpu = oldest;

	if (oldest_tick != 0)
		status = wsp->walk_callback((uintptr_t)rec -
		    (uintptr_t)tc->tc_buf + tc->tc_base, &fullrec,
		    wsp->walk_cbdata);

	tc->tc_rec--;

	if (tc->tc_rec < tc->tc_buf)
		tc->tc_rec = (struct trap_trace_record *)((uintptr_t)
		    tc->tc_buf + (uintptr_t)tc->tc_bufsiz -
		    sizeof (struct trap_trace_record));

	if (tc->tc_rec == tc->tc_stop) {
		tc->tc_rec = NULL;
		mdb_free(tc->tc_buf, tc->tc_bufsiz);
	}

	return (status);
}

void
ttrace_walk_fini(mdb_walk_state_t *wsp)
{
	ttrace_walk_data_t *tw = wsp->walk_data;

	mdb_free(tw->tw_cpus, sizeof (ttrace_cpu_data_t) * tw->tw_ncpu);
	mdb_free(tw, sizeof (ttrace_walk_data_t));
}

int
ttrace(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	uint_t opt_x = FALSE;
	int cpu = -1;
	mdb_walk_cb_t ttprint;
	int ncpu;

	if (mdb_getopts(argc, argv,
	    'x', MDB_OPT_SETBITS, TRUE, &opt_x, NULL) != argc)
		return (DCMD_USAGE);

	if (flags & DCMD_ADDRSPEC) {
		if (mdb_readsym(&ncpu, sizeof (ncpu), "_ncpu") == -1) {
			mdb_warn("symbol '_ncpu' not found");
			return (DCMD_ERR);
		}
		if (addr >= ncpu) {
			mdb_warn("expected cpu between 0 and %d\n", ncpu - 1);
			return (DCMD_ERR);
		}
		cpu = (int)addr;
	}

	if (cpu == -1)
		mdb_printf("CPU ");

	if (opt_x) {
		mdb_printf("%-16s %-16s %-4s %-4s %-?s %-?s %-?s "
		    "F0-4\n", "%tick", "%tstate", "%tt", "%tl",
		    "%tpc", "%sp", "TR");

		ttprint = (mdb_walk_cb_t)ttprint_long;
	} else {
		mdb_printf("%-16s %-4s %-12s %-4s %s\n",
		    "%tick", "%tt", "", "%tl", "%tpc");

		ttprint = (mdb_walk_cb_t)ttprint_short;
	}

	if (mdb_walk("ttrace", ttprint, &cpu) == -1) {
		mdb_warn("couldn't walk ttrace");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

struct {
	int xc_type;
	const char *xc_str;
} xc_data[] = {
	{ XT_ONE_SELF,		"xt-one-self" },
	{ XT_ONE_OTHER,		"xt-one-other" },
	{ XT_SOME_SELF,		"xt-some-self" },
	{ XT_SOME_OTHER,	"xt-some-other" },
	{ XT_ALL_SELF,		"xt-all-self" },
	{ XT_ALL_OTHER,		"xt-all-other" },
	{ XC_ONE_SELF,		"xc-one-self" },
	{ XC_ONE_OTHER,		"xc-one-other" },
	{ XC_ONE_OTHER_H,	"xc-one-other-h" },
	{ XC_SOME_SELF,		"xc-some-self" },
	{ XC_SOME_OTHER,	"xc-some-other" },
	{ XC_SOME_OTHER_H,	"xc-some-other-h" },
	{ XC_ALL_SELF,		"xc-all-self" },
	{ XC_ALL_OTHER,		"xc-all-other" },
	{ XC_ALL_OTHER_H,	"xc-all-other-h" },
	{ XC_ATTENTION,		"xc-attention" },
	{ XC_DISMISSED,		"xc-dismissed" },
	{ XC_LOOP_ENTER,	"xc-loop-enter" },
	{ XC_LOOP_DOIT,		"xc-loop-doit" },
	{ XC_LOOP_EXIT,		"xc-loop-exit" },
	{ 0,			NULL }
};

/*ARGSUSED*/
int
xctrace_walk(uintptr_t addr, const trap_trace_fullrec_t *full, int *cpu)
{
	const struct trap_trace_record *ttp = &full->ttf_rec;
	int i, type = ttp->tt_tt & 0xff;
	const char *str = "???";

	if ((ttp->tt_tt & 0xff00) == TT_XCALL) {
		for (i = 0; xc_data[i].xc_str != NULL; i++) {
			if (xc_data[i].xc_type == type) {
				str = xc_data[i].xc_str;
				break;
			}
		}
	} else if ((ttp->tt_tt & 0xff00) == TT_XCALL_CONT) {
		str = "xcall-cont";
		mdb_printf("%3d %016llx %-16s %08x %08x %08x %08x\n",
		    full->ttf_cpu, ttp->tt_tick, str, ttp->tt_f1, ttp->tt_f2,
		    ttp->tt_f3, ttp->tt_f4);
		return (WALK_NEXT);
	} else if (ttp->tt_tt == 0x60) {
		str = "int-vec";
	} else {
		return (WALK_NEXT);
	}

	mdb_printf("%3d %016llx %-16s %08x %a\n", full->ttf_cpu,
	    ttp->tt_tick, str, ttp->tt_sp, ttp->tt_tr);

	return (WALK_NEXT);
}

/*ARGSUSED*/
int
xctrace(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (mdb_walk("ttrace", (mdb_walk_cb_t)xctrace_walk, NULL) == -1) {
		mdb_warn("couldn't walk ttrace");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

/*
 * Grrr... xc_mbox isn't in an _impl header file; we define it here.
 */
typedef struct xc_mbox {
	xcfunc_t *xc_func;
	uint64_t xc_arg1;
	uint64_t xc_arg2;
	cpuset_t xc_cpuset;
	volatile uint_t xc_state;
} xc_mbox_t;

typedef struct xc_mbox_walk {
	int xw_ndx;
	uintptr_t xw_addr;
	int xw_ncpu;
	xc_mbox_t *xw_mbox;
} xc_mbox_walk_t;

int
xc_mbox_walk_init(mdb_walk_state_t *wsp)
{
	GElf_Sym sym;
	xc_mbox_walk_t *xw;
	int ncpu;

	if (mdb_lookup_by_name("xc_mbox", &sym) == -1) {
		mdb_warn("couldn't find 'xc_mbox'");
		return (WALK_ERR);
	}

	if (mdb_readsym(&ncpu, sizeof (ncpu), "_ncpu") == -1) {
		mdb_warn("symbol '_ncpu' not found");
		return (WALK_ERR);
	}

	xw = mdb_zalloc(sizeof (xc_mbox_walk_t), UM_SLEEP);
	xw->xw_mbox = mdb_zalloc(sizeof (xc_mbox_t) * ncpu, UM_SLEEP);

	if (mdb_readsym(xw->xw_mbox, sizeof (xc_mbox_t) * ncpu, "xc_mbox") ==
	    -1) {
		mdb_warn("couldn't read 'xc_mbox'");
		mdb_free(xw->xw_mbox, sizeof (xc_mbox_t) * ncpu);
		mdb_free(xw, sizeof (xc_mbox_walk_t));
		return (WALK_ERR);
	}

	xw->xw_ncpu = ncpu;
	xw->xw_addr = sym.st_value;
	wsp->walk_data = xw;

	return (WALK_NEXT);
}

int
xc_mbox_walk_step(mdb_walk_state_t *wsp)
{
	xc_mbox_walk_t *xw = wsp->walk_data;
	int status;

	if (xw->xw_ndx == xw->xw_ncpu)
		return (WALK_DONE);

	status = wsp->walk_callback(xw->xw_addr,
	    &xw->xw_mbox[xw->xw_ndx++], wsp->walk_cbdata);

	xw->xw_addr += sizeof (xc_mbox_t);
	return (status);
}

void
xc_mbox_walk_fini(mdb_walk_state_t *wsp)
{
	xc_mbox_walk_t *xw = wsp->walk_data;

	mdb_free(xw->xw_mbox, sizeof (xc_mbox_t) * xw->xw_ncpu);
	mdb_free(xw, sizeof (xc_mbox_walk_t));
}

int
xc_mbox(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	xc_mbox_t mbox;
	GElf_Sym sym;
	int s;

	if (!(flags & DCMD_ADDRSPEC)) {
		if (mdb_walk_dcmd("xc_mbox", "xc_mbox", argc, argv) == -1) {
			mdb_warn("can't walk 'xc_mbox'");
			return (DCMD_ERR);
		}
		return (DCMD_OK);
	}

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%3s %-8s %9s %-8s %?s %s\n", "CPU", "ADDR",
		    "STATE", "CPUSET", "ARG1", "HNDLR");
	}

	if (mdb_vread(&mbox, sizeof (mbox), addr) == -1) {
		mdb_warn("couldn't read xc_mbox at %p", addr);
		return (DCMD_ERR);
	}

	if (mbox.xc_func == NULL)
		return (DCMD_OK);

	if (mdb_lookup_by_name("xc_mbox", &sym) == -1) {
		mdb_warn("couldn't read 'xc_mbox'");
		return (DCMD_ERR);
	}

	mdb_printf("%3d %08x %9s %08x %16llx %a\n", (int)((addr -
	    sym.st_value) / sizeof (mbox)), addr,
	    (s = mbox.xc_state) == XC_IDLE ? "XC_IDLE" :
	    s == XC_ENTER ? "XC_ENTER" : s == XC_WAIT ? "XC_WAIT" :
	    s == XC_DOIT ? "XC_DOIT" : s == XC_EXIT ? "XC_EXIT" : "XC_???",
	    mbox.xc_cpuset, mbox.xc_arg1, mbox.xc_func);

	return (DCMD_OK);
}

typedef struct softint_walk_data {
	struct intr_vector *sw_table;
	uintptr_t sw_base;
	size_t sw_ndx;
	size_t sw_size;
} softint_walk_data_t;

int
softint_walk_init(mdb_walk_state_t *wsp)
{
	GElf_Sym sym;
	softint_walk_data_t *sw;

	if (wsp->walk_addr != NULL) {
		mdb_warn("softint walk only supports global walks\n");
		return (WALK_ERR);
	}

	if (mdb_lookup_by_name("intr_vector", &sym) == -1) {
		mdb_warn("couldn't find intr_vector");
		return (WALK_ERR);
	}

	sw = mdb_zalloc(sizeof (softint_walk_data_t), UM_SLEEP);

	sw->sw_size = sym.st_size;
	sw->sw_base = (uintptr_t)sym.st_value;
	sw->sw_table = mdb_zalloc(sw->sw_size, UM_SLEEP);

	if (mdb_vread(sw->sw_table, sw->sw_size, sw->sw_base) == -1) {
		mdb_free(sw->sw_table, sw->sw_size);
		mdb_free(sw, sizeof (softint_walk_data_t));
	}

	wsp->walk_data = sw;
	return (WALK_NEXT);
}

int
softint_walk_step(mdb_walk_state_t *wsp)
{
	softint_walk_data_t *sw = (softint_walk_data_t *)wsp->walk_data;
	size_t max = sw->sw_size / sizeof (struct intr_vector);

	while (sw->sw_ndx < max) {
		struct intr_vector *iv = &(sw->sw_table[sw->sw_ndx++]);

		if (iv->iv_handler == NULL)
			continue;

		return (wsp->walk_callback((uintptr_t)iv -
		    (uintptr_t)sw->sw_table + sw->sw_base, iv,
		    wsp->walk_cbdata));
	}

	return (WALK_DONE);
}

void
softint_walk_fini(mdb_walk_state_t *wsp)
{
	softint_walk_data_t *sw = wsp->walk_data;

	mdb_free(sw->sw_table, sw->sw_size);
	mdb_free(sw, sizeof (softint_walk_data_t));
}

int
softint_dcmd(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct intr_vector iv;

	if (!(flags & DCMD_ADDRSPEC)) {
		if (mdb_walk_dcmd("softint", "softint", argc, argv) == -1) {
			mdb_warn("can't walk softint");
			return (DCMD_ERR);
		}
		return (DCMD_OK);
	}

	if (DCMD_HDRSPEC(flags)) {
		mdb_printf("%?s %4s %4s %?s %s\n",
		    "ADDR", "PEND", "PIL", "ARG", "HANDLER");
	}

	if (mdb_vread(&iv, sizeof (iv), addr) == -1) {
		mdb_warn("couldn't read intr_vector at %p", addr);
		return (DCMD_ERR);
	}

	mdb_printf("%0?p %4d %4d %?p %a\n", addr, iv.iv_pending,
	    iv.iv_pil, iv.iv_arg, iv.iv_handler);

	return (DCMD_OK);
}

static int
whatis_walk_tt(uintptr_t taddr, const trap_trace_fullrec_t *ttf, uintptr_t *ap)
{
	uintptr_t addr = *ap;

	if (addr < taddr || addr > taddr + sizeof (struct trap_trace_record))
		return (WALK_NEXT);

	mdb_printf("%p is %p+%p, trap trace record for cpu %d\n",
	    addr, taddr, addr - taddr, ttf->ttf_cpu);

	mdb_set_dot(1);
	return (WALK_DONE);
}

/*ARGSUSED*/
int
whatis(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	GElf_Sym sym;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb_lookup_by_name("trap_trace_ctl", &sym) == -1)
		return (DCMD_NEXT);

	mdb_set_dot(0);

	if (mdb_walk("ttrace", (mdb_walk_cb_t)whatis_walk_tt, &addr) == -1)
		mdb_warn("failed to walk 'ttrace'");

	return (DCMD_NEXT);
}

static const mdb_dcmd_t dcmds[] = {
	{ "ttctl", NULL, "dump trap trace ctl records", ttctl },
	{ "ttrace", "[-x]", "dump trap trace buffer for a cpu", ttrace },
	{ "xc_mbox", NULL, "dump xcall mboxes", xc_mbox },
	{ "xctrace", NULL, "dump xcall trace buffer", xctrace },
	{ "softint", NULL, "dumps softint table", softint_dcmd },
	{ "whatis", ":[-abv]", "given an address, return information", whatis },
	{ NULL }
};

static const mdb_walker_t walkers[] = {
	{ "ttrace", "walks the trap trace buffer for a CPU",
		ttrace_walk_init, ttrace_walk_step, ttrace_walk_fini },
	{ "xc_mbox", "walks the cross call mail boxes",
		xc_mbox_walk_init, xc_mbox_walk_step, xc_mbox_walk_fini },
	{ "softint", "walks the softint table",
		softint_walk_init, softint_walk_step, softint_walk_fini },
	{ NULL }
};

static const mdb_modinfo_t modinfo = { MDB_API_VERSION, dcmds, walkers };

const mdb_modinfo_t *
_mdb_init(void)
{
	return (&modinfo);
}
