/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dl.c	1.17	99/10/07 SMI"

/* LINTLIBRARY */

/*
 * Stub library for programmer's interface to the dynamic linker.  Used
 * to satisfy ld processing, and serves as a precedence place-holder at
 * execution-time.  These routines are never actually called.
 */
#include <dlfcn.h>
#include <link.h>

int	dbg_mask;

#pragma weak dlinfo = _dlinfo
int
/* ARGSUSED */
_dlinfo(void * handle, int request, void * p)
{
	return (0);
}

#pragma weak dlmap = _dlmap
void *
/* ARGSUSED */
_dlmap(unsigned int lm_id, const char * path, int mode)
{
	return ((void *)0);
}

#pragma	weak	dlmopen = _dlmopen
/* ARGSUSED */
void *
_dlmopen(Lmid_t * lmid, const char * pathname, int mode)
{
	return ((void *)0);
}

#pragma	weak	dlopen = _dlopen
/* ARGSUSED */
void *
_dlopen(const char * pathname, int mode)
{
	return ((void *)0);
}

#pragma	weak	dlsym = _dlsym
/* ARGSUSED */
void *
_dlsym(void * handle, const char * name)
{
	return ((void *)0);
}

#pragma	weak	dlclose = _dlclose
/* ARGSUSED */
int
_dlclose(void * handle)
{
	return (0);
}

#pragma	weak	dlerror = _dlerror
char *
_dlerror()
{
	return ((char *)0);
}

#pragma	weak	dladdr = _dladdr
/* ARGSUSED */
int
_dladdr(void * addr, Dl_info * dlip)
{
	return (0);
}

#pragma	weak	dldump = _dldump
/* ARGSUSED */
int
_dldump(const char * ipath, const char * opath, int flags)
{
	return (0);
}

/* ARGSUSED */
void
_ld_concurrency(void * funcs)
{
}

/* ARGSUSED */
void
_ld_libc(void * funcs)
{
}
