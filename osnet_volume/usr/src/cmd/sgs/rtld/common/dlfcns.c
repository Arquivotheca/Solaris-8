/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dlfcns.c	1.116	99/10/07 SMI"


/*
 * Programmatic interface to the run_time linker.
 */
#include	"_synonyms.h"

#include	<string.h>
#include	<dlfcn.h>
#include	<synch.h>
#include	"profile.h"
#include	"debug.h"
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"


/*
 * Determine who called us - given a pc determine in which object it resides.
 *
 * For dlopen() the link map of the caller must be passed to load_so() so that
 * the appropriate search rules (4.x or 5.0) are used to locate any
 * dependencies.  Also, if we've been called from a 4.x module it may be
 * necessary to fix the specified pathname so that it conforms with the 5.0 elf
 * rules.
 *
 * For dlsym() the link map of the caller is used to determine RTLD_NEXT
 * requests, together with requests based off of a dlopen(0).
 *
 * If we can't figure out the calling link map assume it's the executable.  It
 * is assumed that at least a readers lock is held when this function is called.
 */
static Rt_map *
_caller(unsigned long cpc)
{
	Lm_list *	lml;
	Listnode *	lnp;

	for (LIST_TRAVERSE(&dynlm_list, lnp, lml)) {
		Rt_map *	tlmp;

		for (tlmp = lml->lm_head; tlmp; tlmp = (Rt_map *)NEXT(tlmp)) {
			if ((cpc > ADDR(tlmp)) && (cpc < (ETEXT(tlmp))) ||
			    (SDATA(tlmp) && (cpc >= SDATA(tlmp)) &&
				(cpc < (ADDR(tlmp) + MSIZE(tlmp)))))
				return (tlmp);
		}
	}
	return ((Rt_map *)lml_main.lm_head);
}

/*
 * Return a pointer to the string describing the last occurring error.  The
 * last occurring error is cleared.
 */
#pragma weak dlerror = _dlerror

char *
_dlerror()
{
	char *	_lasterr = lasterr;

	PRF_MCOUNT(20, _dlerror);

	lasterr = (char *)0;
	return (_lasterr);
}

/*
 * Remove a Dl_handle structure. Free up any permit, and remove it from the
 * associated handle list.
 */
void
hdl_free(Dl_handle * dlp)
{
	unsigned int	ndx;

	PRF_MCOUNT(107, hdl_free);

	if (dlp->dl_permit != 0)
		perm_free(dlp->dl_permit);

	free(dlp);

	/* LINTED */
	ndx = (unsigned int)dlp % HDLISTSZ;

	(void) list_delete(&hdl_list[ndx], dlp);
}

/*
 * Add an object to the handle dependencies, propagating any permissions if
 * required.  Returns 0 on failure, 1 if the dependency already exists, or 2
 * if it is newly added.
 */
int
hdl_add(Dl_handle * dlp, List * list, Rt_map * nlmp, Rt_map * clmp)
{
	Rt_map *	tlmp;
	Listnode *	lnp;

	/*
	 * Make sure this dependency hasn't already been recorded.
	 */
	for (LIST_TRAVERSE(list, lnp, tlmp)) {
		if (nlmp == tlmp)
			return (1);
	}

	if (dlp->dl_permit) {
		/*
		 * A nice optimization here would seem to be if the dependency
		 * is a non-deletable global object and this object is able to
		 * look in any objects, there's no point in giving it a permit,
		 * as we can safely bind to it and it'll never go away.  However
		 * we need to establish permit/donor state so that this object
		 * is added to the handles dependencies (see load_one()).
		 */
		PERMIT(nlmp) = perm_set(PERMIT(nlmp), dlp->dl_permit);
		if (list_append(&DONORS(nlmp), dlp) == 0)
			return (0);
		dlp->dl_permcnt++;
		DBG_CALL(Dbg_file_bind_entry(clmp, nlmp));
	}

	/*
	 * Append the new dependency.
	 */
	if (list_append(list, nlmp) == 0)
		return (0);

	return (2);
}

