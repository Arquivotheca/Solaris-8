/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)nextscn.c	1.7	98/08/28 SMI" 	/* SVr4.0 1.2	*/

#pragma weak	elf_nextscn = _elf_nextscn

#include "syn.h"
#include "libelf.h"
#include "decl.h"


Elf_Scn *
elf_nextscn(Elf * elf, Elf_Scn * scn)
{
	Elf_Scn *	ret_scn = 0;

	if (elf == 0)
		return (0);
	if (scn != 0) {
		READLOCKS(elf, scn)
		ret_scn = scn->s_next;
		READUNLOCKS(elf, scn)
	} else {
		ELFWLOCK(elf)
		if (elf->ed_hdscn == 0) {
			if (elf->ed_hdscn == 0)
				(void) _elf_cook(elf);
		}
		if ((scn = elf->ed_hdscn) != 0)
			ret_scn = scn->s_next;
		ELFUNLOCK(elf)
	}
	return (ret_scn);
}
