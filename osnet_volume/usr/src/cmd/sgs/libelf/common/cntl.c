/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cntl.c	1.8	98/08/28 SMI" 	/* SVr4.0 1.4	*/

#pragma weak	elf_cntl = _elf_cntl


#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"


int
elf_cntl(Elf * elf, Elf_Cmd cmd)
{

	if (elf == 0)
		return (0);
	ELFWLOCK(elf);
	switch (cmd) {
	case ELF_C_FDREAD:
	{
		int	j = 0;

		if ((elf->ed_myflags & EDF_READ) == 0) {
			_elf_seterr(EREQ_CNTLWRT, 0);
			ELFUNLOCK(elf);
			return (-1);
		}
		if ((elf->ed_status != ES_FROZEN) &&
		    ((_elf_cook(elf) != OK_YES) ||
		    (_elf_vm(elf, (size_t)0, elf->ed_fsz) != OK_YES)))
			j = -1;
		elf->ed_fd = -1;
		ELFUNLOCK(elf);
		return (j);
	}

	case ELF_C_FDDONE:
		if ((elf->ed_myflags & EDF_READ) == 0) {
			_elf_seterr(EREQ_CNTLWRT, 0);
			ELFUNLOCK(elf);
			return (-1);
		}
		elf->ed_fd = -1;
		ELFUNLOCK(elf);
		return (0);

	default:
		_elf_seterr(EREQ_CNTLCMD, 0);
		break;
	}
	ELFUNLOCK(elf);
	return (-1);
}
