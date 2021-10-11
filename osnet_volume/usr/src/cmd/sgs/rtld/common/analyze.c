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
#pragma ident	"@(#)analyze.c	1.101	99/10/07 SMI"

#include	"_synonyms.h"

#include	<string.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<sys/stat.h>
#include	<sys/mman.h>
#include	<fcntl.h>
#include	<limits.h>
#include	<dlfcn.h>
#include	<errno.h>
#include	<link.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"
#include	"profile.h"
#include	"debug.h"

#if	defined(i386)
extern int	_elf_copy_gen(Rt_map *);
#endif

static Fct *	vector[] = {
	&elf_fct,
#ifdef A_OUT
	&aout_fct,
#endif A_OUT
	0
};

/*
 * Analyze a link map.  This routine is called at startup to continue the
 * processing of the main executable, or from a dlopen() to continue the
 * processing a newly opened shared object.
 *
 * In each instance we traverse the link-map list starting with the new objects
 * link-map, as dependencies are analyzed they are added to the link-map list.
 * Thus the list grows as we traverse it - this results in the breadth first
 * ordering of all needed objects.
 */
int
analyze_so(Rt_map * clmp)
{
	Rt_map *	lmp;
	Lm_list *	lml = LIST(clmp);
	int		ret = 1;

	PRF_MCOUNT(51, analyze_so);

	if (lml->lm_flags & LML_FLG_ANALYSIS)
		return (1);

	lml->lm_flags |= LML_FLG_ANALYSIS;

	for (lmp = clmp; lmp; lmp = (Rt_map *)NEXT(lmp)) {
		if (FLAGS(lmp) & FLG_RT_ANALYZED)
			continue;

		DBG_CALL(Dbg_file_analyze(NAME(lmp), MODE(lmp)));
		DBG_CALL(Dbg_file_bind_entry(lmp, lmp));

		/*
		 * If this link map represents a relocatable object, then we
		 * need to finish the link-editing of the object at this point.
		 */
		if (FLAGS(lmp) & FLG_RT_OBJECT) {
			if (!(elf_obj_fini(lml, lmp))) {
				if ((lml->lm_flags & LML_TRC_ENABLE) == 0) {
					ret = 0;
					break;
				}
			}
		}
		if (!(LM_LD_NEEDED(lmp)(lmp))) {
			if ((lml->lm_flags & LML_TRC_ENABLE) == 0) {
				ret = 0;
				break;
			}
		}
		FLAGS(lmp) |= FLG_RT_ANALYZED;
	}

	lml->lm_flags &= ~LML_FLG_ANALYSIS;

	return (ret);
}

/*
 * Relocate one or more objects that have just been mapped.
 */
int
relocate_so(Rt_map * clmp)
{
	Rt_map *	lmp, * slmp = clmp;
	int		ret = 1;
	Lm_list *	lml = LIST(clmp);

	PRF_MCOUNT(52, relocate_so);

	if (lml->lm_flags & LML_FLG_RELOCATING)
		return (1);

	lml->lm_flags |= LML_FLG_RELOCATING;

	/*
	 * If we've dlopen()'ed someone requesting its dependencies be opened
	 * RTLD_NOW, then we need to rescan the whole link map looking for its
	 * dependencies.
	 */
	if (rtld_flags & RT_FL_RELNOW) {
		slmp = lml->lm_head;
		rtld_flags &= ~RT_FL_RELNOW;
	}

	for (lmp = slmp; lmp; lmp = (Rt_map *)NEXT(lmp)) {

		if ((FLAGS(lmp) & (FLG_RT_RELOCED | FLG_RT_RELNOW)) ==
		    FLG_RT_RELOCED)
			continue;

		if (((lml->lm_flags & LML_TRC_ENABLE) == 0) ||
		    (lml->lm_flags & LML_TRC_WARN)) {
			int	mode = 0;

			if (FLAGS(lmp) & FLG_RT_RELNOW)
				mode = 1;

			if (LM_RELOC(lmp)(lmp, mode) == 0)
				if ((lml->lm_flags & LML_TRC_ENABLE) == 0) {
					ret = 0;
					break;
				}
			if (mode) {
				FLAGS(lmp) |= FLG_RT_RELNOW;
				continue;
			}
		}

		if (lmp->rt_flags & FLG_RT_MOVE) {
			move_data(lmp);
			lmp->rt_flags &= ~FLG_RT_MOVE;
		}

		/*
		 * Indicate that the objects relocation is complete.
		 */
		FLAGS(lmp) |= FLG_RT_RELOCED;

		/*
		 * If this object is a filter with the load filter flag in
		 * effect, or we're tracing, trigger the loading of all its
		 * filtees.
		 */
		if (REFNAME(lmp) && ((FLAGS(lmp) & FLG_RT_LOADFLTR) ||
		    (rtld_flags & RT_FL_LOADFLTR)))
			(void) SYMINTP(lmp)(0, lmp, 0, 0, 0);

	}

	/*
	 * Perform special copy relocations.  These are only meaningful for
	 * dynamic executables (fixed and head of their link-map list).  If
	 * this ever has to change then the infrastructure of COPY() has to
	 * change as presently this element is used to capture both receiver
	 * and supplier of copy data.
	 */
	if ((FLAGS(clmp) & FLG_RT_FIXED) && (clmp == LIST(clmp)->lm_head) &&
	    (((lml->lm_flags & LML_TRC_ENABLE) == 0) ||
	    (lml->lm_flags & LML_TRC_WARN))) {
		Rel_copy *	rcp;
		List *		list;
		Listnode *	lnp1, * lnp2;

#if	defined(i386)
		if (_elf_copy_gen(clmp) == 0)
			return (0);
#endif
		for (LIST_TRAVERSE(&COPY(clmp), lnp1, list)) {
			for (LIST_TRAVERSE(list, lnp2, rcp)) {
				(void) memcpy(rcp->r_radd, rcp->r_dadd,
				    rcp->r_size);
			}
		}
	}

	lml->lm_flags &= ~LML_FLG_RELOCATING;

	/*
	 * At this point we've completed the addition of a new group of objects,
	 * either the initial objects that start the process (called from
	 * setup()), a group added through lazy loading (after setup()), or from
	 * a dlopen() request.  Thus this is a suitable time to determine if an
	 * `environ' symbol must be initialized.
	 */
	if ((lml->lm_flags & LML_TRC_ENABLE) == 0)
		set_environ(clmp);

	return (ret);
}