Dl_handle *
hdl_alloc()
{
	Dl_handle *	dlp;
	unsigned int	ndx;

	if ((dlp = calloc(sizeof (Dl_handle), 1)) == 0)
		return (0);

	/* LINTED */
	ndx = (unsigned int)dlp % HDLISTSZ;

	if (list_append(&hdl_list[ndx], dlp) == 0) {
		free(dlp);
		return (0);
	}
	return (dlp);
}

Dl_handle *
hdl_create(Lm_list * lml, Rt_map * nlmp, Rt_map * clmp, int parent)
{
	Dl_handle **	dlpp, * dlp;

	/*
	 * Determine if a handle already exists.  For dlopen(0) the handle is
	 * maintained as part of the link-map list, otherwise it is associated
	 * with the referenced link-map.
	 */
	if (nlmp)
		dlpp = &(HANDLE(nlmp));
	else
		dlpp = &(lml->lm_handle);

	if ((dlp = *dlpp) == 0) {
		/*
		 * If this is the first dlopen() request for this handle
		 * allocate and initialize a new handle.
		 */
		if ((dlp = hdl_alloc()) == 0)
			return (0);
		*dlpp = dlp;

		dlp->dl_usercnt = 1;

		/*
		 * A dlopen(0) handle is identified by a permit of 0, the head
		 * of the link-map list is defined as the only dependency.
		 * When this handle is used (for dlsym) a dynamic search through
		 * the entire link-map list will locate all objects with GLOBAL
		 * visibility.
		 *
		 * Another special case is for ld.so.1 (which is effectively
		 * dlopen()'ed as a filtee of libdl.so.1).  We don't want any of
		 * it's dependencies available to the user and there's no need
		 * to create a permit for this case as ld.so.1 shouldn't require
		 * to bind to libdl.so.1.
		 */
		if (nlmp == 0)
			nlmp = lml->lm_head;
		else if (nlmp != lml_rtld.lm_head) {
			if ((dlp->dl_permit = perm_get()) == 0) {
				hdl_free(dlp);
				return (0);
			}
		}

		if (hdl_add(dlp, &dlp->dl_depends, nlmp, nlmp) == 0)
			return (0);

	} else {
		/*
		 * If a Dl_handle already exists bump its reference count.
		 */
		dlp->dl_usercnt++;
	}

	/*
	 * If we're working with a dlopen(0) or ld.so.1, we're done.
	 */
	if ((nlmp == 0) || (nlmp == lml_rtld.lm_head))
		return (dlp);

	/*
	 * If dlopen(..., RTLD_PARENT) we need to add our permissions to the
	 * callers (parent), as we could be opened by different parents this
	 * test is carried out every time a handle is requested.
	 */
	if (parent) {
		if (hdl_add(dlp, &dlp->dl_parents, clmp, nlmp) == 0)
			return (0);
	}

	return (dlp);
}


/*
 * Open a shared object.  Three levels of dlopen are provided:
 *
 * _dlmopen	is commonly called from the user (via libdl.so) and provides
 *		for argument verification, determination of the caller, and
 *		any cleanup necessary before return to the user.
 *
 * dlmopen_lock	insures that the appropriate locks are obtained and released,
 *		and that any new init sections are called.
 *
 * dlmopen_core	provides the basic underlying functionality.
 *
 * On success, returns a pointer (handle) to the structure containing
 * information about the newly added object, ie. can be used by dlsym(). On
 * failure, returns a null pointer.
 */
