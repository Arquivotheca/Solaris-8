/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)symbindrep.c	1.6	97/11/23 SMI"


#include <stdio.h>
#include <stdlib.h>
#include <link.h>

#include "env.h"

static Elist *		bindto_list = 0;
static Elist *		bindfrom_list = 0;
static FILE *		output = stdout;


uint_t
la_version(uint_t version)
{
	if (version < LAV_CURRENT) {
		(void) fprintf(stderr,
			"symbindrep.so: unexpected version: %d\n",
			version);
		return (0);
	}

	build_env_list(&bindto_list, (const char *)"SYMBINDREP_BINDTO");
	build_env_list(&bindfrom_list, (const char *)"SYMBINDREP_BINDFROM");

	(void) fprintf(output,
#ifdef _LP64
	"                            Symbol Bindings\n\n"
	"Referencing                  Defining\n"
	"Object                       Object                       Symbol\n"
	/* CSTYLED */
	"----------------------------------------------------------------------------------\n");
#else
	"                    Symbol Bindings\n\n"
	"Referencing          Defining\n"
	"Object               Object               Symbol\n"
	"------------------------------------------------------------------\n");
#endif
	return (LAV_CURRENT);
}


/* ARGSUSED1 */
uint_t
la_objopen(Link_map * lmp, Lmid_t lmid, uintptr_t * cookie)
{
	uint_t		flags;

	if ((bindto_list == 0) ||
	    (check_list(bindto_list, lmp->l_name)))
		flags = LA_FLG_BINDTO;
	else
		flags = 0;

	if ((bindfrom_list == 0) ||
	    (check_list(bindfrom_list, lmp->l_name)))
		flags |= LA_FLG_BINDFROM;

	*cookie = (uintptr_t)lmp->l_name;
	return (flags);
}


/* ARGSUSED1 */
#if	defined(_LP64)
uintptr_t
la_symbind64(Elf64_Sym * symp, uint_t symndx, uintptr_t * refcook,
	uintptr_t * defcook, uint_t * sb_flags, const char * sym_name)
#else
uintptr_t
la_symbind32(Elf32_Sym * symp, uint_t symndx, uintptr_t * refcook,
	uintptr_t * defcook, uint_t * sb_flags)
#endif
{
#if	!defined(_LP64)
	const char *	sym_name = (const char *)symp->st_name;
#endif

	(void) fprintf(output, "%-28s %-28s %s\n", (char *)(*refcook),
		(char *)(*defcook), sym_name);

	return (symp->st_value);
}

/*
 * Since we only want to report on the symbol bindings for this
 * process and we *do not* want the actuall program to run we exit
 * at this point.
 */
/* ARGSUSED0 */
void
la_preinit(uintptr_t * cookie)
{
	(void) fflush(output);
	exit(0);
}
