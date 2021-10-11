/*	Copyright (c) (1995-1996) Sun Microsystems Inc */
/*	All Rights Reserved. */

#pragma ident  "@(#)thr_init.c 1.3	96/11/13 SMI"

/*LINTLIBRARY*/
#include <sys/types.h>
#include <mtlib.h>
#include <thread.h>

/*
 * set __threaded variable. perf improvement for stdio.
 * If libthread gets linked in or is dlopened it calls
 * _libc_set_threaded to set __threaded to 1.
 */

/* CSTYLED */
#pragma init (_check_threaded)

int __threaded;

void
_check_threaded(void)
{
	if (_thr_main() == -1)
		__threaded = 0;
	else
		__threaded = 1;
}

void
_libc_set_threaded(void)
{
	__threaded = 1;
}

void
_libc_unset_threaded(void)
{
	__threaded = 0;
}