Dl_handle *
dlmopen_core(Lm_list * lml, const char * path, int omode, Rt_map * clmp,
    uint_t flags)
{
	Rt_map *	nlmp;
	char *		name;

	PRF_MCOUNT(22, dlmopen_core);

	DBG_CALL(Dbg_file_dlopen((path ? path : MSG_ORIG(MSG_STR_ZERO)),
	    NAME(clmp), omode));

	/*
	 * Check for magic link-map list values:
	 *
	 *  LM_ID_BASE:		Operate on the PRIMARY (executables) link map
	 *  LM_ID_LDSO:		Operation on ld.so.1's link map
	 *  LM_ID_NEWLM: 	Create a new link-map.
	 */
	if (lml == (Lm_list *)LM_ID_NEWLM) {
		if ((lml = calloc(sizeof (Lm_list), 1)) == 0)
			return (0);

		/*
		 * Establish the new link-map flags from the callers and those
		 * explicitly provided.
		 */
		lml->lm_flags = (LIST(clmp)->lm_flags & LML_MSK_NEWLM);
		if (flags & FLG_RT_AUDIT) {
			lml->lm_flags &= ~LML_MSK_AUDIT;
			lml->lm_flags |= LML_FLG_NOAUDIT;
		}

		if (list_append(&dynlm_list, lml) == 0) {
			free(lml);
			return (0);
		}
	} else if ((uintptr_t)lml < LM_ID_NUM) {
		if ((uintptr_t)lml == LM_ID_BASE)
			lml = &lml_main;
		else if ((uintptr_t)lml == LM_ID_LDSO)
			lml = &lml_rtld;
	}

	/*
	 * If the path specified is null then we're operating on global
	 * objects.  Associate a dummy Dl_handle with the link-map list.
	 */
	if (!path) {
		if (hdl_create(lml, 0, clmp, (omode & RTLD_PARENT)) == 0) {
			remove_so(lml, 0);
			return (0);
		}

		/*
		 * crle()'s second pass loads all objects necessary for building
		 * a configuration file, however none of them have yet been
		 * relocated.  This test allows crle(1) to relocate everything
		 * it has just loaded.
		 */
		nlmp = lml->lm_head;
		if ((FLAGS(nlmp) & FLG_RT_RELOCED) == 0)
			(void) relocate_so(nlmp);

		return (lml->lm_handle);
	}

	/*
	 * Fix the pathname if necessary and load the object.  From this load
	 * we should be left with a new link-map, added to the required link-map
	 * list, and with it will have been created a new handle.
	 */
	if ((name = LM_FIX_NAME(clmp)(path, clmp)) == 0)
		return (0);
	nlmp = load_one(lml, name, clmp, omode, (flags | FLG_RT_HANDLE));
	free(name);

	if (nlmp == 0)
		return (0);

	/*
	 * Analyze and relocate the object family unless we've been asked for a
	 * CONFGEN load.  CONFGEN is used by crle(1) to load objects prior to
	 * dldump'ing them - in this case we simply need to analyze them to
	 * insure and directly bound dependencies are established, all
	 * relocation will be carried out later once all objects are loaded.
	 * If anything fails from this point use dlclose() to clean up.
	 */
	if ((analyze_so(nlmp) == 0) ||
	    (((omode & RTLD_CONFGEN) == 0) && (relocate_so(nlmp) == 0))) {
		(void) dlclose_core(HANDLE(nlmp), nlmp, clmp);
		return (0);
	}

	return (HANDLE(nlmp));
}

Dl_handle *
dlmopen_lock(Lm_list * lml, const char * path, int mode, Rt_map * clmp)
{
	Dl_handle *	dlp;
	int		bind, flags = 0;
	Rt_map **	tobj = 0;

	PRF_MCOUNT(23, dlmopen_lock);

	/*
	 * Verify that a valid pathname has been supplied.
	 */
	if (path && (*path == '\0')) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLPATH));
		return (0);
	}

	/*
	 * Historically we've always verified the mode is either RTLD_NOW or
	 * RTLD_LAZY.  RTLD_NOLOAD is valid by itself.
	 */
	if (((mode & (RTLD_NOW | RTLD_LAZY | RTLD_NOLOAD)) == 0) ||
	    ((mode & (RTLD_NOW | RTLD_LAZY)) == (RTLD_NOW | RTLD_LAZY)) ||
	    (!path && (lml == (Lm_list *)LM_ID_NEWLM))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLMODE), mode);
		return (0);
	}
	if (((mode & (RTLD_GROUP | RTLD_WORLD)) == 0) && !(mode & RTLD_NOLOAD))
		mode |= (RTLD_GROUP | RTLD_WORLD);

	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_wrlock(&bindlock);

	dlp = dlmopen_core(lml, path, mode, clmp, flags);

	if (bind) {
		if ((dlp != 0) && ((mode & RTLD_CONFGEN) == 0)) {
			/*
			 * If objects were loaded determine if any .init
			 * sections need firing.
			 */
			Rt_map *	lmp = HDLHEAD(dlp);

			if ((tobj = tsort(lmp, LIST(lmp)->lm_init,
			    RT_SORT_REV)) == (Rt_map **)S_ERROR)
				tobj = 0;
		}

		if (rtld_flags & RT_FL_CLEANUP)
			cleanup();

		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}
	if ((LIST(clmp)->lm_flags | FLAGS1(clmp)) & FL1_AU_ACTIVITY)
		audit_activity(clmp, LA_ACT_CONSISTENT);

	/*
	 * After releasing any locks call any .init sections if necessary.
	 */
	if (tobj)
		call_init(tobj);

	return (dlp);
}

