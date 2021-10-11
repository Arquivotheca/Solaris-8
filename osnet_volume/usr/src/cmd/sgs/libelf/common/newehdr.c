/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)newehdr.c	1.14	99/05/04 SMI" 	/* SVr4.0 1.4	*/

#include "syn.h"
#include <stdlib.h>
#include <errno.h>
#include "decl.h"
#include "msg.h"

/*
 * This module is compiled twice, the second time having
 * -D_ELF64 defined.  The following set of macros, along
 * with machelf.h, represent the differences between the
 * two compilations.  Be careful *not* to add any class-
 * dependent code (anything that has elf32 or elf64 in the
 * name) to this code without hiding it behind a switch-
 * able macro like these.
 */
#if	defined(_ELF64)
#define	ELFCLASS		ELFCLASS64
#define	_elf_ehdr_init		_elf64_ehdr_init
#define	elf_newehdr		elf64_newehdr
#define	getehdr			elf64_getehdr

#else	/* else ELF32 */

#pragma weak	elf32_newehdr = _elf32_newehdr

#define	ELFCLASS		ELFCLASS32
#define	_elf_ehdr_init		_elf32_ehdr_init
#define	elf_newehdr		_elf32_newehdr
#define	getehdr			elf32_getehdr

#endif	/* ELF64 */



Ehdr *
elf_newehdr(Elf * elf)
{
	register
	Ehdr	*eh;

	if (elf == 0)
		return (0);

	/*
	 * If reading file, return its hdr
	 */

	ELFWLOCK(elf)
	if (elf->ed_myflags & EDF_READ) {
		ELFUNLOCK(elf)
		if ((eh = (Ehdr *)getehdr(elf)) != 0) {
			ELFWLOCK(elf)
			elf->ed_ehflags |= ELF_F_DIRTY;
			ELFUNLOCK(elf)
		}
		return (eh);
	}

	/*
	 * Writing file
	 */

	if (elf->ed_class == ELFCLASSNONE)
		elf->ed_class = ELFCLASS;
	else if (elf->ed_class != ELFCLASS) {
		_elf_seterr(EREQ_CLASS, 0);
		ELFUNLOCK(elf)
		return (0);
	}
	ELFUNLOCK(elf);
	if ((eh = (Ehdr *)getehdr(elf)) != 0) {	/* this cooks if necessary */
		ELFWLOCK(elf)
		elf->ed_ehflags |= ELF_F_DIRTY;
		ELFUNLOCK(elf)
		return (eh);
	}
	ELFWLOCK(elf)

	if ((eh = (Ehdr *)malloc(sizeof (Ehdr))) == 0) {
		_elf_seterr(EMEM_EHDR, errno);
		ELFUNLOCK(elf)
		return (0);
	}
	*eh = _elf_ehdr_init;
	elf->ed_myflags |= EDF_EHALLOC;
	elf->ed_ehflags |= ELF_F_DIRTY;
	elf->ed_ehdr = eh;
	ELFUNLOCK(elf)
	return (eh);
}
