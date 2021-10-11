/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_proc.c	1.2	99/11/19 SMI"

/*
 * User Process Target
 *
 * The user process target is invoked when the -u or -p command-line options
 * are used, or when an ELF executable file or ELF core file is specified on
 * the command-line.  This target is also selected by default when no target
 * options are present.  In this case, it defaults the executable name to
 * "a.out".  If no process or core file is currently attached, the target
 * functions as a kind of virtual /dev/zero (in accordance with adb(1)
 * semantics); reads from the virtual address space return zeroes and writes
 * fail silently.  The proc target itself is designed as a wrapper around the
 * services provided by libproc.so: t->t_pshandle is set to the struct
 * ps_prochandle pointer returned as a handle by libproc.  The target also
 * opens the executable file itself using the MDB GElf services, for
 * interpreting the .symtab and .dynsym if no libproc handle has been
 * initialized, and for handling i/o to and from the object file.  Currently,
 * the only ISA-dependent portions of the proc target are the $r and ::fpregs
 * dcmds, and the list of named registers; these are linked in from the
 * proc_isadep.c file for each ISA and called from the common code in this file.
 */

#include <mdb/mdb_target_impl.h>
#include <mdb/mdb_disasm.h>
#include <mdb/mdb_io_impl.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_gelf.h>
#include <mdb/mdb_conf.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb.h>

#include <sys/utsname.h>
#include <sys/param.h>
#include <libproc.h>
#include <signal.h>
#include <string.h>

/*
 * The mdb_tgt_gregset type is opaque to callers of the target interface.
 * Inside the target we define it explicitly to be a prgregset_t.
 */
struct mdb_tgt_gregset {
	prgregset_t gregs;			/* Procfs general registers */
};

/*
 * The proc_<ISA>dep.c file is expected to define the following
 * ISA-dependent pieces of the proc target:
 */
