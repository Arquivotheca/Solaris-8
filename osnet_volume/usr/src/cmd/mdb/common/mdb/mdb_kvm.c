/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_kvm.c	1.2	99/11/19 SMI"

/*
 * Libkvm Kernel Target
 *
 * The libkvm kernel target provides access to both crash dumps and live
 * kernels through /dev/ksyms and /dev/kmem, using the facilities provided by
 * the libkvm.so library.  The target-specific data structures are shared
 * between this file (common code) and the ISA-dependent parts of the target,
 * and so they are defined in the mdb_kvm.h header.  The target processes an
 * "executable" (/dev/ksyms or the unix.X file) which contains a primary
 * .symtab and .dynsym, and then also iterates over the krtld module chain in
 * the kernel in order to obtain a list of loaded modules and per-module symbol
 * tables.  To improve startup performance, the per-module symbol tables are
 * instantiated on-the-fly whenever an address lookup falls within the text
 * section of a given module.  The target also relies on services from the
 * mdb_ks (kernel support) module, which contains pieces of the implementation
 * that must be compiled against the kernel implementation.
 */

#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>
#include <sys/utsname.h>
#include <sys/panic.h>

#include <dlfcn.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <mdb/mdb_target_impl.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_io_impl.h>
#include <mdb/mdb_kvm.h>
#include <mdb/mdb.h>

#define	KT_RELOC_BUF(buf, obase, nbase) \
	((uintptr_t)(buf) - (uintptr_t)(obase) + (uintptr_t)(nbase))

#define	KT_BAD_BUF(buf, base, size) \
	((uintptr_t)(buf) < (uintptr_t)(base) || \
	((uintptr_t)(buf) >= (uintptr_t)(base) + (uintptr_t)(size)))

typedef struct kt_symarg {
	mdb_tgt_sym_f *sym_cb;		/* Caller's callback function */
	void *sym_data;			/* Callback function argument */
	uint_t sym_type;		/* Symbol type/binding filter */
} kt_symarg_t;

typedef struct kt_maparg {
	mdb_tgt_t *map_target;		/* Target used for mapping iter */
	mdb_tgt_map_f *map_cb;		/* Caller's callback function */
	void *map_data;			/* Callback function argument */
} kt_maparg_t;

static const char KT_RTLD_NAME[] = "krtld";
static const char KT_MODULE[] = "mdb_ks";

static void
kt_load_module(kt_data_t *kt, mdb_tgt_t *t, kt_module_t *km)
{
	km->km_data = mdb_alloc(km->km_datasz, UM_SLEEP);

	(void) mdb_tgt_vread(t, km->km_data, km->km_datasz, km->km_symspace_va);

	km->km_symbuf = (void *)
	    KT_RELOC_BUF(km->km_symtab_va, km->km_symspace_va, km->km_data);

	km->km_strtab = (char *)
	    KT_RELOC_BUF(km->km_strtab_va, km->km_symspace_va, km->km_data);

	km->km_symtab = mdb_gelf_symtab_create_raw(&kt->k_file->gf_ehdr,
	    &km->km_symtab_hdr, km->km_symbuf,
	    &km->km_strtab_hdr, km->km_strtab);
}