/*
 * Determine the object type of a file.
 */
Fct *
are_u_this(const char * path)
{
	int	i;
	char *	maddr;

	PRF_MCOUNT(53, are_u_this);

	/*
	 * Map in the first page of the file.  Determine the memory size based
	 * on the larger of the filesize (obtained in load_so()) or the mapping
	 * size.  The mapping allows for execution as filter libraries may be
	 * able to use this initial mapping and require nothing else.
	 */
	if ((maddr = (char *)mmap(fmap->fm_maddr, fmap->fm_msize,
	    (PROT_READ | PROT_EXEC), fmap->fm_mflags, fmap->fm_fd, 0)) ==
	    (char *)-1) {
		int	err = errno;

		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), path,
		    strerror(err));
		return (0);
	}

	/*
	 * From now on we will re-use fmap->fm_maddr as the mapping address
	 * so we augment the flags with MAP_FIXED.
	 */
	fmap->fm_maddr = maddr;
	fmap->fm_mflags |= MAP_FIXED;
	rtld_flags |= RT_FL_CLEANUP;

	/*
	 * Search through the object vectors to determine what kind of
	 * object we have.
	 */
	for (i = 0; vector[i]; i++) {
		if ((vector[i]->fct_are_u_this)())
			return (vector[i]);
	}

	/*
	 * Unknown file type - return error.
	 */
	eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_UNKNFILE), path);
	return (0);

}


/*
 * Function that determines whether a file name has already been loaded; if so,
 * returns a pointer to its link map structure; else returns a NULL pointer.
 */
static Rt_map *
is_so_matched(Rt_map * lmp, const char * name, int base)
{
	Listnode *	lnp;
	const char *	cp, * _cp;

	/*
	 * Typically, dependencies are specified as simple file names
	 * (DT_NEEDED == libc.so.1), which are expanded to full pathnames to
	 * open the file.  The full pathname is NAME(), and the original name
	 * is maintained on the ALIAS() list. Look through the ALIAS list first,
	 * as this is most likely to match other dependency uses.
	 */
	for (LIST_TRAVERSE(&ALIAS(lmp), lnp, cp)) {
		if (base && ((_cp = strrchr(cp, '/')) != NULL))
			_cp++;
		else
			_cp = cp;

		if (strcmp(name, _cp) == 0)
			return (lmp);
	}

	/*
	 * Finally compare full paths, this is sometimes useful for catching
	 * filter names, or for those that dlopen() the dynamic executable.
	 */
	if (base && ((_cp = strrchr(NAME(lmp), '/')) != NULL))
		_cp++;
	else
		_cp = NAME(lmp);

	if (strcmp(name, _cp) == 0)
		return (lmp);

	if (PATHNAME(lmp) != NAME(lmp)) {
		if (base && ((_cp = strrchr(PATHNAME(lmp), '/')) != NULL))
			_cp++;
		else
			_cp = PATHNAME(lmp);

		if (strcmp(name, _cp) == 0)
			return (lmp);
	}
	return (0);
}

Rt_map *
is_so_loaded(Lm_list * lml, const char * name, int base)
{
	Rt_map *	lmp;
	const char *	_name;

	PRF_MCOUNT(54, is_so_loaded);

	/*
	 * If we've been asked to do a basename search reduce the input name
	 * to its basename.
	 */
	if (base && ((_name = strrchr(name, '/')) != NULL))
		_name++;
	else
		_name = name;

	/*
	 * Loop through the callers link-map list
	 */
	for (lmp = lml->lm_head; lmp; lmp = (Rt_map *)NEXT(lmp)) {
		if (FLAGS(lmp) & FLG_RT_OBJECT)
			continue;

		if (is_so_matched(lmp, _name, base))
			return (lmp);
	}
	return ((Rt_map *)0);
}

/*
 * Tracing is enabled by the LD_TRACE_LOADED_OPTIONS environment variable which
 * is normally set from ldd(1).  For each link map we load, print the load name
 * and the full pathname of the shared object.  Loaded objects are skipped until
 * LML_FLG_TRACE_SKIP is disabled (this enables ldd to trace a shared object as
 * it gets preloaded with lddstub without revealing the object itself).
 */
/* ARGSUSED4 */
static void
trace_so(Lm_list * lml, int found, const char * name, const char * path,
    int alter)
{
	PRF_MCOUNT(85, trace_so);

	if (lml->lm_flags & LML_TRC_SKIP) {
		lml->lm_flags &= ~LML_TRC_SKIP;
		return;
	}

	if (found == 0)
		(void) printf(MSG_INTL(MSG_LDD_FIL_NFOUND), name);
	else {
		const char *	str;

		if (alter)
			str = MSG_INTL(MSG_LDD_FIL_ALTER);
		else
			str = MSG_ORIG(MSG_STR_EMPTY);

		/*
		 * If the load name isn't a full pathname print its associated
		 * pathname.
		 */
		if (*name == '/')
			(void) printf(MSG_ORIG(MSG_LDD_FIL_PATH), path, str);
		else
			(void) printf(MSG_ORIG(MSG_LDD_FIL_EQUIV), name, path,
			    str);
	}
}

