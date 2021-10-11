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
#pragma ident	"@(#)util.c	1.103	99/11/03 SMI"

/*
 * Utility routines for run-time linker.  some are duplicated here from libc
 * (with different names) to avoid name space collisions.
 */
#include	"_synonyms.h"

#include	<sys/types.h>
#include	<sys/mman.h>
#include	<sys/stat.h>
#include	<stdarg.h>
#include	<fcntl.h>
#include	<string.h>
#include	<ctype.h>
#include	<dlfcn.h>
#include	<unistd.h>
#include	<signal.h>
#include	<locale.h>
#include	<libintl.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"msg.h"
#include	"debug.h"
#include	"profile.h"

/*
 * All error messages go through eprintf().  During process initialization these
 * messages should be directed to the standard error, however once control has
 * been passed to the applications code these messages should be stored in an
 * internal buffer for use with dlerror().  Note, fatal error conditions that
 * may occur while running the application will still cause a standard error
 * message, see exit() in this file for details.
 * The `application' flag serves to indicate the transition between process
 * initialization and when the applications code is running.
 */

/*
 * Null function used as place where a debugger can set a breakpoint.
 */
void
rtld_db_dlactivity(void)
{
}

/*
 * Null function used as place where debugger can set a pre .init
 * processing breakpoint.
 */
void
rtld_db_preinit(void)
{
}


/*
 * Null function used as place where debugger can set a post .init
 * processing breakpoint.
 */
void
rtld_db_postinit(void)
{
}

/*
 * Execute any .init sections.  These are passed to us in an lmp array which
 * (by default) will have been sorted.
 */
void
call_init(Rt_map ** tobj)
{
	void (*		iptr)();
	Rt_map **	_tobj, ** _nobj;
	int 		bind;
	static List	pending = { 0, 0 };

	PRF_MCOUNT(60, call_init);

	/*
	 * If we're in the middle of an INITFIRST, this must complete before
	 * any new init's are fired.  In this case add the object list to the
	 * pending queue and return.  We'll pick up the queue after any
	 * INITFIRST objects have their init's fired.
	 */
	if ((bind = bind_guard(THR_FLG_INIT)) == 1)
		(void) rw_wrlock(&initlock);

	if (rtld_flags & RT_FL_INITFIRST) {
		(void) list_append(&pending, tobj);
		if (bind) {
			(void) rw_unlock(&initlock);
			(void) bind_clear(THR_FLG_INIT);
			return;
		}
	}

	if (bind) {
		(void) rw_unlock(&initlock);
		(void) bind_clear(THR_FLG_INIT);
	}

	/*
	 * Traverse the tobj array firing each objects init.
	 */
	for (_tobj = _nobj = tobj, _nobj++; *_tobj != NULL; _tobj++, _nobj++) {

		if ((bind = bind_guard(THR_FLG_INIT)) == 1)
			(void) rw_wrlock(&initlock);

		/*
		 * Establish an initfirst state if necessary - no other inits
		 * will be fired (because of addition relocation bindings) when
		 * in this state.
		 */
		if (FLAGS(*_tobj) & FLG_RT_INITFRST)
			rtld_flags |= RT_FL_INITFIRST;

		/*
		 * Set the initdone flag regardless of whether this object
		 * actually contains an .init section.  This flag prevents us
		 * from processing this section again for an .init and also
		 * signifies that a .fini must be called should it exist.
		 * Clear the sort/idx fields for use in later .fini processing.
		 */
		FLAGS(*_tobj) |= FLG_RT_INITDONE;
		SORTVAL(*_tobj) = 0;
		if (bind) {
			(void) rw_unlock(&initlock);
			(void) bind_clear(THR_FLG_INIT);
		}

		if ((iptr = INIT(*_tobj)) != 0) {
			DBG_CALL(Dbg_util_call_init(NAME(*_tobj)));
			(*iptr)();
		}

		if ((bind = bind_guard(THR_FLG_INIT)) == 1)
			(void) rw_wrlock(&initlock);

		/*
		 * If we're firing an INITFIRST object, and other objects must
		 * be fired which are not INITFIRST, make sure we grab any
		 * pending objects that might have been delayed as this
		 * INITFIRST was processed.
		 */
		if ((rtld_flags & RT_FL_INITFIRST) &&
		    ((*_nobj == NULL) || !(FLAGS(*_nobj) & FLG_RT_INITFRST))) {
			Listnode *	lnp;
			Rt_map **	pobj;

			rtld_flags &= ~RT_FL_INITFIRST;

			for (LIST_TRAVERSE(&pending, lnp, pobj)) {
				if (bind) {
					(void) rw_unlock(&initlock);
					(void) bind_clear(THR_FLG_INIT);
				}
				call_init(pobj);
				if ((bind = bind_guard(THR_FLG_INIT)) == 1)
					(void) rw_wrlock(&initlock);
			}
		}

		if (bind) {
			(void) rw_unlock(&initlock);
			(void) bind_clear(THR_FLG_INIT);
		}
	}
	free(tobj);
}

/*
 * Function called by atexit(3C).  Calls all .fini sections related with the
 * mains dependent shared libraries in the order in which the shared libraries
 * have been loaded.  Skip any .fini defined in the main executable, as this
 * will be called by crt0 (main was never marked as initdone).
 */
void
call_fini(Lm_list * lml, Rt_map ** tobj)
{
	Rt_map **	_tobj;
	void (*		fptr)();

	for (_tobj = tobj; *_tobj != NULL; _tobj++) {
		Rt_map *	clmp, * lmp = *_tobj;
		Listnode *	lnp;

		FLAGS(lmp) |= FLG_RT_FINIDONE;

		if ((fptr = FINI(lmp)) != 0) {
			DBG_CALL(Dbg_util_call_fini(NAME(lmp)));
			(*fptr)();
		}

		/*
		 * Audit `close' operations at this point.  The library has
		 * exercised its last instructions (regardless of whether it
		 * will be unmapped or not).
		 *
		 * First call any global auditing.
		 */
		if (lml->lm_flags & LML_AUD_CLOSE)
			_audit_objclose(&(auditors->ad_list), lmp);

		/*
		 * Finally determine whether this object has local auditing
		 * requirements by inspecting itself and then its dependencies.
		 */
		if ((lml->lm_flags & LML_FLG_LOCAUDIT) == 0)
			continue;

		if (FLAGS1(lmp) & FL1_AU_CLOSE)
			_audit_objclose(&(AUDITORS(lmp)->ad_list), lmp);

		for (LIST_TRAVERSE(&CALLERS(lmp), lnp, clmp)) {
			if (FLAGS1(clmp) & FL1_AU_CLOSE) {
			    _audit_objclose(&(AUDITORS(clmp)->ad_list), lmp);
			    break;
			}
		}
	}
	free(tobj);
}

