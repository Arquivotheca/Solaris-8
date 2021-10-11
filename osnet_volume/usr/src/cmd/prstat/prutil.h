/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PRUTIL_H
#define	_PRUTIL_H

#pragma ident	"@(#)prutil.h	1.1	99/04/19 SMI"

#include <sys/processor.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void Exit();
extern void Die(char *, ...);
extern void Progname(char *);
extern void Usage();
extern int Atoi(char *);
extern void Format_size(char *, size_t, int);
extern void Format_pct(char *, float, int);
extern void Format_num(char *, int, int);
extern void Format_time(char *, ulong_t, int);
extern void Format_state(char *, char, processorid_t, int);
extern void *Realloc(void *, size_t);
extern void *Malloc(size_t);
extern void *Zalloc(size_t);
extern int Setrlimit();
extern void Priocntl(char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _PRUTIL_H */
