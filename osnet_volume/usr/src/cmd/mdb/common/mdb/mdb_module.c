/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mdb_module.c	1.2	99/11/19 SMI"

#include <sys/param.h>
#include <unistd.h>
#include <strings.h>
#include <dlfcn.h>
#include <link.h>

#include <mdb/mdb_module.h>
#include <mdb/mdb_modapi.h>
#include <mdb/mdb_debug.h>
#include <mdb/mdb_string.h>
#include <mdb/mdb_err.h>
#include <mdb/mdb_io.h>
#include <mdb/mdb_frame.h>
#include <mdb/mdb.h>

/*
 * For builtin modules, we set mod_init to this function, which just
 * returns a constant modinfo struct with no dcmds and walkers.
 */
static const mdb_modinfo_t *
builtin_init(void)
{
	static const mdb_modinfo_t info = { MDB_API_VERSION };
	return (&info);
}

static mdb_module_t *
module_create(const char *name, const char *fname, int mode)
{
	const mdb_walker_t empty_walk_list[] = { 0 };
	const mdb_dcmd_t empty_dcmd_list[] = { 0 };

	int dlmode = (mode & MDB_MOD_GLOBAL) ? RTLD_GLOBAL : RTLD_LOCAL;

	const mdb_modinfo_t *info;
	const mdb_dcmd_t *dcp;
	const mdb_walker_t *wp;

	mdb_module_t *mod;
	mdb_var_t *v;

	mod = mdb_zalloc(sizeof (mdb_module_t), UM_SLEEP);
	mod->mod_info = mdb_alloc(sizeof (mdb_modinfo_t), UM_SLEEP);

	mdb_nv_create(&mod->mod_dcmds);
	mdb_nv_create(&mod->mod_walkers);

	mod->mod_name = name;		/* Assign temporary name */
	mdb.m_lmod = mod;		/* Mark module as currently loading */

	if (!(mode & MDB_MOD_BUILTIN)) {
		mdb_dprintf(MDB_DBG_MODULE, "dlopen %s %x\n", fname, dlmode);
		mod->mod_hdl = dlmopen(LM_ID_BASE, fname, RTLD_NOW | dlmode);

		if (mod->mod_hdl == NULL) {
			warn("%s\n", dlerror());
			goto err;
		}

		mod->mod_init = (const mdb_modinfo_t *(*)(void))
		    dlsym(mod->mod_hdl, "_mdb_init");

		mod->mod_fini = (void (*)(void))
		    dlsym(mod->mod_hdl, "_mdb_fini");

		mod->mod_tgt_ctor = (mdb_tgt_ctor_f *)
		    dlsym(mod->mod_hdl, "_mdb_tgt_create");

		mod->mod_dis_ctor = (mdb_dis_ctor_f *)
		    dlsym(mod->mod_hdl, "_mdb_dis_create");
	} else
		mod->mod_init = builtin_init;

	if (mod->mod_init == NULL) {
		warn("%s module is missing _mdb_init definition\n", name);
		goto err;
	}

	if ((info = mod->mod_init()) == NULL) {
		warn("%s module failed to initialize\n", name);
		goto err;
	}

	/*
	 * Reject modules compiled for a newer version of the debugger.
	 */
	if (info->mi_dvers > MDB_API_VERSION) {
		warn("%s module requires newer mdb API version (%hu) than "
		    "debugger (%d)\n", name, info->mi_dvers, MDB_API_VERSION);
		goto err;
	}

	/*
	 * Load modules compiled for the current API version or version 0. API
	 * version 0 was used for Solaris 8 Beta Refresh modules and older
	 * modules stored on the MDB server.  We can remove support for this
	 * obsolete version when those archived modules are no longer needed.
	 */
	switch (info->mi_dvers) {
	case MDB_API_VERSION:
	case 0:
		/*
		 * Current API version -- copy entire modinfo
		 * structure into our own private storage.
		 */
		bcopy(info, mod->mod_info, sizeof (mdb_modinfo_t));
		if (mod->mod_info->mi_dcmds == NULL)
			mod->mod_info->mi_dcmds = empty_dcmd_list;
		if (mod->mod_info->mi_walkers == NULL)
			mod->mod_info->mi_walkers = empty_walk_list;
		break;
	default:
		/*
		 * Too old to be compatible -- abort the load.
		 */
		warn("%s module is compiled for obsolete mdb API "
		    "version %hu\n", name, info->mi_dvers);
		goto err;
	}

	/*
	 * Before we actually go ahead with the load, we need to check
	 * each dcmd and walk structure for any invalid values:
	 */
	for (dcp = &mod->mod_info->mi_dcmds[0]; dcp->dc_name != NULL; dcp++) {
		if (strbadid(dcp->dc_name) != NULL) {
			warn("dcmd name '%s' contains illegal characters\n",
			    dcp->dc_name);
			goto err;
		}

		if (dcp->dc_descr == NULL) {
			warn("dcmd '%s' must have a description\n",
			    dcp->dc_name);
			goto err;
		}

		if (dcp->dc_funcp == NULL) {
			warn("dcmd '%s' has a NULL function pointer\n",
			    dcp->dc_name);
			goto err;
		}
	}

	for (wp = &mod->mod_info->mi_walkers[0]; wp->walk_name != NULL; wp++) {
		if (strbadid(wp->walk_name) != NULL) {
			warn("walk name '%s' contains illegal characters\n",
			    wp->walk_name);
			goto err;
		}

		if (wp->walk_descr == NULL) {
			warn("walk '%s' must have a description\n",
			    wp->walk_name);
			goto err;
		}

		if (wp->walk_step == NULL) {
			warn("walk '%s' has a NULL walk_step function\n",
			    wp->walk_name);
			goto err;
		}
	}

	/*
	 * Now that we've established that there are no problems,
	 * we can go ahead and hash the module, and its dcmds and walks:
	 */
	v = mdb_nv_insert(&mdb.m_modules, name, NULL,
	    (uintptr_t)mod, MDB_NV_RDONLY);

	mod->mod_name = mdb_nv_get_name(v);

	for (dcp = &mod->mod_info->mi_dcmds[0]; dcp->dc_name != NULL; dcp++) {
		if (mdb_module_add_dcmd(mod, dcp, mode) == -1)
			warn("failed to load dcmd %s`%s", name, dcp->dc_name);
	}

	for (wp = &mod->mod_info->mi_walkers[0]; wp->walk_name != NULL; wp++) {
		if (mdb_module_add_walker(mod, wp, mode) == -1)
			warn("failed to load walk %s`%s", name, wp->walk_name);
	}

	/*
	 * Add the module to the end of the list of modules in load-dependency
	 * order.  We maintain this list so we can unload in reverse order.
	 */
	if (mdb.m_mtail != NULL) {
		ASSERT(mdb.m_mtail->mod_next == NULL);
		mdb.m_mtail->mod_next = mod;
		mod->mod_prev = mdb.m_mtail;
		mdb.m_mtail = mod;
	} else {
		ASSERT(mdb.m_mhead == NULL);
		mdb.m_mtail = mdb.m_mhead = mod;
	}

	mdb.m_lmod = NULL;
	return (mod);

err:
	if (mod->mod_hdl != NULL)
		(void) dlclose(mod->mod_hdl);

	mdb_nv_destroy(&mod->mod_dcmds);
	mdb_nv_destroy(&mod->mod_walkers);

	mdb_free(mod->mod_info, sizeof (mdb_modinfo_t));
	mdb_free(mod, sizeof (mdb_module_t));

	mdb.m_lmod = NULL;
	return (NULL);
}

