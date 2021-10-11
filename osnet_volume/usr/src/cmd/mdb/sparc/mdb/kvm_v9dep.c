/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kvm_v9dep.c	1.1	99/08/11 SMI"

/*
 * Libkvm Kernel Target SPARC v9 component
 *
 * This file provides the ISA-dependent portion of the libkvm kernel target.
 * For more details on the implementation refer to mdb_kvm.c.  The SPARC v9
 * ISA code is actually compiled into *both* the sparcv7 and sparcv9 MDB
 * binaries because we need to deal with the sparcv9 CPU registers when
 * debugging a 32-bit crash dump from a kernel running on a sparcv9 CPU.
 */

#ifndef __sparcv9cpu
#define	__sparcv9cpu
#endif

#include <sys/types.h>
#include <sys/machtypes.h>
#include <sys/regset.h>
#include <sys/frame.h>
#include <sys/stack.h>
#include <sys/sysmacros.h>
#include <sys/panic.h>
#include <strings.h>

#include <mdb/mdb_target_impl.h>
#include <mdb/mdb_disasm.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_conf.h>
#include <mdb/mdb_kreg.h>
#include <mdb/mdb_kvm.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb.h>

#ifndef STACK_BIAS
#define	STACK_BIAS	0
#endif

/*
 * The mdb_tgt_gregset type is opaque to callers of the target interface
 * and to our own target common code.  We now can define it explicitly.
 */
struct mdb_tgt_gregset {
	kreg_t kregs[KREG_NGREG];
};

/*
 * We also define an array of register names and their corresponding
 * array indices.  This is used by the getareg and putareg entry points,
 * and also by our register variable discipline.
 */
static const mdb_tgt_regdesc_t kt_sparcv9_regs[] = {
	{ "g0", KREG_G0, MDB_TGT_R_EXPORT },
	{ "g1", KREG_G1, MDB_TGT_R_EXPORT },
	{ "g2", KREG_G2, MDB_TGT_R_EXPORT },
	{ "g3", KREG_G3, MDB_TGT_R_EXPORT },
	{ "g4", KREG_G4, MDB_TGT_R_EXPORT },
	{ "g5", KREG_G5, MDB_TGT_R_EXPORT },
	{ "g6", KREG_G6, MDB_TGT_R_EXPORT },
	{ "g7", KREG_G7, MDB_TGT_R_EXPORT },
	{ "o0", KREG_O0, MDB_TGT_R_EXPORT },
	{ "o1", KREG_O1, MDB_TGT_R_EXPORT },
	{ "o2", KREG_O2, MDB_TGT_R_EXPORT },
	{ "o3", KREG_O3, MDB_TGT_R_EXPORT },
	{ "o4", KREG_O4, MDB_TGT_R_EXPORT },
	{ "o5", KREG_O5, MDB_TGT_R_EXPORT },
	{ "o6", KREG_O6, MDB_TGT_R_EXPORT },
	{ "o7", KREG_O7, MDB_TGT_R_EXPORT },
	{ "l0", KREG_L0, MDB_TGT_R_EXPORT },
	{ "l1", KREG_L1, MDB_TGT_R_EXPORT },
	{ "l2", KREG_L2, MDB_TGT_R_EXPORT },
	{ "l3", KREG_L3, MDB_TGT_R_EXPORT },
	{ "l4", KREG_L4, MDB_TGT_R_EXPORT },
	{ "l5", KREG_L5, MDB_TGT_R_EXPORT },
	{ "l6", KREG_L6, MDB_TGT_R_EXPORT },
	{ "l7", KREG_L7, MDB_TGT_R_EXPORT },
	{ "i0", KREG_I0, MDB_TGT_R_EXPORT },
	{ "i1", KREG_I1, MDB_TGT_R_EXPORT },
	{ "i2", KREG_I2, MDB_TGT_R_EXPORT },
	{ "i3", KREG_I3, MDB_TGT_R_EXPORT },
	{ "i4", KREG_I4, MDB_TGT_R_EXPORT },
	{ "i5", KREG_I5, MDB_TGT_R_EXPORT },
	{ "i6", KREG_I6, MDB_TGT_R_EXPORT },
	{ "i7", KREG_I7, MDB_TGT_R_EXPORT },
	{ "ccr", KREG_CCR, MDB_TGT_R_EXPORT },
	{ "pc", KREG_PC, MDB_TGT_R_EXPORT },
	{ "npc", KREG_NPC, MDB_TGT_R_EXPORT },
	{ "y", KREG_Y, 0 },
	{ "asi", KREG_ASI, MDB_TGT_R_EXPORT },
	{ "fprs", KREG_FPRS, MDB_TGT_R_EXPORT },
	{ "tick", KREG_TICK, MDB_TGT_R_EXPORT },
	{ "pstate", KREG_PSTATE, MDB_TGT_R_PRIV | MDB_TGT_R_EXPORT },
	{ "tl", KREG_TL, MDB_TGT_R_PRIV | MDB_TGT_R_EXPORT },
	{ "pil", KREG_PIL, MDB_TGT_R_PRIV | MDB_TGT_R_EXPORT },
	{ "tba", KREG_TBA, MDB_TGT_R_PRIV | MDB_TGT_R_EXPORT },
	{ "ver", KREG_VER, MDB_TGT_R_PRIV | MDB_TGT_R_EXPORT },
	{ "cwp", KREG_CWP, MDB_TGT_R_PRIV | MDB_TGT_R_EXPORT },
	{ "cansave", KREG_CANSAVE, MDB_TGT_R_PRIV },
	{ "canrestore", KREG_CANRESTORE, MDB_TGT_R_PRIV },
	{ "otherwin", KREG_OTHERWIN, MDB_TGT_R_PRIV },
	{ "wstate", KREG_WSTATE, MDB_TGT_R_PRIV },
	{ "cleanwin", KREG_CLEANWIN, MDB_TGT_R_PRIV },
	{ "sp", KREG_SP, MDB_TGT_R_EXPORT | MDB_TGT_R_ALIAS },
	{ "fp", KREG_FP, MDB_TGT_R_EXPORT | MDB_TGT_R_ALIAS },
	{ NULL, 0, 0 }
};

