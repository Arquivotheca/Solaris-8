/*
 *	Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)_xregs_clrptr.c	1.3	96/12/20 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <ucontext.h>
#include "libc.h"

/*
 * clear the struct ucontext extra register state pointer
 */
void
_xregs_clrptr(ucontext_t *uc)
{
	uc->uc_mcontext.xrs.xrs_id = 0;
	uc->uc_mcontext.xrs.xrs_ptr = NULL;
}