mdb_module_t *
mdb_module_load(const char *name, int mode)
{
	const char *wformat = "no module '%s' could be found\n";
	const char *fullname = NULL;
	char buf[MAXPATHLEN], *p;
	int i;

	if (strchr(name, '/') != NULL) {
		ASSERT(!(mode & MDB_MOD_BUILTIN));

		(void) mdb_iob_snprintf(buf, sizeof (buf), "%s",
		    strbasename(name));
		if ((p = strchr(buf, '.')) != NULL)
			*p = '\0'; /* eliminate suffixes */

		fullname = name;
		name = buf;
	}

	if (strlen(name) > MDB_NV_NAMELEN) {
		wformat = "module name '%s' exceeds name length limit\n";
		goto err;
	}

	if (strbadid(name) != NULL) {
		wformat = "module name '%s' contains illegal characters\n";
		goto err;
	}

	if (mdb_nv_lookup(&mdb.m_modules, name) != NULL) {
		wformat = "%s module is already loaded\n";
		goto err;
	}

	if (fullname != NULL) {
		if (access(fullname, F_OK) != 0) {
			name = fullname; /* for warn() below */
			goto err;
		}
		return (module_create(name, fullname, mode));
	}

	if (mode & MDB_MOD_BUILTIN)
		return (module_create(name, NULL, mode));

	for (i = 0; mdb.m_lpath[i] != NULL; i++) {
		(void) mdb_iob_snprintf(buf, sizeof (buf), "%s/%s.so",
		    mdb.m_lpath[i], name);

		mdb_dprintf(MDB_DBG_MODULE, "checking for %s\n", buf);

		if (access(buf, F_OK) == 0)
			return (module_create(name, buf, mode));
	}
err:
	if (!(mode & MDB_MOD_SILENT))
		warn(wformat, name);

	return (NULL);
}