static void
kt_load_modules(kt_data_t *kt, mdb_tgt_t *t)
{
	char name[MAXNAMELEN];
	uintptr_t addr, head;

	struct module kmod;
	struct modctl ctl;
	Shdr symhdr, strhdr;
	GElf_Sym sym;

	kt_module_t *km;

	if (mdb_tgt_lookup_by_name(t, MDB_TGT_OBJ_EXEC,
	    "modules", &sym) == -1) {
		warn("failed to get 'modules' symbol");
		return;
	}

	if (mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &ctl, sizeof (ctl),
	    MDB_TGT_OBJ_EXEC, "modules") != sizeof (ctl)) {
		warn("failed to read 'modules' struct");
		return;
	}

	addr = head = (uintptr_t)sym.st_value;

	do {
		if (addr == NULL)
			break; /* Avoid spurious NULL pointers in list */

		if (mdb_tgt_vread(t, &ctl, sizeof (ctl), addr) == -1) {
			warn("failed to read modctl at %p", (void *)addr);
			return;
		}

		if (ctl.mod_mp == NULL)
			continue; /* No associated krtld structure */

		if (mdb_tgt_readstr(t, MDB_TGT_AS_VIRT, name, MAXNAMELEN,
		    (uintptr_t)ctl.mod_modname) <= 0) {
			warn("failed to read module name at %p",
			    (void *)ctl.mod_modname);
			continue;
		}

		mdb_dprintf(MDB_DBG_KMOD, "reading mod %s (%p)\n",
		    name, (void *)addr);

		if (mdb_nv_lookup(&kt->k_modules, name) != NULL) {
			warn("skipping duplicate module '%s', id=%d\n",
			    name, ctl.mod_id);
			continue;
		}

		if (mdb_tgt_vread(t, &kmod, sizeof (kmod),
		    (uintptr_t)ctl.mod_mp) == -1) {
			warn("failed to read module at %p\n",
			    (void *)ctl.mod_mp);
			continue;
		}

		if (kmod.symspace == NULL || kmod.symhdr == NULL ||
		    kmod.strhdr == NULL) {
			/*
			 * If no buffer for the symbols has been allocated,
			 * or the shdrs for .symtab and .strtab are missing,
			 * then we're out of luck.
			 */
			continue;
		}

		if (mdb_tgt_vread(t, &symhdr, sizeof (Shdr),
		    (uintptr_t)kmod.symhdr) == -1) {
			warn("failed to read .symtab header for '%s', id=%d",
			    name, ctl.mod_id);
			continue;
		}

		if (mdb_tgt_vread(t, &strhdr, sizeof (Shdr),
		    (uintptr_t)kmod.strhdr) == -1) {
			warn("failed to read .strtab header for '%s', id=%d",
			    name, ctl.mod_id);
			continue;
		}

		/*
		 * Now get clever: f(*^ing krtld didn't used to bother updating
		 * its own kmod.symsize value.  We know that prior to this bug
		 * being fixed, symspace was a contiguous buffer containing
		 * .symtab, .strtab, and the symbol hash table in that order.
		 * So if symsize is zero, recompute it as the size of .symtab
		 * plus the size of .strtab.  We don't need to load the hash
		 * table anyway since we re-hash all the symbols internally.
		 */
		if (kmod.symsize == 0)
			kmod.symsize = symhdr.sh_size + strhdr.sh_size;

		/*
		 * Similar logic can be used to make educated guesses
		 * at the values of kmod.symtbl and kmod.strings.
		 */
		if (kmod.symtbl == NULL)
			kmod.symtbl = kmod.symspace;
		if (kmod.strings == NULL)
			kmod.strings = kmod.symspace + symhdr.sh_size;

		/*
		 * Make sure things seem reasonable before we proceed
		 * to actually read and decipher the symspace.
		 */
		if (KT_BAD_BUF(kmod.symtbl, kmod.symspace, kmod.symsize) ||
		    KT_BAD_BUF(kmod.strings, kmod.symspace, kmod.symsize)) {
			warn("skipping module '%s', id=%d (corrupt symspace)\n",
			    name, ctl.mod_id);
			continue;
		}

		km = mdb_zalloc(sizeof (kt_module_t), UM_SLEEP);
		km->km_name = strdup(name);

		(void) mdb_nv_insert(&kt->k_modules, km->km_name, NULL,
		    (uintptr_t)km, MDB_NV_EXTNAME);

		km->km_datasz = kmod.symsize;
		km->km_symspace_va = (uintptr_t)kmod.symspace;
		km->km_symtab_va = (uintptr_t)kmod.symtbl;
		km->km_strtab_va = (uintptr_t)kmod.strings;
		km->km_symtab_hdr = symhdr;
		km->km_strtab_hdr = strhdr;
		km->km_text_va = (uintptr_t)kmod.text;
		km->km_text_size = kmod.text_size;
		km->km_data_size = kmod.data_size;

		/*
		 * Add the module to the end of the list of modules in load-
		 * dependency order.  This is needed to load the corresponding
		 * debugger modules in the same order for layering purposes.
		 */
		mdb_list_append(&kt->k_modlist, km);

		if (t->t_flags & MDB_TGT_F_PRELOAD) {
			mdb_iob_printf(mdb.m_out, " %s", name);
			mdb_iob_flush(mdb.m_out);
			kt_load_module(kt, t, km);
		}

	} while ((addr = (uintptr_t)ctl.mod_next) != head);
}

int
kt_setflags(mdb_tgt_t *t, int flags)
{
	if (flags & MDB_TGT_F_RDWR) {
		kt_data_t *kt = t->t_data;
		kvm_t *cookie = kvm_open(kt->k_symfile, kt->k_kvmfile,
		    NULL, O_RDWR, mdb.m_pname);

		if (cookie != NULL) {
			t->t_flags |= MDB_TGT_F_RDWR;
			(void) kvm_close(kt->k_cookie);
			kt->k_cookie = cookie;
		} else
			return (-1);
	}

	return (0);
}