#pragma weak dlmopen = _dlmopen

void *
_dlmopen(Lmid_t lmid, const char * path, int mode)
{
	Dl_handle *	dlp;

	PRF_MCOUNT(24, _dlmopen);
	dlp = dlmopen_lock((Lm_list *)lmid, path, mode, _caller(caller()));
	return ((void *)dlp);
}

#pragma weak dlopen = _dlopen
void *
_dlopen(const char * path, int mode)
{
	Dl_handle *	dlp;
	Rt_map *	clmp;

	PRF_MCOUNT(25, _dlopen);

	clmp = _caller(caller());
	dlp = dlmopen_lock(LIST(clmp), path, mode, clmp);
	return ((void *)dlp);
}

/*
 * Sanity check a program-provided dlp handle.
 */
static int
hdl_validate(Dl_handle * dlp)
{
	Listnode *	lnp;
	Dl_handle *	_dlp;
	unsigned int	ndx;

	/* LINTED */
	ndx = (unsigned int)dlp % HDLISTSZ;

	for (LIST_TRAVERSE(&hdl_list[ndx], lnp, _dlp))
		if ((_dlp == dlp) && (dlp->dl_usercnt != 0))
			return (1);

	return (0);
}

/*
 * Search for a specified symbol.  Four levels of dlsym are provided:
 *
 * _dlsym	is commonly called from the user (via libdl.so) and provides
 *		for argument verification, determination of the caller, and
 *		any cleanup necessary before return to the user.
 *
 * dlsym_lock	insures that the appropriate locks are obtained and released.
 *
 * dlsym_core	provides an interface to lower lying functionality by switching
 *		on the handle type.
 *
 * dlsym_handle	provides standard handle traversal.
 *
 * On success, returns a the address of the specified symbol. On error returns
 * a null.
 */