int
mdb_module_unload(const char *name)
{
	mdb_var_t *v = mdb_nv_lookup(&mdb.m_modules, name);
	mdb_module_t *mod;

	if (v == NULL)
		return (set_errno(EMDB_NOMOD));

	mod = mdb_nv_get_cookie(v);

	if (mod == &mdb.m_rmod || mod->mod_hdl == NULL)
		return (set_errno(EMDB_BUILTINMOD));

	mdb_dprintf(MDB_DBG_MODULE, "unloading %s\n", name);

	if (mod->mod_fini != NULL) {
		mdb_dprintf(MDB_DBG_MODULE, "calling %s`_mdb_fini\n", name);
		mod->mod_fini();
	}

	mdb_nv_remove(&mdb.m_modules, v);
	ASSERT(mdb.m_mtail != NULL && mdb.m_mhead != NULL);

	if (mod->mod_prev == NULL) {
		ASSERT(mdb.m_mhead == mod);
		mdb.m_mhead = mod->mod_next;
	} else
		mod->mod_prev->mod_next = mod->mod_next;

	if (mod->mod_next == NULL) {
		ASSERT(mdb.m_mtail == mod);
		mdb.m_mtail = mod->mod_prev;
	} else
		mod->mod_next->mod_prev = mod->mod_prev;

	while (mdb_nv_size(&mod->mod_walkers) != 0) {
		mdb_nv_rewind(&mod->mod_walkers);
		v = mdb_nv_peek(&mod->mod_walkers);
		(void) mdb_module_remove_walker(mod, mdb_nv_get_name(v));
	}

	while (mdb_nv_size(&mod->mod_dcmds) != 0) {
		mdb_nv_rewind(&mod->mod_dcmds);
		v = mdb_nv_peek(&mod->mod_dcmds);
		(void) mdb_module_remove_dcmd(mod, mdb_nv_get_name(v));
	}

	(void) dlclose(mod->mod_hdl);

	mdb_nv_destroy(&mod->mod_walkers);
	mdb_nv_destroy(&mod->mod_dcmds);

	mdb_free(mod->mod_info, sizeof (mdb_modinfo_t));
	mdb_free(mod, sizeof (mdb_module_t));

	return (0);
}