int
kt_setcontext(mdb_tgt_t *t, void *context)
{
	if (context != NULL) {
		const char *argv[2];
		int argc = 0;
		mdb_tgt_t *ct;

		argv[argc++] = (const char *)context;
		argv[argc] = NULL;

		if ((ct = mdb_tgt_create(mdb_kproc_tgt_create,
		    t->t_flags, argc, argv)) == NULL)
			return (-1);

		mdb_printf("debugger context set to proc %p\n", context);
		mdb_tgt_activate(ct);
	} else
		mdb_printf("debugger context set to kernel\n");

	return (0);
}

static int
kt_stack(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kt_data_t *kt = mdb.m_target->t_data;
	return (kt->k_dcmd_stack(addr, flags, argc, argv));
}

static int
kt_stackv(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kt_data_t *kt = mdb.m_target->t_data;
	return (kt->k_dcmd_stackv(addr, flags, argc, argv));
}

static int
kt_regs(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kt_data_t *kt = mdb.m_target->t_data;
	return (kt->k_dcmd_regs(addr, flags, argc, argv));
}

/*ARGSUSED*/
static int
kt_status(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kt_data_t *kt = mdb.m_target->t_data;
	struct utsname uts;
	char buf[BUFSIZ];
	panic_data_t pd;
	GElf_Sym sym;

	bzero(&uts, sizeof (uts));
	(void) strcpy(uts.nodename, "unknown machine");
	(void) kt_uname(mdb.m_target, &uts);

	if (mdb_prop_postmortem) {
		mdb_printf("debugging crash dump %s (%d-bit) from %s\n",
		    kt->k_kvmfile, (int)(sizeof (void *) * NBBY), uts.nodename);
	} else {
		mdb_printf("debugging live kernel (%d-bit) on %s\n",
		    (int)(sizeof (void *) * NBBY), uts.nodename);
	}

	mdb_printf("operating system: %s %s (%s)\n",
	    uts.release, uts.version, uts.machine);

	if (mdb_prop_postmortem && mdb_lookup_by_name("panicbuf", &sym) == 0 &&
	    mdb_vread(&pd, sizeof (pd), sym.st_value) != -1 &&
	    pd.pd_version == PANICBUFVERS && mdb_readstr(buf, sizeof (buf),
	    sym.st_value + pd.pd_msgoff) > 0)
		mdb_printf("panic message: %s\n", buf);

	return (DCMD_OK);
}

static const mdb_dcmd_t kt_dcmds[] = {
	{ "$c", "[cnt]", "print stack backtrace", kt_stack },
	{ "$C", "[cnt]", "print stack backtrace", kt_stackv },
	{ "$r", NULL, "print general-purpose registers", kt_regs },
	{ "$?", NULL, "print status and registers", kt_regs },
	{ "regs", NULL, "print general-purpose registers", kt_regs },
	{ "stack", NULL, "print stack backtrace", kt_stack },
	{ "status", NULL, "print summary of current target", kt_status },
	{ NULL }
};

static uintmax_t
reg_disc_get(const mdb_var_t *v)
{
	mdb_tgt_t *t = MDB_NV_COOKIE(v);
	kt_data_t *kt = t->t_data;
	mdb_tgt_reg_t r = 0;

	(void) mdb_tgt_getareg(t, kt->k_tid, mdb_nv_get_name(v), &r);
	return (r);
}

