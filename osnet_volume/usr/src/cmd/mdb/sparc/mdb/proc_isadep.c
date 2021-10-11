/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_isadep.c	1.1	99/08/11 SMI"

/*
 * User Process Target SPARC v7 and v9 component
 *
 * This file provides the ISA-dependent portion of the user process target
 * for both the sparcv7 and sparcv9 ISAs.  For more details on the
 * implementation refer to mdb_proc.c.
 */

#ifdef __sparcv9
#define	__sparcv9cpu
#endif

#include <mdb/mdb_target_impl.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_kreg.h>
#include <mdb/mdb.h>

#include <sys/elf_SPARC.h>
#include <libproc.h>

const mdb_tgt_regdesc_t pt_regdesc[] = {
	{ "g0", R_G0, MDB_TGT_R_EXPORT },
	{ "g1", R_G1, MDB_TGT_R_EXPORT },
	{ "g2", R_G2, MDB_TGT_R_EXPORT },
	{ "g3", R_G3, MDB_TGT_R_EXPORT },
	{ "g4", R_G4, MDB_TGT_R_EXPORT },
	{ "g5", R_G5, MDB_TGT_R_EXPORT },
	{ "g6", R_G6, MDB_TGT_R_EXPORT },
	{ "g7", R_G7, MDB_TGT_R_EXPORT },
	{ "o0", R_O0, MDB_TGT_R_EXPORT },
	{ "o1", R_O1, MDB_TGT_R_EXPORT },
	{ "o2", R_O2, MDB_TGT_R_EXPORT },
	{ "o3", R_O3, MDB_TGT_R_EXPORT },
	{ "o4", R_O4, MDB_TGT_R_EXPORT },
	{ "o5", R_O5, MDB_TGT_R_EXPORT },
	{ "o6", R_O6, MDB_TGT_R_EXPORT },
	{ "o7", R_O7, MDB_TGT_R_EXPORT },
	{ "l0", R_L0, MDB_TGT_R_EXPORT },
	{ "l1", R_L1, MDB_TGT_R_EXPORT },
	{ "l2", R_L2, MDB_TGT_R_EXPORT },
	{ "l3", R_L3, MDB_TGT_R_EXPORT },
	{ "l4", R_L4, MDB_TGT_R_EXPORT },
	{ "l5", R_L5, MDB_TGT_R_EXPORT },
	{ "l6", R_L6, MDB_TGT_R_EXPORT },
	{ "l7", R_L7, MDB_TGT_R_EXPORT },
	{ "i0", R_I0, MDB_TGT_R_EXPORT },
	{ "i1", R_I1, MDB_TGT_R_EXPORT },
	{ "i2", R_I2, MDB_TGT_R_EXPORT },
	{ "i3", R_I3, MDB_TGT_R_EXPORT },
	{ "i4", R_I4, MDB_TGT_R_EXPORT },
	{ "i5", R_I5, MDB_TGT_R_EXPORT },
	{ "i6", R_I6, MDB_TGT_R_EXPORT },
	{ "i7", R_I7, MDB_TGT_R_EXPORT },
#ifdef __sparcv9
	{ "ccr", R_CCR, MDB_TGT_R_EXPORT },
#else
	{ "psr", R_PSR, MDB_TGT_R_EXPORT },
#endif
	{ "pc", R_PC, MDB_TGT_R_EXPORT },
	{ "npc", R_nPC, MDB_TGT_R_EXPORT },
	{ "y", R_Y, 0 },
#ifdef __sparcv9
	{ "asi", R_ASI, MDB_TGT_R_EXPORT },
	{ "fprs", R_FPRS, MDB_TGT_R_EXPORT },
#else
	{ "wim", R_WIM, MDB_TGT_R_EXPORT | MDB_TGT_R_PRIV },
	{ "tbr", R_TBR, MDB_TGT_R_EXPORT | MDB_TGT_R_PRIV },
#endif
	{ "sp", R_SP, MDB_TGT_R_EXPORT | MDB_TGT_R_ALIAS },
	{ "fp", R_FP, MDB_TGT_R_EXPORT | MDB_TGT_R_ALIAS },
	{ NULL, 0, 0 }
};

