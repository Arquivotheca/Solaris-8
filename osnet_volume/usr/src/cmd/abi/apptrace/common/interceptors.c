/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)interceptors.c	1.1	99/05/14 SMI"

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <synch.h>
#include <wchar.h>
#include <apptrace.h>

/*
 * Assumes that there's a sigset_t omask somewhere
 * in scope.
 */
#define	ABILOCK	\
	(*abi_sigsetmask)(SIG_BLOCK, &abisigset, &omask); \
	(*abi_mutex_lock)(&abi_stdio_mutex)

#define	ABIUNLOCK \
	(*abi_mutex_unlock)(&abi_stdio_mutex); \
	(*abi_sigsetmask)(SIG_SETMASK, &omask, NULL)

/*
 * printf is a special case.  Since there's a handy portable
 * v*printf family, we'll not use the real_printf pointer and
 * the appropriate v*printf function.
 * However, since we have two libc's in the address space, we
 * have to make sure that we get the vprintf from the base
 * link map, thus the macros.  If we don't do this, we get the
 * vprintf from the auditing link map and thus will be writing
 * into the iob of the libc of the auditing link map instead of
 * the base (or application's) iob...
 */
static int libc_printf(const char *, ...);
static abisym_t __abi_libc_printf_sym;

void *
__abi_libc_printf(void *real, int vflag)
{
	ABI_REAL(libc, printf) = real;
	ABI_VFLAG(libc, printf) = vflag;

	return ((void *) libc_printf);
}

static int
libc_printf(const char *format, ...)
{
	int ret;
	int serrno = ABI_ERRNO, ferrno;
	sigset_t omask;
	va_list args;

	ABI_ERRNO = 0;
	va_start(args, format);
	ret = (*ABI_VPRINTF)(format, args);
	va_end(args);
	ferrno = ABI_ERRNO;

	ABILOCK;

	(void) fprintf(ABISTREAM,
	    "format = 0x%p, ...) = %d\n", (void *)format, ret);
	if (ferrno) {
		(void) fprintf(ABISTREAM, " errno = %d (%s)\n",
		    ferrno, strerror(ferrno));
	}

	if (!ABI_VFLAG(libc, printf))
		goto done;

	ABIPUTS("  format = ");
	spf_prtype(ABISTREAM, "char", 1, format);

done:
	ABI_ERRNO = (ferrno == 0) ? serrno : ferrno;
	ABIUNLOCK;
	(void) fflush(stdout);
	return (ret);
}

static int libc_fprintf(FILE *, const char *, ...);
static abisym_t __abi_libc_fprintf_sym;

void *
__abi_libc_fprintf(void *real, int vflag)
{
	ABI_REAL(libc, fprintf) = real;
	ABI_VFLAG(libc, fprintf) = vflag;

	return ((void *) libc_fprintf);
}

static int
libc_fprintf(FILE *fp, const char *format, ...)
{
	int ret, ferrno, serrno = ABI_ERRNO;
	sigset_t omask;
	va_list args;

	ABI_ERRNO = 0;
	va_start(args, format);
	ret = (*ABI_VFPRINTF)(fp, format, args);
	va_end(args);
	ferrno = ABI_ERRNO;

	ABILOCK;

	(void) fprintf(ABISTREAM, "fp = 0x%p, format = 0x%p, ...) = %d\n",
	    (void *)fp, (void *)format, ret);
	if (ferrno) {
		(void) fprintf(ABISTREAM,
		    " errno = %d (%s)\n", ferrno, strerror(ferrno));
	}

	if (!ABI_VFLAG(libc, fprintf))
		goto done;

	ABIPUTS("  fp =     ");
	spf_prtype(ABISTREAM, "FILE", 1, fp);

	ABIPUTS("  format = ");
	spf_prtype(ABISTREAM, "char", 1, format);

done:
	ABI_ERRNO = (ferrno == 0) ? serrno : ferrno;
	ABIUNLOCK;
	return (ret);
}

static int libc_sprintf(char *, const char *, ...);
static abisym_t __abi_libc_sprintf_sym;