static int
kt_getareg(mdb_tgt_t *t, mdb_tgt_tid_t tid,
    const char *rname, mdb_tgt_reg_t *rp)
{
	const mdb_tgt_regdesc_t *rdp;
	kt_data_t *kt = t->t_data;

	if (tid != kt->k_tid)
		return (set_errno(EMDB_NOREGS));

	for (rdp = kt->k_rds; rdp->rd_name != NULL; rdp++) {
		if (strcmp(rname, rdp->rd_name) == 0) {
			*rp = kt->k_regs->kregs[rdp->rd_num];
			return (0);
		}
	}

	return (set_errno(EMDB_BADREG));
}

static int
kt_putareg(mdb_tgt_t *t, mdb_tgt_tid_t tid, const char *rname, mdb_tgt_reg_t r)
{
	const mdb_tgt_regdesc_t *rdp;
	kt_data_t *kt = t->t_data;

	if (tid != kt->k_tid)
		return (set_errno(EMDB_NOREGS));

	for (rdp = kt->k_rds; rdp->rd_name != NULL; rdp++) {
		if (strcmp(rname, rdp->rd_name) == 0) {
			kt->k_regs->kregs[rdp->rd_num] = r;
			return (0);
		}
	}

	return (set_errno(EMDB_BADREG));
}

