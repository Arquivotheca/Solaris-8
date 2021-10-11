/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_target.c	1.1	99/08/11 SMI"

#include <mdb/mdb_target_impl.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_gelf.h>
#include <mdb/mdb.h>

#include <sys/stat.h>
#include <sys/param.h>
#include <strings.h>
#include <errno.h>

mdb_tgt_t *
mdb_tgt_create(mdb_tgt_ctor_f *ctor, int flags, int argc, const char *argv[])
{
	mdb_module_t *mp;
	mdb_tgt_t *t;

	if (flags & ~MDB_TGT_F_ALL) {
		(void) set_errno(EINVAL);
		return (NULL);
	}

	t = mdb_zalloc(sizeof (mdb_tgt_t), UM_SLEEP);
	mdb_list_append(&mdb.m_tgtlist, t);

	t->t_module = &mdb.m_rmod;
	t->t_flags = flags;
	t->t_seseq = 1;

	for (mp = mdb.m_mhead; mp != NULL; mp = mp->mod_next) {
		if (ctor == mp->mod_tgt_ctor) {
			t->t_module = mp;
			break;
		}
	}

	if (ctor(t, argc, argv) != 0) {
		mdb_list_delete(&mdb.m_tgtlist, t);
		mdb_free(t, sizeof (mdb_tgt_t));
		return (NULL);
	}

	mdb_dprintf(MDB_DBG_TGT, "t_create %s (%p)\n",
	    t->t_module->mod_name, (void *)t);

	return (t);
}

int
mdb_tgt_getflags(mdb_tgt_t *t)
{
	return (t->t_flags);
}

int
mdb_tgt_setflags(mdb_tgt_t *t, int flags)
{
	if (flags & ~MDB_TGT_F_ALL)
		return (set_errno(EINVAL));

	if ((flags &= ~t->t_flags) != 0)
		return (t->t_ops->t_setflags(t, flags));

	return (0);
}

int
mdb_tgt_setcontext(mdb_tgt_t *t, void *context)
{
	return (t->t_ops->t_setcontext(t, context));
}

void
mdb_tgt_destroy(mdb_tgt_t *t)
{
	mdb_xdata_t *xdp, *nxdp;

	if (mdb.m_target == t) {
		mdb_dprintf(MDB_DBG_TGT, "t_deactivate %s (%p)\n",
		    t->t_module->mod_name, (void *)t);
		t->t_ops->t_deactivate(t);
		mdb.m_target = NULL;
	}

	for (xdp = mdb_list_next(&t->t_xdlist); xdp != NULL; xdp = nxdp) {
		nxdp = mdb_list_next(xdp);
		mdb_list_delete(&t->t_xdlist, xdp);
		mdb_free(xdp, sizeof (mdb_xdata_t));
	}

	mdb_dprintf(MDB_DBG_TGT, "t_destroy %s (%p)\n",
	    t->t_module->mod_name, (void *)t);
	t->t_ops->t_destroy(t);

	mdb_list_delete(&mdb.m_tgtlist, t);
	mdb_free(t, sizeof (mdb_tgt_t));

	if (mdb.m_target == NULL)
		mdb_tgt_activate(mdb_list_prev(&mdb.m_tgtlist));
}

void
mdb_tgt_activate(mdb_tgt_t *t)
{
	mdb_tgt_t *otgt = mdb.m_target;

	if (mdb.m_target != NULL) {
		mdb_dprintf(MDB_DBG_TGT, "t_deactivate %s (%p)\n",
		    mdb.m_target->t_module->mod_name, (void *)mdb.m_target);
		mdb.m_target->t_ops->t_deactivate(mdb.m_target);
	}

	if ((mdb.m_target = t) != NULL) {
		const char *v = strstr(mdb.m_root, "%V");

		mdb_dprintf(MDB_DBG_TGT, "t_activate %s (%p)\n",
		    t->t_module->mod_name, (void *)t);

		/*
		 * If the root was explicitly set with -R and contains %V,
		 * expand it like a path.  If the resulting directory is
		 * not present, then replace %V with "latest" and re-evaluate.
		 */
		if (v != NULL) {
			char old_root[MAXPATHLEN];
			const char **p;
			struct stat s;
			size_t len;

			p = mdb_path_alloc(mdb.m_root, NULL, 0, &len);
			(void) strcpy(old_root, mdb.m_root);
			(void) strncpy(mdb.m_root, p[0], MAXPATHLEN);
			mdb.m_root[MAXPATHLEN - 1] = '\0';
			mdb_path_free(p, len);

			if (stat(mdb.m_root, &s) == -1 && errno == ENOENT) {
				mdb.m_flags |= MDB_FL_LATEST;
				p = mdb_path_alloc(old_root, NULL, 0, &len);
				(void) strncpy(mdb.m_root, p[0], MAXPATHLEN);
				mdb.m_root[MAXPATHLEN - 1] = '\0';
				mdb_path_free(p, len);
			}
		}

		/*
		 * Re-evaluate the macro and dmod paths now that we have the
		 * new target set and m_root figured out.
		 */
		if (otgt == NULL) {
			mdb_set_ipath(mdb.m_ipathstr);
			mdb_set_lpath(mdb.m_lpathstr);
		}

		t->t_ops->t_activate(t);
	}
}