void
atexit_fini()
{
	Rt_map **	tobj;
	Lm_list *	lml;
	Listnode *	lnp;
	int		_dbg_mask = dbg_mask;

	rtld_flags |= RT_FL_ATEXIT;

	for (LIST_TRAVERSE(&dynlm_list, lnp, lml)) {
		Rt_map *	lmp = (Rt_map *)lml->lm_head;
		Lmid_t		lmid = get_linkmap_id(lml);

		if (lmid == LM_ID_LDSO)
			dbg_mask = 0;
		else
			dbg_mask = _dbg_mask;

		/*
		 * Reverse topological sort the dependency for .fini execution.
		 */
		if (((tobj = tsort(lmp, lml->lm_obj, RT_SORT_FWD)) != 0) &&
		    (tobj != (Rt_map **)S_ERROR))
			call_fini(lml, tobj);

		/*
		 * Add an explicit close to main and ld.so.1 (as their fini
		 * doesn't get processed this auditing will not get caught in
		 * call_fini()).  This is the reverse of the explicit calls to
		 * audit_objopen() made in setup().
		 */
		if ((lmid == LM_ID_BASE) &&
		    ((lml->lm_flags | FLAGS1(lmp)) & FL1_MSK_AUDIT)) {
			audit_objclose(lmp, (Rt_map *)lml_rtld.lm_head);
			audit_objclose(lmp, lmp);
		}
	}
}


/*
 * Append an item to the specified list, and return a pointer to the list
 * node created.
 */
Listnode *
list_append(List * lst, const void * item)
{
	Listnode *	_lnp;

	PRF_MCOUNT(62, list_append);

	if ((_lnp = malloc(sizeof (Listnode))) == 0)
		return (0);

	_lnp->data = (void *)item;
	_lnp->next = NULL;

	if (lst->head == NULL)
		lst->tail = lst->head = _lnp;
	else {
		lst->tail->next = _lnp;
		lst->tail = lst->tail->next;
	}
	return (_lnp);
}

/*
 * Delete a 'listnode' from a list.  If the list not does not
 * reside on this list return (-1) else return (0) on success.
 */
int
list_delete(List * lst, void * item)
{
	Listnode *	clnp, * plnp;

	for (plnp = NULL, clnp = lst->head; clnp; clnp = clnp->next) {
		if (item == clnp->data)
			break;
		plnp = clnp;
	}

	if (clnp == 0)
		return (0);

	if (lst->head == clnp)
		lst->head = clnp->next;
	if (lst->tail == clnp)
		lst->tail = plnp;

	if (plnp)
		plnp->next = clnp->next;

	free(clnp);

	return (1);
}

/*
 * Append/delete an item to the specified link map list - these two routines are
 * maintained together to insure they remain synchronized (one undoes what the
 * other creates).
 */
int
lm_append(Lm_list * lml, Rt_map * lmp)
{
	PRF_MCOUNT(63, lm_append);

	(lml->lm_obj)++;
	(lml->lm_init)++;

	/*
	 * If this is the first link-map for the given list initialize the list.
	 */
	if (lml->lm_head == NULL) {
		lml->lm_head = lmp;
		lml->lm_tail = lmp;

		return (1);
	}

	/*
	 * If this is an interposer then append the link-map following any
	 * other interposers (these may be preloaded objects).
	 *
	 * NOTE: We do not interpose on the head of a list.  This model evolved
	 * because dynamic executables have already been fully relocated within
	 * themselves and thus can't be interposed on.  Nowadays its possible to
	 * have shared objects at the head of a list, which conceptually means
	 * they could be interposed on.  But, shared objects can be created via
	 * dldump() and may only be partially relocated (just relatives), in
	 * which case they are interposable, but are marked as fixed (ET_EXEC).
	 * Thus we really don't have a clear method of deciding when the head of
	 * a link-map is interposable.  So, to be consistent, for now we will
	 * only add interposers after the link-maps head object.
	 */
	if (FLAGS(lmp) & FLG_RT_INTRPOSE) {
		Rt_map *	_lmp;

		for (_lmp = (Rt_map *)NEXT(lml->lm_head); _lmp;
		    _lmp = (Rt_map *)NEXT(_lmp)) {
			if (FLAGS(_lmp) & FLG_RT_INTRPOSE)
				continue;

			if (lml->lm_head == _lmp) {
				lml->lm_head = lmp;
				PREV(lmp) = NULL;
			} else {
				NEXT((Rt_map *)PREV(_lmp)) = (Link_map *)lmp;
				PREV(lmp) = PREV(_lmp);
			}
			NEXT(lmp) = (Link_map *)_lmp;
			PREV(_lmp) = (Link_map *)lmp;

			lml->lm_flags |= LML_FLG_INTRPOSE;

			return (1);
		}
	}

	/*
	 * Fall through to appending the new link map to the tail of the list.
	 */
	NEXT(lml->lm_tail) = (Link_map *)lmp;
	PREV(lmp) = (Link_map *)lml->lm_tail;

	lml->lm_tail = lmp;

	return (1);
}

void
lm_delete(Lm_list * lml, Rt_map * lmp)
{
	/*
	 * Alert the debuggers that we are about to mess with the link-map.
	 */
	if ((rtld_flags & RT_FL_DBNOTIF) == 0) {
		rtld_flags |= (RT_FL_DBNOTIF | RT_FL_CLEANUP);
		rd_event(RD_DLACTIVITY, RT_DELETE, rtld_db_dlactivity());
	}

	if (lml->lm_head == lmp)
		lml->lm_head = (Rt_map *)NEXT(lmp);
	else
		NEXT((Rt_map *)PREV(lmp)) = (void *)NEXT(lmp);

	if (lml->lm_tail == lmp)
		lml->lm_tail = (Rt_map *)PREV(lmp);
	else
		PREV((Rt_map *)NEXT(lmp)) = PREV(lmp);

	(lml->lm_obj)--;
}

/*
 * Internal getenv routine.  Only strings starting with `LD_' are reserved for
 * our use.  By convention, all strings should be of the form `LD_XXXXX=', if
 * the string is followed by a non-null value the appropriate functionality is
 * enabled.
 */
#define	LOC_LANG	1
#define	LOC_MESG	2
#define	LOC_ALL		3

