/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)ndxscn.c	1.7	98/08/28 SMI" 	/* SVr4.0 1.2	*/

#pragma weak	elf_ndxscn = _elf_ndxscn


#include "syn.h"
#include "libelf.h"
#include "decl.h"


size_t
elf_ndxscn(Elf_Scn * scn)
{
	size_t	rc;
	Elf *	elf;

	if (scn == 0)
		return (SHN_UNDEF);
	elf = scn->s_elf;
	READLOCKS(elf, scn)
	rc = scn->s_index;
	READUNLOCKS(elf, scn)
	return (rc);
}
