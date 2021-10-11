/*
 *	Copyright (c) 1996 Sun Microsystems Inc.
 *	All rights reserved.
 */

#ident	"@(#)_pset.s	1.1	96/05/20 SMI"

	.file	"_pset.s"

#include "SYS.h"

/*
 * int
 * _pset(int subcode, long arg1, long arg2, long arg3, long arg4)
 *
 * Syscall entry point for pset_create, pset_assign, pset_destroy,
 * pset_bind, and pset_info.
 */
	ENTRY(_pset)
	SYSTRAP(pset)
	SYSCERROR
	RET
	SET_SIZE(_pset)