int
readenv(const char ** envp, int aout)
{
	const char *	s1, * s2, * flags_str = 0;
	int		flags_lm = 0;
	size_t		loc = 0;
	int		ldpth_override = FALSE;
	int		ldprof = FALSE;
	int		audit_64 = FALSE;

	if (envp == (const char **)0)
		return (-1);
	while (*envp != (const char *)0) {
		s1 = *envp++;
		if (*s1++ != 'L')
			continue;

		/*
		 * See if we have any locale environment settings.  The environ
		 * variables have a precedence, LC_ALL is higher than
		 * LC_MESSAGES which is higher than LANG.
		 */
		s2 = s1;
		if ((*s2++ == 'C') && (*s2++ == '_') && (*s2 != '\0')) {
			if (strncmp(s2, MSG_ORIG(MSG_LC_ALL),
			    MSG_LC_ALL_SIZE) == 0) {
				s2 += MSG_LC_ALL_SIZE;
				if ((*s2 != '\0') && (loc < LOC_ALL)) {
					locale = s2;
					loc = LOC_ALL;
				}
			} else if (strncmp(s2, MSG_ORIG(MSG_LC_MESSAGES),
			    MSG_LC_MESSAGES_SIZE) == 0) {
				s2 += MSG_LC_MESSAGES_SIZE;
				if ((*s2 != '\0') && (loc < LOC_MESG)) {
					locale = s2;
					loc = LOC_MESG;
				}
			}
			continue;
		}
		s2 = s1;
		if ((*s2++ == 'A') && (*s2++ == 'N') && (*s2++ == 'G') &&
		    (*s2++ == '=') && (*s2 != '\0') && (loc < LOC_LANG)) {
			locale = s2;
			loc = LOC_LANG;
			continue;
		}

		/*
		 * Pick off any LD_ environment variables.
		 */
		if ((*s1++ != 'D') || (*s1++ != '_') || (*s1 == '\0'))
			continue;

#ifdef _ELF64
		if (strncmp(s1, MSG_ORIG(MSG_LD_LIBPATH_64),
		    MSG_LD_LIBPATH_64_SIZE) == 0) {
			s1 += MSG_LD_LIBPATH_64_SIZE;
			if (*s1 != '\0')
				envdirs = s1;
			ldpth_override = TRUE;
		} else
#endif
		if (strncmp(s1, MSG_ORIG(MSG_LD_LIBPATH),
		    MSG_LD_LIBPATH_SIZE) == 0) {
			/*
			 * V9 ABI:
			 * LD_LIBRARY_PATH_64 overrides LD_LIBRARY_PATH.
			 */
			if (ldpth_override == FALSE) {
				s1 += MSG_LD_LIBPATH_SIZE;
				if (*s1 != '\0')
					envdirs = s1;
			}
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_PRELOAD),
		    MSG_LD_PRELOAD_SIZE) == 0) {
			s1 += MSG_LD_PRELOAD_SIZE;
			while (isspace(*s1))
				s1++;
			if (*s1 != '\0')
				preload_objs = s1;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_TRACEOBJS),
		    MSG_LD_TRACEOBJS_SIZE) == 0) {
			s1 += MSG_LD_TRACEOBJS_SIZE;
			if ((*s1 == '=') && (*++s1 != '\0')) {
				flags_lm |= LML_TRC_ENABLE;
				if (*s1 == '2')
					flags_lm |= LML_TRC_SKIP;
			} else if (((strncmp(s1, MSG_ORIG(MSG_LD_TRACE_E),
			    MSG_LD_TRACE_E_SIZE) == 0) && !aout) ||
			    ((strncmp(s1, MSG_ORIG(MSG_LD_TRACE_A),
			    MSG_LD_TRACE_A_SIZE) == 0) && aout)) {
				s1 += MSG_LD_TRACE_E_SIZE;
				if (*s1 != '\0') {
					flags_lm |= LML_TRC_ENABLE;
					if (*s1 == '2')
						flags_lm |= LML_TRC_SKIP;
				}
			}
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_TRACEPTHS),
		    MSG_LD_TRACEPTHS_SIZE) == 0) {
			s1 += MSG_LD_TRACEPTHS_SIZE;
			if (*s1 != '\0')
				flags_lm |= LML_TRC_SEARCH;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_VERBOSE),
		    MSG_LD_VERBOSE_SIZE) == 0) {
			s1 += MSG_LD_VERBOSE_SIZE;
			if (*s1 != '\0')
				flags_lm |= LML_TRC_VERBOSE;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_BREADTH),
		    MSG_LD_BREADTH_SIZE) == 0) {
			s1 += MSG_LD_BREADTH_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_BREADTH;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_INIT),
		    MSG_LD_INIT_SIZE) == 0) {
			s1 += MSG_LD_INIT_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_INIT;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NODIRECT),
		    MSG_LD_NODIRECT_SIZE) == 0) {
			s1 += MSG_LD_NODIRECT_SIZE;
			if (*s1 != '\0')
				flags_lm |= LML_FLG_NODIRECT;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_WARN),
		    MSG_LD_WARN_SIZE) == 0) {
			s1 += MSG_LD_WARN_SIZE;
			if (*s1 != '\0')
				flags_lm |= LML_TRC_WARN;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_BINDINGS),
		    MSG_LD_BINDINGS_SIZE) == 0) {
			s1 += MSG_LD_BINDINGS_SIZE;
			if (*s1 != '\0') {
				/*
				 * NOTE, this variable is simply for backward
				 * compatibility.  If this and LD_DEBUG are both
				 * specified, only one of the strings is going
				 * to get processed.
				 */
				dbg_str = MSG_ORIG(MSG_TKN_BINDINGS);
			}
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_BIND_NOW),
		    MSG_LD_BIND_NOW_SIZE) == 0) {
			s1 += MSG_LD_BIND_NOW_SIZE;
			if (*s1 != '\0')
				bind_mode = RTLD_NOW;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_BIND_NOT),
		    MSG_LD_BIND_NOT_SIZE) == 0) {
			s1 += MSG_LD_BIND_NOT_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NOBIND;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NOAUXFLTR),
		    MSG_LD_NOAUXFLTR_SIZE) == 0) {
			s1 += MSG_LD_NOAUXFLTR_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NOAUXFLTR;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_LOADFLTR),
		    MSG_LD_LOADFLTR_SIZE) == 0) {
			s1 += MSG_LD_LOADFLTR_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_LOADFLTR;
			if (*s1 == '2')
				rtld_flags |= RT_FL_WARNFLTR;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NOVERSION),
		    MSG_LD_NOVERSION_SIZE) == 0) {
			s1 += MSG_LD_NOVERSION_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NOVERSION;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_CONFIG),
		    MSG_LD_CONFIG_SIZE) == 0) {
			if (!(rtld_flags & RT_FL_SECURE)) {
				s1 += MSG_LD_CONFIG_SIZE;
				if (*s1 != '\0')
					config->c_name = s1;
			}
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NOCONFIG),
		    MSG_LD_NOCONFIG_SIZE) == 0) {
			s1 += MSG_LD_NOCONFIG_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NOCFG;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NODIRCONFIG),
		    MSG_LD_NODIRCONFIG_SIZE) == 0) {
			s1 += MSG_LD_NODIRCONFIG_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NODIRCFG;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_ORIGIN),
		    MSG_LD_ORIGIN_SIZE) == 0) {
			s1 += MSG_LD_ORIGIN_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_ORIGIN;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NOOBJALTER),
		    MSG_LD_NOOBJALTER_SIZE) == 0) {
			s1 += MSG_LD_NOOBJALTER_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NOOBJALT;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_AUDIT),
		    MSG_LD_AUDIT_SIZE) == 0 && !audit_64) {
			if (ldprof) {
				eprintf(ERR_WARNING,
				    MSG_INTL(MSG_AUD_PROFAUDINC));
				continue;
			}
			s1 += MSG_LD_AUDIT_SIZE;
			while (isspace(*s1))
				s1++;
			if (*s1 != '\0')
				audit_objs = s1;
#ifdef _LP64
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_AUDIT_64),
		    MSG_LD_AUDIT_64_SIZE) == 0) {
			if (ldprof) {
				eprintf(ERR_WARNING,
				    MSG_INTL(MSG_AUD_PROFAUDINC));
				continue;
			}
			audit_64 = TRUE;
			s1 += MSG_LD_AUDIT_64_SIZE;
			while (isspace(*s1))
				s1++;
			if (*s1 != '\0')
				audit_objs = s1;
#endif
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_FLAGS),
		    MSG_LD_FLAGS_SIZE) == 0) {
			s1 += MSG_LD_FLAGS_SIZE;
			flags_str = s1;
			continue;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_AUDIT_ARGS),
		    MSG_LD_AUDIT_ARGS_SIZE) == 0) {
			s1 += MSG_LD_AUDIT_ARGS_SIZE;
			audit_argcnt = atoi(s1);
			/*
			 * On sparc the stack must be 8 byte aligned so we might
			 * as well copy two arguments at a time.  Enforce that
			 * here.
			 */
			audit_argcnt += audit_argcnt % 2;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_DEBUG),
		    MSG_LD_DEBUG_SIZE) == 0) {
			s1 += MSG_LD_DEBUG_SIZE;
			if ((*s1 == '=') && (*++s1 != '\0'))
				dbg_str = s1;
			else if (strncmp(s1, MSG_ORIG(MSG_LD_OUTPUT),
			    MSG_LD_OUTPUT_SIZE) == 0) {
				s1 += MSG_LD_OUTPUT_SIZE;
				if (*s1 != '\0')
					dbg_file = s1;
			}
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_PROFILE),
		    MSG_LD_PROFILE_SIZE) == 0) {
			s1 += MSG_LD_PROFILE_SIZE;
			if ((*s1 == '=') && (*++s1 != '\0')) {
				if (audit_objs) {
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_AUD_PROFAUDINC));
					continue;
				}
				if (strcmp(s1, MSG_ORIG(MSG_FIL_RTLD)) == 0) {
#ifdef	PRF_RTLD
					profile_name = MSG_ORIG(MSG_FIL_RTLD);
#endif
					continue;
				}