void
kt_activate(mdb_tgt_t *t)
{
	static const mdb_nv_disc_t reg_disc = { NULL, reg_disc_get };
	kt_data_t *kt = t->t_data;

	const mdb_tgt_regdesc_t *rdp;
	const mdb_dcmd_t *dcp;
	int oflag;

	mdb_prop_postmortem = (strcmp(kt->k_symfile, "/dev/ksyms") != 0);
	mdb_prop_kernel = TRUE;
	mdb_prop_datamodel = MDB_TGT_MODEL_NATIVE;

	if (kt->k_activated == FALSE) {
		struct utsname u1, u2;
		/*
		 * If we're examining a crash dump, root is /, and uname(2)
		 * does not match the utsname in the dump, issue a warning.
		 * Note that we are assuming that the modules and macros in
		 * /usr/lib are compiled against the kernel from uname -rv.
		 */
		if (mdb_prop_postmortem && strcmp(mdb.m_root, "/") == 0 &&
		    uname(&u1) >= 0 && kt_uname(t, &u2) >= 0 &&
		    (strcmp(u1.release, u2.release) ||
		    strcmp(u1.version, u2.version))) {
			mdb_warn("warning: dump is from %s %s %s; dcmds and "
			    "macros may not match kernel implementation\n",
			    u2.sysname, u2.release, u2.version);
		}

		if (mdb_module_load(KT_MODULE, MDB_MOD_GLOBAL) == NULL) {
			warn("failed to load kernel support module -- "
			    "some modules may not load\n");
		}

		if (t->t_flags & MDB_TGT_F_PRELOAD) {
			oflag = mdb_iob_getflags(mdb.m_out) & MDB_IOB_PGENABLE;

			mdb_iob_clrflags(mdb.m_out, oflag);
			mdb_iob_puts(mdb.m_out, "Preloading module symbols: [");
			mdb_iob_flush(mdb.m_out);
		}

		if (!(t->t_flags & MDB_TGT_F_NOLOAD))
			kt_load_modules(kt, t);

		if (t->t_flags & MDB_TGT_F_PRELOAD) {
			mdb_iob_puts(mdb.m_out, " ]\n");
			mdb_iob_setflags(mdb.m_out, oflag);
		}

		kt->k_activated = TRUE;
	}

	/*
	 * Export our target-specific dcmds.  These are interposed on
	 * top of the default mdb dcmds of the same name.
	 */
	for (dcp = &kt_dcmds[0]; dcp->dc_name != NULL; dcp++) {
		if (mdb_module_add_dcmd(t->t_module, dcp, MDB_MOD_FORCE) == -1)
			warn("failed to add dcmd %s", dcp->dc_name);
	}

	/*
	 * Iterate through the register description list filled in by our
	 * platform component and export each as a named variable.
	 */
	for (rdp = kt->k_rds; rdp->rd_name != NULL; rdp++) {
		if (!(rdp->rd_flags & MDB_TGT_R_EXPORT))
			continue; /* Don't export register as a variable */

		(void) mdb_nv_insert(&mdb.m_nv, rdp->rd_name, &reg_disc,
		    (uintptr_t)t, MDB_NV_PERSIST | MDB_NV_RDONLY);
	}

	mdb_tgt_elf_export(kt->k_file);
}

void
kt_deactivate(mdb_tgt_t *t)
{
	kt_data_t *kt = t->t_data;

	const mdb_tgt_regdesc_t *rdp;
	const mdb_dcmd_t *dcp;

	for (rdp = kt->k_rds; rdp->rd_name != NULL; rdp++) {
		mdb_var_t *v;

		if (!(rdp->rd_flags & MDB_TGT_R_EXPORT))
			continue; /* Didn't export register as a variable */

		if ((v = mdb_nv_lookup(&mdb.m_nv, rdp->rd_name)) != NULL) {
			v->v_flags &= ~MDB_NV_PERSIST;
			mdb_nv_remove(&mdb.m_nv, v);
		}
	}

	for (dcp = &kt_dcmds[0]; dcp->dc_name != NULL; dcp++) {
		if (mdb_module_remove_dcmd(t->t_module, dcp->dc_name) == -1)
			warn("failed to remove dcmd %s", dcp->dc_name);
	}

	mdb_prop_postmortem = FALSE;
	mdb_prop_kernel = FALSE;
	mdb_prop_datamodel = MDB_TGT_MODEL_UNKNOWN;
}

/*ARGSUSED*/
const char *
kt_name(mdb_tgt_t *t)
{
	return ("kvm");
}

const char *
kt_platform(mdb_tgt_t *t)
{
	kt_data_t *kt = t->t_data;
	return (kt->k_platform);
}

int
kt_uname(mdb_tgt_t *t, struct utsname *utsp)
{
	return (mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, utsp,
	    sizeof (struct utsname), MDB_TGT_OBJ_EXEC, "utsname"));
}

ssize_t
kt_aread(mdb_tgt_t *t, mdb_tgt_as_t as, void *buf,
    size_t nbytes, mdb_tgt_addr_t addr)
{
	kt_data_t *kt = t->t_data;
	ssize_t rval;

	if ((rval = kt->k_aread(kt->k_cookie, addr, buf, nbytes, as)) == -1)
		return (set_errno(EMDB_NOMAP));

	return (rval);
}

ssize_t
kt_awrite(mdb_tgt_t *t, mdb_tgt_as_t as, const void *buf,
    size_t nbytes, mdb_tgt_addr_t addr)
{
	kt_data_t *kt = t->t_data;
	ssize_t rval;

	if ((rval = kt->k_awrite(kt->k_cookie, addr, buf, nbytes, as)) == -1)
		return (set_errno(EMDB_NOMAP));

	return (rval);
}