Sym *
dlsym_handle(Dl_handle * dlp, Slookup * sl, Rt_map ** _lmp)
{
	Rt_map *	nlmp, * lmp = (Rt_map *)dlp->dl_depends.head->data;
	Rt_map *	clmp = sl->sl_cmap;
	const char *	name = sl->sl_name;
	int		flags = (LKUP_DEFT | LKUP_FIRST | LKUP_SPEC);
	Sym *		sym = 0;

	/*
	 * Continue processing a dlsym request.  Lookup the required symbol in
	 * each link-map specified by the dlp.
	 *
	 * To leverage off of lazy loading, dlsym() requests can result in two
	 * passes.  The first descends the link-maps of any objects already in
	 * the address space.  If the symbol isn't located, and lazy
	 * dependencies still exist, then a second pass is made to load these
	 * dependencies if applicable.  This model means that in the case where
	 * a symbols exists in more than one object, the one located may not be
	 * constant - this is the standard issue with lazy loading. In addition,
	 * attempting to locate a symbol that doesn't exist will result in the
	 * loading of all lazy dependencies on the given handle, which can
	 * defeat some of the advantages of lazy loading (look out JVM).
	 */
	if (dlp->dl_permit == 0) {
		/*
		 * If this symbol lookup is triggered from a dlopen(0) handle
		 * traverse the present link-map list looking for promiscuous
		 * entries.
		 */
		for (nlmp = lmp; nlmp; nlmp = (Rt_map *)NEXT(nlmp)) {
			if (!(MODE(nlmp) & RTLD_GLOBAL))
				continue;
			sl->sl_imap = nlmp;
			if (sym = LM_LOOKUP_SYM(clmp)(sl, _lmp, flags))
				return (sym);
		}

		/*
		 * If we're unable to locate the symbol and this link-map still
		 * has pending lazy dependencies, start loading them in an
		 * attempt to exhaust the search.  Note that as we're already
		 * traversing a dynamic linked list of link-maps there's no
		 * need for elf_lazy_find_sym() to descend the link-maps itself.
		 */
		if (LIST(lmp)->lm_lazy) {
			DBG_CALL(Dbg_syms_lazy_rescan(name));

			for (nlmp = lmp; nlmp; nlmp = (Rt_map *)NEXT(nlmp)) {
				if (!(MODE(nlmp) & RTLD_GLOBAL) || !LAZY(nlmp))
					continue;
				sl->sl_imap = nlmp;
				if (sym = elf_lazy_find_sym(sl, _lmp,
				    (flags | LKUP_NODESCENT)))
					return (sym);
			}
		}
	} else {
		/*
		 * Traverse the dlopen() handle for the presently loaded
		 * link-maps.
		 */
		Listnode *	lnp;

		for (LIST_TRAVERSE(&dlp->dl_depends, lnp, nlmp)) {
			sl->sl_imap = nlmp;
			if (sym = LM_LOOKUP_SYM(clmp)(sl, _lmp, flags))
				return (sym);
		}

		/*
		 * If we're unable to locate the symbol and this link-map still
		 * has pending lazy dependencies, start loading them in an
		 * attempt to exhaust the search.
		 */
		if (LIST(lmp)->lm_lazy) {
			DBG_CALL(Dbg_syms_lazy_rescan(name));

			for (LIST_TRAVERSE(&dlp->dl_depends, lnp, nlmp)) {
				if (!LAZY(nlmp))
					continue;
				sl->sl_imap = nlmp;
				if (sym = elf_lazy_find_sym(sl, _lmp, flags))
					return (sym);
			}
		}
	}
	return ((Sym *)0);
}

void *
dlsym_core(void * handle, const char * name, Rt_map * clmp)
{
	Rt_map *	_lmp;
	Sym *		sym;
	Slookup		sl;

	PRF_MCOUNT(27, dlsym_core);

	sl.sl_name = name;
	sl.sl_permit = PERMIT(clmp);
	sl.sl_cmap = clmp;
	sl.sl_rsymndx = 0;

	if (handle == RTLD_NEXT) {
		/*
		 * If the handle is RTLD_NEXT start searching in the next link
		 * map from the callers.  Determine permissions from the
		 * present link map.  Indicate to lookup_sym() that we're on an
		 * RTLD_NEXT request so that it will use the callers link map to
		 * start any possible lazy dependency loading.
		 */
		sl.sl_imap = (Rt_map *)NEXT(clmp);

		DBG_CALL(Dbg_syms_dlsym(sl.sl_imap ? NAME(sl.sl_imap) :
		    MSG_INTL(MSG_STR_NULL), name, DBG_DLSYM_NEXT));

		if (sl.sl_imap == 0)
			return (0);

		sym = LM_LOOKUP_SYM(clmp)(&sl, &_lmp, (LKUP_DEFT | LKUP_NEXT));

	} else if (handle == RTLD_DEFAULT) {
		/*
		 * If the handle is RTLD_DEFAULT start searching at the head of
		 * the link map list, just as a regular symbol resolution would.
		 * Determine permissions from the present link map.
		 */
		sl.sl_imap = LIST(clmp)->lm_head;

		DBG_CALL(Dbg_syms_dlsym(NAME(sl.sl_imap), name,
		    DBG_DLSYM_DEFAULT));
		sym = LM_LOOKUP_SYM(clmp)(&sl, &_lmp, (LKUP_DEFT | LKUP_SPEC));

	} else {
		Dl_handle *	dlp = (Dl_handle *)handle;

		/*
		 * Look in the shared object specified by the handle and in all
		 * of its dependencies.
		 */
		DBG_CALL(Dbg_syms_dlsym(NAME((Rt_map *)
		    (dlp->dl_depends.head->data)), name, DBG_DLSYM_DEF));
		sym = LM_DLSYM(clmp)(dlp, &sl, &_lmp);
	}

	if (sym) {
		Addr	addr = sym->st_value;

		if (!(FLAGS(_lmp) & FLG_RT_FIXED))
			addr += ADDR(_lmp);

		DBG_CALL(Dbg_bind_global(NAME(clmp), 0, 0, (uint_t)-1,
		    NAME(_lmp), (caddr_t)addr, (caddr_t)sym->st_value,
		    name));

#if	defined(__ia64)
		/*
		 * For functions return the descriptor to the user.
		 */
		if (ELF_ST_TYPE(sym->st_info) == STT_FUNC) {
			if (find_fptr(_lmp, &addr) == 0)
				return (0);
		}
#endif
		if ((LIST(clmp)->lm_flags | FLAGS1(clmp)) & FL1_AU_SYMBIND) {
			uint_t	sb_flags = LA_SYMB_DLSYM;
			/* LINTED */
			uint_t	symndx = (uint_t)(((Xword)sym -
			    (Xword)SYMTAB(_lmp)) / SYMENT(_lmp));
			addr = audit_symbind(clmp, _lmp, sym, symndx, addr,
			    &sb_flags);
		}
		return ((void *)addr);
	} else
		return (0);
}

