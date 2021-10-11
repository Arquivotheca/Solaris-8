/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
.ident	"@(#)install_utrap.s	1.1	95/11/05 SMI"
/*
 * C library -- install_utrap
 * int install_utrap(utrap_entry_t type, utrap_handler_t new_hander,
 *			utrap_handler_t *old_handlerp);
 */

	.file	"install_utrap.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(install_utrap,function)

#include "SYS.h"

	SYSCALL(install_utrap)
	RET

	SET_SIZE(install_utrap)
