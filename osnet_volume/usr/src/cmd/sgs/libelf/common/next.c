/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)next.c	1.7	98/08/28 SMI" 	/* SVr4.0 1.5	*/

#pragma weak	elf_next = _elf_next


#include "syn.h"
#include "libelf.h"
#include "decl.h"


Elf_Cmd
elf_next(Elf * elf)
{
	Elf	*parent;

	if (elf == 0)
		return (ELF_C_NULL);
	ELFRLOCK(elf)
	if ((parent = elf->ed_parent) == 0) {
		ELFUNLOCK(elf);
		return (ELF_C_NULL);
	}
	ELFWLOCK(parent)
	if (elf->ed_siboff >= parent->ed_fsz) {
		ELFUNLOCK(parent)
		ELFUNLOCK(elf);
		return (ELF_C_NULL);
	}

	parent->ed_nextoff = elf->ed_siboff;
	ELFUNLOCK(parent)
	ELFUNLOCK(elf);
	return (ELF_C_READ);
}