/*
 * This function loads the named file and returns a pointer to its link map.
 * It is assumed that the caller has already checked that the file is not
 * already loaded before calling this function (refer is_so_loaded()).
 * Find and open the file, map it into memory, add it to the end of the list
 * of link maps and return a pointer to the new link map.  Return 0 on error.
 */
static Rt_map *
load_so(Lm_list * lml, const char * name, Rt_map * clmp, int flags)
{
	Fct *		ftp;
	struct stat	status;
	const char *	_str, * str = name;
	int		slash = 0, fd, aflag = 0, lasterr = 0;
	size_t		_len, len;
	Rt_map *	nlmp;
	int		why;
	Word		what;

	PRF_MCOUNT(55, load_so);

	/*
	 * If the file is the run time linker then it's already loaded.
	 */
	if (strcmp(name, NAME(lml_rtld.lm_head)) == 0)
		return (lml_rtld.lm_head);

	/*
	 * Determine the length of the input filename (for max path length
	 * verification) and whether the filename contains any '/'s.
	 */
	for (_str = str; *_str; _str++) {
		if (*_str == '/') {
			slash++;
		}
	}
	_len = len = (_str - str) + 1;

	/*
	 * If we are passed a 'null' link-map this means that this is the first
	 * object to be loaded on this link-map list. In that case we set the
	 * link-map to ld.so.1's link-map.
	 *
	 * This link-map is referenced to determine what lookup rules to use
	 * when searching for files.  By using ld.so.1's we are defaulting to
	 * ELF look-up rules.
	 *
	 * Note: This case happens when loading the first object onto
	 *	 the plt_tracing link-map.
	 */
	if (clmp == 0)
		clmp = lml_rtld.lm_head;

	if (slash) {
		fd = 0;
		_str = str;

		/*
		 * Use the filename as is.  If directory configuration exists
		 * determine if this path is known.
		 */
		if (rtld_flags & RT_FL_DIRCFG) {
			Rtc_obj *	obj;
			const char *	alt;

			if (obj = elf_config_ent(str, elf_hash(str), 0, &alt)) {
				/*
				 * If the path is explicitly defined as
				 * non-existant (ie. a unused platform specific
				 * library), then go no further.
				 */
				if (obj->co_flags & RTC_OBJ_NOEXIST)
					fd = -1;
				else {
					/*
					 * If object alternatives exist
					 * determine if this object is included.
					 */
					if ((rtld_flags & RT_FL_OBJALT) &&
					    (obj->co_flags & RTC_OBJ_ALTER) &&
					    (lml == &lml_main)) {
						_str = alt;

						/*
						 * Check that this alternate
						 * object isn't already loaded.
						 */
						if ((nlmp = is_so_loaded(lml,
						    _str, 0)) != 0)
							return (nlmp);
						aflag = 1;
					}
				}
			}
		}
		if (fd != -1) {
			if (((fd = open(_str, O_RDONLY, 0)) != -1) && aflag) {
				DBG_CALL(Dbg_file_config_obj(str, 0, _str));
			}
		}

		if (fd != -1) {
			(void) fstat(fd, &status);
			fmap->fm_fd = fd;
			fmap->fm_fsize = status.st_size;

			if ((ftp = are_u_this(_str)) == 0) {
				(void) close(fd);
				fd = fmap->fm_fd = -1;
				lasterr = 0;
			} else if ((why = ftp->fct_are_u_compat(&what)) > 0) {
				DBG_CALL(Dbg_file_rejected(_str, why, what));
				(void) close(fd);
				fd = fmap->fm_fd = -1;
				lasterr = ENOENT;
			}
		} else
			lasterr = errno;
	} else {
		/*
		 * No '/' - for each directory on list, make a pathname using
		 * that directory and filename and try to open that file.
		 */
		Pnode *	dir, * dirlist = (Pnode *)0;
		Word	strhash = 0;

		DBG_CALL(Dbg_libs_find(str));

		for (fd = -1, dir = get_next_dir(&dirlist, clmp, flags); dir;
		    dir = get_next_dir(&dirlist, clmp, flags)) {
			static Rtc_obj	Obj = { 0 };
			Rtc_obj *	dobj;

			if (dir->p_name == 0)
				continue;
			if (dir->p_info) {
				dobj = (Rtc_obj *)dir->p_info;
				if (dobj->co_flags & RTC_OBJ_NOEXIST)
					continue;
			} else
				dobj = 0;

			/*
			 * If configuration information exists see if this
			 * directory/file combination exists.
			 */
			if ((rtld_flags & RT_FL_DIRCFG) && ((dobj == 0) ||
			    (dobj->co_id != 0))) {
				Rtc_obj *	fobj;
				const char *	alt = 0;

				/*
				 * If this pnode has not yet been searched for
				 * in the configuration file go find it.
				 */
				if (dobj == 0) {
					dobj = elf_config_ent(dir->p_name,
					    elf_hash(dir->p_name), 0, 0);
					if (dobj == 0)
						dobj = &Obj;
					dir->p_info = (void *)dobj;

					if (dobj->co_flags & RTC_OBJ_NOEXIST)
						continue;
				}

				/*
				 * If we found a directory search for the file.
				 */
				if (dobj->co_id != 0) {
					if (strhash == 0)
						strhash = elf_hash(str);
					fobj = elf_config_ent(str, strhash,
					    dobj->co_id, &alt);

					/*
					 * If this object specifically does
					 * not exist, or the object can't be
					 * found in a know-all-entries
					 * directory, continue looking.  If the
					 * object does exist determine if an
					 * alternative object exists.
					 */
					if (fobj == 0) {
					    if (dobj->co_flags &
						RTC_OBJ_ALLENTS)
						    continue;
					} else {
					    if (fobj->co_flags &
						RTC_OBJ_NOEXIST)
						    continue;

					    if ((rtld_flags & RT_FL_OBJALT) &&
						(fobj->co_flags &
						RTC_OBJ_ALTER) &&
						(lml == &lml_main)) {
						    _str = alt;
						    aflag = 1;
					    }
					}
				}
			}

			if (aflag == 0) {
				/*
				 * Protect ourselves from building an invalid
				 * pathname.
				 */
				len = _len + dir->p_len + 1;
				if (len >= PATH_MAX) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_SYS_OPEN), str,
					    strerror(ENAMETOOLONG));
					continue;
				}
				if (!(_str =
				    (LM_GET_SO(clmp)(dir->p_name, str))))
					continue;
			}

			DBG_CALL(Dbg_libs_found(_str));
			if (lml->lm_flags & LML_TRC_SEARCH)
				(void) printf(MSG_INTL(MSG_LDD_PTH_TRYING),
				    _str);

			/*
			 * If we're being audited tell the audit library of the
			 * file we're about to go search for.  The audit library
			 * may offer an alternative dependency.
			 */
			if ((lml->lm_flags | FLAGS1(clmp)) & FL1_AU_SEARCH) {
				char *	aname = audit_objsearch(clmp, _str,
				    (int)dir->p_orig);
				DBG_CALL(Dbg_libs_audit(_str, aname));
				if (aname == 0)
					continue;
				_str = aname;
				len = strlen(_str) + 1;
			}

			if ((fd = open(_str, O_RDONLY, 0)) != -1) {
				if (aflag) {
				    DBG_CALL(Dbg_file_config_obj(dir->p_name,
					str, _str));
				}

				if (fstat(fd, &status) == -1) {
					int err = errno;
					eprintf(ERR_FATAL,
						MSG_INTL(MSG_SYS_FSTAT),
						_str, strerror(err));
					(void) close(fd);
					fd = fmap->fm_fd = -1;
					lasterr = 0;
					continue;
				}
				fmap->fm_fd = fd;
				fmap->fm_fsize = status.st_size;

				if ((ftp = are_u_this(_str)) == 0) {
					(void) close(fd);
					fd = fmap->fm_fd = -1;
					lasterr = 0;
					continue;
				}

				if ((why = ftp->fct_are_u_compat(&what)) > 0) {
					DBG_CALL(Dbg_file_rejected(_str,
					    why, what));
					(void) close(fd);
					fd = fmap->fm_fd = -1;
					lasterr = 0;
					continue;
				}

				break;
			} else
				lasterr = errno;
		}
	}

	/*
	 * If no file was found complete any tracing output and return.  Note
	 * that auxiliary filters do not constitute an error condition if they
	 * cannot be located.
	 */
	if (fd == -1) {
		if ((flags & FLG_RT_NOERROR) == 0) {
			if (lml->lm_flags & LML_TRC_ENABLE)
				trace_so(lml, 0, name, 0, aflag);
			else if (lasterr) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
				    name, strerror(lasterr));
			}
		}
		return (0);
	}

	/*
	 * If the file has been found determine from the new files status
	 * information if this file is actually linked to one we already have
	 * mapped.  This catches symlink names not caught by is_so_loaded().
	 */
	for (nlmp = lml->lm_head; nlmp; nlmp = (Rt_map *)NEXT(nlmp)) {
		if ((STDEV(nlmp) != status.st_dev) ||
		    (STINO(nlmp) != status.st_ino))
			continue;

		DBG_CALL(Dbg_file_skip(_str, NAME(nlmp)));
		(void) close(fd);
		fmap->fm_fd = -1;
		if (list_append(&ALIAS(nlmp), strdup(name)) == 0)
			return (0);
		return (nlmp);
	}

	if (lml->lm_flags & LML_TRC_ENABLE)
		trace_so(lml, 1, name, _str, aflag);

	/*
	 * Typically we call fct_map_so() with the full pathname of the opened
	 * file (_str) and the name that started the search (name), thus for
	 * a typical dependency on libc this would be /usr/lib/libc.so.1 and
	 * libc.so.1 (DT_NEEDED).  The original name is maintained on an ALIAS
	 * list for comparison when bringing in new dependencies.  If the user
	 * specified name as a full path (from a dlopen() for example) then
	 * there's no need to create an ALIAS.
	 */
	if (name == _str)
		name = 0;

	/*
	 * Unless we're referring to a cache name, duplicate the name string
	 * (any pathname has either been supplied from user code or has been
	 * constructed on our stack).
	 */
	if (!aflag) {
		char *	_new;

		if ((_new = (char *)malloc(len)) == 0)
			return (0);
		(void) strcpy(_new, _str);
		_str = _new;
	}

	/*
	 * Alert the debuggers that we are about to mess with the link-map.
	 */
	if ((rtld_flags & RT_FL_DBNOTIF) == 0) {
		rtld_flags |= (RT_FL_DBNOTIF | RT_FL_CLEANUP);
		rd_event(RD_DLACTIVITY, RT_ADD, rtld_db_dlactivity());
	}

	/*
	 * Find out what type of object we have and map it in.  If the link-map
	 * is created save the dev/inode information for later comparisons.
	 */
	if (ftp && ((nlmp = (ftp->fct_map_so)(lml, _str, name)) != 0)) {
		STDEV(nlmp) = status.st_dev;
		STINO(nlmp) = status.st_ino;

		if (aflag)
			FLAGS(nlmp) |= FLG_RT_ALTER;

		/*
		 * If we're being audited tell the audit library of the file
		 * we've just opened.  Note, if the new link-map requires local
		 * auditing of its dependencies we also register its opening.
		 */
		if (((lml->lm_flags | FLAGS1(clmp) | FLAGS1(nlmp)) &
		    FL1_MSK_AUDIT) && (lml == LIST(clmp))) {
			if (audit_objopen(clmp, nlmp) == 0) {
				remove_so(lml, nlmp);
				nlmp = 0;
			}
		}
	}

	/*
	 * Close the original file so as not to accumulate file descriptors.
	 */
	(void) close(fmap->fm_fd);
	fmap->fm_fd = -1;

	return (nlmp);
}

