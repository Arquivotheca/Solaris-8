/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getshdr.c	1.10	98/08/28 SMI" 	/* SVr4.0 1.4	*/

#pragma weak	elf32_getshdr = _elf32_getshdr

#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"


static void *
getshdr(Elf_Scn * scn, int class)
{
	void *	rc;
	Elf *	elf;
	if (scn == 0)
		return (0);
	elf = scn->s_elf;
	READLOCKS(elf, scn)
	if (elf->ed_class != class) {
		READUNLOCKS(elf, scn)
		_elf_seterr(EREQ_CLASS, 0);
		return (0);
	}

	rc = scn->s_shdr;
	READUNLOCKS(elf, scn)
	return (rc);
}

Elf32_Shdr *
elf32_getshdr(Elf_Scn * scn)
{
	return ((Elf32_Shdr*) getshdr(scn, ELFCLASS32));
}

Elf64_Shdr *
elf64_getshdr(Elf_Scn * scn)
{
	return ((Elf64_Shdr*) getshdr(scn, ELFCLASS64));
}