const char *
mdb_tgt_name(mdb_tgt_t *t)
{
	return (t->t_ops->t_name(t));
}

const char *
mdb_tgt_isa(mdb_tgt_t *t)
{
	return (t->t_ops->t_isa(t));
}

const char *
mdb_tgt_platform(mdb_tgt_t *t)
{
	return (t->t_ops->t_platform(t));
}

int
mdb_tgt_uname(mdb_tgt_t *t, struct utsname *utsp)
{
	return (t->t_ops->t_uname(t, utsp));
}

ssize_t
mdb_tgt_aread(mdb_tgt_t *t, mdb_tgt_as_t as,
	void *buf, size_t n, mdb_tgt_addr_t addr)
{
	switch ((uintptr_t)as) {
	case (uintptr_t)MDB_TGT_AS_VIRT:
		return (t->t_ops->t_vread(t, buf, n, addr));
	case (uintptr_t)MDB_TGT_AS_PHYS:
		return (t->t_ops->t_pread(t, buf, n, addr));
	case (uintptr_t)MDB_TGT_AS_FILE:
		return (t->t_ops->t_fread(t, buf, n, addr));
	case (uintptr_t)MDB_TGT_AS_IO:
		return (t->t_ops->t_ioread(t, buf, n, addr));
	}
	return (t->t_ops->t_aread(t, as, buf, n, addr));
}

ssize_t
mdb_tgt_awrite(mdb_tgt_t *t, mdb_tgt_as_t as,
	const void *buf, size_t n, mdb_tgt_addr_t addr)
{
	if (!(t->t_flags & MDB_TGT_F_RDWR))
		return (set_errno(EMDB_TGTRDONLY));

	switch ((uintptr_t)as) {
	case (uintptr_t)MDB_TGT_AS_VIRT:
		return (t->t_ops->t_vwrite(t, buf, n, addr));
	case (uintptr_t)MDB_TGT_AS_PHYS:
		return (t->t_ops->t_pwrite(t, buf, n, addr));
	case (uintptr_t)MDB_TGT_AS_FILE:
		return (t->t_ops->t_fwrite(t, buf, n, addr));
	case (uintptr_t)MDB_TGT_AS_IO:
		return (t->t_ops->t_iowrite(t, buf, n, addr));
	}
	return (t->t_ops->t_awrite(t, as, buf, n, addr));
}

ssize_t
mdb_tgt_vread(mdb_tgt_t *t, void *buf, size_t n, uintptr_t addr)
{
	return (t->t_ops->t_vread(t, buf, n, addr));
}

ssize_t
mdb_tgt_vwrite(mdb_tgt_t *t, const void *buf, size_t n, uintptr_t addr)
{
	if (t->t_flags & MDB_TGT_F_RDWR)
		return (t->t_ops->t_vwrite(t, buf, n, addr));

	return (set_errno(EMDB_TGTRDONLY));
}

ssize_t
mdb_tgt_pread(mdb_tgt_t *t, void *buf, size_t n, physaddr_t addr)
{
	return (t->t_ops->t_pread(t, buf, n, addr));
}

ssize_t
mdb_tgt_pwrite(mdb_tgt_t *t, const void *buf, size_t n, physaddr_t addr)
{
	if (t->t_flags & MDB_TGT_F_RDWR)
		return (t->t_ops->t_pwrite(t, buf, n, addr));

	return (set_errno(EMDB_TGTRDONLY));
}

ssize_t
mdb_tgt_fread(mdb_tgt_t *t, void *buf, size_t n, uintptr_t addr)
{
	return (t->t_ops->t_fread(t, buf, n, addr));
}