/*
 * If this is a newly added dependency update any modes and determine if
 * immediate bindings need to be established.
 */
static void
update_mode(Rt_map * clmp, Rt_map * lmp, int nmode)
{
	int	omode = MODE(lmp);

	MODE(lmp) |= nmode;

	if ((FLAGS(lmp) & FLG_RT_RELOCED) &&
	    !(omode & RTLD_NOW) && (nmode & RTLD_NOW)) {
		DBG_CALL(Dbg_file_bind_entry(clmp, lmp));
		rtld_flags |= RT_FL_RELNOW;
		FLAGS(lmp) |= FLG_RT_RELNOW;
	}
}

/*
 * The central routine for loading shared objects.  Insures ldd() diagnostics,
 * handles and any other related additions are all done in one place.
 */
Rt_map *
load_one(Lm_list * lml, const char * name, Rt_map * clmp, int nmode, int flags)
{
	Rt_map *	nlmp;
	Listnode *	lnp1;
	Dl_handle *	dlp;

	if (name == 0) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_NULLFILE));
		remove_so(lml, 0);
		return (0);
	}

	/*
	 * Is the file already loaded?  We first compare full pathnames, and if
	 * no match is found attempt to load the file.  This provides for folks
	 * who wish to load different files with the same name (either by
	 * explicit reference to the full path, or because the callers have
	 * different runpath information).
	 */
	if ((nlmp = is_so_loaded(lml, name, 0)) == 0) {
		/*
		 * If this isn't a noload request attempt to load the file.
		 */
		if ((nmode & RTLD_NOLOAD) == 0) {
			/*
			 * First generate any ldd(1) diagnostics.
			 */
			if (lml->lm_flags & (LML_TRC_VERBOSE | LML_TRC_SEARCH))
				(void) printf(MSG_INTL(MSG_LDD_FIL_FIND), name,
				    NAME(clmp));

			/*
			 * If we're being audited tell the audit library of the
			 * file we're about to go search for.  The audit library
			 * may offer an alternative dependency.
			 */
			if (((lml->lm_flags | FLAGS1(clmp)) &
			    FL1_AU_ACTIVITY) && (lml == LIST(clmp)))
				audit_activity(clmp, LA_ACT_ADD);

			if ((lml->lm_flags | FLAGS1(clmp)) & FL1_AU_SEARCH) {
				char * _name;

				if ((_name = audit_objsearch(clmp, name,
				    LA_SER_ORIG)) == 0) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_GEN_AUDITERM), name);
					return (0);
				}
				if (_name != name) {
					free((void *)name);
					name = strdup(_name);
				}
			}
			nlmp = load_so(lml, name, clmp, flags);

			/*
			 * If we've loaded a library which identifies itself as
			 * not being dlopen()'able catch that here.  We let
			 * these objects through under RTLD_CONFGEN as they're
			 * only being mapped to be dldump'ed.
			 */
			if (nlmp && (rtld_flags & RT_FL_APPLIC) &&
			    (FLAGS(nlmp) & FLG_RT_NOOPEN) &&
			    ((nmode & RTLD_CONFGEN) == 0)) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_NOOPEN),
				    NAME(nlmp));
				remove_so(lml, nlmp);
				return (0);
			}
		}

		/*
		 * If the file couldn't be loaded, do another comparison of
		 * loaded files using just the basename.  This catches folks
		 * who may have loaded multiple full pathname files (possibly
		 * from setxid applications) to satisfy dependency relationships
		 * (i.e., a file might have a dependency on foo.so.1 which has
		 * already been opened using its full pathname).
		 */
		if ((nlmp == 0) && ((name[0] == '/') ||
		    ((nlmp = is_so_loaded(lml, name, 1)) == 0))) {
			if (nmode & RTLD_NOLOAD) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN), name,
				    strerror(ENOENT));
			}
			remove_so(lml, 0);
			return (0);
		}

		/*
		 * If this object has indicated that it should be isolated as a
		 * a group (DT_FLAGS_1 contains DF_1_GROUP - object was built
		 * with -B group), or if the callers direct bindings indicate
		 * it should be isolated as a group (DYNINFO flags contains
		 * FLG_DI_GROUP - dependency followed -zgroupperm), establish
		 * the appropriate mode.
		 *
		 * The intent of an object defining itself as a group is to
		 * isolate the relocation of the group within its own members,
		 * however, unless opened through dlopen(), in which case we
		 * assume dlsym() will be used to located symbols in the new
		 * object, we still need to associate it with the caller for
		 * it to be bound with.  This is eqivalent to a
		 * dlopen(RTLD_GROUP) and dlsym() using the returned handle.
		 */
		if ((FLAGS(nlmp) | flags) & FLG_RT_SETGROUP) {
			nmode &= ~RTLD_WORLD;
			nmode |= RTLD_GROUP;
			/*
			 * If the object wasn't explicitly dlopen()'ed associate
			 * it with the parent.
			 */
			if (flags != FLG_RT_HANDLE)
				nmode |= RTLD_PARENT;
		}

		/*
		 * Establish new mode and flags (flags are only appropriate to
		 * initialize an object).
		 */
		MODE(nlmp) |= (nmode & ~RTLD_PARENT);
		FLAGS(nlmp) |= (flags & ~FLG_RT_NOERROR);

	} else {
		if (lml->lm_flags & LML_TRC_VERBOSE) {
			(void) printf(MSG_INTL(MSG_LDD_FIL_FIND), name,
				NAME(clmp));
			if (*name == '/')
				(void) printf(MSG_ORIG(MSG_LDD_FIL_PATH),
					name, MSG_ORIG(MSG_STR_EMPTY));
			else
				(void) printf(MSG_ORIG(MSG_LDD_FIL_EQUIV),
					name, NAME(nlmp),
					MSG_ORIG(MSG_STR_EMPTY));
		}
		update_mode(clmp, nlmp, (nmode & ~RTLD_PARENT));
	}

	/*
	 * If this dependency is associated with a required version insure that
	 * the version is present in the loaded file.
	 */
	if (!(rtld_flags & RT_FL_NOVERSION) && VERNEED(clmp)) {
		if (LM_VERIFY_VERS(clmp)(name, clmp, nlmp) == 0) {
			if ((FLAGS(nlmp) & FLG_RT_ANALYZED) == 0)
				remove_so(lml, nlmp);
			return (0);
		}
	}

	/*
	 * If we've been asked to establish a handle create one for this object.
	 * If not determine if our caller is already associated with a handle,
	 * if so we need to add this object to any handles that already exist.
	 */
	if ((FLAGS(nlmp) | flags) & FLG_RT_HANDLE) {
		FLAGS(nlmp) &= ~FLG_RT_HANDLE;
		if (hdl_create(lml, nlmp, clmp, (nmode & RTLD_PARENT)) == 0) {
			if ((FLAGS(nlmp) & FLG_RT_ANALYZED) == 0)
				remove_so(lml, nlmp);
			return (0);
		}
		return (nlmp);
	}
	if (PERMIT(clmp) == 0)
		return (nlmp);

	/*
	 * Traverse the list of handles that supplied permits to our caller.
	 */
	for (LIST_TRAVERSE(&DONORS(clmp), lnp1, dlp)) {
		Listnode *	lnp2 = 0;
		Rt_map *	lmp2;
		int		skip = 1;

		/*
		 * If the callers permit was established because it was the
		 * parent of a dlopen(RTLD_PARENT) request then we don't want
		 * to consider adding this new object to that handle.
		 */
		for (LIST_TRAVERSE(&dlp->dl_parents, lnp2, lmp2)) {
			if (lmp2 == clmp) {
				break;
			}
		}
		if (lnp2)
			continue;

		/*
		 * Insure that the new object is part of the handle.
		 */
		if (hdl_add(dlp, &dlp->dl_depends, nlmp, clmp) == 0) {
			if ((FLAGS(nlmp) & FLG_RT_ANALYZED) == 0)
				remove_so(lml, nlmp);
			return (0);
		}

		/*
		 * If the object we've added has just been opened, it will not
		 * yet have been processed for its dependencies, these will be
		 * added on later calls to load_one().  Otherwise, this object
		 * already exists, so we add all of its dependencies to the
		 * handle were operating on.
		 */
		if ((FLAGS(nlmp) & FLG_RT_ANALYZED) == 0)
			continue;

		for (LIST_TRAVERSE(&dlp->dl_depends, lnp2, lmp2)) {
			Listnode *	lnp3;
			Rt_map *	lmp3;

			if (nlmp == lmp2)
				skip = 0;
			if (skip)
				continue;

			for (LIST_TRAVERSE(&EDEPENDS(lmp2), lnp3, lmp3)) {
				int	exist;

				exist = hdl_add(dlp, &dlp->dl_depends, lmp3,
				    clmp);
				if (exist == 0)
					return (0);
				if (exist == 1)
					continue;

				update_mode(clmp, lmp3, (nmode & ~RTLD_PARENT));
			}
		}
	}
	return (nlmp);
}

