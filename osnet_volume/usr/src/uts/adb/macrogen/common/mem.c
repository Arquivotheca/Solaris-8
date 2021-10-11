#ident	"@(#)mem.c	1.1	97/08/07 SMI"

/*
 *		Copyright (C) 1995  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

#include <stddef.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include "mem.h"

/*
 * These routines are versions of malloc, realloc, and calloc that call an
 * installed handler if they get an error, allowing the error-handling to
 * be done just once instead of after every memory allocation.
 *
 * Routine set_alloc_err_func() returns the current error handler and
 * installs a new one.
 */

extern char *alloc_func_object = NULL;

/* This is the function to call on alloc failure, if non-NULL. */
static alloc_err_func_t alloc_err_func = NULL;

/* Initial calls to malloc/realloc go to these. */
static void *get_and_call_malloc(size_t);
static void *get_and_call_realloc(void *, size_t);

/* These are pointers to the real malloc and realloc. */
typedef void *(* malloc_func_t)(size_t);
typedef void *(*realloc_func_t)(void *, size_t);
static  malloc_func_t real_malloc  = get_and_call_malloc;
static realloc_func_t real_realloc = get_and_call_realloc;

/*
 * do_dlsym() looks first in RTLD_NEXT for the specified symbol.
 * If this code was linked into the main program, that will cause
 * us to pick it up from libc.so (as opposed to getting our own
 * function again, which would cause an infinite recursion).
 * But if this code was dlopen()ed, then RTLD_NEXT is not going to
 * find libc.so; we have to dlopen() the executable (and things
 * loaded with it, like libc.so) and look there.
 */
static void *
do_dlsym(char *name)
{
	char *problem;
	void *dl_handle;
	void *addr;

	/*
	 * If we weren't given the name of a dynamic object to look in,
	 * look first in RTLD_NEXT (assume we weren't dlopen()ed).
	 */
	if (!alloc_func_object) {
		addr = dlsym(RTLD_NEXT, name);
		if (addr)
			return (addr);
	}

	/* Get a handle for the dynamic object (default = executable). */
	dl_handle = dlopen(alloc_func_object, RTLD_NOW);
	if (dl_handle) {
		addr = dlsym(dl_handle, name);
		if (addr) {
			dlclose(dl_handle);
			return (addr);
		}
	}

	/* Couldn't get it; record the problem and abort. */
	problem = dlerror();
	abort();

	/*NOTREACHED*/
}

/*
 * get_alloc_func_ptrs() gets the pointers to the real malloc() and realloc()
 * routines.  It just aborts if it can't set things up.
 */
static void
get_alloc_func_ptrs()
{
	real_malloc  =  (malloc_func_t) do_dlsym("malloc");
	real_realloc = (realloc_func_t) do_dlsym("realloc");

	/* Make sure we didn't just get pointers to our own routines. */
	if (real_malloc == &malloc || real_realloc == &realloc)
		abort();
}

/*
 * These are the routines we call the first time we call malloc and realloc;
 * they first find the real routines, and then call them.  Doing this allows
 * us to avoid checking to see whether we've done the dlsym in our interposed
 * malloc and realloc (thus making them faster).
 */
static void *
get_and_call_malloc(
	size_t	size)
{
	get_alloc_func_ptrs();
	return (real_malloc(size));
}
static void *
get_and_call_realloc(
	void	*ptr,
	size_t	 size)
{
	get_alloc_func_ptrs();
	return (real_realloc(ptr, size));
}

/*
 * set_alloc_err_func() sets the pointer to the function to call if we
 * can't allocate memory.
 */
alloc_err_func_t
set_alloc_err_func(alloc_err_func_t new)
{
	alloc_err_func_t old = alloc_err_func;
	alloc_err_func = new;
	return (old);
}

/*
 * This is the interposed malloc().
 */
void *
malloc(size_t size)
{
	void *p;

	/*
	 * Call the real malloc; if it fails, call the error routine if there
	 * is one; if it returns true, then try again.
	 */
	do {
		p = (*real_malloc)(size);
		if (p)
			return (p);
	} while (alloc_err_func && (*alloc_err_func)(size));

	/* Either there was no err func, or it returned false.  Fail. */
	return NULL;
}

/*
 * This is the interposed realloc().
 */
void *
realloc(void *old, size_t size)
{
	void *p;

	/*
	 * Call the real realloc; if it fails, call the error routine if there
	 * is one; if it returns true, then try again.
	 */
	do {
		p = (*real_realloc)(old, size);
		if (p)
			return (p);
	} while (alloc_err_func && (*alloc_err_func)(size));

	/* Either there was no err func, or it returned false.  Fail. */
	return NULL;
}

