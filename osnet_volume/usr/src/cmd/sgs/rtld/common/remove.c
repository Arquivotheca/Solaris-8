/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * Remove objects.  Objects need removal from a process as part of:
 *
 *  o	a dlclose() request
 *
 *  o	tearing down a dlopen() heirarchy that failed to completely load
 *
 * Any other failure condition will result in process exit (in which case all
 * we have to do is execute the fini's - tear down is unnecessary).
 *
 * Any removal of objects is therefore associated with a dlopen() handle.  There
 * is a small window between creation of the first dlopen() object and creating
 * its handle (in which case remove_so() can get rid of the new link-map if
 * necessary), but other than this all object removal is driven by inspecting
 * the components of a handle.
 *
 * Things to note. The creation of a link-map, and its addition to the link-map
 * list occurs in {elf|aout}_new_lm(), if this returns success the link-map is
 * valid and added, otherwise any steps (allocations) in the process of creating
 * the link-map would have been undone.  If some other failure now occurs
 * between creating the link-map and adding it to a handle, then remove_so() is
 * called to nuke the link-map.
 */
#pragma ident	"@(#)remove.c	1.9	99/10/07 SMI"

#include	"_synonyms.h"

#include	<string.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<dlfcn.h>
#include	"libc_int.h"
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"profile.h"
#include	"debug.h"
#include	"msg.h"


/*
 * Call back routines and interfaces for libc.
 *
 * List of those entries provided from libc that ld.so.1 is interested in.
 */
static int ci_interest_list[CI_MAX] = {
/* CI_NULL */			0,
/* CI_VERSION */		0,
/* CI_ATEXIT */			1,
};

/*
 * Define the linkers libc table, a function pointer array for holding the
 * libc functions.
 */
static int (*	ci_def_table[CI_MAX])() = {
	0,					/* CI_NULL */
	0,					/* CI_VERSION */
	0					/* CI_ATEXIT */
};

static int (*	ci_jmp_table[CI_MAX])() = {
	0,					/* CI_NULL */
	0,					/* CI_VERSION */
	0					/* CI_ATEXIT */
};

/*
 * The interface with the c library which is supplied through libdl.so.1.
 * A non-null argument allows a function pointer array to be passed to us which
 * is used to re-initialize the linker libc table.  A null argument causes the
 * table to be reset to the defaults.
 */
void
_ld_libc(void * ptr)
{
	int		tag;
	Lc_interface *	funcs = ptr;

	if (funcs) {
		for (tag = funcs->ci_tag; tag; tag = (++funcs)->ci_tag) {
			if ((tag < CI_MAX) && ci_interest_list[tag] &&
			    (funcs->ci_un.ci_func != 0))
				ci_jmp_table[tag] = funcs->ci_un.ci_func;
		}
	} else {
		for (tag = 0; tag < CI_MAX; tag++)
			ci_jmp_table[tag] = ci_def_table[tag];
	}
}


/*
 * Atexit callback provided by libc.  As part of a dlclose() determine the
 * address ranges of all object to be deleted and pass back to libc's pre-atexit
 * routine.  Libc will purge any registered atexit() calls related to those
 * objects about to be deleted.
 */
static Lc_addr_range_t *	addr = 0;
static unsigned int		anum = 0;

static int
purge_exit_handlers(Rt_map ** tobj)
{
	unsigned int		lnum;
	Rt_map **		_tobj;
	Lc_addr_range_t *	_addr;
	int			error;

	/*
	 * Has a callback been established?
	 */
	if (ci_jmp_table[CI_ATEXIT] == 0)
		return (0);

	/*
	 * Determine the number of loadable sections from the objects being
	 * deleted.  Note, we ignore AOUT's - we could process these by
	 * separating the program header processing into elf and a.out specific
	 * routines, but this doesn't seem a real pressing issue.
	 */
	for (lnum = 0, _tobj = tobj; *_tobj != NULL; _tobj++) {
		Rt_map *	lmp = *_tobj;
		Phdr *		phdr = (Phdr *)PHDR(lmp);
		int		pnum;

		if (FCT(lmp) != &elf_fct)
			continue;

		for (pnum = 0; pnum < (int)PHNUM(lmp); pnum++) {
			if (phdr->p_type == PT_LOAD)
				lnum++;

			phdr = (Phdr *)((unsigned long)phdr + PHSZ(lmp));
		}
	}

	/*
	 * Account for a null entry at the end of the address range array.
	 */
	if (lnum++ == 0)
		return (0);

	/*
	 * Determine whether we have an address range array big enough to
	 * maintain the objects being deleted.
	 */
	if (lnum > anum) {
		if (addr != 0)
			free(addr);
		if ((addr = malloc(lnum * sizeof (Lc_addr_range_t))) == 0)
			return (1);
		anum = lnum;
	}

	/*
	 * Fill the address range with each loadable segments size and address.
	 */
	for (_tobj = tobj, _addr = addr; *_tobj != NULL; _tobj++) {
		Rt_map *	lmp = *_tobj;
		Phdr *		phdr = (Phdr *)PHDR(lmp);
		int		pnum;

		if (FCT(lmp) != &elf_fct)
			continue;

		for (pnum = 0; pnum < (int)PHNUM(lmp); pnum++) {
			if (phdr->p_type == PT_LOAD) {
				caddr_t	paddr = (caddr_t)(phdr->p_vaddr +
					ADDR(lmp));

				_addr->lb = (void *)paddr;
				_addr->ub = (void *)(paddr + phdr->p_memsz);
				_addr++;
			}
			phdr = (Phdr *)((unsigned long)phdr + PHSZ(lmp));
		}
	}
	_addr->lb = _addr->ub = 0;

	/*
	 * If we fail to converse with libc, generate an error message to
	 * satisfy any dlerror() usage.
	 */
	if ((error = ((* ci_jmp_table[CI_ATEXIT])(addr, (lnum - 1)))) != 0)
		eprintf(ERR_FATAL, MSG_INTL(MSG_ARG_ATEXIT), error);
	return (error);
}


/*
 * Remove a link-map.  This removes a link-map from its associated list and
 * free's up the link-map itself.  Note, all components that are freed are local
 * to the link-map, no inter-link-map lists are operated on as these are all
 * broken down by dlclose() while all objects are still mapped.
 *
 * This routine is called from dlclose() to zap individual link-maps after their
 * interdependencies (DONER(), CALLER(), handles, etc.) have been removed, and
 * from the bowels of load_one() in the case of a loaded object never getting
 * associated with a handle.
 */
void
remove_so(Lm_list * lml, Rt_map * lmp)
{
	Listnode *	lnp, *olnp;
	Pnode *		pnp, *opnp;
	char *		oname;

	PRF_MCOUNT(50, remove_so);

	/*
	 * Unlink the link map from the link-map list.  If this is the last
	 * map on the list (possible with dlopen(LM_ID_NEWLM), then nuke the
	 * list itself.
	 */
	if (lml && lmp)
		lm_delete(lml, lmp);

	if (lml && (lml->lm_head == 0)) {
		(void) list_delete(&dynlm_list, lml);
		free(lml);
	}
	if (lmp == 0)
		return;

	DBG_CALL(Dbg_file_delete(NAME(lmp)));

	/*
	 * Unmap the object.
	 */
	LM_UNMAP_SO(lmp)(lmp);

	/*
	 * If this link-map was acting as a filter dlclose() the filtees.
	 */
	for (opnp = 0, pnp = FILTEES(lmp); pnp; opnp = pnp, pnp = pnp->p_next) {
		Rt_map *	flmp;

		if (pnp->p_len) {
			flmp = (Rt_map *)pnp->p_info;
			(void) dlclose_core(HANDLE(flmp), flmp, lmp);
		}
		if (opnp)
			free((void *)opnp);
	}
	if (opnp)
		free((void *)opnp);

	/*
	 * Remove any alias names.
	 */
	olnp = 0;
	for (LIST_TRAVERSE(&ALIAS(lmp), lnp, oname)) {
		free(oname);
		if (olnp)
			free(olnp);
		olnp = lnp;
	}
	if (olnp)
		free(olnp);


	/*
	 * Deallocate any remaining cruft and free the link-map.
	 */
	for (opnp = 0, pnp = RLIST(lmp); pnp; opnp = pnp,
	    pnp = pnp->p_next) {
		if (pnp->p_len)
			free((void *)pnp->p_name);
		if (opnp)
			free((void *)opnp);
	}
	if (opnp)
		free((void *)opnp);

	if (DIRSZ(lmp) && (NAME(lmp) != PATHNAME(lmp)))
		free(PATHNAME(lmp));
	if (NAME(lmp) && (!(FLAGS(lmp) & FLG_RT_ALTER)))
		free(NAME(lmp));
	if (REFNAME(lmp))
		free(REFNAME(lmp));
	if (ELFPRV(lmp))
		free(ELFPRV(lmp));
	if (AUDITORS(lmp))
		audit_desc_cleanup(AUDITORS(lmp));
	if (AUDINFO(lmp))
		audit_info_cleanup(AUDINFO(lmp));

	if (DYNINFO(lmp))
		free(DYNINFO(lmp));

	/*
	 * Clean up reglist if needed
	 */
	if (reglist != (Reglist *)0) {
		Reglist *	cur, * prv, * del;

		cur = prv = reglist;
		while (cur != (Reglist *)0) {
			if (cur->rl_lmp == lmp) {
				del = cur;
				if (cur == reglist) {
					reglist = cur->rl_next;
					cur = prv = reglist;
				} else {
					prv->rl_next = cur->rl_next;
					cur = cur->rl_next;
				}
				free(del);
			} else {
				prv = cur;
				cur = cur->rl_next;
			}
		}
	}

	free(lmp);
}


/*
 * Break down the dependency linked lists between link-maps.  Called for both
 * explicit and implicit dependencies.  This routine must be called when all
 * dependencies are still mapped.
 */
static void
remove_deps(List * list,  Rt_map * lmp)
{
	Listnode *	clnp, * oclnp = 0;
	Rt_map *	dlmp;

	for (LIST_TRAVERSE(list, clnp, dlmp)) {
		/*
		 * Decrement the dependencies count.
		 */
		COUNT(dlmp)--;
		DBG_CALL(Dbg_file_bind_entry(lmp, dlmp));

		/*
		 * Remove this object from the dependencies callers.
		 */
		(void) list_delete(&CALLERS(dlmp), (void *)lmp);

		if (oclnp)
			free(oclnp);
		oclnp = clnp;
	}
	if (oclnp)
		free(oclnp);
}

/*
 * Remove the objects associated with a handle.  This involves tearing down the
 * handle itself, freeing any associated permits, and removing each object.
 * This process consists of four main phases:
 *
 *  o	Determine the objects of the handle that can be deleted, and if an
 *	object can't be deleted whether its permit is still required.
 *
 *  o	Fire the fini's of those objects selected for deletion.
 *
 *  o	Remove all inter-dependency linked lists while the objects link-maps
 *	are still available.
 *
 *  o	Remove all deletable objects link-maps and unmap the objects themselves.
 */
int
remove_hdl(Dl_handle * dlp, Rt_map * dlmp, Rt_map * clmp)
{
	Listnode *	lnp;
	Rt_map *	lmp, * olmp, ** tobj;
	int		delcnt = 0, rescan = 0, error = 0;
	unsigned int	ndx;

	/*
	 * Disassociated the handle from its originating link-map.  If any
	 * objects or permits are removed from this handle then the handle can't
	 * be used again and will become orphaned.
	 */
	HANDLE(dlmp) = 0;

	/*
	 * Traverse the handles dependencies and mark any object that has only
	 * one permit set (which must be this handles permit) as a candidate
	 * for deletion.
	 */
	for (LIST_TRAVERSE(&dlp->dl_depends, lnp, lmp)) {
		if (perm_one(PERMIT(lmp)) == 1) {
			FLAGS1(lmp) |= FL1_RT_RMPERM;
			if (!(MODE(lmp) & RTLD_NODELETE)) {
				FLAGS(lmp) |= FLG_RT_DELETE;
				delcnt++;
			}
			continue;
		}

		/*
		 * If an object has more than one permit then it is a member of
		 * more than one handle.  We can't remove it yet, nor can we
		 * remove any other member of this handle that it is bound to.
		 * Set the rescan flag for another traversal.
		 */
		rescan++;
	}
	if (rescan) {
		/*
		 * Rescan the object list to determine if any objects set as
		 * being deletable must be maintained to satisfy the bindings
		 * of objects contained on more than one handle.
		 */
		for (LIST_TRAVERSE(&dlp->dl_depends, lnp, lmp)) {
			Listnode *	lnp1;
			Rt_map *	lmp1;
			int		rmperm = 1;

			if (perm_one(PERMIT(lmp)) == 1)
				continue;

			/*
			 * This object (lmp) has more than one permit, and thus
			 * is part of more than one handle.  Determine if it is
			 * bound to any other object on this handle.
			 */
			for (LIST_TRAVERSE(&dlp->dl_depends, lnp1, lmp1)) {
				Listnode *	lnp2;
				Rt_map *	lmp2;

				if (lmp == lmp1)
					continue;

				/*
				 * Determine if this object (lmp1) is called by
				 * the object containing more than one permit
				 * (lmp).
				 */
				for (LIST_TRAVERSE(&CALLERS(lmp1),
				    lnp2, lmp2)) {
					Permit * permit;

					if (lmp != lmp2)
						continue;

					/*
					 * Determine if this object (lmp1) can
					 * still be called by lmp without the
					 * present handles permit.
					 *
					 * If this object (lmp1) is global and
					 * the caller has access to global
					 * objects then the permit is no longer
					 * necessary.  Typically this occurs
					 * for dependencies on libc, libdl, etc.
					 */
					if ((MODE(lmp) & RTLD_WORLD) &&
					    (MODE(lmp1) & RTLD_GLOBAL))
						break;

					/*
					 * If this object (lmp1) can be called
					 * without the present handles permit
					 * then again the permit is no longer
					 * necessary.  Typically this occurs
					 * when the same dependencies are
					 * part of multiple link-maps.
					 */
					permit = perm_set(0, PERMIT(lmp));
					(void) perm_unset(permit,
					    dlp->dl_permit);
					if (perm_test(permit, PERMIT(lmp1))) {
						free(permit);
						break;
					}
					free(permit);

					/*
					 * This dependency can't be deleted or
					 * referenced without the permit.
					 */
					if (FLAGS(lmp1) & FLG_RT_DELETE) {
						FLAGS(lmp1) &= ~FLG_RT_DELETE;
						delcnt--;
					}
					FLAGS1(lmp1) &= ~FL1_RT_RMPERM;
					FLAGS1(lmp1) |= FL1_RT_PERMRQ;
					rmperm = 0;
					break;
				}
			}

			/*
			 * If this object wasn't bound to anyone else on the
			 * handle, or we can still reference any dependencies
			 * without the present handles permit, indicate that
			 * this handles permit can be removed.
			 */
			if (rmperm && ((FLAGS1(lmp1) & FL1_RT_PERMRQ) == 0)) {
				FLAGS1(lmp) |= FL1_RT_RMPERM;
			}
		}
	}

	/*
	 * If no objects can be delete then we can leave the handle intact and
	 * reassociate it to its orignal defining object.  Clean up any permit
	 * deletion flags so that the handle remains consistant.
	 */
	if (delcnt == 0) {
		for (LIST_TRAVERSE(&dlp->dl_depends, lnp, lmp))
			FLAGS1(lmp) &= ~(FL1_RT_RMPERM | FL1_RT_PERMRQ);

		HANDLE(dlmp) = dlp;
		return (error);
	}

	/*
	 * If we're being audited tell the audit library that we're about
	 * to go deleting dependencies.
	 */
	if (clmp && ((LIST(clmp)->lm_flags | FLAGS1(clmp)) & FL1_AU_ACTIVITY))
		audit_activity(clmp, LA_ACT_DELETE);

	/*
	 * Sort and fire all fini's of the objects selected for deletion.  Note
	 * that we have to start our search from the link-map head - there's no
	 * telling whether this object has dependencies on objects that were
	 * loaded before it and which can now be deleted.
	 * If the tsort() fails because we can't allocate then that might just
	 * be a symptom of why we're here in the first place - forgo the fini's
	 * but continue to try cleaning up.
	 */
	if (((tobj = tsort(LIST(dlmp)->lm_head, delcnt,
	    (RT_SORT_DELETE | RT_SORT_FWD))) != 0) &&
	    (tobj != (Rt_map **)S_ERROR)) {
		error = purge_exit_handlers(tobj);
		call_fini(LIST(dlmp), tobj);
	}

	/*
	 * Audit the closure of the dlopen'ed object to any local auditors.  Any
	 * global auditors would have been caught by call_fini(), but as the
	 * link-maps CALLERS was removed already we do the local auditors
	 * explicity.
	 */
	if (clmp && (FLAGS1(clmp) & FL1_AU_CLOSE))
		_audit_objclose(&(AUDITORS(clmp)->ad_list), dlmp);

	/*
	 * Remove all inter-dependency lists from those objects selected for
	 * deletion and any permits that can be nuked.
	 */
	DBG_CALL(Dbg_file_bind_title(REF_DELETE));
	for (LIST_TRAVERSE(&dlp->dl_depends, lnp, lmp)) {

		FLAGS1(lmp) &= ~FL1_RT_PERMRQ;

		/*
		 * Delete any permit if we can.
		 */
		if (FLAGS1(lmp) & FL1_RT_RMPERM) {
			(void) list_delete(&DONORS(lmp), dlp);
			(void) perm_unset(PERMIT(lmp), dlp->dl_permit);
			DBG_CALL(Dbg_file_bind_entry(lmp, lmp));
			dlp->dl_permcnt--;
		}

		/*
		 * Traverse the objects explicit and implicit dependency list
		 * decrementing the reference count of each object.  Each
		 * dependency will also have this object removed from its list
		 * of callers.  Remove this link-map from any of our callers.
		 */
		if (FLAGS(lmp) & FLG_RT_DELETE) {
			Listnode *	clnp, * oclnp;
			Rt_map *	clmp;

			remove_deps(&EDEPENDS(lmp), lmp);
			remove_deps(&IDEPENDS(lmp), lmp);

			oclnp = 0;
			for (LIST_TRAVERSE(&CALLERS(lmp), clnp,  clmp)) {
				if (list_delete(&EDEPENDS(clmp), lmp) == 0)
				    (void) list_delete(&IDEPENDS(clmp), lmp);
				COUNT(lmp)--;
				DBG_CALL(Dbg_file_bind_entry(clmp, lmp));

				if (oclnp)
					free(oclnp);
				oclnp = clnp;
			}
			if (oclnp)
				free(oclnp);
		}
	}

	/*
	 * Finally remove each dependency link-map.
	 */
	olmp = 0;
	for (LIST_TRAVERSE(&dlp->dl_depends, lnp, lmp)) {
		if (olmp) {
			(void) list_delete(&dlp->dl_depends, olmp);
			olmp = 0;
		}

		/*
		 * Delete the link-map and unmap the associated object.
		 */
		if (FLAGS(lmp) & FLG_RT_DELETE) {
			free(PERMIT(lmp));
			remove_so(LIST(lmp), lmp);
			olmp = lmp;
			continue;
		}

		/*
		 * We can only delete this objects permit (which was already
		 * done before to group the debugging output), but we need to
		 * remove the handles node.
		 */
		if (FLAGS1(lmp) & FL1_RT_RMPERM) {
			FLAGS1(lmp) &= ~FL1_RT_RMPERM;
			olmp = lmp;
			continue;
		}
	}
	if (olmp)
		(void) list_delete(&dlp->dl_depends, olmp);

	/*
	 * If the handle has no dependents free the permit. Catch any parent
	 * permits at this time (?).
	 */
	if (dlp->dl_depends.head == 0) {
		if (dlp->dl_permcnt != 0) {
			olmp = 0;
			for (LIST_TRAVERSE(&dlp->dl_parents, lnp, lmp)) {
				(void) list_delete(&DONORS(lmp), dlp);
				(void) perm_unset(PERMIT(lmp), dlp->dl_permit);
				dlp->dl_permcnt--;

				if (olmp)
				    (void) list_delete(&dlp->dl_parents, olmp);
				olmp = lmp;
			}
			if (olmp)
				(void) list_delete(&dlp->dl_parents, olmp);
		}

		hdl_free(dlp);
		return (error);
	}

	/*
	 * We're now left with a handle containing one or more objects that are
	 * also in use by another handle.  By maintaining this handle we should
	 * prevent the continued creation of more handles - if these objects
	 * are used again, they will be added to a new handle and given an
	 * additional permit.  But on deletion of that handle we should
	 * determine that the new permit can be removed, and hence the handle
	 * thrown away, as the inter-relationship between these objects is still
	 * maintained by the original handle.
	 *
	 * For now move the handle to the orphan slot - perhaps we'll start
	 * doing some comparison of handles on this slot should they accumulate.
	 */
	/* LINTED */
	ndx = (unsigned int)dlp % HDLISTSZ;

	(void) list_delete(&hdl_list[ndx], dlp);
	(void) list_append(&hdl_list[HDLISTSZ], dlp);

	DBG_CALL(Dbg_file_bind_title(REF_ORPHAN));
	for (LIST_TRAVERSE(&dlp->dl_depends, lnp, lmp))
		DBG_CALL(Dbg_file_bind_entry(lmp, lmp));

	return (error);
}
