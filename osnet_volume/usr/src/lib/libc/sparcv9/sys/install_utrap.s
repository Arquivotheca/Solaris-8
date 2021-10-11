/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
.ident	"@(#)install_utrap.s	1.2	97/08/21 SMI"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(install_utrap,function)

#include "SYS.h"

	.file	"install_utrap.s"

/*
 * int install_utrap(utrap_entry_t type, utrap_handler_t new_hander,
 *			utrap_handler_t *old_handlerp)
 */
	SYSCALL(install_utrap)
	RET

	SET_SIZE(install_utrap)

/*
 * int
 * __sparc_utrap_install(utrap_entry_t type,
 *       utrap_handler_t new_precise, utrap_handler_t new_deferred,
 *       utrap_handler_t *old_precise, utrap_handler_t *old_deferred)
 */
	ENTRY(__sparc_utrap_install)
	SYSTRAP(sparc_utrap_install)
	SYSCERROR
	RET
	SET_SIZE(__sparc_utrap_install)
