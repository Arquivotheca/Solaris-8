/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getscn.c	1.8	98/08/28 SMI" 	/* SVr4.0 1.10	*/

#pragma weak	elf_getscn = _elf_getscn


#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"


Elf_Scn *
elf_getscn(Elf * elf, size_t index)
{
	Elf_Scn	*	s;
	Elf_Scn	*	prev_s;
	size_t		j = index;
	size_t		tabsz;

	if (elf == 0)
		return (0);

	ELFRLOCK(elf)
	tabsz = elf->ed_scntabsz;
	if (elf->ed_hdscn == 0) {
		ELFUNLOCK(elf)
		ELFWLOCK(elf)
		if ((elf->ed_hdscn == 0) && (_elf_cook(elf) != OK_YES)) {
			ELFUNLOCK(elf);
			return (0);
		}
		ELFUNLOCK(elf);
		ELFRLOCK(elf)
	}
	/*
	 * If the section in question is part of a table allocated
	 * from within _elf_prescn() then we can index straight
	 * to it.
	 */
	if (index < tabsz) {
		s = &elf->ed_hdscn[index];
		ELFUNLOCK(elf);
		return (s);
	}

#ifndef	__lock_lint
	if (tabsz)
		s = &elf->ed_hdscn[tabsz - 1];
	else
		s = elf->ed_hdscn;

	for (prev_s = 0; s != 0; prev_s = s, s = s->s_next) {
		if (prev_s) {
			SCNUNLOCK(prev_s)
		}
		SCNLOCK(s)
		if (j == 0) {
			if (s->s_index == index) {
				SCNUNLOCK(s)
				ELFUNLOCK(elf);
				return (s);
			}
			_elf_seterr(EBUG_SCNLIST, 0);
			SCNUNLOCK(s)
			ELFUNLOCK(elf)
			return (0);
		}
		--j;
	}
	if (prev_s) {
		SCNUNLOCK(prev_s)
	}
#endif
	_elf_seterr(EREQ_NDX, 0);
	ELFUNLOCK(elf);
	return (0);
}
