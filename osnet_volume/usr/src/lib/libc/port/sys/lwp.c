/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)lwp.c	1.21	99/09/13 SMI"

/*LINTLIBRARY*/

#pragma weak _lwp_mutex_lock = __lwp_mutex_lock
#pragma weak _lwp_mutex_trylock = __lwp_mutex_trylock
#pragma weak _lwp_sema_init = __lwp_sema_init

#include "synonyms.h"
#include "mtlib.h"
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <synch.h>
#include <sys/synch32.h>

#if defined(__i386)
/*
 * On x86, we must avoid invoking the dynamic linker at this point
 * because we might have just deallocated our curthread selector
 * register (%gs) and we would malfunction if we called back into
 * libthread via an invocation of the dynamic linker.  To avoid
 * the dynamic linker, we call these non-global functions rather
 * than the normal exported functions.
 */
extern int _private_lock_try(unsigned char *);
extern int _private___lwp_mutex_lock(mutex_t *);

#pragma weak _private_lwp_mutex_lock = _lwp_mutex_lock
int
_lwp_mutex_lock(mutex_t *mp)
{
	if (!_private_lock_try(&mp->mutex_lockw))
		return (_private___lwp_mutex_lock(mp));
	return (0);
}

#else	/* __i386 */

extern int _lock_try(unsigned char *);
extern int ___lwp_mutex_lock(mutex_t *);

int
_lwp_mutex_lock(mutex_t *mp)
{
	if (!_lock_try(&mp->mutex_lockw))
		return (___lwp_mutex_lock(mp));
	return (0);
}

#endif	/* __i386 */

int
_lwp_mutex_trylock(mutex_t *mp)
{
	if (_lock_try(&mp->mutex_lockw)) {
		return (0);
	}
	return (EBUSY);
}

int
_lwp_sema_init(lwp_sema_t *sp, int count)
{
	sp->sema_count = count;
	sp->sema_waiters = 0;
	sp->type = USYNC_PROCESS;
	return (0);
}

void
_halt(void)
{
	/*LINTED*/
	while (1)
		continue;
}
