/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rand.c	1.8	98/08/28 SMI" 	/* SVr4.0 1.2	*/

#pragma weak	elf_rand = _elf_rand


#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"

size_t
elf_rand(Elf * elf, size_t off)
{
	if (elf == 0)
		return (0);
	ELFWLOCK(elf)
	if (elf->ed_kind != ELF_K_AR) {
		_elf_seterr(EREQ_AR, 0);
		ELFUNLOCK(elf)
		return (0);
	}
	if ((off == 0) || (elf->ed_fsz < off)) {
		_elf_seterr(EREQ_RAND, 0);
		ELFUNLOCK(elf)
		return (0);
	}
	elf->ed_nextoff = off;
	ELFUNLOCK(elf)
	return (off);
}