/*ARGSUSED*/
static int
module_load(void *ignored, const mdb_map_t *map, const char *name)
{
	if (mdb_module_load(strbasename(name),
	    MDB_MOD_LOCAL | MDB_MOD_SILENT) != NULL && mdb.m_term != NULL) {
		mdb_iob_printf(mdb.m_out, " %s", name);
		mdb_iob_flush(mdb.m_out);
	}

	return (0);
}

/*ARGSUSED*/
static int
module_nop(void *ignored, const mdb_map_t *map, const char *name)
{
	return (-1); /* Just force t_object_iter to return immediately */
}

void
mdb_module_load_all(void)
{
	const char *platform = mdb_tgt_platform(mdb.m_target);
	uint_t oflag;

	if (mdb_tgt_object_iter(mdb.m_target, module_nop, NULL) != 0)
		return; /* Don't do anything if object iter isn't ready yet */

	if (mdb.m_term != NULL) {
		oflag = mdb_iob_getflags(mdb.m_out) & MDB_IOB_PGENABLE;
		mdb_iob_clrflags(mdb.m_out, oflag);
		mdb_iob_puts(mdb.m_out, "Loading modules: [");
		mdb_iob_flush(mdb.m_out);
	}

	if (mdb_module_load(platform,
	    MDB_MOD_LOCAL | MDB_MOD_SILENT) != NULL && mdb.m_term != NULL) {
		mdb_iob_printf(mdb.m_out, " %s", platform);
		mdb_iob_flush(mdb.m_out);
	}

	(void) mdb_tgt_object_iter(mdb.m_target, module_load, NULL);

	if (mdb.m_term != NULL) {
		mdb_iob_puts(mdb.m_out, " ]\n");
		mdb_iob_setflags(mdb.m_out, oflag);
	}
}

void
mdb_module_unload_all(void)
{
	mdb_module_t *mod, *pmod;

	/*
	 * We unload modules in the reverse order in which they were loaded
	 * so as to allow _mdb_fini routines to invoke code which may be
	 * present in a previously-loaded module (such as mdb_ks, etc.).
	 */
	for (mod = mdb.m_mtail; mod != NULL; mod = pmod) {
		pmod =  mod->mod_prev;
		(void) mdb_module_unload(mod->mod_name);
	}
}

int
mdb_module_add_dcmd(mdb_module_t *mod, const mdb_dcmd_t *dcp, int flags)
{
	mdb_var_t *v = mdb_nv_lookup(&mod->mod_dcmds, dcp->dc_name);
	mdb_idcmd_t *idcp;

	uint_t nflag = MDB_NV_OVERLOAD | MDB_NV_SILENT;

	if (flags & MDB_MOD_FORCE)
		nflag |= MDB_NV_INTERPOS;

	if (v != NULL)
		return (set_errno(EMDB_DCMDEXISTS));

	idcp = mdb_alloc(sizeof (mdb_idcmd_t), UM_SLEEP);

	idcp->idc_usage = dcp->dc_usage;
	idcp->idc_descr = dcp->dc_descr;
	idcp->idc_help = dcp->dc_help;
	idcp->idc_funcp = dcp->dc_funcp;
	idcp->idc_modp = mod;

	v = mdb_nv_insert(&mod->mod_dcmds, dcp->dc_name, NULL,
	    (uintptr_t)idcp, MDB_NV_SILENT | MDB_NV_RDONLY);

	idcp->idc_name = mdb_nv_get_name(v);
	idcp->idc_var = mdb_nv_insert(&mdb.m_dcmds, idcp->idc_name, NULL,
	    (uintptr_t)v, nflag);

	mdb_dprintf(MDB_DBG_DCMD, "added dcmd %s`%s\n",
	    mod->mod_name, idcp->idc_name);

	return (0);
}