/*
 * If an object specifies direct bindings (it contains a syminfo structure
 * describing where each binding was established during link-editing, and the
 * object was built -Bdirect), then look for the symbol in the specific object.
 */
static Sym *
lookup_sym_direct(Slookup * slp, Rt_map ** dlmp, int flags, uint_t bound,
    Rt_map * lmp, unsigned long hash)
{
	const char *	name = slp->sl_name;
	Rt_map *	clmp = slp->sl_cmap, * ilmp;
	Lm_list *	lml = LIST(clmp);
	Sym *		sym = 0;
	Listnode *	lnp;

	/*
	 * If we need to direct bind to our parent start looking in each caller
	 * link map.  If the caller is the head of its link-map list use it,
	 * otherwise skip it so that we can first look in any interposers.
	 */
	if (bound == SYMINFO_BT_PARENT) {
		for (LIST_TRAVERSE(&CALLERS(clmp), lnp, ilmp)) {
			if (lml->lm_head != ilmp)
				continue;

			sym = SYMINTP(ilmp)(name, ilmp, dlmp, flags, hash);
			if (sym != 0)
				return (sym);
		}
	}

	/*
	 * Check if any interposing symbols satisfy this reference.  Interposers
	 * are defined as preloaded or DF_1_INTERPOSE objects.
	 */
	if (lml->lm_flags & LML_FLG_INTRPOSE) {
		for (ilmp = (Rt_map *)NEXT(lml->lm_head); ilmp;
		    ilmp = (Rt_map *)NEXT(ilmp)) {
			if ((FLAGS(ilmp) & FLG_RT_INTRPOSE) == 0)
				break;

			sym = SYMINTP(ilmp)(name, ilmp, dlmp, flags, hash);
			if (sym != 0) {
				lmp = ilmp;
				break;
			}
		}
	}

	/*
	 * If we haven't found the symbol in any interposers keep looking. If
	 * we need to look in our parent, continue the search after any parent
	 * that's the head of the link-map (already inspected above).
	 */
	if ((sym == 0) && (bound == SYMINFO_BT_PARENT)) {
		for (LIST_TRAVERSE(&CALLERS(clmp), lnp, ilmp)) {
			if (lml->lm_head == ilmp)
				continue;

			if (sym = SYMINTP(ilmp)(name, ilmp, dlmp, flags, hash))
				break;
		}
		if (sym == 0)
			return ((Sym *)0);
		lmp = ilmp;
	}

	/*
	 * If we need to direct bind to anything else we look in the link map
	 * we've just loaded.
	 */
	if ((sym == 0) && (bound != SYMINFO_BT_PARENT)) {
		if (bound == SYMINFO_BT_SELF)
			lmp = clmp;

		if ((sym = SYMINTP(lmp)(name, lmp, dlmp, flags, hash)) == 0)
			return ((Sym *)0);
	}

	/*
	 * If we've bound to a symbol from which a copy relocation has been
	 * taken then we need to assign this binding to the original copy
	 * reference.
	 */
	if (FLAGS(lmp) & FLG_RT_COPYTOOK) {
		Rel_copy *	rcp;

		for (LIST_TRAVERSE(&COPY(lmp), lnp, rcp)) {
			if (sym == rcp->r_dsym) {
				*dlmp = rcp->r_rlmp;
				return (rcp->r_rsym);
			}
		}
	}

	return (sym);
}

