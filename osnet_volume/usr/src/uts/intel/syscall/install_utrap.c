/*
 *	Copyright (c) 1995,1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)install_utrap.c	1.2	99/05/04 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/utrap.h>

/* ARGSUSED */
int
install_utrap(utrap_entry_t type,
		utrap_handler_t new_handler,
		utrap_handler_t *old_handlerp)
{
	return ((int)set_errno(ENOSYS));
}
