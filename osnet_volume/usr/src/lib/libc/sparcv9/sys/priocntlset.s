/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*       Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)priocntlset.s	1.1	96/12/04 SMI"	/* SVr4.0 1.1	*/

/* C library -- priocntlset					*/
/* long priocntlset(procset_t psp, int cmd, int arg)		*/

	.file	"priocntlset.s"

#include "SYS.h"

	ENTRY(__priocntlset)
	SYSTRAP(priocntlsys)
	SYSCERROR
	RET

	SET_SIZE(__priocntlset)