#if	defined(_ELF64) && defined(__sparcv9)
				audit_objs = MSG_ORIG(MSG_FIL_SP64_LDPROF);
#elif	defined(_ELF64) && defined(__ia64)
				audit_objs = MSG_ORIG(MSG_FIL_IA64_LDPROF);
#else
				audit_objs = MSG_ORIG(MSG_FIL_LDPROF);
#endif
				ldprof = TRUE;
			}
		}
	}

	s1 = flags_str;
	/* LINTED */
	while (flags_str) {
		size_t len;

		if (s2 = strchr(s1, ','))
			len = s2 - s1;
		else
			len = strlen(s1);

		if ((len == MSG_LDFLG_NOLAZY_SIZE) &&
		    (strncmp(s1, MSG_ORIG(MSG_LDFLG_NOLAZY),
		    MSG_LDFLG_NOLAZY_SIZE) == 0)) {
			flags_lm |= LML_FLG_NOLAZYLD;

		} else if ((len == MSG_LDFLG_NODIRECT_SIZE) &&
		    (strncmp(s1, MSG_ORIG(MSG_LDFLG_NODIRECT),
		    MSG_LDFLG_NODIRECT_SIZE) == 0)) {
			flags_lm |= LML_FLG_NODIRECT;

		} else if ((len == MSG_LDFLG_NOAUDIT_SIZE) &&
		    (strncmp(s1, MSG_ORIG(MSG_LDFLG_NOAUDIT),
		    MSG_LDFLG_NOAUDIT_SIZE) == 0)) {
			rtld_flags |= RT_FL_NOAUDIT;

		} else if ((len == MSG_LDFLG_CONFGEN_SIZE) &&
		    (strncmp(s1, MSG_ORIG(MSG_LDFLG_CONFGEN),
		    MSG_LDFLG_CONFGEN_SIZE) == 0)) {
			rtld_flags |= RT_FL_CONFGEN;
			flags_lm |= LML_FLG_IGNERROR;

		} else if ((len == MSG_LDFLG_LOADAVAIL_SIZE) &&
		    (strncmp(s1, MSG_ORIG(MSG_LDFLG_LOADAVAIL),
		    MSG_LDFLG_LOADAVAIL_SIZE) == 0)) {
			flags_lm |= LML_FLG_LOADAVAIL;

		} else if ((len == MSG_LDFLG_LOADFLTR_SIZE) &&
		    (strncmp(s1, MSG_ORIG(MSG_LDFLG_LOADFLTR),
		    MSG_LDFLG_LOADFLTR_SIZE) == 0)) {
			rtld_flags |= RT_FL_LOADFLTR;
		}

		s1 += len;
		if (*s1 == '\0')
			break;
		s1++;
	}

	/*
	 * LD_WARN, LD_TRACE_SEARCH_PATHS and LD_VERBOSE are meaningful only if
	 * tracing.  Don't allow environment controlled auditing when tracing.
	 */
	if ((flags_lm & LML_TRC_ENABLE) || (rtld_flags & RT_FL_NOAUDIT)) {
		audit_objs = 0;
#ifdef	PRF_RTLD
		profile_name = 0;
#endif
	} else
		flags_lm &= ~(LML_TRC_SEARCH | LML_TRC_WARN | LML_TRC_VERBOSE);


	/*
	 * If we have a locale setting make sure its worth processing further.
	 */
	if (locale) {
		if (((*locale == 'C') && (*(locale + 1) == '\0')) ||
		    (strcmp(locale, MSG_ORIG(MSG_TKN_POSIX)) == 0))
			locale = 0;
	}
	return (flags_lm);
}

int
dowrite(Prfbuf * prf)
{
	/*
	 * We do not have a valid file descriptor, so we are unable
	 * to flush the buffer.
	 */
	if (prf->pr_fd == -1)
		return (0);
	(void) write(prf->pr_fd, prf->pr_buf, prf->pr_cur - prf->pr_buf);
	prf->pr_cur = prf->pr_buf;
	return (1);
}

/*
 * Simplified printing.  The following conversion specifications are supported:
 *
 *	% [#] [-] [min field width] [. precision] s|d|x|c
 *
 *
 * dorprf takes the output buffer in the form of Prfbuf which permits
 * the verification of the output buffer size and the concatination
 * of data to an already existing output buffer.  The Prfbuf
 * structure contains the following:
 *
 *	p_buf	- pointer to the beginning of the output buffer.
 *	p_cur	- pointer to the next available byte in the output
 *		  buffer.  By setting p_cur ahead of p_buf you can
 *		  append to an already existing buffer.
 *	p_len	- the size of the output buffer.  By setting p_len
 *		  to '0' you disable protection from overflows in the
 *		  output buffer.
 *	p_fd	- a pointer to the file-descriptor the buffer will
 *		  eventually be output to.  If p_fd is set to '-1' then
 *		  it's assumed there is no output buffer and doprf() will
 *		  return with an error if the output buffer is overflowed.
 *		  If p_fd is > '-1' then when the output buffer is filled
 *		  it will be flushed to 'p_fd' and then the available
 *		  for additional data.
 */
