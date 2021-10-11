/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)getehdr.c	1.9	98/08/28 SMI" 	/* SVr4.0 1.8	*/


#pragma weak	elf32_getehdr = _elf32_getehdr


#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"


static void*
getehdr(Elf * elf, int class)
{
	void *	rc;
	if (elf == 0)
		return (0);
	ELFWLOCK(elf);
	if (elf->ed_class != class) {
		_elf_seterr(EREQ_CLASS, 0);
		ELFUNLOCK(elf);
		return (0);
	}
	if (elf->ed_ehdr == 0)
		(void) _elf_cook(elf);

	rc = elf->ed_ehdr;
	ELFUNLOCK(elf);

	return (rc);
}


Elf32_Ehdr *
elf32_getehdr(Elf * elf)
{
	return ((Elf32_Ehdr*) getehdr(elf, ELFCLASS32));
}


Elf64_Ehdr *
elf64_getehdr(Elf * elf)
{
	return ((Elf64_Ehdr*) getehdr(elf, ELFCLASS64));
}
