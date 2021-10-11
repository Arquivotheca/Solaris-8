/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getident.c	1.7	98/08/28 SMI" 	/* SVr4.0 1.8	*/

#pragma weak	elf_getident = _elf_getident


#include "syn.h"
#include "libelf.h"
#include "decl.h"


char *
elf_getident(Elf * elf, size_t * ptr)
{
	size_t	sz = 0;
	char *	id = 0;

	if (elf != 0) {
		ELFRLOCK(elf)
		if (elf->ed_identsz != 0) {
			if ((elf->ed_vm == 0) || (elf->ed_status !=
			    ES_COOKED)) {
				/*
				 * We need to upgrade to a Writers
				 * lock
				 */
				ELFUNLOCK(elf)
				ELFWLOCK(elf)
				if ((_elf_cook(elf) == OK_YES) &&
				    (_elf_vm(elf, (size_t)0,
				    elf->ed_identsz) == OK_YES)) {
					id = elf->ed_ident;
					sz = elf->ed_identsz;
				}
			} else {
				id = elf->ed_ident;
				sz = elf->ed_identsz;
			}
		}
		ELFUNLOCK(elf)
	}
	if (ptr != 0)
		*ptr = sz;
	return (id);
}
