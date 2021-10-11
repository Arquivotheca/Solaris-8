/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)private.c	1.1	99/05/14 SMI"

#define	_REENTRANT

#include <sys/types.h>
#include <stdio.h>
#include <malloc.h>
#include <synch.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <apptrace.h>

/*
 * Local/private implementations of libc functions so to deal with the
 * breakage associated with re-entrancy and the additional complexties
 * introduced by being on a link map other than the base.
 *
 * See comments in apptrace.c for more info.
 */

mutex_t abi_stdio_mutex = DEFAULTMUTEX;
static mutex_t abi_malloc_mutex = DEFAULTMUTEX;

int
abi_fprintf(FILE *stream, const char *format, ...)
{
	int ret;
	sigset_t omask;
	va_list ap;

	(*abi_sigsetmask)(SIG_BLOCK, &abisigset, &omask);
	(*abi_mutex_lock)(&abi_stdio_mutex);

	va_start(ap, format);
	ret = vfprintf(stream, format, ap);
	va_end(ap);

	(*abi_mutex_unlock)(&abi_stdio_mutex);
	(*abi_sigsetmask)(SIG_SETMASK, &omask, NULL);

	return (ret);
}

int
abi_putc(int ch, FILE *stream)
{
	int ret;
	sigset_t omask;

	(*abi_sigsetmask)(SIG_BLOCK, &abisigset, &omask);
	(*abi_mutex_lock)(&abi_stdio_mutex);

	ret = putc(ch, stream);

	(*abi_mutex_unlock)(&abi_stdio_mutex);
	(*abi_sigsetmask)(SIG_SETMASK, &omask, NULL);

	return (ret);
}

int
abi_fputs(const char *ptr, FILE *stream)
{
	int ret;
	sigset_t omask;

	(*abi_sigsetmask)(SIG_BLOCK, &abisigset, &omask);
	(*abi_mutex_lock)(&abi_stdio_mutex);

	ret = fputs(ptr, stream);

	(*abi_mutex_unlock)(&abi_stdio_mutex);
	(*abi_sigsetmask)(SIG_SETMASK, &omask, NULL);

	return (ret);
}

void *
abi_malloc(size_t size)
{
	void *ret;
	sigset_t omask;

	(*abi_sigsetmask)(SIG_BLOCK, &abisigset, &omask);
	(*abi_mutex_lock)(&abi_malloc_mutex);

	ret = malloc(size);

	(*abi_mutex_unlock)(&abi_malloc_mutex);
	(*abi_sigsetmask)(SIG_SETMASK, &omask, NULL);

	return (ret);
}

void *
abi_calloc(size_t num, size_t size)
{
	void *ret;
	sigset_t omask;

	(*abi_sigsetmask)(SIG_BLOCK, &abisigset, &omask);
	(*abi_mutex_lock)(&abi_malloc_mutex);

	num *= size;
	ret = malloc(num);
	if (ret != NULL)
		(void) memset(ret, 0, num);

	(*abi_mutex_unlock)(&abi_malloc_mutex);
	(*abi_sigsetmask)(SIG_SETMASK, &omask, NULL);

	return (ret);
}

void *
abi_realloc(void *ptr, size_t size)
{
	void *ret;
	sigset_t omask;

	(*abi_sigsetmask)(SIG_BLOCK, &abisigset, &omask);
	(*abi_mutex_lock)(&abi_malloc_mutex);

	ret = realloc(ptr, size);

	(*abi_mutex_unlock)(&abi_malloc_mutex);
	(*abi_sigsetmask)(SIG_SETMASK, &omask, NULL);

	return (ret);
}

void
abi_free(void *ptr)
{
	sigset_t omask;

	(*abi_sigsetmask)(SIG_BLOCK, &abisigset, &omask);
	(*abi_mutex_lock)(&abi_malloc_mutex);

	free(ptr);

	(*abi_mutex_unlock)(&abi_malloc_mutex);
	(*abi_sigsetmask)(SIG_SETMASK, &omask, NULL);
}
