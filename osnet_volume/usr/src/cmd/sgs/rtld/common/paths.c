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
#pragma ident	"@(#)paths.c	1.54	99/10/12 SMI"

/*
 * PATH setup and search directory functions.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<limits.h>
#include	<fcntl.h>
#include	<string.h>
#include	<sys/systeminfo.h>
#include	"_rtld.h"
#include	"msg.h"
#include	"conv.h"
#include	"profile.h"
#include	"debug.h"

/*
 * Given a search rule type, return a list of directories to search according
 * to the specified rule.
 */
static Pnode *
get_dir_list(int rules, Rt_map * lmp, int flags)
{
	Pnode *		dirlist = (Pnode *)0;
	Lm_list *	lml = LIST(lmp);
	int		search;

	/*
	 * Determine whether ldd -s is in effect - ignore when we're searching
	 * for audit libraries as these will be added to their own link-map.
	 */
	if ((lml->lm_flags & LML_TRC_SEARCH) && !(flags & FLG_RT_AUDIT))
		search = 1;
	else
		search = 0;

	PRF_MCOUNT(46, get_dir_list);
	switch (rules) {
	case ENVDIRS:
		/*
		 * Initialize the environment variable (LD_LIBRARY_PATH) search
		 * path list.  Note, we always call Dbg_libs_path() so that
		 * every library lookup diagnostic can be preceded with the
		 * appropriate search path information.
		 */
		if (envdirs) {
			DBG_CALL(Dbg_libs_path(envdirs));

			/*
			 * For ldd(1) -s, indicate the search paths that'll
			 * be used.  If this is a secure program then some
			 * search paths may be ignored, therefore reset the
			 * envlist pointer each time so that the diagnostics
			 * related to these unsecure directories will be
			 * output for each image loaded.
			 */
			if (search)
				(void) printf(MSG_INTL(MSG_LDD_PTH_ENVDIR),
				    envdirs);
			if (envlist && (rtld_flags & RT_FL_SECURE) &&
			    (search || dbg_mask)) {
				free(envlist);
				envlist = 0;
			}
			if (!envlist) {
				/*
				 * If this is a secure application we need to
				 * to be selective over what LD_LIBRARY_PATH
				 * directories we use.  Pass the list of
				 * trusted directories so that the appropriate
				 * security check can be carried out.
				 */
				envlist = make_pnode_list(envdirs,
				    LA_SER_LIBPATH, 1,
				    LM_SECURE_DIRS(LIST(lmp)->lm_head), lmp);
			}
			dirlist = envlist;
		}
		break;
	case RUNDIRS:
		/*
		 * Initialize the runpath search path list.  To be consistent
		 * with the debugging display of ENVDIRS (above), always call
		 * Dbg_libs_rpath().
		 */
		if (RPATH(lmp)) {
			DBG_CALL(Dbg_libs_rpath(NAME(lmp), RPATH(lmp)));

			/*
			 * For ldd(1) -s, indicate the search paths that'll
			 * be used.  If this is a secure program then some
			 * search paths may be ignored, therefore reset the
			 * runlist pointer each time so that the diagnostics
			 * related to these unsecure directories will be
			 * output for each image loaded.
			 */
			if (search)
				(void) printf(MSG_INTL(MSG_LDD_PTH_RPATH),
				    RPATH(lmp), NAME(lmp));
			if (RLIST(lmp) && (rtld_flags & RT_FL_SECURE) &&
			    (search || dbg_mask)) {
				free(RLIST(lmp));
				RLIST(lmp) = 0;
			}
			if (!(RLIST(lmp)))
				RLIST(lmp) = make_pnode_list(RPATH(lmp),
				    LA_SER_RUNPATH, 1, 0, lmp);
			dirlist = RLIST(lmp);
		}
		break;
	case DEFAULT:
		if ((FLAGS1(lmp) & FL1_RT_NODEFLIB) == 0) {
			if ((rtld_flags & RT_FL_SECURE) &&
			    (flags & (FLG_RT_PRELOAD | FLG_RT_AUDIT)))
				dirlist = LM_SECURE_DIRS(lmp);
			else
				dirlist = LM_DFLT_DIRS(lmp);
		}

		/*
		 * For ldd(1) -s, indicate the default paths that'll be used.
		 */
		if (dirlist && (search || dbg_mask)) {
			Pnode *	pnp = dirlist;
			int	num = 0;

			if (search)
				(void) printf(MSG_INTL(MSG_LDD_PTH_BGNDFL));
			for (; pnp && pnp->p_name; pnp = pnp->p_next, num++) {
				if (search) {
					const char *	fmt;

					if (num)
					    fmt = MSG_ORIG(MSG_LDD_FMT_PATHN);
					else
					    fmt = MSG_ORIG(MSG_LDD_FMT_PATH1);
					(void) printf(fmt, pnp->p_name);
				} else
					DBG_CALL(Dbg_libs_dpath(pnp->p_name));
			}
			if (search) {
				if (dirlist->p_orig & LA_SER_CONFIG)
				    (void) printf(MSG_INTL(MSG_LDD_PTH_ENDDFLC),
					config->c_name);
				else
				    (void) printf(MSG_INTL(MSG_LDD_PTH_ENDDFL));
			}
		}
		break;
	default:
		break;
	}
	return (dirlist);
}