/*
 * Symbol lookup routine.  Takes an ELF symbol name, and a list of link maps to
 * search (if the flag indicates LKUP_FIRST only the first link map of the list
 * is searched ie. we've been called from dlsym()).
 * If successful, return a pointer to the symbol table entry and a pointer to
 * the link map of the enclosing object.  Else return a null pointer.
 *
 * To improve elf performance, we first compute the elf hash value and pass
 * it to each find_sym() routine.  The elf function will use this value to
 * locate the symbol, the a.out function will simply ignore it.
 */
Sym *
lookup_sym(Slookup * slp, Rt_map ** dlmp, int flags)
{
	const char *	name = slp->sl_name;
	Permit *	permit = slp->sl_permit;
	Rt_map *	clmp = slp->sl_cmap;
	Rt_map *	ilmp = slp->sl_imap;
	unsigned long	rsymndx = slp->sl_rsymndx;
	unsigned long	hash;
	Sym *		sym = 0;
	Rt_map *	lmp;
	Syminfo *	sip;

	PRF_MCOUNT(56, lookup_sym);

	hash = elf_hash(name);

	/*
	 * Search the initial link map for the required symbol (this category is
	 * selected by dlsym(), where individual link maps are searched for a
	 * required symbol.  Therefore, we know we have permission to look at
	 * the link map).
	 */
	if (flags & LKUP_FIRST)
		return (SYMINTP(ilmp)(name, ilmp, dlmp, flags, hash));

	/*
	 * Determine whether this lookup can be satisfied by an objects direct
	 * or lazy binding information.  This is triggered by a relocation from
	 * the object (hence rsymndx is set).
	 */
	if (rsymndx && (sip = SYMINFO(clmp))) {
		/*
		 * Find the corresponding Syminfo entry for the original
		 * referencing symbol.
		 */
		sip = (Syminfo *)((unsigned long)sip +
			rsymndx * SYMINENT(clmp));

		/*
		 * If the symbol information indicates a direct binding
		 * determine the link map that is required to satisfy the
		 * binding.
		 */
		if (sip->si_flags & SYMINFO_FLG_DIRECT) {
			uint_t	bound = sip->si_boundto;

			if ((bound < SYMINFO_BT_LOWRESERVE) &&
			    (lmp = elf_lazy_load(clmp, bound, name)) == 0)
				return ((Sym *)0);

			/*
			 * If we need to direct bind to anything we look in
			 * ourselves, our parent or in the link map we've just
			 * loaded, otherwise, even though we may have lazily
			 * loaded an object we still continue to search for
			 * symbols from the head of the link map list.
			 */
			if (FLAGS(clmp) & FLG_RT_DIRECT)
				return (lookup_sym_direct(slp, dlmp, flags,
				    bound, lmp, hash));
		}
	}

	/*
	 * Copy relocations should start their search after the head of the
	 * linkmap list.  Since the list may have been null on entry and may
	 * now have additional objects lazily loaded, set the start point here.
	 */
	if (flags & LKUP_COPY)
		ilmp = (Rt_map *)NEXT(LIST(clmp)->lm_head);

	/*
	 * Examine the list of link maps, skipping any whose symbols are denied
	 * to this caller.
	 */
	for (lmp = ilmp; lmp; lmp = (Rt_map *)NEXT(lmp)) {
		if ((lmp == clmp) ||
		    ((((MODE(clmp) & RTLD_WORLD) &&
		    (MODE(lmp) & RTLD_GLOBAL)) ||
		    ((MODE(clmp) & RTLD_GROUP) &&
		    perm_test(PERMIT(lmp), permit))))) {
			if (sym = SYMINTP(lmp)(name, lmp, dlmp, flags, hash))
				break;
		}
	}

	/*
	 * To allow transitioning into a world of lazy loading dependencies see
	 * if this link map contains objects that have lazy dependencies still
	 * outstanding.  If so, and we haven't been able to locate a non-weak
	 * symbol reference, start bringing in any lazy dependencies to see if
	 * the reference can be satisfied.
	 */
	if (sym == 0) {
		if ((flags & LKUP_WEAK) || (LIST(ilmp)->lm_lazy == 0))
			return ((Sym *)0);

		DBG_CALL(Dbg_syms_lazy_rescan(name));

		/*
		 * If this request originated from a dlsym(RTLD_NEXT) then we
		 * need to start looking for dependencies from the callers,
		 * other wise simply use the initial link-map.
		 */
		if (flags & LKUP_NEXT)
			lmp = clmp;
		else
			lmp = ilmp;
		for (; lmp; lmp = (Rt_map *)NEXT(lmp)) {
			if (LAZY(lmp) == 0)
				continue;
			if ((lmp == clmp) ||
			    ((((MODE(clmp) & RTLD_WORLD) &&
			    (MODE(lmp) & RTLD_GLOBAL)) ||
			    ((MODE(clmp) & RTLD_GROUP) &&
			    perm_test(PERMIT(lmp), permit))))) {
				slp->sl_imap = lmp;
				if (sym = elf_lazy_find_sym(slp, dlmp, flags))
					break;
			}
		}
		if (sym == 0)
			return ((Sym *)0);
	}

	/*
	 * If this is an RTLD_GROUP binding and we've bound to a copy relocation
	 * definition then we need to assign this binding to the original copy
	 * reference.
	 */
	if ((MODE(clmp) & RTLD_GROUP) && (FLAGS(*dlmp) & FLG_RT_COPYTOOK)) {
		Listnode *	lnp;
		Rel_copy *	rcp;

		for (LIST_TRAVERSE(&COPY(*dlmp), lnp, rcp)) {
			if (sym == rcp->r_dsym) {
				*dlmp = rcp->r_rlmp;
				return (rcp->r_rsym);
			}
		}
	}
	return (sym);
}

