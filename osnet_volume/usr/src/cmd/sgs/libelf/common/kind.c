/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kind.c	1.7	98/08/28 SMI" 	/* SVr4.0 1.4	*/

#pragma weak	elf_kind = _elf_kind


#include "syn.h"
#include "libelf.h"
#include "decl.h"


Elf_Kind
elf_kind(Elf * elf)
{
	Elf_Kind	rc;
	if (elf == 0)
		return (ELF_K_NONE);
	ELFRLOCK(elf);
	rc = elf->ed_kind;
	ELFUNLOCK(elf);
	return (rc);
}