static int
kt_stack_iter(mdb_tgt_t *t, const mdb_tgt_gregset_t *gsp,
    mdb_tgt_stack_f *func, void *arg)
{
	mdb_tgt_gregset_t gregs;
	kreg_t *kregs = &gregs.kregs[0];
	int got_pc = (gsp->kregs[KREG_PC] != 0);

	struct rwindow rwin;
	uintptr_t sp;
	long argv[6];
	int i;

	bcopy(gsp, &gregs, sizeof (gregs));

	for (;;) {
		for (i = 0; i < 6; i++)
			argv[i] = kregs[KREG_I0 + i];

		if (got_pc && func(arg, kregs[KREG_PC], 6, argv, &gregs) != 0)
			break;

		kregs[KREG_PC] = kregs[KREG_I7];
		kregs[KREG_NPC] = kregs[KREG_PC] + 4;

		bcopy(&kregs[KREG_I0], &kregs[KREG_O0], 8 * sizeof (kreg_t));
		got_pc |= (kregs[KREG_PC] != 0);

		if ((sp = kregs[KREG_FP] + STACK_BIAS) == STACK_BIAS || sp == 0)
			break; /* Stop if we're at the end of the stack */

		if (sp & (STACK_ALIGN - 1))
			return (set_errno(EMDB_STKALIGN));

		if (mdb_tgt_vread(t, &rwin, sizeof (rwin), sp) != sizeof (rwin))
			return (-1); /* Failed to read frame */

		for (i = 0; i < 8; i++)
			kregs[KREG_L0 + i] = (uintptr_t)rwin.rw_local[i];
		for (i = 0; i < 8; i++)
			kregs[KREG_I0 + i] = (uintptr_t)rwin.rw_in[i];
	}

	return (0);
}

static const char *
pstate_mm_to_str(kreg_t pstate)
{
	if (KREG_PSTATE_MM_TSO(pstate))
		return ("TSO");

	if (KREG_PSTATE_MM_PSO(pstate))
		return ("PSO");

	if (KREG_PSTATE_MM_RMO(pstate))
		return ("RMO");

	return ("???");
}