/*
 * Get the next dir in the search rules path.
 */
Pnode *
get_next_dir(Pnode ** dirlist, Rt_map * lmp, int flag)
{
	static int *	rules = NULL;

	PRF_MCOUNT(45, get_next_dir);
	/*
	 * Search rules consist of one or more directories names. If this is a
	 * new search, then start at the beginning of the search rules.
	 * Otherwise traverse the list of directories that make up the rule.
	 */
	if (!*dirlist) {
		rules = LM_SEARCH_RULES(lmp);
	} else {
		if ((*dirlist = (*dirlist)->p_next) != 0)
			return (*dirlist);
		else
			rules++;
	}

	while (*rules) {
		if ((*dirlist = get_dir_list(*rules, lmp, flag)) != 0)
			return (*dirlist);
		else
			rules++;
	}

	/*
	 * If we got here, no more directories to search, return NULL.
	 */
	return ((Pnode *) NULL);
}


/*
 * Process a directory (runpath) or filename (needed or filter) string looking
 * for tokens to expand.  The `new' flag indicates whether a new string should
 * be created.  This is typical for multiple runpath strings, as these are
 * colon separated and must be null separated to facilitate the creation of
 * a search path list.  Note that if token expansion occurs a new string must be
 * produced.
 */
int
expand(char ** name, size_t * len, char ** list, Rt_map * lmp)
{
	char	_name[PATH_MAX];
	char *	optr, * _optr, * nptr, * _list;
	size_t	olen = 0, nlen = 0, nrem = PATH_MAX, _len;
	int	expanded = 0, _expanded, isaflag = 0, origin = 0;

	PRF_MCOUNT(47, expand);

	optr = _optr = *name;
	nptr = _name;

	while ((olen < *len) && nrem) {
		if ((*optr != '$') || ((olen - *len) == 1)) {
			olen++, optr++, nrem--;
			continue;
		}

		/*
		 * Copy any string we've presently passed over to the new
		 * buffer.
		 */
		if ((_len = (optr - _optr)) != 0) {
			(void) strncpy(nptr, _optr, _len);
			nptr = nptr + _len;
			nlen += _len;
		}

		/*
		 * Skip the token delimiter and determine if a reserved token
		 * match is found.
		 */
		olen++, optr++;
		_expanded = 0;

		if (strncmp(optr, MSG_ORIG(MSG_TKN_ORIGIN),
		    MSG_TKN_ORIGIN_SIZE) == 0) {
			origin = 1;

			/*
			 * $ORIGIN expansion is required.  Determine this
			 * objects basename.  As hardlinks can fool $ORIGIN
			 * disallow for secure applications.
			 */
			if (!(rtld_flags & RT_FL_SECURE) &&
			    (((_len = DIRSZ(lmp)) != 0) ||
			    ((_len = fullpath(lmp)) != 0))) {
				if (nrem > _len) {
					(void) strncpy(nptr,
					    PATHNAME(lmp), _len);
					nptr = nptr +_len;
					nlen += _len;
					nrem -= _len;
					olen += MSG_TKN_ORIGIN_SIZE;
					optr += MSG_TKN_ORIGIN_SIZE;
					expanded = _expanded = 1;
				}
			}

		} else if (strncmp(optr, MSG_ORIG(MSG_TKN_PLATFORM),
		    MSG_TKN_PLATFORM_SIZE) == 0) {
			/*
			 * $PLATFORM expansion required.  This would have been
			 * established from the AT_SUN_PLATFORM aux vector, but
			 * if not attempt to get it from sysconf().
			 */
			if ((platform == 0) && (platform_sz == 0)) {
				char	_info[SYS_NMLN];
				long	_size;

				_size = sysinfo(SI_PLATFORM, _info, SYS_NMLN);
				if ((_size != -1) &&
				    ((platform = malloc((size_t)_size)) != 0)) {
					(void) strcpy(platform, _info);
					platform_sz = (size_t)_size - 1;
				} else
					platform_sz = 1;
			}
			if ((platform != 0) && (nrem > platform_sz)) {
				(void) strncpy(nptr, platform, platform_sz);
				nptr = nptr + platform_sz;
				nlen += platform_sz;
				nrem -= platform_sz;
				olen += MSG_TKN_PLATFORM_SIZE;
				optr += MSG_TKN_PLATFORM_SIZE;
				_expanded = 1;
			}

		} else if (strncmp(optr, MSG_ORIG(MSG_TKN_OSNAME),
		    MSG_TKN_OSNAME_SIZE) == 0) {
			/*
			 * $OSNAME expansion required.  This is established
			 * from the sysname[] returned by uname(2).
			 */
			if (uts == 0)
				uts = conv_uts();

			if (uts && uts->uts_osnamesz &&
			    (nrem > uts->uts_osnamesz)) {
				(void) strncpy(nptr, uts->uts_osname,
				    uts->uts_osnamesz);
				nptr = nptr + uts->uts_osnamesz;
				nlen += uts->uts_osnamesz;
				nrem -= uts->uts_osnamesz;
				olen += MSG_TKN_OSNAME_SIZE;
				optr += MSG_TKN_OSNAME_SIZE;
				_expanded = 1;
			}

		} else if (strncmp(optr, MSG_ORIG(MSG_TKN_OSREL),
		    MSG_TKN_OSREL_SIZE) == 0) {
			/*
			 * $OSREL expansion required.  This is established
			 * from the release[] returned by uname(2).
			 */
			if (uts == 0)
				uts = conv_uts();

			if (uts && uts->uts_osrelsz &&
			    (nrem > uts->uts_osrelsz)) {
				(void) strncpy(nptr, uts->uts_osrel,
				    uts->uts_osrelsz);
				nptr = nptr + uts->uts_osrelsz;
				nlen += uts->uts_osrelsz;
				nrem -= uts->uts_osrelsz;
				olen += MSG_TKN_OSREL_SIZE;
				optr += MSG_TKN_OSREL_SIZE;
				_expanded = 1;
			}

		} else if ((strncmp(optr, MSG_ORIG(MSG_TKN_ISALIST),
		    MSG_TKN_ISALIST_SIZE) == 0) && (list != 0) &&
		    (isaflag++ == 0)) {
			/*
			 * $ISALIST expansion required.
			 */
			if (isa == 0)
				isa = conv_isalist();

			if (isa && isa->isa_listsz &&
			    (nrem > isa->isa_opt->isa_namesz)) {
				size_t		no, mlen, tlen, hlen = olen - 1;
				char *		lptr;
				Isa_opt *	opt = isa->isa_opt;

				(void) strncpy(nptr,  opt->isa_name,
				    opt->isa_namesz);
				nptr = nptr + opt->isa_namesz;
				nlen += opt->isa_namesz;
				nrem -= opt->isa_namesz;
				olen += MSG_TKN_ISALIST_SIZE;
				optr += MSG_TKN_ISALIST_SIZE;
				_expanded = 1;

				tlen = *len - olen;

				/*
				 * As ISALIST expands to a number of elements,
				 * establish a new list to return to the caller.
				 * This will contain the present path being
				 * processed redefined for each isalist option,
				 * plus the original remaining list entries.
				 */
				mlen = ((hlen + tlen) * (isa->isa_optno - 1)) +
				    isa->isa_listsz - opt->isa_namesz +
				    strlen(*list);
				if ((_list = lptr = malloc(mlen)) == 0)
					return (0);

				for (no = 1, opt++; no < isa->isa_optno;
				    no++, opt++) {
					(void) strncpy(lptr, *name, hlen);
					lptr = lptr + hlen;
					(void) strncpy(lptr, opt->isa_name,
					    opt->isa_namesz);
					lptr = lptr + opt->isa_namesz;
					(void) strncpy(lptr, optr, tlen);
					lptr = lptr + tlen;
					*lptr++ = ':';
				}
				if (**list)
					(void) strcpy(lptr, *list);
				else
					*--lptr = '\0';
			}
		}

		/*
		 * No reserved token has been found, or its expansion isn't
		 * possible.  Replace the token delimiter.
		 */
		if (_expanded == 0) {
			*nptr++ = '$';
			nlen++, nrem--;
		}
		_optr = optr;
	}

	/*
	 * If any ISALIST processing has occurred not only do we return the
	 * expanded node we're presently working on, but we must also update the
	 * remaining list so that it is effectively prepended with this node
	 * expanded to all remaining isalist options.  Note that we can only
	 * handle one ISALIST per node.  For more than one ISALIST to be
	 * processed we'd need a better algorithm than above to replace the
	 * newly generated list.  Whether we want to encourage the number of
	 * pathname permutations this would provide is another question. So, for
	 * now if more than one ISALIST is encountered we return the original
	 * node untouched.
	 */
	if (isaflag) {
		if (isaflag == 1)
			*list = _list;
		else {
			if ((nptr = calloc(1, *len + 1)) == 0)
				return (0);
			(void) strncpy(nptr, *name, *len);
			*name = nptr;

			return (1);
		}
	}

	/*
	 * Copy any remaining string. Terminate the new string with a null as
	 * this string can be displayed via debugging diagnostics.
	 */
	if (((_len = (optr - _optr)) != 0) && (nrem > _len)) {
		(void) strncpy(nptr, _optr, _len);
		nptr = nptr + _len;
		nlen += _len;
	}
	*nptr = '\0';

	/*
	 * If any $ORIGIN string expansion has occurred, resolve the new path to
	 * simplify and remove any symlinks. If the resolve doesn't work we'll
	 * use the string as is (the resolvepath(2) trap was added in 2.6).
	 */
	if (expanded && (rtld_flags & RT_FL_EXECNAME)) {
		/* LINTED */
		if ((int)(_len = resolvepath(_name, _name, PATH_MAX)) >= 0) {
			nlen = _len;
			_name[nlen] = '\0';
		}
	}

	/*
	 * Allocate permanent storage for the new string and return to the user.
	 */
	if ((nptr = malloc(nlen + 1)) == 0)
		return (0);
	(void) strcpy(nptr, _name);
	*name = nptr;
	*len = nlen;

	/*
	 * Special return value for $ORIGIN.  Under security this value isn't
	 * expanded, in fact it would still be dangerous to use the string as
	 * is (for fear of someone creating a $ORIGIN directory).  The calling
	 * program can use a return of 2 to check whether the path should be
	 * used at all.
	 */
	return (origin ? 2 : 1);
}

