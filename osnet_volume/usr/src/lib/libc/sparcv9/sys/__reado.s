/*	Copyright (c) 1989 by Sun Microsystems, Inc.	*/

.ident	"@(#)__reado.s	1.1	96/12/04 SMI"

/*	int __reado (int fildes, ioreq_t *req, int nreq);	*/

	.file   "__reado.s"

#include "SYS.h"

	SYSREENTRY(__reado)
	SYSTRAP(reado)
	SYSRESTART(.restart___reado)
	RET

	SET_SIZE(__reado)