void *
__abi_libc_sprintf(void *real, int vflag)
{
	ABI_REAL(libc, sprintf) = real;
	ABI_VFLAG(libc, sprintf) = vflag;

	return ((void *) libc_sprintf);
}

static int
libc_sprintf(char *buf, const char *format, ...)
{
	int ret;
	sigset_t omask;
	va_list args;

	va_start(args, format);
	ret = (*ABI_VSPRINTF)(buf, format, args);
	va_end(args);

	ABILOCK;

	(void) fprintf(ABISTREAM, "buf = 0x%p, format = 0x%p, ...) = %d\n",
	    (void *)buf, (void *)format, ret);

	if (!ABI_VFLAG(libc, sprintf))
		goto done;

	ABIPUTS("  buf =    ");
	spf_prtype(ABISTREAM, "char", 1, buf);
	ABIPUTS("  format = ");
	spf_prtype(ABISTREAM, "char", 1, format);

done:
	ABIUNLOCK;
	return (ret);
}

static int libc_snprintf(char *, size_t, const char *, ...);
static abisym_t __abi_libc_snprintf_sym;

void *
__abi_libc_snprintf(void *real, int vflag)
{
	ABI_REAL(libc, snprintf) = real;
	ABI_VFLAG(libc, snprintf) = vflag;

	return ((void *) libc_snprintf);
}

static int
libc_snprintf(char *buf, size_t n, const char *format, ...)
{
	int ret;
	sigset_t omask;
	va_list args;

	va_start(args, format);
	ret = (*ABI_VSNPRINTF)(buf, n, format, args);
	va_end(args);

	ABILOCK;
	(void) fprintf(ABISTREAM,
	    "buf = 0x%p, n = %u, format = 0x%p, ...) = %d\n",
	    (void *)buf, n, (void *)format, ret);

	if (!ABI_VFLAG(libc, snprintf))
		goto done;

	ABIPUTS("  buf =    ");
	spf_prtype(ABISTREAM, "char", 1, buf);
	ABIPUTS("  n =      ");
	spf_prtype(ABISTREAM, "size_t", 0, &n);
	ABIPUTS("  format = ");
	spf_prtype(ABISTREAM, "char", 1, format);

done:
	ABIUNLOCK;
	return (ret);
}

static int libc_swprintf(wchar_t *, size_t, const wchar_t *, ...);
static abisym_t __abi_libc_swprintf_sym;

void *
__abi_libc_swprintf(void *real, int vflag)
{
	ABI_REAL(libc, swprintf) = real;
	ABI_VFLAG(libc, swprintf) = vflag;

	return ((void *) libc_swprintf);
}

static int
libc_swprintf(wchar_t *s, size_t n, const wchar_t *format, ...)
{
	int ret;
	sigset_t omask;
	va_list args;

	va_start(args, format);
	ret = (*ABI_VSWPRINTF)(s, n, format, args);
	va_end(args);

	ABILOCK;
	(void) fprintf(ABISTREAM,
	    "s = 0x%p, n = %u, format = 0x%p, ...) = %d\n",
	    (void *)s, n, (void *)format, ret);

	if (!ABI_VFLAG(libc, swprintf))
		goto done;

	ABIPUTS("  s =      ");
	spf_prtype(ABISTREAM, "wchar_t", 1, s);
	ABIPUTS("  n =      ");
	spf_prtype(ABISTREAM, "size_t", 0, &n);
	ABIPUTS("  format = ");
	spf_prtype(ABISTREAM, "wchar_t", 1, format);

done:
	ABIUNLOCK;
	return (ret);
}

static int libc_wprintf(const wchar_t *, ...);
static abisym_t __abi_libc_wprintf_sym;

void *
__abi_libc_wprintf(void *real, int vflag)
{
	ABI_REAL(libc, wprintf) = real;
	ABI_VFLAG(libc, wprintf) = vflag;

	return ((void *) libc_wprintf);
}

