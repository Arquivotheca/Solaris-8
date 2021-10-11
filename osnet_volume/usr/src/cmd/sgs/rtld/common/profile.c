/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)profile.c	1.28	99/06/23 SMI"

/*
 * Routines to provide profiling of ld.so itself, or any shared libraries
 * required by the called executable.
 */

#ifdef	PRF_RTLD

#include	"_synonyms.h"
#include	"_rtld.h"
#include	"profile.h"
#include	"msg.h"

uintptr_t (*	p_cg_interp)(int, caddr_t, caddr_t);

static int (*	p_open)(const char *, Link_map *);

int
profile_setup(Link_map * lm)
{
	Dl_handle *	dlp;
#if	defined(_ELF64) && defined(__sparcv9)
	const char *	path = MSG_ORIG(MSG_FIL_SP64_LDPROF);
#elif	defined(_ELF64) && defined(__ia64)
	const char *	path = MSG_ORIG(MSG_FIL_IA64_LDPROF);
#else
	const char *	path = MSG_ORIG(MSG_FIL_LDPROF);
#endif

	if ((dlp = dlmopen_core((Lm_list *)LM_ID_LDSO, path,
	    (RTLD_NOW | RTLD_GROUP | RTLD_WORLD), lml_rtld.lm_head,
	    FLG_RT_AUDIT)) == 0)
		return (0);

	if ((p_open = (int (*)(const char *, Link_map *))
	    dlsym_core(dlp, MSG_ORIG(MSG_SYM_PROFOPEN),
	    lml_rtld.lm_head)) == 0)
		return (0);

	if ((p_cg_interp = (uintptr_t(*)(int, caddr_t, caddr_t))
	    dlsym_core(dlp, MSG_ORIG(MSG_SYM_PROFCGINTRP),
	    lml_rtld.lm_head)) == 0)
		return (0);

	return (p_open(MSG_ORIG(MSG_FIL_RTLD), lm));
}

#endif