/*ARGSUSED*/
int
pt_regs(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	prgregset_t grs;

#define	GETREG2(x) ((uintptr_t)grs[(x)]), ((uintptr_t)grs[(x)])

	if (argc != 0 || (flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (mdb.m_target->t_pshandle == NULL) {
		mdb_warn("no process active\n");
		return (DCMD_ERR);
	}

	if (Plwp_getregs(mdb.m_target->t_pshandle,
	    Pstatus(mdb.m_target->t_pshandle)->pr_lwp.pr_lwpid, grs) != 0) {
		mdb_warn("failed to get current register set");
		return (DCMD_ERR);
	}

	mdb_printf("%%g0 = 0x%0?p %15A %%l0 = 0x%0?p %A\n",
	    GETREG2(R_G0), GETREG2(R_L0));

	mdb_printf("%%g1 = 0x%0?p %15A %%l1 = 0x%0?p %A\n",
	    GETREG2(R_G1), GETREG2(R_L1));

	mdb_printf("%%g2 = 0x%0?p %15A %%l2 = 0x%0?p %A\n",
	    GETREG2(R_G2), GETREG2(R_L2));

	mdb_printf("%%g3 = 0x%0?p %15A %%l3 = 0x%0?p %A\n",
	    GETREG2(R_G3), GETREG2(R_L3));

	mdb_printf("%%g4 = 0x%0?p %15A %%l4 = 0x%0?p %A\n",
	    GETREG2(R_G4), GETREG2(R_L4));

	mdb_printf("%%g5 = 0x%0?p %15A %%l5 = 0x%0?p %A\n",
	    GETREG2(R_G5), GETREG2(R_L5));

	mdb_printf("%%g6 = 0x%0?p %15A %%l6 = 0x%0?p %A\n",
	    GETREG2(R_G6), GETREG2(R_L6));

	mdb_printf("%%g7 = 0x%0?p %15A %%l7 = 0x%0?p %A\n\n",
	    GETREG2(R_G7), GETREG2(R_L7));

	mdb_printf("%%o0 = 0x%0?p %15A %%i0 = 0x%0?p %A\n",
	    GETREG2(R_O0), GETREG2(R_I0));

	mdb_printf("%%o1 = 0x%0?p %15A %%i1 = 0x%0?p %A\n",
	    GETREG2(R_O1), GETREG2(R_I1));

	mdb_printf("%%o2 = 0x%0?p %15A %%i2 = 0x%0?p %A\n",
	    GETREG2(R_O2), GETREG2(R_I2));

	mdb_printf("%%o3 = 0x%0?p %15A %%i3 = 0x%0?p %A\n",
	    GETREG2(R_O3), GETREG2(R_I3));

	mdb_printf("%%o4 = 0x%0?p %15A %%i4 = 0x%0?p %A\n",
	    GETREG2(R_O4), GETREG2(R_I4));

	mdb_printf("%%o5 = 0x%0?p %15A %%i5 = 0x%0?p %A\n",
	    GETREG2(R_O5), GETREG2(R_I5));

	mdb_printf("%%o6 = 0x%0?p %15A %%i6 = 0x%0?p %A\n",
	    GETREG2(R_O6), GETREG2(R_I6));

	mdb_printf("%%o7 = 0x%0?p %15A %%i7 = 0x%0?p %A\n\n",
	    GETREG2(R_O7), GETREG2(R_I7));

#ifdef __sparcv9
	mdb_printf(" %%ccr = 0x%02x xcc=%c%c%c%c icc=%c%c%c%c\n", grs[R_CCR],
	    (grs[R_CCR] & KREG_CCR_XCC_N_MASK) ? 'N' : 'n',
	    (grs[R_CCR] & KREG_CCR_XCC_Z_MASK) ? 'Z' : 'z',
	    (grs[R_CCR] & KREG_CCR_XCC_V_MASK) ? 'V' : 'v',
	    (grs[R_CCR] & KREG_CCR_XCC_C_MASK) ? 'C' : 'c',
	    (grs[R_CCR] & KREG_CCR_ICC_N_MASK) ? 'N' : 'n',
	    (grs[R_CCR] & KREG_CCR_ICC_Z_MASK) ? 'Z' : 'z',
	    (grs[R_CCR] & KREG_CCR_ICC_V_MASK) ? 'V' : 'v',
	    (grs[R_CCR] & KREG_CCR_ICC_C_MASK) ? 'C' : 'c');
#else	/* __sparcv9 */
	mdb_printf(" %%psr = 0x%08x impl=0x%x ver=0x%x icc=%c%c%c%c\n"
	    "                   ec=%u ef=%u pil=%u s=%u ps=%u et=%u cwp=0x%x\n",
	    grs[R_PSR],
	    (grs[R_PSR] & KREG_PSR_IMPL_MASK) >> KREG_PSR_IMPL_SHIFT,
	    (grs[R_PSR] & KREG_PSR_VER_MASK) >> KREG_PSR_VER_SHIFT,
	    (grs[R_PSR] & KREG_PSR_ICC_N_MASK) ? 'N' : 'n',
	    (grs[R_PSR] & KREG_PSR_ICC_Z_MASK) ? 'Z' : 'z',
	    (grs[R_PSR] & KREG_PSR_ICC_V_MASK) ? 'V' : 'v',
	    (grs[R_PSR] & KREG_PSR_ICC_C_MASK) ? 'C' : 'c',
	    grs[R_PSR] & KREG_PSR_EC_MASK, grs[R_PSR] & KREG_PSR_EF_MASK,
	    (grs[R_PSR] & KREG_PSR_PIL_MASK) >> KREG_PSR_PIL_SHIFT,
	    grs[R_PSR] & KREG_PSR_S_MASK, grs[R_PSR] & KREG_PSR_PS_MASK,
	    grs[R_PSR] & KREG_PSR_ET_MASK,
	    (grs[R_PSR] & KREG_PSR_CWP_MASK) >> KREG_PSR_CWP_SHIFT);
#endif	/* __sparcv9 */

	mdb_printf("   %%y = 0x%0?p\n", grs[R_Y]);

	mdb_printf("  %%pc = 0x%0?p %A\n", GETREG2(R_PC));
	mdb_printf(" %%npc = 0x%0?p %A\n", GETREG2(R_nPC));

	mdb_printf("  %%sp = 0x%0?p\n", grs[R_SP]);
	mdb_printf("  %%fp = 0x%0?p\n\n", grs[R_FP]);

#ifdef __sparcv9
	mdb_printf(" %%asi = 0x%02lx\n", grs[R_ASI]);
	mdb_printf("%%fprs = 0x%02lx\n", grs[R_FPRS]);
#else	/* __sparcv9 */
	mdb_printf(" %%wim = 0x%08x\n", grs[R_WIM]);
	mdb_printf(" %%tbr = 0x%08x\n", grs[R_TBR]);
#endif	/* __sparcv9 */

	return (DCMD_OK);
}

/*ARGSUSED*/
int
pt_fpregs(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct ps_prochandle *P = mdb.m_target->t_pshandle;
	prfpregset_t fprs;
	lwpid_t lwpid;
	int i;

	/*
	 * The prfpregset structure only provides us with the FPU in the form
	 * of 32 32-bit integers, or 16 doubles.  We use this union of the
	 * various types to be able to display floats and doubles.
	 */
	union {
		struct {
			uint32_t i1;
			uint32_t i2;
		} ip;
		float f;
		double d;
	} fpu;

#ifdef __sparcv9
	prgregset_t grs; /* v9 stores FPRS in the general registers */
#else
	prxregset_t xrs; /* v8plus stores FPRS in the extra registers */
#endif
	if (argc != 0 || (flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (P == NULL) {
		mdb_warn("no process active\n");
		return (DCMD_ERR);
	}

	lwpid = Pstatus(P)->pr_lwp.pr_lwpid;

#ifdef __sparcv9
	if (Plwp_getregs(P, lwpid, grs) == 0)
		mdb_printf("fprs %lx\n", grs[R_FPRS]);
	else
		mdb_warn("failed to get general registers -- fprs unknown");
#else
	if (Plwp_getxregs(P, lwpid, &xrs) == 0 && xrs.pr_type == XR_TYPE_V8P) {
		uint64_t xfsr = (uint64_t)xrs.pr_un.pr_v8p.pr_xfsr << 32;
		mdb_printf("fprs %llx\n", xfsr | xrs.pr_un.pr_v8p.pr_fprs);
	} else
		mdb_warn("failed to get extra registers -- fprs unknown");
#endif
	if (Plwp_getfpregs(P, lwpid, &fprs) != 0) {
		mdb_warn("failed to get floating point registers");
		return (DCMD_ERR);
	}

	mdb_printf("fsr  %lx\n", (ulong_t)fprs.pr_fsr);

	/*
	 * Print each FPU register as a 32-bit integer and single-precision
	 * float.  Print each pair of registers as a double-precision double.
	 */
	for (i = 0; i < 32; i++) {
		fpu.ip.i1 = fprs.pr_fr.pr_regs[i];
		mdb_printf("f%-3d %08x   %e", i, fpu.ip.i1, fpu.f);
		if (i & 1) {
			fpu.ip.i1 = fprs.pr_fr.pr_regs[i - 1];
			fpu.ip.i2 = fprs.pr_fr.pr_regs[i];
			mdb_printf("   %g", fpu.d);
		}
		mdb_printf("\n");
	}

	return (DCMD_OK);
}

const char *
pt_disasm(const GElf_Ehdr *ehp)
{
#ifdef __sparcv9
	const char *disname = "v9plus";
#else
	const char *disname = "v8";
#endif
	/*
	 * If e_machine is SPARC32+, the program has been compiled v8plus or
	 * v8plusa and we need to allow v9 and potentially VIS opcodes.
	 */
	if (ehp != NULL && ehp->e_machine == EM_SPARC32PLUS) {
		if (ehp->e_flags & EF_SPARC_SUN_US1)
			disname = "v9plus";
		else
			disname = "v9";
	}

	return (disname);
}