ssize_t
mdb_tgt_fwrite(mdb_tgt_t *t, const void *buf, size_t n, uintptr_t addr)
{
	if (t->t_flags & MDB_TGT_F_RDWR)
		return (t->t_ops->t_fwrite(t, buf, n, addr));

	return (set_errno(EMDB_TGTRDONLY));
}

ssize_t
mdb_tgt_ioread(mdb_tgt_t *t, void *buf, size_t n, uintptr_t addr)
{
	return (t->t_ops->t_ioread(t, buf, n, addr));
}

ssize_t
mdb_tgt_iowrite(mdb_tgt_t *t, const void *buf, size_t n, uintptr_t addr)
{
	if (t->t_flags & MDB_TGT_F_RDWR)
		return (t->t_ops->t_iowrite(t, buf, n, addr));

	return (set_errno(EMDB_TGTRDONLY));
}

int
mdb_tgt_vtop(mdb_tgt_t *t, mdb_tgt_as_t as, uintptr_t va, physaddr_t *pap)
{
	return (t->t_ops->t_vtop(t, as, va, pap));
}

ssize_t
mdb_tgt_readstr(mdb_tgt_t *t, mdb_tgt_as_t as, char *buf,
	size_t nbytes, mdb_tgt_addr_t addr)
{
	ssize_t n, nread = mdb_tgt_aread(t, as, buf, nbytes, addr);
	char *p;

	if (nread >= 0) {
		if ((p = memchr(buf, '\0', nread)) != NULL)
			nread = (size_t)(p - buf);
		goto done;
	}

	nread = 0;
	p = &buf[0];

	while (nread < nbytes && (n = mdb_tgt_aread(t, as, p, 1, addr)) == 1) {
		if (*p == '\0')
			return (nread);
		nread++;
		addr++;
		p++;
	}

	if (nread == 0 && n == -1)
		return (-1); /* If we can't even read a byte, return -1 */

done:
	if (nread >= nbytes)
		nread = nbytes - 1;

	buf[nread] = '\0';
	return (nread);
}

ssize_t
mdb_tgt_writestr(mdb_tgt_t *t, mdb_tgt_as_t as,
	const char *buf, mdb_tgt_addr_t addr)
{
	ssize_t nwritten = mdb_tgt_awrite(t, as, buf, strlen(buf) + 1, addr);
	return (nwritten > 0 ? nwritten - 1 : nwritten);
}

int
mdb_tgt_lookup_by_name(mdb_tgt_t *t, const char *obj,
	const char *name, GElf_Sym *symp)
{
	GElf_Sym sym;

	if (name == NULL)
		return (set_errno(EINVAL));

	if (obj == MDB_TGT_OBJ_EVERY &&
	    mdb_gelf_symtab_lookup_by_name(mdb.m_prsym, name, &sym) == 0)
		goto found;

	if (t->t_ops->t_lookup_by_name(t, obj, name, &sym) == 0)
		goto found;

	return (-1);

found:
	if (symp != NULL)
		*symp = sym;
	return (0);
}

int
mdb_tgt_lookup_by_addr(mdb_tgt_t *t, uintptr_t addr, uint_t flags,
	char *buf, size_t len, GElf_Sym *symp)
{
	GElf_Sym sym;

	if (t->t_ops->t_lookup_by_addr(t, addr, flags,
	    buf, len, &sym) == 0) {
		if (symp != NULL)
			*symp = sym;
		return (0);
	}

	return (-1);
}

int
mdb_tgt_symbol_iter(mdb_tgt_t *t, const char *obj, uint_t which,
	uint_t type, mdb_tgt_sym_f *cb, void *p)
{
	if ((which != MDB_TGT_SYMTAB && which != MDB_TGT_DYNSYM) ||
	    (type & ~(MDB_TGT_BIND_ANY | MDB_TGT_TYPE_ANY)) != 0)
		return (set_errno(EINVAL));

	return (t->t_ops->t_symbol_iter(t, obj, which, type, cb, p));
}

ssize_t
mdb_tgt_readsym(mdb_tgt_t *t, mdb_tgt_as_t as, void *buf, size_t nbytes,
	const char *obj, const char *name)
{
	GElf_Sym sym;

	if (mdb_tgt_lookup_by_name(t, obj, name, &sym) == 0)
		return (mdb_tgt_aread(t, as, buf, nbytes, sym.st_value));

	return (-1);
}