ssize_t
kt_vread(mdb_tgt_t *t, void *buf, size_t nbytes, uintptr_t addr)
{
	kt_data_t *kt = t->t_data;
	ssize_t rval;

	if ((rval = kvm_kread(kt->k_cookie, addr, buf, nbytes)) == -1)
		return (set_errno(EMDB_NOMAP));

	return (rval);
}

ssize_t
kt_vwrite(mdb_tgt_t *t, const void *buf, size_t nbytes, uintptr_t addr)
{
	kt_data_t *kt = t->t_data;
	ssize_t rval;

	if ((rval = kvm_kwrite(kt->k_cookie, addr, buf, nbytes)) == -1)
		return (set_errno(EMDB_NOMAP));

	return (rval);
}

ssize_t
kt_fread(mdb_tgt_t *t, void *buf, size_t nbytes, uintptr_t addr)
{
	return (kt_vread(t, buf, nbytes, addr));
}

ssize_t
kt_fwrite(mdb_tgt_t *t, const void *buf, size_t nbytes, uintptr_t addr)
{
	return (kt_vwrite(t, buf, nbytes, addr));
}

ssize_t
kt_pread(mdb_tgt_t *t, void *buf, size_t nbytes, physaddr_t addr)
{
	kt_data_t *kt = t->t_data;
	ssize_t rval;

	if ((rval = kt->k_pread(kt->k_cookie, addr, buf, nbytes)) == -1)
		return (set_errno(EMDB_NOMAP));

	return (rval);
}

ssize_t
kt_pwrite(mdb_tgt_t *t, const void *buf, size_t nbytes, physaddr_t addr)
{
	kt_data_t *kt = t->t_data;
	ssize_t rval;

	if ((rval = kt->k_pwrite(kt->k_cookie, addr, buf, nbytes)) == -1)
		return (set_errno(EMDB_NOMAP));

	return (rval);
}

int
kt_vtop(mdb_tgt_t *t, mdb_tgt_as_t as, uintptr_t va, physaddr_t *pap)
{
	kt_data_t *kt = t->t_data;

	struct as *asp;
	physaddr_t pa;

	switch ((uintptr_t)as) {
	case (uintptr_t)MDB_TGT_AS_PHYS:
	case (uintptr_t)MDB_TGT_AS_FILE:
	case (uintptr_t)MDB_TGT_AS_IO:
		return (set_errno(EINVAL));
	case (uintptr_t)MDB_TGT_AS_VIRT:
		asp = kt->k_as;
		break;
	default:
		asp = (struct as *)as;
	}

	if ((pa = kvm_physaddr(kt->k_cookie, asp, va)) != -1ULL) {
		*pap = pa;
		return (0);
	}

	return (set_errno(EMDB_NOMAP));
}

int
kt_lookup_by_name(mdb_tgt_t *t, const char *obj, const char *name, GElf_Sym *s)
{
	kt_data_t *kt = t->t_data;
	kt_module_t *km, kmod;
	mdb_var_t *v;
	int n;

	/*
	 * To simplify the implementation, we create a fake module on the stack
	 * which is "prepended" to k_modlist and whose symtab is kt->k_symtab.
	 */
	kmod.km_symtab = kt->k_symtab;
	kmod.km_list.ml_next = mdb_list_next(&kt->k_modlist);

	switch ((uintptr_t)obj) {
	case (uintptr_t)MDB_TGT_OBJ_EXEC:
		km = &kmod;
		n = 1;
		break;

	case (uintptr_t)MDB_TGT_OBJ_EVERY:
		km = &kmod;
		n = mdb_nv_size(&kt->k_modules) + 1;
		break;

	case (uintptr_t)MDB_TGT_OBJ_RTLD:
		obj = KT_RTLD_NAME;
		/*FALLTHRU*/

	default:
		if ((v = mdb_nv_lookup(&kt->k_modules, obj)) == NULL)
			return (set_errno(EMDB_NOOBJ));

		km = mdb_nv_get_cookie(v);
		n = 1;

		if (km->km_symtab == NULL)
			kt_load_module(kt, t, km);
	}

	for (; n > 0; n--, km = mdb_list_next(km)) {
		if (mdb_gelf_symtab_lookup_by_name(km->km_symtab, name, s) == 0)
			return (0);
	}

	return (set_errno(EMDB_NOSYM));
}