#define	FLG_UT_MINUS	0x0001	/* - */
#define	FLG_UT_SHARP	0x0002	/* # */
#define	FLG_UT_DOTSEEN	0x0008	/* dot appeared in format spec */

/*
 * This macro is for use from within doprf only.  it's to be used
 * for checking the output buffer size and placing characters into
 * the buffer.
 */
#define	PUTC(c) \
	{ \
		register char tmpc; \
		\
		tmpc = (c); \
		if ((bufsiz) && ((bp + 1) >= bufend)) { \
			prf->pr_cur = bp; \
			if (dowrite(prf) == 0) \
				return (0); \
			bp = prf->pr_cur; \
		} \
		*bp++ = tmpc; \
	}

size_t
doprf(const char * format, va_list args, Prfbuf * prf)
{
	char	c;
	char *	bp = prf->pr_cur;
	char *	bufend = prf->pr_buf + prf->pr_len;
	size_t	bufsiz = prf->pr_len;

	PRF_MCOUNT(65, doprf);

	while ((c = *format++) != '\0') {
		if (c != '%') {
			PUTC(c);
		} else {
			int	base = 0, flag = 0, width = 0, prec = 0;
			size_t	_i;
			int	_c, _n;
			char *	_s;
			int	ls = 0;
again:
			c = *format++;
			switch (c) {
			case '-':
				flag |= FLG_UT_MINUS;
				goto again;
			case '#':
				flag |= FLG_UT_SHARP;
				goto again;
			case '.':
				flag |= FLG_UT_DOTSEEN;
				goto again;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (flag & FLG_UT_DOTSEEN)
					prec = (prec * 10) + c - '0';
				else
					width = (width * 10) + c - '0';
				goto again;
			case 'x':
			case 'X':
				base = 16;
				break;
			case 'd':
			case 'D':
			case 'u':
				base = 10;
				flag &= ~FLG_UT_SHARP;
				break;
			case 'l':
				base = 10;
				ls++; /* number of l's (long or long long) */
				if ((*format == 'l') ||
				    (*format == 'd') || (*format == 'D') ||
				    (*format == 'x') || (*format == 'X') ||
				    (*format == 'o') || (*format == 'O'))
					goto again;
				break;
			case 'o':
			case 'O':
				base = 8;
				break;
			case 'c':
				_c = va_arg(args, int);

				/* LINTED */
				for (_i = 24; _i > 0; _i -= 8) {
					if ((c = ((_c >> _i) & 0x7f)) != 0) {
						PUTC(c);
					}
				}
				if ((c = ((_c >> _i) & 0x7f)) != 0) {
					PUTC(c);
				}
				break;
			case 's':
				_s = va_arg(args, char *);
				_i = strlen(_s);
				/* LINTED */
				_n = (int)(width - _i);
				if (!prec)
					/* LINTED */
					prec = (int)_i;

				if (width && !(flag & FLG_UT_MINUS)) {
					while (_n-- > 0)
						PUTC(' ');
				}
				while (((c = *_s++) != 0) && prec--) {
					PUTC(c);
				}
				if (width && (flag & FLG_UT_MINUS)) {
					while (_n-- > 0)
						PUTC(' ');
				}
				break;
			case '%':
				PUTC('%');
				break;
			default:
				break;
			}

			/*
			 * Numeric processing
			 */
			if (base) {
				char		local[20];
				const char *	string =
						    MSG_ORIG(MSG_STR_HEXNUM);
				size_t		ssize = 0, psize = 0;
				const char *	prefix =
						    MSG_ORIG(MSG_STR_EMPTY);
				unsigned long long	num;

				switch (ls) {
				case 0:	/* int */
					num = (unsigned long long)
					    va_arg(args, unsigned int);
					break;
				case 1:	/* long */
					num = (unsigned long long)
					    va_arg(args, unsigned long);
					break;
				case 2:	/* long long */
					num = va_arg(args, unsigned long long);
					break;
				}

				if (flag & FLG_UT_SHARP) {
					if (base == 16) {
						prefix = MSG_ORIG(MSG_STR_HEX);
						psize = 2;
					} else {
						prefix = MSG_ORIG(MSG_STR_ZERO);
						psize = 1;
					}
				}
				if ((base == 10) && (long)num < 0) {
					prefix = MSG_ORIG(MSG_STR_NEGATE);
					psize = MSG_STR_NEGATE_SIZE;
					num = (unsigned long long)
					    (-(long long)num);
				}

				/*
				 * Convert the numeric value into a local
				 * string (stored in reverse order).
				 */
				_s = local;
				do {
					*_s++ = string[num % base];
					num /= base;
					ssize++;
				} while (num);

				/*
				 * Provide any precision or width padding.
				 */
				if (prec) {
					/* LINTED */
					_n = (int)(prec - ssize);
					while (_n-- > 0) {
						*_s++ = '0';
						ssize++;
					}
				}
				if (width && !(flag & FLG_UT_MINUS)) {
					/* LINTED */
					_n = (int)(width - ssize - psize);
					while (_n-- > 0) {
						PUTC(' ');
					}
				}

				/*
				 * Print any prefix and the numeric string
				 */
				while (*prefix)
					PUTC(*prefix++);
				do {
					PUTC(*--_s);
				} while (_s > local);

				/*
				 * Provide any width padding.
				 */
				if (width && (flag & FLG_UT_MINUS)) {
					/* LINTED */
					_n = (int)(width - ssize - psize);
					while (_n-- > 0)
						PUTC(' ');
				}
			}
		}
	}
	PUTC('\0');
	prf->pr_cur = bp;
	return (1);
}

/* VARARGS2 */
int
sprintf(char * buf, const char * format, ...)
{
	va_list	args;
	int	len;
	Prfbuf	prf;

	PRF_MCOUNT(67, sprintf);

	va_start(args, format);
	prf.pr_buf = prf.pr_cur = buf;
	prf.pr_len = 0;
	prf.pr_fd = -1;
	/* LINTED */
	len = (int)doprf(format, args, &prf);
	va_end(args);

	return (len);
}

/* VARARGS3 */
int
snprintf(char * buf, size_t n, const char * format, ...)
{
	va_list	args;
	size_t	len;
	Prfbuf	prf;

	va_start(args, format);
	prf.pr_buf = prf.pr_cur = buf;
	prf.pr_len = n;
	prf.pr_fd = -1;
	if (doprf(format, args, &prf) == 0)
		len = 0;
	else
		len = (uintptr_t)prf.pr_cur - (uintptr_t)prf.pr_len;
	va_end(args);

	/* LINTED */
	return ((int)len);
}

/* VARARGS2 */
int
bufprint(Prfbuf * prf, const char * format, ...)
{
	va_list	args;
	int	rc;

	va_start(args, format);
	/* LINTED */
	rc = (int)doprf(format, args, prf);
	va_end(args);
	return (rc);
}

