/*	Copyright (c) 1989 by Sun Microsystems, Inc.	*/

.ident	"@(#)__writeo.s	1.3	92/07/14 SMI"	/* SVr4.0 1.9	*/

/*	int __writeo (int fildes, ioreq_t *req, int nreq);	*/

	.file   "__writeo.s"

#include "SYS.h"

	SYSREENTRY(__writeo)
	SYSTRAP(writeo)
	SYSRESTART(.restart___writeo)
	RET

	SET_SIZE(__writeo)
