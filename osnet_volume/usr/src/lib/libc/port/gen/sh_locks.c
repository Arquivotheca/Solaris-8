/*	Copyright (c) 1992 SMI	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sh_locks.c	1.6	97/12/16 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

#include <mtlib.h>
#include <sys/types.h>
#include <synch.h>
#include <thread.h>

/* These locks are for internal library usage only  */

/* Lock for sbrk/brk */
mutex_t __sbrk_lock = DEFAULTMUTEX;

/* lock for malloc() */
mutex_t __malloc_lock = DEFAULTMUTEX;

/* lock for utmp/utmpx */
mutex_t __utx_lock = DEFAULTMUTEX;

/*
 * fork1-saftey.
 * These three routines are used to make libc interfaces fork1-safe and are
 * private private between libc and libthread.
 */

void
_libc_prepare_atfork(void)
{
	(void) _mutex_lock(&__malloc_lock);
}

void
_libc_child_atfork(void)
{
	(void) _mutex_unlock(&__malloc_lock);
}

void
_libc_parent_atfork(void)
{
	(void) _mutex_unlock(&__malloc_lock);
}