/*PRINTFLIKE1*/
int
printf(const char * format, ...)
{
	va_list	args;
	char 	buffer[ERRSIZE];
	Prfbuf	prf;

	PRF_MCOUNT(68, printf);

	va_start(args, format);
	prf.pr_buf = prf.pr_cur = buffer;
	prf.pr_len = ERRSIZE;
	prf.pr_fd = 1;
	(void) doprf(format, args, &prf);
	va_end(args);
	/*
	 * Trim trailing '\0' form buffer
	 */
	prf.pr_cur--;
	return (dowrite(&prf));
}

/*PRINTFLIKE2*/
void
eprintf(Error error, const char * format, ...)
{
	va_list			args;
	static char *		buffer = 0;
	int			bind;
	static const char *	strings[ERR_NUM] = {MSG_ORIG(MSG_STR_EMPTY)};
	static int		lock = 0;
	Prfbuf			prf;

	PRF_MCOUNT(66, eprintf);

	/*
	 * Because eprintf() uses a global buffer to store it's work a write
	 * lock is needed around the whole routine.
	 *
	 * Note: no lock is placed around printf() because it uses a buffer off
	 * of the stack and does it's write in a single atomic write().
	 */
	if ((ti_version > 0) &&
	    ((bind = bind_guard(THR_FLG_PRINT) == 1)))
		(void) rw_wrlock(&printlock);

	if (lock)
		return;

	/*
	 * Note: this lock is here to prevent the same thread from
	 *	 recursivly entering itself during a eprintf.
	 *	 ie: during eprintf malloc() fails and we try and call
	 *	 eprintf...and then malloc() fails....
	 */
	lock = 1;

	/*
	 * Allocate the error string buffer, if one doesn't already exist.
	 * Reassign lasterr, incase it was `cleared' by a dlerror() call.
	 */
	if (!buffer) {
		if ((buffer = malloc(ERRSIZE + 1)) == 0) {
			lasterr = (char *)MSG_ORIG(MSG_EMG_NOSPACE);
			lock = 0;
			if ((ti_version > 0) && bind) {
				(void) rw_unlock(&printlock);
				(void) bind_clear(THR_FLG_PRINT);
			}
			if (!(rtld_flags & RT_FL_APPLIC)) {
				(void) write(2, MSG_ORIG(MSG_EMG_NOSPACE),
					MSG_EMG_NOSPACE_SIZE);
				(void) write(2, MSG_ORIG(MSG_STR_NL),
					MSG_STR_NL_SIZE);
			}
			return;
		}
	}
	lasterr = buffer;

	/*
	 * If we have completed startup initialization all error messages
	 * must be saved.  These are reported through dlerror().  If we're
	 * still in the initialization stage output the error directly and
	 * add a newline.
	 */
	va_start(args, format);

	prf.pr_buf = prf.pr_cur = buffer;
	prf.pr_len = ERRSIZE;

	if (!(rtld_flags & RT_FL_APPLIC))
		prf.pr_fd = 2;
	else
		prf.pr_fd = -1;

	if (error > ERR_NONE) {
		if (error == ERR_WARNING) {
			if (strings[ERR_WARNING] == 0)
			    strings[ERR_WARNING] = MSG_INTL(MSG_ERR_WARNING);
		} else if (error == ERR_FATAL) {
			if (strings[ERR_FATAL] == 0)
			    strings[ERR_FATAL] = MSG_INTL(MSG_ERR_FATAL);
		} else if (error == ERR_ELF) {
			if (strings[ERR_ELF] == 0)
			    strings[ERR_ELF] = MSG_INTL(MSG_ERR_ELF);
		}
		if (bufprint(&prf, MSG_ORIG(MSG_STR_EMSGFOR1),
		    rt_name, pr_name, strings[error]) == 0) {
			va_end(args);
			lock = 0;
			if ((ti_version > 0) && bind) {
				(void) rw_unlock(&printlock);
				(void) bind_clear(THR_FLG_PRINT);
			}
			eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
				MSG_ORIG(MSG_STR_EMSGFOR1));
			eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
				format);
			return;
		}
		/*
		 * remove the terminating '\0'
		 */
		prf.pr_cur--;
	}

	if (doprf(format, args, &prf) == 0) {
		va_end(args);
		lock = 0;
		if ((ti_version > 0) && bind) {
			(void) rw_unlock(&printlock);
			(void) bind_clear(THR_FLG_PRINT);
		}
		eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
			format);
		return;
	}

	/*
	 * If this is an ELF error it will have been generated by a support
	 * object that has a dependency on libelf.  ld.so.1 doesn't generate any
	 * ELF error messages as it doesn't interact with libelf.  Determine the
	 * ELF error string.
	 */
	if (error == ERR_ELF) {
		static int (*		elfeno)() = 0;
		static const char * (*	elfemg)();
		const char *		emsg;
		Rt_map *		lmp = lml_rtld.lm_head;

		if (NEXT(lmp) && (elfeno == 0)) {
			if (((elfemg = (const char *(*)())dlsym_core(RTLD_NEXT,
			    MSG_ORIG(MSG_SYM_ELFERRMSG), lmp)) == 0) ||
			    ((elfeno = (int (*)())dlsym_core(RTLD_NEXT,
			    MSG_ORIG(MSG_SYM_ELFERRNO), lmp)) == 0))
				elfeno = 0;
		}
		/*
		 * Lookup the message; equivalent to elf_errmsg(elf_errno()).
		 */
		if (elfeno && ((emsg = (* elfemg)((* elfeno)())) != 0)) {
			prf.pr_cur--;
			if (bufprint(&prf, MSG_ORIG(MSG_STR_EMSGFOR2),
			    emsg) == 0) {
				va_end(args);
				lock = 0;
				if ((ti_version > 0) && bind) {
					(void) rw_unlock(&printlock);
					(void) bind_clear(THR_FLG_PRINT);
				}
				eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
					MSG_ORIG(MSG_STR_EMSGFOR2));
				eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
					format);
				return;
			}
		}
	}

	if (!(rtld_flags & RT_FL_APPLIC)) {
		*(prf.pr_cur - 1) = '\n';
		(void) dowrite(&prf);
	} else {
		DBG_CALL(Dbg_util_str(buffer));
	}
	va_end(args);
	lock = 0;
	if ((ti_version > 0) && bind) {
		(void) rw_unlock(&printlock);
		(void) bind_clear(THR_FLG_PRINT);
	}
}

/*
 * Exit.  If we arrive here with a non zero status it's because of a fatal
 * error condition (most commonly a relocation error).  If the application has
 * already had control, then the actual fatal error message will have been
 * recorded in the dlerror() message buffer.  Print the message before really
 * exiting.
 */
void
exit(int status)
{
	if (status) {
		if (rtld_flags & RT_FL_APPLIC) {
			(void) write(2, lasterr, strlen(lasterr));
			(void) write(2, MSG_ORIG(MSG_STR_NL), MSG_STR_NL_SIZE);
		}
		(void) kill(getpid(), SIGKILL);
	}
	_exit(status);
}

/*
 * Routines to co-ordinate the opening of /dev/zero and /proc.
 */