int
mdb_module_remove_dcmd(mdb_module_t *mod, const char *dname)
{
	mdb_var_t *v = mdb_nv_lookup(&mod->mod_dcmds, dname);
	mdb_idcmd_t *idcp;
	mdb_cmd_t *cp;

	if (v == NULL)
		return (set_errno(EMDB_NODCMD));

	mdb_dprintf(MDB_DBG_DCMD, "removed dcmd %s`%s\n", mod->mod_name, dname);
	idcp = mdb_nv_get_cookie(v);

	/*
	 * If we're removing a dcmd that is part of the most recent command,
	 * we need to free mdb.m_lastcp so we don't attempt to execute some
	 * text we've removed from our address space if -o repeatlast is set.
	 */
	for (cp = mdb_list_next(&mdb.m_lastc); cp; cp = mdb_list_next(cp)) {
		if (cp->c_dcmd == idcp) {
			while ((cp = mdb_list_next(&mdb.m_lastc)) != NULL) {
				mdb_list_delete(&mdb.m_lastc, cp);
				mdb_cmd_destroy(cp);
			}
			break;
		}
	}

	mdb_nv_remove(&mdb.m_dcmds, idcp->idc_var);
	mdb_nv_remove(&mod->mod_dcmds, v);
	mdb_free(idcp, sizeof (mdb_idcmd_t));

	return (0);
}

/*ARGSUSED*/
static int
default_walk_init(mdb_walk_state_t *wsp)
{
	return (WALK_NEXT);
}

/*ARGSUSED*/
static void
default_walk_fini(mdb_walk_state_t *wsp)
{
	/* Nothing to do here */
}

int
mdb_module_add_walker(mdb_module_t *mod, const mdb_walker_t *wp, int flags)
{
	mdb_var_t *v = mdb_nv_lookup(&mod->mod_walkers, wp->walk_name);
	mdb_iwalker_t *iwp;

	uint_t nflag = MDB_NV_OVERLOAD | MDB_NV_SILENT;

	if (flags & MDB_MOD_FORCE)
		nflag |= MDB_NV_INTERPOS;

	if (v != NULL)
		return (set_errno(EMDB_WALKEXISTS));

	if (wp->walk_descr == NULL || wp->walk_step == NULL)
		return (set_errno(EINVAL));

	iwp = mdb_alloc(sizeof (mdb_iwalker_t), UM_SLEEP);

	iwp->iwlk_descr = strdup(wp->walk_descr);
	iwp->iwlk_init = wp->walk_init;
	iwp->iwlk_step = wp->walk_step;
	iwp->iwlk_fini = wp->walk_fini;
	iwp->iwlk_init_arg = wp->walk_init_arg;
	iwp->iwlk_modp = mod;

	if (iwp->iwlk_init == NULL)
		iwp->iwlk_init = default_walk_init;
	if (iwp->iwlk_fini == NULL)
		iwp->iwlk_fini = default_walk_fini;

	v = mdb_nv_insert(&mod->mod_walkers, wp->walk_name, NULL,
	    (uintptr_t)iwp, MDB_NV_SILENT | MDB_NV_RDONLY);

	iwp->iwlk_name = mdb_nv_get_name(v);
	iwp->iwlk_var = mdb_nv_insert(&mdb.m_walkers, iwp->iwlk_name, NULL,
	    (uintptr_t)v, nflag);

	mdb_dprintf(MDB_DBG_WALK, "added walk %s`%s\n",
	    mod->mod_name, iwp->iwlk_name);

	return (0);
}

int
mdb_module_remove_walker(mdb_module_t *mod, const char *wname)
{
	mdb_var_t *v = mdb_nv_lookup(&mod->mod_walkers, wname);
	mdb_iwalker_t *iwp;

	if (v == NULL)
		return (set_errno(EMDB_NOWALK));

	mdb_dprintf(MDB_DBG_WALK, "removed walk %s`%s\n", mod->mod_name, wname);

	iwp = mdb_nv_get_cookie(v);
	mdb_nv_remove(&mdb.m_walkers, iwp->iwlk_var);
	mdb_nv_remove(&mod->mod_walkers, v);

	strfree(iwp->iwlk_descr);
	mdb_free(iwp, sizeof (mdb_iwalker_t));

	return (0);
}
