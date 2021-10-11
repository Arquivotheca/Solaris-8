/*	Copyright (c) 1989 by Sun Microsystems, Inc.	*/

.ident	"@(#)__reado.s	1.3	92/07/14 SMI"

/*	int __reado (int fildes, ioreq_t *req, int nreq);	*/

	.file   "__reado.s"

#include "SYS.h"

	SYSREENTRY(__reado)
	SYSTRAP(reado)
	SYSRESTART(.restart___reado)
	RET

	SET_SIZE(__reado)
