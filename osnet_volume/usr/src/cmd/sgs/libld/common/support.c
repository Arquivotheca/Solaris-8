/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)support.c	1.12	98/08/28 SMI"

#include	<stdio.h>
#include	<dlfcn.h>
#include	<libelf.h>
#include	<link.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"



/*
 * Table which defines the default functions to be called by the library
 * SUPPORT (-S <libname>).  These functions can be redefined by the
 * ld_support_loadso() routine.
 */
static Support_list support[] = {
#ifdef _ELF64
	MSG_ORIG(MSG_SUP_START_64),	{ 0, 0 },	/* LD_START64 */
	MSG_ORIG(MSG_SUP_ATEXIT_64),	{ 0, 0 },	/* LD_ATEXIT64 */
	MSG_ORIG(MSG_SUP_FILE_64),	{ 0, 0 },	/* LD_FILE64 */
	MSG_ORIG(MSG_SUP_SECTION_64),	{ 0, 0 },	/* LD_SECTION64 */
#else  /* Elf32 */
	MSG_ORIG(MSG_SUP_START),	{ 0, 0 },	/* LD_START */
	MSG_ORIG(MSG_SUP_ATEXIT),	{ 0, 0 },	/* LD_ATEXIT */
	MSG_ORIG(MSG_SUP_FILE),		{ 0, 0 },	/* LD_FILE */
	MSG_ORIG(MSG_SUP_SECTION),	{ 0, 0 },	/* LD_SECTION */
#endif
	NULL,				{ 0, 0 },
};

/*
 * Loads in a support shared object specified using the SGS_SUPPORT environment
 * variable or the -S ld option, and determines which interface functions are
 * provided by that object.
 *
 * return values for ld_support_loadso:
 *	0 -	unable to open shared object
 *	1 -	shared object loaded sucessfully
 *	S_ERROR - aww, damn!
 */
uintptr_t
ld_support_loadso(const char * obj)
{
	void *		handle;
	void (*		fptr)();
	Func_list *	flp;
	int 		i;

	/*
	 * Load the required support library.  If we are unable to load it fail
	 * silently. (this might be changed in the future).
	 */
	if ((handle = dlopen(obj, RTLD_LAZY)) == NULL)
		return (0);

	for (i = 0; support[i].sup_name; i++) {
		if ((fptr = (void (*)())dlsym(handle, support[i].sup_name))) {

			if ((flp = (Func_list *)
			    libld_malloc(sizeof (Func_list))) == NULL)
				return (S_ERROR);

			flp->fl_obj = obj;
			flp->fl_fptr = fptr;
			if (list_appendc(&support[i].sup_funcs, flp) == 0)
				return (S_ERROR);

			DBG_CALL(Dbg_support_load(obj, support[i].sup_name));
		}
	}
	return (1);
}


/*
 * Wrapper routines for the ld support library calls.
 */
void
lds_start(const char * ofile, const Half etype, const char * caller)
{
	Func_list *	flp;
	Listnode *	lnp;

	for (LIST_TRAVERSE(&support[LD_START].sup_funcs, lnp, flp)) {
		DBG_CALL(Dbg_support_action(flp->fl_obj,
		    support[LD_START].sup_name, DBG_SUP_START, ofile));
		(*flp->fl_fptr)(ofile, etype, caller);
	}
}


void
lds_atexit(int exit_code)
{
	Func_list *	flp;
	Listnode *	lnp;

	for (LIST_TRAVERSE(&support[LD_ATEXIT].sup_funcs, lnp, flp)) {
		DBG_CALL(Dbg_support_action(flp->fl_obj,
		    support[LD_ATEXIT].sup_name, DBG_SUP_ATEXIT, 0));
		(*flp->fl_fptr)(exit_code);
	}
}


void
lds_file(const char * ifile, const Elf_Kind ekind, int flags, Elf * elf)
{
	Func_list *	flp;
	Listnode *	lnp;

	for (LIST_TRAVERSE(&support[LD_FILE].sup_funcs, lnp, flp)) {
		int	_flags = 0;

		if (!(flags & FLG_IF_CMDLINE))
			_flags |= LD_SUP_DERIVED;
		if (!(flags & FLG_IF_NEEDED))
			_flags |= LD_SUP_INHERITED;
		if (flags & FLG_IF_EXTRACT)
			_flags |= LD_SUP_EXTRACTED;

		DBG_CALL(Dbg_support_action(flp->fl_obj,
		    support[LD_FILE].sup_name, DBG_SUP_FILE, ifile));
		(*flp->fl_fptr)(ifile, ekind, _flags, elf);
	}
}


void
lds_section(const char * scn, Shdr * shdr, Word ndx,
    Elf_Data * data, Elf * elf)
{
	Func_list *	flp;
	Listnode *	lnp;

	for (LIST_TRAVERSE(&support[LD_SECTION].sup_funcs, lnp, flp)) {
		DBG_CALL(Dbg_support_action(flp->fl_obj,
		    support[LD_SECTION].sup_name, DBG_SUP_SECTION, scn));
		(*flp->fl_fptr)(scn, shdr, ndx, data, elf);
	}
}
