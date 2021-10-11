/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)open.s	1.5	98/05/04 SMI"

	.file	"open.s"

#include "SYS.h"

/*
 * C library -- open
 * int open(const char *path, int oflag, [ mode_t mode ] )
 */

	ENTRY(__open);
	SYSTRAP(open);
	SYSCERROR

	RET

	SET_SIZE(__open)