extern int pt_regs(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int pt_fpregs(uintptr_t, uint_t, int, const mdb_arg_t *);
extern const char *pt_disasm(const GElf_Ehdr *);
extern const mdb_tgt_regdesc_t pt_regdesc[];

typedef struct pt_data {
	mdb_gelf_symtab_t *p_symtab;		/* Standard symbol table */
	mdb_gelf_symtab_t *p_dynsym;		/* Dynamic symbol table */
	mdb_gelf_file_t *p_file;		/* ELF file object */
	mdb_io_t *p_fio;			/* File i/o backend */
	char p_platform[MAXNAMELEN];		/* Platform string */
	mdb_map_t p_map;			/* Persistent map for callers */
	const mdb_tgt_regdesc_t *p_rds;		/* Register description table */
	int p_oflags;				/* Flags for open(2) */
	int p_gflags;				/* Flags for Pgrab(3X) */
	int p_rflags;				/* Flags for Prelease(3X) */
} pt_data_t;

typedef struct pt_symarg {
	mdb_tgt_t *psym_targ;			/* Target pointer */
	uint_t psym_which;			/* Type of symbol table */
	uint_t psym_type;			/* Type of symbols to match */
	mdb_tgt_sym_f *psym_func;		/* Callback function */
	void *psym_private;			/* Callback data */
} pt_symarg_t;

typedef struct pt_maparg {
	mdb_tgt_t *pmap_targ;			/* Target pointer */
	mdb_tgt_map_f *pmap_func;		/* Callback function */
	void *pmap_private;			/* Callback data */
} pt_maparg_t;

typedef struct pt_stkarg {
	mdb_tgt_stack_f *pstk_func;		/* Callback function */
	void *pstk_private;			/* Callback data */
} pt_stkarg_t;

static const char PT_EXEC_PATH[] = "a.out";	/* Default executable */
static const char PT_CORE_PATH[] = "core";	/* Default core file */

static int
pt_setflags(mdb_tgt_t *t, int flags)
{
	pt_data_t *pt = t->t_data;

	if (flags & MDB_TGT_F_RDWR) {
		mdb_io_t *io;

		if (pt->p_fio == NULL)
			return (set_errno(EMDB_NOEXEC));

		io = mdb_fdio_create_path(NULL, IOP_NAME(pt->p_fio), O_RDWR, 0);

		if (io != NULL) {
			t->t_flags |= MDB_TGT_F_RDWR;
			pt->p_fio = mdb_io_hold(io);
			mdb_io_rele(pt->p_file->gf_io);
			pt->p_file->gf_io = pt->p_fio;
		} else
			return (-1);
	}

	if (flags & MDB_TGT_F_FORCE) {
		t->t_flags |= MDB_TGT_F_FORCE;
		pt->p_gflags |= PGRAB_FORCE;
	}

	return (0);
}

/*ARGSUSED*/
static int
pt_frame(void *arglim, uintptr_t pc, uint_t argc, const long *argv,
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
pt_framev(void *arglim, uintptr_t pc, uint_t argc, const long *argv,
    const mdb_tgt_gregset_t *gregs)
{

	argc = MIN(argc, (uint_t)(uintptr_t)arglim);
#if defined(i386) || defined(__i386)
	mdb_printf("%0?lr %a(", gregs->gregs[R_FP], pc);
#else
	mdb_printf("%0?lr %a(", gregs->gregs[R_SP], pc);
#endif

	if (argc != 0) {
		mdb_printf("%lr", *argv++);
		for (argc--; argc != 0; argc--)
			mdb_printf(", %lr", *argv++);
	}

	mdb_printf(")\n");
	return (0);
}

static int
pt_stack_common(uintptr_t addr, uint_t flags, int argc,
    const mdb_arg_t *argv, mdb_tgt_stack_f *func)
{
	void *arg = (void *)mdb.m_nargs;
	mdb_tgt_gregset_t gregs;

	if (argc != 0) {
		if (argv->a_type == MDB_TYPE_CHAR || argc > 1)
			return (DCMD_USAGE);

		if (argv->a_type == MDB_TYPE_STRING)
			arg = (void *)(uint_t)mdb_strtoull(argv->a_un.a_str);
		else
			arg = (void *)(uint_t)argv->a_un.a_val;
	}

	if (mdb.m_target->t_pshandle == NULL) {
		mdb_warn("no process active\n");
		return (DCMD_ERR);
	}

	/*
	 * In the universe of sparcv7, sparcv9, and ia32, this code can be
	 * common: <sys/procfs_isa.h> conveniently #defines R_FP to be the
	 * appropriate register we need to set in order to perform a stack
	 * traceback from a given frame address.  It is my suspicion that
	 * this will not be a valid assumption in the ia64 universe.
	 */
	if (flags & DCMD_ADDRSPEC) {
		bzero(&gregs, sizeof (gregs));
		gregs.gregs[R_FP] = addr;
	} else if (Plwp_getregs(mdb.m_target->t_pshandle,
	    Pstatus(mdb.m_target->t_pshandle)->pr_lwp.pr_lwpid, gregs.gregs)) {
		mdb_warn("failed to get current register set");
		return (DCMD_ERR);
	}

	(void) mdb_tgt_stack_iter(mdb.m_target, &gregs, func, arg);
	return (DCMD_OK);
}

static int
pt_stack(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (pt_stack_common(addr, flags, argc, argv, pt_frame));
}

static int
pt_stackv(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	return (pt_stack_common(addr, flags, argc, argv, pt_framev));
}

/*ARGSUSED*/
static int
pt_attach(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_tgt_t *t = mdb.m_target;
	pt_data_t *pt = t->t_data;
	int perr;

	if (!(flags & DCMD_ADDRSPEC) && argc == 0)
		return (DCMD_USAGE);

	if (((flags & DCMD_ADDRSPEC) && argc != 0) || argc > 1 ||
	    (argc != 0 && argv->a_type != MDB_TYPE_STRING))
		return (DCMD_USAGE);

	if (t->t_pshandle != NULL) {
		mdb_warn("debugger is already attached to a %s\n",
		    (Pstate(t->t_pshandle) == PS_DEAD) ? "core" : "process");
		return (DCMD_ERR);
	}

	if (pt->p_fio == NULL) {
		mdb_warn("attach requires executable to be specified on "
		    "command-line (or use -p)\n");
		return (DCMD_ERR);
	}

	if (flags & DCMD_ADDRSPEC)
		t->t_pshandle = Pgrab((pid_t)addr, pt->p_gflags, &perr);
	else
		t->t_pshandle = proc_arg_grab(argv->a_un.a_str,
		    PR_ARG_ANY, pt->p_gflags, &perr);


	if (t->t_pshandle == NULL) {
		mdb_warn("cannot attach: %s\n", Pgrab_error(perr));
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

static int
pt_regstatus(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_tgt_t *t = mdb.m_target;

	if (t->t_pshandle != NULL) {
		const pstatus_t *psp = Pstatus(t->t_pshandle);
		int cursig = psp->pr_lwp.pr_cursig;
		char signame[SIG2STR_MAX];

		if (Pstate(t->t_pshandle) != PS_DEAD)
			mdb_printf("process id = %d\n", psp->pr_pid);
		else
			mdb_printf("no process\n");

		if (cursig != 0 && sig2str(cursig, signame) == 0)
			mdb_printf("SIG%s: %s\n", signame, strsignal(cursig));
	}

	return (pt_regs(addr, flags, argc, argv));
}

/*ARGSUSED*/
static int
pt_detach(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	mdb_tgt_t *t = mdb.m_target;
	pt_data_t *pt = t->t_data;

	if ((flags & DCMD_ADDRSPEC) || argc != 0)
		return (DCMD_USAGE);

	if (t->t_pshandle == NULL) {
		mdb_warn("debugger is not currently attached to a process "
		    "or core file\n");
		return (DCMD_ERR);
	}

	Prelease(t->t_pshandle, pt->p_rflags);
	t->t_pshandle = NULL;
	return (DCMD_OK);
}

static uintmax_t
reg_disc_get(const mdb_var_t *v)
{
	mdb_tgt_t *t = MDB_NV_COOKIE(v);
	mdb_tgt_reg_t r = 0;
	const pstatus_t *psp;

	if (t->t_pshandle != NULL && (psp = Pstatus(t->t_pshandle)) != NULL) {
		(void) mdb_tgt_getareg(t, (mdb_tgt_tid_t)psp->pr_lwp.pr_lwpid,
		    mdb_nv_get_name(v), &r);
	}

	return (r);
}

/*ARGSUSED*/
static int
pt_status(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	struct ps_prochandle *P = mdb.m_target->t_pshandle;
	pt_data_t *pt = mdb.m_target->t_data;

	if (P != NULL) {
		const psinfo_t *pip = Ppsinfo(P);
		const pstatus_t *psp = Pstatus(P);
		int cursig = 0, bits = 0;

		char execname[MAXPATHLEN];
		char signame[SIG2STR_MAX];
		struct utsname uts;

		(void) strcpy(uts.nodename, "unknown machine");
		(void) Puname(P, &uts);

		if (pip != NULL)
			bits = pip->pr_dmodel == PR_MODEL_ILP32 ? 32 : 64;
		if (psp != NULL)
			cursig = psp->pr_lwp.pr_cursig;

		if (Pstate(P) != PS_DEAD) {
			mdb_printf("debugging PID %d (%d-bit)\n",
			    pip->pr_pid, bits);
		} else if (pip != NULL) {
			mdb_printf("debugging core file of %s (%d-bit) "
			    "from %s\n", pip->pr_fname, bits, uts.nodename);
		} else
			mdb_printf("debugging core file\n");

		if (Pexecname(P, execname, sizeof (execname)) != NULL)
			mdb_printf("executable file: %s\n", execname);

		if (pip != NULL)
			mdb_printf("initial argv: %s\n", pip->pr_psargs);

		if (cursig != 0 && sig2str(cursig, signame) == 0) {
			mdb_printf("status: SIG%s (%s)\n", signame,
			    strsignal(cursig));
		}

	} else if (pt->p_file != NULL) {
		const GElf_Ehdr *ehp = &pt->p_file->gf_ehdr;

		mdb_printf("debugging %s '%s' (%d-bit)\n",
		    ehp->e_type == ET_EXEC ?  "executable" : "object file",
		    IOP_NAME(pt->p_fio),
		    ehp->e_ident[EI_CLASS] == ELFCLASS32 ? 32 : 64);
	}

	return (DCMD_OK);
}

static const mdb_dcmd_t pt_dcmds[] = {
	{ "$c", "[cnt]", "print stack backtrace", pt_stack },
	{ "$C", "[cnt]", "print stack backtrace", pt_stackv },
	{ "$r", NULL, "print general-purpose registers", pt_regs },
	{ "$x", NULL, "print floating point registers", pt_fpregs },
	{ "$X", NULL, "print floating point registers", pt_fpregs },
	{ "$y", NULL, "print floating point registers", pt_fpregs },
	{ "$Y", NULL, "print floating point registers", pt_fpregs },
	{ "$?", NULL, "print status and registers", pt_regstatus },
	{ ":A", "?[core|pid]", "attach to process or core file", pt_attach },
	{ ":R", NULL, "release the previously attached process", pt_detach },
	{ "attach", "?[core|pid]",
	    "attach to process or core file", pt_attach },
	{ "release", NULL,
	    "release the previously attached process", pt_detach },
	{ "regs", NULL, "print general-purpose registers", pt_regs },
	{ "fpregs", NULL, "print floating point registers", pt_fpregs },
	{ "stack", NULL, "print stack backtrace", pt_stack },
	{ "status", NULL, "print summary of current target", pt_status },
	{ NULL }
};

static void
pt_activate(mdb_tgt_t *t)
{
	static const mdb_nv_disc_t reg_disc = { NULL, reg_disc_get };

	pt_data_t *pt = t->t_data;
	const mdb_tgt_regdesc_t *rdp;
	const mdb_dcmd_t *dcp;
	struct utsname u1, u2;
	GElf_Sym s1, s2;

	if (t->t_pshandle) {
		mdb_prop_postmortem = (Pstate(t->t_pshandle) == PS_DEAD);
		mdb_prop_kernel = FALSE;
	} else
		mdb_prop_kernel = mdb_prop_postmortem = FALSE;

	mdb_prop_datamodel = MDB_TGT_MODEL_NATIVE;

	/*
	 * If we're examining a core file, and uname(2) does not match the
	 * NT_UTSNAME note recorded in the core file, issue a warning.
	 * Someday I will put text sections in the damn core files.
	 */
	if (mdb_prop_postmortem == TRUE && uname(&u1) >= 0 &&
	    Puname(t->t_pshandle, &u2) == 0 && (strcmp(u1.release,
	    u2.release) || strcmp(u1.version, u2.version))) {
		mdb_warn("warning: core file is from %s %s %s; shared text "
		    "mappings may not match installed libraries\n",
		    u2.sysname, u2.release, u2.version);
	}

	/*
	 * If we have a libproc handle and AT_BASE is set, the process or core
	 * is dynamically linked.  We call Prd_agent() to force libproc to
	 * try to initialize librtld_db, and issue a warning if that fails.
	 */
	if (t->t_pshandle != NULL && Pgetauxval(t->t_pshandle,
	    AT_BASE) != -1L && Prd_agent(t->t_pshandle) == NULL) {
		mdb_warn("warning: librtld_db failed to initialize; shared "
		    "library information will not be available\n");
	}

	/*
	 * If we've got an _start symbol with a zero size, prime the private
	 * symbol table with a copy of _start with its size set to the distance
	 * between _mcount and _start.  We do this because DevPro has shipped
	 * the Intel crt1.o without proper .size directives for years, which
	 * precludes proper identification of _start in stack traces.
	 */
	if (mdb_gelf_symtab_lookup_by_name(pt->p_dynsym, "_start", &s1) == 0 &&
	    s1.st_size == 0 && GELF_ST_TYPE(s1.st_info) == STT_FUNC) {
		if (mdb_gelf_symtab_lookup_by_name(pt->p_dynsym, "_mcount",
		    &s2) == 0 && GELF_ST_TYPE(s2.st_info) == STT_FUNC) {
			s1.st_size = s2.st_value - s1.st_value;
			mdb_gelf_symtab_insert(mdb.m_prsym, "_start", &s1);
		}
	}

	/*
	 * If there's a global object named '_mdb_abort_info', assuming we're
	 * debugging mdb itself and load the developer support module.
	 */
	if (mdb_gelf_symtab_lookup_by_name(pt->p_symtab, "_mdb_abort_info",
	    &s1) == 0 && GELF_ST_TYPE(s1.st_info) == STT_OBJECT) {
		if (mdb_module_load("mdb_ds", MDB_MOD_SILENT) == NULL)
			warn("Warning: failed to load developer support\n");
	}

	/*
	 * Export our target-specific dcmds.  These are interposed on
	 * top of the default mdb dcmds of the same name.
	 */
	for (dcp = &pt_dcmds[0]; dcp->dc_name != NULL; dcp++) {
		if (mdb_module_add_dcmd(t->t_module, dcp, MDB_MOD_FORCE) == -1)
			warn("failed to add dcmd %s", dcp->dc_name);
	}

	/*
	 * Iterate through our register description list and export
	 * each register as a named variable.
	 */
	for (rdp = pt->p_rds; rdp->rd_name != NULL; rdp++) {
		if (!(rdp->rd_flags & MDB_TGT_R_EXPORT))
			continue; /* Don't export register as a variable */

		(void) mdb_nv_insert(&mdb.m_nv, rdp->rd_name, &reg_disc,
		    (uintptr_t)t, MDB_NV_PERSIST | MDB_NV_RDONLY);
	}

	mdb_tgt_elf_export(pt->p_file);
}

static void
pt_deactivate(mdb_tgt_t *t)
{
	pt_data_t *pt = t->t_data;
	const mdb_tgt_regdesc_t *rdp;
	const mdb_dcmd_t *dcp;

	for (rdp = pt->p_rds; rdp->rd_name != NULL; rdp++) {
		mdb_var_t *v;

		if (!(rdp->rd_flags & MDB_TGT_R_EXPORT))
			continue; /* Didn't export register as a variable */

		if ((v = mdb_nv_lookup(&mdb.m_nv, rdp->rd_name)) != NULL) {
			v->v_flags &= ~MDB_NV_PERSIST;
			mdb_nv_remove(&mdb.m_nv, v);
		}
	}

	for (dcp = &pt_dcmds[0]; dcp->dc_name != NULL; dcp++) {
		if (mdb_module_remove_dcmd(t->t_module, dcp->dc_name) == -1)
			warn("failed to remove dcmd %s", dcp->dc_name);
	}

	mdb_gelf_symtab_delete(mdb.m_prsym, "_start", NULL);

	mdb_prop_postmortem = FALSE;
	mdb_prop_kernel = FALSE;
	mdb_prop_datamodel = MDB_TGT_MODEL_UNKNOWN;
}

static void
pt_destroy(mdb_tgt_t *t)
{
	pt_data_t *pt = t->t_data;

	if (pt->p_symtab != NULL)
		mdb_gelf_symtab_destroy(pt->p_symtab);
	if (pt->p_dynsym != NULL)
		mdb_gelf_symtab_destroy(pt->p_dynsym);
	if (pt->p_file != NULL)
		mdb_gelf_destroy(pt->p_file);

	if (t->t_pshandle != NULL)
		Prelease(t->t_pshandle, pt->p_rflags);

	mdb_free(pt, sizeof (pt_data_t));
}

/*ARGSUSED*/
static const char *
pt_name(mdb_tgt_t *t)
{
	return ("proc");
}

static const char *
pt_platform(mdb_tgt_t *t)
{
	pt_data_t *pt = t->t_data;

	if (t->t_pshandle != NULL &&
	    Pplatform(t->t_pshandle, pt->p_platform, MAXNAMELEN) != NULL)
		return (pt->p_platform);

	return (mdb_conf_platform());
}

static int
pt_uname(mdb_tgt_t *t, struct utsname *utsp)
{
	if (t->t_pshandle != NULL)
		return (Puname(t->t_pshandle, utsp));

	return (uname(utsp) >= 0 ? 0 : -1);
}

static ssize_t
pt_vread(mdb_tgt_t *t, void *buf, size_t nbytes, uintptr_t addr)
{
	ssize_t n;

	/*
	 * If no handle is open yet, reads from virtual addresses are
	 * allowed to succeed but return zero-filled memory.
	 */
	if (t->t_pshandle == NULL) {
		bzero(buf, nbytes);
		return (nbytes);
	}

	if ((n = Pread(t->t_pshandle, buf, nbytes, addr)) <= 0)
		return (set_errno(EMDB_NOMAP));

	return (n);
}

static ssize_t
pt_vwrite(mdb_tgt_t *t, const void *buf, size_t nbytes, uintptr_t addr)
{
	ssize_t n;

	/*
	 * If no handle is open yet, writes to virtual addresses are
	 * allowed to succeed but do not actually modify anything.
	 */
	if (t->t_pshandle == NULL)
		return (nbytes);

	n = Pwrite(t->t_pshandle, buf, nbytes, addr);

	if (n == -1 && errno == EIO)
		return (set_errno(EMDB_NOMAP));

	return (n);
}

static ssize_t
pt_fread(mdb_tgt_t *t, void *buf, size_t nbytes, uintptr_t addr)
{
	pt_data_t *pt = t->t_data;

	if (pt->p_file != NULL) {
		return (mdb_gelf_rw(pt->p_file, buf, nbytes, addr,
		    IOPF_READ(pt->p_fio), GIO_READ));
	}

	bzero(buf, nbytes);
	return (nbytes);
}

static ssize_t
pt_fwrite(mdb_tgt_t *t, const void *buf, size_t nbytes, uintptr_t addr)
{
	pt_data_t *pt = t->t_data;

	if (pt->p_file != NULL) {
		return (mdb_gelf_rw(pt->p_file, (void *)buf, nbytes, addr,
		    IOPF_WRITE(pt->p_fio), GIO_WRITE));
	}

	return (nbytes);
}

static int
pt_lookup_by_name(mdb_tgt_t *t, const char *object,
    const char *name, GElf_Sym *symp)
{
	pt_data_t *pt = t->t_data;

	if (t->t_pshandle != NULL && (object == MDB_TGT_OBJ_EVERY ||
	    Pname_to_map(t->t_pshandle, object) != NULL) &&
	    Plookup_by_name(t->t_pshandle, object, name, symp) == 0)
		return (0);

	if (object == MDB_TGT_OBJ_RTLD)
		return (set_errno(EMDB_NOSYM));

	if (object != MDB_TGT_OBJ_EXEC && object != MDB_TGT_OBJ_EVERY) {
		return (mdb_gelf_symtab_lookup_by_file(pt->p_symtab,
		    object, name, symp));
	}

	if (mdb_gelf_symtab_lookup_by_name(pt->p_symtab, name, symp) == 0 ||
	    mdb_gelf_symtab_lookup_by_name(pt->p_dynsym, name, symp) == 0)
		return (0);

	return (set_errno(EMDB_NOSYM));
}

static int
pt_lookup_by_addr(mdb_tgt_t *t, uintptr_t addr, uint_t flags,
    char *buf, size_t nbytes, GElf_Sym *symp)
{
	pt_data_t *pt = t->t_data;

	const char ptags[] = ":=";
	uintptr_t pltdst = NULL;
	int bound, match, i;

	mdb_gelf_symtab_t *gsts[3];	/* mdb.m_prsym, .symtab, .dynsym */
	int gstc = 0;			/* number of valid gsts[] entries */

	mdb_gelf_symtab_t *gst = NULL;	/* set if 'sym' is from a gst */
	const prmap_t *pmp = NULL;	/* set if 'sym' is from libproc */
	GElf_Sym sym;			/* best symbol found so far if !exact */

	/*
	 * Fill in our array of symbol table pointers with the private symbol
	 * table, static symbol table, and dynamic symbol if applicable.
	 * These are done in order of precedence so that if we match and
	 * MDB_TGT_SYM_EXACT is set, we need not look any further.
	 */
	if (mdb.m_prsym != NULL)
		gsts[gstc++] = mdb.m_prsym;
	if (t->t_pshandle == NULL && pt->p_symtab != NULL)
		gsts[gstc++] = pt->p_symtab;
	if (t->t_pshandle == NULL && pt->p_dynsym != NULL)
		gsts[gstc++] = pt->p_dynsym;

	/*
	 * Loop through our array attempting to match the address.  If we match
	 * and we're in exact mode, we're done.  Otherwise save the symbol in
	 * the local sym variable if it is closer than our previous match.
	 * We explicitly watch for zero-valued symbols since DevPro insists
	 * on storing __fsr_init_value's value as the symbol value instead
	 * of storing it in a constant integer.
	 */
	for (i = 0; i < gstc; i++) {
		if (mdb_gelf_symtab_lookup_by_addr(gsts[i], addr, flags,
		    buf, nbytes, symp) != 0 || symp->st_value == 0)
			continue;

		if (flags & MDB_TGT_SYM_EXACT) {
			gst = gsts[i];
			goto found;
		}

		if (gst == NULL || mdb_gelf_sym_closer(symp, &sym, addr)) {
			gst = gsts[i];
			sym = *symp;
		}
	}

	/*
	 * If we have no libproc handle active, we're done: fail if gst is
	 * NULL; otherwise copy out our best symbol and skip to the end.
	 * We also skip to found if gst is the private symbol table: we
	 * want this to always take precedence over PLT re-vectoring.
	 */
	if (t->t_pshandle == NULL || (gst != NULL && gst == mdb.m_prsym)) {
		if (gst == NULL)
			return (set_errno(EMDB_NOSYMADDR));
		*symp = sym;
		goto found;
	}

	/*
	 * Check to see if the address is in a PLT: if it is, reset addr to
	 * the destination address of the PLT entry, add a special prefix
	 * string to the caller's buf, and forget our previous guess.
	 */
	if ((pltdst = Ppltdest(t->t_pshandle, addr, &bound)) != NULL) {
		(void) mdb_snprintf(buf, nbytes, "PLT%c", ptags[bound]);
		addr = pltdst;
		nbytes -= strlen(buf);
		buf += strlen(buf);
		gst = NULL;
	}

	/*
	 * Ask libproc to convert the address to the closest symbol for us.
	 * Once we get the closest symbol, we perform the EXACT match or
	 * smart-mode or absolute distance check ourself:
	 */
	if (Plookup_by_addr(t->t_pshandle, addr, buf, nbytes, symp) == 0 &&
	    symp->st_value != 0 && (gst == NULL ||
	    mdb_gelf_sym_closer(symp, &sym, addr))) {

		if (flags & MDB_TGT_SYM_EXACT)
			match = (addr == symp->st_value);
		else if (mdb.m_symdist == 0)
			match = (addr >= symp->st_value &&
			    addr < symp->st_value + symp->st_size);
		else
			match = (addr >= symp->st_value &&
			    addr < symp->st_value + mdb.m_symdist);

		if (match) {
			pmp = Paddr_to_map(t->t_pshandle, addr);
			gst = NULL;
			goto found;
		}
	}

	/*
	 * If we get here, Plookup_by_addr has failed us.  If we have no
	 * previous best symbol (gst == NULL), we've failed completely.
	 * Otherwise we copy out that symbol and continue on to 'found'.
	 */
	if (gst == NULL)
		return (set_errno(EMDB_NOSYMADDR));
	*symp = sym;
found:
	/*
	 * Once we've found something, copy the final name into the caller's
	 * buffer and prefix it with the mapping name if appropriate.
	 */
	if (pmp != NULL && pmp != Pname_to_map(t->t_pshandle, PR_OBJ_EXEC)) {
		const char *prefix = pmp->pr_mapname;
		char symname[MDB_TGT_SYM_NAMLEN];
		char objname[MDB_TGT_MAPSZ];

		if (Pobjname(t->t_pshandle, addr, objname, MDB_TGT_MAPSZ))
			prefix = objname;

		(void) strncpy(symname, buf, sizeof (symname));
		symname[MDB_TGT_SYM_NAMLEN - 1] = '\0';

		(void) mdb_snprintf(buf, nbytes, "%s`%s",
		    strbasename(prefix), symname);

	} else if (gst != NULL) {
		(void) strncpy(buf, mdb_gelf_sym_name(gst, symp), nbytes);
		buf[nbytes - 1] = '\0';
	}

	symp->st_other = (pltdst != NULL);
	return (0);
}

/*ARGSUSED*/
static int
pt_objsym_iter(void *arg, const prmap_t *map, const char *object)
{
	pt_symarg_t *psp = arg;

	(void) Psymbol_iter(psp->psym_targ->t_pshandle, object, psp->psym_which,
	    psp->psym_type, psp->psym_func, psp->psym_private);

	return (0);
}

static int
pt_symbol_filt(void *arg, const GElf_Sym *sym, const char *name)
{
	pt_symarg_t *psp = arg;

	if (mdb_tgt_sym_match(sym, psp->psym_type))
		return (psp->psym_func(psp->psym_private, sym, name));

	return (0);
}

static int
pt_symbol_iter(mdb_tgt_t *t, const char *object, uint_t which,
    uint_t type, mdb_tgt_sym_f *func, void *private)
{
	pt_data_t *pt = t->t_data;
	mdb_gelf_symtab_t *gst;
	pt_symarg_t ps;

	ps.psym_targ = t;
	ps.psym_which = which;
	ps.psym_type = type;
	ps.psym_func = func;
	ps.psym_private = private;

	if (t->t_pshandle != NULL) {
		if (object != MDB_TGT_OBJ_EVERY) {
			if (Pname_to_map(t->t_pshandle, object) == NULL)
				return (set_errno(EMDB_NOOBJ));
			(void) Psymbol_iter(t->t_pshandle, object,
			    which, type, func, private);
			return (0);
		} else if (Prd_agent(t->t_pshandle) != NULL) {
			(void) Pobject_iter(t->t_pshandle, pt_objsym_iter, &ps);
			return (0);
		}
	}

	if (object != MDB_TGT_OBJ_EXEC && object != MDB_TGT_OBJ_EVERY)
		return (set_errno(EMDB_NOOBJ));

	if (which == MDB_TGT_SYMTAB)
		gst = pt->p_symtab;
	else
		gst = pt->p_dynsym;

	if (gst != NULL)
		mdb_gelf_symtab_iter(gst, pt_symbol_filt, &ps);

	return (0);
}

static const mdb_map_t *
pt_prmap_to_mdbmap(mdb_tgt_t *t, const prmap_t *prp, mdb_map_t *mp)
{
	if (Pobjname(t->t_pshandle, prp->pr_vaddr,
	    mp->map_name, MDB_TGT_MAPSZ) == NULL) {
		(void) strncpy(mp->map_name, prp->pr_mapname,
		    MDB_TGT_MAPSZ - 1);
		mp->map_name[MDB_TGT_MAPSZ - 1] = '\0';
	}

	mp->map_base = prp->pr_vaddr;
	mp->map_size = prp->pr_size;
	mp->map_flags = 0;

	if (prp->pr_mflags & MA_READ)
		mp->map_flags |= MDB_TGT_MAP_R;
	if (prp->pr_mflags & MA_WRITE)
		mp->map_flags |= MDB_TGT_MAP_W;
	if (prp->pr_mflags & MA_EXEC)
		mp->map_flags |= MDB_TGT_MAP_X;
	if (prp->pr_shmid != -1)
		mp->map_flags |= MDB_TGT_MAP_SHMEM;
	if (prp->pr_mflags & MA_BREAK)
		mp->map_flags |= MDB_TGT_MAP_HEAP;
	if (prp->pr_mflags & MA_STACK)
		mp->map_flags |= MDB_TGT_MAP_STACK;
	if (prp->pr_mflags & MA_ANON)
		mp->map_flags |= MDB_TGT_MAP_ANON;

	return (mp);
}

static int
pt_map_apply(void *arg, const prmap_t *prp, const char *name)
{
	pt_maparg_t *pmp = arg;
	mdb_map_t map;

	return (pmp->pmap_func(pmp->pmap_private,
	    pt_prmap_to_mdbmap(pmp->pmap_targ, prp, &map), name));
}

static int
pt_mapping_iter(mdb_tgt_t *t, mdb_tgt_map_f *func, void *private)
{
	if (t->t_pshandle != NULL) {
		pt_maparg_t pm;

		pm.pmap_targ = t;
		pm.pmap_func = func;
		pm.pmap_private = private;

		(void) Pmapping_iter(t->t_pshandle, pt_map_apply, &pm);
		return (0);
	}

	return (set_errno(EMDB_NOPROC));
}

static int
pt_object_iter(mdb_tgt_t *t, mdb_tgt_map_f *func, void *private)
{
	if (t->t_pshandle != NULL) {
		pt_maparg_t pm;

		pm.pmap_targ = t;
		pm.pmap_func = func;
		pm.pmap_private = private;

		(void) Pobject_iter(t->t_pshandle, pt_map_apply, &pm);
		return (0);
	}

	return (set_errno(EMDB_NOPROC));
}

static const mdb_map_t *
pt_addr_to_map(mdb_tgt_t *t, uintptr_t addr)
{
	pt_data_t *pt = t->t_data;
	const prmap_t *pmp;

	if (t->t_pshandle == NULL) {
		(void) set_errno(EMDB_NOPROC);
		return (NULL);
	}

	if ((pmp = Paddr_to_map(t->t_pshandle, addr)) == NULL) {
		(void) set_errno(EMDB_NOMAP);
		return (NULL);
	}

	return (pt_prmap_to_mdbmap(t, pmp, &pt->p_map));
}

static const mdb_map_t *
pt_name_to_map(mdb_tgt_t *t, const char *name)
{
	pt_data_t *pt = t->t_data;
	const prmap_t *pmp;

	if (t->t_pshandle == NULL) {
		(void) set_errno(EMDB_NOPROC);
		return (NULL);
	}

	if ((pmp = Pname_to_map(t->t_pshandle, name)) == NULL) {
		(void) set_errno(EMDB_NOOBJ);
		return (NULL);
	}

	return (pt_prmap_to_mdbmap(t, pmp, &pt->p_map));
}

static int
pt_getareg(mdb_tgt_t *t, mdb_tgt_tid_t tid,
    const char *rname, mdb_tgt_reg_t *rp)
{
	const mdb_tgt_regdesc_t *rdp;
	pt_data_t *pt = t->t_data;
	lwpid_t lid = (lwpid_t)(uintptr_t)tid;
	prgregset_t grs;

	for (rdp = pt->p_rds; rdp->rd_name != NULL; rdp++) {
		if (strcmp(rname, rdp->rd_name) == 0) {
			/*
			 * Avoid sign-extension by casting: recall that procfs
			 * defines prgreg_t as a long or int and our native
			 * register handling uses uint64_t's.
			 */
			if (Plwp_getregs(t->t_pshandle, lid, grs) == 0) {
				*rp = (ulong_t)grs[rdp->rd_num];
				return (0);
			}
			return (-1);
		}
	}

	return (set_errno(EMDB_BADREG));
}

static int
pt_putareg(mdb_tgt_t *t, mdb_tgt_tid_t tid, const char *rname, mdb_tgt_reg_t r)
{
	const mdb_tgt_regdesc_t *rdp;
	pt_data_t *pt = t->t_data;
	lwpid_t lid = (lwpid_t)(uintptr_t)tid;
	prgregset_t grs;

	for (rdp = pt->p_rds; rdp->rd_name != NULL; rdp++) {
		if (strcmp(rname, rdp->rd_name) == 0) {
			if (Plwp_getregs(t->t_pshandle, lid, grs) == 0) {
				grs[rdp->rd_num] = (prgreg_t)r;
				return (Plwp_setregs(t->t_pshandle, lid, grs));
			}
			return (-1);
		}
	}

	return (set_errno(EMDB_BADREG));
}

static int
pt_stack_call(pt_stkarg_t *psp, const prgregset_t grs, uint_t argc, long *argv)
{
	return (psp->pstk_func(psp->pstk_private, grs[R_PC],
	    argc, argv, (const struct mdb_tgt_gregset *)grs));
}

static int
pt_stack_iter(mdb_tgt_t *t, const mdb_tgt_gregset_t *gsp,
    mdb_tgt_stack_f *func, void *arg)
{
	if (t->t_pshandle != NULL) {
		pt_stkarg_t pstk;

		pstk.pstk_func = func;
		pstk.pstk_private = arg;

		(void) Pstack_iter(t->t_pshandle, gsp->gregs,
		    (proc_stack_f *)pt_stack_call, &pstk);

		return (0);
	}

	return (set_errno(EMDB_NOPROC));
}

static const mdb_tgt_ops_t proc_ops = {
	pt_setflags,				/* t_setflags */
	(int (*)()) mdb_tgt_notsup,		/* t_setcontext */
	pt_activate,				/* t_activate */
	pt_deactivate,				/* t_deactivate */
	pt_destroy,				/* t_destroy */
	pt_name,				/* t_name */
	(const char *(*)()) mdb_conf_isa,	/* t_isa */
	pt_platform,				/* t_platform */
	pt_uname,				/* t_uname */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_aread */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_awrite */
	pt_vread,				/* t_vread */
	pt_vwrite,				/* t_vwrite */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_pread */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_pwrite */
	pt_fread,				/* t_fread */
	pt_fwrite,				/* t_fwrite */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_ioread */
	(ssize_t (*)()) mdb_tgt_notsup,		/* t_iowrite */
	(int (*)()) mdb_tgt_notsup,		/* t_vtop */
	pt_lookup_by_name,			/* t_lookup_by_name */
	pt_lookup_by_addr,			/* t_lookup_by_addr */
	pt_symbol_iter,				/* t_symbol_iter */
	pt_mapping_iter,			/* t_mapping_iter */
	pt_object_iter,				/* t_object_iter */
	pt_addr_to_map,				/* t_addr_to_map */
	pt_name_to_map,				/* t_name_to_map */
	(int (*)()) mdb_tgt_notsup,		/* t_thread_iter XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_cpu_iter XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_thr_status XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_cpu_status XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_status XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_run XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_step XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_continue XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_call XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_add_brkpt XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_add_pwapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_vwapt XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_add_iowapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_ixwapt */
	(int (*)()) mdb_tgt_notsup,		/* t_add_sysenter XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_add_sysexit XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_add_signal XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_add_object_load XXX */
	(int (*)()) mdb_tgt_notsup,		/* t_add_object_unload XXX */
	pt_getareg,				/* t_getareg */
	pt_putareg,				/* t_putareg */
	pt_stack_iter				/* t_stack_iter */
};

static ssize_t
pt_xd_auxv(mdb_tgt_t *t, void *buf, size_t nbytes)
{
	const auxv_t *auxp, *auxv = NULL;
	int auxn = 0;

	if (t->t_pshandle != NULL && ps_pauxv(t->t_pshandle, &auxv) == PS_OK &&
	    auxv != NULL && auxv->a_type != AT_NULL) {
		for (auxp = auxv, auxn = 1; auxp->a_type != NULL; auxp++)
			auxn++;
	}

	if (buf == NULL && nbytes == 0)
		return (sizeof (auxv_t) * auxn);

	if (auxn == 0)
		return (set_errno(ENODATA));

	nbytes = MIN(nbytes, sizeof (auxv_t) * auxn);
	bcopy(auxv, buf, nbytes);
	return (nbytes);
}

static ssize_t
pt_xd_cred(mdb_tgt_t *t, void *buf, size_t nbytes)
{
	prcred_t cr, *crp;
	size_t cbytes = 0;

	if (t->t_pshandle != NULL && Pcred(t->t_pshandle, &cr, 1) == 0) {
		cbytes = (cr.pr_ngroups <= 1) ? sizeof (prcred_t) :
		    (sizeof (prcred_t) + (cr.pr_ngroups - 1) * sizeof (gid_t));
	}

	if (buf == NULL && nbytes == 0)
		return (cbytes);

	if (cbytes == 0)
		return (set_errno(ENODATA));

	crp = mdb_alloc(cbytes, UM_SLEEP);

	if (Pcred(t->t_pshandle, crp, cr.pr_ngroups) == -1)
		return (set_errno(ENODATA));

	nbytes = MIN(nbytes, cbytes);
	bcopy(crp, buf, nbytes);
	mdb_free(crp, cbytes);
	return (nbytes);
}

static int
pt_copy_lwp(lwpstatus_t **lspp, const lwpstatus_t *lsp)
{
	bcopy(lsp, *lspp, sizeof (lwpstatus_t));
	(*lspp)++;
	return (0);
}

static ssize_t
pt_xd_lwpstatus(mdb_tgt_t *t, void *buf, size_t nbytes)
{
	lwpstatus_t *lsp, *lbuf;
	const pstatus_t *psp;
	int nlwp = 0;

	if (t->t_pshandle != NULL && (psp = Pstatus(t->t_pshandle)) != NULL)
		nlwp = psp->pr_nlwp;

	if (buf == NULL && nbytes == 0)
		return (sizeof (lwpstatus_t) * nlwp);

	if (nlwp == 0)
		return (set_errno(ENODATA));

	lsp = lbuf = mdb_alloc(sizeof (lwpstatus_t) * nlwp, UM_SLEEP);
	nbytes = MIN(nbytes, sizeof (lwpstatus_t) * nlwp);

	(void) Plwp_iter(t->t_pshandle, (proc_lwp_f *)pt_copy_lwp, &lsp);
	bcopy(lbuf, buf, nbytes);

	mdb_free(lbuf, sizeof (lwpstatus_t) * nlwp);
	return (nbytes);
}

static ssize_t
pt_xd_pshandle(mdb_tgt_t *t, void *buf, size_t nbytes)
{
	if (buf == NULL && nbytes == 0)
		return (sizeof (struct ps_prochandle *));

	if (t->t_pshandle == NULL || nbytes != sizeof (struct ps_prochandle *))
		return (set_errno(ENODATA));

	bcopy(&t->t_pshandle, buf, nbytes);
	return (nbytes);
}

static ssize_t
pt_xd_psinfo(mdb_tgt_t *t, void *buf, size_t nbytes)
{
	const psinfo_t *psp;

	if (buf == NULL && nbytes == 0)
		return (sizeof (psinfo_t));

	if (t->t_pshandle == NULL || (psp = Ppsinfo(t->t_pshandle)) == NULL)
		return (set_errno(ENODATA));

	nbytes = MIN(nbytes, sizeof (psinfo_t));
	bcopy(psp, buf, nbytes);
	return (nbytes);
}

static ssize_t
pt_xd_pstatus(mdb_tgt_t *t, void *buf, size_t nbytes)
{
	const pstatus_t *psp;

	if (buf == NULL && nbytes == 0)
		return (sizeof (pstatus_t));

	if (t->t_pshandle == NULL || (psp = Pstatus(t->t_pshandle)) == NULL)
		return (set_errno(ENODATA));

	nbytes = MIN(nbytes, sizeof (pstatus_t));
	bcopy(psp, buf, nbytes);
	return (nbytes);
}

static ssize_t
pt_xd_utsname(mdb_tgt_t *t, void *buf, size_t nbytes)
{
	struct utsname uts;

	if (buf == NULL && nbytes == 0)
		return (sizeof (struct utsname));

	if (t->t_pshandle == NULL || Puname(t->t_pshandle, &uts) != 0)
		return (set_errno(ENODATA));

	nbytes = MIN(nbytes, sizeof (struct utsname));
	bcopy(&uts, buf, nbytes);
	return (nbytes);
}

int
mdb_proc_tgt_create(mdb_tgt_t *t, int argc, const char *argv[])
{
	pt_data_t *pt = mdb_zalloc(sizeof (pt_data_t), UM_SLEEP);

	const char *aout_path = argc > 0 ? argv[0] : PT_EXEC_PATH;
	const char *core_path = argc > 1 ? argv[1] : NULL;

	char execname[MAXPATHLEN];
	int perr;

	if (t->t_flags & MDB_TGT_F_RDWR)
		pt->p_oflags = O_RDWR;
	else
		pt->p_oflags = O_RDONLY;

	if (t->t_flags & MDB_TGT_F_FORCE)
		pt->p_gflags |= PGRAB_FORCE;

	t->t_ops = &proc_ops;
	t->t_data = pt;

	if (argc > 2)
		return (set_errno(EINVAL));

	/*
	 * If no core file name was specified, but the file ./core is present,
	 * infer that we want to debug it.  I find this behavior confusing,
	 * so we only do this when precise adb(1) compatibility is required.
	 */
	if (core_path == NULL && (mdb.m_flags & MDB_FL_ADB) &&
	    access(PT_CORE_PATH, F_OK) == 0)
		core_path = PT_CORE_PATH;

	/*
	 * If a core file or pid was specified, attempt to grab it now using
	 * proc_arg_grab(); otherwise we'll create a fresh process later.
	 */
	if (core_path != NULL && (t->t_pshandle = proc_arg_grab(core_path,
	    PR_ARG_ANY, pt->p_gflags, &perr)) == NULL) {
		mdb_warn("cannot debug %s: %s\n", core_path, Pgrab_error(perr));
		goto err;
	}

	/*
	 * If we don't have an executable path but we now have a libproc
	 * handle, attempt to derive the executable path using Pexecname():
	 */
	if (aout_path == NULL && t->t_pshandle != NULL) {
		aout_path = Pexecname(t->t_pshandle, execname, MAXPATHLEN);
		if (aout_path == NULL) {
			mdb_warn("warning: failed to infer pathname to "
			    "executable; symbol table will not be available\n");
		}
	}

	/*
	 * Attempt to open the executable file.  We only want this operation
	 * to actually cause the constructor to abort if the executable file
	 * name was given explicitly.  If we defaulted to PT_EXEC_PATH or
	 * derived the executable using Pexecname, then we want to continue
	 * along with p_fio and p_file set to NULL.
	 */
	if (aout_path != NULL && (pt->p_fio = mdb_fdio_create_path(NULL,
	    aout_path, pt->p_oflags, 0)) == NULL && argc > 0) {
		mdb_warn("failed to open %s", aout_path);
		goto err;
	}

	/*
	 * Now create an ELF file from the input file, if we have one.  Again,
	 * only abort the constructor if the name was given explicitly.
	 */
	if (pt->p_fio != NULL && (pt->p_file = mdb_gelf_create(pt->p_fio,
	    ET_NONE, GF_FILE)) == NULL) {
		mdb_io_destroy(pt->p_fio);
		pt->p_fio = NULL;
		if (argc > 0)
			goto err;
	}

	/*
	 * If we've successfully opened an ELF file, attempt to load its
	 * static and dynamic symbol tables, if either are present, and
	 * then select the appropriate disassembler based on the ELF header.
	 */
	if (pt->p_file != NULL) {
		pt->p_symtab = mdb_gelf_symtab_create_file(pt->p_file,
		    ".symtab", ".strtab");
		pt->p_dynsym = mdb_gelf_symtab_create_file(pt->p_file,
		    ".dynsym", ".dynstr");
		(void) mdb_dis_select(pt_disasm(&pt->p_file->gf_ehdr));
	} else
		(void) mdb_dis_select(pt_disasm(NULL));

	/*
	 * For now we only have one register description list: we keep a
	 * pointer inside the target in case that needs to change later.
	 */
	pt->p_rds = pt_regdesc;

	/*
	 * Certain important /proc structures may be of interest to mdb
	 * modules and their dcmds.  Export these using the xdata interface:
	 */
	(void) mdb_tgt_xdata_insert(t, "auxv",
	    "procfs auxv_t array", pt_xd_auxv);
	(void) mdb_tgt_xdata_insert(t, "cred",
	    "procfs prcred_t structure", pt_xd_cred);
	(void) mdb_tgt_xdata_insert(t, "lwpstatus",
	    "procfs lwpstatus_t array", pt_xd_lwpstatus);
	(void) mdb_tgt_xdata_insert(t, "pshandle",
	    "libproc proc service API handle", pt_xd_pshandle);
	(void) mdb_tgt_xdata_insert(t, "psinfo",
	    "procfs psinfo_t structure", pt_xd_psinfo);
	(void) mdb_tgt_xdata_insert(t, "pstatus",
	    "procfs pstatus_t structure", pt_xd_pstatus);
	(void) mdb_tgt_xdata_insert(t, "utsname",
	    "utsname structure", pt_xd_utsname);

	/*
	 * If adb(1) compatibility mode is on, then print the appropriate
	 * greeting message if we have grabbed a core file.
	 */
	if ((mdb.m_flags & MDB_FL_ADB) && t->t_pshandle != NULL &&
	    Pstate(t->t_pshandle) == PS_DEAD) {
		const pstatus_t *psp = Pstatus(t->t_pshandle);
		int cursig = psp->pr_lwp.pr_cursig;
		char signame[SIG2STR_MAX];

		mdb_printf("core file = %s -- program ``%s'' on platform %s\n",
		    core_path, aout_path ? aout_path : "?", pt_platform(t));

		if (cursig != 0 && sig2str(cursig, signame) == 0)
			mdb_printf("SIG%s: %s\n", signame, strsignal(cursig));
	}

	return (0);

err:
	pt_destroy(t);
	return (-1);
}