ssize_t
mdb_tgt_writesym(mdb_tgt_t *t, mdb_tgt_as_t as, const void *buf,
	size_t nbytes, const char *obj, const char *name)
{
	GElf_Sym sym;

	if (mdb_tgt_lookup_by_name(t, obj, name, &sym) == 0)
		return (mdb_tgt_awrite(t, as, buf, nbytes, sym.st_value));

	return (-1);
}

int
mdb_tgt_mapping_iter(mdb_tgt_t *t, mdb_tgt_map_f *cb, void *p)
{
	return (t->t_ops->t_mapping_iter(t, cb, p));
}

int
mdb_tgt_object_iter(mdb_tgt_t *t, mdb_tgt_map_f *cb, void *p)
{
	return (t->t_ops->t_object_iter(t, cb, p));
}

const mdb_map_t *
mdb_tgt_addr_to_map(mdb_tgt_t *t, uintptr_t addr)
{
	return (t->t_ops->t_addr_to_map(t, addr));
}

const mdb_map_t *
mdb_tgt_name_to_map(mdb_tgt_t *t, const char *name)
{
	return (t->t_ops->t_name_to_map(t, name));
}

int
mdb_tgt_thread_iter(mdb_tgt_t *t, mdb_tgt_thread_f *cb, void *p)
{
	return (t->t_ops->t_thread_iter(t, cb, p));
}

int
mdb_tgt_cpu_iter(mdb_tgt_t *t, mdb_tgt_cpu_f *cb, void *p)
{
	return (t->t_ops->t_cpu_iter(t, cb, p));
}

int
mdb_tgt_thr_status(mdb_tgt_t *t, mdb_tgt_tid_t tid, mdb_tgt_status_t *tsp)
{
	return (t->t_ops->t_thr_status(t, tid, tsp));
}

int
mdb_tgt_cpu_status(mdb_tgt_t *t, mdb_tgt_cpuid_t cpuid, mdb_tgt_status_t *tsp)
{
	return (t->t_ops->t_cpu_status(t, cpuid, tsp));
}

int
mdb_tgt_status(mdb_tgt_t *t, mdb_tgt_status_t *tsp)
{
	return (t->t_ops->t_status(t, tsp));
}

int
mdb_tgt_run(mdb_tgt_t *t, int argc, const mdb_arg_t *argv)
{
	int i;

	for (i = 0; i < argc; i++) {
		if (argv->a_type != MDB_TYPE_STRING)
			return (set_errno(EINVAL));
	}

	return (t->t_ops->t_run(t, argc, argv));
}

int
mdb_tgt_step(mdb_tgt_t *t, mdb_tgt_tid_t tid)
{
	return (t->t_ops->t_step(t, tid));
}

int
mdb_tgt_continue(mdb_tgt_t *t, mdb_tgt_status_t *tsp)
{
	return (t->t_ops->t_continue(t, tsp));
}

int
mdb_tgt_call(mdb_tgt_t *t, uintptr_t addr, int argc, const mdb_arg_t *argv)
{
	return (t->t_ops->t_call(t, addr, argc, argv));
}

char *
mdb_tgt_sespec_info(mdb_tgt_t *t, int id, char *buf, size_t nbytes)
{
	mdb_sespec_t *sep;

	for (sep = mdb_list_next(&t->t_selist); sep; sep = mdb_list_next(sep)) {
		if (sep->se_id == id)
			return (sep->se_ops->se_info(t, sep, buf, nbytes));
	}

	(void) set_errno(EMDB_NOSESPEC);
	return (NULL);
}

void *
mdb_tgt_sespec_data(mdb_tgt_t *t, int id)
{
	mdb_sespec_t *sep;

	for (sep = mdb_list_next(&t->t_selist); sep; sep = mdb_list_next(sep)) {
		if (sep->se_id == id)
			return (sep->se_cookie);
	}

	(void) set_errno(EMDB_NOSESPEC);
	return (NULL);
}

int
mdb_tgt_sespec_iter(mdb_tgt_t *t, mdb_tgt_sespec_f *cb, void *p)
{
	mdb_sespec_t *sep;

	for (sep = mdb_list_next(&t->t_selist); sep; sep = mdb_list_next(sep)) {
		if (cb(p, sep->se_id, sep->se_cookie))
			break;
	}

	return (0);
}

