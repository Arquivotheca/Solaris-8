/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_isadep.c	1.1	99/08/11 SMI"

/*
 * User Process Target Intel 32-bit component
 *
 * This file provides the ISA-dependent portion of the user process target.
 * For more details on the implementation refer to mdb_proc.c.
 */

#include <mdb/mdb_target_impl.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_kreg.h>
#include <mdb/mdb.h>

#include <libproc.h>
#include <sys/fp.h>
#include <ieeefp.h>

const mdb_tgt_regdesc_t pt_regdesc[] = {
	{ "gs", GS, MDB_TGT_R_EXPORT },
	{ "fs", FS, MDB_TGT_R_EXPORT },
	{ "es", ES, MDB_TGT_R_EXPORT },
	{ "ds", DS, MDB_TGT_R_EXPORT },
	{ "edi", EDI, MDB_TGT_R_EXPORT },
	{ "esi", ESI, MDB_TGT_R_EXPORT },
	{ "ebp", EBP, MDB_TGT_R_EXPORT },
	{ "kesp", ESP, MDB_TGT_R_EXPORT },
	{ "ebx", EBX, MDB_TGT_R_EXPORT },
	{ "edx", EDX, MDB_TGT_R_EXPORT },
	{ "ecx", ECX, MDB_TGT_R_EXPORT },
	{ "eax", EAX, MDB_TGT_R_EXPORT },
	{ "trapno", TRAPNO, MDB_TGT_R_EXPORT },
	{ "err", ERR, MDB_TGT_R_EXPORT },
	{ "eip", EIP, MDB_TGT_R_EXPORT },
	{ "cs", CS, MDB_TGT_R_EXPORT },
	{ "eflags", EFL, MDB_TGT_R_EXPORT },
	{ "esp", UESP, MDB_TGT_R_EXPORT },
	{ "ss", SS, MDB_TGT_R_EXPORT },
	{ NULL, 0, 0 }
};

/*ARGSUSED*/
int
pt_regs(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	prgregset_t grs;
	prgreg_t eflags;

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

	eflags = grs[EFL];

	mdb_printf("%%cs = 0x%04x\t\t%%eax = 0x%0?p %A\n",
	    grs[CS], grs[EAX], grs[EAX]);

	mdb_printf("%%ds = 0x%04x\t\t%%ebx = 0x%0?p %A\n",
	    grs[DS], grs[EBX], grs[EBX]);

	mdb_printf("%%ss = 0x%04x\t\t%%ecx = 0x%0?p %A\n",
	    grs[SS], grs[ECX], grs[ECX]);

	mdb_printf("%%es = 0x%04x\t\t%%edx = 0x%0?p %A\n",
	    grs[ES], grs[EDX], grs[EDX]);

	mdb_printf("%%fs = 0x%04x\t\t%%esi = 0x%0?p %A\n",
	    grs[FS], grs[ESI], grs[ESI]);

	mdb_printf("%%gs = 0x%04x\t\t%%edi = 0x%0?p %A\n\n",
	    grs[GS], grs[EDI], grs[EDI]);

	mdb_printf(" %%eip = 0x%0?p %A\n", grs[EIP], grs[EIP]);
	mdb_printf(" %%ebp = 0x%0?p\n", grs[EBP]);
	mdb_printf("%%kesp = 0x%0?p\n\n", grs[ESP]);
	mdb_printf("%%eflags = 0x%08x\n", eflags);

	mdb_printf("  id=%u vip=%u vif=%u ac=%u vm=%u rf=%u nt=%u iopl=0x%x\n",
	    (eflags & KREG_EFLAGS_ID_MASK) >> KREG_EFLAGS_ID_SHIFT,
	    (eflags & KREG_EFLAGS_VIP_MASK) >> KREG_EFLAGS_VIP_SHIFT,
	    (eflags & KREG_EFLAGS_VIF_MASK) >> KREG_EFLAGS_VIF_SHIFT,
	    (eflags & KREG_EFLAGS_AC_MASK) >> KREG_EFLAGS_AC_SHIFT,
	    (eflags & KREG_EFLAGS_VM_MASK) >> KREG_EFLAGS_VM_SHIFT,
	    (eflags & KREG_EFLAGS_RF_MASK) >> KREG_EFLAGS_RF_SHIFT,
	    (eflags & KREG_EFLAGS_NT_MASK) >> KREG_EFLAGS_NT_SHIFT,
	    (eflags & KREG_EFLAGS_IOPL_MASK) >> KREG_EFLAGS_IOPL_SHIFT);

	mdb_printf("  status=<%s,%s,%s,%s,%s,%s,%s,%s,%s>\n\n",
	    (eflags & KREG_EFLAGS_OF_MASK) ? "OF" : "of",
	    (eflags & KREG_EFLAGS_DF_MASK) ? "DF" : "df",
	    (eflags & KREG_EFLAGS_IF_MASK) ? "IF" : "if",
	    (eflags & KREG_EFLAGS_TF_MASK) ? "TF" : "tf",
	    (eflags & KREG_EFLAGS_SF_MASK) ? "SF" : "sf",
	    (eflags & KREG_EFLAGS_ZF_MASK) ? "ZF" : "zf",
	    (eflags & KREG_EFLAGS_AF_MASK) ? "AF" : "af",
	    (eflags & KREG_EFLAGS_PF_MASK) ? "PF" : "pf",
	    (eflags & KREG_EFLAGS_CF_MASK) ? "CF" : "cf");

	mdb_printf("   %%esp = 0x%0?x\n", grs[UESP]);
	mdb_printf("%%trapno = 0x%x\n", grs[TRAPNO]);
	mdb_printf("   %%err = 0x%x\n", grs[ERR]);

	return (DCMD_OK);
}

