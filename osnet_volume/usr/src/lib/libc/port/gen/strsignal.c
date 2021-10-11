/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma	ident	"@(#)strsignal.c	1.6	96/10/15 SMI"

/*LINTLIBRARY*/

#pragma weak strsignal = _strsignal

#include "synonyms.h"
#include "_libc_gettext.h"
#include <string.h>
#include <stddef.h>
#include <sys/types.h>

extern const char	**_sys_siglistp;
extern const int	_sys_siglistn;

char *
_strsignal(int signum)
{
	if (signum < _sys_siglistn && signum >= 0)
		return (_libc_gettext((char *)_sys_siglistp[signum]));
	return (NULL);
}
