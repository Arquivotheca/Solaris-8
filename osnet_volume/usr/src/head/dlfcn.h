/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 *	Copyright (c) 1989 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 */

#ifndef _DLFCN_H
#define	_DLFCN_H

#pragma ident	"@(#)dlfcn.h	1.31	99/09/14 SMI"	/* SVr4.0 1.2	*/

#include <sys/types.h>


#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Information structure returned by dladdr().
 */
#if !defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__)
#ifdef __STDC__
typedef struct	dl_info {
	const char *	dli_fname;	/* file containing address range */
	void *		dli_fbase;	/* base address of file image */
	const char *	dli_sname;	/* symbol name */
	void *		dli_saddr;	/* symbol address */
} Dl_info;
#else
typedef struct	dl_info {
	char *		dli_fname;
	void *		dli_fbase;
	char *		dli_sname;
	void *		dli_saddr;
} Dl_info;
#endif /* __STDC__ */
#endif /* !defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__) */


typedef ulong_t		Lmid_t;

/*
 * Declarations used for dynamic linking support routines.
 */
#ifdef __STDC__
extern void *		dlopen(const char *, int);
extern void *		dlsym(void *, const char *);
extern int		dlclose(void *);
extern char *		dlerror(void);
#if !defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__)
extern void *		dlmopen(Lmid_t, const char *, int);
extern int		dladdr(void *, Dl_info *);
extern int		dldump(const char *, const char *, int);
extern int		dlinfo(void *, int, void *);
#endif /* !defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__) */
#else
extern void *		dlopen();
extern void *		dlmopen();
extern int		dlinfo();
extern void *		dlsym();
extern int		dlclose();
extern char *		dlerror();
#if !defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__)
extern void *		dlmopen();
extern int		dladdr();
extern int		dldump();
#endif /* !defined(_XOPEN_SOURCE) || defined(__EXTENSIONS__) */
#endif /* __STDC__ */

/*
 * Valid values for handle argument to dlsym(3x).
 */
#define	RTLD_NEXT		(void *)-1	/* look in `next' dependency */
#define	RTLD_DEFAULT		(void *)-2	/* look up symbol from scope */
						/*	of current object */

/*
 * Valid values for mode argument to dlopen.
 */
#define	RTLD_LAZY		0x00001		/* deferred function binding */
#define	RTLD_NOW		0x00002		/* immediate function binding */
#define	RTLD_NOLOAD		0x00004		/* don't load object */

#define	RTLD_GLOBAL		0x00100		/* export symbols to others */
#define	RTLD_LOCAL		0x00000		/* symbols are only available */
						/*	to group members */
#define	RTLD_PARENT		0x00200		/* add parent (caller) to */
						/*	a group dependencies */
#define	RTLD_GROUP		0x00400		/* resolve symbols within */
						/*	members of the group */
#define	RTLD_WORLD		0x00800		/* resolve symbols within */
						/*	global objects */
#define	RTLD_NODELETE		0x01000		/* do not remove members */
#define	RTLD_CONFGEN		0x10000		/* crle(1) config generation */
						/*	internal use only */

/*
 * Valid values for flag argument to dldump.
 */
#define	RTLD_REL_RELATIVE	0x00001		/* apply relative relocs */
#define	RTLD_REL_EXEC		0x00002		/* apply symbolic relocs that */
						/*	bind to main */
#define	RTLD_REL_DEPENDS	0x00004		/* apply symbolic relocs that */
						/*	bind to dependencies */
#define	RTLD_REL_PRELOAD	0x00008		/* apply symbolic relocs that */
						/*	bind to preload objs */
#define	RTLD_REL_SELF		0x00010		/* apply symbolic relocs that */
						/*	bind to ourself */
#define	RTLD_REL_WEAK		0x00020		/* apply symbolic weak relocs */
						/*	even if unresolved */
#define	RTLD_REL_ALL		0x00fff 	/* apply all relocs */

#define	RTLD_MEMORY		0x01000		/* use memory sections */
#define	RTLD_STRIP		0x02000		/* retain allocable sections */
						/*	only */
#define	RTLD_NOHEAP		0x04000		/* do no save any heap */
#define	RTLD_CONFSET		0x10000		/* crle(1) config generation */
						/*	internal use only */

/*
 * Arguments for dlinfo()
 */
#define	RTLD_DI_LMID		1		/* obtain link-map id */
#define	RTLD_DI_LINKMAP		2		/* obtain link-map */
#define	RTLD_DI_CONFIGADDR	3		/* obtain config addr */
#define	RTLD_DI_MAX		3


#ifdef	__cplusplus
}
#endif

#endif	/* _DLFCN_H */
