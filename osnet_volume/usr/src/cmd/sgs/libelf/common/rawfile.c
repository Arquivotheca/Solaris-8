/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)rawfile.c	1.7	98/08/28 SMI" 	/* SVr4.0 1.4	*/

#pragma weak	elf_rawfile = _elf_rawfile


#include "syn.h"
#include "libelf.h"
#include "decl.h"


char *
elf_rawfile(Elf * elf, size_t * ptr)
{
	register size_t	sz;
	char		*p = 0;

	if (elf == 0) {
		if (ptr != 0)
			*ptr = 0;
		return (0);
	}

	ELFWLOCK(elf)
	if ((sz = elf->ed_fsz) == 0) {
		if (ptr != 0)
			*ptr = 0;
		ELFUNLOCK(elf)
		return (0);
	}

	if (elf->ed_raw != 0)
		p = elf->ed_raw;
	else if (elf->ed_status == ES_COOKED) {
		if ((p = _elf_read(elf->ed_fd, elf->ed_baseoff, sz)) != 0) {
			elf->ed_raw = p;
			elf->ed_myflags |= EDF_RAWALLOC;
		} else
			sz = 0;
	} else {
		p = elf->ed_raw = elf->ed_ident;
		elf->ed_status = ES_FROZEN;
		if (_elf_vm(elf, (size_t)0, elf->ed_fsz) != OK_YES) {
			p = 0;
			sz = 0;
		}
	}
	if (ptr != 0)
		*ptr = sz;
	ELFUNLOCK(elf)
	return (p);
}
