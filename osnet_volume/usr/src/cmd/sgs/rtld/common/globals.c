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
#pragma ident	"@(#)globals.c	1.58	99/10/12 SMI"

#include	<sys/types.h>
#include	<sys/mman.h>
#include	<signal.h>
#include	<dlfcn.h>
#include	<synch.h>
#include	"_rtld.h"

/*
 * Declarations of global variables used in ld.so.
 */
int		bind_mode =	RTLD_LAZY;
rwlock_t	bindlock =	DEFAULTRWLOCK;
rwlock_t	initlock =	DEFAULTRWLOCK;
rwlock_t	malloclock =	DEFAULTRWLOCK;
rwlock_t	printlock =	DEFAULTRWLOCK;
rwlock_t	boundlock =	DEFAULTRWLOCK;

/*
 * Major link-map lists.
 */
Lm_list		lml_main =	{ 0 };		/* the `main's link map list */
Lm_list		lml_rtld =	{ 0 };		/* rtld's link map list */
List		dynlm_list =	{ 0, 0 };	/* dynamic list of link-maps */

Reglist *	reglist = 0;			/* list of register symbols */

struct r_debug r_debug = {
	R_DEBUG_VERSION,			/* version no. */
	0,					/* r_map */
	(unsigned long)rtld_db_dlactivity,	/* r_brk */
	RT_CONSISTENT,				/* r_state */
	0,					/* r_ldbase */
	0,					/* r_ldsomap */
	RD_NONE,				/* r_rdevent */
	RD_FL_NONE				/* r_flags */
};

/*
 * Private structure for passing of information between librltd_db
 * and rtld.
 *
 * Note:  Any data that's being 'exported' to librtld_db must not
 *	  require any 'relocations' before it can be examined.  That's
 *	  because librtld_db will examine this structure before rtld has
 *	  started to execute (and before it's relocated itself).  So - all
 *	  data in this structure must be available at that point.
 */
Rtld_db_priv	rtld_db_priv = {
	R_RTLDDB_VERSION,		/* rtd_version */
	0,				/* rtd_objpad */
	0				/* rtd_dynlmlst */
};

/*
 * Initialized fmap structure.
 */
static Fmap	_fmap = {
	-1,				/* fm_fd */
	0,				/* fm_maddr */
	0,				/* fm_msize */
	MAP_PRIVATE,			/* fm_mflags */
	0,				/* fm_fsize */
	0				/* fm_etext */
};

Fmap *		fmap = &_fmap;		/* Initial file mapping info */

const char *	pr_name;
Sxword		ti_version = 0;		/* version of thread interface */
const char *	rt_name;		/* the run time linkers name */
char *		lasterr = (char *)0;	/* string describing last error */
					/*	cleared by each dlerror() */
Interp *	interp = 0;		/* ELF interpreter info */
const char *	preload_objs;		/* LD_PRELOAD objects */
const char *	envdirs = 0;		/* LD_LIBRARY_PATH and its */
Pnode *		envlist = 0;		/*	associated Pnode list */
List		hdl_list[HDLISTSZ + 1];	/* dlopen() handle list */
size_t		syspagsz = 0;		/* system page size */
unsigned long	flags = 0;		/* machine specific file flags */
char *		platform = 0;		/* platform name from AT_SUN_PLATFORM */
size_t		platform_sz = 0;	/* platform string length */
Uts_desc *	uts;			/* utsname descriptor */
Isa_desc *	isa;			/* isalist descriptor */

const char *	audit_objs = 0;		/* LD_AUDIT objects */
uint_t		audit_argcnt = 64;	/* no. of stack args to copy (default */
					/*	is all) */
Audit_desc *	auditors = 0;		/* global auditors (LD_AUDIT) */

int		rtld_flags = 0;		/* status flags for RTLD */

Rtc_head *	cachehead = 0;		/* head of the cache structure */
const char *	cd_dir = 0;		/* Cache directory */
const char *	cd_file = 0;		/* Cache diagnostic file. */

const char *	locale = 0;		/* locale environment definition */

const char *	dbg_str = 0;		/* debugging tokens */
int		dbg_mask;		/* debugging classes */
const char * 	dbg_file = 0;		/* debugging directed to file */

#pragma weak	environ = _environ	/* environ for PLT tracing - we */
char **		_environ = 0;		/* supply the pair to satisfy any */
					/* libc requirements (hwmuldiv) */

#ifdef	PRF_RTLD
int		profile_rtld = 0;	/* Indicate rtld is being profiled */
					/*	for LD_MCOUNT test */
const char *	profile_name;
#endif