void *
dlsym_lock(void * handle, const char * name, Rt_map * clmp)
{
	void *		error;
	int		bind;
	Rt_map **	tobj = 0;

	PRF_MCOUNT(28, dlsym_lock);

	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_rdlock(&bindlock);

	error = dlsym_core(handle, name, clmp);

	if (bind) {
		/*
		 * If objects were loaded determine if any .init sections need
		 * firing.  Note that we don't know exactly where any new
		 * objects are loaded (we know the object that supplied the
		 * symbol, but others may have been loaded lazily as we
		 * searched for the symbol).
		 *
		 * Note, error may equal 0 because no symbol was found, but
		 * objects might still have been loaded, thus don't check for
		 * .init's based on a non-zero error.
		 */
		if ((tobj = tsort(clmp, LIST(clmp)->lm_init,
		    RT_SORT_REV)) == (Rt_map **)S_ERROR)
			tobj = 0;

		if (rtld_flags & RT_FL_CLEANUP)
			cleanup();

		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}

	if ((LIST(clmp)->lm_flags | FLAGS1(clmp)) & FL1_AU_ACTIVITY)
		audit_activity(clmp, LA_ACT_CONSISTENT);

	/*
	 * After releasing any locks call any .init sections if necessary.
	 */
	if (tobj)
		call_init(tobj);

	return (error);
}

#pragma weak dlsym = _dlsym

void *
_dlsym(void * handle, const char * name)
{
	void *	error;

	PRF_MCOUNT(29, _dlsym);

	/*
	 * Verify the arguments.
	 */
	if (name == 0) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLSYM));
		return (0);
	}
	if ((handle != RTLD_NEXT) && (handle != RTLD_DEFAULT) &&
	    (!hdl_validate((Dl_handle *)handle))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INVHNDL));
		return (0);
	}

	/*
	 * Determine the symbols address.  Clean up any temporary memory
	 * mappings and file descriptors.
	 */
	error = dlsym_lock(handle, name, _caller(caller()));
	if (error == 0)
		eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_NOSYM), name);
	return (error);
}

/*
 * Close a shared object.  Three levels of dlclose are provided:
 *
 * _dlclose	is commonly called from the user (via libdl.so) and provides
 *		for handle verification before calling the lock level.
 *
 * dlclose_lock	insures that the appropriate locks are obtained and released.
 *
 * dlclose_core	provides the basic underlying functionality.
 *
 * On success returns 0, and on failure 1.
 */