int
kt_lookup_by_addr(mdb_tgt_t *t, uintptr_t addr, uint_t flags,
    char *buf, size_t nbytes, GElf_Sym *symp)
{
	kt_data_t *kt = t->t_data;
	kt_module_t kmods[3], *kmods_begin = &kmods[0], *kmods_end;
	const char *name;

	kt_module_t *km = &kmods[0];	/* Point km at first fake module */
	kt_module_t *sym_km = NULL;	/* Module associated with best sym */
	GElf_Sym sym;			/* Best symbol found so far if !exact */

	/*
	 * To simplify the implementation, we create fake modules on the stack
	 * that are "prepended" to k_modlist and whose symtab is set to
	 * each of three special symbol tables, in order of precedence.
	 */
	km->km_symtab = mdb.m_prsym;

	if (kt->k_symtab != NULL) {
		km->km_list.ml_next = (mdb_list_t *)(km + 1);
		km = mdb_list_next(km);
		km->km_symtab = kt->k_symtab;
	}

	if (kt->k_dynsym != NULL) {
		km->km_list.ml_next = (mdb_list_t *)(km + 1);
		km = mdb_list_next(km);
		km->km_symtab = kt->k_dynsym;
	}

	km->km_list.ml_next = mdb_list_next(&kt->k_modlist);
	kmods_end = km;

	/*
	 * Now iterate over the list of fake and real modules.  If the module
	 * has no symbol table and the address is in the text section,
	 * instantiate the module's symbol table.  In exact mode, we can
	 * jump to 'found' immediately if we match.  Otherwise we continue
	 * looking and improve our choice if we find a closer symbol.
	 */
	for (km = &kmods[0]; km != NULL; km = mdb_list_next(km)) {
		if (km->km_symtab == NULL && addr >= km->km_text_va &&
		    addr < km->km_text_va + km->km_text_size)
			kt_load_module(kt, t, km);

		if (mdb_gelf_symtab_lookup_by_addr(km->km_symtab, addr,
		    flags, buf, nbytes, symp) != 0 || symp->st_value == 0)
			continue;

		if (flags & MDB_TGT_SYM_EXACT) {
			sym_km = km;
			goto found;
		}

		if (sym_km == NULL || mdb_gelf_sym_closer(symp, &sym, addr)) {
			sym_km = km;
			sym = *symp;
		}
	}

	if (sym_km == NULL)
		return (set_errno(EMDB_NOSYMADDR));

	*symp = sym; /* Copy our best symbol into the caller's symbol */
found:
	/*
	 * Once we've found something, copy the final name into the caller's
	 * buffer and prefix it with the load object name if appropriate.
	 */
	name = mdb_gelf_sym_name(sym_km->km_symtab, symp);

	if (sym_km < kmods_begin || sym_km > kmods_end) {
		(void) mdb_snprintf(buf, nbytes, "%s`%s",
		    sym_km->km_name, name);
	} else {
		(void) strncpy(buf, name, nbytes);
		buf[nbytes - 1] = '\0';
	}

	return (0);
}

static int
kt_symtab_func(const kt_symarg_t *argp, const GElf_Sym *sym, const char *name)
{
	if (mdb_tgt_sym_match(sym, argp->sym_type))
		return (argp->sym_cb(argp->sym_data, sym, name));

	return (0);
}

static void
kt_symtab_iter(mdb_gelf_symtab_t *gst, uint_t type, mdb_tgt_sym_f *cb, void *p)
{
	kt_symarg_t arg;

	arg.sym_cb = cb;
	arg.sym_data = p;
	arg.sym_type = type;

	mdb_gelf_symtab_iter(gst, (int (*)(void *, const GElf_Sym *,
	    const char *))kt_symtab_func, &arg);
}

int
kt_symbol_iter(mdb_tgt_t *t, const char *obj, uint_t which,
    uint_t type, mdb_tgt_sym_f *cb, void *data)
{
	kt_data_t *kt = t->t_data;
	kt_module_t *km;

	mdb_gelf_symtab_t *symtab = NULL;
	mdb_var_t *v;

	switch ((uintptr_t)obj) {
	case (uintptr_t)MDB_TGT_OBJ_EXEC:
		if (which == MDB_TGT_SYMTAB)
			symtab = kt->k_symtab;
		else
			symtab = kt->k_dynsym;
		break;

	case (uintptr_t)MDB_TGT_OBJ_EVERY:
		if (which == MDB_TGT_DYNSYM) {
			symtab = kt->k_dynsym;
			break;
		}

		kt_symtab_iter(kt->k_symtab, type, cb, data);
		mdb_nv_rewind(&kt->k_modules);

		while ((v = mdb_nv_advance(&kt->k_modules)) != NULL) {
			km = mdb_nv_get_cookie(v);
			if (km->km_symtab) {
				kt_symtab_iter(km->km_symtab,
				    type, cb, data);
			}
		}
		break;

	case (uintptr_t)MDB_TGT_OBJ_RTLD:
		obj = KT_RTLD_NAME;
		/*FALLTHRU*/

	default:
		v = mdb_nv_lookup(&kt->k_modules, obj);

		if (v == NULL)
			return (set_errno(EMDB_NOOBJ));

		km = mdb_nv_get_cookie(v);

		if (km->km_symtab == NULL)
			kt_load_module(kt, t, km);

		symtab = km->km_symtab;
	}

	if (symtab)
		kt_symtab_iter(symtab, type, cb, data);

	return (0);
}

