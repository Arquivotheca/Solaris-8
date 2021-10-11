/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getbase.c	1.7	98/08/28 SMI" 	/* SVr4.0 1.6	*/

#pragma weak	elf_getbase = _elf_getbase


#include "syn.h"
#include "libelf.h"
#include "decl.h"


off_t
elf_getbase(Elf * elf)
{
	off_t	rc;
	if (elf == 0)
		return (-1);
	ELFRLOCK(elf)
	rc = elf->ed_baseoff;
	ELFUNLOCK(elf)
	return (rc);
}