static const char *
fpcw2str(uint32_t cw, char *buf, size_t nbytes)
{
	char *end = buf + nbytes;
	char *p = buf;

	buf[0] = '\0';

	/*
	 * Decode all masks in the 80387 control word.
	 */
	if (cw & FPINV)
		p += mdb_snprintf(p, (size_t)(end - p), "|INV");
	if (cw & FPDNO)
		p += mdb_snprintf(p, (size_t)(end - p), "|DNO");
	if (cw & FPZDIV)
		p += mdb_snprintf(p, (size_t)(end - p), "|ZDIV");
	if (cw & FPOVR)
		p += mdb_snprintf(p, (size_t)(end - p), "|OVR");
	if (cw & FPUNR)
		p += mdb_snprintf(p, (size_t)(end - p), "|UNR");
	if (cw & FPPRE)
		p += mdb_snprintf(p, (size_t)(end - p), "|PRE");
	if (cw & FPPC)
		p += mdb_snprintf(p, (size_t)(end - p), "|PC");
	if (cw & FPRC)
		p += mdb_snprintf(p, (size_t)(end - p), "|RC");
	if (cw & FPIC)
		p += mdb_snprintf(p, (size_t)(end - p), "|IC");
	if (cw & WFPDE)
		p += mdb_snprintf(p, (size_t)(end - p), "|WFPDE");

	/*
	 * Decode precision, rounding, and infinity options in control word.
	 */
	if (cw & FPSIG24)
		p += mdb_snprintf(p, (size_t)(end - p), "|SIG24");
	if (cw & FPSIG53)
		p += mdb_snprintf(p, (size_t)(end - p), "|SIG53");
	if (cw & FPSIG64)
		p += mdb_snprintf(p, (size_t)(end - p), "|SIG64");
	if (cw & FPRTN)
		p += mdb_snprintf(p, (size_t)(end - p), "|RTN");
	if (cw & FPRD)
		p += mdb_snprintf(p, (size_t)(end - p), "|RD");
	if (cw & FPRU)
		p += mdb_snprintf(p, (size_t)(end - p), "|RU");
	if (cw & FPCHOP)
		p += mdb_snprintf(p, (size_t)(end - p), "|CHOP");
	if (cw & FPP)
		p += mdb_snprintf(p, (size_t)(end - p), "|P");
	if (cw & FPA)
		p += mdb_snprintf(p, (size_t)(end - p), "|A");
	if (cw & WFPB17)
		p += mdb_snprintf(p, (size_t)(end - p), "|WFPB17");
	if (cw & WFPB24)
		p += mdb_snprintf(p, (size_t)(end - p), "|WFPB24");

	if (buf[0] == '|')
		return (buf + 1);

	return ("0");
}

static const char *
fpsw2str(uint32_t cw, char *buf, size_t nbytes)
{
	char *end = buf + nbytes;
	char *p = buf;

	buf[0] = '\0';

	/*
	 * Decode all masks in the 80387 status word.
	 */
	if (cw & FPS_IE)
		p += mdb_snprintf(p, (size_t)(end - p), "|IE");
	if (cw & FPS_DE)
		p += mdb_snprintf(p, (size_t)(end - p), "|DE");
	if (cw & FPS_ZE)
		p += mdb_snprintf(p, (size_t)(end - p), "|ZE");
	if (cw & FPS_OE)
		p += mdb_snprintf(p, (size_t)(end - p), "|OE");
	if (cw & FPS_UE)
		p += mdb_snprintf(p, (size_t)(end - p), "|UE");
	if (cw & FPS_PE)
		p += mdb_snprintf(p, (size_t)(end - p), "|PE");
	if (cw & FPS_SF)
		p += mdb_snprintf(p, (size_t)(end - p), "|SF");
	if (cw & FPS_ES)
		p += mdb_snprintf(p, (size_t)(end - p), "|ES");
	if (cw & FPS_C0)
		p += mdb_snprintf(p, (size_t)(end - p), "|C0");
	if (cw & FPS_C1)
		p += mdb_snprintf(p, (size_t)(end - p), "|C1");
	if (cw & FPS_C2)
		p += mdb_snprintf(p, (size_t)(end - p), "|C2");
	if (cw & FPS_C3)
		p += mdb_snprintf(p, (size_t)(end - p), "|C3");
	if (cw & FPS_B)
		p += mdb_snprintf(p, (size_t)(end - p), "|B");

	if (buf[0] == '|')
		return (buf + 1);

	return ("0");
}

