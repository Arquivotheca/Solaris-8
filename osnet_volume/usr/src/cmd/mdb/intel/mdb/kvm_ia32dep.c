/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kvm_ia32dep.c	1.1	99/08/11 SMI"

/*
 * Libkvm Kernel Target Intel 32-bit component
 *
 * This file provides the ISA-dependent portion of the libkvm kernel target.
 * For more details on the implementation refer to mdb_kvm.c.
 */

#include <sys/types.h>
#include <sys/reg.h>
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
static const mdb_tgt_regdesc_t kt_ia32_regs[] = {
	{ "eax", KREG_EAX, MDB_TGT_R_EXPORT },
	{ "ebx", KREG_EBX, MDB_TGT_R_EXPORT },
	{ "ecx", KREG_ECX, MDB_TGT_R_EXPORT },
	{ "edx", KREG_EDX, MDB_TGT_R_EXPORT },
	{ "esi", KREG_ESI, MDB_TGT_R_EXPORT },
	{ "edi", KREG_EDI, MDB_TGT_R_EXPORT },
	{ "ebp", KREG_EBP, MDB_TGT_R_EXPORT },
	{ "esp", KREG_ESP, MDB_TGT_R_EXPORT },
	{ "cs", KREG_CS, MDB_TGT_R_EXPORT },
	{ "ds", KREG_DS, MDB_TGT_R_EXPORT },
	{ "ss", KREG_SS, MDB_TGT_R_EXPORT },
	{ "es", KREG_ES, MDB_TGT_R_EXPORT },
	{ "fs", KREG_FS, MDB_TGT_R_EXPORT },
	{ "gs", KREG_GS, MDB_TGT_R_EXPORT },
	{ "eflags", KREG_EFLAGS, MDB_TGT_R_EXPORT },
	{ "eip", KREG_EIP, MDB_TGT_R_EXPORT },
	{ "uesp", KREG_UESP, MDB_TGT_R_EXPORT | MDB_TGT_R_PRIV },
	{ "trapno", KREG_TRAPNO, MDB_TGT_R_EXPORT | MDB_TGT_R_PRIV },
	{ "err", KREG_ERR, MDB_TGT_R_EXPORT | MDB_TGT_R_PRIV },
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

/*
 * Return a flag indicating if the specified %eip is likely to have an
 * interrupt frame on the stack.  We do this by comparing the address to the
 * range of addresses spanned by several well-known routines, and looking
 * to see if the next and previous %ebp values are "far" apart.  Sigh.
 */
static int
kt_intrframe(mdb_tgt_t *t, uintptr_t pc, uintptr_t fp, uintptr_t prevfp)
{
	kt_data_t *kt = t->t_data;

	return ((pc >= kt->k_intr_sym.st_value &&
	    (pc < kt->k_intr_sym.st_value + kt->k_intr_sym.st_size)) ||
	    (pc >= kt->k_trap_sym.st_value &&
	    (pc < kt->k_trap_sym.st_value + kt->k_trap_sym.st_size)) ||
	    (fp >= prevfp + 0x2000) || (fp <= prevfp - 0x2000));
}

/*
 * Given a return address (%eip), determine the likely number of arguments
 * that were pushed on the stack prior to its execution.  We do this by
 * expecting that a typical call sequence consists of pushing arguments on
 * the stack, executing a call instruction, and then performing an add
 * on %esp to restore it to the value prior to pushing the arguments for
 * the call.  We attempt to detect such an add, and divide the addend
 * by the size of a word to determine the number of pushed arguments.
 */
static uint_t
kt_argcount(mdb_tgt_t *t, uintptr_t eip, ssize_t size)
{
	uint8_t ins[6];
	ulong_t n;

	enum {
		M_MODRM_ESP = 0xc4,	/* Mod/RM byte indicates %esp */
		M_ADD_IMM32 = 0x81,	/* ADD imm32 to r/m32 */
		M_ADD_IMM8  = 0x83	/* ADD imm8 to r/m32 */
	};

	if (mdb_tgt_vread(t, ins, sizeof (ins), eip) != sizeof (ins))
		return (0);

	if (ins[1] != M_MODRM_ESP)
		return (0);

	switch (ins[0]) {
	case M_ADD_IMM32:
		n = ins[2] + (ins[3] << 8) + (ins[4] << 16) + (ins[5] << 24);
		break;

	case M_ADD_IMM8:
		n = ins[2];
		break;

	default:
		n = 0;
	}

	return (MIN((ssize_t)n, size) / sizeof (long));
}

static int
kt_stack_impl(mdb_tgt_t *t, const mdb_tgt_gregset_t *gsp,
    mdb_tgt_stack_f *func, void *arg)
{
	mdb_tgt_gregset_t gregs;
	kreg_t *kregs = &gregs.kregs[0];
	int got_pc = (gsp->kregs[KREG_EIP] != 0);

	struct {
		uintptr_t fr_savfp;
		uintptr_t fr_savpc;
		long fr_argv[32];
	} fr;

	uintptr_t fp = gsp->kregs[KREG_EBP];
	uintptr_t pc = gsp->kregs[KREG_EIP];

	struct regs regs;
	uintptr_t prevfp;
	int is_intr;
	ssize_t size;
	uint_t argc;

	bcopy(gsp, &gregs, sizeof (gregs));

	for (is_intr = 0; fp != 0; is_intr = kt_intrframe(t, pc, fp, prevfp)) {
		if (is_intr && mdb_tgt_vread(t, &regs,
		    sizeof (regs), fp) == sizeof (regs)) {
			fp = regs.r_ebp;
			pc = regs.r_eip;
		}

		if (fp & (STACK_ALIGN - 1))
			return (set_errno(EMDB_STKALIGN));

		if ((size = mdb_tgt_vread(t, &fr, sizeof (fr), fp)) >=
		    (ssize_t)(2 * sizeof (uintptr_t))) {
			size -= (ssize_t)(2 * sizeof (uintptr_t));
			argc = kt_argcount(t, fr.fr_savpc, size);
		} else {
			bzero(&fr, sizeof (fr));
			argc = 0;
		}

		if (got_pc && func(arg, pc, argc, fr.fr_argv, &gregs) != 0)
			break;

		kregs[KREG_ESP] = kregs[KREG_EBP];
		prevfp = fp;

		kregs[KREG_EBP] = fp = fr.fr_savfp;
		kregs[KREG_EIP] = pc = fr.fr_savpc;

		got_pc |= (pc != 0);
	}

	return (0);
}

/*ARGSUSED*/
static int
kt_framecount(void *arg, uintptr_t eip, uint_t argc, const long *argv,
    const mdb_tgt_gregset_t *gsp)
{
	(*((int *)arg))++;
	return (0);
}

/*
 * The x86 kernel implementation of swtch() has an interesting side effect:
 * the stack pointer value saved to the t_pcb label_t is not actually pointing
 * at the current frame, but is actually 12 bytes less.  This is because after
 * pushing %ebp, the SAVE_REGS() macro in swtch.s also saves %esi, %edi, and
 * %ebx to the stack.  The resume() code uses these registers to compute the
 * address of curthread->t_sp in order to save %esp there.  Rather than saving
 * %esp + 12, the %esp after the SAVE_REGS() is stored.  Even worse, it seems
 * that the only reason it was implemented this way was that someone forgot
 * the ia32 label_t actually contains space for six registers (not just two
 * like the SPARC label_t); we use all six in setjmp(), but only two in swtch().
 * Naturally, when someone issues the command "address$c", we have no way of
 * telling if the address was cut-and-pasted from $<thread output, in which
 * case we really want to execute "address+0t12$c".  Adb takes the approach
 * of always adding 12; we try to be a little smarter by attempting two stack
 * backtraces and picking the longer stack.  Sigh.
 */
static int
kt_stack_iter(mdb_tgt_t *t, const mdb_tgt_gregset_t *gsp,
    mdb_tgt_stack_f *func, void *arg)
{
	mdb_tgt_gregset_t gs = *gsp;
	int f1 = 0, f2 = 0;

	gs.kregs[KREG_EBP] += 12; /* See comments above */
	(void) kt_stack_impl(t, gsp, kt_framecount, &f1);

	if (f1 > 0) {
		(void) kt_stack_impl(t, &gs, kt_framecount, &f2);
		if (f2 >= f1)
			gsp = &gs;

		return (kt_stack_impl(t, gsp, func, arg));
	} else
		return (kt_stack_impl(t, &gs, func, arg));
}

/*ARGSUSED*/
static int
kt_regs(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kt_data_t *kt = mdb.m_target->t_data;
	const kreg_t *kregs = &kt->k_regs->kregs[0];
	kreg_t eflags = kregs[KREG_EFLAGS];

	if (argc != 0 || (flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	mdb_printf("%%cs = 0x%04x\t\t%%eax = 0x%0?p %A\n",
	    kregs[KREG_CS], kregs[KREG_EAX], kregs[KREG_EAX]);

	mdb_printf("%%ds = 0x%04x\t\t%%ebx = 0x%0?p %A\n",
	    kregs[KREG_DS], kregs[KREG_EBX], kregs[KREG_EBX]);

	mdb_printf("%%ss = 0x%04x\t\t%%ecx = 0x%0?p %A\n",
	    kregs[KREG_SS], kregs[KREG_ECX], kregs[KREG_ECX]);

	mdb_printf("%%es = 0x%04x\t\t%%edx = 0x%0?p %A\n",
	    kregs[KREG_ES], kregs[KREG_EDX], kregs[KREG_EDX]);

	mdb_printf("%%fs = 0x%04x\t\t%%esi = 0x%0?p %A\n",
	    kregs[KREG_FS], kregs[KREG_ESI], kregs[KREG_ESI]);

	mdb_printf("%%gs = 0x%04x\t\t%%edi = 0x%0?p %A\n\n",
	    kregs[KREG_GS], kregs[KREG_EDI], kregs[KREG_EDI]);

	mdb_printf("%%eip = 0x%0?p %A\n", kregs[KREG_EIP], kregs[KREG_EIP]);
	mdb_printf("%%ebp = 0x%0?p\n", kregs[KREG_EBP]);
	mdb_printf("%%esp = 0x%0?p\n\n", kregs[KREG_ESP]);
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

	mdb_printf("  %%uesp = 0x%0?x\n", kregs[KREG_UESP]);
	mdb_printf("%%trapno = 0x%x\n", kregs[KREG_TRAPNO]);
	mdb_printf("   %%err = 0x%x\n", kregs[KREG_ERR]);

	return (DCMD_OK);
}

/*ARGSUSED*/
static int
kt_frame(void *arglim, uintptr_t pc, uint_t argc, const long *argv,
    const mdb_tgt_gregset_t *gregs)
{
	argc = MIN(argc, (uint_t)arglim);
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
	argc = MIN(argc, (uint_t)arglim);
	mdb_printf("%0?lr %a(", gregs->kregs[KREG_EBP], pc);

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
		gregs.kregs[KREG_EBP] = addr;
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

const mdb_tgt_ops_t kt_ia32_ops = {
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
kt_ia32_init(mdb_tgt_t *t)
{
	kt_data_t *kt = t->t_data;

	panic_data_t pd;
	kreg_t *kregs;
	label_t label;
	struct regs regs;
	uintptr_t addr;

	/*
	 * Initialize the machine-dependent parts of the kernel target
	 * structure.  Once this is complete and we fill in the ops
	 * vector, the target is now fully constructed and we can use
	 * the target API itself to perform the rest of our initialization.
	 */
	kt->k_rds = kt_ia32_regs;
	kt->k_regs = mdb_zalloc(sizeof (mdb_tgt_gregset_t), UM_SLEEP);
	kt->k_regsize = sizeof (mdb_tgt_gregset_t);
	kt->k_dcmd_regs = kt_regs;
	kt->k_dcmd_stack = kt_stack;
	kt->k_dcmd_stackv = kt_stackv;

	t->t_ops = &kt_ia32_ops;
	kregs = kt->k_regs->kregs;

	(void) mdb_dis_select("ia32");

	/*
	 * Lookup the symbols corresponding to subroutines in locore.s where
	 * we expect a saved regs structure to be pushed on the stack.  When
	 * performing stack tracebacks we will attempt to detect interrupt
	 * frames by comparing the %eip value to these symbols.
	 */
	(void) mdb_tgt_lookup_by_name(t, MDB_TGT_OBJ_EXEC,
	    "cmnint", &kt->k_intr_sym);

	(void) mdb_tgt_lookup_by_name(t, MDB_TGT_OBJ_EXEC,
	    "cmntrap", &kt->k_trap_sym);

	/*
	 * Don't attempt to load any thread or register information if
	 * we're examining the live operating system.
	 */
	if (strcmp(kt->k_symfile, "/dev/ksyms") == 0)
		return;

	/*
	 * If the panicbuf symbol is present and we can consume a panicbuf
	 * header of the appropriate version from this address, then we can
	 * initialize our current register set based on its contents.
	 * Prior to the re-structuring of panicbuf, our only register data
	 * was the panic_regs label_t, into which a setjmp() was performed,
	 * or the panic_reg register pointer, which was only non-zero if
	 * the system panicked as a result of a trap calling die().
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
			(void) kt_putareg(t, kt->k_tid,
			    pdp->pd_nvdata[i].pnv_name,
			    pdp->pd_nvdata[i].pnv_value);
		}

		mdb_free(pdp, pd_size);

	} else if (mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &addr, sizeof (addr),
	    MDB_TGT_OBJ_EXEC, "panic_reg") == sizeof (addr) && addr != NULL &&
	    mdb_tgt_vread(t, &regs, sizeof (regs), addr) == sizeof (regs)) {

		kregs[KREG_EAX] = regs.r_eax;
		kregs[KREG_EBX] = regs.r_ebx;
		kregs[KREG_ECX] = regs.r_ecx;
		kregs[KREG_EDX] = regs.r_edx;
		kregs[KREG_ESI] = regs.r_esi;
		kregs[KREG_EDI] = regs.r_edi;
		kregs[KREG_EBP] = regs.r_ebp;
		kregs[KREG_ESP] = regs.r_esp;
		kregs[KREG_CS] = regs.r_cs;
		kregs[KREG_DS] = regs.r_ds;
		kregs[KREG_SS] = regs.r_ss;
		kregs[KREG_ES] = regs.r_es;
		kregs[KREG_FS] = regs.r_fs;
		kregs[KREG_GS] = regs.r_gs;
		kregs[KREG_EFLAGS] = regs.r_efl;
		kregs[KREG_EIP] = regs.r_eip;
		kregs[KREG_UESP] = regs.r_uesp;
		kregs[KREG_TRAPNO] = regs.r_trapno;
		kregs[KREG_ERR] = regs.r_err;

	} else if (mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &label, sizeof (label),
	    MDB_TGT_OBJ_EXEC, "panic_regs") == sizeof (label)) {

		kregs[KREG_EDI] = label.val[0];
		kregs[KREG_ESI] = label.val[1];
		kregs[KREG_EBX] = label.val[2];
		kregs[KREG_EBP] = label.val[3];
		kregs[KREG_ESP] = label.val[4];
		kregs[KREG_EIP] = label.val[5];

	} else {
		warn("failed to read panicbuf, panic_reg and panic_regs -- "
		    "current register set will be unavailable\n");
	}
}