static int	dz_fd = FD_UNAVAIL;

void
dz_init(int fd)
{
	PRF_MCOUNT(69, dz_init);

	dz_fd = fd;
	rtld_flags |= RT_FL_CLEANUP;
}

/*
 * Map anonymous memory from /dev/zero, or via MAP_ANON.
 *
 * (MAP_ANON only appears on Solaris 8, so we need fall-back
 * behaviour for older systems.)
 */
caddr_t
dz_map(caddr_t addr, size_t len, int prot, int flags)
{
	caddr_t	va;
	int	err;

#if defined(MAP_ANON)
	static int	noanon = 0;

	if (noanon == 0) {
		if ((va = (caddr_t)mmap(addr, len, prot,
		    (flags | MAP_ANON), -1, 0)) != (caddr_t)-1)
			return (va);

		if ((errno != EBADF) && (errno != EINVAL))
			return (va);
		else
			noanon = 1;
	}
#endif
	if (dz_fd == FD_UNAVAIL) {
		if ((dz_fd = open(MSG_ORIG(MSG_PTH_DEVZERO),
		    O_RDONLY)) == FD_UNAVAIL) {
			err = errno;
			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
			    MSG_ORIG(MSG_PTH_DEVZERO), strerror(err));
			return ((caddr_t)-1);
		}
		rtld_flags |= RT_FL_CLEANUP;
	}

	if ((va = (caddr_t)mmap(addr, len, prot, flags, dz_fd, 0)) ==
	    (caddr_t)-1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP),
		    MSG_ORIG(MSG_PTH_DEVZERO), strerror(err));
	}
	return (va);
}

static int	pr_fd = FD_UNAVAIL;

int
pr_open()
{
	char	proc[16];

	PRF_MCOUNT(71, pr_open);

	if (pr_fd == FD_UNAVAIL) {
		(void) snprintf(proc, 16, MSG_ORIG(MSG_FMT_PROC),
			(int)getpid());
		if ((pr_fd = open(proc, O_RDONLY)) == FD_UNAVAIL) {
			int	err = errno;

			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN), proc,
			    strerror(err));
		}
	}
	rtld_flags |= RT_FL_CLEANUP;
	return (pr_fd);
}

static int	nu_fd = FD_UNAVAIL;

caddr_t
nu_map(caddr_t addr, size_t len, int prot, int flags)
{
	caddr_t	va;
	int	err;

	if (nu_fd == FD_UNAVAIL) {
		if ((nu_fd = open(MSG_ORIG(MSG_PTH_DEVNULL),
		    O_RDONLY)) == FD_UNAVAIL) {
			err = errno;
			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
			    MSG_ORIG(MSG_PTH_DEVNULL), strerror(err));
			return ((caddr_t)-1);
		}
		rtld_flags |= RT_FL_CLEANUP;
	}

	if ((va = (caddr_t)mmap(addr, len, prot, flags, nu_fd, 0)) ==
	    (caddr_t)-1) {
		err = errno;
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP),
		    MSG_ORIG(MSG_PTH_DEVNULL), strerror(err));
	}
	return (va);
}

/*
 * Generic cleanup routine called prior to returning control to the user.
 * Insures that any ld.so.1 specific file descriptors or temport mapping are
 * released.
 */
void
cleanup()
{
	PRF_MCOUNT(87, cleanup);

	if (rtld_flags & RT_FL_DBNOTIF) {
		rd_event(RD_DLACTIVITY, RT_CONSISTENT, rtld_db_dlactivity());
		rtld_flags &= ~RT_FL_DBNOTIF;
	}

	if (dz_fd != FD_UNAVAIL) {
		(void) close(dz_fd);
		dz_fd = FD_UNAVAIL;
	}

	if (pr_fd != FD_UNAVAIL) {
		(void) close(pr_fd);
		pr_fd = FD_UNAVAIL;
	}

	if (nu_fd != FD_UNAVAIL) {
		(void) close(nu_fd);
		nu_fd = FD_UNAVAIL;
	}

	if (fmap) {
		fmap->fm_mflags = MAP_PRIVATE;
		if (fmap->fm_maddr) {
			(void) munmap((caddr_t)fmap->fm_maddr, fmap->fm_msize);
			fmap->fm_maddr = 0;
		}
		fmap->fm_msize = syspagsz;
	}
	rtld_flags &= ~RT_FL_CLEANUP;
}

/*
 * Routines for initializing, testing, and freeing link map permission values.
 */
static Permit	__Permit = { 1, ~0UL };
static Permit *	_Permit = &__Permit;

Permit *
perm_get()
{
	unsigned long	_cnt, cnt = _Permit->p_cnt;
	unsigned long *	_value, * value = &_Permit->p_value[0];
	Permit *	permit;

	PRF_MCOUNT(72, perm_get);

	/*
	 * Allocate a new Permit structure for return to the user based on the
	 * static Permit structure presently in use.
	 */
	if ((permit = calloc(sizeof (unsigned long), cnt + 1)) == 0)
		return ((Permit *)0);
	permit->p_cnt = cnt;
	_value = &permit->p_value[0];

	/*
	 * Determine the next available permission bit and update the global
	 * permit value to indicate this value is now taken.
	 */
	for (_cnt = 0; _cnt < cnt; _cnt++, _value++, value++) {
		unsigned long	bit;
		for (bit = 0x1; bit; bit = bit << 1) {
			if (*value & bit) {
				*value &= ~bit;
				*_value = bit;
				return (permit);
			}
		}
	}

	/*
	 * If all the present permission values have been exhausted allocate
	 * a new reference Permit structure.
	 */
	cnt++;
	if ((_Permit = calloc(sizeof (unsigned long), cnt + 1)) == 0)
		return ((Permit *)0);
	_Permit->p_cnt = cnt;
	value = &_Permit->p_value[cnt - 1];
	*value = ~0UL;

	/*
	 * Free the original Permit structure obtained for the user, and try
	 * again.
	 */
	free(permit);
	return (perm_get());
}

void
perm_free(Permit * permit)
{
	unsigned long	_cnt, cnt;
	unsigned long *	_value, * value;

	PRF_MCOUNT(73, perm_free);

	if (!permit)
		return;

	cnt = permit->p_cnt;
	_value = &permit->p_value[0];
	value = &_Permit->p_value[0];

	/*
	 * Set the users permit bit in the global Permit structure thus
	 * indicating thats its free for future use.
	 */
	for (_cnt = 0; _cnt < cnt; _cnt++, _value++, value++)
		*value |= *_value;

	free(permit);
}

int
perm_test(Permit * permit1, Permit * permit2)
{
	unsigned long	_cnt, cnt;
	unsigned long *	_value1, * _value2;

	PRF_MCOUNT(74, perm_test);

	if (!permit1 || !permit2)
		return (0);

	_value1 = &permit1->p_value[0];
	_value2 = &permit2->p_value[0];

	/*
	 * Determine which permit structure is the smaller.  Loop through the
	 * `p_value' elements looking for a match.
	 */
	if ((cnt = permit1->p_cnt) > permit2->p_cnt)
		cnt = permit2->p_cnt;

	for (_cnt = 0; _cnt < cnt; _cnt++, _value1++, _value2++)
		if (*_value1 & *_value2)
			return (1);
	return (0);
}