/*ARGSUSED*/
int
pt_fpregs(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct ps_prochandle *P = mdb.m_target->t_pshandle;
	uint32_t hw = FP_NO;
	prfpregset_t fprs;
	struct _fpstate fps;
	char buf[256];
	uint_t top;
	int i;

	/*
	 * Union for overlaying _fpreg structure on to quad-precision
	 * floating-point value (long double).
	 */
	union {
		struct _fpreg reg;
		long double ld;
	} fpru;

	/*
	 * Array of strings corresponding to FPU tag word values (see
	 * section 7.3.6 of the Intel Programmer's Reference Manual).
	 */
	const char *tag_strings[] = { "valid", "zero", "special", "empty" };

	if (argc != 0 || (flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (P == NULL) {
		mdb_warn("no process active\n");
		return (DCMD_ERR);
	}

	if (mdb_tgt_readsym(mdb.m_target, MDB_TGT_AS_VIRT, &hw,
	    sizeof (hw), "libc.so", "_fp_hw") < 0 &&
	    mdb_tgt_readsym(mdb.m_target, MDB_TGT_AS_VIRT, &hw,
	    sizeof (hw), MDB_TGT_OBJ_EXEC, "_fp_hw") < 0)
		mdb_warn("failed to read _fp_hw value");

	mdb_printf("_fp_hw 0x%02x (", hw);
	switch (hw) {
	case FP_SW:
		mdb_printf("80387 software emulator");
		break;
	case FP_287:
		mdb_printf("80287 chip");
		break;
	case FP_387:
		mdb_printf("80387 chip");
		break;
	case FP_486:
		mdb_printf("80486 chip");
		break;
	default:
		mdb_printf("no floating point support");
		break;
	}
	mdb_printf(")\n");

	if (!(hw & FP_HW))
		return (DCMD_OK); /* just abort if no hardware present */

	if (Plwp_getfpregs(P, Pstatus(P)->pr_lwp.pr_lwpid, &fprs) != 0) {
		mdb_warn("failed to get floating point registers");
		return (DCMD_ERR);
	}

	bcopy(&fprs.fp_reg_set.fpchip_state, &fps, sizeof (fps));
	top = (fps.status & FPS_TOP) >> 11;

	fps.cw &= 0xffff;	/* control word is really 16 bits */
	fps.sw &= 0xffff;	/* status word is really 16 bits */
	fps.status &= 0xffff;	/* saved status word is really 16 bits */

	mdb_printf("cw     0x%04x (%s)\n", fps.cw,
	    fpcw2str(fps.cw, buf, sizeof (buf)));

	mdb_printf("sw     0x%04x (TOP=0t%d) (%s)\n", fps.sw,
	    (fps.sw & FPS_TOP) >> 11, fpsw2str(fps.sw, buf, sizeof (buf)));

	mdb_printf("xcp sw 0x%04x (TOP=0t%u) (%s)\n\n", fps.status,
	    (fps.status & FPS_TOP) >> 11,
	    fpsw2str(fps.status, buf, sizeof (buf)));

	mdb_printf("ipoff  %la\n", fps.ipoff);
	mdb_printf("cssel  0x%lx\n", fps.cssel);
	mdb_printf("dtoff  %la\n", fps.dataoff);
	mdb_printf("dtsel  0x%lx\n\n", fps.datasel);

	for (i = 0; i < 8; i++) {
		/*
		 * Recall that we need to use the current TOP-of-stack value to
		 * associate the _st[] index back to a physical register number,
		 * since tag word indices are physical register numbers.  Then
		 * to get the tag value, we shift over two bits for each tag
		 * index, and then grab the bottom two bits.
		 */
		uint_t tag_index = (i + top) & 7;
		uint_t tag_value = (fps.tag >> (tag_index * 2)) & 3;

		fpru.reg = fps._st[i];
		mdb_printf("ST%d    %lg %s\n", i, fpru.ld,
		    tag_strings[tag_value]);
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
const char *
pt_disasm(const GElf_Ehdr *ehp)
{
	return ("ia32");
}