int
dlclose_core(Dl_handle * dlp, Rt_map * dlmp, Rt_map * clmp)
{
	PRF_MCOUNT(90, dlclose_core);

	if (dlmp == 0) {
		DBG_CALL(Dbg_file_dlclose(MSG_ORIG(MSG_STR_ZERO), 1));
	} else if (MODE(dlmp) & RTLD_NODELETE) {
		DBG_CALL(Dbg_file_dlclose(NAME(dlmp), 1));
	} else {
		DBG_CALL(Dbg_file_dlclose(NAME(dlmp), 0));
	}

	/*
	 * Decrement reference count of this object.
	 */
	if (--(dlp->dl_usercnt))
		return (0);

	/*
	 * If this handle is special (dlopen(0)), then leave it around - it
	 * has little overhead.
	 */
	if (dlmp == 0)
		return (0);

	/*
	 * This dlopen handle is no longer being referenced.
	 */
	DBG_CALL(Dbg_file_bind_title(REF_DLCLOSE));
	DBG_CALL(Dbg_file_bind_entry(dlmp, dlmp));

	return (remove_hdl(dlp, dlmp, clmp));
}

int
dlclose_lock(Dl_handle * dlp, Rt_map * dlmp, Rt_map * clmp)
{
	int	error, bind;

	PRF_MCOUNT(91, dlclose_lock);

	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_wrlock(&bindlock);

	/*
	 * If we're already at atexit() there's no point processing further,
	 * all objects have already been tsorted for fini processing.
	 */
	if ((rtld_flags & RT_FL_ATEXIT) == 0)
		error = dlclose_core(dlp, dlmp, clmp);
	else
		error = 0;

	if (bind) {
		if (rtld_flags & RT_FL_CLEANUP)
			cleanup();

		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}

	if ((LIST(clmp)->lm_flags | FLAGS1(clmp)) & FL1_AU_ACTIVITY)
		audit_activity(clmp, LA_ACT_CONSISTENT);

	return (error);
}

#pragma weak dlclose = _dlclose

int
_dlclose(void * handle)
{
	Dl_handle *	dlp = (Dl_handle *)handle;
	Rt_map *	dlmp;

	PRF_MCOUNT(92, _dlclose);

	if (!hdl_validate(dlp)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INVHNDL));
		return (1);
	}
	if (dlp->dl_permit != 0)
		dlmp = (Rt_map *)dlp->dl_depends.head->data;
	else
		dlmp = 0;

	return (dlclose_lock(dlp, dlmp, _caller(caller())));
}

/*
 * Return an information structure that reflects the symbol closest to the
 * address specified.
 */
int
__dladdr(void * addr, Dl_info * dlip)
{
	Rt_map *	lmp;

	PRF_MCOUNT(93, __dladdr);

	/*
	 * Scan the executables link map list to determine which image covers
	 * the required address.
	 */
	lmp = _caller((unsigned long)addr);
	if (((unsigned long) addr < ADDR(lmp)) ||
	    ((unsigned long)addr > (ADDR(lmp) + MSIZE(lmp)))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INVADDR),
		    EC_ADDR(addr));
		return (0);
	}

	/*
	 * Set up generic information and any defaults.
	 */
	dlip->dli_fname = PATHNAME(lmp);

	dlip->dli_fbase = (void *)ADDR(lmp);
	dlip->dli_sname = 0;
	dlip->dli_saddr = 0;

	/*
	 * Determine the nearest symbol to this address.
	 */
	LM_DLADDR(lmp)((unsigned long)addr, lmp, dlip);
	return (1);
}

#pragma weak dladdr = _dladdr

int
_dladdr(void * addr, Dl_info * dlip)
{
	int	error;
	int	bind;

	PRF_MCOUNT(96, _dladdr);

	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_rdlock(&bindlock);
	error = __dladdr(addr, dlip);
	if (bind) {
		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}
	return (error);
}


#pragma weak dldump = _dldump