/*
 * Add a explicit (NEEDED) dependency.
 */
static int
add_neededs(Rt_map * clmp, Rt_map * nlmp)
{
	PRF_MCOUNT(97, add_neededs);

	/*
	 * Add the new object to the callers explicit dependency list.
	 */
	if (list_append(&EDEPENDS(clmp), nlmp) == 0)
		return (0);

	/*
	 * Add the caller to the new objects list of callers.
	 */
	if (list_append(&CALLERS(nlmp), clmp) == 0)
		return (0);

	/*
	 * Bump the reference count of the object being bound to.
	 */
	COUNT(nlmp)++;

	return (1);
}

/*
 * Add an implicit binding.
 * To aid diagnostic output distiguish between an object that already exists in
 * one of the dependency lists (2) and a newly added dependency (1).
 */
static int
_add_one_binding(Rt_map * clmp, Rt_map * nlmp)
{
	Rt_map *	tlmp;
	Listnode *	lnp;

	/*
	 * Determine if this object already exists as an explicit
	 * dependency - if so we're done.
	 */
	for (LIST_TRAVERSE(&EDEPENDS(clmp), lnp, tlmp))
		if (tlmp == nlmp)
			return (2);

	/*
	 * Determine if this object has already been previously bound to
	 * and thus exists as an implicit dependency - if so we're done.
	 */
	for (LIST_TRAVERSE(&IDEPENDS(clmp), lnp, tlmp))
		if (tlmp == nlmp)
			return (2);

	/*
	 * Add the new object to the callers implicit dependency list.
	 */
	if (list_append(&IDEPENDS(clmp), nlmp) == 0)
		return (0);

	/*
	 * Add the caller to the new objects list of callers.
	 */
	if (list_append(&CALLERS(nlmp), clmp) == 0)
		return (0);

	/*
	 * Bump the reference count of the object being bound to.
	 */
	COUNT(nlmp)++;

	return (1);
}