int
mdb_tgt_sespec_delete(mdb_tgt_t *t, int id, mdb_tgt_sespec_f *cb, void *p)
{
	mdb_sespec_t *sep;

	for (sep = mdb_list_next(&t->t_selist); sep; sep = mdb_list_next(sep)) {
		if (sep->se_id == id) {
			(void) cb(p, sep->se_id, sep->se_cookie);
			sep->se_ops->se_destroy(t, sep);
			mdb_list_delete(&t->t_selist, sep);
			mdb_free(sep, sizeof (mdb_sespec_t));
			return (0);
		}
	}

	return (set_errno(EMDB_NOSESPEC));
}

int
mdb_tgt_add_brkpt(mdb_tgt_t *t, uintptr_t addr, void *p)
{
	return (t->t_ops->t_add_brkpt(t, addr, p));
}

int
mdb_tgt_add_pwapt(mdb_tgt_t *t, physaddr_t pa, size_t n, uint_t flags, void *p)
{
	if (flags & ~(MDB_TGT_WA_R | MDB_TGT_WA_W | MDB_TGT_WA_X))
		return (set_errno(EINVAL));

	return (t->t_ops->t_add_pwapt(t, pa, n, flags, p));
}

int
mdb_tgt_add_vwapt(mdb_tgt_t *t, uintptr_t va, size_t n, uint_t flags, void *p)
{
	if (flags & ~(MDB_TGT_WA_R | MDB_TGT_WA_W | MDB_TGT_WA_X))
		return (set_errno(EINVAL));

	return (t->t_ops->t_add_vwapt(t, va, n, flags, p));
}

int
mdb_tgt_add_iowapt(mdb_tgt_t *t, ioaddr_t ia, size_t n, uint_t flags, void *p)
{
	if (flags & ~(MDB_TGT_WA_R | MDB_TGT_WA_W))
		return (set_errno(EINVAL));

	return (t->t_ops->t_add_iowapt(t, ia, n, flags, p));
}

int
mdb_tgt_add_ixwapt(mdb_tgt_t *t, ulong_t opcode, ulong_t mask, void *p)
{
	return (t->t_ops->t_add_ixwapt(t, opcode, mask, p));
}

int
mdb_tgt_add_sysenter(mdb_tgt_t *t, int sysnum, void *p)
{
	return (t->t_ops->t_add_sysenter(t, sysnum, p));
}

int
mdb_tgt_add_sysexit(mdb_tgt_t *t, int sysnum, void *p)
{
	return (t->t_ops->t_add_sysexit(t, sysnum, p));
}

int
mdb_tgt_add_signal(mdb_tgt_t *t, int sysnum, void *p)
{
	return (t->t_ops->t_add_signal(t, sysnum, p));
}

int
mdb_tgt_add_object_load(mdb_tgt_t *t, void *p)
{
	return (t->t_ops->t_add_object_load(t, p));
}

int
mdb_tgt_add_object_unload(mdb_tgt_t *t, void *p)
{
	return (t->t_ops->t_add_object_unload(t, p));
}

int
mdb_tgt_getareg(mdb_tgt_t *t, mdb_tgt_tid_t tid,
	const char *rname, mdb_tgt_reg_t *rp)
{
	return (t->t_ops->t_getareg(t, tid, rname, rp));
}

int
mdb_tgt_putareg(mdb_tgt_t *t, mdb_tgt_tid_t tid,
	const char *rname, mdb_tgt_reg_t r)
{
	return (t->t_ops->t_putareg(t, tid, rname, r));
}

int
mdb_tgt_stack_iter(mdb_tgt_t *t, const mdb_tgt_gregset_t *gregs,
    mdb_tgt_stack_f *cb, void *p)
{
	return (t->t_ops->t_stack_iter(t, gregs, cb, p));
}

int
mdb_tgt_xdata_iter(mdb_tgt_t *t, mdb_tgt_xdata_f *func, void *private)
{
	mdb_xdata_t *xdp;

	for (xdp = mdb_list_next(&t->t_xdlist); xdp; xdp = mdb_list_next(xdp)) {
		if (func(private, xdp->xd_name, xdp->xd_desc,
		    xdp->xd_copy(t, NULL, 0)) != 0)
			break;
	}

	return (0);
}

ssize_t
mdb_tgt_getxdata(mdb_tgt_t *t, const char *name, void *buf, size_t nbytes)
{
	mdb_xdata_t *xdp;

	for (xdp = mdb_list_next(&t->t_xdlist); xdp; xdp = mdb_list_next(xdp)) {
		if (strcmp(xdp->xd_name, name) == 0)
			return (xdp->xd_copy(t, buf, nbytes));
	}

	return (set_errno(ENODATA));
}