static int
kt_mapping_walk(uintptr_t addr, const void *data, kt_maparg_t *marg)
{
	/*
	 * This is a bit sketchy but avoids problematic compilation of this
	 * target against the current VM implementation.  Now that we have
	 * vmem, we can make this less broken and more informative by changing
	 * this code to invoke the vmem walker in the near future.
	 */
	const struct kt_seg {
		caddr_t s_base;
		size_t s_size;
	} *segp = (const struct kt_seg *)data;

	mdb_map_t map;
	GElf_Sym sym;

	map.map_base = (uintptr_t)segp->s_base;
	map.map_size = segp->s_size;

	if (kt_lookup_by_addr(marg->map_target, addr, MDB_TGT_SYM_EXACT,
	    map.map_name, MDB_TGT_MAPSZ, &sym) == -1) {

		(void) mdb_iob_snprintf(map.map_name, MDB_TGT_MAPSZ,
		    "%lr", addr);
	}

	return (marg->map_cb(marg->map_data, &map, map.map_name));
}

int
kt_mapping_iter(mdb_tgt_t *t, mdb_tgt_map_f *func, void *private)
{
	kt_data_t *kt = t->t_data;
	kt_maparg_t m;

	m.map_target = t;
	m.map_cb = func;
	m.map_data = private;

	return (mdb_pwalk("seg", (mdb_walk_cb_t)kt_mapping_walk, &m,
	    (uintptr_t)kt->k_as));
}

int
kt_object_iter(mdb_tgt_t *t, mdb_tgt_map_f *func, void *private)
{
	kt_data_t *kt = t->t_data;
	kt_module_t *km;
	mdb_map_t map;

	for (km = mdb_list_next(&kt->k_modlist); km; km = mdb_list_next(km)) {
		(void) strncpy(map.map_name, km->km_name, MDB_TGT_MAPSZ);
		map.map_name[MDB_TGT_MAPSZ - 1] = '\0';
		map.map_base = km->km_text_va;
		map.map_size = km->km_text_size + km->km_data_size;

		if (func(private, &map, km->km_name) == -1)
			break;
	}

	return (0);
}

void
kt_destroy(mdb_tgt_t *t)
{
	kt_data_t *kt = t->t_data;
	kt_module_t *km, *nkm;

	(void) mdb_module_unload(KT_MODULE);

	if (kt->k_regs != NULL)
		mdb_free(kt->k_regs, kt->k_regsize);

	if (kt->k_symtab != NULL)
		mdb_gelf_symtab_destroy(kt->k_symtab);

	if (kt->k_dynsym != NULL)
		mdb_gelf_symtab_destroy(kt->k_dynsym);

	mdb_gelf_destroy(kt->k_file);
	(void) kvm_close(kt->k_cookie);

	for (km = mdb_list_next(&kt->k_modlist); km; km = nkm) {
		if (km->km_symtab)
			mdb_gelf_symtab_destroy(km->km_symtab);

		if (km->km_data)
			mdb_free(km->km_data, km->km_datasz);

		nkm = mdb_list_next(km);
		strfree(km->km_name);
		mdb_free(km, sizeof (kt_module_t));
	}

	mdb_nv_destroy(&kt->k_modules);
	mdb_free(kt, sizeof (kt_data_t));
}

