/*
 * Copyright (c) 1991,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RMT_H
#define	_RMT_H

#pragma ident	"@(#)rmt.h	1.6	99/01/22 SMI"

#include <sys/mtio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef __STDC__
extern void rmtinit(void (*)(const char *, ...), void (*)(int));
extern int rmthost(char *, uint_t);
extern int rmtopen(char *, int);
extern void rmtclose(void);
extern int rmtstatus(struct mtget *);
extern int rmtread(char *, uint_t);
extern int rmtwrite(char *, uint_t);
extern int rmtseek(int, int);
extern int rmtioctl(int, long);
#else
extern void rmtinit();
extern int rmthost();
extern int rmtopen();
extern void rmtclose();
extern int rmtstatus();
extern int rmtread();
extern int rmtwrite();
extern int rmtseek();
extern int rmtioctl();
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _RMT_H */