int
_dldump(const char * ipath, const char * opath, int flags)
{
	int		error = 1, bind, pfd = 0;
	Addr		addr = 0;
	Rt_map *	lmp;

	PRF_MCOUNT(94, _dldump);

	/*
	 * Verify any arguments first.
	 */
	if ((!opath || (*opath == '\0')) || (ipath && (*ipath == '\0'))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLPATH));
		return (1);
	}

	/*
	 * If an input file is specified make sure its one of our dependencies.
	 */
	if (ipath) {
		if ((bind = bind_guard(THR_FLG_BIND)) == 1)
			(void) rw_rdlock(&bindlock);
		if ((lmp = is_so_loaded(&lml_main, ipath, 0)) == 0)
			lmp = is_so_loaded(&lml_main, ipath, 1);
		if (bind) {
			(void) rw_unlock(&bindlock);
			(void) bind_clear(THR_FLG_BIND);
		}
		if (lmp == 0) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_NOFILE), ipath);
			return (1);
		}
		if (FLAGS(lmp) & FLG_RT_ALTER) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_ALTER), ipath);
			return (1);
		}
		if (FLAGS(lmp) & FLG_RT_NODUMP) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_NODUMP), ipath);
			return (1);
		}
	} else
		lmp = lml_main.lm_head;


	DBG_CALL(Dbg_file_dldump(NAME(lmp), opath, flags));

	/*
	 * If we've been asked to process the dynamic object that started this
	 * process, then obtain a /proc file descriptor.
	 */
	if (lmp == lml_main.lm_head) {
		if ((pfd = pr_open()) == FD_UNAVAIL)
			return (1);
	}
	if (!(FLAGS(lmp) & FLG_RT_FIXED))
		addr = ADDR(lmp);

	/*
	 * Obtain the shared object necessary to perform the real dldump(),
	 * and the shared objects entry point.
	 */
	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_wrlock(&bindlock);

	if (elf_rtld_load(lml_rtld.lm_head) == 0)
		return (0);

	/*
	 * Dump the required image.  Note that we're getting a writer
	 * lock even though we're really only traversing the link-maps
	 * to obtain things like relocation information.  Because we are
	 * bringing libelf() which requires thread interfaces that
	 * aren't all being offered by ld.so.1 yet this seems a safer
	 * option.
	 */
	error = rt_dldump(lmp, opath, pfd, flags, addr);

	if (bind) {
		if (rtld_flags & RT_FL_CLEANUP)
			cleanup();

		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}
	return (error);
}


/*
 * get_linkmap_id() is used to translate Lm_list * pointers to
 * the Link_map id as used by the rtld_db interface and the dlmopen()
 * interface.
 *
 * It will check to see if the Link_map is one of the primary ones
 * and if so returns it's special token:
 *		LM_ID_BASE
 *		LM_ID_LDSO
 *
 * If it's not one of the primary link_map id's it will instead
 * return a pointer to the Lm_list structure which uniquely identifies
 * the Link_map.
 */
Lmid_t
get_linkmap_id(Lm_list * lml)
{
	if (lml->lm_flags & LML_FLG_BASELM)
		return (LM_ID_BASE);
	if (lml->lm_flags & LML_FLG_RTLDLM)
		return (LM_ID_LDSO);

	return ((Lmid_t)lml);
}


/*
 * Extract information for a dlopen() handle.  The valid request are:
 *
 *  RTLD_DI_LMID:	return Lmid_t of the Link-Map list that the current
 *			handle is loaded on.
 *  RTLD_DI_LINKMAP:	return a pointer to the Link-Map structure associated
 *			with the current object.
 *  RTLD_DI_CONFIGADDR:	return configuration cache name and address.
 */
#pragma weak dlinfo = _dlinfo
int
_dlinfo(void * handle, int request, void * p)
{
	Dl_handle *	dlp;
	Rt_map *	lmp;

	if (request > RTLD_DI_MAX) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ILLVAL));
		return (-1);
	}

	if (request == RTLD_DI_CONFIGADDR) {
		Dl_info *	dlip = (Dl_info *)p;

		if ((config->c_name == 0) || (config->c_bgn == 0) ||
		    (config->c_end == 0)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_NOCONFIG));
			return (-1);
		}
		dlip->dli_fname = config->c_name;
		dlip->dli_fbase = (void *)config->c_bgn;
		return (0);
	}

	if (!hdl_validate((Dl_handle *)handle)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_INVHNDL));
		return (-1);
	}
	dlp = (Dl_handle*)handle;
	lmp = (Rt_map *)dlp->dl_depends.head->data;

	switch (request) {
	case RTLD_DI_LMID:
		*(Lmid_t *)p = get_linkmap_id(LIST(lmp));
		break;
	case RTLD_DI_LINKMAP:
		*(Link_map **)p = (Link_map *)lmp;
		break;
	}
	return (0);
}