static int
libc_wprintf(const wchar_t *format, ...)
{
	int ret, ferrno, serrno = ABI_ERRNO;
	sigset_t omask;
	va_list args;

	ABI_ERRNO = 0;
	va_start(args, format);
	ret = (*ABI_VWPRINTF)(format, args);
	va_end(args);
	ferrno = ABI_ERRNO;

	ABILOCK;

	(void) fprintf(ABISTREAM, "format = 0x%p, ...) = %d\n",
	    (void *)format, ret);
	if (ferrno) {
		(void) fprintf(ABISTREAM,
		    " errno = %d (%s)\n", ferrno, strerror(ferrno));
	}

	if (!ABI_VFLAG(libc, wprintf))
		goto done;

	ABIPUTS("  format = ");
	spf_prtype(ABISTREAM, "wchar_t", 1, format);

done:
	ABI_ERRNO = (ferrno == 0) ? serrno : ferrno;
	ABIUNLOCK;
	return (ret);
}

static int libc_fwprintf(FILE *, const wchar_t *, ...);
static abisym_t __abi_libc_fwprintf_sym;

void *
__abi_libc_fwprintf(void *real, int vflag)
{
	ABI_REAL(libc, fwprintf) = real;
	ABI_VFLAG(libc, fwprintf) = vflag;

	return ((void *) libc_fwprintf);
}

static int
libc_fwprintf(FILE *stream, const wchar_t *format, ...)
{
	int ret, ferrno, serrno = ABI_ERRNO;
	sigset_t omask;
	va_list args;

	ABI_ERRNO = 0;
	va_start(args, format);
	ret = (*ABI_VFWPRINTF)(stream, format, args);
	va_end(args);
	ferrno = ABI_ERRNO;

	ABILOCK;
	(void) fprintf(ABISTREAM,
	    "stream = 0x%p, format = 0x%p, ...) = %d\n",
	    (void *)stream, (void *)format, ret);
	if (ferrno) {
		(void) fprintf(ABISTREAM,
		    " errno = %d (%s)\n", ferrno, strerror(ferrno));
	}

	if (!ABI_VFLAG(libc, fwprintf))
		goto done;

	ABIPUTS("  stream = ");
	spf_prtype(ABISTREAM, "FILE", 1, stream);
	ABIPUTS("  format = ");
	spf_prtype(ABISTREAM, "wchar_t", 1, format);

done:
	ABI_ERRNO = (ferrno == 0) ? serrno : ferrno;
	ABIUNLOCK;

	return (ret);
}

extern void exit(int);
static void libc_exit(int);
static abisym_t __abi_libc_exit_sym;

void *
__abi_libc_exit(void *real, int vflag)
{
	ABI_REAL(libc, exit) = real;
	ABI_VFLAG(libc, exit) = vflag;

	return ((void *) libc_exit);
}

static void
libc_exit(int status)
{
	sigset_t omask;

	ABILOCK;
	(void) fprintf(ABISTREAM, "status = %d)\n", status);
	ABIUNLOCK;

	ABI_CALL_REAL(libc, exit, (void (*)(int)))(status);
}

static pid_t libc_fork(void);
static abisym_t __abi_libc_fork_sym;

void *
__abi_libc_fork(void *real, int vflag)
{
	ABI_REAL(libc, fork) = real;
	ABI_VFLAG(libc, fork) = vflag;

	return ((void *) libc_fork);
}

static pid_t
libc_fork(void)
{
	sigset_t omask;
	pid_t ret;
	int ferrno, serrno = ABI_ERRNO;

	ABI_ERRNO = 0;
	ret = ABI_CALL_REAL(libc, fork, (pid_t (*)(void)))();
	ferrno = ABI_ERRNO;

	ABILOCK;

	ABIPUTS(") = ");

	switch (ret) {
	case 0:
		ABIPUTS("returning as child\n");
		break;
	case -1:
		(void) fprintf(ABISTREAM, "%ld, %d (%s)\n",
		    ret, ferrno, strerror(ferrno));
		break;
	default:
		(void) fprintf(ABISTREAM, "%lu\n", ret);
		break;
	}

	ABIUNLOCK;
	ABI_ERRNO = (ferrno == 0) ? serrno : ferrno;
	return (ret);
}