/*
 * During relocation processing, one or more symbol bindings may have been
 * established to deletable objects.  If these are not already explicit
 * dependencies, they are added to the implicit dependency list, and their
 * reference count incremented.  These references insure that the bound object
 * doesn't get deleted while this object is using it.
 */
static int
add_many_bindings(Rt_map * clmp)
{
	Rt_map *	nlmp;
	int		ret;

	PRF_MCOUNT(98, add_many_bindings);
	DBG_CALL(Dbg_file_bind_title(REF_SYMBOL));

	/*
	 * Traverse the entire link-map list to determine if the calling object
	 * has bound to any other objects.
	 */
	for (nlmp = LIST(clmp)->lm_head; nlmp; nlmp = (Rt_map *)NEXT(nlmp)) {

		if (!(FLAGS(nlmp) & FLG_RT_BOUND))
			continue;

		FLAGS(nlmp) &= ~FLG_RT_BOUND;

		if ((ret = _add_one_binding(clmp, nlmp)) == 0)
			return (0);

		if (ret == 1)
			DBG_CALL(Dbg_file_bind_entry(clmp, nlmp));
	}
	return (1);
}

/*
 * During .plt relocation a binding may have been established to a non-deletable
 * object.  This is a subset of the add_many_bindings() logic.
 */
static int
add_one_binding(Rt_map * clmp, Rt_map * nlmp)
{
	int	ret;

	PRF_MCOUNT(99, add_one_binding);

	if ((ret = _add_one_binding(clmp, nlmp)) == 0)
		return (0);

	if (ret == 1) {
		DBG_CALL(Dbg_file_bind_title(REF_SYMBOL));
		DBG_CALL(Dbg_file_bind_entry(clmp, nlmp));
	}

	return (1);
}

/*
 * Add a new dependency to the callers dependency list.  This dependency can
 * either be a standard (needed) dependency, or it may be an additional
 * dependency because of a symbol binding (these can only be to promiscuous
 * objects that are not defined as normal dependencies).
 *
 * Maintaining the DEPENDS list allows all referenced objects to have their
 * reference counts reduced should this object be deleted, and provides for
 * building the Dl_handle dependency list for dlopen'ed objects.
 *
 * Returns 1 (binding added/already exists), or 0 (failure).
 */
int
bound_add(int mode, Rt_map * clmp, Rt_map * nlmp)
{
	int	ref, bind = 0;

	PRF_MCOUNT(57, bound_add);

	if (((Sxword)ti_version > 0) &&
	    (bind = bind_guard(THR_FLG_BOUND)))
		(void) rw_wrlock(&boundlock);

	if (mode == REF_SYMBOL) {
		if (nlmp == 0)
			ref = add_many_bindings(clmp);
		else
			ref = add_one_binding(clmp, nlmp);
	} else
		ref = add_neededs(clmp, nlmp);

	if (bind) {
		(void) rw_unlock(&boundlock);
		(void) bind_clear(THR_FLG_BOUND);
	}
	return (ref);
}
