/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)newphdr.c	1.14	99/05/04 SMI" 	/* SVr4.0 1.3	*/

#include "syn.h"
#include <stdlib.h>
#include <memory.h>
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
#define	ELFCLASS	ELFCLASS64
#define	elf_newphdr	elf64_newphdr
#define	elf_getehdr	elf64_getehdr
#define	_elf_msize	_elf64_msize
#define	elf_fsize	elf64_fsize

#else	/* else ELF32 */
#define	ELFCLASS	ELFCLASS32
#pragma weak		elf32_newphdr = _elf32_newphdr
#define	elf_newphdr	elf32_newphdr
#define	elf_getehdr	elf32_getehdr
#define	_elf_msize	_elf32_msize
#define	elf_fsize	elf32_fsize

#endif /* ELF64 */



Phdr *
elf_newphdr(Elf * elf, size_t count)
{
	Elf_Void *	ph;
	size_t		sz;
	Phdr *		rc;
	unsigned	work;

	if (elf == 0)
		return (0);
	ELFRLOCK(elf)
	if (elf->ed_class != ELFCLASS) {
		_elf_seterr(EREQ_CLASS, 0);
		ELFUNLOCK(elf)
		return (0);
	}
	ELFUNLOCK(elf)
	if (elf_getehdr(elf) == 0) {		/* this cooks if necessary */
		_elf_seterr(ESEQ_EHDR, 0);
		return (0);
	}

	/*
	 * Free the existing header if appropriate.  This could reuse
	 * existing space if big enough, but that's unlikely, benefit
	 * would be negligible, and code would be more complicated.
	 */

	ELFWLOCK(elf)
	if (elf->ed_myflags & EDF_PHALLOC) {
		elf->ed_myflags &= ~EDF_PHALLOC;
		rc = elf->ed_phdr;
		free(rc);
	}

	/*
	 * Delete the header if count is zero.
	 */

	ELFACCESSDATA(work, _elf_work)
	if ((sz = count * _elf_msize(ELF_T_PHDR, work)) == 0) {
		elf->ed_phflags &= ~ELF_F_DIRTY;
		elf->ed_phdr = 0;
		((Ehdr*)elf->ed_ehdr)->e_phnum = 0;
		((Ehdr*)elf->ed_ehdr)->e_phentsize = 0;
		elf->ed_phdrsz = 0;
		ELFUNLOCK(elf)
		return (0);
	}

	if ((ph = malloc(sz)) == 0) {
		_elf_seterr(EMEM_PHDR, errno);
		elf->ed_phflags &= ~ELF_F_DIRTY;
		elf->ed_phdr = 0;
		((Ehdr*)elf->ed_ehdr)->e_phnum = 0;
		((Ehdr*)elf->ed_ehdr)->e_phentsize = 0;
		elf->ed_phdrsz = 0;
		ELFUNLOCK(elf)
		return (0);
	}

	elf->ed_myflags |= EDF_PHALLOC;
	(void) memset(ph, 0, sz);
	elf->ed_phflags |= ELF_F_DIRTY;
	/* LINTED */
	((Ehdr*)elf->ed_ehdr)->e_phnum = (Half)count;
	((Ehdr*)elf->ed_ehdr)->e_phentsize
	    /* LINTED */
	    = (Half)elf_fsize(ELF_T_PHDR, 1, work);
	elf->ed_phdrsz = sz;
	elf->ed_phdr = rc = ph;

	ELFUNLOCK(elf)
	return (rc);
}