long
mdb_tgt_notsup()
{
	return (set_errno(EMDB_TGTNOTSUP));
}

void *
mdb_tgt_null()
{
	(void) set_errno(EMDB_TGTNOTSUP);
	return (NULL);
}

long
mdb_tgt_nop()
{
	return (0L);
}

int
mdb_tgt_xdata_insert(mdb_tgt_t *t, const char *name, const char *desc,
	ssize_t (*copy)(mdb_tgt_t *, void *, size_t))
{
	mdb_xdata_t *xdp;

	for (xdp = mdb_list_next(&t->t_xdlist); xdp; xdp = mdb_list_next(xdp)) {
		if (strcmp(xdp->xd_name, name) == 0)
			return (set_errno(EMDB_XDEXISTS));
	}

	xdp = mdb_alloc(sizeof (mdb_xdata_t), UM_SLEEP);
	mdb_list_append(&t->t_xdlist, xdp);

	xdp->xd_name = name;
	xdp->xd_desc = desc;
	xdp->xd_copy = copy;

	return (0);
}

int
mdb_tgt_xdata_delete(mdb_tgt_t *t, const char *name)
{
	mdb_xdata_t *xdp;

	for (xdp = mdb_list_next(&t->t_xdlist); xdp; xdp = mdb_list_next(xdp)) {
		if (strcmp(xdp->xd_name, name) == 0) {
			mdb_list_delete(&t->t_xdlist, xdp);
			mdb_free(xdp, sizeof (mdb_xdata_t));
			return (0);
		}
	}

	return (set_errno(EMDB_NOXD));
}

int
mdb_tgt_sym_match(const GElf_Sym *sym, uint_t mask)
{
	uchar_t s_bind = GELF_ST_BIND(sym->st_info);
	uchar_t s_type = GELF_ST_TYPE(sym->st_info);

	/*
	 * In case you haven't already guessed, this relies on the bitmask
	 * used by <mdb/mdb_target.h> and <libproc.h> for encoding symbol
	 * type and binding matching the order of STB and STT constants
	 * in <sys/elf.h>.  ELF can't change without breaking binary
	 * compatibility, so I think this is reasonably fair game.
	 */
	if (s_bind < STB_NUM && s_type < STT_NUM) {
		uint_t type = (1 << (s_type + 8)) | (1 << s_bind);
		return ((type & ~mask) == 0);
	}

	return (0); /* Unknown binding or type; fail to match */
}

void
mdb_tgt_elf_export(mdb_gelf_file_t *gf)
{
	GElf_Xword d = 0, t = 0;
	GElf_Addr b = 0, e = 0;
	uint32_t m = 0;
	mdb_var_t *v;

	/*
	 * Reset legacy adb variables based on the specified ELF object file
	 * provided by the target.  We define these variables:
	 *
	 * b - the address of the data segment (first writeable Phdr)
	 * d - the size of the data segment
	 * e - the address of the entry point
	 * m - the magic number identifying the file
	 * t - the address of the text segment (first executable Phdr)
	 */
	if (gf != NULL) {
		const GElf_Phdr *text = NULL, *data = NULL;
		size_t i;

		e = gf->gf_ehdr.e_entry;
		bcopy(&gf->gf_ehdr.e_ident[EI_MAG0], &m, sizeof (m));

		for (i = 0; i < gf->gf_npload; i++) {
			if (text == NULL && (gf->gf_phdrs[i].p_flags & PF_X))
				text = &gf->gf_phdrs[i];
			if (data == NULL && (gf->gf_phdrs[i].p_flags & PF_W))
				data = &gf->gf_phdrs[i];
		}

		if (text != NULL)
			t = text->p_memsz;
		if (data != NULL) {
			b = data->p_vaddr;
			d = data->p_memsz;
		}
	}

	if ((v = mdb_nv_lookup(&mdb.m_nv, "b")) != NULL)
		mdb_nv_set_value(v, b);
	if ((v = mdb_nv_lookup(&mdb.m_nv, "d")) != NULL)
		mdb_nv_set_value(v, d);
	if ((v = mdb_nv_lookup(&mdb.m_nv, "e")) != NULL)
		mdb_nv_set_value(v, e);
	if ((v = mdb_nv_lookup(&mdb.m_nv, "m")) != NULL)
		mdb_nv_set_value(v, m);
	if ((v = mdb_nv_lookup(&mdb.m_nv, "t")) != NULL)
		mdb_nv_set_value(v, t);
}
