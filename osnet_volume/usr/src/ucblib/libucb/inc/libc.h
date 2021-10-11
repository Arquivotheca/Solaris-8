/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This is where all the interfaces that are internal to libucb
 * which do not have a better home live
 */

#ifndef _LIBC_H
#define	_LIBC_H

#pragma ident	"@(#)libc.h	1.1	97/01/07 SMI"

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/regset.h>
#include <sys/times.h>
#include <sys/ucontext.h>
#include <sys/dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * getdents64 transitional interface is intentionally internal to libc
 */
extern int getdents64(int, struct dirent64 *, size_t);

/*
 * defined in port/stdio/doprnt.c
 */
extern int _doprnt(char *format, va_list in_args, FILE *iop);

/*
 * defined in port/gen/_psignal.c
 */
extern void _psignal(unsigned int sig, char *s);

/*
 * defined in _getsp.s
 */
extern greg_t _getsp(void);

/*
 * defined in ucontext.s
 */
extern int __getcontext(ucontext_t *);

/*
 * defined in _sigaction.s
 */
extern int __sigaction(int, const struct sigaction *, struct sigaction *);

/*
 * External Variables
 */
extern void (*_siguhandler[])(int, int, struct sigcontext *, char *);
	/* for BSD */

/*
 * defined in port/gen/siglist.c
 */
extern char *sys_siglist[];

#ifdef __cplusplus
}
#endif

#endif /* _LIBC_H */