/*
 * Take a colon separated file or path specification and build a list of Pnode
 * structures.  Each string is passed to expand() for possible token expansion.
 * All string nodes are maintained in allocated memory (regardless of whether
 * they are constant (":"), or token expanded) to simplify pnode removal.
 */
Pnode *
make_pnode_list(const char * list, Half orig, int secure, Pnode * sdir,
    Rt_map * clmp)
{
	size_t	len;
	char *	str, * olist = 0, * nlist = (char *)list;
	Pnode *	pnp, * npnp, * opnp;

	PRF_MCOUNT(48, make_pnode_list);

	for (pnp = opnp = 0, str = nlist; *nlist; str = nlist) {
		if (*nlist == ';')
			++nlist;
		if (*nlist == ':') {
			if ((str = strdup(MSG_ORIG(MSG_FMT_CWD))) == NULL)
				return ((Pnode *)0);
			len = MSG_FMT_CWD_SIZE;

			if (*nlist)
				nlist++;
		} else {
			char *	elist;
			int	exp;

			len = 0;
			while (*nlist && (*nlist != ':') && (*nlist != ';')) {
				nlist++, len++;
			}

			if (*nlist)
				nlist++;

			/*
			 * Expand the captured string.  Besides expanding the
			 * present path/file entry, we may have a new list to
			 * deal with (ISALIST expands to multiple new entries).
			 */
			elist = nlist;
			if ((exp = expand(&str, &len, &elist, clmp)) == 0)
				return ((Pnode *)0);
			if ((exp == 2) && (rtld_flags & RT_FL_SECURE)) {
				DBG_CALL(Dbg_libs_ignore(str));
				if (LIST(clmp)->lm_flags & LML_TRC_SEARCH)
				    (void) printf(MSG_INTL(MSG_LDD_PTH_IGNORE),
						str);
				free((void *)str);
				continue;
			}

			if (elist != nlist) {
				if (olist)
					free(olist);
				nlist = olist = elist;
			}
		}

		/*
		 * If we're only allowed to recognize secure paths make sure
		 * that the path just processed is valid.
		 */
		if (secure && (rtld_flags & RT_FL_SECURE)) {
			Pnode *		_sdir;
			int		ok = 0;

			if (*str == '/') {
				if (sdir) {
					for (_sdir = sdir;
					    (_sdir && _sdir->p_name);
					    _sdir = _sdir->p_next) {
						if (strcmp(str,
						    _sdir->p_name) == 0) {
							ok = 1;
							break;
						}
					}
				} else
					ok = 1;
			}
			if (!ok) {
				DBG_CALL(Dbg_libs_ignore(str));
				if (LIST(clmp)->lm_flags & LML_TRC_SEARCH)
				    (void) printf(MSG_INTL(MSG_LDD_PTH_IGNORE),
						str);
				free((void *)str);
				continue;
			}
		}

		/*
		 * Allocate a new Pnode for this string.
		 */
		if ((npnp = malloc(sizeof (Pnode))) == 0)
			return ((Pnode *)0);
		if (opnp == 0)
			pnp = npnp;
		else
			opnp->p_next = npnp;

		npnp->p_name = str;
		npnp->p_len = len;
		npnp->p_orig = orig;
		npnp->p_info = 0;
		npnp->p_next = 0;

		opnp = npnp;
	}

	if (olist)
		free(olist);

	return (pnp);
}