/*ARGSUSED*/
static int
kt_regs(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kt_data_t *kt = mdb.m_target->t_data;
	const kreg_t *kregs = &kt->k_regs->kregs[0];

	if (argc != 0 || (flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

#define	GETREG2(x) ((uintptr_t)kregs[(x)]), ((uintptr_t)kregs[(x)])

	mdb_printf("%%g0 = 0x%0?p %15A %%l0 = 0x%0?p %A\n",
	    GETREG2(KREG_G0), GETREG2(KREG_L0));

	mdb_printf("%%g1 = 0x%0?p %15A %%l1 = 0x%0?p %A\n",
	    GETREG2(KREG_G1), GETREG2(KREG_L1));

	mdb_printf("%%g2 = 0x%0?p %15A %%l2 = 0x%0?p %A\n",
	    GETREG2(KREG_G2), GETREG2(KREG_L2));

	mdb_printf("%%g3 = 0x%0?p %15A %%l3 = 0x%0?p %A\n",
	    GETREG2(KREG_G3), GETREG2(KREG_L3));

	mdb_printf("%%g4 = 0x%0?p %15A %%l4 = 0x%0?p %A\n",
	    GETREG2(KREG_G4), GETREG2(KREG_L4));

	mdb_printf("%%g5 = 0x%0?p %15A %%l5 = 0x%0?p %A\n",
	    GETREG2(KREG_G5), GETREG2(KREG_L5));

	mdb_printf("%%g6 = 0x%0?p %15A %%l6 = 0x%0?p %A\n",
	    GETREG2(KREG_G6), GETREG2(KREG_L6));

	mdb_printf("%%g7 = 0x%0?p %15A %%l7 = 0x%0?p %A\n\n",
	    GETREG2(KREG_G7), GETREG2(KREG_L7));

	mdb_printf("%%o0 = 0x%0?p %15A %%i0 = 0x%0?p %A\n",
	    GETREG2(KREG_O0), GETREG2(KREG_I0));

	mdb_printf("%%o1 = 0x%0?p %15A %%i1 = 0x%0?p %A\n",
	    GETREG2(KREG_O1), GETREG2(KREG_I1));

	mdb_printf("%%o2 = 0x%0?p %15A %%i2 = 0x%0?p %A\n",
	    GETREG2(KREG_O2), GETREG2(KREG_I2));

	mdb_printf("%%o3 = 0x%0?p %15A %%i3 = 0x%0?p %A\n",
	    GETREG2(KREG_O3), GETREG2(KREG_I3));

	mdb_printf("%%o4 = 0x%0?p %15A %%i4 = 0x%0?p %A\n",
	    GETREG2(KREG_O4), GETREG2(KREG_I4));

	mdb_printf("%%o5 = 0x%0?p %15A %%i5 = 0x%0?p %A\n",
	    GETREG2(KREG_O5), GETREG2(KREG_I5));

	mdb_printf("%%o6 = 0x%0?p %15A %%i6 = 0x%0?p %A\n",
	    GETREG2(KREG_O6), GETREG2(KREG_I6));

	mdb_printf("%%o7 = 0x%0?p %15A %%i7 = 0x%0?p %A\n\n",
	    GETREG2(KREG_O7), GETREG2(KREG_I7));

	mdb_printf(" %%ccr = 0x%02llx "
	    "xcc=%c%c%c%c icc=%c%c%c%c\n", kregs[KREG_CCR],
	    (kregs[KREG_CCR] & KREG_CCR_XCC_N_MASK) ? 'N' : 'n',
	    (kregs[KREG_CCR] & KREG_CCR_XCC_Z_MASK) ? 'Z' : 'z',
	    (kregs[KREG_CCR] & KREG_CCR_XCC_V_MASK) ? 'V' : 'v',
	    (kregs[KREG_CCR] & KREG_CCR_XCC_C_MASK) ? 'C' : 'c',
	    (kregs[KREG_CCR] & KREG_CCR_ICC_N_MASK) ? 'N' : 'n',
	    (kregs[KREG_CCR] & KREG_CCR_ICC_Z_MASK) ? 'Z' : 'z',
	    (kregs[KREG_CCR] & KREG_CCR_ICC_V_MASK) ? 'V' : 'v',
	    (kregs[KREG_CCR] & KREG_CCR_ICC_C_MASK) ? 'C' : 'c');

	mdb_printf("%%fprs = 0x%02llx "
	    "fef=%llu du=%llu dl=%llu\n", kregs[KREG_FPRS],
	    (kregs[KREG_FPRS] & KREG_FPRS_FEF_MASK) >> KREG_FPRS_FEF_SHIFT,
	    (kregs[KREG_FPRS] & KREG_FPRS_DU_MASK) >> KREG_FPRS_DU_SHIFT,
	    (kregs[KREG_FPRS] & KREG_FPRS_DL_MASK) >> KREG_FPRS_DL_SHIFT);

	mdb_printf(" %%asi = 0x%02llx\n", kregs[KREG_ASI]);
	mdb_printf("   %%y = 0x%0?p\n", (uintptr_t)kregs[KREG_Y]);

	mdb_printf("  %%pc = 0x%0?p %A\n", GETREG2(KREG_PC));
	mdb_printf(" %%npc = 0x%0?p %A\n", GETREG2(KREG_NPC));

#if STACK_BIAS != 0
	mdb_printf("  %%sp = 0x%0?p unbiased=0x%0?p\n",
	    (uintptr_t)kregs[KREG_SP], (uintptr_t)kregs[KREG_SP] + STACK_BIAS);
#else
	mdb_printf("  %%sp = 0x%0?p\n", (uintptr_t)kregs[KREG_SP]);
#endif
	mdb_printf("  %%fp = 0x%0?p\n\n", (uintptr_t)kregs[KREG_FP]);

	mdb_printf("  %%tick = 0x%016llx\n", kregs[KREG_TICK]);
	mdb_printf("   %%tba = 0x%016llx\n", kregs[KREG_TBA]);
	mdb_printf("    %%tl = 0x%01llx\n", kregs[KREG_TL]);
	mdb_printf("   %%pil = 0x%01llx\n", kregs[KREG_PIL]);

	mdb_printf("%%pstate = 0x%03llx cle=%llu tle=%llu mm=%s"
	    " red=%llu pef=%llu am=%llu priv=%llu ie=%llu ag=%llu\n\n",
	    kregs[KREG_PSTATE],
	    (kregs[KREG_PSTATE] & KREG_PSTATE_CLE_MASK) >>
	    KREG_PSTATE_CLE_SHIFT,
	    (kregs[KREG_PSTATE] & KREG_PSTATE_TLE_MASK) >>
	    KREG_PSTATE_TLE_SHIFT, pstate_mm_to_str(kregs[KREG_PSTATE]),
	    (kregs[KREG_PSTATE] & KREG_PSTATE_RED_MASK) >>
	    KREG_PSTATE_RED_SHIFT,
	    (kregs[KREG_PSTATE] & KREG_PSTATE_PEF_MASK) >>
	    KREG_PSTATE_PEF_SHIFT,
	    (kregs[KREG_PSTATE] & KREG_PSTATE_AM_MASK) >> KREG_PSTATE_AM_SHIFT,
	    (kregs[KREG_PSTATE] & KREG_PSTATE_PRIV_MASK) >>
	    KREG_PSTATE_PRIV_SHIFT,
	    (kregs[KREG_PSTATE] & KREG_PSTATE_IE_MASK) >> KREG_PSTATE_IE_SHIFT,
	    (kregs[KREG_PSTATE] & KREG_PSTATE_AG_MASK) >> KREG_PSTATE_AG_SHIFT);

	mdb_printf("       %%cwp = 0x%02llx  %%cansave = 0x%02llx\n",
	    kregs[KREG_CWP], kregs[KREG_CANSAVE]);

	mdb_printf("%%canrestore = 0x%02llx %%otherwin = 0x%02llx\n",
	    kregs[KREG_CANRESTORE], kregs[KREG_OTHERWIN]);

	mdb_printf("    %%wstate = 0x%02llx %%cleanwin = 0x%02llx\n",
	    kregs[KREG_WSTATE], kregs[KREG_CLEANWIN]);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
kt_frame(void *arglim, uintptr_t pc, uint_t argc, const long *argv,
    const mdb_tgt_gregset_t *gregs)
{
	argc = MIN(argc, (uint_t)(uintptr_t)arglim);
	mdb_printf("%a(", pc);

	if (argc != 0) {
		mdb_printf("%lr", *argv++);
		for (argc--; argc != 0; argc--)
			mdb_printf(", %lr", *argv++);
	}

	mdb_printf(")\n");
	return (0);
}

static int
kt_framev(void *arglim, uintptr_t pc, uint_t argc, const long *argv,
    const mdb_tgt_gregset_t *gregs)
{
	argc = MIN(argc, (uint_t)(uintptr_t)arglim);
	mdb_printf("%0?llr %a(", gregs->kregs[KREG_SP], pc);

	if (argc != 0) {
		mdb_printf("%lr", *argv++);
		for (argc--; argc != 0; argc--)
			mdb_printf(", %lr", *argv++);
	}

	mdb_printf(")\n");
	return (0);
}

static int
kt_stack_common(uintptr_t addr, uint_t flags, int argc,
    const mdb_arg_t *argv, mdb_tgt_stack_f *func)
{
	kt_data_t *kt = mdb.m_target->t_data;
	void *arg = (void *)mdb.m_nargs;
	mdb_tgt_gregset_t gregs, *grp;

	if (flags & DCMD_ADDRSPEC) {
		bzero(&gregs, sizeof (gregs));
		gregs.kregs[KREG_FP] = addr;
		grp = &gregs;
	} else
		grp = kt->k_regs;

	if (argc != 0) {
		if (argv->a_type == MDB_TYPE_CHAR || argc > 1)
			return (DCMD_USAGE);

		if (argv->a_type == MDB_TYPE_STRING)
			arg = (void *)(uint_t)mdb_strtoull(argv->a_un.a_str);
		else
			arg = (void *)(uint_t)argv->a_un.a_val;
	}

	(void) kt_stack_iter(mdb.m_target, grp, func, arg);
	return (DCMD_OK);
}

static int
kt_stack(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (kt_stack_common(addr, flags, argc, argv, kt_frame));
}

static int
kt_stackv(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (kt_stack_common(addr, flags, argc, argv, kt_framev));
}

const mdb_tgt_ops_t kt_sparcv9_ops = {
	kt_setflags,				/* t_setflags */
	kt_setcontext,				/* t_setcontext */
	kt_activate,				/* t_activate */
	kt_deactivate,				/* t_deactivate */
	kt_destroy,				/* t_destroy */
	kt_name,				/* t_name */
	(const char *(*)()) mdb_conf_isa,	/* t_isa */
	kt_platform,				/* t_platform */
	kt_uname,				/* t_uname */
	kt_aread,				/* t_aread */
	kt_awrite,				/* t_awrite */
	kt_vread,				/* t_vread */
	kt_vwrite,				/* t_vwrite */
	kt_pread,				/* t_pread */
	kt_pwrite,				/* t_pwrite */
	kt_fread,				/* t_fread */
	kt_fwrite,				/* t_fwrite */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_ioread */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_iowrite */
	kt_vtop,				/* t_vtop */
	kt_lookup_by_name,			/* t_lookup_by_name */
	kt_lookup_by_addr,			/* t_lookup_by_addr */
	kt_symbol_iter,				/* t_symbol_iter */
	kt_mapping_iter,			/* t_mapping_iter */
	kt_object_iter,				/* t_object_iter */
	(const mdb_map_t *(*)()) mdb_tgt_null,	/* t_addr_to_map XXX */
	(const mdb_map_t *(*)()) mdb_tgt_null,	/* t_name_to_map XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_thread_iter XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_cpu_iter XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_thr_status XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_cpu_status XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_status XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_run */
	(int (*)()) mdb_tgt_notsup,		/* t_step */
	(int (*)()) mdb_tgt_notsup,		/* t_continue */
	(int (*)()) mdb_tgt_notsup,		/* t_call */
	(int (*)()) mdb_tgt_notsup,		/* t_add_brkpt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_pwapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_vwapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_iowapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_ixwapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_sysenter */
	(int (*)()) mdb_tgt_notsup,		/* t_add_sysexit */
	(int (*)()) mdb_tgt_notsup,		/* t_add_signal */
	(int (*)()) mdb_tgt_notsup,		/* t_add_object_load */
	(int (*)()) mdb_tgt_notsup,		/* t_add_object_unload */
	kt_getareg,				/* t_getareg */
	kt_putareg,				/* t_putareg */
	kt_stack_iter,				/* t_stack_iter */
};

void
kt_sparcv9_init(mdb_tgt_t *t)
{
	kt_data_t *kt = t->t_data;

	struct rwindow rwin;
	panic_data_t pd;
	label_t label;
	kreg_t *kregs;

	uint64_t tick;
	uint32_t pil;

	/*
	 * Initialize the machine-dependent parts of the kernel target
	 * structure.  Once this is complete and we fill in the ops
	 * vector, the target is now fully constructed and we can use
	 * the target API itself to perform the rest of our initialization.
	 */
	kt->k_rds = kt_sparcv9_regs;
	kt->k_regs = mdb_zalloc(sizeof (mdb_tgt_gregset_t), UM_SLEEP);
	kt->k_regsize = sizeof (mdb_tgt_gregset_t);
	kt->k_dcmd_regs = kt_regs;
	kt->k_dcmd_stack = kt_stack;
	kt->k_dcmd_stackv = kt_stackv;

	t->t_ops = &kt_sparcv9_ops;
	kregs = kt->k_regs->kregs;

	(void) mdb_dis_select("v9plus");

	/*
	 * Don't attempt to load any thread or register information if
	 * we're examining the live operating system.
	 */
	if (strcmp(kt->k_symfile, "/dev/ksyms") == 0)
		return;

	/*
	 * If the panicbuf symbol is present and we can consume a panicbuf
	 * header of the appropriate version from this address, then
	 * we can initialize our current register set based on its contents:
	 */
	if (mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &pd, sizeof (pd),
	    MDB_TGT_OBJ_EXEC, "panicbuf") == sizeof (pd) &&
	    pd.pd_version == PANICBUFVERS) {

		size_t pd_size = MIN(PANICBUFSIZE, pd.pd_msgoff);
		panic_data_t *pdp = mdb_zalloc(pd_size, UM_SLEEP);
		uint_t i, n;

		(void) mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, pdp, pd_size,
		    MDB_TGT_OBJ_EXEC, "panicbuf");

		n = (pd_size - (sizeof (panic_data_t) -
		    sizeof (panic_nv_t))) / sizeof (panic_nv_t);

		for (i = 0; i < n; i++) {
			const char *name = pdp->pd_nvdata[i].pnv_name;
			uint64_t value = pdp->pd_nvdata[i].pnv_value;

			if (strcmp(name, "tstate") == 0) {
				kregs[KREG_CCR] = KREG_TSTATE_CCR(value);
				kregs[KREG_ASI] = KREG_TSTATE_ASI(value);
				kregs[KREG_PSTATE] = KREG_TSTATE_PSTATE(value);
				kregs[KREG_CWP] = KREG_TSTATE_CWP(value);
			} else
				(void) kt_putareg(t, kt->k_tid, name, value);
		}

		mdb_free(pdp, pd_size);
	}

	/*
	 * Prior to the re-structuring of panicbuf, our only register data
	 * was the panic_regs label_t, into which a setjmp() was performed.
	 */
	if (kregs[KREG_PC] == 0 && kregs[KREG_SP] == 0 &&
	    mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &label, sizeof (label),
	    MDB_TGT_OBJ_EXEC, "panic_regs") == sizeof (label)) {

		kregs[KREG_PC] = label.val[0];
		kregs[KREG_SP] = label.val[1];
	}

	/*
	 * If we can read a saved register window from the stack at %sp,
	 * we can also fill in the locals and inputs.
	 */
	if (kregs[KREG_SP] != 0 && mdb_tgt_vread(t, &rwin, sizeof (rwin),
	    kregs[KREG_SP] + STACK_BIAS) == sizeof (rwin)) {

		kregs[KREG_L0] = rwin.rw_local[0];
		kregs[KREG_L1] = rwin.rw_local[1];
		kregs[KREG_L2] = rwin.rw_local[2];
		kregs[KREG_L3] = rwin.rw_local[3];
		kregs[KREG_L4] = rwin.rw_local[4];
		kregs[KREG_L5] = rwin.rw_local[5];
		kregs[KREG_L6] = rwin.rw_local[6];
		kregs[KREG_L7] = rwin.rw_local[7];

		kregs[KREG_I0] = rwin.rw_in[0];
		kregs[KREG_I1] = rwin.rw_in[1];
		kregs[KREG_I2] = rwin.rw_in[2];
		kregs[KREG_I3] = rwin.rw_in[3];
		kregs[KREG_I4] = rwin.rw_in[4];
		kregs[KREG_I5] = rwin.rw_in[5];
		kregs[KREG_I6] = rwin.rw_in[6];
		kregs[KREG_I7] = rwin.rw_in[7];

	} else if (kregs[KREG_SP] != 0) {
		warn("failed to read rwindow at %p -- current "
		    "frame inputs will be unavailable\n",
		    (void *)(kregs[KREG_SP] + STACK_BIAS));
	}

	/*
	 * The panic_ipl variable records the IPL of the panic CPU,
	 * which on sparcv9 is the %pil register's value.
	 */
	if (mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &pil, sizeof (pil),
	    MDB_TGT_OBJ_EXEC, "panic_ipl") == sizeof (pil))
		kregs[KREG_PIL] = pil;

	/*
	 * The panic_tick variable records %tick at the approximate
	 * time of the panic in a DEBUG kernel.
	 */
	if (mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &tick, sizeof (tick),
	    MDB_TGT_OBJ_EXEC, "panic_tick") == sizeof (tick))
		kregs[KREG_TICK] = tick;
}
