/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)link.s 1.4     97/04/02 SMI"

/*
 * C library -- link
 * int link(const char *path1, const char *path2);
 */

	.file	"link.s"

#include <sys/asm_linkage.h>

#include "SYS.h"

	ENTRY(__link)
	SYSTRAP(link)
	SYSCERROR
	RETC
	SET_SIZE(__link)