int
mdb_kvm_tgt_create(mdb_tgt_t *t, int argc, const char *argv[])
{
	kt_data_t *kt = mdb_zalloc(sizeof (kt_data_t), UM_SLEEP);
	int oflag = (t->t_flags & MDB_TGT_F_RDWR) ? O_RDWR : O_RDONLY;

	struct utsname uts;
	GElf_Sym sym;
	pgcnt_t pmem;

	if (argc != 2)
		return (set_errno(EINVAL));

	kt->k_symfile = argv[0];
	kt->k_kvmfile = argv[1];

	if ((kt->k_cookie = kvm_open(kt->k_symfile, kt->k_kvmfile, NULL,
	    oflag, (char *)mdb.m_pname)) == NULL)
		goto err;

	if ((kt->k_fio = mdb_fdio_create_path(NULL, kt->k_symfile,
	    O_RDONLY, 0)) == NULL) {
		mdb_warn("failed to open %s", kt->k_symfile);
		goto err;
	}

	if ((kt->k_file = mdb_gelf_create(kt->k_fio,
	    ET_EXEC, GF_FILE)) == NULL) {
		mdb_io_destroy(kt->k_fio);
		goto err;
	}

	kt->k_symtab =
	    mdb_gelf_symtab_create_file(kt->k_file, ".symtab", ".strtab");

	kt->k_dynsym =
	    mdb_gelf_symtab_create_file(kt->k_file, ".dynsym", ".dynstr");

	if (mdb_gelf_symtab_lookup_by_name(kt->k_symtab, "kas", &sym) == -1) {
		warn("'kas' symbol is missing from kernel\n");
		goto err;
	}

	kt->k_as = (struct as *)sym.st_value;

	if (mdb_gelf_symtab_lookup_by_name(kt->k_symtab, "platform", &sym)) {
		warn("'platform' symbol is missing from kernel\n");
		goto err;
	}

	if (kvm_kread(kt->k_cookie, sym.st_value,
	    kt->k_platform, MAXNAMELEN) <= 0) {
		warn("failed to read 'platform' string from kernel");
		goto err;
	}

	if (mdb_gelf_symtab_lookup_by_name(kt->k_symtab, "utsname", &sym)) {
		warn("'utsname' symbol is missing from kernel\n");
		goto err;
	}

	if (kvm_kread(kt->k_cookie, sym.st_value, &uts, sizeof (uts)) <= 0) {
		warn("failed to read 'utsname' struct from kernel");
		goto err;
	}

	/*
	 * These libkvm symbols were not present in Solaris 2.6; as such
	 * we need to look them up manually using the run-time linker.
	 */
	kt->k_aread = (ssize_t (*)()) dlsym(RTLD_DEFAULT, "kvm_aread");
	kt->k_awrite = (ssize_t (*)()) dlsym(RTLD_DEFAULT, "kvm_awrite");
	kt->k_pread = (ssize_t (*)()) dlsym(RTLD_DEFAULT, "kvm_pread");
	kt->k_pwrite = (ssize_t (*)()) dlsym(RTLD_DEFAULT, "kvm_pwrite");

	/*
	 * If any of these functions are unavailable with the current
	 * bindings, replace them with calls to mdb_tgt_notsup().
	 */
	if (kt->k_aread == NULL)
		kt->k_aread = (ssize_t (*)())mdb_tgt_notsup;
	if (kt->k_awrite == NULL)
		kt->k_awrite = (ssize_t (*)())mdb_tgt_notsup;
	if (kt->k_pread == NULL)
		kt->k_pread = (ssize_t (*)())mdb_tgt_notsup;
	if (kt->k_pwrite == NULL)
		kt->k_pwrite = (ssize_t (*)())mdb_tgt_notsup;

	mdb_nv_create(&kt->k_modules);
	t->t_pshandle = kt->k_cookie;
	t->t_data = kt;

#ifdef __sparc
#ifdef __sparcv9
	kt_sparcv9_init(t);
#else	/* __sparcv9 */
	if (strcmp(uts.machine, "sun4u") != 0)
		kt_sparcv7_init(t);
	else
		kt_sparcv9_init(t);
#endif	/* __sparcv9 */
#else	/* __sparc */
	kt_ia32_init(t);
#endif	/* __sparc */

	/*
	 * We read our representative thread ID (address) from the kernel's
	 * global panic_thread.  It will remain 0 if this is a live kernel.
	 */
	(void) mdb_tgt_readsym(t, MDB_TGT_AS_VIRT, &kt->k_tid, sizeof (void *),
	    MDB_TGT_OBJ_EXEC, "panic_thread");

	if ((mdb.m_flags & MDB_FL_ADB) && mdb_tgt_readsym(t, MDB_TGT_AS_VIRT,
	    &pmem, sizeof (pmem), MDB_TGT_OBJ_EXEC, "physmem") == sizeof (pmem))
		mdb_printf("physmem %lx\n", (ulong_t)pmem);

	return (0);

err:
	if (kt->k_symtab != NULL)
		mdb_gelf_symtab_destroy(kt->k_symtab);

	if (kt->k_dynsym != NULL)
		mdb_gelf_symtab_destroy(kt->k_dynsym);

	if (kt->k_file != NULL)
		mdb_gelf_destroy(kt->k_file);

	if (kt->k_cookie != NULL)
		(void) kvm_close(kt->k_cookie);

	mdb_free(kt, sizeof (kt_data_t));
	return (-1);
}
