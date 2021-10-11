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
#pragma ident	"@(#)setup.c	1.59	99/09/14 SMI"


/*
 * Run time linker common setup.
 *
 * Called from _setup to get the process going at startup.
 */
#include	"_synonyms.h"

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<string.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<dlfcn.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"
#include	"profile.h"
#include	"debug.h"

int
setup(Rt_map * lmp, unsigned long envp, unsigned long auxv)
{
	Rt_map *	nlmp, * rlmp, * mlmp, ** tobj = 0;
	struct stat	status;
	int		features = 0;

	PRF_MCOUNT(49, setup);

	/*
	 * Finish any generic link-map initialization for the first object.
	 * Mark the object as having its .init/.fini already processed - they're
	 * not really processed yet, but will be by crt0, by marking them here
	 * we avoid collecting them for tsort() processing.
	 */
	FLAGS(lmp) |= (FLG_RT_ISMAIN | FLG_RT_INITCLCT | FLG_RT_FINICLCT);

	/*
	 * Add our two main link-maps to the dynlm_list
	 */
	lml_main.lm_flags |= LML_FLG_BASELM;
	if (list_append(&dynlm_list, &lml_main) == 0)
		return (0);

	lml_rtld.lm_flags |= (LML_FLG_RTLDLM | LML_FLG_NOAUDIT);
	if (list_append(&dynlm_list, &lml_rtld) == 0)
		return (0);

	/*
	 * Reset the link-map counts for both lists.  The init count is used to
	 * track how many objects have pending init sections.  As we don't fire
	 * off mains .init (crt does) and ld.so.1's has already been processed,
	 * we can effectively remove these objects from their list counts.  The
	 * object count is used to track how many objects have pending fini
	 * sections, so by the same logic we can reduce this as well.
	 */
	lml_main.lm_init = lml_main.lm_obj = 0;
	lml_rtld.lm_init = lml_rtld.lm_obj = 0;

	/*
	 * Initialize debugger information structure.  Some parts of this
	 * structure were initialized statically.
	 */
	r_debug.r_map = (Link_map *)lml_main.lm_head;
	r_debug.r_ldsomap = (Link_map *)lml_rtld.lm_head;
	r_debug.r_ldbase = r_debug.r_ldsomap->l_addr;
	rtld_db_priv.rtd_dynlmlst = &dynlm_list;

	mlmp = (Rt_map *)lml_main.lm_head;
	rlmp = (Rt_map *)lml_rtld.lm_head;

	/*
	 * Alert the debuggers that we are about to mess with the link-map list
	 * (dbx doesn't care about this as it doesn't know we're around until
	 * the getpid() call later, prehaps adb needs this state information).
	 */
	rtld_flags |= RT_FL_DBNOTIF;
	rd_event(RD_DLACTIVITY, RT_ADD, rtld_db_dlactivity());

	/*
	 * Determine whether the kernel has supplied a AT_SUN_EXECNAME aux
	 * vector.  This vector points to the full pathname, on the stack, of
	 * the object that started the process.  If this is null then
	 * AT_SUN_EXECNAME isn't supported (if the pathname exceeded the system
	 * limit (PATH_MAX) the exec would have failed).
	 */
	if (PATHNAME(lmp) != 0)
		rtld_flags |= RT_FL_EXECNAME;
	else
		PATHNAME(lmp) = NAME(lmp);

	/*
	 * Establish the interpretors name as that defined within the initial
	 * object (executable).  This provides for ORIGIN processing of ld.so.1
	 * dependencies.
	 */
	if (interp)
		PATHNAME(rlmp) = interp->i_name;
	else
		PATHNAME(rlmp) = NAME(rlmp);

	/*
	 * Far the most common application execution revolves around appending
	 * the application name to the users PATH definition, thus a full name
	 * is passed to exec() which will in turn be returned via
	 * AT_SUN_EXECNAME.  Applications may also be invoked from the current
	 * working directory, or via a relative name.
	 *
	 * By default, the expansion of a relative pathname is deferred until
	 * it is required.  However, should a process change directory before a
	 * full pathname is required, it may be necessary to determine the
	 * pathname in advance.
	 */
	if (FLAGS(lmp) & FLG_RT_ORIGIN)
		(void) fullpath(lmp);
	if (FLAGS(rlmp) & FLG_RT_ORIGIN)
		(void) fullpath(rlmp);

	if (platform)
		platform_sz = strlen(platform);

	/*
	 * Determine the dev/inode information for the executable to complete
	 * load_so() checking for those who might dlopen(a.out).
	 */
	if (stat(PATHNAME(lmp), &status) == 0) {
		STDEV(lmp) = status.st_dev;
		STINO(lmp) = status.st_ino;
	}

	/*
	 * Initialize any configuration information.
	 */
	if (!(rtld_flags & RT_FL_NOCFG)) {
		if ((features = elf_config(lmp)) == -1)
			return (0);
	}

	/*
	 * Establish the modes of the intial object.  These modes are
	 * propagated to any preloaded objects and explicit shared library
	 * dependencies.
	 */
	MODE(lmp) |= (bind_mode | RTLD_NODELETE | RTLD_GLOBAL | RTLD_WORLD);
	COUNT(lmp) = 1;

	/*
	 * If debugging was requested initialize things now that any cache has
	 * been established.
	 */
	if (dbg_str) {
		dbg_mask |= dbg_setup(dbg_str);
	}

	/*
	 * Now that debugging is enabled generate any diagnostics from any
	 * previous events.
	 */
	if (features) {
		DBG_CALL(Dbg_file_config_dis(config->c_name, features));
	}
	if (dbg_mask) {
		DBG_CALL(Dbg_file_ldso(rt_name, (unsigned long)DYN(rlmp),
		    ADDR(rlmp), envp, auxv));

		DBG_CALL(Dbg_file_elf(PATHNAME(mlmp), (unsigned long)DYN(mlmp),
		    ADDR(mlmp), MSIZE(mlmp), ENTRY(mlmp),
		    (unsigned long)PHDR(mlmp), PHNUM(mlmp),
		    get_linkmap_id(LIST(mlmp))));
	}

	/*
	 * Enable auditing.
	 */
	if (audit_objs) {
		/*
		 * Any global auditing (set using LD_AUDIT) that can't be
		 * established is non-fatal.
		 */
		if (((auditors = calloc(1, sizeof (Audit_desc))) == 0) ||
		    ((auditors->ad_name = strdup(audit_objs)) == 0))
			return (0);

		(void) audit_setup(mlmp, auditors);
		lml_main.lm_flags |= auditors->ad_flags;
	}
	if (AUDITORS(mlmp)) {
		/*
		 * Any object required auditing (set with a DT_DEPAUDIT dynamic
		 * entry) that can't be established is fatal.
		 */
		if (audit_setup(mlmp, AUDITORS(mlmp)) == 0)
			return (0);
		FLAGS1(mlmp) |= AUDITORS(mlmp)->ad_flags;
		lml_main.lm_flags |= LML_FLG_LOCAUDIT;
	}

	/*
	 * Explicitly add the initial object and ld.so.1 to those objects being
	 * audited.  Note, although the ld.so.1 link-map isn't auditable we need
	 * to establish a cookie for ld.so.1 as this may be bound to via the
	 * dl*() family.
	 */
	if ((lml_main.lm_flags | FLAGS1(mlmp)) & FL1_MSK_AUDIT) {
		if (((audit_objopen(mlmp, mlmp) == 0) ||
		    (audit_objopen(mlmp, rlmp) == 0)) &&
		    (FLAGS1(mlmp) & FL1_MSK_AUDIT))
			return (0);
	}

	/*
	 * Map in any preloadable shared objects.  Note, it is valid to preload
	 * a 4.x shared object with a 5.0 executable (or visa-versa), as this
	 * functionality is required by ldd(1).
	 */
	if (preload_objs) {
		char *	objs, * ptr, *next;

		DBG_CALL(Dbg_util_nl());

		if ((objs = strdup(preload_objs)) == 0)
			return (0);

		ptr = strtok_r(objs, MSG_ORIG(MSG_STR_DELIMIT), &next);
		do {
			DBG_CALL(Dbg_file_preload(ptr));

			/*
			 * If this is a secure application only allow simple
			 * filenames to be preloaded. The lookup for these files
			 * will be restricted, but is allowed by placing
			 * preloaded objects in secure directories.
			 */
			if (rtld_flags & RT_FL_SECURE) {
				if (strchr(ptr, '/')) {
					DBG_CALL(Dbg_libs_ignore(ptr));
					continue;
				}
			}

			if (((nlmp = load_one(&lml_main, ptr, lmp, MODE(lmp),
			    (FLG_RT_PRELOAD | FLG_RT_INTRPOSE))) == 0) ||
			    (bound_add(REF_NEEDED, lmp, nlmp) == 0)) {
				if (lml_main.lm_flags & LML_TRC_ENABLE)
					continue;
				else
					return (0);
			}
			lml_main.lm_flags |= LML_FLG_INTRPOSE;

		} while ((ptr = strtok_r(NULL,
		    MSG_ORIG(MSG_STR_DELIMIT), &next)) != NULL);

		free(objs);
	}

	/*
	 * Load all dependent (needed) objects.
	 */
	if (analyze_so(lmp) == 0)
		return (0);

	/*
	 * Relocate all the dependencies we've just added.
	 * crle()'s for its dependency and dldump() pass sets RTLD_CONFGEN.
	 * During the former only dependency information is gathered.  Note that
	 * a side-effect of this is that filters don't get loaded.  For now this
	 * is ok, as we'd have to find a way of establishing LD_LOADFLTR at
	 * runtime to make this all invisible to the user.  For the latter pass
	 * relocations are carried out later once all objects required by the
	 * configuration cache have been loaded.
	 */
	if ((rtld_flags & RT_FL_CONFGEN) == 0) {
		DBG_CALL(Dbg_file_nl());

		if (relocate_so(lmp) == 0)
			return (0);

		/*
		 * Sort the .init sections of all objects we've added.  (If
		 * we're tracing we only need to execute this under ldd(1) -i).
		 */
		if (((lml_main.lm_flags & LML_TRC_ENABLE) == 0) ||
		    (rtld_flags & RT_FL_INIT)) {
			if ((tobj = tsort(lmp, LIST(lmp)->lm_init,
			    RT_SORT_REV)) == (Rt_map **)S_ERROR)
				return (0);
		}

		/*
		 * If we are tracing we're done.
		 */
		if (lml_main.lm_flags & LML_TRC_ENABLE)
			exit(0);
	}

	/*
	 * Inform the debuggers we're here and stable.  Newer debuggers can
	 * indicate their presence by setting the DT_DEBUG entry in the dynamic
	 * executable (see elf_new_lm()).  In this case call getpid() so that
	 * the debugger can catch the system call.  This allows the debugger to
	 * initialize at this point and consequently allows the user to set
	 * break points in .init code.
	 */
	rd_event(RD_DLACTIVITY, RT_CONSISTENT, rtld_db_dlactivity());
	rtld_flags &= ~RT_FL_DBNOTIF;

	if (rtld_flags & RT_FL_DEBUGGER) {
		r_debug.r_flags |= RD_FL_ODBG;
		(void) getpid();
	}

	/*
	 * Call any necessary auiting routines, clean up any file descriptors
	 * and such, and then fire all dependencies .init sections.
	 */
	rtld_flags |= RT_FL_APPLIC;

	rd_event(RD_PREINIT, 0, rtld_db_preinit());

	if ((lml_main.lm_flags | FLAGS1(mlmp)) & FL1_AU_ACTIVITY)
		audit_activity(mlmp, LA_ACT_CONSISTENT);
	if ((lml_main.lm_flags | FLAGS1(mlmp)) & FL1_AU_PREINIT)
		audit_preinit(mlmp);

	if (rtld_flags & RT_FL_CLEANUP)
		cleanup();

	if (tobj)
		call_init(tobj);

	rd_event(RD_POSTINIT, 0, rtld_db_postinit());

	return (1);
}