Permit *
perm_set(Permit * permit1, Permit * permit2)
{
	unsigned long	_cnt, cnt;
	unsigned long *	_value1, * _value2;
	Permit *	_permit = permit1;

	PRF_MCOUNT(75, perm_set);

	if (!permit2)
		return ((Permit *)0);

	cnt = permit2->p_cnt;
	_value2 = &permit2->p_value[0];

	/*
	 * If the original permission structure has not yet been initialized
	 * allocate a new structure for return to the user and simply copy the
	 * new structure to it.
	 */
	if (_permit == 0) {
		if ((_permit = calloc(sizeof (unsigned long), cnt + 1)) == 0)
			return ((Permit *)0);
		_permit->p_cnt = cnt;
		_value1 = &_permit->p_value[0];
		for (_cnt = 0; _cnt < cnt; _cnt++, _value1++, _value2++)
			*_value1 = *_value2;
		return (_permit);
	}

	/*
	 * If we don't presently have room in the destination permit structure
	 * to hold the new permission bit, reallocate a new structure.
	 */
	if (cnt > _permit->p_cnt) {
		if ((_permit = realloc((void *) _permit,
		    (size_t)((cnt + 1) * sizeof (unsigned long)))) == 0)
			return ((Permit *)0);

		/*
		 * Make sure the newly added entries are cleared, and update the
		 * new permission structures count.
		 */
		for (_cnt = _permit->p_cnt; _cnt < cnt; _cnt++)
			_permit->p_value[_cnt] = 0;
		_permit->p_cnt = cnt;
	}

	/*
	 * Set the appropriate permission bits.
	 */
	_value1 = &_permit->p_value[0];
	for (_cnt = 0; _cnt < cnt; _cnt++, _value1++, _value2++)
		*_value1 |= *_value2;

	return (_permit);
}

/*
 * Remove a permit.  It's possible that we could calculate if any permits still
 * exist in permit1, and if so free up the entire structure.  However, if this
 * object has been assigned a permit once there's a strong change it will get
 * them assigned again, so leave the `empty' permit structure alone.
 */
Permit *
perm_unset(Permit * permit1, Permit * permit2)
{
	unsigned long	_cnt, cnt;
	unsigned long *	_value1, * _value2;

	PRF_MCOUNT(76, perm_unset);

	if (!permit1 || !permit2)
		return ((Permit *)0);

	cnt = permit2->p_cnt;
	_value1 = &permit1->p_value[0];
	_value2 = &permit2->p_value[0];

	/*
	 * Unset the appropriate permission bits.
	 */
	for (_cnt = 0; _cnt < cnt; _cnt++, _value1++, _value2++)
		*_value1 &= ~(*_value2);

	return (permit1);
}

/*
 * Routine to determine if *only* one permit is set.
 */
int
perm_one(Permit * permit)
{
	unsigned long	cnt, _cnt;
	unsigned long *	value;
	int		number;

	if (!permit)
		return (0);

	cnt = permit->p_cnt;
	value = &permit->p_value[0];

	for (_cnt = number = 0; _cnt < cnt; _cnt++, value++) {
		if (*value == 0)
			continue;

		/*
		 * Given a bit pattern, subtract one and `and' it with the
		 * original.  If only one bit is set the result is zero.
		 */
		if ((*value & (*value - 1)) != 0)
			number++;
		number++;
	}
	return (number);
}


/*
 * Initialize the environ symbol. Traditionally this is carried out by the crt
 * code prior to jumping to main. However, init sections get fired before this
 * variable is initialized, so ld.so.1 sets this directly from the AUX vector
 * information.  In addition, a process may have multiple link-maps (ld.so.1's
 * debugging and preloading objects), and link auditing, and each may need an
 * environ variable set.
 *
 * This routine is called after a relocation() pass, and thus provides for:
 *
 *  o	setting environ on the main link-map after the initial application and
 *	its dependencies have been established.  Typically environ lives in the
 *	application (provided by its crt), but in older applications it might
 *	be in libc.  Who knows what's expected of applications not built on
 *	Solaris.
 *
 *  o	after loading a new shared object.  We can add shared objects to various
 *	link-maps, and any link-map dependencies requiring getopt() require
 *	their own environ.  In addition, lazy loading might bring in the
 *	supplier of environ (libc) after the link-map has been established and
 *	other objects are present.
 *
 * This routine handles all these scenarios, without adding unnecessary overhead
 * to ld.so.1.
 */
void
set_environ(Rt_map * lmp)
{
	Rt_map *	_lmp;
	Sym *		sym;
	Slookup		sl;

	/*
	 * Don't bother attempting to initialize the main link map until we've
	 * finished loading the initial dependencies, or if we've already
	 * established an environ.
	 */
	if (LIST(lmp)->lm_flags & LML_FLG_ENVIRON)
		return;

	sl.sl_name = MSG_ORIG(MSG_SYM_ENVIRON);
	sl.sl_permit = PERMIT(lmp);
	sl.sl_cmap = lmp;
	sl.sl_imap = lmp;
	sl.sl_rsymndx = 0;

	if (sym = LM_LOOKUP_SYM(lmp)(&sl, &_lmp, (LKUP_DEFT | LKUP_WEAK))) {
		char **	addr = (char **)sym->st_value;

		if (!(FLAGS(_lmp) & FLG_RT_FIXED))
			addr = (char **)((uintptr_t)addr +
				(uintptr_t)ADDR(_lmp));
		*addr = (char *)environ;
		LIST(lmp)->lm_flags |= LML_FLG_ENVIRON;
	}
}

/*
 * Determine whether we have a secure executable.  Uid and gid information
 * can be passed to us via the aux vector, however if these values are -1
 * then use the appropriate system call to obtain them.
 *
 *  o	If the user is the root they can do anything
 *
 *  o	If the real and effective uid's don't match, or the real and
 *	effective gid's don't match then this is determined to be a `secure'
 *	application.
 *
 * This function is called prior to any dependency processing (see _setup.c).
 * Any secure setting will remain in effect for the life of the process.
 */
void
security(uid_t uid, uid_t euid, gid_t gid, gid_t egid)
{
	if (uid == -1)
		uid = getuid();
	if (uid) {
		if (euid == -1)
			euid = geteuid();
		if (uid != euid)
			rtld_flags |= RT_FL_SECURE;
		else {
			if (gid == -1)
				gid = getgid();
			if (egid == -1)
				egid = getegid();
			if (gid != egid)
				rtld_flags |= RT_FL_SECURE;
		}
	}
}

/*
 * _REENTRANT code gets errno redefined to a function so provide for return
 * of the thread errno if applicable.  This has no meaning in ld.so.1 which
 * is basically singled threaded.  Provide the interface for our dependencies.
 */
#undef errno
int *
___errno()
{
	extern	int	errno;

	return (&errno);
}
