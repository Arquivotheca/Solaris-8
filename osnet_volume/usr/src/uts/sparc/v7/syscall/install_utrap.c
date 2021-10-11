/*
 *	Copyright (c) 1995 Sun Microsystems, Inc.
 *	All rights reserved.
 */
#ident	"@(#)install_utrap.c	1.1	95/11/05 SMI"

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
	return ((int) set_errno(EINVAL));
}
