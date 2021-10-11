/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getarhdr.c	1.9	98/08/28 SMI" 	/* SVr4.0 1.7	*/

#pragma weak	elf_getarhdr = _elf_getarhdr


#include <ar.h>
#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "member.h"
#include "msg.h"


Elf_Arhdr *
elf_getarhdr(Elf * elf)
{
	Member *	mh;
	Elf_Arhdr *	rc;

	if (elf == 0)
		return (0);
	ELFRLOCK(elf)
	if ((mh = elf->ed_armem) == 0) {
		ELFUNLOCK(elf)
		_elf_seterr(EREQ_AR, 0);
		return (0);
	}
	if (mh->m_err != 0)
		/*LINTED*/
		_elf_seterr((Msg)mh->m_err, 0);
	rc = &elf->ed_armem->m_hdr;
	ELFUNLOCK(elf)
	return (rc);
}
